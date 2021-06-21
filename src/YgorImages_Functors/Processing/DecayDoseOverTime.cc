
#include <cmath>
#include <exception>
#include <functional>
#include <list>
#include <stdexcept>

#include "../../BED_Conversion.h"
#include "../ConvenienceRoutines.h"
#include "DecayDoseOverTime.h"
#include "YgorImages.h"
#include "YgorMisc.h"

template <class T> class contour_collection;

bool DecayDoseOverTime(planar_image_collection<float,double>::images_list_it_t first_img_it,
                       std::list<planar_image_collection<float,double>::images_list_it_t> selected_img_its,
                       std::list<std::reference_wrapper<planar_image_collection<float,double>>>,
                       std::list<std::reference_wrapper<contour_collection<double>>> ccsl, 
                       std::any user_data){

    //This routine walks over all voxels in the first image, overwriting voxel values. The values are treated
    // as dose and decayed over time according to the selected model.
    //
    // NOTE: This routine currently ignores all except the first image. You can save computational effort by
    //       only bothering to hand this routine time-independent image arrays (i.e., arrays with spatial but
    //       not temporal indices).

    //This routine requires a valid HighlightROIVoxelsUserData struct packed into the user_data. 
    DecayDoseOverTimeUserData *user_data_s;
    try{
        user_data_s = std::any_cast<DecayDoseOverTimeUserData *>(user_data);
    }catch(const std::exception &e){
        FUNCWARN("Unable to cast user_data to appropriate format. Cannot continue with computation");
        return false;
    }

    if(selected_img_its.size() != 1) throw std::invalid_argument("This routine operates on individual images only");

    if(ccsl.empty()){
        FUNCWARN("Missing needed contour information. Cannot continue with computation");
        return false;
    }

    //Allocate a second channel to store a mask.
    if(first_img_it->channels == 1){
        first_img_it->add_channel(0.0);
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

    //Record the min and max (outgoing) pixel values for windowing purposes.
    Mutate_Voxels_Opts ebv_opts;
    ebv_opts.editstyle      = Mutate_Voxels_Opts::EditStyle::InPlace; // Note: the mask scheme below requires in-place in order to decay with a single pass.
    ebv_opts.inclusivity    = Mutate_Voxels_Opts::Inclusivity::Inclusive;
    ebv_opts.contouroverlap = Mutate_Voxels_Opts::ContourOverlap::HonourOppositeOrientations;
    ebv_opts.aggregate      = Mutate_Voxels_Opts::Aggregate::Mean;
    ebv_opts.adjacency      = Mutate_Voxels_Opts::Adjacency::SingleVoxel;
    ebv_opts.maskmod        = Mutate_Voxels_Opts::MaskMod::Noop;

    auto f_bounded = [=](long int row, long int col, long int channel,
                         std::reference_wrapper<planar_image<float,double>> /*img_refw*/,
                         std::reference_wrapper<planar_image<float,double>> /*mask_img_refw*/,
                         float &voxel_val) {

        // First, check if the mask is set for this voxel. If it is, do NOT re-process.
        // It means the voxel has been processed in a previous decay operation (e.g., for another overlapping ROI) and
        // should not be decayed again.
        const auto mask_val = first_img_it->value(row, col, 1);
        if(mask_val != 0.0) return;

        // If the voxel is in the mask channel, disregard it.
        if(channel == 1) return;

        // Otherwise, perform the decay and then mark the mask.
        if(user_data_s->model == DecayDoseOverTimeMethod::Halve){
            voxel_val *= 0.5;

        }else if(user_data_s->model == DecayDoseOverTimeMethod::Jones_and_Grant_2014){
            const auto BED_abr_c1 = BEDabr_from_n_D_abr(user_data_s->Course1NumberOfFractions, 
                                                        voxel_val,
                                                        user_data_s->AlphaBetaRatio);

            //The model does not apply to doses beyond the tolerance dose, so the most conservative
            // approach is to leave the dose in such voxels as-is.
            double BED_ratio = (BED_abr_c1/BED_abr_tol);
            if( (0 < BED_ratio) && (BED_ratio < 1) ){
                const double time_scale_factor = std::pow((1.0 - BED_ratio),r_exp);
                const auto BED_abr_c1_eff = BED_abr_tol * (1.0 - time_scale_factor);

                const auto D_c1_eff = D_from_n_BEDabr(user_data_s->Course1NumberOfFractions,
                                                      BED_abr_c1_eff);
                voxel_val = D_c1_eff;

                //Debugging/validation:
                //if( ((row == 0) && (col == 19)) || ( (row == 9) && (col == 9)) ){
                //    FUNCINFO("BED_abr_tol    = " << BED_abr_tol.val);
                //    FUNCINFO("r              = " << r);
                //    FUNCINFO("r_exp          = " << r_exp);
                //    FUNCINFO("Dincoming      = " << voxel_val);
                //    FUNCINFO("BED_abr_c1     = " << BED_abr_c1.val);
                //    FUNCINFO("BED_ratio      = " << BED_ratio);
                //    FUNCINFO("BED_C1_eff     = " << BED_abr_c1_eff.val);
                //    FUNCINFO("Doutgoing      = " << D_c1_eff);
                //}
            }

        }else{
            throw std::logic_error("Provided an invalid model. Cannot continue.");
        }
        first_img_it->reference(row, col, 1) = 1.0;
        return;
    };

    std::list<std::reference_wrapper<planar_image<float,double>>> selected_imgs;
    for(auto &img_it : selected_img_its) selected_imgs.push_back( std::ref(*img_it) );

    Mutate_Voxels<float,double>( std::ref(*first_img_it),
                                 selected_imgs, 
                                 ccsl, 
                                 ebv_opts, 
                                 f_bounded );

    //Alter the first image's metadata to reflect that averaging has occurred. You might want to consider
    // a selective whitelist approach so that unique IDs are not duplicated accidentally.
    UpdateImageDescription( std::ref(*first_img_it), "DoseDecayedOverTime" );
    UpdateImageWindowCentreWidth( std::ref(*first_img_it) );

    return true;
}

