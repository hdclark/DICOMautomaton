//AccumulatePixelDistributions.cc.

#include <exception>
#include <any>
#include <optional>
#include <functional>
#include <list>
#include <map>
#include <ostream>
#include <stdexcept>

#include "../Grouping/Misc_Functors.h"
#include "AccumulatePixelDistributions.h"
#include "YgorImages.h"
#include "YgorMath.h"
#include "YgorMisc.h"
#include "YgorStats.h"       //Needed for Stats:: namespace.





bool AccumulatePixelDistributions(planar_image_collection<float,double> &imagecoll,
                          std::list<std::reference_wrapper<planar_image_collection<float,double>>>,
                          std::list<std::reference_wrapper<contour_collection<double>>> ccsl,
                          std::any user_data ){

    //This routine accumulates pixel/voxel intensities on an individual ROI-basis. The entire distribution is collected
    // so that various quantities can be computed afterward. In particular, direct comparison of distributions. Another
    // reason for collecting the entire distribution is that the action can be performed iteratively.
    //
    // The primary need for this routine was computing dose distributions on 'SGF' data sets. This routine replaces an
    // older routine that performs a nearly identical computation, but is less flexible.
    // 
    // Spatial overlapping images are combined by summing voxel intensities. The images must align exactly and contain
    // the same number of rows and columns. If something more exotic or robust is needed, images muct be combined prior 
    // to calling this routine. In any case, it is best to combine images prior to this routine.
    //
    // This routine does not modify the images it uses to compute ROIs, so there is no need to create copies.
    //

    //We require a valid AccumulatePixelDistributionsUserData struct packed into the user_data.
    AccumulatePixelDistributionsUserData *user_data_s;
    try{
        user_data_s = std::any_cast<AccumulatePixelDistributionsUserData *>(user_data);
    }catch(const std::exception &e){
        FUNCWARN("Unable to cast user_data to appropriate format. Cannot continue with computation");
        return false;
    }

    //Figure out if there are any contours for which are within the spatial extent of the image. 
    // There are many ways to do this! Since we are merely highlighting the contours, we scan 
    // all specified collections and treat them homogeneously.
    //
    // NOTE: We only bother to grab individual contours here. You could alter this if you wanted 
    //       each contour_collection's contours to have an identifying colour.
    if(ccsl.empty()){
        FUNCWARN("Missing needed contour information. Cannot continue with computation");
        return false;
    }

    //Generate a comprehensive list of iterators to all as-of-yet-unused images. This list will be
    // pruned after images have been successfully operated on.
    auto all_images = imagecoll.get_all_images();
    while(!all_images.empty()){
        FUNCINFO("Images still to be processed: " << all_images.size());

        // Find the images which spatially overlap with this image.
        auto curr_img_it = all_images.front();
        auto selected_imgs = GroupSpatiallyOverlappingImages(curr_img_it, std::ref(imagecoll));

        if(selected_imgs.empty()){
            throw std::logic_error("No spatially-overlapping images found. There should be at least one"
                                   " image (the 'seed' image) which should match. Verify the spatial" 
                                   " overlap grouping routine.");
        }
        {
          auto rows     = curr_img_it->rows;
          auto columns  = curr_img_it->columns;
          auto channels = curr_img_it->channels;

          for(const auto &an_img_it : selected_imgs){
              if( (rows     != an_img_it->rows)
              ||  (columns  != an_img_it->columns)
              ||  (channels != an_img_it->channels) ){
                  throw std::domain_error("Images have differing number of rows, columns, or channels."
                                          " This is not currently supported -- though it could be if needed."
                                          " Are you sure you've got the correct data?");
              }
              // NOTE: We assume the first image in the selected_images set is representative of the following
              //       images. We assume they all share identical row and column units, spatial extent, planar
              //       orientation, and (possibly) that row and column indices for one image are spatially
              //       equal to all other images. Breaking the last assumption would require an expensive 
              //       position_space-to-row_and_column_index lookup for each voxel.
          }
        }
        for(auto &an_img_it : selected_imgs){
             all_images.remove(an_img_it); //std::list::remove() erases all elements equal to input value.
        }

        planar_image<float,double> &img = std::ref(*selected_imgs.front());
        //Loop over the rois, rows, columns, channels, and finally any selected images (if applicable).
        const auto row_unit   = img.row_unit;
        const auto col_unit   = img.col_unit;
        const auto ortho_unit = row_unit.Cross( col_unit ).unit();
    
        //Loop over the ccsl, rois, rows, columns, channels, and finally any selected images (if applicable).
        //for(const auto &roi : rois){
        for(auto &ccs : ccsl){
            for(auto & contour : ccs.get().contours){
                if(contour.points.empty()) continue;
                if(! img.encompasses_contour_of_points(contour)) continue;
    
                const auto ROIName =  contour.GetMetadataValueAs<std::string>("ROIName");
                if(!ROIName){
                    FUNCWARN("Missing necessary tags for reporting analysis results. Cannot continue");
                    return false;
                }
                
        /*
                //Construct a bounding box to reduce computational demand of checking every voxel.
                auto BBox = roi_it->Bounding_Box_Along(row_unit, 1.0);
                auto BBoxBestFitPlane = BBox.Least_Squares_Best_Fit_Plane(vec3<double>(0.0,0.0,1.0));
                auto BBoxProjectedContour = BBox.Project_Onto_Plane_Orthogonally(BBoxBestFitPlane);
                const bool BBoxAlreadyProjected = true;
        */
        
                //Prepare a contour for fast is-point-within-the-polygon checking.
                auto BestFitPlane = contour.Least_Squares_Best_Fit_Plane(ortho_unit);
                auto ProjectedContour = contour.Project_Onto_Plane_Orthogonally(BestFitPlane);
                const bool AlreadyProjected = true;
        
                for(auto row = 0; row < img.rows; ++row){
                    for(auto col = 0; col < img.columns; ++col){
                        //Figure out the spatial location of the present voxel.
                        const auto point = img.position(row,col);
        
        /*
                        //Check if within the bounding box. It will generally be cheaper than the full contour (4 points vs. ? points).
                        auto BBoxProjectedPoint = BBoxBestFitPlane.Project_Onto_Plane_Orthogonally(point);
                        if(!BBoxProjectedContour.Is_Point_In_Polygon_Projected_Orthogonally(BBoxBestFitPlane,
                                                                                            BBoxProjectedPoint,
                                                                                            BBoxAlreadyProjected)) continue;
        */
        
                        //Perform a more detailed check to see if we are in the ROI.
                        auto ProjectedPoint = BestFitPlane.Project_Onto_Plane_Orthogonally(point);
                        if(ProjectedContour.Is_Point_In_Polygon_Projected_Orthogonally(BestFitPlane,
                                                                                       ProjectedPoint,
                                                                                       AlreadyProjected)){
                            for(auto chan = 0; chan < img.channels; ++chan){
                                //Cycle over the grouped images, accumulating the voxel intensity.
                                double combined_voxel_intensity = 0.0;
                                for(auto & img_it : selected_imgs){

                                    //Collect the datum of voxels and nearby voxels for an average.
                                    std::list<double> in_pixs;
                                    const auto boxr = 0;
                                    const auto min_datum = 1;
                                    for(auto lrow = (row-boxr); lrow <= (row+boxr); ++lrow){
                                        for(auto lcol = (col-boxr); lcol <= (col+boxr); ++lcol){
                                            //Check if the coordinates are legal and in the ROI.
                                            if( !isininc(0,lrow,img_it->rows-1) || !isininc(0,lcol,img_it->columns-1) ) continue;
        
                                            //const auto boxpoint = first_img_it->spatial_location(row,col);  //For standard contours(?).
                                            //const auto neighbourpoint = vec3<double>(lrow*1.0, lcol*1.0, SliceLocation*1.0);  //For the pixel integer contours.
                                            const auto neighbourpoint = img.position(lrow,lcol);
                                            auto ProjectedNeighbourPoint = BestFitPlane.Project_Onto_Plane_Orthogonally(neighbourpoint);
                                            if(!ProjectedContour.Is_Point_In_Polygon_Projected_Orthogonally(BestFitPlane,
                                                                                                            ProjectedNeighbourPoint,
                                                                                                            AlreadyProjected)) continue;
                                            const auto val = static_cast<double>(img_it->value(lrow, lcol, chan));
                                            in_pixs.push_back(val);
                                        }
                                    }
                                    if(in_pixs.size() < min_datum) continue; //If contours are too narrow so that there is too few datum for meaningful results.
                                    const auto combined_val = Stats::Sum(in_pixs);
                                    combined_voxel_intensity += combined_val;
                                }
        
                                // --------------- Incorporate the data into the user_data struct ------------------
                                user_data_s->accumulated_voxels[ ROIName.value() ].emplace_back(combined_voxel_intensity);
        
                                // ----------------------------------------------------------------------------
        
                            }//Loop over channels.
        
                        //If we're in the bounding box but not the ROI, do something.
                        }else{
                            //for(auto chan = 0; chan < first_img_it->channels; ++chan){
                            //    const auto curr_val = working.value(row, col, chan);
                            //    if(curr_val != 0) FUNCERR("There are overlapping ROI bboxes. This code currently cannot handle this. "
                            //                              "You will need to run the functor individually on the overlapping ROIs.");
                            //    working.reference(row, col, chan) = static_cast<float>(10);
                            //}
                        } // If is in ROI or ROI bbox.
                    } //Loop over cols
                } //Loop over rows
            } //Loop over ROIs.
        } //Loop over contour_collections.

    }

    return true;
}

