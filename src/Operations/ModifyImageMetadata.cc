//ModifyImageMetadata.cc - A part of DICOMautomaton 2018. Written by hal clark.

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
#include <functional>

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../Metadata.h"

#include "YgorImages.h"
#include "YgorString.h"       //Needed for GetFirstRegex(...)
#include "YgorTime.h"

#include "ModifyImageMetadata.h"


OperationDoc OpArgDocModifyImageMetadata(){
    OperationDoc out;
    out.name = "ModifyImageMetadata";
    out.tags.emplace_back("category: image processing");
    out.tags.emplace_back("category: meta");

    out.desc = 
        "This operation injects metadata into images."
        " It can also modify image spatial characteristics, which are distinct from metadata.";

    out.args.emplace_back();
    out.args.back() = IAWhitelistOpArgDoc();
    out.args.back().name = "ImageSelection";
    out.args.back().default_val = "last";

    out.args.emplace_back();
    out.args.back() = MetadataInjectionOpArgDoc();
    out.args.back().name = "KeyValues";
    out.args.back().default_val = "";

    out.args.emplace_back();
    out.args.back().name = "SliceThickness";
    out.args.back().desc = "Image slices will be have this thickness (in DICOM units: mm)."
                           " For most purposes, SliceThickness should be equal to SpacingBetweenSlices."
                           " If SpacingBetweenSlices is smaller than SliceThickness, images will overlap."
                           " If SpacingBetweenSlices is larger than SliceThickness, there will be a gap between images."
                           " Updating the SliceThickness or image positioning using this operation will alter the"
                           " image, but will not update SpacingBetweenSlices. This gives the user freedom to"
                           " alter all image planes individually, allowing construction of non-rectilinear image"
                           " volumes. If SpacingBetweenSlices is known and consistent, it should be reflected"
                           " in the image metadata (by the user).";
    out.args.back().default_val = "1.0";
    out.args.back().expected = false;
    out.args.back().examples = { "0.1", "0.5", "1.0", "10.0" };

    out.args.emplace_back();
    out.args.back().name = "VoxelWidth";
    out.args.back().desc = "Voxels will have this (in-plane) width (in DICOM units: mm)."
                           " This means that the centre of two voxels that are in the same row but adjacent columns"
                           " will be separated by VoxelWidth."
                           " Each voxel will have dimensions: VoxelWidth x VoxelHeight x SliceThickness.";
    out.args.back().default_val = "1.0";
    out.args.back().expected = false;
    out.args.back().examples = { "0.1", "0.5", "1.0", "10.0" };

    out.args.emplace_back();
    out.args.back().name = "VoxelHeight";
    out.args.back().desc = "Voxels will have this (in-plane) height (in DICOM units: mm)."
                           " This means that the centre of two voxels that are in the same column but adjacent rows"
                           " will be separated by VoxelHeight."
                           " Each voxel will have dimensions: VoxelWidth x VoxelHeight x SliceThickness.";
    out.args.back().default_val = "1.0";
    out.args.back().expected = false;
    out.args.back().examples = { "0.1", "0.5", "1.0", "10.0" };


    out.args.emplace_back();
    out.args.back().name = "ImageAnchor";
    out.args.back().desc = "A point in 3D space which denotes the origin (in DICOM units: mm)."
                           " All other vectors are taken to be relative to this point."
                           " Under most circumstance the anchor should be (0,0,0)."
                           " Specify coordinates separated by commas.";
    out.args.back().default_val = "0.0, 0.0, 0.0";
    out.args.back().expected = false;
    out.args.back().examples = { "0.0, 0.0, 0.0", 
                                 "0.0,0.0,0.0",
                                 "1.0, -2.3, 45.6" };

    out.args.emplace_back();
    out.args.back().name = "ImagePosition";
    out.args.back().desc = "The centre of the row=0, column=0 voxel in the first image (in DICOM units: mm)."
                           " Specify coordinates separated by commas.";
    out.args.back().default_val = "0.0, 0.0, 0.0";
    out.args.back().expected = false;
    out.args.back().examples = { "0.0, 0.0, 0.0", 
                                 "100.0,100.0,100.0",
                                 "1.0, -2.3, 45.6" };

    out.args.emplace_back();
    out.args.back().name = "ImageOrientationColumn";
    out.args.back().desc = "The orientation unit vector that is aligned with image columns."
                           " Care should be taken to ensure ImageOrientationRow and ImageOrientationColumn are"
                           " orthogonal. (A Gram-Schmidt orthogonalization procedure ensures they are, but the"
                           " image orientation may not match the expected orientation.)"
                           " Note that the magnitude will also be scaled to unit length for convenience."
                           " Specify coordinates separated by commas.";
    out.args.back().default_val = "1.0, 0.0, 0.0";
    out.args.back().expected = false;
    out.args.back().examples = { "1.0, 0.0, 0.0", 
                                 "1.0, 1.0, 0.0",
                                 "0.0, 0.0, -1.0" };

    out.args.emplace_back();
    out.args.back().name = "ImageOrientationRow";
    out.args.back().desc = "The orientation unit vector that is aligned with image rows."
                           " Care should be taken to ensure ImageOrientationRow and ImageOrientationColumn are"
                           " orthogonal. (A Gram-Schmidt orthogonalization procedure ensures they are, but the"
                           " image orientation may not match the expected orientation.)"
                           " Note that the magnitude will also be scaled to unit length for convenience."
                           " Specify coordinates separated by commas.";
    out.args.back().default_val = "0.0, 1.0, 0.0";
    out.args.back().expected = false;
    out.args.back().examples = { "0.0, 1.0, 0.0", 
                                 "0.0, 1.0, 1.0",
                                 "-1.0, 0.0, 0.0" };

    return out;
}



bool ModifyImageMetadata(Drover &DICOM_data,
                           const OperationArgPkg& OptArgs,
                           std::map<std::string, std::string>& InvocationMetadata,
                           const std::string& /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();

    const auto KeyValuesOpt = OptArgs.getValueStr("KeyValues");

    const auto SliceThicknessOpt = OptArgs.getValueStr("SliceThickness");

    const auto VoxelWidthOpt = OptArgs.getValueStr("VoxelWidth");
    const auto VoxelHeightOpt = OptArgs.getValueStr("VoxelHeight");

    const auto ImageAnchorOpt = OptArgs.getValueStr("ImageAnchor");
    const auto ImagePositionOpt = OptArgs.getValueStr("ImagePosition");

    const auto ImageOrientationColumnOpt = OptArgs.getValueStr("ImageOrientationColumn");
    const auto ImageOrientationRowOpt = OptArgs.getValueStr("ImageOrientationRow");

    //-----------------------------------------------------------------------------------------------------------------
    if(!!(ImageOrientationColumnOpt) != !!(ImageOrientationRowOpt)){
        throw std::invalid_argument("Either both or neither of image orientation vectors must be provided.");
    }
    if(!!(ImageAnchorOpt) != !!(ImagePositionOpt)){
        throw std::invalid_argument("Either both or neither of image anchor and offset must be provided.");
    }
    if(!!(VoxelWidthOpt) != !!(VoxelHeightOpt)){
        throw std::invalid_argument("Either both or neither of voxel width and height must be provided.");
    }

    const auto parse_vec3 = [](const std::string &in) -> vec3<double> {
        auto out = vec3<double>(0.0, 0.0, 0.0);

        const auto xyz = SplitStringToVector(in, ',', 'd');
        if(xyz.size() != 3){
            throw std::invalid_argument("Unable to parse vec3 from '"_s + in + "'. Refusing to continue.");
        }
        out.x = std::stod(xyz.at(0));
        out.y = std::stod(xyz.at(1));
        out.z = std::stod(xyz.at(2));
        return out;
    };

    // Parse user-provided metadata, if any has been provided.
    const auto key_values = parse_key_values(KeyValuesOpt.value_or(""));

    // Implement changes for selected images.
    auto IAs_all = All_IAs( DICOM_data );
    auto IAs = Whitelist( IAs_all, ImageSelectionStr );
    for(auto & iap_it : IAs){
        for(auto &animg : (*iap_it)->imagecoll.images){

            // Update spatial characteristics as necessary.
            if(SliceThicknessOpt){
                const auto SliceThickness = std::stod(SliceThicknessOpt.value());

                animg.init_spatial(animg.pxl_dx, animg.pxl_dy, SliceThickness, animg.anchor, animg.offset);
                animg.metadata["SliceThickness"] = SliceThicknessOpt.value();
            }

            if(VoxelWidthOpt && VoxelHeightOpt){
                const auto VoxelWidth = std::stod(VoxelWidthOpt.value());
                const auto VoxelHeight = std::stod(VoxelHeightOpt.value());

                animg.init_spatial(VoxelWidth, VoxelHeight, animg.pxl_dz, animg.anchor, animg.offset);
                animg.metadata["PixelSpacing"] = std::to_string(VoxelHeight) + "\\" 
                                               + std::to_string(VoxelWidth);
            }

            if(ImageAnchorOpt && ImagePositionOpt){
                const auto ImageAnchor = parse_vec3(ImageAnchorOpt.value());
                const auto ImagePosition = parse_vec3(ImagePositionOpt.value());

                animg.init_spatial(animg.pxl_dx, animg.pxl_dy, animg.pxl_dz, ImageAnchor, ImagePosition);
                animg.metadata["ImagePositionPatient"] = ImagePositionOpt.value();
            }

            if(ImageOrientationRowOpt && ImageOrientationColumnOpt){
                auto ImageOrientationColumn = parse_vec3(ImageOrientationColumnOpt.value());
                auto ImageOrientationRow = parse_vec3(ImageOrientationRowOpt.value());
                auto ImageOrientationOrtho = ImageOrientationColumn.Cross( ImageOrientationRow ).unit();

                if(!ImageOrientationColumn.GramSchmidt_orthogonalize(ImageOrientationRow, ImageOrientationOrtho)){
                    throw std::invalid_argument("ImageOrientation vectors could not be orthogonalized. Refusing to continue.");
                }
                ImageOrientationColumn = ImageOrientationColumn.unit();
                ImageOrientationRow = ImageOrientationRow.unit();
                ImageOrientationOrtho = ImageOrientationOrtho.unit();

                animg.init_orientation(ImageOrientationRow, ImageOrientationColumn);
                animg.metadata["ImageOrientationPatient"] = std::to_string(ImageOrientationRow.x) + "\\"
                                                          + std::to_string(ImageOrientationRow.y) + "\\"
                                                          + std::to_string(ImageOrientationRow.z) + "\\"
                                                          + std::to_string(ImageOrientationColumn.x) + "\\"
                                                          + std::to_string(ImageOrientationColumn.y) + "\\"
                                                          + std::to_string(ImageOrientationColumn.z);
            }

            // Insert a copy of the user-provided key-values, but pre-process to replace macros and evaluate known
            // functions.
            auto l_key_values = key_values;
            inject_metadata( animg.metadata, std::move(l_key_values) );
        }
    }

    return true;
}
