
#include <any>
#include <optional>
#include <functional>
#include <list>

#include "../ConvenienceRoutines.h"
#include "YgorImages.h"
#include "YgorMath.h"
#include "YgorMisc.h"
#include "YgorLog.h"
#include "YgorStats.h"       //Needed for Stats:: namespace.


const auto boxr = 2; //The inclusive 'radius' of the square box to use to average nearby pixels. Controls amount of spatial averaging.
const auto min_datum = 3; //The minimum number of nearby pixels needed to proceed with each average/variance estimate/etc.. 
                          // Note this is very sensistive to boxr. If boxr = 1 the max min_datum is 5. If boxr = 2 the max min_datum 
                          // is 13. In general, it is best to keep it at 3 or maybe 5 if you want to be extra precise about interpreting
                          // variance estimates.

bool TimeCourseSlopeDifference(planar_image_collection<float,double>::images_list_it_t first_img_it,
                               std::list<planar_image_collection<float,double>::images_list_it_t> selected_img_its,
                               std::list<std::reference_wrapper<planar_image_collection<float,double>>>,
                               std::list<std::reference_wrapper<contour_collection<double>>> ccsl,
                               double t1min, double t1max,  
                               double t2min, double t2max,
                               std::any ){

    //This routine computes a map of the difference of slopes fit over two time periods:
    //    (slope over t2range) - (slope over t1range).
    // The ranges may overlap if you want.
    const bool InhibitSort = true; //Disable continuous sorting (defer to single sort later) to speed up data ingress.

    //Figure out if there are any contours for which are within the spatial extent of the image. 
    // There are many ways to do this! Since we are merely highlighting the contours, we scan 
    // all specified collections and treat them homogeneously.
    //
    // NOTE: We only bother to grab individual contours here. You could alter this if you wanted 
    //       each contour_collection's contours to have an identifying colour.
    if(ccsl.empty()) YLOGERR("Missing contour info needed for voxel colouring. Cannot continue");
    using contour_iter = std::list<contour_of_points<double> >::iterator;
    std::list<contour_iter> rois;
    for(auto &ccs : ccsl){
        for(auto it =  ccs.get().contours.begin(); it != ccs.get().contours.end(); ++it){
            if(it->points.empty()) continue;
            //if(first_img_it->encompasses_contour_of_points(*it)) rois.emplace_back(*it);
            if(first_img_it->encompasses_contour_of_points(*it)) rois.push_back(it);
        }
    }

    //Make a 'working' image which we can edit. Start by duplicating the first image.
    planar_image<float,double> working;
    working = *first_img_it;

    //Paint all pixels black.
    working.fill_pixels(static_cast<float>(0));

    //Record the min and max actual pixel values for windowing purposes.
    Stats::Running_MinMax<float> minmax_pixel;

    //Loop over the rois, rows, columns, channels, and finally any selected images (if applicable).
    const auto row_unit   = first_img_it->row_unit;
    const auto col_unit   = first_img_it->col_unit;
    const auto ortho_unit = row_unit.Cross( col_unit ).unit();

    //Loop over the rois, rows, columns, channels, and finally any selected images (if applicable).
    for(const auto &roi : rois){

/*
        //Construct a bounding box to reduce computational demand of checking every voxel.
        auto BBox = roi->Bounding_Box_Along(row_unit, 1.0);
        auto BBoxBestFitPlane = BBox.Least_Squares_Best_Fit_Plane(vec3<double>(0.0,0.0,1.0));
        auto BBoxProjectedContour = BBox.Project_Onto_Plane_Orthogonally(BBoxBestFitPlane);
        const bool BBoxAlreadyProjected = true;
*/

        //Prepare a contour for fast is-point-within-the-polygon checking.
        auto BestFitPlane = roi->Least_Squares_Best_Fit_Plane(ortho_unit);
        auto ProjectedContour = roi->Project_Onto_Plane_Orthogonally(BestFitPlane);
        const bool AlreadyProjected = true;

        for(auto row = 0; row < first_img_it->rows; ++row){
            for(auto col = 0; col < first_img_it->columns; ++col){
                //Figure out the spatial location of the present voxel.
                const auto point = first_img_it->position(row,col);

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
                    for(auto chan = 0; chan < first_img_it->channels; ++chan){
                        //Cycle over the grouped images (temporal slices, or whatever the user has decided).
                        // Harvest the time course or any other voxel-specific numbers.
                        samples_1D<double> channel_time_course;
                        channel_time_course.uncertainties_known_to_be_independent_and_random = true;
                        for(auto & img_it : selected_img_its){
                            //Collect the datum of voxels and nearby voxels for an average.
                            std::list<double> in_pixs;
                            for(auto lrow = (row-boxr); lrow <= (row+boxr); ++lrow){
                                for(auto lcol = (col-boxr); lcol <= (col+boxr); ++lcol){
                                    //Check if the coordinates are legal and in the ROI.
                                    if( !isininc(0,lrow,img_it->rows-1) || !isininc(0,lcol,img_it->columns-1) ) continue;

                                    //const auto boxpoint = first_img_it->spatial_location(row,col);  //For standard contours(?).
                                    //const auto neighbourpoint = vec3<double>(lrow*1.0, lcol*1.0, SliceLocation*1.0);  //For the pixel integer contours.
                                    const auto neighbourpoint = first_img_it->position(lrow,lcol);
                                    auto ProjectedNeighbourPoint = BestFitPlane.Project_Onto_Plane_Orthogonally(neighbourpoint);
                                    if(!ProjectedContour.Is_Point_In_Polygon_Projected_Orthogonally(BestFitPlane,
                                                                                                    ProjectedNeighbourPoint,
                                                                                                    AlreadyProjected)) continue;
                                    const auto val = static_cast<double>(img_it->value(lrow, lcol, chan));
                                    in_pixs.push_back(val);
                                }
                            }
                            const auto avg_val = Stats::Mean(in_pixs);
                            if(in_pixs.size() < min_datum) continue; //If contours are too narrow so that there is too few datum for meaningful results.
                            //const auto avg_val_sigma = std::sqrt(Stats::Unbiased_Var_Est(in_pixs))/std::sqrt(1.0*in_pixs.size());

                            if(auto dt = img_it->GetMetadataValueAs<double>("dt")){
                                //channel_time_course.push_back(dt.value(), 0.0, avg_val, avg_val_sigma, InhibitSort);
                                channel_time_course.push_back(dt.value(), 0.0, avg_val, 0.0, InhibitSort);
                            }else{
                                YLOGERR("Image is missing time metadata. Bailing");
                            }
                        }
                        channel_time_course.stable_sort();
                        if(channel_time_course.empty()) continue;
       
                        // --------------- Perform some calculations on the time course ---------------
  
                        bool t1wasOK(false), t2wasOK(false);
                        bool SkipExtras = true; // Skip costly stats computations irrelevant to our use case.
                        auto t1range = channel_time_course.Select_Those_Within_Inc(t1min, t1max);
                        auto t2range = channel_time_course.Select_Those_Within_Inc(t2min, t2max);

                        auto res_t1 = t1range.Linear_Least_Squares_Regression(&t1wasOK, SkipExtras);
                        auto res_t2 = t2range.Linear_Least_Squares_Regression(&t2wasOK, SkipExtras);
                        if(t1wasOK && t2wasOK){
                            const auto slope_diff = res_t2.slope - res_t1.slope;

                            //Update the pixei.
                            const auto newval = static_cast<float>(slope_diff);

                            working.reference(row, col, chan) = newval;
                            minmax_pixel.Digest(newval);
                        }

                        // ----------------------------------------------------------------------------

                    }//Loop over channels.
                } // If is in ROI or ROI bbox.
            } //Loop over cols
        } //Loop over rows
    } //Loop over ROIs.

    //Swap the original image with the working image.
    *first_img_it = working;

    UpdateImageDescription( std::ref(*first_img_it), "Time Course dSlope Map" );
    UpdateImageWindowCentreWidth( std::ref(*first_img_it), minmax_pixel );

    return true;
}

