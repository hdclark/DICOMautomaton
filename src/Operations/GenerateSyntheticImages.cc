//GenerateSyntheticImages.cc - A part of DICOMautomaton 2019. Written by hal clark.

#include <algorithm>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>    

#include "../Imebra_Shim.h"
#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "Explicator.h"       //Needed for Explicator class.
#include "GenerateSyntheticImages.h"
#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorString.h"       //Needed for GetFirstRegex(...)


OperationDoc OpArgDocGenerateSyntheticImages(){
    OperationDoc out;
    out.name = "GenerateSyntheticImages";
    out.desc = 
        "This operation generates a synthetic, regular bitmap image array."
        " It can be used for testing how images are quantified or transformed.";

    out.args.emplace_back();
    out.args.back().name = "NumberOfImages";
    out.args.back().desc = "The number of images to create.";
    out.args.back().default_val = "100";
    out.args.back().expected = true;
    out.args.back().examples = { "1", "100", "1000" };

    out.args.emplace_back();
    out.args.back().name = "NumberOfRows";
    out.args.back().desc = "The number of rows each image should contain.";
    out.args.back().default_val = "256";
    out.args.back().expected = true;
    out.args.back().examples = { "1", "100", "1000" };

    out.args.emplace_back();
    out.args.back().name = "NumberOfColumns";
    out.args.back().desc = "The number of columns each image should contain.";
    out.args.back().default_val = "256";
    out.args.back().expected = true;
    out.args.back().examples = { "1", "100", "1000" };

    out.args.emplace_back();
    out.args.back().name = "NumberOfChannels";
    out.args.back().desc = "The number of channels each image should contain.";
    out.args.back().default_val = "1";
    out.args.back().expected = true;
    out.args.back().examples = { "1", "10", "100" };

    out.args.emplace_back();
    out.args.back().name = "SliceThickness";
    out.args.back().desc = "Image slices will be have this thickness (in DICOM units: mm)."
                           " For most purposes, SliceThickness should be equal to SpacingBetweenSlices."
                           " If SpacingBetweenSlices is smaller than SliceThickness, images will overlap."
                           " If SpacingBetweenSlices is larger than SliceThickness, there will be a gap between images.";
    out.args.back().default_val = "1.0";
    out.args.back().expected = true;
    out.args.back().examples = { "0.1", "0.5", "1.0", "10.0" };

    out.args.emplace_back();
    out.args.back().name = "SpacingBetweenSlices";
    out.args.back().desc = "Image slice centres will be separated by this distance (in DICOM units: mm)."
                           " For most purposes, SpacingBetweenSlices should be equal to SliceThickness."
                           " If SpacingBetweenSlices is smaller than SliceThickness, images will overlap."
                           " If SpacingBetweenSlices is larger than SliceThickness, there will be a gap between images.";
    out.args.back().default_val = "1.0";
    out.args.back().expected = true;
    out.args.back().examples = { "0.1", "0.5", "1.0", "10.0" };

    out.args.emplace_back();
    out.args.back().name = "VoxelWidth";
    out.args.back().desc = "Voxels will have this (in-plane) width (in DICOM units: mm)."
                           " This means that row-adjacent voxels centres will be separated by VoxelWidth)."
                           " Each voxel will have dimensions: VoxelWidth x VoxelHeight x SliceThickness.";
    out.args.back().default_val = "1.0";
    out.args.back().expected = true;
    out.args.back().examples = { "0.1", "0.5", "1.0", "10.0" };

    out.args.emplace_back();
    out.args.back().name = "VoxelHeight";
    out.args.back().desc = "Voxels will have this (in-plane) height (in DICOM units: mm)."
                           " This means that column-adjacent voxels centres will be separated by VoxelHeight)."
                           " Each voxel will have dimensions: VoxelWidth x VoxelHeight x SliceThickness.";
    out.args.back().default_val = "1.0";
    out.args.back().expected = true;
    out.args.back().examples = { "0.1", "0.5", "1.0", "10.0" };


    out.args.emplace_back();
    out.args.back().name = "ImageAnchor";
    out.args.back().desc = "A point in 3D space which denotes the origin (in DICOM units: mm)."
                           " All other vectors are taken to be relative to this point."
                           " Under most circumstance the anchor should be (0,0,0)."
                           " Specify coordinates separated by commas.";
    out.args.back().default_val = "0.0, 0.0, 0.0";
    out.args.back().expected = true;
    out.args.back().examples = { "0.0, 0.0, 0.0", 
                                 "0.0,0.0,0.0",
                                 "1.0, -2.3, 45.6" };

    out.args.emplace_back();
    out.args.back().name = "ImagePosition";
    out.args.back().desc = "The centre of the row=0, column=0 voxel in the first image (in DICOM units: mm)."
                           " Specify coordinates separated by commas.";
    out.args.back().default_val = "0.0, 0.0, 0.0";
    out.args.back().expected = true;
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
    out.args.back().expected = true;
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
    out.args.back().expected = true;
    out.args.back().examples = { "0.0, 1.0, 0.0", 
                                 "0.0, 1.0, 1.0",
                                 "-1.0, 0.0, 0.0" };

    out.args.emplace_back();
    out.args.back().name = "InstanceNumber";
    out.args.back().desc = "A number affixed to the first image, and then incremented and affixed for each"
                           " subsequent image.";
    out.args.back().default_val = "1";
    out.args.back().expected = true;
    out.args.back().examples = { "1", "100", "1234" };

    out.args.emplace_back();
    out.args.back().name = "AcquisitionNumber";
    out.args.back().desc = "A number affixed to all images, meant to indicate membership in a single acquisition.";
    out.args.back().default_val = "1";
    out.args.back().expected = true;
    out.args.back().examples = { "1", "100", "1234" };

    out.args.emplace_back();
    out.args.back().name = "VoxelValue";
    out.args.back().desc = "The value that is assigned to all voxels, or possibly every other voxel."
                           " Note that if StipleValue is given a finite value, only half the voxels will be"
                           " assigned a value of VoxelValue and the other half will be assigned a value of"
                           " StipleValue. This produces a checkerboard pattern.";
    out.args.back().default_val = "0.0";
    out.args.back().expected = true;
    out.args.back().examples = { "0.0", "1.0E4", "-1234", "nan" };

    out.args.emplace_back();
    out.args.back().name = "StipleValue";
    out.args.back().desc = "The value that is assigned to every other voxel."
                           " If StipleValue is given a finite value, half of all voxels will be"
                           " assigned a value of VoxelValue and the other half will be assigned a value of"
                           " StipleValue. This produces a checkerboard pattern.";
    out.args.back().default_val = "nan";
    out.args.back().expected = true;
    out.args.back().examples = { "1.0", "-1.0E4", "1234" };

    out.args.emplace_back();
    out.args.back().name = "Metadata";
    out.args.back().desc = "A semicolon-separated list of 'key@value' metadata to imbue into each image."
                           " This metadata will overwrite any existing keys with the provided values.";
    out.args.back().default_val = "";
    out.args.back().expected = false;
    out.args.back().examples = { "keyA@valueA;keyB@valueB" };

    return out;
}

Drover GenerateSyntheticImages(Drover DICOM_data, const OperationArgPkg& OptArgs, const std::map<std::string,std::string>&, const std::string& FilenameLex){

    Explicator X(FilenameLex);

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto NumberOfImages = std::stol( OptArgs.getValueStr("NumberOfImages").value() );
    const auto NumberOfRows = std::stol( OptArgs.getValueStr("NumberOfRows").value() );
    const auto NumberOfColumns = std::stol( OptArgs.getValueStr("NumberOfColumns").value() );
    const auto NumberOfChannels = std::stol( OptArgs.getValueStr("NumberOfChannels").value() );

    const auto SliceThickness = std::stod( OptArgs.getValueStr("SliceThickness").value() );
    const auto SpacingBetweenSlices = std::stod( OptArgs.getValueStr("SpacingBetweenSlices").value() );

    const auto VoxelWidth = std::stod( OptArgs.getValueStr("VoxelWidth").value() );
    const auto VoxelHeight = std::stod( OptArgs.getValueStr("VoxelHeight").value() );

    const auto ImageAnchorStr = OptArgs.getValueStr("ImageAnchor").value();
    const auto ImagePositionStr = OptArgs.getValueStr("ImagePosition").value();

    const auto ImageOrientationColumnStr = OptArgs.getValueStr("ImageOrientationColumn").value();
    const auto ImageOrientationRowStr = OptArgs.getValueStr("ImageOrientationRow").value();

    auto InstanceNumber = std::stol( OptArgs.getValueStr("InstanceNumber").value() );
    const auto AcquisitionNumber = std::stol( OptArgs.getValueStr("AcquisitionNumber").value() );

    const auto VoxelValue = std::stod( OptArgs.getValueStr("VoxelValue").value() );
    const auto StipleValue = std::stod( OptArgs.getValueStr("StipleValue").value() );

    const auto MetadataOpt = OptArgs.getValueStr("Metadata");

    //-----------------------------------------------------------------------------------------------------------------

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
    const auto ImageAnchor = parse_vec3(ImageAnchorStr);
    auto ImagePosition = parse_vec3(ImagePositionStr);

    auto ImageOrientationColumn = parse_vec3(ImageOrientationColumnStr).unit();
    auto ImageOrientationRow = parse_vec3(ImageOrientationRowStr).unit();
    auto ImageOrientationOrtho = ImageOrientationColumn.Cross( ImageOrientationRow ).unit();
    if(!ImageOrientationColumn.GramSchmidt_orthogonalize(ImageOrientationRow, ImageOrientationOrtho)){
        throw std::invalid_argument("ImageOrientation vectors could not be orthogonalized. Refusing to continue.");
    }
    ImageOrientationColumn = ImageOrientationColumn.unit();
    ImageOrientationRow = ImageOrientationRow.unit();
    ImageOrientationOrtho = ImageOrientationOrtho.unit();


    // Parse user-provided metadata.
    std::map<std::string, std::string> Metadata;
    if(MetadataOpt){
        // First, check if this is a multi-key@value statement.
        const auto regex_split = Compile_Regex("^.*;.*$");
        const auto ismulti = std::regex_match(MetadataOpt.value(), regex_split);

        std::vector<std::string> v_kvs;
        if(!ismulti){
            v_kvs.emplace_back(MetadataOpt.value());
        }else{
            v_kvs = SplitStringToVector(MetadataOpt.value(), ';', 'd');
            if(v_kvs.size() < 1){
                throw std::logic_error("Unable to separate multiple key@value tokens");
            }
        }

        // Now attempt to parse each key@value statement.
        for(auto & k_at_v : v_kvs){
            const auto regex_split = Compile_Regex("^.*@.*$");
            const auto iskv = std::regex_match(k_at_v, regex_split);
            if(!iskv){
                throw std::logic_error("Unable to parse key@value token: '"_s + k_at_v + "'. Refusing to continue.");
            }
            
            auto v_k_v = SplitStringToVector(k_at_v, '@', 'd');
            if(v_k_v.size() <= 1) throw std::logic_error("Unable to separate key@value specifier");
            if(v_k_v.size() != 2) break; // Not a key@value statement (hint: maybe multiple @'s present?).
            Metadata[v_k_v.front()] = v_k_v.back();
        }
    }

    using loaded_imgs_storage_t = decltype(DICOM_data.image_data);
    std::list<loaded_imgs_storage_t> loaded_imgs_storage;
    //std::shared_ptr<Contour_Data> loaded_contour_data_storage = std::make_shared<Contour_Data>();


    // Temporal metadata.
    const std::string ContentDate = "20190427";
    const std::string ContentTime = "111558";

    // Other metadata.
    //const double RescaleSlope = 1.0;
    //const double RescaleIntercept = 0.0;
    const std::string OriginFilename = "/dev/null";
    const std::string PatientID = "SyntheticImage";
    const std::string StudyInstanceUID = PatientID + "_Study1";
    const std::string SeriesInstanceUID = StudyInstanceUID + "_Series1";
    const std::string& FrameofReferenceUID = PatientID;
    const std::string Modality = "CT";

    // --- The virtual 'signal' image series ---
    loaded_imgs_storage.emplace_back();
    for(long int img_index = 0; img_index < NumberOfImages; ++img_index){
        const std::string SOPInstanceUID = Generate_Random_String_of_Length(6);

        std::unique_ptr<Image_Array> out(new Image_Array());
        out->imagecoll.images.emplace_back();

        out->imagecoll.images.back().metadata["Filename"] = OriginFilename;

        out->imagecoll.images.back().metadata["PatientID"] = PatientID;
        out->imagecoll.images.back().metadata["StudyInstanceUID"] = StudyInstanceUID;
        out->imagecoll.images.back().metadata["SeriesInstanceUID"] = SeriesInstanceUID;
        out->imagecoll.images.back().metadata["SOPInstanceUID"] = SOPInstanceUID;

        out->imagecoll.images.back().metadata["Rows"] = std::to_string(NumberOfRows);
        out->imagecoll.images.back().metadata["Columns"] = std::to_string(NumberOfColumns);
        out->imagecoll.images.back().metadata["SliceThickness"] = std::to_string(SliceThickness);
        out->imagecoll.images.back().metadata["SpacingBetweenSlices"] = std::to_string(SpacingBetweenSlices);
        out->imagecoll.images.back().metadata["ImagePositionPatient"] = std::to_string(ImagePosition.x) + "\\"
                                                                      + std::to_string(ImagePosition.y) + "\\"
                                                                      + std::to_string(ImagePosition.z);
        out->imagecoll.images.back().metadata["ImageOrientationPatient"] = std::to_string(ImageOrientationRow.x) + "\\"
                                                                         + std::to_string(ImageOrientationRow.y) + "\\"
                                                                         + std::to_string(ImageOrientationRow.z) + "\\"
                                                                         + std::to_string(ImageOrientationColumn.x) + "\\"
                                                                         + std::to_string(ImageOrientationColumn.y) + "\\"
                                                                         + std::to_string(ImageOrientationColumn.z);
        out->imagecoll.images.back().metadata["PixelSpacing"] = std::to_string(VoxelWidth) + "\\" + std::to_string(VoxelHeight);
        out->imagecoll.images.back().metadata["FrameofReferenceUID"] = FrameofReferenceUID;

        out->imagecoll.images.back().metadata["StudyTime"] = ContentTime;
        out->imagecoll.images.back().metadata["SeriesTime"] = ContentTime;
        out->imagecoll.images.back().metadata["AcquisitionTime"] = ContentTime;
        out->imagecoll.images.back().metadata["ContentTime"] = ContentTime;

        out->imagecoll.images.back().metadata["StudyDate"] = ContentDate;
        out->imagecoll.images.back().metadata["SeriesDate"] = ContentDate;
        out->imagecoll.images.back().metadata["AcquisitionDate"] = ContentDate;
        out->imagecoll.images.back().metadata["ContentDate"] = ContentDate;

        out->imagecoll.images.back().metadata["InstanceNumber"] = std::to_string(InstanceNumber);
        out->imagecoll.images.back().metadata["AcquisitionNumber"] = std::to_string(AcquisitionNumber);

        out->imagecoll.images.back().metadata["Modality"] = Modality;

        // Finally, insert user-specified metadata.
        //
        // Note: This must occur last so it overwrites incumbent metadata entries.
        for(const auto &kvp : Metadata){
            out->imagecoll.images.back().metadata[kvp.first] = kvp.second;
        }

        // ---

        out->imagecoll.images.back().init_orientation(ImageOrientationRow, ImageOrientationColumn);
        out->imagecoll.images.back().init_buffer(NumberOfRows, NumberOfColumns, NumberOfChannels);
        out->imagecoll.images.back().init_spatial(VoxelWidth, VoxelHeight, SliceThickness, ImageAnchor, ImagePosition);

        out->imagecoll.images.back().fill_pixels(VoxelValue);

        if(std::isfinite(StipleValue)){
            for(long int row = 0; row < NumberOfRows; ++row){
                for(long int col = 0; col < NumberOfColumns; ++col){
                    //const auto R = out->imagecoll.images.back().position(row,col);
                    for(long int chnl = 0; chnl < NumberOfChannels; ++chnl){
                        const auto stipled = ( (img_index + row + col + chnl) % 2) == 0;
                        const auto val = (stipled) ? StipleValue : VoxelValue;
                        out->imagecoll.images.back().reference(row,col,chnl) = val;
                    } //Loop over channels.
                } //Loop over columns.
            } //Loop over rows.
        }


        ImagePosition += ImageOrientationOrtho * SpacingBetweenSlices;
        ++InstanceNumber;

        loaded_imgs_storage.back().push_back( std::move( out ) );
    }


    //Collate each group of images into a single set, if possible. Also stuff the correct contour data in the same set.
    for(auto &loaded_img_set : loaded_imgs_storage){
        if(loaded_img_set.empty()) continue;

        auto collated_imgs = Collate_Image_Arrays(loaded_img_set);
        if(!collated_imgs){
            throw std::runtime_error("Unable to collate images. Virtual data should never cause this error.");
        }

        DICOM_data.image_data.emplace_back(std::move(collated_imgs));
    }

    //Create an empty contour set iff one does not exist.
    if(DICOM_data.contour_data == nullptr){
        std::unique_ptr<Contour_Data> output (new Contour_Data());
        DICOM_data.contour_data = std::move(output);
    }

    return DICOM_data;
}
