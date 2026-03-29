// DCMA_DICOM_Dictionaries.cc - A part of DICOMautomaton 2026. Written by hal clark.
//
// This file contains DICOM dictionary definitions and dictionary I/O routines.
//

#include <cstdint>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <map>
#include <vector>
#include <utility>
#include <cctype>
#include <stdexcept>

#include "DCMA_DICOM_Dictionaries.h"

namespace DCMA_DICOM {

///////////////////////////////////////////////////////////////////////////////
// DICOM Dictionary.
///////////////////////////////////////////////////////////////////////////////

const DICOMDictionary& get_default_dictionary(){
    // This dictionary covers tags used throughout DICOMautomaton for common modalities (CT, MR, RT Structure Sets,
    // dose, plans, etc.). It is seeded from the tags referenced in the DICOM write functions and common read tags.
    // A full DICOM dictionary can be imported via read_dictionary() if needed.
    //
    // Each entry is verified against the published DICOM standard (PS3.6).
    // VR, VM, keyword, and retirement status are all recorded.
    static const DICOMDictionary dict = {
        // File Meta Information (group 0x0002).
        {{0x0002, 0x0000}, {"UL", "FileMetaInformationGroupLength", "1", false}},
        {{0x0002, 0x0001}, {"OB", "FileMetaInformationVersion", "1", false}},
        {{0x0002, 0x0002}, {"UI", "MediaStorageSOPClassUID", "1", false}},
        {{0x0002, 0x0003}, {"UI", "MediaStorageSOPInstanceUID", "1", false}},
        {{0x0002, 0x0010}, {"UI", "TransferSyntaxUID", "1", false}},
        {{0x0002, 0x0012}, {"UI", "ImplementationClassUID", "1", false}},
        {{0x0002, 0x0013}, {"SH", "ImplementationVersionName", "1", false}},
        {{0x0002, 0x0016}, {"AE", "SourceApplicationEntityTitle", "1", false}},

        // SOP Common, General Study, General Series, Patient (group 0x0008, 0x0010).
        {{0x0008, 0x0005}, {"CS", "SpecificCharacterSet", "1-n", false}},
        {{0x0008, 0x0008}, {"CS", "ImageType", "2-n", false}},
        {{0x0008, 0x0012}, {"DA", "InstanceCreationDate", "1", false}},
        {{0x0008, 0x0013}, {"TM", "InstanceCreationTime", "1", false}},
        {{0x0008, 0x0014}, {"UI", "InstanceCreatorUID", "1", false}},
        {{0x0008, 0x0016}, {"UI", "SOPClassUID", "1", false}},
        {{0x0008, 0x0018}, {"UI", "SOPInstanceUID", "1", false}},
        {{0x0008, 0x0020}, {"DA", "StudyDate", "1", false}},
        {{0x0008, 0x0021}, {"DA", "SeriesDate", "1", false}},
        {{0x0008, 0x0022}, {"DA", "AcquisitionDate", "1", false}},
        {{0x0008, 0x0023}, {"DA", "ContentDate", "1", false}},
        {{0x0008, 0x0030}, {"TM", "StudyTime", "1", false}},
        {{0x0008, 0x0031}, {"TM", "SeriesTime", "1", false}},
        {{0x0008, 0x0032}, {"TM", "AcquisitionTime", "1", false}},
        {{0x0008, 0x0033}, {"TM", "ContentTime", "1", false}},
        {{0x0008, 0x0050}, {"SH", "AccessionNumber", "1", false}},
        {{0x0008, 0x0060}, {"CS", "Modality", "1", false}},
        {{0x0008, 0x0070}, {"LO", "Manufacturer", "1", false}},
        {{0x0008, 0x0080}, {"LO", "InstitutionName", "1", false}},
        {{0x0008, 0x0090}, {"PN", "ReferringPhysicianName", "1", false}},
        {{0x0008, 0x0114}, {"ST", "CodingSchemeExternalID", "1", false}},
        {{0x0008, 0x1010}, {"SH", "StationName", "1", false}},
        {{0x0008, 0x1030}, {"LO", "StudyDescription", "1", false}},
        {{0x0008, 0x103E}, {"LO", "SeriesDescription", "1", false}},
        {{0x0008, 0x1040}, {"LO", "InstitutionalDepartmentName", "1", false}},
        {{0x0008, 0x1070}, {"PN", "OperatorsName", "1-n", false}},
        {{0x0008, 0x1090}, {"LO", "ManufacturerModelName", "1", false}},
        {{0x0008, 0x1150}, {"UI", "ReferencedSOPClassUID", "1", false}},
        {{0x0008, 0x1155}, {"UI", "ReferencedSOPInstanceUID", "1", false}},

        // Patient (group 0x0010).
        {{0x0010, 0x0010}, {"PN", "PatientName", "1", false}},
        {{0x0010, 0x0020}, {"LO", "PatientID", "1", false}},
        {{0x0010, 0x0030}, {"DA", "PatientBirthDate", "1", false}},
        {{0x0010, 0x0032}, {"TM", "PatientBirthTime", "1", false}},
        {{0x0010, 0x0040}, {"CS", "PatientSex", "1", false}},
        {{0x0010, 0x1010}, {"AS", "PatientAge", "1", false}},
        {{0x0010, 0x1020}, {"DS", "PatientSize", "1", false}},
        {{0x0010, 0x1030}, {"DS", "PatientWeight", "1", false}},

        // Acquisition parameters (group 0x0018).
        {{0x0018, 0x0015}, {"CS", "BodyPartExamined", "1", false}},
        {{0x0018, 0x0020}, {"CS", "ScanningSequence", "1-n", false}},
        {{0x0018, 0x0021}, {"CS", "SequenceVariant", "1-n", false}},
        {{0x0018, 0x0022}, {"CS", "ScanOptions", "1-n", false}},
        {{0x0018, 0x0023}, {"CS", "MRAcquisitionType", "1", false}},
        {{0x0018, 0x0024}, {"SH", "SequenceName", "1", false}},
        {{0x0018, 0x0025}, {"CS", "AngioFlag", "1", false}},
        {{0x0018, 0x0050}, {"DS", "SliceThickness", "1", false}},
        {{0x0018, 0x0060}, {"DS", "KVP", "1", false}},
        {{0x0018, 0x0080}, {"DS", "RepetitionTime", "1", false}},
        {{0x0018, 0x0081}, {"DS", "EchoTime", "1", false}},
        {{0x0018, 0x0082}, {"DS", "InversionTime", "1", false}},
        {{0x0018, 0x0083}, {"DS", "NumberOfAverages", "1", false}},
        {{0x0018, 0x0084}, {"DS", "ImagingFrequency", "1", false}},
        {{0x0018, 0x0085}, {"SH", "ImagedNucleus", "1", false}},
        {{0x0018, 0x0086}, {"IS", "EchoNumbers", "1-n", false}},
        {{0x0018, 0x0087}, {"DS", "MagneticFieldStrength", "1", false}},
        {{0x0018, 0x0088}, {"DS", "SpacingBetweenSlices", "1", false}},
        {{0x0018, 0x0089}, {"IS", "NumberOfPhaseEncodingSteps", "1", false}},
        {{0x0018, 0x0090}, {"DS", "DataCollectionDiameter", "1", false}},
        {{0x0018, 0x0091}, {"IS", "EchoTrainLength", "1", false}},
        {{0x0018, 0x0093}, {"DS", "PercentSampling", "1", false}},
        {{0x0018, 0x0094}, {"DS", "PercentPhaseFieldOfView", "1", false}},
        {{0x0018, 0x0095}, {"DS", "PixelBandwidth", "1", false}},
        {{0x0018, 0x1000}, {"LO", "DeviceSerialNumber", "1", false}},
        {{0x0018, 0x1020}, {"LO", "SoftwareVersions", "1-n", false}},
        {{0x0018, 0x1030}, {"LO", "ProtocolName", "1", false}},
        {{0x0018, 0x1060}, {"DS", "TriggerTime", "1", false}},
        {{0x0018, 0x5100}, {"CS", "PatientPosition", "1", false}},
        {{0x0018, 0x9073}, {"FD", "AcquisitionDuration", "1", false}},
        {{0x0018, 0x9075}, {"CS", "DiffusionDirectionality", "1", false}},
        {{0x0018, 0x9076}, {"SQ", "DiffusionGradientDirectionSequence", "1", false}},
        {{0x0018, 0x9087}, {"FD", "DiffusionBValue", "1", false}},
        {{0x0018, 0x9089}, {"FD", "DiffusionGradientOrientation", "3", false}},
        {{0x0018, 0x9117}, {"SQ", "MRDiffusionSequence", "1", false}},
        {{0x0018, 0x9147}, {"CS", "DiffusionAnisotropyType", "1", false}},
        {{0x0018, 0x9601}, {"SQ", "DiffusionBMatrixSequence", "1", false}},
        {{0x0018, 0x9602}, {"FD", "DiffusionBValueXX", "1", false}},
        {{0x0018, 0x9603}, {"FD", "DiffusionBValueXY", "1", false}},
        {{0x0018, 0x9604}, {"FD", "DiffusionBValueXZ", "1", false}},
        {{0x0018, 0x9605}, {"FD", "DiffusionBValueYY", "1", false}},
        {{0x0018, 0x9606}, {"FD", "DiffusionBValueYZ", "1", false}},
        {{0x0018, 0x9607}, {"FD", "DiffusionBValueZZ", "1", false}},

        // Frame of Reference, General Image (group 0x0020).
        {{0x0020, 0x000D}, {"UI", "StudyInstanceUID", "1", false}},
        {{0x0020, 0x000E}, {"UI", "SeriesInstanceUID", "1", false}},
        {{0x0020, 0x0010}, {"SH", "StudyID", "1", false}},
        {{0x0020, 0x0011}, {"IS", "SeriesNumber", "1", false}},
        {{0x0020, 0x0012}, {"IS", "AcquisitionNumber", "1", false}},
        {{0x0020, 0x0013}, {"IS", "InstanceNumber", "1", false}},
        {{0x0020, 0x0020}, {"CS", "PatientOrientation", "2", false}},
        {{0x0020, 0x0032}, {"DS", "ImagePositionPatient", "3", false}},
        {{0x0020, 0x0037}, {"DS", "ImageOrientationPatient", "6", false}},
        {{0x0020, 0x0052}, {"UI", "FrameOfReferenceUID", "1", false}},
        {{0x0020, 0x0060}, {"CS", "Laterality", "1", false}},
        {{0x0020, 0x1040}, {"LO", "PositionReferenceIndicator", "1", false}},
        {{0x0020, 0x1041}, {"DS", "SliceLocation", "1", false}},

        // Image Pixel (group 0x0028).
        {{0x0028, 0x0002}, {"US", "SamplesPerPixel", "1", false}},
        {{0x0028, 0x0004}, {"CS", "PhotometricInterpretation", "1", false}},
        {{0x0028, 0x0006}, {"US", "PlanarConfiguration", "1", false}},
        {{0x0028, 0x0008}, {"IS", "NumberOfFrames", "1", false}},
        {{0x0028, 0x0010}, {"US", "Rows", "1", false}},
        {{0x0028, 0x0011}, {"US", "Columns", "1", false}},
        {{0x0028, 0x0030}, {"DS", "PixelSpacing", "2", false}},
        {{0x0028, 0x0100}, {"US", "BitsAllocated", "1", false}},
        {{0x0028, 0x0101}, {"US", "BitsStored", "1", false}},
        {{0x0028, 0x0102}, {"US", "HighBit", "1", false}},
        {{0x0028, 0x0103}, {"US", "PixelRepresentation", "1", false}},
        {{0x0028, 0x1050}, {"DS", "WindowCenter", "1-n", false}},
        {{0x0028, 0x1051}, {"DS", "WindowWidth", "1-n", false}},
        {{0x0028, 0x1052}, {"DS", "RescaleIntercept", "1", false}},
        {{0x0028, 0x1053}, {"DS", "RescaleSlope", "1", false}},
        {{0x0028, 0x1054}, {"LO", "RescaleType", "1", false}},

        // RT Dose (group 0x3004).
        {{0x3004, 0x0002}, {"CS", "DoseType", "1", false}},
        {{0x3004, 0x0004}, {"CS", "DoseUnits", "1", false}},
        {{0x3004, 0x000A}, {"CS", "DoseSummationType", "1", false}},
        {{0x3004, 0x000C}, {"DS", "GridFrameOffsetVector", "2-n", false}},
        {{0x3004, 0x000E}, {"DS", "DoseGridScaling", "1", false}},

        // RT Structure Set (group 0x3006).
        {{0x3006, 0x0002}, {"SH", "StructureSetLabel", "1", false}},
        {{0x3006, 0x0004}, {"LO", "StructureSetName", "1", false}},
        {{0x3006, 0x0006}, {"ST", "StructureSetDescription", "1", false}},
        {{0x3006, 0x0008}, {"DA", "StructureSetDate", "1", false}},
        {{0x3006, 0x0009}, {"TM", "StructureSetTime", "1", false}},
        {{0x3006, 0x0010}, {"SQ", "ReferencedFrameOfReferenceSequence", "1", false}},
        {{0x3006, 0x0012}, {"SQ", "RTReferencedStudySequence", "1", false}},
        {{0x3006, 0x0014}, {"SQ", "RTReferencedSeriesSequence", "1", false}},
        {{0x3006, 0x0016}, {"SQ", "ContourImageSequence", "1", false}},
        {{0x3006, 0x0020}, {"SQ", "StructureSetROISequence", "1", false}},
        {{0x3006, 0x0022}, {"IS", "ROINumber", "1", false}},
        {{0x3006, 0x0024}, {"UI", "ReferencedFrameOfReferenceUID", "1", false}},
        {{0x3006, 0x0026}, {"LO", "ROIName", "1", false}},
        {{0x3006, 0x0028}, {"ST", "ROIDescription", "1", false}},
        {{0x3006, 0x002A}, {"IS", "ROIDisplayColor", "3", false}},
        {{0x3006, 0x0036}, {"CS", "ROIGenerationAlgorithm", "1", false}},
        {{0x3006, 0x0038}, {"LO", "ROIGenerationDescription", "1", false}},
        {{0x3006, 0x0039}, {"SQ", "ROIContourSequence", "1", false}},
        {{0x3006, 0x0040}, {"SQ", "ContourSequence", "1", false}},
        {{0x3006, 0x0042}, {"CS", "ContourGeometricType", "1", false}},
        {{0x3006, 0x0044}, {"DS", "ContourSlabThickness", "1", false}},
        {{0x3006, 0x0045}, {"DS", "ContourOffsetVector", "3", false}},
        {{0x3006, 0x0046}, {"IS", "NumberOfContourPoints", "1", false}},
        {{0x3006, 0x0048}, {"IS", "ContourNumber", "1", false}},
        {{0x3006, 0x0049}, {"IS", "AttachedContours", "1-n", true}},
        {{0x3006, 0x0050}, {"DS", "ContourData", "3-3n", false}},
        {{0x3006, 0x0080}, {"SQ", "RTROIObservationsSequence", "1", false}},
        {{0x3006, 0x0082}, {"IS", "ObservationNumber", "1", false}},
        {{0x3006, 0x0084}, {"IS", "ReferencedROINumber", "1", false}},
        {{0x3006, 0x00A4}, {"CS", "RTROIInterpretedType", "1", false}},
        {{0x3006, 0x00A6}, {"PN", "ROIInterpreter", "1", false}},

        // RT Plan (group 0x300A).
        {{0x300A, 0x0002}, {"SH", "RTPlanLabel", "1", false}},
        {{0x300A, 0x0003}, {"LO", "RTPlanName", "1", false}},
        {{0x300A, 0x0006}, {"DA", "RTPlanDate", "1", false}},
        {{0x300A, 0x0007}, {"TM", "RTPlanTime", "1", false}},
        {{0x300A, 0x000C}, {"CS", "RTPlanGeometry", "1", false}},

        // Approval (group 0x300E).
        {{0x300E, 0x0002}, {"CS", "ApprovalStatus", "1", false}},
        {{0x300E, 0x0004}, {"DA", "ReviewDate", "1", false}},
        {{0x300E, 0x0005}, {"TM", "ReviewTime", "1", false}},
        {{0x300E, 0x0008}, {"PN", "ReviewerName", "1", false}},

        // Pixel Data.
        {{0x7FE0, 0x0010}, {"OW", "PixelData", "1", false}},
    };
    return dict;
}


// Helper: returns true if the token looks like a DICOM value multiplicity string
// (e.g., "1", "1-n", "2", "3-3n", "2-2n").
static bool looks_like_VM(const std::string &s){
    if(s.empty()) return false;
    // A VM string starts with a digit and contains only digits, '-', and 'n'.
    if(!std::isdigit(static_cast<unsigned char>(s[0]))) return false;
    for(const auto c : s){
        if(!std::isdigit(static_cast<unsigned char>(c)) && c != '-' && c != 'n') return false;
    }
    return true;
}


DICOMDictionary read_dictionary(std::istream &is){
    DICOMDictionary dict;
    std::string line;
    while(std::getline(is, line)){
        // Skip comments and blank lines.
        if(line.empty()) continue;
        if(line.front() == '#') continue;

        // Expected formats:
        //   New:    "GGGG,EEEE VR VM Keyword [RETIRED]"
        //   Legacy: "GGGG,EEEE VR Keyword"
        // At minimum: "GGGG,EEEE VR" which is 12 characters.
        if(line.size() < 12) continue;

        uint16_t group = 0;
        uint16_t element = 0;
        try{
            group   = static_cast<uint16_t>(std::stoul(line.substr(0, 4), nullptr, 16));
            element = static_cast<uint16_t>(std::stoul(line.substr(5, 4), nullptr, 16));
        }catch(const std::exception &){
            continue; // Skip malformed lines.
        }

        if(line.at(4) != ',') continue;
        if(line.at(9) != ' ') continue;

        // Parse the remaining tokens after position 10.
        std::istringstream iss(line.substr(10));
        std::string vr;
        if(!(iss >> vr)) continue;
        if(vr.size() != 2) continue;

        std::string vm;
        std::string keyword;
        bool retired = false;

        std::string token;
        if(iss >> token){
            if(looks_like_VM(token)){
                // New format: VR VM [Keyword] [RETIRED].
                vm = token;
                if(iss >> token){
                    if(token == "RETIRED"){
                        retired = true;
                    }else{
                        keyword = token;
                        if(iss >> token && token == "RETIRED"){
                            retired = true;
                        }
                    }
                }
            }else if(token == "RETIRED"){
                retired = true;
            }else{
                // Legacy format: VR Keyword [RETIRED].
                keyword = token;
                if(iss >> token && token == "RETIRED"){
                    retired = true;
                }
            }
        }

        dict[{group, element}] = {vr, keyword, vm, retired};
    }
    return dict;
}


void write_dictionary(std::ostream &os, const DICOMDictionary &dict){
    // Save and restore stream formatting state so subsequent writes to the same
    // stream are not affected by the hex/uppercase/fill settings used here.
    const auto prev_flags = os.flags();
    const auto prev_fill  = os.fill();

    os << "# DICOM Dictionary\n";
    os << "# Format: GGGG,EEEE VR VM Keyword [RETIRED]\n";
    for(const auto &[key, entry] : dict){
        os << std::hex << std::setfill('0') << std::uppercase
           << std::setw(4) << key.first << ","
           << std::setw(4) << key.second
           << std::dec << " " << entry.VR;
        if(!entry.VM.empty()){
            os << " " << entry.VM;
        }
        if(!entry.keyword.empty()){
            os << " " << entry.keyword;
        }
        if(entry.retired){
            os << " RETIRED";
        }
        os << "\n";
    }

    os.flags(prev_flags);
    os.fill(prev_fill);
}


std::string lookup_VR(uint16_t group, uint16_t element,
                      const std::vector<const DICOMDictionary*> &dicts){
    // Search additional dictionaries in reverse order (later entries take precedence).
    for(auto it = dicts.rbegin(); it != dicts.rend(); ++it){
        if(*it == nullptr) continue;
        auto entry = (*it)->find({group, element});
        if(entry != (*it)->end()) return entry->second.VR;
    }

    // Consult the built-in default dictionary.
    const auto &def = get_default_dictionary();
    auto entry = def.find({group, element});
    if(entry != def.end()) return entry->second.VR;

    // Special case: group length tags always have VR "UL".
    if(element == 0x0000) return "UL";

    return ""; // Unknown.
}


std::string lookup_VM(uint16_t group, uint16_t element,
                      const std::vector<const DICOMDictionary*> &dicts){
    for(auto it = dicts.rbegin(); it != dicts.rend(); ++it){
        if(*it == nullptr) continue;
        auto entry = (*it)->find({group, element});
        if(entry != (*it)->end() && !entry->second.VM.empty()) return entry->second.VM;
    }

    const auto &def = get_default_dictionary();
    auto entry = def.find({group, element});
    if(entry != def.end()) return entry->second.VM;

    // Special case: group length tags always have VM "1".
    if(element == 0x0000) return "1";

    return "";
}


std::string lookup_keyword(uint16_t group, uint16_t element,
                           const std::vector<const DICOMDictionary*> &dicts){
    for(auto it = dicts.rbegin(); it != dicts.rend(); ++it){
        if(*it == nullptr) continue;
        auto entry = (*it)->find({group, element});
        if(entry != (*it)->end() && !entry->second.keyword.empty()) return entry->second.keyword;
    }

    const auto &def = get_default_dictionary();
    auto entry = def.find({group, element});
    if(entry != def.end()) return entry->second.keyword;

    return "";
}

} // namespace DCMA_DICOM

