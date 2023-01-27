
#include <exception>
#include <any>
#include <optional>
#include <functional>
#include <list>
#include <map>
#include <ostream>
#include <stdexcept>

#include "../Grouping/Misc_Functors.h"
#include "Per_ROI_Time_Courses.h"
#include "YgorImages.h"
#include "YgorMath.h"
#include "YgorMisc.h"
#include "YgorLog.h"
#include "YgorStats.h"       //Needed for Stats:: namespace.





bool ComputePerROICourses(planar_image_collection<float,double> &imagecoll,
                          std::list<std::reference_wrapper<planar_image_collection<float,double>>>,
                          std::list<std::reference_wrapper<contour_collection<double>>> ccsl,
                          std::any user_data ){

    //This routine computes aggregate courses for the specified ROIs; pixels within a contour are value
    // averaged into a samples_1D. Typically these will be time courses, but can be groupings along any
    // dimension in which you can cluster images. (For example, maybe flip angle, or kVp setting, or 
    // series number, etc.)
    // 
    // This routine does not modify the images it uses to compute ROIs, so there is no need to create copies.
    //

    //We require a valid ComputePerROITimeCoursesUserData struct packed into the user_data.
    ComputePerROITimeCoursesUserData *user_data_s;
    try{
        user_data_s = std::any_cast<ComputePerROITimeCoursesUserData *>(user_data);
    }catch(const std::exception &e){
        YLOGWARN("Unable to cast user_data to appropriate format. Cannot continue with computation");
        return false;
    }

    //This routine performs a number of calculations. It is experimental and excerpts you plan to rely on should be
    // made into their own analysis functors.
    const bool InhibitSort = true; //Disable continuous sorting (defer to single sort later) to speed up data ingress.


    //Figure out if there are any contours for which are within the spatial extent of the image. 
    // There are many ways to do this! Since we are merely highlighting the contours, we scan 
    // all specified collections and treat them homogeneously.
    //
    // NOTE: We only bother to grab individual contours here. You could alter this if you wanted 
    //       each contour_collection's contours to have an identifying colour.
    if(ccsl.empty()){
        YLOGWARN("Missing needed contour information. Cannot continue with computation");
        return false;
    }



/*
    //typedef std::list<contour_of_points<double>>::iterator contour_iter;
    //std::list<contour_iter> rois;
    decltype(ccsl) rois;
    for(auto &ccs : ccsl){
        for(auto it =  ccs.get().contours.begin(); it != ccs.get().contours.end(); ++it){
            if(it->points.empty()) continue;
            //if(first_img_it->encompasses_contour_of_points(*it)) rois.push_back(it);
            if(first_img_it->encompasses_contour_of_points(*it)) rois.push_back(
        }
    }
*/


    //Generate a comprehensive list of iterators to all as-of-yet-unused images. This list will be
    // pruned after images have been successfully operated on.
    auto all_images = imagecoll.get_all_images();
    while(!all_images.empty()){
        YLOGINFO("Images still to be processed: " << all_images.size());

        //Find the images which spatially overlap with this image.
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
                    YLOGWARN("Missing necessary tags for reporting analysis results. Cannot continue");
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
                                //Cycle over the grouped images (temporal slices, or whatever the user has decided).
                                // Harvest the time course or any other voxel-specific numbers.
                                samples_1D<double> channel_time_course;
                                channel_time_course.uncertainties_known_to_be_independent_and_random = true;
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
                                    auto dt = img_it->GetMetadataValueAs<double>("dt");
                                    if(!dt) YLOGERR("Image is missing time metadata. Bailing");

                                    const auto avg_val = Stats::Mean(in_pixs);
                                    if(in_pixs.size() < min_datum) continue; //If contours are too narrow so that there is too few datum for meaningful results.
                                    //const auto avg_val_sigma = std::sqrt(Stats::Unbiased_Var_Est(in_pixs))/std::sqrt(1.0*in_pixs.size());
                                    //channel_time_course.push_back(dt.value(), 0.0, avg_val, avg_val_sigma, InhibitSort);
                                    channel_time_course.push_back(dt.value(), 0.0, avg_val, 0.0, InhibitSort);

                                }
                                channel_time_course.stable_sort();
                                if(channel_time_course.empty()) continue;
        
                                //Fill in some basic time course metadata.
         
                                // ... TODO ...  (Worthwhile at this stage?)
     
                                // --------------- Append the time course data into the user_data struct ------------------
                                //user_data_s->total_voxel_count[ std::ref(ccs) ] += channel_time_course.size();
                                //user_data_s->voxel_count[ std::ref(ccs) ] += 1;
                                user_data_s->time_courses[ ROIName.value() ] = user_data_s->time_courses[ ROIName.value() ].Sum_With(channel_time_course);
                                user_data_s->total_voxel_count[ ROIName.value() ] += channel_time_course.size();
                                user_data_s->voxel_count[ ROIName.value() ] += 1;
        
                                // ----------------------------------------------------------------------------
        
                            }//Loop over channels.
        
                        //If we're in the bounding box but not the ROI, do something.
                        }else{
                            //for(auto chan = 0; chan < first_img_it->channels; ++chan){
                            //    const auto curr_val = working.value(row, col, chan);
                            //    if(curr_val != 0) YLOGERR("There are overlapping ROI bboxes. This code currently cannot handle this. "
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

