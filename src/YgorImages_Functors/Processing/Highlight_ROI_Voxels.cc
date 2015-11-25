
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


bool HighlightROIVoxels(planar_image_collection<float,double>::images_list_it_t first_img_it,
                        std::list<planar_image_collection<float,double>::images_list_it_t> ,
                        std::list<std::reference_wrapper<planar_image_collection<float,double>>>,
                        std::list<std::reference_wrapper<contour_collection<double>>> ccsl, 
                        std::experimental::any ){

    //This routine walks over the voxels, emphasizing voxels within the ROI. It also colours the bounding box
    // so you can see if which bbox orientation results in a reasonably small area. 
    //
    // NOTE: This routine currently ignores all except the first image. You can save computational effort by
    //       only bothering to hand this routine time-indenpendent image arrays (i.e., arrays with spatial but
    //       not temporal indices).

    //Figure out if there are any contours which are within the spatial extent of the image. 
    // There are many ways to do this! Since we are merely highlighting the contours, we scan 
    // all specified collections and treat them homogeneously.
    //
    // NOTE: We only bother to grab individual contours here. You could alter this if you wanted 
    //       each contour_collection's contours to have an identifying colour.
    if(ccsl.empty()) FUNCERR("Missing contour info needed for voxel colouring. Cannot continue");
    typedef std::list<contour_of_points<double>>::iterator contour_iter;
    std::list<contour_iter> rois;
//std::list<contour_of_points<double>> rois;
    for(auto &ccs : ccsl){
        for(auto it =  ccs.get().contours.begin(); it != ccs.get().contours.end(); ++it){
            if(it->points.empty()) continue;
            //if(first_img_it->encompasses_contour_of_points(*it)) rois.emplace_back(*it);
            if(first_img_it->encompasses_contour_of_points(*it)) rois.push_back(it);
        }
    }

    //Paint all pixels black.
    first_img_it->fill_pixels(static_cast<float>(0));

    //Loop over the rois, rows, columns, channels, and finally any selected images (if applicable).
    const auto row_unit   = first_img_it->row_unit;
    const auto col_unit   = first_img_it->col_unit;
    const auto ortho_unit = row_unit.Cross( col_unit ).unit();

    Stats::Running_MinMax<float> minmax_pixel;

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
//auto BestFitPlane = roi.Least_Squares_Best_Fit_Plane(orthog_unit);
//auto ProjectedContour = roi.Project_Onto_Plane_Orthogonally(BestFitPlane);
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

                //If we're in the bounding box, perform a more detailed check to see if we are in the ROI.
                auto ProjectedPoint = BestFitPlane.Project_Onto_Plane_Orthogonally(point);
                if(ProjectedContour.Is_Point_In_Polygon_Projected_Orthogonally(BestFitPlane,
                                                                               ProjectedPoint,
                                                                               AlreadyProjected)){
                    for(auto chan = 0; chan < first_img_it->channels; ++chan){
                        first_img_it->reference(row, col, chan) = static_cast<float>(20);
                        minmax_pixel.Digest(static_cast<float>(20));
                    }

                //If we're in the bounding box but not the ROI, indicate so. Do not overwrite if a previous ROI
                // overlaps.
                }else{
                    for(auto chan = 0; chan < first_img_it->channels; ++chan){
                        const auto curr_val = first_img_it->value(row, col, chan);
                        if(curr_val != 0){ 
                            first_img_it->reference(row, col, chan) = static_cast<float>(10);
                            minmax_pixel.Digest(static_cast<float>(10));
                        }
                    }
                }
            }
        }
    }

    //Alter the first image's metadata to reflect that averaging has occurred. You might want to consider
    // a selective whitelist approach so that unique IDs are not duplicated accidentally.
    UpdateImageDescription( std::ref(*first_img_it), "Highlighted ROIs" );
    UpdateImageWindowCentreWidth( std::ref(*first_img_it), minmax_pixel );

    return true;
}





