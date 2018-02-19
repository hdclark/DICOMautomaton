
#include <list>
#include <functional>
#include <limits>
#include <map>
#include <cmath>
#include <experimental/any>

#include <pqxx/pqxx>         //PostgreSQL C++ interface.
#include <jansson.h>         //For JSON handling.

#include "YgorMisc.h"
#include "YgorMath.h"
#include "YgorImages.h"
#include "YgorStats.h"       //Needed for Stats:: namespace.
#include "YgorFilesDirs.h"   //Needed for Does_File_Exist_And_Can_Be_Read(...), etc..
#include "YgorAlgorithms.h"  //Needed for For_Each_In_Parallel<..>(...)
#include "YgorString.h"      //Needed for GetFirstRegex(...)
#include "YgorPlot.h"

#include "../ConvenienceRoutines.h"
#include "../Grouping/Misc_Functors.h"
#include "Contour_Similarity.h"




bool ComputeContourSimilarity(planar_image_collection<float,double> &imagecoll,
                              std::list<std::reference_wrapper<planar_image_collection<float,double>>> external_imgs,
                              std::list<std::reference_wrapper<contour_collection<double>>> ccsl,
                              std::experimental::any user_data ){

    //This routine computes a Dice similarity metric between two (and only two) contour_collections.
    // (You can combine contours into a single contour_collection if you want them to be computed as a
    // logical group, e.g., both eyes).
    //
    // This routine 'blits' voxels onto a grid with the same resolution as the provided image set.
    // So do not provide a course grid and expect a precise coefficient. In practice, you should 
    // probably just use the same grid size as the contours were originally contoured on (e.g., for
    // CTs probably 512x512 for each image).
    // 
    // You should also ensure the grid has enough spatial extent to fully encompass all contours.
    //
    // This routine does not modify the provided images, so there is no need to create copies.
    //

    //We require a valid ComputeContourSimilarityUserData struct packed into the user_data.
    ComputeContourSimilarityUserData *user_data_s;
    try{
        user_data_s = std::experimental::any_cast<ComputeContourSimilarityUserData *>(user_data);
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
    if(ccsl.size() != 2){
        FUNCWARN("This routine requires exactly two contour_collections. Cannot continue with computation");
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
        FUNCINFO("Images still to be processed: " << all_images.size());

        //Find the images which spatially overlap with this image.
        auto curr_img_it = all_images.front();
        auto selected_imgs = GroupSpatiallyOverlappingImages(curr_img_it, std::ref(imagecoll));

        if(selected_imgs.empty()){
            throw std::logic_error("No spatially-overlapping images found. There should be at least one"
                                   " image (the 'seed' image) which should match. Verify the spatial" 
                                   " overlap grouping routine.");
        }
        if(selected_imgs.size() != 1){
            throw std::logic_error("Spatially-overlapping images found. The similarity metric requires"
                                   " a uniform spatial grid without any overlap. Please trim all"
                                   " unnecessary images (or average, or something else).");
            // NOTE: We *could* just proceed using only the first image, but it is better to be explicit
            //       about what the routine should accept. 
        }
        for(auto &an_img_it : selected_imgs){
             all_images.remove(an_img_it); //std::list::remove() erases all elements equal to input value.
        }

        planar_image<float,double> &img = std::ref(*selected_imgs.front());
        //Loop over the rois, rows, columns, channels, and finally any selected images (if applicable).
        const auto row_unit   = img.row_unit;
        const auto col_unit   = img.col_unit;
        const auto ortho_unit = row_unit.Cross( col_unit ).unit();
   
        planar_image<float,double> img_L = (*selected_imgs.front()); //Create copies for blitting. Could be uint8_t or bool for space saving...
        planar_image<float,double> img_R = (*selected_imgs.front());
        img_L.fill_pixels(0.0); // 0.0 == boolean FALSE. Everything else == boolean TRUE.
        img_R.fill_pixels(0.0);


        //Loop over the ccsl, rois, rows, columns, channels, and finally any selected images (if applicable).
        //for(const auto &roi : rois){
        long int cc_number = 0;
        for(auto &ccs : ccsl){
            ++cc_number; // == 1 (L) or 2 (R).
            for(auto & contour : ccs.get().contours){
                if(contour.points.empty()) continue;
                if(! img.encompasses_contour_of_points(contour)) continue;
    
                //const auto ROIName =  roi_it->GetMetadataValueAs<std::string>("ROIName");
                //if(!ROIName){
                //    FUNCWARN("Missing necessary tags for reporting analysis results. Cannot continue");
                //    return false;
                //}
                
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
                            //for(auto chan = 0; chan < img.channels; ++chan){
                            //}//Loop over channels.

                            if(cc_number == 1){
                                img_L.reference(row,col,0) = 1.0;
                            }else if(cc_number == 2){
                                img_R.reference(row,col,0) = 1.0;
                            }else{
                                throw std::logic_error("Execution should never reach this point. Programming error.");
                            }
 
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

        for(auto row = 0; row < img.rows; ++row){
            for(auto col = 0; col < img.columns; ++col){
                const bool isL = (img_L.value(row,col,0) != 0.0);
                const bool isR = (img_R.value(row,col,0) != 0.0);
                if(isL) user_data_s->contour_L_voxels++;
                if(isR) user_data_s->contour_R_voxels++;
                if(isL && isR) user_data_s->overlap_voxels++;
            }
        }   
    }

    return true;
}

