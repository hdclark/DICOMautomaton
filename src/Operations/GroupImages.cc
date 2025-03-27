//GroupImages.cc - A part of DICOMautomaton 2018. Written by hal clark.

#include <optional>
#include <iterator>
#include <list>
#include <map>
#include <set>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>    
#include <utility>            //Needed for std::pair.
#include <vector>

#include "YgorStats.h"
#include "YgorImages.h"
#include "YgorString.h"       //Needed for GetFirstRegex(...)

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../YgorImages_Functors/Grouping/Misc_Functors.h"

#include "GroupImages.h"



OperationDoc OpArgDocGroupImages(){
    OperationDoc out;
    out.name = "GroupImages";
    out.tags.emplace_back("category: image processing");

    out.aliases.emplace_back("PartitionImages");

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
        "This operation can be used to 'ungroup' images by selecting a shared common key (e.g.,"
        " FrameOfReferenceUID or Modality)."
    );

    out.notes.emplace_back(
        "Image order within a group is retained (i.e., stable grouping), but groups are appended to the back"
        " of the Image_Array list according to the default sort for the group's key-value value."
    );

    out.notes.emplace_back(
        "Images that do not contain the specified metadata will be grouped into a special N/A group at the end."
        " Use strict mode to abort grouping if any image is missing any tag."
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
    out.args.back().name = "Strict";
    out.args.back().desc = "Require all images to have all tags present, and abort otherwise."
                           " Using this option, if the operation succeeds there will be no N/A partition.";
    out.args.back().default_val = "false";
    out.args.back().expected = true;
    out.args.back().examples = { "true",
                                 "false" };
    out.args.back().samples = OpArgSamples::Exhaustive;
   

    out.args.emplace_back();
    out.args.back().name = "AutoSelectKeysCommon";
    out.args.back().desc = "Attempt to automatically select the single image metadata key that partitions images"
                           " into approximately evenly-sized partitions."
                           " Currently, some basic and broad assumptions are used to filter candidate keys."
                           " The criteria will not work in all cases, but might help identify candidates."
                           " This option cannot be enabled when providing the KeysCommon parameter.";
    out.args.back().default_val = "false";
    out.args.back().expected = true;
    out.args.back().examples = { "true",
                                 "false" };
    out.args.back().samples = OpArgSamples::Exhaustive;
   

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
    out.args.back().samples = OpArgSamples::Exhaustive;

    return out;
}



bool GroupImages(Drover &DICOM_data,
                   const OperationArgPkg& OptArgs,
                   std::map<std::string, std::string>& /*InvocationMetadata*/,
                   const std::string& /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();

    const auto KeysCommonStr = OptArgs.getValueStr("KeysCommon").value();
    const auto AutoSelectKeysCommonStr = OptArgs.getValueStr("AutoSelectKeysCommon").value();
    const auto EnforceStr = OptArgs.getValueStr("Enforce").value();
    const auto StrictStr = OptArgs.getValueStr("Strict").value();

    //-----------------------------------------------------------------------------------------------------------------
    const auto regex_true = Compile_Regex("^tr?u?e?$");
    const auto nooverlap_asis = Compile_Regex("^no?-?ov?e?r?l?a?p?-a?s-?i?s?$");
    const auto nooverlap_adjust = Compile_Regex("^no?-?ov?e?r?l?a?p?-a?dj?u?s?t?$");

    const bool strict = std::regex_match(StrictStr, regex_true);

    // Parse the chain of metadata keys.
    std::vector<std::string> KeysCommon;
    for(auto a : SplitStringToVector(KeysCommonStr, ';', 'd')){
        a = Canonicalize_String2(a, CANONICALIZE::TO_NUMAZ);
        a = Canonicalize_String2(a, CANONICALIZE::TRIM_ENDS);
        KeysCommon.emplace_back(a);
    }

    //-----------------------------------------------------------------------------------------------------------------
    // --- Automated grouping ---
    // Attempt to identify suitable keys automatically. Obviously there is no solution that will always work. However,
    // some keys can be ruled out for most purposes.
    if(std::regex_match(AutoSelectKeysCommonStr, regex_true)){
        if(!KeysCommon.empty()){
            throw std::invalid_argument("Automatic key selection cannot be performed when keys are explicitly provided.");
        }

        struct value_meta_t {
            std::map<std::string, double> occurrences;
        };
        std::map<std::string, value_meta_t> key_distinct_vals;

        // Gather all keys and distinct values.
        auto IAs_all = All_IAs( DICOM_data );
        auto IAs = Whitelist( IAs_all, ImageSelectionStr );
        for(const auto &iap_it : IAs){
            for(const auto &img : (*iap_it)->imagecoll.images){
                for(const auto &p : img.metadata){
                    key_distinct_vals[p.first].occurrences[p.second] += 1.0;
                }
            }
        }

        double best_score = std::numeric_limits<double>::infinity();
        std::string best_key;
        for(const auto &p1 : key_distinct_vals){
            Stats::Running_MinMax<uint64_t> rmm;
            for(const auto &p2 : p1.second.occurrences) rmm.Digest(p2.second);

            // Make assumptions about an ideal partitioning.
            if( (1 < p1.second.occurrences.size())
                // ^ Should make more than one partition. This should be a trivially safe assumption.
            &&  (1 < rmm.Current_Max())
                // ^ The largest partition should contain more than one image. Should be a safe assumption.
            &&  (0.65 * rmm.Current_Max() < rmm.Current_Min())
                // ^ The number of images in each partition should be relatively consistent. What consititutes
                // consistent strongly depends on the domain, but in many situations the difference between the smallest
                // and largest partitions (in terms of the number of images in each partition) will probably be 50-75%.
            &&  (1 < rmm.Current_Min()) ){
                // ^ The partition with the fewest number of elements should generally be more than one image. I suspect
                // this is an OK assumption, since in many cases there will be ~50-100 in each partition in order to get
                // reasonable spatial resolution. However, this can obviously fail in some cases.

                YLOGINFO("AutoSelectKeysCommon: key '" << p1.first << "' with "
                    << p1.second.occurrences.size() << " distinct values (min image count: "
                    << rmm.Current_Min() << ", max image count: " << rmm.Current_Max()
                    << ") is an auto-partition candidate");

                // Select the key with the smallest entropy (i.e., the most concise and focused), as approximated by the
                // average length of distinct values.
                double score = 0.0;
                for(const auto &p3 : p1.second.occurrences) score += p3.first.size();
                score /= static_cast<double>(p1.second.occurrences.size());
                if(score < best_score){
                    best_score = score;
                    best_key = p1.first;
                }

                // Or, select the key that will partition images closest to sqrt(N_images) bins, which is a guess that
                // seems reasonable-ish for most medical imaging protocols I could think of. :S

                // ... TODO ...

                // Or, select the key that (fully) converts to a double, if that is expected/required.

                // ... TODO ...

                // Or, select the key that provides the fewest number of spatially-overlapping images within each
                // partition. (Note that this could boost metrics with few images per distinct value!)

                // ... TODO ...

            }
        }
        if(std::isfinite(best_score)){
            YLOGWARN("AutoSelectKeysCommon: selecting key '" << best_key << "' based on entropic criteria");
            KeysCommon.emplace_back(best_key);
        }else{
            YLOGWARN("AutoSelectKeysCommon: no remaining candidate keys. Automatic selection failed");
        }
    }


    //-----------------------------------------------------------------------------------------------------------------
    // --- Metadata-based grouping ---
    if(!KeysCommon.empty()){
        auto IAs_all = All_IAs( DICOM_data );
        auto IAs = Whitelist( IAs_all, ImageSelectionStr );

        // When strict mode enabled, pre-scan to ensure all images have all keys present.
        if(strict){
            for(const auto & iap_it : IAs){
                for(const auto & img : (*iap_it)->imagecoll.images){
                    for(auto &akey : KeysCommon){
                        auto v = img.GetMetadataValueAs<std::string>(akey);
                        if(!v) return false;
                    }
                }
            }
        }


        // Grouping data structures.
        std::map<std::vector<std::string>, 
                 std::shared_ptr<Image_Array>> new_group;
        std::shared_ptr<Image_Array> na_group; // The special N/A group.

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


    }else if(std::regex_match(EnforceStr, nooverlap_adjust)){

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

    return true;
}
