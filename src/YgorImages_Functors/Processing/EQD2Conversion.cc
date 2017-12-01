
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

#include "EQD2Conversion.h"

bool EQD2Conversion(planar_image_collection<float,double>::images_list_it_t first_img_it,
                       std::list<planar_image_collection<float,double>::images_list_it_t> selected_img_its,
                       std::list<std::reference_wrapper<planar_image_collection<float,double>>>,
                       std::list<std::reference_wrapper<contour_collection<double>>> ccsl, 
                       std::experimental::any user_data){

    //This routine converts voxel intensities (dose) into EQD2 doses -- the dose BED-based dose equivalent if
    // the radiation were delivered in 2Gy fractions. 
    //
    // Note the the NumberOfFractions must be specified. The DosePerFraction would need to be provided for
    // each individual voxel!
    //

    EQD2ConversionUserData *user_data_s;
    try{
        user_data_s = std::experimental::any_cast<EQD2ConversionUserData *>(user_data);
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

    auto f_bounded = [=](long int /*row*/, long int /*col*/, long int /*channel*/, float &voxel_val) {
        if(voxel_val <= 0.0) return; // No-op if there is no dose.

        BEDabr BED_voxel;
        if(user_data_s->NumberOfFractions > 0.0){
            BED_voxel = BEDabr_from_n_D_abr(user_data_s->NumberOfFractions,
                                            voxel_val,
                                            user_data_s->AlphaBetaRatioTumour);
        }else{
            throw std::invalid_argument("This routine requires NumberOfFractions to be specified");
        }
        voxel_val = D_from_d_BEDabr(2.0, BED_voxel);
        return;
    };

    auto f_unbounded = [=](long int /*row*/, long int /*col*/, long int /*channel*/, float &voxel_val) {
        if(voxel_val <= 0.0) return; // No-op if there is no dose.

        BEDabr BED_voxel;
        if(user_data_s->NumberOfFractions > 0.0){
            BED_voxel = BEDabr_from_n_D_abr(user_data_s->NumberOfFractions,
                                            voxel_val,
                                            user_data_s->AlphaBetaRatioNormal);
        }else{
            throw std::invalid_argument("This routine requires NumberOfFractions to be specified");
        }
        voxel_val = D_from_d_BEDabr(2.0, BED_voxel);
        return;
    };

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
    UpdateImageDescription( std::ref(*first_img_it), "EQD2" );
    UpdateImageWindowCentreWidth( std::ref(*first_img_it) );

    return true;
}

