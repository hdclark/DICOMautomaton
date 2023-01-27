//GenerateSurfaceMask.cc.

#include <asio.hpp>
#include <exception>
#include <any>
#include <functional>
#include <limits>
#include <list>
#include <memory>
#include <ostream>
#include <stdexcept>
#include <tuple>
#include <utility>

#include "../../Thread_Pool.h"
#include "../Grouping/Misc_Functors.h"
#include "GenerateSurfaceMask.h"
#include "YgorImages.h"
#include "YgorMath.h"
#include "YgorMisc.h"
#include "YgorLog.h"


bool ComputeGenerateSurfaceMask(planar_image_collection<float,double> &imagecoll,
                          std::list<std::reference_wrapper<planar_image_collection<float,double>>>,
                          std::list<std::reference_wrapper<contour_collection<double>>> ccsl,
                          std::any user_data ){

    //This routine takes an image volume (which is assumed to cover the ROI without overlap or gaps) with an arbitrary
    // (cartesian) grid, some ROI(s) of interest, and assigns voxel values to the image based on whether the voxel is
    // inside, outside, or on the boundary of the ROI(s).
    //
    // Ensure the image volume has a margin around the ROI or the surface may be truncated.
    //
    // This routine modifies image_collection. It is recommended to either use the image grid the contours were
    // originally defined on OR -- even better -- to generate a custom grid that more tightly bound the ROI(s) but
    // is guaranteed to leave a margin around it for capturing the surface.
    //
    // This routine treats all ROIs as though they belong to a single entity. Therefore, contours should not overlap or
    // provide conflicting information. For example, if there are two parotid contours overlapping an image slice and a
    // given voxel is inside one but not the other; in such case the results are undefined. (It might be the case that
    // only the first ROI will be considered, but you should not rely on this behaviour!)
    //
    // Only the first channel will be altered.
    //
    // NOTE: This routine has been written with two concepts of 'neighbours' being used: in-plane neighbours are
    //       'box-radius' neighbours (which also consider diagonals and cover a square grid with a given width =
    //       2*boxradius) and adjacent image slice neighbours. The box-radius is set to 1 for in-plane and 0 for
    //       adjacent images. This gives a fairly thick surface, but it also provides a good chance of detecting
    //       surface boundaries. 
    //

    //We require a valid GenerateSurfaceMaskUserData struct packed into the user_data.
    GenerateSurfaceMaskUserData *user_data_s;
    try{
        user_data_s = std::any_cast<GenerateSurfaceMaskUserData *>(user_data);
    }catch(const std::exception &e){
        YLOGWARN("Unable to cast user_data to appropriate format. Cannot continue with computation");
        return false;
    }

    //Check that there are contours to operate on.
    if(ccsl.empty()){
        YLOGWARN("Missing needed contour information. Cannot continue with computation");
        return false;
    }

    //Generate a comprehensive list of iterators to all as-of-yet-unused images. This list will be
    // pruned after images have been successfully operated on.
    auto all_images = imagecoll.get_all_images();
    while(!all_images.empty()){
        YLOGINFO("Images still to be processed: " << all_images.size());

        // Find the images which spatially overlap with this image.
        auto curr_img_it = all_images.front();
        auto selected_imgs = GroupSpatiallyOverlappingImages(curr_img_it, std::ref(imagecoll));

        if(selected_imgs.empty()){
            throw std::logic_error("No spatially-overlapping images found. There should be at least one"
                                   " image (the 'seed' image) which should match. Verify the spatial" 
                                   " overlap grouping routine.");
        }
        if(selected_imgs.size() != 1){
            throw std::logic_error("Multiple spatially-overlapping images found. This routine makes use"
                                   " of nearest neighbours and therefore cannot deal with overlapping"
                                   " images. Please trim them first.");
        }
        for(auto &an_img_it : selected_imgs){
             all_images.remove(an_img_it); //std::list::remove() erases all elements equal to input value.
        }

        planar_image<float,double> &img = std::ref(*selected_imgs.front());
        //Loop over the rois, rows, columns, channels, and finally any selected images.
        const auto row_unit   = img.row_unit;
        const auto col_unit   = img.col_unit;
        const auto ortho_unit = row_unit.Cross( col_unit ).unit();
   
        img.fill_pixels( 0, std::numeric_limits<float>::quiet_NaN() );
        
        //Find the (ranked) nearest images (above and below, if there are any) for later use.
        const auto ab_list_pair = imagecoll.get_nearest_images_above_below_not_encompassing_image(img);
        const auto above = ab_list_pair.first;
        const auto below = ab_list_pair.second;

        //Check if there are any contours on this image or it's neighbours. If not, skip checking the image.
        auto cc_select = ccsl;
        cc_select.remove_if([=](std::reference_wrapper<contour_collection<double>> cc) -> bool {
            const auto present_img = img.encompasses_any_part_of_contour_in_collection( cc.get() );
            if(present_img) return false; //Do not remove if img intersects a contour.

            if(!above.empty()){
                const auto present_above = above.front()->encompasses_any_part_of_contour_in_collection( cc.get() );
                if(present_above) return false;
            }
            if(!below.empty()){
                const auto present_below = below.front()->encompasses_any_part_of_contour_in_collection( cc.get() );
                if(present_below) return false;
            }
            return true;
        });

        if(cc_select.empty()){
            img.fill_pixels( 0, user_data_s->background_val );
            continue;
        }

        //Loop over the pixels of the image.
        {
            asio_thread_pool tp;

            for(auto row = 0; row < img.rows; ++row){
                tp.submit_task([&,row]() -> void {
                    for(auto col = 0; col < img.columns; ++col){
                        const auto point = img.position(row,col);

                        //Check if there are any ROI's this voxel is inside. 
                        bool is_in_an_roi = false;
                        for(auto &ccs : cc_select){
                            for(auto & contour : ccs.get().contours){
                                if(contour.points.empty()) continue;
                                if(! img.encompasses_contour_of_points(contour)) continue;

                                //Prepare a contour for fast is-point-within-the-polygon checking.
                                auto BestFitPlane = contour.Least_Squares_Best_Fit_Plane(ortho_unit);
                                auto ProjectedContour = contour.Project_Onto_Plane_Orthogonally(BestFitPlane);
                                const bool AlreadyProjected = true;
                
                                auto ProjectedPoint = BestFitPlane.Project_Onto_Plane_Orthogonally(point);
                                is_in_an_roi = ProjectedContour.Is_Point_In_Polygon_Projected_Orthogonally(BestFitPlane,
                                                                                                           ProjectedPoint,
                                                                                                           AlreadyProjected);
                                if(is_in_an_roi) break;
                            }
                            if(is_in_an_roi) break;
                        }
                        img.reference(row, col, 0) =  (is_in_an_roi) ? (user_data_s->interior_val)
                                                                     : (user_data_s->background_val);

                        //Create a lambda routine that takes an image and checks in-plane if any neighbours are (!is_in_an_roi).
                        auto check_inclusion = [&](const planar_image<float,double> &limg,
                                                   long int boxr ) -> bool {

                                //Project the original image's position onto the plane of this image, so we know where the central
                                // neighbour point is.
                                const auto limg_plane = limg.image_plane();
                                const auto lpoint = limg_plane.Project_Onto_Plane_Orthogonally(point);
                                const long int lindx = limg.index(lpoint, 0);
                                const auto rcc = limg.row_column_channel_from_index(lindx);
                                const auto lrow = std::get<0>(rcc);
                                const auto lcol = std::get<1>(rcc);

                                for(auto brow = (lrow-boxr); brow <= (lrow+boxr); ++brow){
                                    for(auto bcol = (lcol-boxr); bcol <= (lcol+boxr); ++bcol){
                                        //Check if the coordinates are legal and in the ROI.
                                        if( !isininc(0,brow,limg.rows-1) || !isininc(0,bcol,limg.columns-1) ) continue;
                                        const auto bpoint = limg.position(brow, bcol);

                                        for(auto &ccs : cc_select){
                                            for(auto & contour : ccs.get().contours){
                                                if(contour.points.empty()) continue;
                                                if(! limg.encompasses_contour_of_points(contour)) continue;

                                                //Prepare a contour for fast is-point-within-the-polygon checking.
                                                auto BestFitPlane = contour.Least_Squares_Best_Fit_Plane(ortho_unit);
                                                auto ProjectedContour = contour.Project_Onto_Plane_Orthogonally(BestFitPlane);
                                                const bool AlreadyProjected = true;
                                
                                                auto ProjectedPoint = BestFitPlane.Project_Onto_Plane_Orthogonally(bpoint);
                                                const auto bis_in_an_roi = ProjectedContour.Is_Point_In_Polygon_Projected_Orthogonally(BestFitPlane,
                                                                                                                                       ProjectedPoint,
                                                                                                                                       AlreadyProjected);
                                                if(bis_in_an_roi != is_in_an_roi) return true;
                                            }
                                        }
                                    }
                                }
                                return false; //No point (!is_in_an_roi) was found.
                        };


                        if(check_inclusion(img, 1)){
                            img.reference(row, col, 0) = user_data_s->surface_val;

                        //Apply the check to the nearest neighbouring image slices.
                        }else if( !above.empty() && check_inclusion(*(above.front()), 0) ){
                            img.reference(row, col, 0) = user_data_s->surface_val;
                        }else if( !below.empty() && check_inclusion(*(below.front()), 0) ){
                            img.reference(row, col, 0) = user_data_s->surface_val;
                        }
                    }
                });
            }
        }
    } //Finish tasks and terminate thread pool.

    return true;
}

