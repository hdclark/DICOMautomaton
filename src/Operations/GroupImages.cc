//GroupImages.cc - A part of DICOMautomaton 2018. Written by hal clark.

#include <experimental/optional>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>    
#include <utility>            //Needed for std::pair.
#include <vector>

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "GroupImages.h"
#include "YgorImages.h"
#include "YgorString.h"       //Needed for GetFirstRegex(...)



OperationDoc OpArgDocGroupImages(void){
    OperationDoc out;
    out.name = "GroupImages";
    out.desc = 
        "This operation will group individual image slices into collections (Image_Arrays) based on the values"
        " of the specified metadata tags. DICOMautomaton operations are usually performed on containers rather"
        " than individual images, and grouping can express connections between images. For example a group"
        " could contain the scans belonging to a whole study, one of the series in a study, individual image "
        " volumes within a single series (e.g., a 3D volume in a temporal perfusion scan), or individual slices."
        " A group could also contain all the slices that intersect a given plane, or were taken on a specified"
        " StationName.";

    out.notes.emplace_back(
        "Images are moved, not copied."
    );

    out.notes.emplace_back(
        "Image order within a group is retained (i.e., stable grouping), but groups are appended to the back"
        " of the Image_Array list according to the default sort for the group's key-value value."
    );

    out.notes.emplace_back(
        "Images that do not contain the specified metadata will be grouped into a special N/A group at the end."
    );


    out.args.emplace_back();
    out.args.back().name = "ImageSelection";
    out.args.back().desc = "Images to operate on. Either 'none', 'last', or 'all'. Only images from the selection"
                           " will be grouped.";
    out.args.back().default_val = "all";
    out.args.back().expected = true;
    out.args.back().examples = { "last", "all" };


    out.args.emplace_back();
    out.args.back().name = "KeysCommon";
    out.args.back().desc = "Image metadata keys to use for exact-match groupings. For each group that is produced,"
                           " every image will share the same key-value pair. This is generally useful for non-numeric"
                           " (or integer, date, etc.) key-values. A ';'-delimited list can be specified to group"
                           " on multiple criteria simultaneously.";
    out.args.back().default_val = "";
    out.args.back().expected = true;
    out.args.back().examples = { "SeriesNumber",
                                 "BodyPartExamined;StudyDate",
                                 "SeriesInstanceUID", 
                                 "StationName" };

    return out;
}



Drover GroupImages(Drover DICOM_data, 
                           OperationArgPkg OptArgs, 
                           std::map<std::string,std::string> /*InvocationMetadata*/, 
                           std::string /*FilenameLex*/ ){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();

    const auto KeysCommonStr = OptArgs.getValueStr("KeysCommon").value();

    //-----------------------------------------------------------------------------------------------------------------
    const auto regex_none = std::regex("no?n?e?$", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto regex_last = std::regex("la?s?t?$", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto regex_all  = std::regex("al?l?$",   std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);

    if( !std::regex_match(ImageSelectionStr, regex_none)
    &&  !std::regex_match(ImageSelectionStr, regex_last)
    &&  !std::regex_match(ImageSelectionStr, regex_all) ){
        throw std::invalid_argument("Image selection is not valid. Cannot continue.");
    }

    std::vector<std::string> KeysCommon;
    for(auto a : SplitStringToVector(KeysCommonStr, ';', 'd')){
        KeysCommon.emplace_back(a);
    }

    std::map<std::vector<std::string>, 
             std::shared_ptr<Image_Array>> new_group;

    std::shared_ptr<Image_Array> na_group; // The special N/A group.

    //Image data.
    auto iap_it = DICOM_data.image_data.begin();
    if(false){
    }else if(std::regex_match(ImageSelectionStr, regex_none)){
        iap_it = DICOM_data.image_data.end();
    }else if(std::regex_match(ImageSelectionStr, regex_last)){
        if(!DICOM_data.image_data.empty()) iap_it = std::prev(DICOM_data.image_data.end());
    }
    while(iap_it != DICOM_data.image_data.end()){
        while(!(*iap_it)->imagecoll.images.empty()){
            auto img_it = (*iap_it)->imagecoll.images.begin();

            //Retrieve the selected metadata.
            std::vector<std::string> img_m;
            for(auto &akey : KeysCommon){
                auto v = img_it->GetMetadataValueAs<std::string>(akey);
                if(!v) break;
                img_m.push_back(v.value());
            }

            // If one or more metadata are missing.
            if(img_m.size() != KeysCommon.size()){
                if(na_group == nullptr) na_group = std::make_shared<Image_Array>();
                na_group->imagecoll.images.splice( na_group->imagecoll.images.end(),
                                                   (*iap_it)->imagecoll.images,
                                                   img_it );

            // Otherwise, if all metadata were present.
            }else{
                auto ia = new_group[img_m];
                if(ia == nullptr) ia = std::make_shared<Image_Array>();

                ia->imagecoll.images.splice( ia->imagecoll.images.end(),
                                             (*iap_it)->imagecoll.images,
                                             img_it );
                new_group[img_m] = ia;
            }

        }
        ++iap_it;
    }

    // Inject the new Image_Array groups.
    for(auto &p : new_group){
        DICOM_data.image_data.emplace_back( p.second );
    }
    if(na_group != nullptr){
        DICOM_data.image_data.emplace_back( na_group );
    }

    // Purge (all) empty Image_Arrays.
    DICOM_data.image_data.remove_if([](std::shared_ptr<Image_Array> &ia) -> bool {
        return ( (ia == nullptr) || (ia->imagecoll.images.empty()) );
    });

    return DICOM_data;
}
