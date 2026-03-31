// DCMA_DICOM_Loader.cc - DICOMautomaton 2026. Written by hal clark.
//
// This file implements routines for extracting semantic data from a parsed DICOM Node tree
// into the DICOMautomaton data model. It mirrors the Imebra-based loaders in Imebra_Shim.cc.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <exception>
#include <optional>
#include <functional>
#include <fstream>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "YgorContainers.h"
#include "YgorMath.h"
#include "YgorMisc.h"
#include "YgorLog.h"
#include "YgorString.h"
#include "YgorImages.h"

#include "DCMA_DICOM_Loader.h"
#include "DCMA_DICOM.h"
#include "DCMA_DICOM_PixelData.h"
#include "Structs.h"
#include "Metadata.h"
#include "Alignment_Rigid.h"
#include "Alignment_Field.h"

using namespace std::string_literals;

namespace DCMA_DICOM {

// ============================================================================
// Internal helpers.
// ============================================================================

// Strip trailing null bytes and spaces from a tag value.
static std::string read_text_from_val(const std::string &val){
    std::string out = val;
    while(!out.empty() && (out.back() == '\0' || out.back() == ' ')) out.pop_back();
    return out;
}

// Read a text-valued tag from the tree.
static std::string read_text(const Node &root, uint16_t group, uint16_t tag){
    const auto *n = root.find(group, tag);
    if(n == nullptr) return "";
    return read_text_from_val(n->val);
}

// Split a backslash-separated DICOM multi-value string into tokens.
static std::vector<std::string> split_vm(const std::string &s){
    return SplitStringToVector(s, '\\', 'd');
}

// Read a DS/FD-valued tag as a double.
static std::optional<double> read_first_double(const Node &root, uint16_t group, uint16_t tag){
    auto s = read_text(root, group, tag);
    if(s.empty()) return std::nullopt;
    // DS may have leading/trailing whitespace.
    while(!s.empty() && s.front() == ' ') s.erase(s.begin());
    while(!s.empty() && s.back() == ' ') s.pop_back();
    // Handle VM > 1 (take first).
    const auto tokens = split_vm(s);
    if(tokens.empty()) return std::nullopt;
    try{
        return std::stod(tokens.front());
    }catch(const std::exception &){
        return std::nullopt;
    }
}

// Read an IS/UL/US-valued tag as int64_t.
static std::optional<int64_t> read_first_long(const Node &root, uint16_t group, uint16_t tag){
    auto s = read_text(root, group, tag);
    if(s.empty()) return std::nullopt;
    while(!s.empty() && s.front() == ' ') s.erase(s.begin());
    while(!s.empty() && s.back() == ' ') s.pop_back();
    const auto tokens = split_vm(s);
    if(tokens.empty()) return std::nullopt;
    try{
        return static_cast<int64_t>(std::stol(tokens.front()));
    }catch(const std::exception &){
        return std::nullopt;
    }
}

// Read a DS/FD VM>1 tag as vector of doubles.
static std::vector<double> read_all_doubles(const Node &root, uint16_t group, uint16_t tag){
    std::vector<double> out;
    auto s = read_text(root, group, tag);
    if(s.empty()) return out;
    for(auto &tok : split_vm(s)){
        while(!tok.empty() && tok.front() == ' ') tok.erase(tok.begin());
        while(!tok.empty() && tok.back() == ' ') tok.pop_back();
        if(tok.empty()) continue;
        try{
            out.push_back(std::stod(tok));
        }catch(const std::exception &){}
    }
    return out;
}

// String-vector converters matching Imebra_Shim.cc patterns.
static std::optional<std::string>
convert_first_to_string(const std::vector<std::string> &in){
    if(!in.empty()) return in.front();
    return std::nullopt;
}

static std::optional<int64_t>
convert_first_to_long_int(const std::vector<std::string> &in){
    if(!in.empty()){
        try{ return static_cast<int64_t>(std::stol(in.front())); }catch(const std::exception &){}
    }
    return std::nullopt;
}

static std::optional<double>
convert_first_to_double(const std::vector<std::string> &in){
    if(!in.empty()){
        try{ return std::stod(in.front()); }catch(const std::exception &){}
    }
    return std::nullopt;
}

static std::optional<vec3<double>>
convert_to_vec3_double(const std::vector<std::string> &in){
    if(3 <= in.size()){
        try{
            return vec3<double>(std::stod(in.at(0)), std::stod(in.at(1)), std::stod(in.at(2)));
        }catch(const std::exception &){}
    }
    return std::nullopt;
}

static std::optional<vec3<int64_t>>
convert_to_vec3_int64(const std::vector<std::string> &in){
    if(3 <= in.size()){
        try{
            return vec3<int64_t>(std::stol(in.at(0)), std::stol(in.at(1)), std::stol(in.at(2)));
        }catch(const std::exception &){}
    }
    return std::nullopt;
}

static std::vector<double>
convert_to_vector_double(const std::vector<std::string> &in){
    std::vector<double> out;
    for(const auto &x : in){
        try{ out.push_back(std::stod(x)); }catch(const std::exception &){}
    }
    return out;
}

// Return the list of item-delimiter child nodes from a sequence tag.
// SQ nodes store their items as children: each child is an (FFFE,E000) item delimiter
// whose own children are the data tags for that item.
static std::list<const Node*> get_seq_items(const Node &root, uint16_t group, uint16_t tag){
    std::list<const Node*> out;
    const auto *sq = root.find(group, tag);
    if(sq == nullptr) return out;
    for(const auto &child : sq->children){
        // Items are (FFFE,E000).
        if(child.key.group == 0xFFFE && child.key.tag == 0xE000){
            out.push_back(&child);
        }
    }
    return out;
}

// Insert a trimmed, non-empty tag value into a metadata map.
static void insert_tag_value(metadata_map_t &out, const Node &src,
                             uint16_t group, uint16_t tag, const std::string &name){
    if(out.count(name) != 0) return;
    const auto *n = src.find(group, tag);
    if(n == nullptr) return;
    const auto ctrim = CANONICALIZE::TRIM_ENDS;
    // Handle multi-element tags.
    const auto raw = read_text_from_val(n->val);
    const auto tokens = split_vm(raw);
    bool first = true;
    std::string accum;
    for(const auto &tok : tokens){
        const auto trimmed = Canonicalize_String2(tok, ctrim);
        if(trimmed.empty()) break;
        if(!first) accum += R"***(\)***"s;
        accum += trimmed;
        first = false;
    }
    if(!accum.empty()) out.emplace(name, accum);
}

// Insert a tag value from a sequence item node into a metadata map.
static void insert_seq_item_tag_value(metadata_map_t &out, const Node &item_node,
                                      uint16_t tag_group, uint16_t tag_tag, const std::string &name){
    insert_tag_value(out, item_node, tag_group, tag_tag, name);
}

// Extract tag as string vector from a node (for compatibility with Imebra converters).
static std::vector<std::string> extract_tag_as_strings(const Node &src, uint16_t group, uint16_t tag){
    const auto *n = src.find(group, tag);
    if(n == nullptr) return {};
    auto raw = read_text_from_val(n->val);
    return split_vm(raw);
}

// Insert a single-level sequence tag into metadata.
static void insert_seq_tag(metadata_map_t &out, const Node &root,
                           uint16_t seq_group, uint16_t seq_tag, const std::string &seq_name,
                           uint16_t tag_group, uint16_t tag_tag, const std::string &tag_name){
    const auto full_name = seq_name + "/"s + tag_name;
    if(out.count(full_name) != 0) return;
    const auto items = get_seq_items(root, seq_group, seq_tag);
    if(items.empty()) return;
    const auto *item = items.front();
    insert_tag_value(out, *item, tag_group, tag_tag, full_name);
    // Copy result back.
    if(out.count(full_name) != 0) return; // Already inserted above.
}

// Extract an affine_transform from a 16-element vector (row-major 4x4).
static affine_transform<double> extract_affine_transform(const std::vector<double> &v){
    affine_transform<double> t;
    if(v.size() != 16){
        throw std::runtime_error("Unanticipated matrix transformation dimensions");
    }
    for(size_t row = 0; row < 3; ++row){
        for(size_t col = 0; col < 4; ++col){
            t.coeff(row,col) = v[col + row * 4];
        }
    }
    if((v[12] != 0.0) || (v[13] != 0.0) || (v[14] != 0.0) || (v[15] != 1.0)){
        throw std::runtime_error("Transformation cannot be represented using an Affine matrix");
    }
    return t;
}

// Parse a DICOM file from disk into a Node tree.
static Node parse_dicom_file(const std::filesystem::path &filename){
    Node root;
    std::ifstream fi(filename, std::ios::binary);
    if(!fi) throw std::runtime_error("Unable to open file '"s + filename.string() + "'");
    const auto dict = get_default_dictionary();
    root.read_DICOM(fi, {&dict});
    return root;
}

// Read a tag from a specific node, handling sequence paths.
// A "path" is a list of (group,tag,element) tuples leading to a nested tag.
struct path_node {
    uint16_t group = 0;
    uint16_t tag   = 0;
    uint32_t element = 0;
};

// Extract tag value following a sequence path from a node.
static std::vector<std::string>
extract_seq_path_tag(const Node &root, const std::vector<path_node> &path){
    if(path.empty()) return {};
    if(path.size() == 1){
        // Leaf: directly read the tag. The element field selects which VM component to start from.
        auto vals = extract_tag_as_strings(root, path[0].group, path[0].tag);
        if(path[0].element < vals.size()){
            return std::vector<std::string>(vals.begin() + path[0].element, vals.end());
        }
        return {};
    }
    // Navigate into a sequence.
    const auto items = get_seq_items(root, path[0].group, path[0].tag);
    if(items.empty()) return {};
    uint32_t idx = path[0].element;
    if(idx >= items.size()) return {};
    auto it = items.begin();
    std::advance(it, idx);
    std::vector<path_node> sub_path(path.begin() + 1, path.end());
    return extract_seq_path_tag(**it, sub_path);
}

// "Coalesce" wrapper: tries multiple paths, returns first non-empty result.
static std::vector<std::string>
coalesce_paths(const Node &root, const std::vector<std::vector<path_node>> &paths){
    for(const auto &p : paths){
        auto res = extract_seq_path_tag(root, p);
        if(!res.empty()) return res;
    }
    return {};
}

// Coalesce from metadata map keys.
static std::vector<std::string>
coalesce_metadata(const metadata_map_t &tlm, const std::list<std::string> &keys){
    for(const auto &key : keys){
        const auto res = tlm.find(key);
        if(res != std::end(tlm)){
            try{
                auto tokens = SplitStringToVector(res->second, '\\', 'd');
                tokens = SplitVector(tokens, ',', 'd');
                tokens = SplitVector(tokens, '\t', 'd');
                return tokens;
            }catch(const std::exception &){}
        }
    }
    return {};
}


// ============================================================================
// General tag accessors.
// ============================================================================

std::string get_tag_as_string_from_node(const Node &root, uint16_t group, uint16_t tag){
    return read_text(root, group, tag);
}

std::string get_modality_from_node(const Node &root){
    return read_text(root, 0x0008, 0x0060);
}

std::string get_patient_ID_from_node(const Node &root){
    return read_text(root, 0x0010, 0x0020);
}


// ============================================================================
// Metadata extraction.
// ============================================================================

metadata_map_t get_metadata_top_level_tags_from_node(const Node &root, const std::string &filename){
    metadata_map_t out;

    auto itag = [&out,&root](uint16_t g, uint16_t t, const std::string &name){
        insert_tag_value(out, root, g, t, name);
    };

    auto iseq = [&out,&root](uint16_t sg, uint16_t st, const std::string &sn,
                             uint16_t tg, uint16_t tt, const std::string &tn){
        insert_seq_tag(out, root, sg, st, sn, tg, tt, tn);
    };

    // Misc.
    if(!filename.empty()) out["Filename"] = filename;

    // SOP Common Module.
    itag(0x0008, 0x0016, "SOPClassUID");
    itag(0x0008, 0x0018, "SOPInstanceUID");
    itag(0x0008, 0x0005, "SpecificCharacterSet");
    itag(0x0008, 0x0012, "InstanceCreationDate");
    itag(0x0008, 0x0013, "InstanceCreationTime");
    itag(0x0008, 0x0014, "InstanceCreatorUID");
    itag(0x0008, 0x0114, "CodingSchemeExternalUID");
    itag(0x0020, 0x0013, "InstanceNumber");

    // Patient Module.
    itag(0x0010, 0x0010, "PatientsName");
    itag(0x0010, 0x0020, "PatientID");
    itag(0x0010, 0x0030, "PatientsBirthDate");
    itag(0x0010, 0x0040, "PatientsGender");

    // General Study Module.
    itag(0x0020, 0x000D, "StudyInstanceUID");
    itag(0x0008, 0x0020, "StudyDate");
    itag(0x0008, 0x0030, "StudyTime");
    itag(0x0008, 0x0090, "ReferringPhysiciansName");
    itag(0x0020, 0x0010, "StudyID");
    itag(0x0008, 0x0050, "AccessionNumber");
    itag(0x0008, 0x1030, "StudyDescription");

    // General Series Module.
    itag(0x0008, 0x0060, "Modality");
    itag(0x0020, 0x000E, "SeriesInstanceUID");
    itag(0x0020, 0x0011, "SeriesNumber");
    itag(0x0008, 0x0021, "SeriesDate");
    itag(0x0008, 0x0031, "SeriesTime");
    itag(0x0008, 0x103E, "SeriesDescription");
    itag(0x0018, 0x0015, "BodyPartExamined");
    itag(0x0018, 0x5100, "PatientPosition");
    itag(0x0040, 0x1001, "RequestedProcedureID");
    itag(0x0040, 0x0009, "ScheduledProcedureStepID");
    itag(0x0008, 0x1070, "OperatorsName");

    const auto modality = get_as<std::string>(out, "Modality").value_or("");

    // Patient Study Module.
    itag(0x0010, 0x1010, "PatientsAge");
    itag(0x0010, 0x1020, "PatientsSize");
    itag(0x0010, 0x1030, "PatientsWeight");

    // Frame of Reference Module.
    itag(0x0020, 0x0052, "FrameOfReferenceUID");
    itag(0x0020, 0x1040, "PositionReferenceIndicator");
    iseq(0x3006, 0x0010, "ReferencedFrameOfReferenceSequence",
         0x0020, 0x0052, "FrameOfReferenceUID");
    if(out.count("ReferencedFrameOfReferenceSequence/FrameOfReferenceUID") != 0){
        out["FrameOfReferenceUID"] = out["ReferencedFrameOfReferenceSequence/FrameOfReferenceUID"];
    }

    // General Equipment Module.
    itag(0x0008, 0x0070, "Manufacturer");
    itag(0x0008, 0x0080, "InstitutionName");
    itag(0x0018, 0x1000, "DeviceSerialNumber");
    itag(0x0008, 0x1010, "StationName");
    itag(0x0008, 0x1040, "InstitutionalDepartmentName");
    itag(0x0008, 0x1090, "ManufacturersModelName");
    itag(0x0018, 0x1020, "SoftwareVersions");

    // VOI LUT Module.
    itag(0x0028, 0x1050, "WindowCenter");
    itag(0x0028, 0x1051, "WindowWidth");

    // General Image Module.
    itag(0x0020, 0x0013, "InstanceNumber");
    itag(0x0020, 0x0020, "PatientOrientation");
    itag(0x0008, 0x0023, "ContentDate");
    itag(0x0008, 0x0033, "ContentTime");
    itag(0x0008, 0x0008, "ImageType");
    itag(0x0020, 0x0012, "AcquisitionNumber");
    itag(0x0008, 0x0022, "AcquisitionDate");
    itag(0x0008, 0x0032, "AcquisitionTime");
    itag(0x0008, 0x2111, "DerivationDescription");
    itag(0x0020, 0x1002, "ImagesInAcquisition");
    itag(0x0020, 0x4000, "ImageComments");
    itag(0x0028, 0x0300, "QualityControlImage");

    // Image Plane Module.
    itag(0x0028, 0x0030, "PixelSpacing");
    itag(0x0020, 0x0037, "ImageOrientationPatient");
    itag(0x0020, 0x0032, "ImagePositionPatient");
    itag(0x0018, 0x0050, "SliceThickness");
    itag(0x0020, 0x1041, "SliceLocation");

    // Image Pixel Module.
    itag(0x0028, 0x0002, "SamplesPerPixel");
    itag(0x0028, 0x0004, "PhotometricInterpretation");
    itag(0x0028, 0x0010, "Rows");
    itag(0x0028, 0x0011, "Columns");
    itag(0x0028, 0x0100, "BitsAllocated");
    itag(0x0028, 0x0101, "BitsStored");
    itag(0x0028, 0x0102, "HighBit");
    itag(0x0028, 0x0103, "PixelRepresentation");
    itag(0x0028, 0x0006, "PlanarConfiguration");
    itag(0x0028, 0x0034, "PixelAspectRatio");
    itag(0x0028, 0x0106, "SmallestImagePixelValue");
    itag(0x0028, 0x0107, "LargestImagePixelValue");

    // Multi-Frame Module.
    itag(0x0028, 0x0008, "NumberOfFrames");
    itag(0x0028, 0x0009, "FrameIncrementPointer");

    // Modality LUT Module.
    itag(0x0028, 0x3002, "LUTDescriptor");
    itag(0x0028, 0x3004, "ModalityLUTType");
    itag(0x0028, 0x3006, "LUTData");
    itag(0x0028, 0x1052, "RescaleIntercept");
    itag(0x0028, 0x1053, "RescaleSlope");
    itag(0x0028, 0x1054, "RescaleType");

    if(modality == "RTDOSE"){
        itag(0x3004, 0x0002, "DoseUnits");
        itag(0x3004, 0x0004, "DoseType");
        itag(0x3004, 0x000a, "DoseSummationType");
        itag(0x3004, 0x000e, "DoseGridScaling");
        iseq(0x300C, 0x0002, "ReferencedRTPlanSequence", 0x0008, 0x1150, "ReferencedSOPClassUID");
        iseq(0x300C, 0x0002, "ReferencedRTPlanSequence", 0x0008, 0x1155, "ReferencedSOPInstanceUID");
        iseq(0x300C, 0x0020, "ReferencedFractionGroupSequence", 0x300C, 0x0022, "ReferencedFractionGroupNumber");
        iseq(0x300C, 0x0004, "ReferencedBeamSequence", 0x300C, 0x0006, "ReferencedBeamNumber");
    }

    if(modality == "CT"){
        itag(0x0018, 0x0060, "KVP");
        itag(0x0018, 0x0090, "DataCollectionDiameter");
        itag(0x0018, 0x1100, "ReconstructionDiameter");
    }

    if(modality == "RTIMAGE"){
        itag(0x3002, 0x0002, "RTImageLabel");
        itag(0x3002, 0x0004, "RTImageDescription");
        itag(0x3002, 0x000a, "ReportedValuesOrigin");
        itag(0x3002, 0x000c, "RTImagePlane");
        itag(0x3002, 0x000d, "XRayImageReceptorTranslation");
        itag(0x3002, 0x000e, "XRayImageReceptorAngle");
        itag(0x3002, 0x0010, "RTImageOrientation");
        itag(0x3002, 0x0011, "ImagePlanePixelSpacing");
        itag(0x3002, 0x0012, "RTImagePosition");
        itag(0x3002, 0x0020, "RadiationMachineName");
        itag(0x3002, 0x0022, "RadiationMachineSAD");
        itag(0x3002, 0x0026, "RTImageSID");
        itag(0x3002, 0x0029, "FractionNumber");
        itag(0x300a, 0x00b3, "PrimaryDosimeterUnit");
        itag(0x300a, 0x011e, "GantryAngle");
        itag(0x300a, 0x0120, "BeamLimitingDeviceAngle");
        itag(0x300a, 0x0122, "PatientSupportAngle");
        itag(0x300a, 0x0128, "TableTopVerticalPosition");
        itag(0x300a, 0x0129, "TableTopLongitudinalPosition");
        itag(0x300a, 0x012a, "TableTopLateralPosition");
        itag(0x300a, 0x012c, "IsocenterPosition");
        itag(0x300c, 0x0006, "ReferencedBeamNumber");
        itag(0x300c, 0x0008, "StartCumulativeMetersetWeight");
        itag(0x300c, 0x0009, "EndCumulativeMetersetWeight");
        itag(0x300c, 0x0022, "ReferencedFractionGroupNumber");
        iseq(0x3002, 0x0030, "ExposureSequence", 0x0018, 0x0060, "KVP");
        iseq(0x3002, 0x0030, "ExposureSequence", 0x0018, 0x1150, "ExposureTime");
        iseq(0x3002, 0x0030, "ExposureSequence", 0x3002, 0x0032, "MetersetExposure");
    }

    if(modality == "RTPLAN"){
        itag(0x300a, 0x0002, "RTPlanLabel");
        itag(0x300a, 0x0003, "RTPlanName");
        itag(0x300a, 0x0004, "RTPlanDescription");
        itag(0x300a, 0x0006, "RTPlanDate");
        itag(0x300a, 0x0007, "RTPlanTime");
        itag(0x300a, 0x000c, "RTPlanGeometry");
        itag(0x300a, 0x000a, "PlanIntent");
        itag(0x300e, 0x0002, "ApprovalStatus");
        itag(0x300e, 0x0004, "ReviewDate");
        itag(0x300e, 0x0005, "ReviewTime");
        itag(0x300e, 0x0008, "ReviewerName");
    }

    if(modality == "MR"){
        itag(0x0018, 0x0020, "ScanningSequence");
        itag(0x0018, 0x0021, "SequenceVariant");
        itag(0x0018, 0x0024, "SequenceName");
        itag(0x0018, 0x0022, "ScanOptions");
        itag(0x0018, 0x0023, "MRAcquisitionType");
        itag(0x0018, 0x0080, "RepetitionTime");
        itag(0x0018, 0x0081, "EchoTime");
        itag(0x0018, 0x0082, "InversionTime");
        itag(0x0018, 0x0091, "EchoTrainLength");
        itag(0x0018, 0x1060, "TriggerTime");
        itag(0x0018, 0x0025, "AngioFlag");
        itag(0x0018, 0x1062, "NominalInterval");
        itag(0x0018, 0x1090, "CardiacNumberOfImages");
        itag(0x0018, 0x0083, "NumberOfAverages");
        itag(0x0018, 0x0084, "ImagingFrequency");
        itag(0x0018, 0x0085, "ImagedNucleus");
        itag(0x0018, 0x0086, "EchoNumbers");
        itag(0x0018, 0x0087, "MagneticFieldStrength");
        itag(0x0018, 0x0088, "SpacingBetweenSlices");
        itag(0x0018, 0x0089, "NumberOfPhaseEncodingSteps");
        itag(0x0018, 0x0093, "PercentSampling");
        itag(0x0018, 0x0094, "PercentPhaseFieldOfView");
        itag(0x0018, 0x0095, "PixelBandwidth");
        itag(0x0018, 0x1250, "ReceiveCoilName");
        itag(0x0018, 0x1251, "TransmitCoilName");
        itag(0x0018, 0x1310, "AcquisitionMatrix");
        itag(0x0018, 0x1312, "InplanePhaseEncodingDirection");
        itag(0x0018, 0x1314, "FlipAngle");
        itag(0x0018, 0x1315, "VariableFlipAngleFlag");
        itag(0x0018, 0x1316, "SAR");
        itag(0x0018, 0x1318, "dBdt");
        itag(0x0018, 0x9073, "AcquisitionDuration");
        itag(0x0019, 0x0010, "SiemensMRHeader");
        itag(0x0019, 0x100c, "DiffusionBValue");
        itag(0x0019, 0x100d, "DiffusionDirection");
        itag(0x0019, 0x100e, "DiffusionGradientVector");
        itag(0x0019, 0x1027, "DiffusionBMatrix");

        // NOTE: Siemens CSA2 binary header parsing (0x0029,0x1010 / 0x0029,0x1020) is not yet
        // implemented in the Node-based loader. The binary OB data would need a dedicated parser.
        // TODO: implement CSA2 binary header parsing.
        itag(0x0029, 0x0010, "SiemensCSAHeaderVersion");
        itag(0x0029, 0x0011, "SiemensMEDCOMHeaderVersion");
        itag(0x0029, 0x1008, "CSAImageHeaderType");
        itag(0x0029, 0x1009, "CSAImageHeaderVersion");
        itag(0x0029, 0x1018, "CSASeriesHeaderType");
        itag(0x0029, 0x1019, "CSASeriesHeaderVersion");
    }

    if(modality == "PT"){
        itag(0x0054, 0x1001, "Units");
    }

    // Unclassified others.
    itag(0x2001, 0x100a, "SliceNumber");
    itag(0x0054, 0x1330, "ImageIndex");
    itag(0x3004, 0x000C, "GridFrameOffsetVector");
    itag(0x0020, 0x0100, "TemporalPositionIdentifier");
    itag(0x0020, 0x9128, "TemporalPositionIndex");
    itag(0x0020, 0x0105, "NumberOfTemporalPositions");
    itag(0x0020, 0x0110, "TemporalResolution");
    itag(0x0054, 0x1300, "FrameReferenceTime");
    itag(0x0018, 0x1063, "FrameTime");
    itag(0x0018, 0x1060, "TriggerTime");
    itag(0x0018, 0x1069, "TriggerTimeOffset");
    itag(0x0040, 0x0244, "PerformedProcedureStepStartDate");
    itag(0x0040, 0x0245, "PerformedProcedureStepStartTime");
    itag(0x0040, 0x0250, "PerformedProcedureStepEndDate");
    itag(0x0040, 0x0251, "PerformedProcedureStepEndTime");
    itag(0x0018, 0x1152, "Exposure");
    itag(0x0018, 0x1150, "ExposureTime");
    itag(0x0018, 0x1153, "ExposureInMicroAmpereSeconds");
    itag(0x0018, 0x1151, "XRayTubeCurrent");
    itag(0x0018, 0x1030, "ProtocolName");
    itag(0x0008, 0x0090, "ReferringPhysicianName");

    return out;
}

// ============================================================================
// Contour data.
// ============================================================================

bimap<std::string,int64_t> get_ROI_tags_and_numbers_from_node(const Node &root){
    bimap<std::string,int64_t> the_pairs;
    // StructureSetROISequence (3006,0020).
    const auto items = get_seq_items(root, 0x3006, 0x0020);
    for(const auto *item : items){
        const auto name = read_text(*item, 0x3006, 0x0026); // ROIName
        const auto num_opt = read_first_long(*item, 0x3006, 0x0022); // ROINumber
        if(!name.empty() && num_opt){
            the_pairs[num_opt.value()] = name;
        }
    }
    return the_pairs;
}

std::unique_ptr<Contour_Data> get_Contour_Data_from_node(const Node &root, const std::string &filename){
    auto output = std::make_unique<Contour_Data>();
    auto tags_names_and_numbers = get_ROI_tags_and_numbers_from_node(root);
    auto FileMetadata = get_metadata_top_level_tags_from_node(root, filename);

    std::map<std::tuple<std::string,int64_t>, contour_collection<double>> mapcache;

    // ROIContourSequence (3006,0039).
    const auto roi_contour_items = get_seq_items(root, 0x3006, 0x0039);
    for(const auto *roi_item : roi_contour_items){
        int64_t Last_ROI_Numb = 0;
        // ContourSequence (3006,0040).
        const auto contour_items = get_seq_items(*roi_item, 0x3006, 0x0040);

        auto ROI_number_opt = read_first_long(*roi_item, 0x3006, 0x0084); // ReferencedROINumber
        int64_t ROI_number = ROI_number_opt.value_or(0);
        if(ROI_number == 0){
            ROI_number = Last_ROI_Numb;
        }else{
            Last_ROI_Numb = ROI_number;
        }

        for(const auto *contour_node : contour_items){
            // Read ContourData (3006,0050).
            const auto contour_data_doubles = read_all_doubles(*contour_node, 0x3006, 0x0050);
            if(contour_data_doubles.empty()) continue;

            contour_of_points<double> shtl;
            shtl.closed = true;
            for(size_t N = 0; (N + 2) < contour_data_doubles.size(); N += 3){
                shtl.points.emplace_back(contour_data_doubles[N+0],
                                         contour_data_doubles[N+1],
                                         contour_data_doubles[N+2]);
            }
            shtl.metadata = FileMetadata;

            auto ROIName = tags_names_and_numbers[ROI_number];
            shtl.metadata["ROINumber"] = std::to_string(ROI_number);
            shtl.metadata["ROIName"] = ROIName;

            const auto key = std::make_tuple(ROIName, ROI_number);
            mapcache[key].contours.push_back(std::move(shtl));
        }
    }

    // RTROIObservationsSequence (3006,0080).
    const auto insert_or_append = [](metadata_map_t &m, const std::string &key, const std::string &val){
        if(!val.empty()){
            auto &v = m[key];
            v += (v.empty()) ? val : ", "s + val;
        }
    };
    const auto obs_items = get_seq_items(root, 0x3006, 0x0080);
    for(const auto *obs_item : obs_items){
        auto ref_roi_opt = read_first_long(*obs_item, 0x3006, 0x0084);
        if(!ref_roi_opt) continue;
        const auto ReferencedROINumber = ref_roi_opt.value();
        const auto ROIName = tags_names_and_numbers[ReferencedROINumber];
        const auto key = std::make_tuple(ROIName, ReferencedROINumber);
        auto &cc = mapcache[key];

        const auto ObservationNumber         = read_text(*obs_item, 0x3006, 0x0082);
        const auto ROIObservationDescription = read_text(*obs_item, 0x3006, 0x0088);
        const auto RTROIInterpretedType      = read_text(*obs_item, 0x3006, 0x00A4);
        const auto ROIInterpreter            = read_text(*obs_item, 0x3006, 0x00A6);
        for(auto &c : cc.contours){
            insert_or_append(c.metadata, "RTROIObservationsSequence/ObservationNumber", ObservationNumber);
            insert_or_append(c.metadata, "RTROIObservationsSequence/ROIObservationDescription", ROIObservationDescription);
            insert_or_append(c.metadata, "RTROIObservationsSequence/RTROIInterpretedType", RTROIInterpretedType);
            insert_or_append(c.metadata, "RTROIObservationsSequence/ROIInterpreter", ROIInterpreter);
        }

        // RTROIIdentificationCodeSequence (3006,0086).
        const auto code_items = get_seq_items(*obs_item, 0x3006, 0x0086);
        for(const auto *code_item : code_items){
            const auto CodeValue              = read_text(*code_item, 0x0008, 0x0100);
            const auto CodingSchemeDesignator = read_text(*code_item, 0x0008, 0x0102);
            const auto CodingSchemeVersion    = read_text(*code_item, 0x0008, 0x0103);
            const auto CodeMeaning            = read_text(*code_item, 0x0008, 0x0104);
            const auto MappingResource        = read_text(*code_item, 0x0008, 0x0105);
            const auto ContextGroupVersion    = read_text(*code_item, 0x0008, 0x0106);
            const auto ContextIdentifier      = read_text(*code_item, 0x0008, 0x010F);
            const auto ContextUID             = read_text(*code_item, 0x0008, 0x0117);
            const auto MappingResourceUID     = read_text(*code_item, 0x0008, 0x0118);
            const auto MappingResourceName    = read_text(*code_item, 0x0008, 0x0122);
            for(auto &c : cc.contours){
                insert_or_append(c.metadata, "RTROIObservationsSequence/RTROIIdentificationCodeSequence/CodeValue", CodeValue);
                insert_or_append(c.metadata, "RTROIObservationsSequence/RTROIIdentificationCodeSequence/CodingSchemeDesignator", CodingSchemeDesignator);
                insert_or_append(c.metadata, "RTROIObservationsSequence/RTROIIdentificationCodeSequence/CodingSchemeVersion", CodingSchemeVersion);
                insert_or_append(c.metadata, "RTROIObservationsSequence/RTROIIdentificationCodeSequence/CodeMeaning", CodeMeaning);
                insert_or_append(c.metadata, "RTROIObservationsSequence/RTROIIdentificationCodeSequence/MappingResource", MappingResource);
                insert_or_append(c.metadata, "RTROIObservationsSequence/RTROIIdentificationCodeSequence/ContextGroupVersion", ContextGroupVersion);
                insert_or_append(c.metadata, "RTROIObservationsSequence/RTROIIdentificationCodeSequence/ContextIdentifier", ContextIdentifier);
                insert_or_append(c.metadata, "RTROIObservationsSequence/RTROIIdentificationCodeSequence/ContextUID", ContextUID);
                insert_or_append(c.metadata, "RTROIObservationsSequence/RTROIIdentificationCodeSequence/MappingResourceUID", MappingResourceUID);
                insert_or_append(c.metadata, "RTROIObservationsSequence/RTROIIdentificationCodeSequence/MappingResourceName", MappingResourceName);
            }
        }
    }

    // Move sorted contour collections into output.
    for(auto &m_it : mapcache){
        output->ccs.emplace_back();
        output->ccs.back() = m_it.second;
    }

    // Find minimum separation.
    double min_spacing = 1E30;
    for(auto &cc : output->ccs){
        if(cc.contours.size() < 2) continue;
        for(auto c1_it = cc.contours.begin(); c1_it != --(cc.contours.end()); ++c1_it){
            auto c2_it = c1_it;
            ++c2_it;
            const double h1 = c1_it->Average_Point().Dot(vec3<double>(0.0,0.0,1.0));
            const double h2 = c2_it->Average_Point().Dot(vec3<double>(0.0,0.0,1.0));
            const double spacing = YGORABS(h2 - h1);
            if((spacing < min_spacing) && (spacing > 1E-3)) min_spacing = spacing;
        }
    }
    for(auto &cc_it : output->ccs){
        for(auto &c : cc_it.contours) c.metadata["MinimumSeparation"] = std::to_string(min_spacing);
    }

    return output;
}


// ============================================================================
// Image loading (CT, MR, etc.).
// ============================================================================

std::unique_ptr<Image_Array> Load_Image_Array_from_node(const Node &root, const std::string &filename){
    const auto inf = std::numeric_limits<double>::infinity();
    auto out = std::make_unique<Image_Array>();

    const auto tlm = get_metadata_top_level_tags_from_node(root, filename);
    const auto modality = read_text(root, 0x0008, 0x0060);
    const auto frame_count = read_first_long(root, 0x0028, 0x0008).value_or(1);

    // Helper lambdas matching Imebra_Shim patterns.
    const auto l_coalesce_metadata_as_vector_double = [&tlm](const std::list<std::string> &keys){
        return convert_to_vector_double(coalesce_metadata(tlm, keys));
    };
    const auto l_coalesce_metadata_as_double = [&tlm](const std::list<std::string> &keys){
        return convert_first_to_double(coalesce_metadata(tlm, keys));
    };
    const auto l_coalesce_as_string = [&root](const std::vector<std::vector<path_node>> &ds){
        return convert_first_to_string(coalesce_paths(root, ds));
    };
    const auto l_coalesce_as_double = [&root](const std::vector<std::vector<path_node>> &ds){
        return convert_first_to_double(coalesce_paths(root, ds));
    };
    const auto l_coalesce_as_long_int = [&root](const std::vector<std::vector<path_node>> &ds){
        return convert_first_to_long_int(coalesce_paths(root, ds));
    };
    const auto l_coalesce_as_vector_double = [&root](const std::vector<std::vector<path_node>> &ds){
        return convert_to_vector_double(coalesce_paths(root, ds));
    };

    // Extract pixel data.
    auto pixel_data_opt = extract_native_pixel_data(root);
    if(!pixel_data_opt){
        pixel_data_opt = extract_encapsulated_pixel_data(root);
    }
    if(!pixel_data_opt){
        throw std::domain_error("This file does not have accessible pixel data");
    }
    auto &pixel_images = pixel_data_opt.value();

    // Determine if modality LUT should be applied. Look for real-world mapping first.
    std::function<float(float)> real_world_mapping;

    // Build candidate paths for real-world mapping intercept/slope.
    // Always prefer global RealWorldValueMappingSequence (0040,9096).
    std::vector<std::vector<path_node>> rw_intercept_paths = {
        { {0x0040, 0x9096, 0}, {0x0040, 0x9224, 0} }
    };
    std::vector<std::vector<path_node>> rw_slope_paths = {
        { {0x0040, 0x9096, 0}, {0x0040, 0x9225, 0} }
    };

    // Only fall back to a per-frame mapping when there is exactly one frame. For
    // multi-frame images, the mapping can vary per frame, so using item 0 for all
    // frames would be unsafe.
    if(pixel_images.size() == 1){
        rw_intercept_paths.push_back(
            { {0x5200, 0x9230, 0}, {0x0028, 0x9145, 0}, {0x0028, 0x1052, 0} }
        );
        rw_slope_paths.push_back(
            { {0x5200, 0x9230, 0}, {0x0028, 0x9145, 0}, {0x0028, 0x1053, 0} }
        );
    }

    auto rw_lut_intercept = l_coalesce_as_double(rw_intercept_paths);
    auto rw_lut_slope     = l_coalesce_as_double(rw_slope_paths);
    if(rw_lut_intercept && rw_lut_slope){
        auto m = rw_lut_slope.value();
        auto b = rw_lut_intercept.value();
        real_world_mapping = [m, b](float x) -> float { return b + m*x; };
    }

    // Fall back to standard RescaleSlope/Intercept if no real-world mapping.
    auto lut_intercept = read_first_double(root, 0x0028, 0x1052);
    auto lut_slope     = read_first_double(root, 0x0028, 0x1053);
    if(!real_world_mapping && lut_intercept && lut_slope){
        auto m = lut_slope.value();
        auto b = lut_intercept.value();
        real_world_mapping = [m, b](float x) -> float { return b + m*x; };
    }

    // If we found a real-world or modality LUT, we apply it ourselves.
    // If not, apply the DCMA_DICOM modality LUT pipeline.
    if(!real_world_mapping){
        auto mlut = get_modality_lut_params(root);
        if(mlut){
            for(auto &img : pixel_images.images){
                apply_modality_lut(img, mlut.value());
            }
        }
    }

    // Build image array from pixel data.
    int64_t frame_idx = 0;
    for(auto &pimg : pixel_images.images){
        if(frame_idx >= frame_count) break;
        auto l_meta = tlm;
        out->imagecoll.images.emplace_back();

        const auto image_rows = static_cast<int64_t>(pimg.rows);
        const auto image_cols = static_cast<int64_t>(pimg.columns);
        const auto img_chnls  = static_cast<int64_t>(pimg.channels);

        // Position.
        auto image_pos_x_opt = l_coalesce_as_double(
            { { {0x0020, 0x0032, 0} }, { {0x3002, 0x0012, 0} } });
        auto image_pos_y_opt = l_coalesce_as_double(
            { { {0x0020, 0x0032, 1} }, { {0x3002, 0x0012, 1} } });
        auto image_pos_z_opt = l_coalesce_as_double(
            { { {0x0020, 0x0032, 2} }, { {0x3002, 0x000d, 2} } });

        if(!image_pos_x_opt || !image_pos_y_opt || !image_pos_z_opt){
            const auto xyz = l_coalesce_metadata_as_vector_double({"CSAImage/ImagePositionPatient"});
            if(xyz.size() == 3UL){
                image_pos_x_opt = xyz.at(0);
                image_pos_y_opt = xyz.at(1);
                image_pos_z_opt = xyz.at(2);
            }
        }
        if(!image_pos_z_opt && (modality == "RTIMAGE")){
            const auto RTImageSID = read_first_double(root, 0x3002, 0x0026).value_or(1000.0);
            const auto RadMchnSAD = read_first_double(root, 0x3002, 0x0022).value_or(1000.0);
            image_pos_z_opt = (RadMchnSAD - RTImageSID);
        }
        const vec3<double> image_pos(image_pos_x_opt.value_or(0.0),
                                     image_pos_y_opt.value_or(0.0),
                                     image_pos_z_opt.value_or(0.0));
        insert_if_new(l_meta, "ImagePositionPatient",
                      std::to_string(image_pos.x) + '\\' + std::to_string(image_pos.y) + '\\' + std::to_string(image_pos.z));

        const auto image_anchor = vec3<double>(0.0, 0.0, 0.0);

        // Orientation.
        const auto orien_vec = read_all_doubles(root, 0x0020, 0x0037);
        vec3<double> image_orien_r(1.0, 0.0, 0.0);
        vec3<double> image_orien_c(0.0, 1.0, 0.0);
        if(orien_vec.size() == 6UL){
            image_orien_r = vec3<double>(orien_vec[0], orien_vec[1], orien_vec[2]).unit();
            image_orien_c = vec3<double>(orien_vec[3], orien_vec[4], orien_vec[5]).unit();
        }
        insert_if_new(l_meta, "ImageOrientationPatient",
                      std::to_string(image_orien_r.x) + '\\' + std::to_string(image_orien_r.y) + '\\' + std::to_string(image_orien_r.z) + '\\'
                    + std::to_string(image_orien_c.x) + '\\' + std::to_string(image_orien_c.y) + '\\' + std::to_string(image_orien_c.z));

        // Pixel spacing.
        const auto pxl_spacing = read_all_doubles(root, 0x0028, 0x0030);
        double image_pxldy = 1.0, image_pxldx = 1.0;
        if(pxl_spacing.size() == 2UL){
            image_pxldy = pxl_spacing[0];
            image_pxldx = pxl_spacing[1];
        }else{
            const auto alt_spacing = read_all_doubles(root, 0x3002, 0x0011); // ImagePlanePixelSpacing
            if(alt_spacing.size() == 2UL){
                image_pxldy = alt_spacing[0];
                image_pxldx = alt_spacing[1];
            }
        }
        insert_if_new(l_meta, "PixelSpacing", std::to_string(image_pxldy) + '\\' + std::to_string(image_pxldx));

        // Thickness.
        const auto image_thickness = read_first_double(root, 0x0018, 0x0050).value_or(1.0);
        insert_if_new(l_meta, "SliceThickness", std::to_string(image_thickness));

        // Populate image.
        out->imagecoll.images.back().metadata = l_meta;
        out->imagecoll.images.back().init_orientation(image_orien_r, image_orien_c);
        out->imagecoll.images.back().init_buffer(image_rows, image_cols, img_chnls);
        out->imagecoll.images.back().init_spatial(image_pxldx, image_pxldy, image_thickness, image_anchor, image_pos);

        // Copy pixel data, applying real-world mapping if present.
        for(int64_t row = 0; row < image_rows; ++row){
            for(int64_t col = 0; col < image_cols; ++col){
                for(int64_t chnl = 0; chnl < img_chnls; ++chnl){
                    auto val = pimg.value(row, col, chnl);
                    if(real_world_mapping) val = real_world_mapping(val);
                    out->imagecoll.images.back().reference(row, col, chnl) = val;
                }
            }
        }

        ++frame_idx;
    }
    return out;
}


// ============================================================================
// Dose loading (RTDOSE).
// ============================================================================

std::unique_ptr<Image_Array> Load_Dose_Array_from_node(const Node &root, const std::string &filename){
    auto metadata = get_metadata_top_level_tags_from_node(root, filename);
    if(metadata["Modality"] != "RTDOSE"){
        throw std::runtime_error("Unsupported modality");
    }

    auto out = std::make_unique<Image_Array>();

    const auto image_pos_vec = read_all_doubles(root, 0x0020, 0x0032);
    if(image_pos_vec.size() < 3) throw std::domain_error("Missing ImagePositionPatient");
    const vec3<double> image_pos(image_pos_vec[0], image_pos_vec[1], image_pos_vec[2]);

    const auto orien_vec = read_all_doubles(root, 0x0020, 0x0037);
    if(orien_vec.size() < 6) throw std::domain_error("Missing ImageOrientationPatient");
    const vec3<double> image_orien_r = vec3<double>(orien_vec[0], orien_vec[1], orien_vec[2]).unit();
    const vec3<double> image_orien_c = vec3<double>(orien_vec[3], orien_vec[4], orien_vec[5]).unit();
    const vec3<double> image_stack_unit = (image_orien_r.Cross(image_orien_c)).unit();
    const vec3<double> image_anchor(0.0, 0.0, 0.0);

    const auto frame_count = read_first_long(root, 0x0028, 0x0008).value_or(0);
    if(frame_count == 0) throw std::domain_error("No frames found in dose file");

    // GridFrameOffsetVector (3004,000C).
    const auto gfov_vec = read_all_doubles(root, 0x3004, 0x000c);
    if(static_cast<int64_t>(gfov_vec.size()) != frame_count){
        throw std::domain_error("GridFrameOffsetVector size does not match NumberOfFrames");
    }

    const double image_thickness = (gfov_vec.size() > 1) ? (gfov_vec[1] - gfov_vec[0]) : 1.0;

    const auto image_rows  = read_first_long(root, 0x0028, 0x0010).value_or(0);
    const auto image_cols  = read_first_long(root, 0x0028, 0x0011).value_or(0);
    const auto pxl_spacing = read_all_doubles(root, 0x0028, 0x0030);
    const auto image_pxldy = (pxl_spacing.size() >= 1) ? pxl_spacing[0] : 1.0;
    const auto image_pxldx = (pxl_spacing.size() >= 2) ? pxl_spacing[1] : 1.0;
    const auto grid_scale  = read_first_double(root, 0x3004, 0x000e).value_or(1.0);

    // Extract pixel data.
    auto pixel_data_opt = extract_native_pixel_data(root);
    if(!pixel_data_opt){
        pixel_data_opt = extract_encapsulated_pixel_data(root);
    }
    if(!pixel_data_opt){
        throw std::domain_error("Dose file does not have accessible pixel data");
    }
    auto &pixel_images = pixel_data_opt.value();

    // Build per-frame images.
    int64_t curr_frame = 0;
    auto pimg_it = pixel_images.images.begin();
    for(int64_t f = 0; f < frame_count && pimg_it != pixel_images.images.end(); ++f, ++pimg_it, ++curr_frame){
        out->imagecoll.images.emplace_back();

        const auto gvof_offset = gfov_vec[static_cast<size_t>(f)];
        const auto img_offset = image_pos + image_stack_unit * gvof_offset;
        const auto img_chnls = static_cast<int64_t>(pimg_it->channels);

        out->imagecoll.images.back().metadata = metadata;
        out->imagecoll.images.back().init_orientation(image_orien_r, image_orien_c);
        out->imagecoll.images.back().init_buffer(image_rows, image_cols, img_chnls);
        out->imagecoll.images.back().init_spatial(image_pxldx, image_pxldy, image_thickness, image_anchor, img_offset);

        out->imagecoll.images.back().metadata["GridFrameOffset"] = std::to_string(gvof_offset);
        out->imagecoll.images.back().metadata["Frame"] = std::to_string(curr_frame);
        out->imagecoll.images.back().metadata["ImagePositionPatient"] = img_offset.to_string();

        // Copy pixel data with grid scaling.
        for(int64_t row = 0; row < image_rows; ++row){
            for(int64_t col = 0; col < image_cols; ++col){
                for(int64_t chnl = 0; chnl < img_chnls; ++chnl){
                    const auto val = pimg_it->value(row, col, chnl);
                    out->imagecoll.images.back().reference(row, col, chnl) = val * static_cast<float>(grid_scale);
                }
            }
        }
    }

    return out;
}


// ============================================================================
// RT Plan loading.
// ============================================================================

std::unique_ptr<RTPlan> Load_RTPlan_from_node(const Node &root, const std::string &filename){
    auto out = std::make_unique<RTPlan>();
    out->metadata = get_metadata_top_level_tags_from_node(root, filename);
    if(out->metadata["Modality"] != "RTPLAN"){
        throw std::runtime_error("Unsupported modality");
    }

    // DoseReferenceSequence (300A,0010).
    uint32_t i = 0;
    for(const auto *item : get_seq_items(root, 0x300a, 0x0010)){
        const std::string prfx = "DoseReferenceSequence"s + std::to_string(i) + "/"s;
        insert_seq_item_tag_value(out->metadata, *item, 0x300a, 0x0012, prfx + "DoseReferenceNumber");
        insert_seq_item_tag_value(out->metadata, *item, 0x300a, 0x0013, prfx + "DoseReferenceUID");
        insert_seq_item_tag_value(out->metadata, *item, 0x300a, 0x0014, prfx + "DoseReferenceStructureType");
        insert_seq_item_tag_value(out->metadata, *item, 0x300a, 0x0016, prfx + "DoseReferenceDescription");
        insert_seq_item_tag_value(out->metadata, *item, 0x300a, 0x0020, prfx + "DoseReferenceType");
        insert_seq_item_tag_value(out->metadata, *item, 0x300a, 0x0026, prfx + "TargetPrescriptionDose");
        ++i;
    }

    // ReferencedStructureSetSequence (300C,0060).
    i = 0;
    for(const auto *item : get_seq_items(root, 0x300c, 0x0060)){
        const std::string prfx = "ReferencedStructureSetSequence"s + std::to_string(i) + "/"s;
        insert_seq_item_tag_value(out->metadata, *item, 0x0008, 0x1150, prfx + "ReferencedSOPClassUID");
        insert_seq_item_tag_value(out->metadata, *item, 0x0008, 0x1155, prfx + "ReferencedSOPInstanceUID");
        ++i;
    }

    // ToleranceTableSequence (300A,0040).
    i = 0;
    for(const auto *tol_item : get_seq_items(root, 0x300a, 0x0040)){
        const std::string prfx = "ToleranceTableSequence"s + std::to_string(i) + "/"s;
        insert_seq_item_tag_value(out->metadata, *tol_item, 0x300a, 0x0042, prfx + "ToleranceTableNumber");
        insert_seq_item_tag_value(out->metadata, *tol_item, 0x300a, 0x0043, prfx + "ToleranceTableLabel");
        insert_seq_item_tag_value(out->metadata, *tol_item, 0x300a, 0x0044, prfx + "GantryAngleTolerance");
        insert_seq_item_tag_value(out->metadata, *tol_item, 0x300a, 0x0046, prfx + "BeamLimitingDeviceAngleTolerance");
        insert_seq_item_tag_value(out->metadata, *tol_item, 0x300a, 0x004c, prfx + "PatientSupportAngleTolerance");
        insert_seq_item_tag_value(out->metadata, *tol_item, 0x300a, 0x0051, prfx + "TableTopVerticalPositionTolerance");
        insert_seq_item_tag_value(out->metadata, *tol_item, 0x300a, 0x0052, prfx + "TableTopLongitudinalPositionTolerance");
        insert_seq_item_tag_value(out->metadata, *tol_item, 0x300a, 0x0053, prfx + "TableTopLateralPositionTolerance");

        uint32_t j = 0;
        for(const auto *bld_item : get_seq_items(*tol_item, 0x300a, 0x0048)){
            const std::string pprfx = prfx + "BeamLimitingDeviceToleranceSequence"s + std::to_string(j) + "/"s;
            insert_seq_item_tag_value(out->metadata, *bld_item, 0x300a, 0x004a, pprfx + "BeamLimitingDevicePositionTolerance");
            insert_seq_item_tag_value(out->metadata, *bld_item, 0x300a, 0x00b8, pprfx + "RTBeamLimitingDeviceType");
            ++j;
        }
        ++i;
    }

    // FractionGroupSequence (300A,0070).
    i = 0;
    for(const auto *fg_item : get_seq_items(root, 0x300a, 0x0070)){
        const std::string prfx = "FractionGroupSequence"s + std::to_string(i) + "/"s;
        insert_seq_item_tag_value(out->metadata, *fg_item, 0x300a, 0x0071, prfx + "FractionGroupNumber");
        insert_seq_item_tag_value(out->metadata, *fg_item, 0x300a, 0x0078, prfx + "NumberOfFractionsPlanned");
        insert_seq_item_tag_value(out->metadata, *fg_item, 0x300a, 0x0080, prfx + "NumberOfBeams");
        insert_seq_item_tag_value(out->metadata, *fg_item, 0x300a, 0x00a0, prfx + "NumberOfBrachyApplicationSetups");

        uint32_t j = 0;
        for(const auto *rb_item : get_seq_items(*fg_item, 0x300c, 0x0004)){
            const std::string pprfx = prfx + "ControlPointSequence"s + std::to_string(j) + "/"s;
            insert_seq_item_tag_value(out->metadata, *rb_item, 0x300a, 0x0084, pprfx + "BeamDose");
            insert_seq_item_tag_value(out->metadata, *rb_item, 0x300a, 0x0086, pprfx + "BeamMeterset");
            insert_seq_item_tag_value(out->metadata, *rb_item, 0x300c, 0x0006, pprfx + "ReferencedBeamNumber");
            ++j;
        }
        ++i;
    }

    // BeamSequence (300A,00B0).
    for(const auto *beam_item : get_seq_items(root, 0x300a, 0x00b0)){
        out->dynamic_states.emplace_back();
        Dynamic_Machine_State *dms = &(out->dynamic_states.back());

        insert_seq_item_tag_value(dms->metadata, *beam_item, 0x0008, 0x0070, "Manufacturer");
        insert_seq_item_tag_value(dms->metadata, *beam_item, 0x0008, 0x0080, "InstitutionName");
        insert_seq_item_tag_value(dms->metadata, *beam_item, 0x0008, 0x1040, "InstitutionalDepartmentName");
        insert_seq_item_tag_value(dms->metadata, *beam_item, 0x0008, 0x1090, "ManufacturerModelName");
        insert_seq_item_tag_value(dms->metadata, *beam_item, 0x0018, 0x1000, "DeviceSerialNumber");
        insert_seq_item_tag_value(dms->metadata, *beam_item, 0x300a, 0x00b2, "TreatmentMachineName");
        insert_seq_item_tag_value(dms->metadata, *beam_item, 0x300a, 0x00b3, "PrimaryDosimeterUnit");
        insert_seq_item_tag_value(dms->metadata, *beam_item, 0x300a, 0x00b4, "SourceAxisDistance");
        insert_seq_item_tag_value(dms->metadata, *beam_item, 0x300a, 0x00c0, "BeamNumber");
        insert_seq_item_tag_value(dms->metadata, *beam_item, 0x300a, 0x00c2, "BeamName");
        insert_seq_item_tag_value(dms->metadata, *beam_item, 0x300a, 0x00c4, "BeamType");
        insert_seq_item_tag_value(dms->metadata, *beam_item, 0x300a, 0x00c6, "RadiationType");
        insert_seq_item_tag_value(dms->metadata, *beam_item, 0x300a, 0x00ce, "TreatmentDeliveryType");
        insert_seq_item_tag_value(dms->metadata, *beam_item, 0x300a, 0x00d0, "NumberOfWedges");
        insert_seq_item_tag_value(dms->metadata, *beam_item, 0x300a, 0x00e0, "NumberOfCompensators");
        insert_seq_item_tag_value(dms->metadata, *beam_item, 0x300a, 0x00ed, "NumberOfBoli");
        insert_seq_item_tag_value(dms->metadata, *beam_item, 0x300a, 0x00f0, "NumberOfBlocks");
        insert_seq_item_tag_value(dms->metadata, *beam_item, 0x300a, 0x010e, "FinalCumulativeMetersetWeight");
        insert_seq_item_tag_value(dms->metadata, *beam_item, 0x300a, 0x0110, "NumberOfControlPoints");
        insert_seq_item_tag_value(dms->metadata, *beam_item, 0x300c, 0x006a, "ReferencedPatientSetupNumber");
        insert_seq_item_tag_value(dms->metadata, *beam_item, 0x300c, 0x00a0, "ReferencedToleranceTableNumber");

        auto BeamNumberOpt = read_first_long(*beam_item, 0x300a, 0x00c0);
        auto BeamNameOpt = convert_first_to_string(extract_tag_as_strings(*beam_item, 0x300a, 0x00c2));
        auto FinalCumulativeMetersetWeightOpt = read_first_double(*beam_item, 0x300a, 0x010e);
        auto NumberOfControlPointsOpt = read_first_long(*beam_item, 0x300a, 0x0110);

        if(!BeamNumberOpt || !BeamNameOpt || !FinalCumulativeMetersetWeightOpt || !NumberOfControlPointsOpt){
            throw std::runtime_error("RTPLAN is missing required data. Refusing to continue.");
        }
        dms->BeamNumber = BeamNumberOpt.value();
        dms->FinalCumulativeMetersetWeight = FinalCumulativeMetersetWeightOpt.value();

        // BeamLimitingDeviceSequence (300A,00B6).
        for(const auto *bld_item : get_seq_items(*beam_item, 0x300a, 0x00b6)){
            insert_seq_item_tag_value(dms->metadata, *bld_item, 0x300a, 0x00b8, "RTBeamLimitingDeviceType");
            insert_seq_item_tag_value(dms->metadata, *bld_item, 0x300a, 0x00bc, "NumberOfLeafJawPairs");
            insert_seq_item_tag_value(dms->metadata, *bld_item, 0x300a, 0x00be, "LeafPositionBoundaries");
        }

        const auto nan = std::numeric_limits<double>::quiet_NaN();
        const auto dir_str_to_double = [](const std::string &s) -> double {
            const auto ctrim = CANONICALIZE::TRIM_ENDS | CANONICALIZE::TO_UPPER;
            const auto trimmed = Canonicalize_String2(s, ctrim);
            if(trimmed == "NONE") return 0.0;
            if(trimmed == "CCW") return 1.0;
            if(trimmed == "CW") return -1.0;
            return std::numeric_limits<double>::quiet_NaN();
        };

        // ControlPointSequence (300A,0111).
        for(const auto *cp_item : get_seq_items(*beam_item, 0x300a, 0x0111)){
            dms->static_states.emplace_back();
            Static_Machine_State *sms = &(dms->static_states.back());

            auto ControlPointIndexOpt = read_first_long(*cp_item, 0x300a, 0x0112);
            auto CumulativeMetersetWeightOpt = read_first_double(*cp_item, 0x300a, 0x0134);

            if(!ControlPointIndexOpt || !CumulativeMetersetWeightOpt){
                throw std::runtime_error("RTPLAN has an invalid control point. Refusing to continue.");
            }

            sms->ControlPointIndex = ControlPointIndexOpt.value();
            sms->CumulativeMetersetWeight = CumulativeMetersetWeightOpt.value();

            sms->GantryAngle = read_first_double(*cp_item, 0x300a, 0x011e).value_or(nan);
            sms->GantryRotationDirection = dir_str_to_double(read_text(*cp_item, 0x300a, 0x011f));
            sms->BeamLimitingDeviceAngle = read_first_double(*cp_item, 0x300a, 0x0120).value_or(nan);
            sms->BeamLimitingDeviceRotationDirection = dir_str_to_double(read_text(*cp_item, 0x300a, 0x0121));
            sms->PatientSupportAngle = read_first_double(*cp_item, 0x300a, 0x0122).value_or(nan);
            sms->PatientSupportRotationDirection = dir_str_to_double(read_text(*cp_item, 0x300a, 0x0123));
            sms->TableTopEccentricAngle = read_first_double(*cp_item, 0x300a, 0x0125).value_or(nan);
            sms->TableTopEccentricRotationDirection = dir_str_to_double(read_text(*cp_item, 0x300a, 0x0126));
            sms->TableTopVerticalPosition = read_first_double(*cp_item, 0x300a, 0x0128).value_or(nan);
            sms->TableTopLongitudinalPosition = read_first_double(*cp_item, 0x300a, 0x0129).value_or(nan);
            sms->TableTopLateralPosition = read_first_double(*cp_item, 0x300a, 0x012a).value_or(nan);
            sms->TableTopPitchAngle = read_first_double(*cp_item, 0x300a, 0x0140).value_or(nan);
            sms->TableTopPitchRotationDirection = dir_str_to_double(read_text(*cp_item, 0x300a, 0x0142));
            sms->TableTopRollAngle = read_first_double(*cp_item, 0x300a, 0x0144).value_or(nan);
            sms->TableTopRollRotationDirection = dir_str_to_double(read_text(*cp_item, 0x300a, 0x0146));

            auto iso_strs = extract_tag_as_strings(*cp_item, 0x300a, 0x012c);
            sms->IsocentrePosition = convert_to_vec3_double(iso_strs).value_or(vec3<double>(nan,nan,nan));

            // BeamLimitingDevicePositionSequence (300A,011A).
            for(const auto *bldp_item : get_seq_items(*cp_item, 0x300a, 0x011a)){
                auto type_opt = convert_first_to_string(extract_tag_as_strings(*bldp_item, 0x300a, 0x00b8));
                auto positions_vec = convert_to_vector_double(extract_tag_as_strings(*bldp_item, 0x300a, 0x011c));
                if(!type_opt || positions_vec.empty()){
                    throw std::runtime_error("RTPLAN has an invalid beam limiting device position.");
                }
                const auto ctrim = CANONICALIZE::TRIM_ENDS | CANONICALIZE::TO_UPPER;
                const auto trimmed = Canonicalize_String2(type_opt.value(), ctrim);
                if(trimmed == "ASYMX" || trimmed == "X"){
                    sms->JawPositionsX = positions_vec;
                }else if(trimmed == "ASYMY" || trimmed == "Y"){
                    sms->JawPositionsY = positions_vec;
                }else if(trimmed == "MLCX"){
                    sms->MLCPositionsX = positions_vec;
                }
            }
        } // ControlPointSequence.
    } // BeamSequence.

    // Order the beams.
    std::sort(std::begin(out->dynamic_states), std::end(out->dynamic_states),
              [](const Dynamic_Machine_State &L, const Dynamic_Machine_State &R){
                  return (L.BeamNumber < R.BeamNumber);
              });
    for(auto &ds : out->dynamic_states) ds.sort_states();
    for(const auto &ds : out->dynamic_states){
        if(ds.static_states.size() < 2){
            throw std::runtime_error("Insufficient number of control points. Refusing to continue.");
        }
        if(!ds.verify_states_are_ordered()){
            throw std::runtime_error("A control point is missing. Refusing to continue.");
        }
    }
    for(auto &ds : out->dynamic_states) ds.normalize_states();

    return std::move(out);
}


// ============================================================================
// Registration loading (REG).
// ============================================================================

std::unique_ptr<Transform3> Load_Transform_from_node(const Node &root, const std::string &filename){
    auto out = std::make_unique<Transform3>();
    out->metadata = get_metadata_top_level_tags_from_node(root, filename);
    if(out->metadata["Modality"] != "REG"){
        throw std::runtime_error("Unsupported modality");
    }

    // RegistrationSequence (0070,0308).
    uint32_t ri = 0;
    for(const auto *reg_item : get_seq_items(root, 0x0070, 0x0308)){
        const std::string prfx = "RegistrationSequence"s + std::to_string(ri) + "/"s;
        insert_seq_item_tag_value(out->metadata, *reg_item, 0x0020, 0x0052, prfx + "FrameOfReferenceUID");

        // MatrixRegistrationSequence (0070,0309).
        for(const auto *mreg_item : get_seq_items(*reg_item, 0x0070, 0x0309)){
            // MatrixSequence (0070,030A).
            for(const auto *mat_item : get_seq_items(*mreg_item, 0x0070, 0x030a)){
                insert_seq_item_tag_value(out->metadata, *mat_item, 0x0070, 0x030c, prfx + "FrameOfReferenceTransformationMatrixType");
                const auto mat_vec = convert_to_vector_double(extract_tag_as_strings(*mat_item, 0x3006, 0x00c6));
                if(mat_vec.size() != 16) continue;
                out->transform = extract_affine_transform(mat_vec);
            }
        }
        ++ri;
    }

    // DeformableRegistrationSequence (0064,0002).
    for(const auto *dreg_item : get_seq_items(root, 0x0064, 0x0002)){
        insert_seq_item_tag_value(out->metadata, *dreg_item, 0x0064, 0x0003, "DeformableRegistrationSequence/SourceFrameOfReferenceUID");

        // DeformableRegistrationGridSequence (0064,0005).
        for(const auto *grid_item : get_seq_items(*dreg_item, 0x0064, 0x0005)){
            const auto ImagePositionPatient = convert_to_vec3_double(extract_tag_as_strings(*grid_item, 0x0020, 0x0032));
            const auto ImageOrientationPatient = convert_to_vector_double(extract_tag_as_strings(*grid_item, 0x0020, 0x0037));
            const auto GridDimensions = convert_to_vec3_int64(extract_tag_as_strings(*grid_item, 0x0064, 0x0007));
            const auto GridResolution = convert_to_vec3_double(extract_tag_as_strings(*grid_item, 0x0064, 0x0008));

            const auto zero = vec3<double>(0.0, 0.0, 0.0);
            const auto zeroL = vec3<int64_t>(0, 0, 0);
            const auto image_pos = ImagePositionPatient.value();

            if(ImageOrientationPatient.size() != 6) throw std::runtime_error("Invalid ImageOrientationPatient tag");
            const auto image_orien_r = vec3<double>(ImageOrientationPatient[0], ImageOrientationPatient[1], ImageOrientationPatient[2]).unit();
            const auto image_orien_c = vec3<double>(ImageOrientationPatient[3], ImageOrientationPatient[4], ImageOrientationPatient[5]).unit();
            const auto image_ortho = image_orien_c.Cross(image_orien_r).unit();

            const auto image_cols = GridDimensions.value_or(zeroL).x;
            const auto image_rows = GridDimensions.value_or(zeroL).y;
            const auto image_chns = static_cast<int64_t>(3);
            const auto image_imgs = GridDimensions.value_or(zeroL).z;
            const auto image_buffer_length = image_rows * image_cols * image_chns * image_imgs;
            if(image_buffer_length <= 0) throw std::runtime_error("Invalid image buffer dimensions");

            const auto image_pxldx = GridResolution.value_or(zero).x;
            const auto image_pxldy = GridResolution.value_or(zero).y;
            const auto image_pxldz = GridResolution.value_or(zero).z;
            if(image_pxldy * image_pxldx * image_pxldz <= 0.0) throw std::runtime_error("Invalid grid voxel dimensions");

            const vec3<double> image_anchor(0.0, 0.0, 0.0);

            const auto VectorGridData = convert_to_vector_double(extract_tag_as_strings(*grid_item, 0x0064, 0x0009));
            if(static_cast<int64_t>(VectorGridData.size()) != image_buffer_length){
                throw std::runtime_error("Encountered incomplete VectorGridData tag");
            }

            auto v_it = std::begin(VectorGridData);
            planar_image_collection<double,double> pic;
            for(int64_t n = 0; n < image_imgs; ++n){
                pic.images.emplace_back();
                pic.images.back().init_orientation(image_orien_r, image_orien_c);
                pic.images.back().init_buffer(image_rows, image_cols, image_chns);
                const auto image_offset = image_pos + image_ortho * n;
                pic.images.back().init_spatial(image_pxldx, image_pxldy, image_pxldz, image_anchor, image_offset);

                for(int64_t row = 0; row < image_rows; ++row){
                    for(int64_t col = 0; col < image_cols; ++col){
                        for(int64_t chn = 0; chn < image_chns; ++chn, ++v_it){
                            pic.images.back().reference(row, col, chn) = *v_it;
                        }
                    }
                }
            }
            YLOGINFO("Loaded deformation field with dimensions " << image_rows << " x " << image_cols << " x " << image_imgs);
            deformation_field t(std::move(pic));
            out->transform = t;
        }
    }

    return std::move(out);
}


// ============================================================================
// File-based convenience wrappers.
// ============================================================================

std::string node_get_tag_as_string(const std::filesystem::path &filename, uint16_t group, uint16_t tag){
    try{
        auto root = parse_dicom_file(filename);
        return get_tag_as_string_from_node(root, group, tag);
    }catch(const std::exception &){
        return "";
    }
}

std::string node_get_modality(const std::filesystem::path &filename){
    return node_get_tag_as_string(filename, 0x0008, 0x0060);
}

std::string node_get_patient_ID(const std::filesystem::path &filename){
    return node_get_tag_as_string(filename, 0x0010, 0x0020);
}

metadata_map_t node_get_metadata_top_level_tags(const std::filesystem::path &filename){
    auto root = parse_dicom_file(filename);
    return get_metadata_top_level_tags_from_node(root, filename.string());
}

bimap<std::string,int64_t> node_get_ROI_tags_and_numbers(const std::filesystem::path &filename){
    auto root = parse_dicom_file(filename);
    return get_ROI_tags_and_numbers_from_node(root);
}

std::unique_ptr<Contour_Data> node_get_Contour_Data(const std::filesystem::path &filename){
    auto root = parse_dicom_file(filename);
    return get_Contour_Data_from_node(root, filename.string());
}

std::unique_ptr<Image_Array> node_Load_Image_Array(const std::filesystem::path &filename){
    auto root = parse_dicom_file(filename);
    return Load_Image_Array_from_node(root, filename.string());
}

std::list<std::shared_ptr<Image_Array>> node_Load_Image_Arrays(const std::list<std::filesystem::path> &filenames){
    std::list<std::shared_ptr<Image_Array>> out;
    for(const auto &filename : filenames){
        out.push_back(node_Load_Image_Array(filename));
    }
    return out;
}

std::unique_ptr<Image_Array> node_Collate_Image_Arrays(std::list<std::shared_ptr<Image_Array>> &in){
    auto out = std::make_unique<Image_Array>();
    if(in.empty()) return out;
    while(!in.empty()){
        auto pic_it = in.begin();
        const bool GeometricalOverlapOK = true;
        if(!out->imagecoll.Collate_Images((*pic_it)->imagecoll, GeometricalOverlapOK)){
            in.push_back(std::move(out));
            return nullptr;
        }
        pic_it = in.erase(pic_it);
    }
    return out;
}

std::unique_ptr<Image_Array> node_Load_Dose_Array(const std::filesystem::path &filename){
    auto root = parse_dicom_file(filename);
    return Load_Dose_Array_from_node(root, filename.string());
}

std::list<std::shared_ptr<Image_Array>> node_Load_Dose_Arrays(const std::list<std::filesystem::path> &filenames){
    std::list<std::shared_ptr<Image_Array>> out;
    for(const auto &filename : filenames){
        out.push_back(node_Load_Dose_Array(filename));
    }
    return out;
}

std::unique_ptr<RTPlan> node_Load_RTPlan(const std::filesystem::path &filename){
    auto root = parse_dicom_file(filename);
    return Load_RTPlan_from_node(root, filename.string());
}

std::unique_ptr<Transform3> node_Load_Transform(const std::filesystem::path &filename){
    auto root = parse_dicom_file(filename);
    return Load_Transform_from_node(root, filename.string());
}


} // namespace DCMA_DICOM

