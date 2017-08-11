
#include <list>
#include <functional>
#include <limits>
#include <map>
#include <cmath>

#include "YgorMisc.h"
#include "YgorMath.h"
#include "YgorImages.h"
#include "YgorString.h"

#include "../ConvenienceRoutines.h"

#include "Highlight_ROI_Voxels.h"

bool HighlightROIVoxels(planar_image_collection<float,double>::images_list_it_t first_img_it,
                        std::list<planar_image_collection<float,double>::images_list_it_t> ,
                        std::list<std::reference_wrapper<planar_image_collection<float,double>>>,
                        std::list<std::reference_wrapper<contour_collection<double>>> ccsl, 
                        std::experimental::any user_data){

    //This routine walks over all voxels in the first image, overwriting voxel values. The value can depend on whether
    // the voxel is interior or exterior to the specified ROI(s) boundaries.
    //
    // NOTE: This routine currently ignores all except the first image. You can save computational effort by
    //       only bothering to hand this routine time-independent image arrays (i.e., arrays with spatial but
    //       not temporal indices).

    //This routine requires a valid HighlightROIVoxelsUserData struct packed into the user_data. 
    HighlightROIVoxelsUserData *user_data_s;
    try{
        user_data_s = std::experimental::any_cast<HighlightROIVoxelsUserData *>(user_data);
    }catch(const std::exception &e){
        FUNCWARN("Unable to cast user_data to appropriate format. Cannot continue with computation");
        return false;
    }

    if(!user_data_s->overwrite_interior && !user_data_s->overwrite_exterior){
        FUNCWARN("Nothing to do. Select either interior or exterior. Currently a no-op, but proceeding anyway");
    }
    
    //Figure out if there are any contours which are within the spatial extent of the image. 
    // There are many ways to do this! Since we are merely highlighting the contours, we scan 
    // all specified collections and treat them homogeneously.
    //
    // NOTE: We only bother to grab individual contours here. You could alter this if you wanted 
    //       each contour_collection's contours to have an identifying colour.
    if(ccsl.empty()) FUNCERR("Missing contour info needed for voxel colouring. Cannot continue");
    typedef std::list<contour_of_points<double>>::iterator contour_iter;
    std::list<contour_iter> rois;
    for(auto &ccs : ccsl){
        for(auto it =  ccs.get().contours.begin(); it != ccs.get().contours.end(); ++it){
            if(it->points.empty()) continue;
            if(first_img_it->encompasses_contour_of_points(*it)) rois.push_back(it);
        }
    }

    //Loop over the rois, rows, columns, channels, and finally any selected images (if applicable).
    const auto row_unit   = first_img_it->row_unit;
    const auto col_unit   = first_img_it->col_unit;
    const auto ortho_unit = row_unit.Cross( col_unit ).unit();
    const auto pxl_dx     = first_img_it->pxl_dx; 
    const auto pxl_dy     = first_img_it->pxl_dy; 

    //Generate a mask of the interior points.
    planar_image<uint8_t,double> mask;
    mask.init_buffer(first_img_it->rows, first_img_it->columns, 1);
    mask.fill_pixels(static_cast<uint8_t>(0));
    for(const auto &roi : rois){
        //Prepare a contour for fast is-point-within-the-polygon checking.
        auto BestFitPlane = roi->Least_Squares_Best_Fit_Plane(ortho_unit);
        auto ProjectedContour = roi->Project_Onto_Plane_Orthogonally(BestFitPlane);
        const bool AlreadyProjected = true;

        //Tests if a given point is interior to the ROI.
        const auto is_interior = [BestFitPlane,ProjectedContour,AlreadyProjected](vec3<double> point) -> bool {
            auto ProjectedPoint = BestFitPlane.Project_Onto_Plane_Orthogonally(point);
            return ProjectedContour.Is_Point_In_Polygon_Projected_Orthogonally(BestFitPlane,
                                                                               ProjectedPoint,
                                                                               AlreadyProjected);
        };

        for(auto row = 0; row < first_img_it->rows; ++row){
            for(auto col = 0; col < first_img_it->columns; ++col){

                const auto centre = first_img_it->position(row,col);
                if(user_data_s->inclusivity == HighlightInclusionMethod::centre){
                    if(is_interior(centre)){
                        mask.reference(row, col, 0) = static_cast<uint8_t>(1);
                    }

                }else if(   (user_data_s->inclusivity == HighlightInclusionMethod::planar_corners_inclusive)
                         || (user_data_s->inclusivity == HighlightInclusionMethod::planar_corners_exclusive) ){
                    const auto cornerA = centre + (row_unit * 0.5 * pxl_dx) + (col_unit * 0.5 * pxl_dy);
                    const auto cornerB = centre + (row_unit * 0.5 * pxl_dx) - (col_unit * 0.5 * pxl_dy);
                    const auto cornerC = centre - (row_unit * 0.5 * pxl_dx) - (col_unit * 0.5 * pxl_dy);
                    const auto cornerD = centre - (row_unit * 0.5 * pxl_dx) + (col_unit * 0.5 * pxl_dy);
                    if(   (  (user_data_s->inclusivity == HighlightInclusionMethod::planar_corners_inclusive)
                          && (is_interior(cornerA) || is_interior(cornerB) || is_interior(cornerC) || is_interior(cornerD)) )
                       || (  (user_data_s->inclusivity == HighlightInclusionMethod::planar_corners_exclusive)
                          && (is_interior(cornerA) && is_interior(cornerB) && is_interior(cornerC) && is_interior(cornerD)) ) ){
                        mask.reference(row, col, 0) = static_cast<uint8_t>(1);
                    }

                }
            }
        }
    }

    //Modify the first image as per the mask and specified behaviour.
    Stats::Running_MinMax<float> minmax_pixel;
    for(auto row = 0; row < first_img_it->rows; ++row){
        for(auto col = 0; col < first_img_it->columns; ++col){
            for(auto chan = 0; chan < first_img_it->channels; ++chan){
                //Only attempt an update if this is the desired channel.
                // (We have to cycle through them all regardless to update the window.)
                if((user_data_s->channel == -1) || (user_data_s->channel == chan)){
                    if(mask.value(row, col, 0) == static_cast<uint8_t>(1)){
                        if(user_data_s->overwrite_interior){
                            first_img_it->reference(row, col, chan) = user_data_s->outgoing_interior_val;
                        }
                    }else{
                        if(user_data_s->overwrite_exterior){
                            first_img_it->reference(row, col, chan) = user_data_s->outgoing_exterior_val;
                        }
                    }
                }
                minmax_pixel.Digest(first_img_it->value(row, col, chan));
            }
        }
    }

    //Alter the first image's metadata to reflect that averaging has occurred. You might want to consider
    // a selective whitelist approach so that unique IDs are not duplicated accidentally.
    UpdateImageDescription( std::ref(*first_img_it), "Highlighted ROIs" );
    UpdateImageWindowCentreWidth( std::ref(*first_img_it), minmax_pixel );

    return true;
}





