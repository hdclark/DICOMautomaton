// DCMA_DICOM_Loader.cc - A part of DICOMautomaton 2026. Written by hal clark.
//
// This file contains routines for loading CT, MR, and RTDOSE images from a parsed
// DICOM Node tree. It delegates pixel data extraction to DCMA_DICOM_PixelData and
// enriches the resulting planar_image objects with spatial parameters and metadata
// read directly from the Node tree.
//
// References:
//   - DICOM PS3.3 2026b, Section A.3.3:  CT Image IOD Module Table.
//   - DICOM PS3.3 2026b, Section A.4.3:  MR Image IOD Module Table.
//   - DICOM PS3.3 2026b, Section A.18.3: RT Dose IOD Module Table.

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <optional>

#include "YgorLog.h"
#include "YgorImages.h"
#include "YgorMath.h"
#include "YgorString.h"

#include "DCMA_DICOM.h"
#include "DCMA_DICOM_PixelData.h"
#include "DCMA_DICOM_Loader.h"
#include "Structs.h"
#include "Metadata.h"


namespace DCMA_DICOM {


// ============================================================================
// Static helpers for reading values from a DICOM Node tree.
// ============================================================================
//
// These mirror the helpers in DCMA_DICOM_PixelData.cc but are kept local to this
// translation unit to avoid coupling to internal PixelData symbols.

// Read a text-valued tag, stripping trailing padding (nulls and spaces).
static std::string read_text(const Node &root, uint16_t group, uint16_t tag){
    const auto *n = root.find(group, tag);
    if(n == nullptr) return "";

    std::string out = n->val;
    while(!out.empty() && (out.back() == '\0' || out.back() == ' ')) out.pop_back();
    return out;
}

// Read a DS-valued tag and return the first value as double.
static std::optional<double> read_DS(const Node &root, uint16_t group, uint16_t tag){
    auto s = read_text(root, group, tag);
    if(s.empty()) return std::nullopt;

    while(!s.empty() && s.front() == ' ') s.erase(s.begin());
    while(!s.empty() && s.back() == ' ') s.pop_back();
    if(s.empty()) return std::nullopt;

    try{
        return std::stod(s);
    }catch(const std::exception &){
        return std::nullopt;
    }
}

// Read a multi-valued DS tag and return all values as a vector of doubles.
// DS values use backslash as the value multiplicity separator.
static std::vector<double> read_DS_multi(const Node &root, uint16_t group, uint16_t tag){
    const auto s = read_text(root, group, tag);
    if(s.empty()) return {};

    const auto tokens = SplitStringToVector(s, '\\', 'd');
    std::vector<double> out;
    out.reserve(tokens.size());
    for(const auto &t : tokens){
        try{
            out.push_back(std::stod(t));
        }catch(const std::exception &){
            break;
        }
    }
    return out;
}


// ============================================================================
// Metadata extraction from the DICOM Node tree.
// ============================================================================
//
// These routines populate a metadata_map_t from the parsed Node tree, providing
// the same key names as Imebra_Shim.cc's get_metadata_top_level_tags() for
// downstream compatibility.

// Insert a tag value into the metadata map if the tag is present and non-empty.
static void insert_tag(metadata_map_t &meta, const Node &root,
                       uint16_t group, uint16_t tag, const std::string &key){
    if(meta.count(key) != 0) return;
    const auto val = read_text(root, group, tag);
    if(!val.empty()) meta.emplace(key, val);
}

// Extract metadata common to all image modalities.
//
// This covers the SOP Common, Patient, General Study, General Series, Patient Study,
// Frame of Reference, General Equipment, VOI LUT, General Image, Image Plane,
// Image Pixel, Multi-Frame, and Modality LUT DICOM modules.
static metadata_map_t extract_common_metadata(const Node &root){
    metadata_map_t meta;

    // SOP Common Module (PS3.3 C.12.1).
    insert_tag(meta, root, 0x0008, 0x0016, "SOPClassUID");
    insert_tag(meta, root, 0x0008, 0x0018, "SOPInstanceUID");
    insert_tag(meta, root, 0x0008, 0x0005, "SpecificCharacterSet");
    insert_tag(meta, root, 0x0008, 0x0012, "InstanceCreationDate");
    insert_tag(meta, root, 0x0008, 0x0013, "InstanceCreationTime");
    insert_tag(meta, root, 0x0008, 0x0014, "InstanceCreatorUID");
    insert_tag(meta, root, 0x0020, 0x0013, "InstanceNumber");

    // Patient Module (PS3.3 C.7.1.1).
    insert_tag(meta, root, 0x0010, 0x0010, "PatientsName");
    insert_tag(meta, root, 0x0010, 0x0020, "PatientID");
    insert_tag(meta, root, 0x0010, 0x0030, "PatientsBirthDate");
    insert_tag(meta, root, 0x0010, 0x0040, "PatientsGender");

    // General Study Module (PS3.3 C.7.2.1).
    insert_tag(meta, root, 0x0020, 0x000D, "StudyInstanceUID");
    insert_tag(meta, root, 0x0008, 0x0020, "StudyDate");
    insert_tag(meta, root, 0x0008, 0x0030, "StudyTime");
    insert_tag(meta, root, 0x0008, 0x0090, "ReferringPhysiciansName");
    insert_tag(meta, root, 0x0020, 0x0010, "StudyID");
    insert_tag(meta, root, 0x0008, 0x0050, "AccessionNumber");
    insert_tag(meta, root, 0x0008, 0x1030, "StudyDescription");

    // General Series Module (PS3.3 C.7.3.1).
    insert_tag(meta, root, 0x0008, 0x0060, "Modality");
    insert_tag(meta, root, 0x0020, 0x000E, "SeriesInstanceUID");
    insert_tag(meta, root, 0x0020, 0x0011, "SeriesNumber");
    insert_tag(meta, root, 0x0008, 0x0021, "SeriesDate");
    insert_tag(meta, root, 0x0008, 0x0031, "SeriesTime");
    insert_tag(meta, root, 0x0008, 0x103E, "SeriesDescription");
    insert_tag(meta, root, 0x0018, 0x0015, "BodyPartExamined");
    insert_tag(meta, root, 0x0018, 0x5100, "PatientPosition");
    insert_tag(meta, root, 0x0040, 0x1001, "RequestedProcedureID");
    insert_tag(meta, root, 0x0040, 0x0009, "ScheduledProcedureStepID");
    insert_tag(meta, root, 0x0008, 0x1070, "OperatorsName");

    // Patient Study Module (PS3.3 C.7.2.2).
    insert_tag(meta, root, 0x0010, 0x1010, "PatientsAge");
    insert_tag(meta, root, 0x0010, 0x1020, "PatientsSize");
    insert_tag(meta, root, 0x0010, 0x1030, "PatientsWeight");

    // Frame of Reference Module (PS3.3 C.7.4.1).
    insert_tag(meta, root, 0x0020, 0x0052, "FrameOfReferenceUID");
    insert_tag(meta, root, 0x0020, 0x1040, "PositionReferenceIndicator");

    // General Equipment Module (PS3.3 C.7.5.1).
    insert_tag(meta, root, 0x0008, 0x0070, "Manufacturer");
    insert_tag(meta, root, 0x0008, 0x0080, "InstitutionName");
    insert_tag(meta, root, 0x0018, 0x1000, "DeviceSerialNumber");
    insert_tag(meta, root, 0x0008, 0x1010, "StationName");
    insert_tag(meta, root, 0x0008, 0x1040, "InstitutionalDepartmentName");
    insert_tag(meta, root, 0x0008, 0x1090, "ManufacturersModelName");
    insert_tag(meta, root, 0x0018, 0x1020, "SoftwareVersions");

    // VOI LUT Module (PS3.3 C.11.2).
    insert_tag(meta, root, 0x0028, 0x1050, "WindowCenter");
    insert_tag(meta, root, 0x0028, 0x1051, "WindowWidth");

    // General Image Module (PS3.3 C.7.6.1).
    insert_tag(meta, root, 0x0020, 0x0020, "PatientOrientation");
    insert_tag(meta, root, 0x0008, 0x0023, "ContentDate");
    insert_tag(meta, root, 0x0008, 0x0033, "ContentTime");
    insert_tag(meta, root, 0x0008, 0x0008, "ImageType");
    insert_tag(meta, root, 0x0020, 0x0012, "AcquisitionNumber");
    insert_tag(meta, root, 0x0008, 0x0022, "AcquisitionDate");
    insert_tag(meta, root, 0x0008, 0x0032, "AcquisitionTime");
    insert_tag(meta, root, 0x0008, 0x2111, "DerivationDescription");
    insert_tag(meta, root, 0x0020, 0x1002, "ImagesInAcquisition");
    insert_tag(meta, root, 0x0020, 0x4000, "ImageComments");
    insert_tag(meta, root, 0x0028, 0x0300, "QualityControlImage");

    // Image Plane Module (PS3.3 C.7.6.2).
    insert_tag(meta, root, 0x0028, 0x0030, "PixelSpacing");
    insert_tag(meta, root, 0x0020, 0x0037, "ImageOrientationPatient");
    insert_tag(meta, root, 0x0020, 0x0032, "ImagePositionPatient");
    insert_tag(meta, root, 0x0018, 0x0050, "SliceThickness");
    insert_tag(meta, root, 0x0020, 0x1041, "SliceLocation");

    // Image Pixel Module (PS3.3 C.7.6.3).
    insert_tag(meta, root, 0x0028, 0x0002, "SamplesPerPixel");
    insert_tag(meta, root, 0x0028, 0x0004, "PhotometricInterpretation");
    insert_tag(meta, root, 0x0028, 0x0010, "Rows");
    insert_tag(meta, root, 0x0028, 0x0011, "Columns");
    insert_tag(meta, root, 0x0028, 0x0100, "BitsAllocated");
    insert_tag(meta, root, 0x0028, 0x0101, "BitsStored");
    insert_tag(meta, root, 0x0028, 0x0102, "HighBit");
    insert_tag(meta, root, 0x0028, 0x0103, "PixelRepresentation");
    insert_tag(meta, root, 0x0028, 0x0006, "PlanarConfiguration");
    insert_tag(meta, root, 0x0028, 0x0034, "PixelAspectRatio");
    insert_tag(meta, root, 0x0028, 0x0106, "SmallestImagePixelValue");
    insert_tag(meta, root, 0x0028, 0x0107, "LargestImagePixelValue");

    // Multi-Frame Module (PS3.3 C.7.6.6).
    insert_tag(meta, root, 0x0028, 0x0008, "NumberOfFrames");
    insert_tag(meta, root, 0x0028, 0x0009, "FrameIncrementPointer");

    // Modality LUT Module (PS3.3 C.11.1).
    insert_tag(meta, root, 0x0028, 0x3002, "LUTDescriptor");
    insert_tag(meta, root, 0x0028, 0x3004, "ModalityLUTType");
    insert_tag(meta, root, 0x0028, 0x3006, "LUTData");
    insert_tag(meta, root, 0x0028, 0x1052, "RescaleIntercept");
    insert_tag(meta, root, 0x0028, 0x1053, "RescaleSlope");
    insert_tag(meta, root, 0x0028, 0x1054, "RescaleType");

    // Miscellaneous tags used by downstream processing.
    insert_tag(meta, root, 0x3004, 0x000C, "GridFrameOffsetVector");
    insert_tag(meta, root, 0x0020, 0x0100, "TemporalPositionIdentifier");
    insert_tag(meta, root, 0x0020, 0x9128, "TemporalPositionIndex");
    insert_tag(meta, root, 0x0020, 0x0105, "NumberOfTemporalPositions");
    insert_tag(meta, root, 0x0020, 0x0110, "TemporalResolution");
    insert_tag(meta, root, 0x0054, 0x1300, "FrameReferenceTime");
    insert_tag(meta, root, 0x0018, 0x1063, "FrameTime");
    insert_tag(meta, root, 0x0018, 0x1060, "TriggerTime");
    insert_tag(meta, root, 0x0018, 0x1069, "TriggerTimeOffset");
    insert_tag(meta, root, 0x0040, 0x0244, "PerformedProcedureStepStartDate");
    insert_tag(meta, root, 0x0040, 0x0245, "PerformedProcedureStepStartTime");
    insert_tag(meta, root, 0x0040, 0x0250, "PerformedProcedureStepEndDate");
    insert_tag(meta, root, 0x0040, 0x0251, "PerformedProcedureStepEndTime");
    insert_tag(meta, root, 0x0018, 0x1152, "Exposure");
    insert_tag(meta, root, 0x0018, 0x1150, "ExposureTime");
    insert_tag(meta, root, 0x0018, 0x1153, "ExposureInMicroAmpereSeconds");
    insert_tag(meta, root, 0x0018, 0x1151, "XRayTubeCurrent");
    insert_tag(meta, root, 0x0018, 0x1030, "ProtocolName");
    insert_tag(meta, root, 0x0008, 0x0090, "ReferringPhysicianName");

    return meta;
}

// Extract CT Image Module metadata (PS3.3 C.8.2.1).
static void extract_ct_metadata(metadata_map_t &meta, const Node &root){
    insert_tag(meta, root, 0x0018, 0x0060, "KVP");
    insert_tag(meta, root, 0x0018, 0x0090, "DataCollectionDiameter");
    insert_tag(meta, root, 0x0018, 0x1100, "ReconstructionDiameter");
}

// Extract MR Image Module metadata (PS3.3 C.8.3.1).
static void extract_mr_metadata(metadata_map_t &meta, const Node &root){
    insert_tag(meta, root, 0x0018, 0x0020, "ScanningSequence");
    insert_tag(meta, root, 0x0018, 0x0021, "SequenceVariant");
    insert_tag(meta, root, 0x0018, 0x0024, "SequenceName");
    insert_tag(meta, root, 0x0018, 0x0022, "ScanOptions");
    insert_tag(meta, root, 0x0018, 0x0023, "MRAcquisitionType");
    insert_tag(meta, root, 0x0018, 0x0080, "RepetitionTime");
    insert_tag(meta, root, 0x0018, 0x0081, "EchoTime");
    insert_tag(meta, root, 0x0018, 0x0082, "InversionTime");
    insert_tag(meta, root, 0x0018, 0x0091, "EchoTrainLength");
    insert_tag(meta, root, 0x0018, 0x1060, "TriggerTime");
    insert_tag(meta, root, 0x0018, 0x0025, "AngioFlag");
    insert_tag(meta, root, 0x0018, 0x1062, "NominalInterval");
    insert_tag(meta, root, 0x0018, 0x1090, "CardiacNumberOfImages");
    insert_tag(meta, root, 0x0018, 0x0083, "NumberOfAverages");
    insert_tag(meta, root, 0x0018, 0x0084, "ImagingFrequency");
    insert_tag(meta, root, 0x0018, 0x0085, "ImagedNucleus");
    insert_tag(meta, root, 0x0018, 0x0086, "EchoNumbers");
    insert_tag(meta, root, 0x0018, 0x0087, "MagneticFieldStrength");
    insert_tag(meta, root, 0x0018, 0x0088, "SpacingBetweenSlices");
    insert_tag(meta, root, 0x0018, 0x0089, "NumberOfPhaseEncodingSteps");
    insert_tag(meta, root, 0x0018, 0x0093, "PercentSampling");
    insert_tag(meta, root, 0x0018, 0x0094, "PercentPhaseFieldOfView");
    insert_tag(meta, root, 0x0018, 0x0095, "PixelBandwidth");
    insert_tag(meta, root, 0x0018, 0x1250, "ReceiveCoilName");
    insert_tag(meta, root, 0x0018, 0x1251, "TransmitCoilName");
    insert_tag(meta, root, 0x0018, 0x1310, "AcquisitionMatrix");
    insert_tag(meta, root, 0x0018, 0x1312, "InplanePhaseEncodingDirection");
    insert_tag(meta, root, 0x0018, 0x1314, "FlipAngle");
    insert_tag(meta, root, 0x0018, 0x1315, "VariableFlipAngleFlag");
    insert_tag(meta, root, 0x0018, 0x1316, "SAR");
    insert_tag(meta, root, 0x0018, 0x1318, "dBdt");
    insert_tag(meta, root, 0x0018, 0x9073, "AcquisitionDuration");
}

// Extract RT Dose Module metadata (PS3.3 C.8.8.3).
static void extract_dose_metadata(metadata_map_t &meta, const Node &root){
    insert_tag(meta, root, 0x3004, 0x0002, "DoseUnits");
    insert_tag(meta, root, 0x3004, 0x0004, "DoseType");
    insert_tag(meta, root, 0x3004, 0x000a, "DoseSummationType");
    insert_tag(meta, root, 0x3004, 0x000e, "DoseGridScaling");
}


// ============================================================================
// Pixel data extraction wrapper.
// ============================================================================

// Extract pixel data from the Node tree, selecting between native and encapsulated
// extraction based on the transfer syntax.
static std::optional<planar_image_collection<float,double>>
extract_pixel_data(const Node &root){
    auto desc = get_pixel_data_desc(root);
    if(!desc) return std::nullopt;

    if(is_native_transfer_syntax(desc->transfer_syntax)){
        return extract_native_pixel_data(root);
    }else if(is_encapsulated_transfer_syntax(desc->transfer_syntax)){
        return extract_encapsulated_pixel_data(root);
    }

    YLOGWARN("Unknown or unsupported transfer syntax");
    return std::nullopt;
}


// ============================================================================
// Spatial parameter extraction.
// ============================================================================

struct SpatialParams {
    vec3<double> position = vec3<double>(0.0, 0.0, 0.0);
    vec3<double> row_unit = vec3<double>(1.0, 0.0, 0.0);
    vec3<double> col_unit = vec3<double>(0.0, 1.0, 0.0);
    double pxl_dx = 1.0;  // Pixel spacing along columns.
    double pxl_dy = 1.0;  // Pixel spacing along rows.
    double pxl_dz = 1.0;  // Slice thickness.
};

// Read the Image Plane module tags from the Node tree.
static SpatialParams extract_spatial_params(const Node &root){
    SpatialParams sp;

    // Image Position Patient (0020,0032).
    const auto pos = read_DS_multi(root, 0x0020, 0x0032);
    if(pos.size() == 3){
        sp.position = vec3<double>(pos[0], pos[1], pos[2]);
    }else{
        YLOGINFO("Unable to find ImagePositionPatient, using defaults");
    }

    // Image Orientation Patient (0020,0037): row_x\row_y\row_z\col_x\col_y\col_z.
    const auto orien = read_DS_multi(root, 0x0020, 0x0037);
    if(orien.size() == 6){
        sp.row_unit = vec3<double>(orien[0], orien[1], orien[2]).unit();
        sp.col_unit = vec3<double>(orien[3], orien[4], orien[5]).unit();
    }else{
        YLOGINFO("Unable to find ImageOrientationPatient, using defaults");
    }

    // Pixel Spacing (0028,0030): row_spacing\column_spacing.
    const auto pxl_spacing = read_DS_multi(root, 0x0028, 0x0030);
    if(pxl_spacing.size() == 2){
        sp.pxl_dy = pxl_spacing[0];
        sp.pxl_dx = pxl_spacing[1];
    }else{
        YLOGINFO("Unable to find PixelSpacing, using defaults");
    }

    // Slice Thickness (0018,0050).
    const auto thickness = read_DS(root, 0x0018, 0x0050);
    if(thickness){
        sp.pxl_dz = *thickness;
    }else{
        YLOGINFO("Unable to find SliceThickness, using default");
    }

    return sp;
}


// ============================================================================
// Common CT/MR image loading.
// ============================================================================
//
// CT and MR images share the same spatial parameter extraction and pixel data
// pipeline; only the modality-specific metadata and (potentially) the pixel
// transformation differ. This helper contains the shared logic.

static std::unique_ptr<Image_Array>
load_images_common(const Node &root, const metadata_map_t &meta){
    auto out = std::make_unique<Image_Array>();

    // Extract pixel data.
    auto pic_opt = extract_pixel_data(root);
    if(!pic_opt){
        YLOGWARN("Unable to extract pixel data");
        return nullptr;
    }
    out->imagecoll = std::move(*pic_opt);

    // Extract spatial parameters.
    const auto sp = extract_spatial_params(root);
    const auto anchor = vec3<double>(0.0, 0.0, 0.0);

    // Determine if Modality LUT (rescale slope/intercept) should be applied.
    auto lut = get_modality_lut_params(root);

    // Set spatial parameters and metadata on each image.
    for(auto &img : out->imagecoll.images){
        img.init_orientation(sp.row_unit, sp.col_unit);
        img.init_spatial(sp.pxl_dx, sp.pxl_dy, sp.pxl_dz, anchor, sp.position);

        img.metadata = meta;

        // Ensure spatial metadata is present and consistent with the parsed values.
        insert_if_new(img.metadata, "ImagePositionPatient",
                      std::to_string(sp.position.x) + "\\"
                    + std::to_string(sp.position.y) + "\\"
                    + std::to_string(sp.position.z));
        insert_if_new(img.metadata, "ImageOrientationPatient",
                      std::to_string(sp.row_unit.x) + "\\"
                    + std::to_string(sp.row_unit.y) + "\\"
                    + std::to_string(sp.row_unit.z) + "\\"
                    + std::to_string(sp.col_unit.x) + "\\"
                    + std::to_string(sp.col_unit.y) + "\\"
                    + std::to_string(sp.col_unit.z));
        insert_if_new(img.metadata, "PixelSpacing",
                      std::to_string(sp.pxl_dy) + "\\"
                    + std::to_string(sp.pxl_dx));
        insert_if_new(img.metadata, "SliceThickness",
                      std::to_string(sp.pxl_dz));

        // Apply the Modality LUT (Rescale Slope / Intercept) to transform stored
        // values to output units (e.g., Hounsfield Units for CT).
        if(lut){
            apply_modality_lut(img, *lut);
        }
    }

    return out;
}


// ============================================================================
// Public API.
// ============================================================================

std::unique_ptr<Image_Array> load_ct_images(const Node &root){
    auto meta = extract_common_metadata(root);
    extract_ct_metadata(meta, root);
    return load_images_common(root, meta);
}


std::unique_ptr<Image_Array> load_mr_images(const Node &root){
    auto meta = extract_common_metadata(root);
    extract_mr_metadata(meta, root);
    return load_images_common(root, meta);
}


std::unique_ptr<Image_Array> load_dose_images(const Node &root){
    auto meta = extract_common_metadata(root);
    extract_dose_metadata(meta, root);

    auto out = std::make_unique<Image_Array>();

    // Extract pixel data.
    auto pic_opt = extract_pixel_data(root);
    if(!pic_opt){
        YLOGWARN("Unable to extract pixel data");
        return nullptr;
    }
    out->imagecoll = std::move(*pic_opt);

    // Extract spatial parameters.
    const auto sp = extract_spatial_params(root);
    const auto anchor = vec3<double>(0.0, 0.0, 0.0);
    const auto stack_unit = sp.row_unit.Cross(sp.col_unit).unit();

    // Grid Frame Offset Vector (3004,000C): per-frame offsets along the stack direction.
    // See DICOM PS3.3, C.7.6.6 and C.8.8.3.
    const auto gfov = read_DS_multi(root, 0x3004, 0x000C);

    // Determine image thickness from the Grid Frame Offset Vector if available.
    // The offset vector provides z-positions relative to the first frame; the frame
    // separation is the difference between consecutive entries.
    double image_thickness = sp.pxl_dz;
    if(gfov.size() > 1){
        image_thickness = gfov[1] - gfov[0];
    }

    // Dose Grid Scaling (3004,000E).
    const auto grid_scale = read_DS(root, 0x3004, 0x000E).value_or(1.0);

    // Determine if Modality LUT (rescale slope/intercept) should be applied.
    auto lut = get_modality_lut_params(root);

    // Set spatial parameters and metadata on each image/frame.
    uint32_t frame = 0;
    for(auto &img : out->imagecoll.images){
        const double gfov_offset = (frame < gfov.size()) ? gfov[frame] : 0.0;
        const auto img_offset = sp.position + stack_unit * gfov_offset;

        img.init_orientation(sp.row_unit, sp.col_unit);
        img.init_spatial(sp.pxl_dx, sp.pxl_dy, image_thickness, anchor, img_offset);

        img.metadata = meta;

        // Per-frame metadata.
        img.metadata["GridFrameOffset"] = std::to_string(gfov_offset);
        img.metadata["Frame"] = std::to_string(frame);
        img.metadata["ImagePositionPatient"] =
            std::to_string(img_offset.x) + "\\"
          + std::to_string(img_offset.y) + "\\"
          + std::to_string(img_offset.z);

        insert_if_new(img.metadata, "ImageOrientationPatient",
                      std::to_string(sp.row_unit.x) + "\\"
                    + std::to_string(sp.row_unit.y) + "\\"
                    + std::to_string(sp.row_unit.z) + "\\"
                    + std::to_string(sp.col_unit.x) + "\\"
                    + std::to_string(sp.col_unit.y) + "\\"
                    + std::to_string(sp.col_unit.z));
        insert_if_new(img.metadata, "PixelSpacing",
                      std::to_string(sp.pxl_dy) + "\\"
                    + std::to_string(sp.pxl_dx));
        insert_if_new(img.metadata, "SliceThickness",
                      std::to_string(image_thickness));

        // Apply the Modality LUT if present.
        if(lut){
            apply_modality_lut(img, *lut);
        }

        // Apply dose grid scaling to convert stored pixel values to dose values.
        if(grid_scale != 1.0){
            for(int64_t row = 0; row < img.rows; ++row){
                for(int64_t col = 0; col < img.columns; ++col){
                    for(int64_t chn = 0; chn < img.channels; ++chn){
                        img.reference(row, col, chn) *= static_cast<float>(grid_scale);
                    }
                }
            }
        }

        ++frame;
    }

    return out;
}


} // namespace DCMA_DICOM
