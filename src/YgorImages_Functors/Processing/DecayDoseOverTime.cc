
#include <list>
#include <functional>
#include <limits>
#include <map>
#include <cmath>
#include <algorithm>

#include "YgorMisc.h"
#include "YgorMath.h"
#include "YgorImages.h"
#include "YgorStats.h"       //Needed for Stats:: namespace.
#include "YgorAlgorithms.h"  //Needed for For_Each_In_Parallel<..>(...)
#include "YgorString.h"      //Needed for GetFirstRegex(...)
#include "YgorPlot.h"

#include "../ConvenienceRoutines.h"
#include "../../BED_Conversion.h"

#include "DecayDoseOverTime.h"

bool DecayDoseOverTime(planar_image_collection<float,double>::images_list_it_t first_img_it,
                       std::list<planar_image_collection<float,double>::images_list_it_t> selected_img_its,
                       std::list<std::reference_wrapper<planar_image_collection<float,double>>>,
                       std::list<std::reference_wrapper<contour_collection<double>>> ccsl, 
                       std::experimental::any user_data){

    //This routine walks over all voxels in the first image, overwriting voxel values. The values are treated
    // as dose and decayed over time according to the selected model.
    //
    // NOTE: This routine currently ignores all except the first image. You can save computational effort by
    //       only bothering to hand this routine time-independent image arrays (i.e., arrays with spatial but
    //       not temporal indices).

    //This routine requires a valid HighlightROIVoxelsUserData struct packed into the user_data. 
    DecayDoseOverTimeUserData *user_data_s;
    try{
        user_data_s = std::experimental::any_cast<DecayDoseOverTimeUserData *>(user_data);
    }catch(const std::exception &e){
        FUNCWARN("Unable to cast user_data to appropriate format. Cannot continue with computation");
        return false;
    }

    if(selected_img_its.size() != 1) throw std::invalid_argument("This routine operates on individual images only");

    if(ccsl.empty()){
        FUNCWARN("Missing needed contour information. Cannot continue with computation");
        return false;
    }

    //Work out some model parameters.
    const auto BED_abr_tol = BEDabr_from_n_D_abr(user_data_s->ToleranceNumberOfFractions, 
                                                 user_data_s->ToleranceTotalDose,
                                                 user_data_s->AlphaBetaRatio);

    //This is the 'recovery exponent' described in Jones and Grant 2014 (figure 4). Caption states:
    //   "Exponent r values obtained from data points obtained from 10% level of survival in Ang et al. [4] and using
    //   Equation A5, with two curves displayed for least squares data fitting using r = 2.8 + exp(1.67(t - 1)) (blue
    //   line), where t is elapsed time in years. The more cautious red line is based on r = 1.5 + exp(1.2(t - 1)) and
    //   may be preferred due to the experimental data limitations."
    // Note that [4] --> Ang KK, Jiang GL, Feng Y, Stephens LC, Tucker SL, Price RE. Extent and kinetics of recovery
    //                   of occult spinal cord injury. Int J Radiat Oncol Biol Phys 2001;50(4):1013e1020.
    const double r = (user_data_s->UseMoreConservativeRecovery) ?
                     1.5 + std::exp(0.100000 * (user_data_s->TemporalGapMonths - 12.0)) : // (t-1y)*1.2 converted to mo.
                     2.8 + std::exp(0.139177 * (user_data_s->TemporalGapMonths - 12.0)) ;
    const double r_exp = 1.0 / (1.0 + r);

    //Make a 'guard' image which we use to avoid re-processing each voxel.
    planar_image<float,double> guard;
    guard = *first_img_it;
    guard.fill_pixels(static_cast<float>(0));

    //Loop over the rois, rows, columns, channels, and finally any selected images (if applicable).
    const auto row_unit   = first_img_it->row_unit;
    const auto col_unit   = first_img_it->col_unit;
    const auto ortho_unit = row_unit.Cross( col_unit ).unit();

    //Record the min and max (outgoing) pixel values for windowing purposes.
    Stats::Running_MinMax<float> minmax_pixel;

    //Loop over the ccsl, rois, rows, columns, channels, and finally any selected images (if applicable).
    //for(const auto &roi : rois){
    for(auto &ccs : ccsl){
        for(auto roi_it = ccs.get().contours.begin(); roi_it != ccs.get().contours.end(); ++roi_it){
            if(roi_it->points.empty()) continue;
            if(! first_img_it->encompasses_contour_of_points(*roi_it)) continue;
            
    /*
            //Construct a bounding box to reduce computational demand of checking every voxel.
            auto BBox = roi_it->Bounding_Box_Along(row_unit, 1.0);
            auto BBoxBestFitPlane = BBox.Least_Squares_Best_Fit_Plane(vec3<double>(0.0,0.0,1.0));
            auto BBoxProjectedContour = BBox.Project_Onto_Plane_Orthogonally(BBoxBestFitPlane);
            const bool BBoxAlreadyProjected = true;
    */
    
            //Prepare a contour for fast is-point-within-the-polygon checking.
            auto BestFitPlane = roi_it->Least_Squares_Best_Fit_Plane(ortho_unit);
            auto ProjectedContour = roi_it->Project_Onto_Plane_Orthogonally(BestFitPlane);
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
                            //if( (user_data_s->channel != -1) && (user_data_s->channel != chan) ) continue;
                            if( user_data_s->channel != -1 ){
                                if( user_data_s->channel != chan ) continue;
                            }

                            //Check if another ROI has already written to this voxel. Bail if so.
                            // Otherwise, leave a mark to denote that we've visited this voxel already.
                            {
                                const auto curr_val = guard.value(row, col, chan);
                                if(curr_val != 0) continue;
                            }
                            guard.reference(row, col, chan) = static_cast<float>(1);
    
    
                            //Cycle over the grouped images (temporal slices, or whatever the user has decided).
                            // Harvest the time course or any other voxel-specific numbers.
                            for(auto & img_it : selected_img_its){
                                float newval = 0.0f;
                                const auto pixel_val = img_it->value(row, col, chan);

                                if(false){
                                }else if(user_data_s->model == DecayDoseOverTimeMethod::Halve){
                                    newval = pixel_val * 0.5;

                                }else if(user_data_s->model == DecayDoseOverTimeMethod::Jones_and_Grant_2014){
                                    const auto BED_abr_c1 = BEDabr_from_n_D_abr(user_data_s->Course1NumberOfFractions, 
                                                                                pixel_val,
                                                                                user_data_s->AlphaBetaRatio);
                                    double BED_ratio = (BED_abr_c1/BED_abr_tol);
                                    if( (0 < BED_ratio) && (BED_ratio < 1) ){
                                        const double time_scale_factor = std::pow((1.0 - BED_ratio),r_exp);
                                        const auto BED_abr_c1_eff = BED_abr_tol * (1.0 - time_scale_factor);

                                        const auto D_c1_eff = D_from_n_BEDabr(user_data_s->Course1NumberOfFractions,
                                                                              BED_abr_c1_eff);
                                        newval = D_c1_eff;

                                        //Debugging/validation:
                                        //if( ((row == 0) && (col == 19)) || ( (row == 9) && (col == 9)) ){
                                        //    FUNCINFO("BED_abr_tol    = " << BED_abr_tol.val);
                                        //    FUNCINFO("r              = " << r);
                                        //    FUNCINFO("r_exp          = " << r_exp);
                                        //    FUNCINFO("Dincoming      = " << pixel_val);
                                        //    FUNCINFO("BED_abr_c1     = " << BED_abr_c1.val);
                                        //    FUNCINFO("BED_ratio      = " << BED_ratio);
                                        //    FUNCINFO("BED_C1_eff     = " << BED_abr_c1_eff.val);
                                        //    FUNCINFO("Doutgoing      = " << D_c1_eff);
                                        //}
                                    }else{
                                        //The model does not apply to doses beyond the tolerance dose, so the safest
                                        // approach is to leave the dose in such voxels as-is.
                                        newval = pixel_val;
                                    }

                                }else{
                                    throw std::logic_error("Provided an invalid model. Cannot continue.");
                                }

                                img_it->reference(row, col, chan) = newval;
                                minmax_pixel.Digest(newval);
                            }
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

    //Alter the first image's metadata to reflect that averaging has occurred. You might want to consider
    // a selective whitelist approach so that unique IDs are not duplicated accidentally.
    UpdateImageDescription( std::ref(*first_img_it), "DoseDecayedOverTime" );
    UpdateImageWindowCentreWidth( std::ref(*first_img_it), minmax_pixel );

    return true;
}

