
#include <exception>
#include <functional>
#include <limits>
#include <list>
#include <map>
#include <stdexcept>
#include <string>

#include "../../BED_Conversion.h"
#include "../ConvenienceRoutines.h"
#include "EQDConversion.h"
#include "YgorImages.h"
#include "YgorMisc.h"

template <class T> class contour_collection;

bool EQDConversion(planar_image_collection<float,double>::images_list_it_t first_img_it,
                       std::list<planar_image_collection<float,double>::images_list_it_t> selected_img_its,
                       std::list<std::reference_wrapper<planar_image_collection<float,double>>>,
                       std::list<std::reference_wrapper<contour_collection<double>>> ccsl, 
                       std::any user_data){

    //This routine converts voxel intensities (dose) into EQD doses -- the dose BED-based dose equivalent if
    // the radiation were delivered in 2Gy fractions. 
    //
    // Note that both NumberOfFractions and DosePrescription (to the PTV or CTV) must be specified. 
    //
    // Remember: only the prescription dose will have 2Gy fractions.

    EQDConversionUserData *user_data_s;
    try{
        user_data_s = std::any_cast<EQDConversionUserData *>(user_data);
    }catch(const std::exception &e){
        FUNCWARN("Unable to cast user_data to appropriate format. Cannot continue with computation");
        return false;
    }

    if(selected_img_its.size() != 1) throw std::invalid_argument("This routine operates on individual images only");

    if(ccsl.empty()){
        FUNCWARN("Missing needed contour information. Cannot continue with computation");
        return false;
    }

    Mutate_Voxels_Opts ebv_opts;
    ebv_opts.editstyle      = Mutate_Voxels_Opts::EditStyle::InPlace;
    ebv_opts.inclusivity    = Mutate_Voxels_Opts::Inclusivity::Centre;
    ebv_opts.contouroverlap = Mutate_Voxels_Opts::ContourOverlap::Ignore;
    ebv_opts.aggregate      = Mutate_Voxels_Opts::Aggregate::First;
    ebv_opts.adjacency      = Mutate_Voxels_Opts::Adjacency::SingleVoxel;
    ebv_opts.maskmod        = Mutate_Voxels_Opts::MaskMod::Noop;

    std::function< void(long int, long int, long int, std::reference_wrapper<planar_image<float,double>>, float &) > f_bounded;
    std::function< void(long int, long int, long int, std::reference_wrapper<planar_image<float,double>>, float &) > f_unbounded;

    if(user_data_s->model == EQDConversionUserData::Model::SimpleLinearQuadratic){
        if(user_data_s->NumberOfFractions <= 0){
            throw std::invalid_argument("NumberOfFractions not specified or invalid.");
        }else if(user_data_s->AlphaBetaRatioTumour <= 0){
            throw std::invalid_argument("AlphaBetaRatioTumour not specified or invalid.");
        }else if(user_data_s->TargetDosePerFraction <= 0){
            throw std::invalid_argument("TargetDosePerFraction not specified or invalid.");
        }else if(user_data_s->AlphaBetaRatioNormal <= 0){
            throw std::invalid_argument("AlphaBetaRatioNormal not specified or invalid.");
        }

        f_bounded = [=](long int /*row*/, long int /*col*/, long int /*channel*/, std::reference_wrapper<planar_image<float,double>> /*img_refw*/, float &voxel_val) {
            if(voxel_val <= 0.0) return; // No-op if there is no dose.

            const auto D_voxel = voxel_val;
            const auto numer = D_voxel * ( (D_voxel / user_data_s->NumberOfFractions) + user_data_s->AlphaBetaRatioTumour );
            const auto denom = (user_data_s->TargetDosePerFraction + user_data_s->AlphaBetaRatioTumour );
            const auto EQD = numer / denom;
            voxel_val = EQD;
            return;
        };

        f_unbounded = [=](long int /*row*/, long int /*col*/, long int /*channel*/, std::reference_wrapper<planar_image<float,double>> /*img_refw*/, float &voxel_val) {
            if(voxel_val <= 0.0) return; // No-op if there is no dose.

            const auto D_voxel = voxel_val;
            const auto numer = D_voxel * ( (D_voxel / user_data_s->NumberOfFractions) + user_data_s->AlphaBetaRatioNormal );
            const auto denom = (user_data_s->TargetDosePerFraction + user_data_s->AlphaBetaRatioNormal );
            const auto EQD = numer / denom;
            voxel_val = EQD;
            return;
        };

        first_img_it->metadata["EQD_NumberOfFractions"] = std::to_string(user_data_s->NumberOfFractions);
        first_img_it->metadata["EQD_Model"] = "Pinned LQ";
        first_img_it->metadata["EQD_TargetDosePerFraction"] = std::to_string(user_data_s->TargetDosePerFraction);
        first_img_it->metadata["EQD_NormalTissue_AlphaBetaRatio"] = std::to_string(user_data_s->AlphaBetaRatioNormal);
        first_img_it->metadata["EQD_TumourTissue_AlphaBetaRatio"] = std::to_string(user_data_s->AlphaBetaRatioTumour);

    }else if(user_data_s->model == EQDConversionUserData::Model::PinnedLinearQuadratic){

        if(user_data_s->NumberOfFractions <= 0){
            throw std::invalid_argument("NumberOfFractions not specified or invalid.");
        }else if(user_data_s->PrescriptionDose <= 0){
            throw std::invalid_argument("PrescriptionDose not specified or invalid.");
        }else if(user_data_s->AlphaBetaRatioTumour <= 0){
            throw std::invalid_argument("AlphaBetaRatioTumour not specified or invalid.");
        }else if(user_data_s->TargetDosePerFraction <= 0){
            throw std::invalid_argument("TargetDosePerFraction not specified or invalid.");
        }else if(user_data_s->AlphaBetaRatioNormal <= 0){
            throw std::invalid_argument("AlphaBetaRatioNormal not specified or invalid.");
        }

        //Work out the prescription dose EQD to get the number of fractions.
        //
        // Explanation: Only the mythical prescription dose should receive d dose per fraction. Voxels with higher dose will
        // receive higher dose per fraction and voxels with lower dose will received lower dose per fraction. So we cannot
        // directly transform each voxel assuming d dose per fraction. However, we can transform the prescription dose, assume
        // d dose per fraction, and then extract the corresponding number of fractions according to the EQD fractionation.
        // Using this number of fractions we can avoid having to specify the dose per fraction for a given voxel. 
        auto EQD_D = std::numeric_limits<double>::quiet_NaN(); // BED Dose.
        auto EQD_n = std::numeric_limits<double>::quiet_NaN(); // # of fractions.
        {
            auto BED_actual = BEDabr_from_n_D_abr(user_data_s->NumberOfFractions,
                                                  user_data_s->PrescriptionDose,
                                                  user_data_s->AlphaBetaRatioTumour);
            EQD_D = D_from_d_BEDabr(user_data_s->TargetDosePerFraction, BED_actual);
            EQD_n = EQD_D / user_data_s->TargetDosePerFraction;
        }

        f_bounded = [=](long int /*row*/, long int /*col*/, long int /*channel*/, std::reference_wrapper<planar_image<float,double>> /*img_refw*/, float &voxel_val) {
            if(voxel_val <= 0.0) return; // No-op if there is no dose.

            BEDabr BED_voxel;
            BED_voxel = BEDabr_from_n_D_abr(user_data_s->NumberOfFractions,
                                            voxel_val,
                                            user_data_s->AlphaBetaRatioTumour);
            voxel_val = D_from_n_BEDabr(EQD_n, BED_voxel);
            return;
        };

        f_unbounded = [=](long int /*row*/, long int /*col*/, long int /*channel*/, std::reference_wrapper<planar_image<float,double>> /*img_refw*/, float &voxel_val) {
            if(voxel_val <= 0.0) return; // No-op if there is no dose.

            BEDabr BED_voxel;
            BED_voxel = BEDabr_from_n_D_abr(user_data_s->NumberOfFractions,
                                            voxel_val,
                                            user_data_s->AlphaBetaRatioNormal);
            voxel_val = D_from_n_BEDabr(EQD_n, BED_voxel);
            return;
        };

        first_img_it->metadata["EQD_PrescriptionDose"] = std::to_string(user_data_s->PrescriptionDose);
        first_img_it->metadata["EQD_NumberOfFractions"] = std::to_string(user_data_s->NumberOfFractions);
        first_img_it->metadata["EQD_PrescriptionDose"] = std::to_string(EQD_D);
        first_img_it->metadata["EQD_NumberOfFractions"] = std::to_string(EQD_n);
        first_img_it->metadata["EQD_Model"] = "Simple LQ";
        first_img_it->metadata["EQD_TargetDosePerFraction"] = std::to_string(user_data_s->TargetDosePerFraction);
        first_img_it->metadata["EQD_NormalTissue_AlphaBetaRatio"] = std::to_string(user_data_s->AlphaBetaRatioNormal);
        first_img_it->metadata["EQD_TumourTissue_AlphaBetaRatio"] = std::to_string(user_data_s->AlphaBetaRatioTumour);

    }else{
        throw std::invalid_argument("Model not specified or invalid.");
    }

    std::list<std::reference_wrapper<planar_image<float,double>>> selected_imgs;
    for(auto &img_it : selected_img_its) selected_imgs.push_back( std::ref(*img_it) );

    Mutate_Voxels<float,double>( std::ref(*first_img_it),
                                 selected_imgs, 
                                 ccsl, 
                                 ebv_opts, 
                                 f_bounded,
                                 f_unbounded );

    //Alter the first image's metadata to reflect that averaging has occurred. You might want to consider
    // a selective whitelist approach so that unique IDs are not duplicated accidentally.
    UpdateImageDescription( std::ref(*first_img_it), "EQD" );
    UpdateImageWindowCentreWidth( std::ref(*first_img_it) );

    return true;
}

