// DCMA_DICOM_RTPLAN.cc - A part of DICOMautomaton 2026. Written by hal clark.
//
// This file contains routines for extracting RT Plan data from a parsed DICOM Node tree.
//
// References:
//   - DICOM PS3.3 2026b, Section A.20.3: RT Plan IOD Module Table.
//   - DICOM PS3.3 2026b, Section C.8.8.9: RT General Plan Module.
//   - DICOM PS3.3 2026b, Section C.8.8.14: RT Beams Module.

#include <cstdint>
#include <cstring>
#include <cmath>
#include <limits>
#include <string>
#include <vector>
#include <map>
#include <optional>
#include <stdexcept>
#include <algorithm>
#include <memory>

#include "YgorLog.h"
#include "YgorMath.h"
#include "YgorString.h"

#include "DCMA_DICOM.h"
#include "DCMA_DICOM_RTPLAN.h"
#include "Structs.h"
#include "Metadata.h"


namespace DCMA_DICOM {

// ============================================================================
// Internal helper functions.
// ============================================================================

// Strip trailing null and space padding from a DICOM value string.
static std::string strip_padding(const std::string &s){
    std::string out = s;
    while(!out.empty() && (out.back() == '\0' || out.back() == ' ')) out.pop_back();
    return out;
}

// Read a text-valued tag from the tree and strip padding.
static std::string read_text_tag(const Node &root, uint16_t group, uint16_t tag){
    const auto *n = root.find(group, tag);
    if(n == nullptr) return "";
    return strip_padding(n->val);
}

// Read a text-valued tag from an item's direct children (shallow search).
static std::string read_text_shallow(const Node &item, uint16_t group, uint16_t tag){
    for(const auto &child : item.children){
        if(child.key.group == group && child.key.tag == tag){
            return strip_padding(child.val);
        }
    }
    return "";
}

// Find a child node with the given group/tag (shallow search).
static const Node* find_child_shallow(const Node &item, uint16_t group, uint16_t tag){
    for(const auto &child : item.children){
        if(child.key.group == group && child.key.tag == tag){
            return &child;
        }
    }
    return nullptr;
}

// Read a DS (Decimal String) VR value and parse as a single double.
static std::optional<double> read_DS(const std::string &val){
    std::string s = strip_padding(val);
    if(s.empty()) return std::nullopt;

    // Trim whitespace.
    while(!s.empty() && s.front() == ' ') s.erase(s.begin());
    while(!s.empty() && s.back() == ' ') s.pop_back();
    if(s.empty()) return std::nullopt;

    try{
        return std::stod(s);
    }catch(const std::exception &){
        return std::nullopt;
    }
}

// Read an IS (Integer String) VR value as int64_t.
static std::optional<int64_t> read_IS(const std::string &val){
    std::string s = strip_padding(val);
    if(s.empty()) return std::nullopt;

    try{
        return std::stol(s);
    }catch(const std::exception &){
        return std::nullopt;
    }
}

// Read a DS vector from a backslash-separated value.
static std::vector<double> read_DS_vector(const std::string &val){
    std::vector<double> out;
    const auto s = strip_padding(val);
    if(s.empty()) return out;

    auto tokens = SplitStringToVector(s, '\\', 'd');
    for(const auto &tok : tokens){
        auto d = read_DS(tok);
        if(d.has_value()){
            out.push_back(d.value());
        }
    }
    return out;
}

// Read a vec3<double> from a DS tag with 3 values.
static std::optional<vec3<double>> read_DS_vec3(const std::string &val){
    auto v = read_DS_vector(val);
    if(v.size() < 3) return std::nullopt;
    return vec3<double>(v[0], v[1], v[2]);
}

// Insert a metadata value if it is non-empty.
static void insert_if_nonempty(metadata_map_t &m, const std::string &key, const std::string &val){
    const auto trimmed = strip_padding(val);
    if(!trimmed.empty()){
        m[key] = trimmed;
    }
}

// Convert rotation direction string to numeric value.
// DICOM PS3.3 2026b, C.8.8.14.8: "NONE", "CW", "CCW".
// Returns: 0.0 for NONE, -1.0 for CW, +1.0 for CCW, NaN otherwise.
static double parse_rotation_direction(const std::string &s){
    const auto ctrim = CANONICALIZE::TRIM_ENDS | CANONICALIZE::TO_UPPER;
    const auto trimmed = Canonicalize_String2(s, ctrim);
    const double nan = std::numeric_limits<double>::quiet_NaN();

    if(trimmed == "NONE"){
        return 0.0;
    }else if(trimmed == "CCW"){
        return 1.0;
    }else if(trimmed == "CW"){
        return -1.0;
    }
    return nan;
}


// ============================================================================
// RT Plan extraction.
// ============================================================================

// Extract common top-level metadata.
static metadata_map_t extract_plan_metadata(const Node &root){
    metadata_map_t m;

    // Patient module (DICOM PS3.3, C.7.1.1).
    insert_if_nonempty(m, "PatientName", read_text_tag(root, 0x0010, 0x0010));
    insert_if_nonempty(m, "PatientID", read_text_tag(root, 0x0010, 0x0020));
    insert_if_nonempty(m, "PatientBirthDate", read_text_tag(root, 0x0010, 0x0030));
    insert_if_nonempty(m, "PatientSex", read_text_tag(root, 0x0010, 0x0040));

    // General Study module (DICOM PS3.3, C.7.2.1).
    insert_if_nonempty(m, "StudyInstanceUID", read_text_tag(root, 0x0020, 0x000D));
    insert_if_nonempty(m, "StudyDate", read_text_tag(root, 0x0008, 0x0020));
    insert_if_nonempty(m, "StudyTime", read_text_tag(root, 0x0008, 0x0030));
    insert_if_nonempty(m, "StudyDescription", read_text_tag(root, 0x0008, 0x1030));
    insert_if_nonempty(m, "StudyID", read_text_tag(root, 0x0020, 0x0010));

    // General Series module (DICOM PS3.3, C.7.3.1).
    insert_if_nonempty(m, "SeriesInstanceUID", read_text_tag(root, 0x0020, 0x000E));
    insert_if_nonempty(m, "SeriesDate", read_text_tag(root, 0x0008, 0x0021));
    insert_if_nonempty(m, "SeriesTime", read_text_tag(root, 0x0008, 0x0031));
    insert_if_nonempty(m, "SeriesDescription", read_text_tag(root, 0x0008, 0x103E));
    insert_if_nonempty(m, "SeriesNumber", read_text_tag(root, 0x0020, 0x0011));
    insert_if_nonempty(m, "Modality", read_text_tag(root, 0x0008, 0x0060));

    // SOP Common module (DICOM PS3.3, C.12.1).
    insert_if_nonempty(m, "SOPClassUID", read_text_tag(root, 0x0008, 0x0016));
    insert_if_nonempty(m, "SOPInstanceUID", read_text_tag(root, 0x0008, 0x0018));

    // Frame of Reference module (DICOM PS3.3, C.7.4.1).
    insert_if_nonempty(m, "FrameOfReferenceUID", read_text_tag(root, 0x0020, 0x0052));

    // RT General Plan Module (DICOM PS3.3 2026b, C.8.8.9).
    insert_if_nonempty(m, "RTPlanLabel", read_text_tag(root, 0x300A, 0x0002));
    insert_if_nonempty(m, "RTPlanName", read_text_tag(root, 0x300A, 0x0003));
    insert_if_nonempty(m, "RTPlanDescription", read_text_tag(root, 0x300A, 0x0004));
    insert_if_nonempty(m, "RTPlanDate", read_text_tag(root, 0x300A, 0x0006));
    insert_if_nonempty(m, "RTPlanTime", read_text_tag(root, 0x300A, 0x0007));
    insert_if_nonempty(m, "RTPlanGeometry", read_text_tag(root, 0x300A, 0x000C));

    // Manufacturer info.
    insert_if_nonempty(m, "Manufacturer", read_text_tag(root, 0x0008, 0x0070));
    insert_if_nonempty(m, "InstitutionName", read_text_tag(root, 0x0008, 0x0080));
    insert_if_nonempty(m, "StationName", read_text_tag(root, 0x0008, 0x1010));

    return m;
}


std::unique_ptr<RTPlan> extract_rtplan(const Node &root){
    auto out = std::make_unique<RTPlan>();
    const double nan = std::numeric_limits<double>::quiet_NaN();

    // Verify modality is RTPLAN.
    const auto modality = read_text_tag(root, 0x0008, 0x0060);
    if(modality != "RTPLAN"){
        YLOGWARN("Expected modality RTPLAN but found '" << modality << "'");
        return nullptr;
    }

    // Extract top-level metadata.
    out->metadata = extract_plan_metadata(root);

    // -------------------------------------------------------------------------
    // DoseReferenceSequence (300A,0010).
    // DICOM PS3.3 2026b, C.8.8.10: Dose Reference Module.
    // Each item describes a dose reference point or volume.
    // -------------------------------------------------------------------------
    const auto *dose_ref_seq = root.find(0x300A, 0x0010);
    if(dose_ref_seq != nullptr){
        int i = 0;
        for(const auto &item : dose_ref_seq->children){
            const std::string prfx = "DoseReferenceSequence" + std::to_string(i) + "/";

            insert_if_nonempty(out->metadata, prfx + "DoseReferenceNumber",
                               read_text_shallow(item, 0x300A, 0x0012));
            insert_if_nonempty(out->metadata, prfx + "DoseReferenceUID",
                               read_text_shallow(item, 0x300A, 0x0013));
            insert_if_nonempty(out->metadata, prfx + "DoseReferenceStructureType",
                               read_text_shallow(item, 0x300A, 0x0014));
            insert_if_nonempty(out->metadata, prfx + "DoseReferenceDescription",
                               read_text_shallow(item, 0x300A, 0x0016));
            insert_if_nonempty(out->metadata, prfx + "DoseReferenceType",
                               read_text_shallow(item, 0x300A, 0x0020));
            insert_if_nonempty(out->metadata, prfx + "TargetPrescriptionDose",
                               read_text_shallow(item, 0x300A, 0x0026));
            ++i;
        }
    }

    // -------------------------------------------------------------------------
    // ReferencedStructureSetSequence (300C,0060).
    // DICOM PS3.3 2026b, C.8.8.9: RT General Plan Module.
    // Links plan to structure set(s).
    // -------------------------------------------------------------------------
    const auto *ref_ss_seq = root.find(0x300C, 0x0060);
    if(ref_ss_seq != nullptr){
        int i = 0;
        for(const auto &item : ref_ss_seq->children){
            const std::string prfx = "ReferencedStructureSetSequence" + std::to_string(i) + "/";

            insert_if_nonempty(out->metadata, prfx + "ReferencedSOPClassUID",
                               read_text_shallow(item, 0x0008, 0x1150));
            insert_if_nonempty(out->metadata, prfx + "ReferencedSOPInstanceUID",
                               read_text_shallow(item, 0x0008, 0x1155));
            ++i;
        }
    }

    // -------------------------------------------------------------------------
    // ToleranceTableSequence (300A,0040).
    // DICOM PS3.3 2026b, C.8.8.12: RT Tolerance Tables Module.
    // Defines tolerance values for machine parameters.
    // -------------------------------------------------------------------------
    const auto *tol_seq = root.find(0x300A, 0x0040);
    if(tol_seq != nullptr){
        int i = 0;
        for(const auto &item : tol_seq->children){
            const std::string prfx = "ToleranceTableSequence" + std::to_string(i) + "/";

            insert_if_nonempty(out->metadata, prfx + "ToleranceTableNumber",
                               read_text_shallow(item, 0x300A, 0x0042));
            insert_if_nonempty(out->metadata, prfx + "ToleranceTableLabel",
                               read_text_shallow(item, 0x300A, 0x0043));
            insert_if_nonempty(out->metadata, prfx + "GantryAngleTolerance",
                               read_text_shallow(item, 0x300A, 0x0044));
            insert_if_nonempty(out->metadata, prfx + "BeamLimitingDeviceAngleTolerance",
                               read_text_shallow(item, 0x300A, 0x0046));
            insert_if_nonempty(out->metadata, prfx + "PatientSupportAngleTolerance",
                               read_text_shallow(item, 0x300A, 0x004C));
            insert_if_nonempty(out->metadata, prfx + "TableTopVerticalPositionTolerance",
                               read_text_shallow(item, 0x300A, 0x0051));
            insert_if_nonempty(out->metadata, prfx + "TableTopLongitudinalPositionTolerance",
                               read_text_shallow(item, 0x300A, 0x0052));
            insert_if_nonempty(out->metadata, prfx + "TableTopLateralPositionTolerance",
                               read_text_shallow(item, 0x300A, 0x0053));

            // BeamLimitingDeviceToleranceSequence (300A,0048).
            const auto *bld_tol_seq = find_child_shallow(item, 0x300A, 0x0048);
            if(bld_tol_seq != nullptr){
                int j = 0;
                for(const auto &bld_item : bld_tol_seq->children){
                    const std::string pprfx = prfx + "BeamLimitingDeviceToleranceSequence" + std::to_string(j) + "/";

                    insert_if_nonempty(out->metadata, pprfx + "BeamLimitingDevicePositionTolerance",
                                       read_text_shallow(bld_item, 0x300A, 0x004A));
                    insert_if_nonempty(out->metadata, pprfx + "RTBeamLimitingDeviceType",
                                       read_text_shallow(bld_item, 0x300A, 0x00B8));
                    ++j;
                }
            }
            ++i;
        }
    }

    // -------------------------------------------------------------------------
    // FractionGroupSequence (300A,0070).
    // DICOM PS3.3 2026b, C.8.8.13: RT Fraction Scheme Module.
    // Describes fractionation scheme and beam references.
    // -------------------------------------------------------------------------
    const auto *frac_seq = root.find(0x300A, 0x0070);
    if(frac_seq != nullptr){
        int i = 0;
        for(const auto &item : frac_seq->children){
            const std::string prfx = "FractionGroupSequence" + std::to_string(i) + "/";

            insert_if_nonempty(out->metadata, prfx + "FractionGroupNumber",
                               read_text_shallow(item, 0x300A, 0x0071));
            insert_if_nonempty(out->metadata, prfx + "NumberOfFractionsPlanned",
                               read_text_shallow(item, 0x300A, 0x0078));
            insert_if_nonempty(out->metadata, prfx + "NumberOfBeams",
                               read_text_shallow(item, 0x300A, 0x0080));
            insert_if_nonempty(out->metadata, prfx + "NumberOfBrachyApplicationSetups",
                               read_text_shallow(item, 0x300A, 0x00A0));

            // ReferencedBeamSequence (300C,0004).
            const auto *ref_beam_seq = find_child_shallow(item, 0x300C, 0x0004);
            if(ref_beam_seq != nullptr){
                int j = 0;
                for(const auto &rb_item : ref_beam_seq->children){
                    const std::string pprfx = prfx + "ReferencedBeamSequence" + std::to_string(j) + "/";

                    insert_if_nonempty(out->metadata, pprfx + "BeamDose",
                                       read_text_shallow(rb_item, 0x300A, 0x0084));
                    insert_if_nonempty(out->metadata, pprfx + "BeamMeterset",
                                       read_text_shallow(rb_item, 0x300A, 0x0086));
                    insert_if_nonempty(out->metadata, pprfx + "ReferencedBeamNumber",
                                       read_text_shallow(rb_item, 0x300C, 0x0006));
                    ++j;
                }
            }
            ++i;
        }
    }

    // -------------------------------------------------------------------------
    // BeamSequence (300A,00B0).
    // DICOM PS3.3 2026b, C.8.8.14: RT Beams Module.
    // Each item describes a treatment beam → Dynamic_Machine_State.
    // -------------------------------------------------------------------------
    const auto *beam_seq = root.find(0x300A, 0x00B0);
    if(beam_seq == nullptr){
        YLOGWARN("BeamSequence (300A,00B0) not found in RTPLAN");
        return nullptr;
    }

    for(const auto &beam_item : beam_seq->children){
        out->dynamic_states.emplace_back();
        Dynamic_Machine_State &dms = out->dynamic_states.back();

        // Beam-level metadata (DICOM PS3.3 2026b, C.8.8.14.1).
        insert_if_nonempty(dms.metadata, "Manufacturer",
                           read_text_shallow(beam_item, 0x0008, 0x0070));
        insert_if_nonempty(dms.metadata, "InstitutionName",
                           read_text_shallow(beam_item, 0x0008, 0x0080));
        insert_if_nonempty(dms.metadata, "InstitutionalDepartmentName",
                           read_text_shallow(beam_item, 0x0008, 0x1040));
        insert_if_nonempty(dms.metadata, "ManufacturerModelName",
                           read_text_shallow(beam_item, 0x0008, 0x1090));
        insert_if_nonempty(dms.metadata, "DeviceSerialNumber",
                           read_text_shallow(beam_item, 0x0018, 0x1000));
        insert_if_nonempty(dms.metadata, "TreatmentMachineName",
                           read_text_shallow(beam_item, 0x300A, 0x00B2));
        insert_if_nonempty(dms.metadata, "PrimaryDosimeterUnit",
                           read_text_shallow(beam_item, 0x300A, 0x00B3));
        insert_if_nonempty(dms.metadata, "SourceAxisDistance",
                           read_text_shallow(beam_item, 0x300A, 0x00B4));

        // Required beam fields.
        const auto beam_number_str = read_text_shallow(beam_item, 0x300A, 0x00C0);
        const auto beam_name_str = read_text_shallow(beam_item, 0x300A, 0x00C2);
        const auto final_cum_mw_str = read_text_shallow(beam_item, 0x300A, 0x010E);
        const auto num_cp_str = read_text_shallow(beam_item, 0x300A, 0x0110);

        insert_if_nonempty(dms.metadata, "BeamNumber", beam_number_str);
        insert_if_nonempty(dms.metadata, "BeamName", beam_name_str);
        insert_if_nonempty(dms.metadata, "BeamType",
                           read_text_shallow(beam_item, 0x300A, 0x00C4));
        insert_if_nonempty(dms.metadata, "RadiationType",
                           read_text_shallow(beam_item, 0x300A, 0x00C6));
        insert_if_nonempty(dms.metadata, "TreatmentDeliveryType",
                           read_text_shallow(beam_item, 0x300A, 0x00CE));
        insert_if_nonempty(dms.metadata, "NumberOfWedges",
                           read_text_shallow(beam_item, 0x300A, 0x00D0));
        insert_if_nonempty(dms.metadata, "NumberOfCompensators",
                           read_text_shallow(beam_item, 0x300A, 0x00E0));
        insert_if_nonempty(dms.metadata, "NumberOfBoli",
                           read_text_shallow(beam_item, 0x300A, 0x00ED));
        insert_if_nonempty(dms.metadata, "NumberOfBlocks",
                           read_text_shallow(beam_item, 0x300A, 0x00F0));
        insert_if_nonempty(dms.metadata, "FinalCumulativeMetersetWeight", final_cum_mw_str);
        insert_if_nonempty(dms.metadata, "NumberOfControlPoints", num_cp_str);
        insert_if_nonempty(dms.metadata, "ReferencedPatientSetupNumber",
                           read_text_shallow(beam_item, 0x300C, 0x006A));
        insert_if_nonempty(dms.metadata, "ReferencedToleranceTableNumber",
                           read_text_shallow(beam_item, 0x300C, 0x00A0));

        // Validate required fields.
        auto beam_number_opt = read_IS(beam_number_str);
        auto final_cum_mw_opt = read_DS(final_cum_mw_str);

        if(!beam_number_opt.has_value() || beam_name_str.empty() || !final_cum_mw_opt.has_value()){
            YLOGERR("RTPLAN beam is missing required data (BeamNumber, BeamName, or FinalCumulativeMetersetWeight). Refusing to continue.");
            throw std::runtime_error("RTPLAN is missing required data. Refusing to continue.");
        }

        dms.BeamNumber = beam_number_opt.value();
        dms.FinalCumulativeMetersetWeight = final_cum_mw_opt.value();

        // BeamLimitingDeviceSequence (300A,00B6).
        // DICOM PS3.3 2026b, C.8.8.14.3.
        const auto *bld_seq = find_child_shallow(beam_item, 0x300A, 0x00B6);
        if(bld_seq != nullptr){
            int j = 0;
            for(const auto &bld_item : bld_seq->children){
                const std::string prfx = "BeamLimitingDeviceSequence" + std::to_string(j) + "/";

                insert_if_nonempty(dms.metadata, prfx + "RTBeamLimitingDeviceType",
                                   read_text_shallow(bld_item, 0x300A, 0x00B8));
                insert_if_nonempty(dms.metadata, prfx + "NumberOfLeafJawPairs",
                                   read_text_shallow(bld_item, 0x300A, 0x00BC));
                insert_if_nonempty(dms.metadata, prfx + "LeafPositionBoundaries",
                                   read_text_shallow(bld_item, 0x300A, 0x00BE));
                ++j;
            }
        }

        // PrimaryFluenceModeSequence (3002,0050).
        // DICOM PS3.3 2026b, C.8.8.14.2.
        const auto *fluence_seq = find_child_shallow(beam_item, 0x3002, 0x0050);
        if(fluence_seq != nullptr){
            int j = 0;
            for(const auto &f_item : fluence_seq->children){
                const std::string prfx = "PrimaryFluenceModeSequence" + std::to_string(j) + "/";

                insert_if_nonempty(dms.metadata, prfx + "FluenceMode",
                                   read_text_shallow(f_item, 0x3002, 0x0051));
                insert_if_nonempty(dms.metadata, prfx + "FluenceModeID",
                                   read_text_shallow(f_item, 0x3002, 0x0052));
                ++j;
            }
        }

        // ---------------------------------------------------------------------
        // ControlPointSequence (300A,0111).
        // DICOM PS3.3 2026b, C.8.8.14.5.
        // Each item describes a control point → Static_Machine_State.
        // ---------------------------------------------------------------------
        const auto *cp_seq = find_child_shallow(beam_item, 0x300A, 0x0111);
        if(cp_seq == nullptr){
            YLOGWARN("ControlPointSequence (300A,0111) not found for beam " << dms.BeamNumber);
            continue;
        }

        for(const auto &cp_item : cp_seq->children){
            dms.static_states.emplace_back();
            Static_Machine_State &sms = dms.static_states.back();

            // Required control point fields.
            const auto cp_index_str = read_text_shallow(cp_item, 0x300A, 0x0112);
            const auto cum_mw_str = read_text_shallow(cp_item, 0x300A, 0x0134);

            auto cp_index_opt = read_IS(cp_index_str);
            auto cum_mw_opt = read_DS(cum_mw_str);

            if(!cp_index_opt.has_value() || !cum_mw_opt.has_value()){
                YLOGERR("RTPLAN has an invalid control point (missing ControlPointIndex or CumulativeMetersetWeight). Refusing to continue.");
                throw std::runtime_error("RTPLAN has an invalid control point. Refusing to continue.");
            }

            sms.ControlPointIndex = cp_index_opt.value();
            sms.CumulativeMetersetWeight = cum_mw_opt.value();

            // Optional control point metadata.
            const std::string cp_prfx = "ControlPointSequence" + std::to_string(sms.ControlPointIndex) + "/";
            insert_if_nonempty(sms.metadata, cp_prfx + "NominalBeamEnergy",
                               read_text_shallow(cp_item, 0x300A, 0x0114));
            insert_if_nonempty(sms.metadata, cp_prfx + "DoseRateSet",
                               read_text_shallow(cp_item, 0x300A, 0x0115));

            // Machine angles and positions.
            // DICOM PS3.3 2026b, C.8.8.14.5: If absent, value is unchanged from previous control point.
            const auto gantry_angle_str = read_text_shallow(cp_item, 0x300A, 0x011E);
            const auto gantry_rot_dir_str = read_text_shallow(cp_item, 0x300A, 0x011F);
            const auto bld_angle_str = read_text_shallow(cp_item, 0x300A, 0x0120);
            const auto bld_rot_dir_str = read_text_shallow(cp_item, 0x300A, 0x0121);
            const auto ps_angle_str = read_text_shallow(cp_item, 0x300A, 0x0122);
            const auto ps_rot_dir_str = read_text_shallow(cp_item, 0x300A, 0x0123);
            const auto tte_angle_str = read_text_shallow(cp_item, 0x300A, 0x0125);
            const auto tte_rot_dir_str = read_text_shallow(cp_item, 0x300A, 0x0126);
            const auto ttv_pos_str = read_text_shallow(cp_item, 0x300A, 0x0128);
            const auto ttl_pos_str = read_text_shallow(cp_item, 0x300A, 0x0129);
            const auto ttlat_pos_str = read_text_shallow(cp_item, 0x300A, 0x012A);
            const auto iso_pos_str = read_text_shallow(cp_item, 0x300A, 0x012C);
            const auto ttp_angle_str = read_text_shallow(cp_item, 0x300A, 0x0140);
            const auto ttp_rot_dir_str = read_text_shallow(cp_item, 0x300A, 0x0142);
            const auto ttr_angle_str = read_text_shallow(cp_item, 0x300A, 0x0144);
            const auto ttr_rot_dir_str = read_text_shallow(cp_item, 0x300A, 0x0146);

            sms.GantryAngle = read_DS(gantry_angle_str).value_or(nan);
            sms.GantryRotationDirection = parse_rotation_direction(gantry_rot_dir_str);

            sms.BeamLimitingDeviceAngle = read_DS(bld_angle_str).value_or(nan);
            sms.BeamLimitingDeviceRotationDirection = parse_rotation_direction(bld_rot_dir_str);

            sms.PatientSupportAngle = read_DS(ps_angle_str).value_or(nan);
            sms.PatientSupportRotationDirection = parse_rotation_direction(ps_rot_dir_str);

            sms.TableTopEccentricAngle = read_DS(tte_angle_str).value_or(nan);
            sms.TableTopEccentricRotationDirection = parse_rotation_direction(tte_rot_dir_str);

            sms.TableTopVerticalPosition = read_DS(ttv_pos_str).value_or(nan);
            sms.TableTopLongitudinalPosition = read_DS(ttl_pos_str).value_or(nan);
            sms.TableTopLateralPosition = read_DS(ttlat_pos_str).value_or(nan);

            sms.TableTopPitchAngle = read_DS(ttp_angle_str).value_or(nan);
            sms.TableTopPitchRotationDirection = parse_rotation_direction(ttp_rot_dir_str);

            sms.TableTopRollAngle = read_DS(ttr_angle_str).value_or(nan);
            sms.TableTopRollRotationDirection = parse_rotation_direction(ttr_rot_dir_str);

            sms.IsocentrePosition = read_DS_vec3(iso_pos_str).value_or(vec3<double>(nan, nan, nan));

            // BeamLimitingDevicePositionSequence (300A,011A).
            // DICOM PS3.3 2026b, C.8.8.14.5.
            const auto *bldp_seq = find_child_shallow(cp_item, 0x300A, 0x011A);
            if(bldp_seq != nullptr){
                for(const auto &bldp_item : bldp_seq->children){
                    const auto device_type = read_text_shallow(bldp_item, 0x300A, 0x00B8);
                    const auto leaf_jaw_pos_str = read_text_shallow(bldp_item, 0x300A, 0x011C);
                    const auto positions = read_DS_vector(leaf_jaw_pos_str);

                    if(device_type.empty() || positions.empty()){
                        YLOGERR("RTPLAN has an invalid beam limiting device position (missing type or positions). Refusing to continue.");
                        throw std::runtime_error("RTPLAN has an invalid beam limiting device position. Refusing to continue.");
                    }

                    // Canonicalize device type.
                    const auto ctrim = CANONICALIZE::TRIM_ENDS | CANONICALIZE::TO_UPPER;
                    const auto device_type_upper = Canonicalize_String2(device_type, ctrim);

                    if(device_type_upper == "ASYMX" || device_type_upper == "X"){
                        sms.JawPositionsX = positions;
                    }else if(device_type_upper == "ASYMY" || device_type_upper == "Y"){
                        sms.JawPositionsY = positions;
                    }else if(device_type_upper == "MLCX"){
                        sms.MLCPositionsX = positions;
                    }
                    // Note: MLCY not commonly used but could be added similarly.
                }
            }

            // ReferencedDoseReferenceSequence (300C,0050).
            const auto *ref_dose_seq = find_child_shallow(cp_item, 0x300C, 0x0050);
            if(ref_dose_seq != nullptr){
                int k = 0;
                for(const auto &rd_item : ref_dose_seq->children){
                    const std::string rprfx = cp_prfx + "ReferencedDoseReferenceSequence" + std::to_string(k) + "/";

                    insert_if_nonempty(sms.metadata, rprfx + "CumulativeDoseReferenceCoefficient",
                                       read_text_shallow(rd_item, 0x300A, 0x010C));
                    insert_if_nonempty(sms.metadata, rprfx + "ReferencedDoseReferenceNumber",
                                       read_text_shallow(rd_item, 0x300C, 0x0051));
                    ++k;
                }
            }
        }
    }

    // Order beams by beam number.
    std::sort(out->dynamic_states.begin(), out->dynamic_states.end(),
              [](const Dynamic_Machine_State &L, const Dynamic_Machine_State &R){
                  return L.BeamNumber < R.BeamNumber;
              });

    // Order control points within each beam.
    for(auto &dms : out->dynamic_states){
        dms.sort_states();
    }

    // Validate control point ordering.
    for(const auto &dms : out->dynamic_states){
        if(dms.static_states.size() < 2){
            YLOGERR("Insufficient number of control points (" << dms.static_states.size()
                    << ") for beam " << dms.BeamNumber << ". Refusing to continue.");
            throw std::runtime_error("Insufficient number of control points. Refusing to continue.");
        }
        if(!dms.verify_states_are_ordered()){
            YLOGERR("Control points are out of order or missing for beam " << dms.BeamNumber
                    << ". Refusing to continue.");
            throw std::runtime_error("A control point is missing. Refusing to continue.");
        }
    }

    // Propagate control point state to handle "unchanged" NaN values.
    for(auto &dms : out->dynamic_states){
        dms.normalize_states();
    }

    return out;
}


} // namespace DCMA_DICOM

