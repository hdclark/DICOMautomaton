//GroupImages.cc - A part of DICOMautomaton 2018. Written by hal clark.

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
#include "../YgorImages_Functors/Grouping/Misc_Functors.h"

#include "GroupImages.h"



OperationDoc OpArgDocGroupImages(){
    OperationDoc out;
    out.name = "GroupImages";
    out.desc = 
        "This operation will group individual image slices into partitions (Image_Arrays) based on the values"
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
    out.args.back() = IAWhitelistOpArgDoc();
    out.args.back().name = "ImageSelection";
    out.args.back().default_val = "all";


    out.args.emplace_back();
    out.args.back().name = "KeysCommon";
    out.args.back().desc = "Image metadata keys to use for exact-match groupings. For each group that is produced,"
                           " every image will share the same key-value pair. This is generally useful for non-numeric"
                           " (or integer, date, etc.) key-values. A ';'-delimited list can be specified to group"
                           " on multiple criteria simultaneously. An empty string disables metadata-based grouping.";
    out.args.back().default_val = "";
    out.args.back().expected = true;
    out.args.back().examples = { "SeriesNumber",
                                 "BodyPartExamined;StudyDate",
                                 "SeriesInstanceUID", 
                                 "StationName" };


    out.args.emplace_back();
    out.args.back().name = "Enforce";
    out.args.back().desc = "Other specialized grouping operations that involve custom logic."
                           " Currently, only 'no-overlap' is available, but it has two variants."
                           " Both partition based on the spatial extent of images;"
                           " in each non-overlapping partition, no two images will spatially overlap."
                           " 'No-overlap-as-is' will effectively insert partitions without altering the order."
                           " A partition is inserted when an image is found to overlap with an image already"
                           " within the partition."
                           " For this grouping to be useful, images must be sorted so that partitions"
                           " can be inserted without any necessary reordering."
                           " An example of when this grouping is useful is CT shuttling in which the ordering"
                           " of images alternate between increasing and decreasing SliceNumber."
                           " Note that, depending on the ordering, some partitions may therefore be incomplete."
                           " 'No-overlap-adjust' will rearrange images so that the first partition is always"
                           " complete. This is achieved by building a queue of spatially-overlapping images"
                           " and greedily stealing one of each kind when constructing partitions."
                           " An example of when this grouping is useful are 4DCTs which have been acquired for"
                           " all phases while the couch remains at a single SliceNumber; the images are"
                           " ordered on disk in the acquisition order (i.e., alike SliceNumbers are bunched"
                           " together) but the logical analysis order is that each contiguous volume should"
                           " represent a single phase."
                           " An empty string disables logic-based grouping.";
    out.args.back().default_val = "";
    out.args.back().expected = true;
    out.args.back().examples = { "no-overlap-as-is",
                                 "no-overlap-adjust" };

    return out;
}



Drover GroupImages(Drover DICOM_data, 
                           OperationArgPkg OptArgs, 
                           std::map<std::string,std::string> /*InvocationMetadata*/, 
                           std::string /*FilenameLex*/ ){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();

    const auto KeysCommonStr = OptArgs.getValueStr("KeysCommon").value();
    const auto EnforceStr = OptArgs.getValueStr("Enforce").value();

    //-----------------------------------------------------------------------------------------------------------------
    const auto nooverlap_asis = Compile_Regex("^no?-?ov?e?r?l?a?p?-a?s-?i?s?$");
    const auto nooverlap_adjust = Compile_Regex("^no?-?ov?e?r?l?a?p?-a?dj?u?s?t?$");

    //-----------------------------------------------------------------------------------------------------------------
    // --- Metadata-based grouping ---

    // Parse the chain of metadata keys.
    std::vector<std::string> KeysCommon;
    for(auto a : SplitStringToVector(KeysCommonStr, ';', 'd')){
        KeysCommon.emplace_back(a);
    }

    if(!KeysCommon.empty()){
        // Grouping data structures.
        std::map<std::vector<std::string>, 
                 std::shared_ptr<Image_Array>> new_group;
        std::shared_ptr<Image_Array> na_group; // The special N/A group.

        auto IAs_all = All_IAs( DICOM_data );
        auto IAs = Whitelist( IAs_all, ImageSelectionStr );
        for(auto & iap_it : IAs){
            while(!(*iap_it)->imagecoll.images.empty()){
                auto img_it = (*iap_it)->imagecoll.images.begin();

                //Retrieve the selected metadata.
                std::vector<std::string> img_m;
                for(auto &akey : KeysCommon){
                    auto v = img_it->GetMetadataValueAs<std::string>(akey);
                    if(!v) break;
                    img_m.push_back(v.value());
                }

                // If one or more metadata are missing, the image gets moved to the special NA group.
                if(img_m.size() != KeysCommon.size()){
                    if(na_group == nullptr) na_group = std::make_shared<Image_Array>();
                    na_group->imagecoll.images.splice( na_group->imagecoll.images.end(),
                                                       (*iap_it)->imagecoll.images,
                                                       img_it );

                // Otherwise, if all metadata were present move the image to the corresponding group.
                }else{
                    auto ia = new_group[img_m];
                    if(ia == nullptr) ia = std::make_shared<Image_Array>();

                    ia->imagecoll.images.splice( ia->imagecoll.images.end(),
                                                 (*iap_it)->imagecoll.images,
                                                 img_it );
                    new_group[img_m] = ia;
                }

            }
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
    }


    //-----------------------------------------------------------------------------------------------------------------
    // --- Logic-based grouping ---

    if(std::regex_match(EnforceStr, nooverlap_asis)){

        // Grouping data structures.
        std::list<std::shared_ptr<Image_Array>> new_groups;
        std::shared_ptr<Image_Array> shtl;

        auto IAs_all = All_IAs( DICOM_data );
        auto IAs = Whitelist( IAs_all, ImageSelectionStr );
        for(auto & iap_it : IAs){
            while(!(*iap_it)->imagecoll.images.empty()){
                auto img_it = (*iap_it)->imagecoll.images.begin();

                // Cycle through the shuttle, testing if this image overlaps with any in the shuttle.
                if(shtl == nullptr) shtl = std::make_shared<Image_Array>();
                auto overlapping_imgs = GroupSpatiallyOverlappingImages(img_it, std::ref(shtl->imagecoll));
                bool overlaps = (!overlapping_imgs.empty());

                // If there was overlap, commit the shuttle as a new group and start a new shuttle with this image.
                if(overlaps){
                    new_groups.emplace_back();
                    new_groups.back().swap( shtl );
                }

                // Regardless of overlap, add the image to the shuttle before continuing.
                if(shtl == nullptr) shtl = std::make_shared<Image_Array>();
                shtl->imagecoll.images.splice( shtl->imagecoll.images.end(),
                                               (*iap_it)->imagecoll.images,
                                               img_it );
                // ....
            }

            // Add the remainder of the shuttle as a new group iff it is not empty.
            if(shtl == nullptr) shtl = std::make_shared<Image_Array>();
            if(!(shtl->imagecoll.images.empty())){
                new_groups.emplace_back();
                new_groups.back().swap( shtl );
            }
        }

        // Inject the new Image_Array groups.
        for(auto &g : new_groups){
            DICOM_data.image_data.emplace_back( g );
        }

        // Purge (all) empty Image_Arrays.
        DICOM_data.image_data.remove_if([](std::shared_ptr<Image_Array> &ia) -> bool {
            return ( (ia == nullptr) || (ia->imagecoll.images.empty()) );
        });
    }


    if(std::regex_match(EnforceStr, nooverlap_adjust)){

        // Grouping data structures.
        std::list<std::shared_ptr<Image_Array>> new_groups;

        auto IAs_all = All_IAs( DICOM_data );
        auto IAs = Whitelist( IAs_all, ImageSelectionStr );
        for(auto & iap_it : IAs){
            std::list<std::shared_ptr<Image_Array>> phases;

            auto all_images = (*iap_it)->imagecoll.get_all_images();
            while(!all_images.empty()){
                // Find the images which spatially overlap with this image.
                auto curr_img_it = all_images.front();
                auto selected_imgs = GroupSpatiallyOverlappingImages(curr_img_it, std::ref((*iap_it)->imagecoll));

                if(selected_imgs.empty()){
                    throw std::logic_error("No spatially-overlapping images found. There should be at least one"
                                           " image (the 'seed' image) which should match. Verify the spatial" 
                                           " overlap grouping routine.");
                }
                for(auto &an_img_it : selected_imgs){
                     all_images.remove(an_img_it); //std::list::remove() erases all elements equal to input value.
                }

                // Establish the images as comprising a whole 'phase.'
                phases.emplace_back();
                phases.back() = std::make_shared<Image_Array>();
                for(auto &an_img_it : selected_imgs){
                    phases.back()->imagecoll.images.splice( phases.back()->imagecoll.images.end(),
                                                            (*iap_it)->imagecoll.images,
                                                            an_img_it );
                }
            }

            while(true){
                std::shared_ptr<Image_Array> shtl;
                shtl = std::make_shared<Image_Array>();
                bool AllConsumed = true;

                // Walk the phases, stealing the first available image from each.
                for(auto &ia_phase : phases){
                    if(ia_phase->imagecoll.images.empty()) continue;
                    AllConsumed = false;
                    shtl->imagecoll.images.splice( shtl->imagecoll.images.end(),
                                                   ia_phase->imagecoll.images,
                                                   ia_phase->imagecoll.images.begin() );
                }
                if(AllConsumed) break; // All images were exhausted.

                new_groups.emplace_back();
                new_groups.back().swap( shtl );
            }
        }

        // Inject the new Image_Array groups.
        for(auto &g : new_groups){
            DICOM_data.image_data.emplace_back( g );
        }

        // Purge (all) empty Image_Arrays.
        DICOM_data.image_data.remove_if([](std::shared_ptr<Image_Array> &ia) -> bool {
            return ( (ia == nullptr) || (ia->imagecoll.images.empty()) );
        });
    }

    return DICOM_data;
}
