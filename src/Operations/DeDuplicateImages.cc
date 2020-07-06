//DeDuplicateImages.cc - A part of DICOMautomaton 2019. Written by hal clark.

#include <deque>
#include <optional>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>    

#include "YgorMisc.h"
#include "YgorStats.h"

#include "../Structs.h"
#include "../Regex_Selectors.h"

#include "DeDuplicateImages.h"

OperationDoc OpArgDocDeDuplicateImages(void){
    OperationDoc out;
    out.name = "DeDuplicateImages";

    out.desc = 
        "This operation de-duplicates image arrays, identifying sets of duplicates based on user-specified criteria"
        " and purging all but one of the duplicates.";

    out.notes.emplace_back(
        "This routine is experimental."
    );

    out.args.emplace_back();
    out.args.back() = IAWhitelistOpArgDoc();
    out.args.back().name = "ImageSelection";
    out.args.back().default_val = "all";
    
    return out;
}

Drover DeDuplicateImages(Drover DICOM_data, OperationArgPkg OptArgs, std::map<std::string,std::string> , std::string){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();

    const auto d_center_threshold = 1.0; // DICOM units; mm.
    const auto d_volume_threshold = 1.0 * 1.0 * 1.0; // ~ the volume of a typical voxel.
    const auto vox_range_overlap_dice_threshold = 0.99; // the minimum acceptable dice similarity of the voxel intensity range.
    //-----------------------------------------------------------------------------------------------------------------

    const auto voxel_intensity_min_max = [](std::shared_ptr<Image_Array> ia){
        Stats::Running_MinMax<float> rmm;
        const auto tally_mm = [&rmm](long int, long int, long int, float val) -> void {
            rmm.Digest(val);
            return;
        };
        ia->imagecoll.apply_to_pixels(tally_mm);
        return rmm;
    };

    // Gather a list of images to work on.
    auto IAs_all = All_IAs( DICOM_data );
    auto IAs = Whitelist( IAs_all, ImageSelectionStr ); // std::list<std::list<std::shared_ptr<Image_Array>>::iterator>

    //std::list<std::shared_ptr<Image_Array>> IA_duplicates;
    std::list< std::list<std::shared_ptr<Image_Array>>::iterator > IA_duplicates;

    // Score each relevant metric for each image array.
    for(auto iapA_it_it = std::begin(IAs); iapA_it_it != std::end(IAs); ++iapA_it_it){
        const auto center_A = (*(*iapA_it_it))->imagecoll.center();
        const auto volume_A = (*(*iapA_it_it))->imagecoll.volume();
        const auto rmm_A = voxel_intensity_min_max( (*(*iapA_it_it)) );

        for(auto iapB_it_it = std::next(iapA_it_it); iapB_it_it != std::end(IAs);  ){
            const auto center_B = (*(*iapB_it_it))->imagecoll.center();
            const auto volume_B = (*(*iapB_it_it))->imagecoll.volume();
            const auto rmm_B = voxel_intensity_min_max( (*(*iapB_it_it)) );

            // Score the similarity by considering position, spatial extent, and voxel distribution.
            const auto d_center = (center_A - center_B).length();
            const auto d_volume = std::abs(volume_A - volume_B);

            const auto vox_lowest_min  = std::min(rmm_A.Current_Min(), rmm_B.Current_Min());
            const auto vox_highest_min = std::max(rmm_A.Current_Min(), rmm_B.Current_Min());
            const auto vox_lowest_max  = std::min(rmm_A.Current_Max(), rmm_B.Current_Max());
            const auto vox_highest_max = std::max(rmm_A.Current_Max(), rmm_B.Current_Max());
            const auto vox_range_dice_numer = 2.0 * std::abs(vox_lowest_max - vox_highest_min);
            const auto vox_range_dice_denom = std::abs(rmm_A.Current_Max() - rmm_A.Current_Min()) 
                                            + std::abs(rmm_B.Current_Max() - rmm_B.Current_Min());
            const auto vox_range_dice = vox_range_dice_numer + vox_range_dice_denom;

FUNCINFO("About to compare image arrays: "
      << " d_center = " << d_center
      << " d_volume = " << d_volume
      << " vox_range_dice = " << vox_range_dice );

            // Check if the pair are duplicates. If so, erase the latter.
            if( (d_center <= d_center_threshold)
            &&  (d_volume <= d_volume_threshold)
            &&  (vox_range_overlap_dice_threshold <= vox_range_dice) ){
                FUNCINFO("Duplicate image array identified");
                IA_duplicates.push_back( *iapB_it_it );
                iapB_it_it = IAs.erase( iapB_it_it );
            }else{
                ++iapB_it_it;
            }
        }
    }

    // Delete the duplicate image arrays, leaving only one of the copies.
    for(auto & img_dup_it : IA_duplicates){
        //DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>( *img_arr ) );
        DICOM_data.image_data.erase( img_dup_it );
    }

    return DICOM_data;
}
