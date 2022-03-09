//OrderImages.cc - A part of DICOMautomaton 2022. Written by hal clark.

#include <optional>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>    
#include <utility>            //Needed for std::pair.
#include <vector>

#include "YgorImages.h"
#include "YgorString.h"       //Needed for GetFirstRegex(...)

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../Metadata.h"

#include "OrderImages.h"



OperationDoc OpArgDocOrderImages(){
    OperationDoc out;
    out.name = "OrderImages";
    out.desc = 
        "This operation will order either individual image slices within each image array, or image arrays"
        " based on the values of the specified metadata tags.";

    out.notes.emplace_back(
        "Images and image arrays are moved, not copied."
    );
    out.notes.emplace_back(
        "Image arrays (groupings) are always retained, though the order of images within each array"
        " and the order of the arrays themselves will change."
    );
    out.notes.emplace_back(
        "Images that do not contain the specified metadata will be placed at the end."
        " Similarly, image arrays that do not have consensus (i.e., the constituent images have heterogeneous"
        " metadata) will be placed at the end."
    );
    out.notes.emplace_back(
        "Image array sorting permits selection of specific image arrays. Only selected arrays will participate"
        " in the sort, and sorted selection will be reinjected such that the position of all unselected arrays"
        " remain unchanged. For example, representing unselected arrays as letters (ABC...) and selected arrays"
        " as numbers (123...) then sorting 'AB3C12' would result in 'AB1C23'. Note that the unselected arrays"
        " do not move, even when the selected arrays are reordered."
    );


    out.args.emplace_back();
    out.args.back() = IAWhitelistOpArgDoc();
    out.args.back().name = "ImageSelection";
    out.args.back().default_val = "all";


    out.args.emplace_back();
    out.args.back().name = "Variant";
    out.args.back().desc = "Controls whether images (internal) or image arrays (external) are sorted.";
    out.args.back().default_val = "internal";
    out.args.back().expected = true;
    out.args.back().examples = { "internal",
                                 "external" };
    out.args.back().samples = OpArgSamples::Exhaustive;

    out.args.emplace_back();
    out.args.back().name = "Key";
    out.args.back().desc = "Metadata key to use for ordering."
                           " Values will be sorted according to a 'natural' sorting order, which"
                           " greedily compares sub-strings of numbers and characters separately."
                           " Note this ordering is expected to be stable, but may not always be on some systems.";
    out.args.back().default_val = "";
    out.args.back().expected = false;
    out.args.back().examples = { "AcquisitionTime",
                                 "ContentTime",
                                 "SeriesNumber", 
                                 "SeriesDescription" };

    out.args.emplace_back();
    out.args.back().name = "Unit";
    out.args.back().desc = "Unit vector use for spatial ordering."
                           " Images will be sorted according to the position of the corner nearest the (0,0) voxel"
                           " along the given unit vector. For image arrays, the 'first' image is used -- which"
                           " occurs 'first' can be controlled by first sorting internally.";
    out.args.back().default_val = "";
    out.args.back().expected = false;
    out.args.back().examples = { "(0.0, 0.0, 1.0)",
                                 "(0.0, -1.0, 0.0)",
                                 "(0.1, -0.2, 0.3)" };

    return out;
}


bool OrderImages(Drover &DICOM_data,
                 const OperationArgPkg& OptArgs,
                 std::map<std::string, std::string>& /*InvocationMetadata*/,
                 const std::string& /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();

    const auto VariantStr = OptArgs.getValueStr("Variant").value();
    const auto KeyStrOpt = OptArgs.getValueStr("Key");
    const auto UnitStrOpt = OptArgs.getValueStr("Unit");

    //-----------------------------------------------------------------------------------------------------------------
    const auto regex_internal = Compile_Regex("^int?e?r?n?a?l?");
    const auto regex_external = Compile_Regex("^ext?e?r?n?a?l?");

    if( std::regex_match(VariantStr, regex_internal) ){
        using img_t = planar_image<float,double>;

        // Define an ordering that will work for words/characters and numbers mixed together.
        const auto key_ordering = [KeyStrOpt]( const img_t &A, const img_t &B ) -> bool {
            const auto A_opt = A.GetMetadataValueAs<std::string>(KeyStrOpt.value());
            const auto B_opt = B.GetMetadataValueAs<std::string>(KeyStrOpt.value());
            
            return natural_lt( A_opt, B_opt );
        };

        // Spatial ordering.
        const auto unit_ordering = [UnitStrOpt]( const img_t &A, const img_t &B ) -> bool {
            const auto A_pos = A.anchor + A.offset;
            const auto B_pos = B.anchor + B.offset;

            const auto unit = vec3<double>().from_string( UnitStrOpt.value() ).unit();

            const auto A_proj = A_pos.Dot(unit);
            const auto B_proj = B_pos.Dot(unit);
            return (A_proj < B_proj);
        };

        auto IAs_all = All_IAs( DICOM_data );
        auto IAs = Whitelist( IAs_all, ImageSelectionStr );
        for(auto & iap_it : IAs){
            if(KeyStrOpt){
                (*iap_it)->imagecoll.images.sort( key_ordering );
            }else if(UnitStrOpt){
                (*iap_it)->imagecoll.images.sort( unit_ordering );
            }
        }

    }else if( std::regex_match(VariantStr, regex_external) ){
        using img_arr_ptr_t = std::shared_ptr<Image_Array>;

        // Maintain the relative ordering of selected vs un-selected image arrays by sorting with proxy objects.
        //
        // Unselected image arrays will retain their position, but selected image arrays will be extracted, sorted, and
        // re-inserted to maintain unselected positions.
        //
        // Note that using proxy objects also provides transactional behaviour -- exceptions won't result in data loss.
        auto IAs_all = All_IAs( DICOM_data );
        auto IAs = Whitelist( IAs_all, ImageSelectionStr );

        // The selection are isolated to simplify later sorting.
        std::list<img_arr_ptr_t> selected;
        for(auto& img_arr_ptr_it : IAs){
            selected.emplace_back( *img_arr_ptr_it );
        }

        // All image arrays are assessed to determine if they were selected, but also to simplify later re-insertion.
        struct tagged_all_t {
            bool selected;
            img_arr_ptr_t img_arr_ptr;
        };
        std::list<tagged_all_t> all_img_arrs;
        for(auto& img_arr_ptr : DICOM_data.image_data){
            all_img_arrs.emplace_back();
            const auto l_selected = (std::find(std::begin(selected), std::end(selected), img_arr_ptr) != std::end(selected));
            all_img_arrs.back().selected = l_selected;
            if(l_selected) all_img_arrs.back().img_arr_ptr = img_arr_ptr;
        }

        // Sort the selected images arrays.

        // Define an ordering that will work for words/characters and numbers mixed together.
        const auto key_ordering = [KeyStrOpt]( const img_arr_ptr_t &A,
                                               const img_arr_ptr_t &B ) -> bool {
            std::optional<std::string> A_opt;
            if(A != nullptr){
                auto A_m = A->imagecoll.get_common_metadata({});
                A_opt = get_as<std::string>(A_m, KeyStrOpt.value());
            }

            std::optional<std::string> B_opt;
            if(B != nullptr){
                auto B_m = B->imagecoll.get_common_metadata({});
                B_opt = get_as<std::string>(B_m, KeyStrOpt.value());
            }
            
            return natural_lt( A_opt, B_opt );
        };

        // Spatial ordering.
        const auto unit_ordering = [UnitStrOpt]( const img_arr_ptr_t &A,
                                                 const img_arr_ptr_t &B ) -> bool {
            const bool A_valid =  (A != nullptr)
                               && !A->imagecoll.images.empty()
                               && A->imagecoll.images.front().anchor.isfinite()
                               && A->imagecoll.images.front().offset.isfinite();
            const bool B_valid =  (B != nullptr)
                               && !B->imagecoll.images.empty()
                               && B->imagecoll.images.front().anchor.isfinite()
                               && B->imagecoll.images.front().offset.isfinite();

            // Handle degenerate cases.
            if(false){
            }else if( A_valid && !B_valid ){
                return true;  // Known before unknown.
            }else if(!A_valid &&  B_valid ){
                return false; // Known before unknown.
            }else if(!A_valid && !B_valid ){
                return false; // Both unknown. Considered equal.
            }

            // Handle full case.
            const auto A_pos = A->imagecoll.images.front().anchor + A->imagecoll.images.front().offset;
            const auto B_pos = B->imagecoll.images.front().anchor + B->imagecoll.images.front().offset;

            const auto unit = vec3<double>().from_string( UnitStrOpt.value() ).unit();

            const auto A_proj = A_pos.Dot(unit);
            const auto B_proj = B_pos.Dot(unit);
            return (A_proj < B_proj);
        };

        if(KeyStrOpt){
            selected.sort( key_ordering );
        }else if(UnitStrOpt){
            selected.sort( unit_ordering );
        }

        // Re-insert the sorted selection back into the Drover class.
        std::list<std::shared_ptr<Image_Array>> image_data;
        for(auto& tia : all_img_arrs){
            if(!tia.selected){
                // Insert the unselected image array directly.
                image_data.emplace_back(tia.img_arr_ptr);
            }else{
                // 'Steal' the first available sorted image array to take the previous array's place.
                if(selected.empty()){
                    throw std::logic_error("Sort stability broken, expecting another selected image. Aborting transaction");
                }
                image_data.splice( std::end(image_data),
                                   selected,
                                   std::begin(selected) );
            }
        }
        if(!selected.empty()){
            throw std::logic_error("Unused selected images were not reinjected back into Drover object. Aborting transaction");
        }
        std::swap(DICOM_data.image_data, image_data);

    }else{
        throw std::invalid_argument("Variant not understood");
    }

    return true;
}
