// DCMA_DICOM_Contours.cc - A part of DICOMautomaton 2026. Written by hal clark.
//
// This file contains routines for extracting RT Structure Set (contour) data from a parsed
// DICOM Node tree.
//
// References:
//   - DICOM PS3.3 2026b, Section A.19.3: RT Structure Set IOD Module Table.
//   - DICOM PS3.3 2026b, Section C.8.8.5: Structure Set Module.
//   - DICOM PS3.3 2026b, Section C.8.8.6: ROI Contour Module.
//   - DICOM PS3.3 2026b, Section C.8.8.8: RT ROI Observations Module.

#include <cstdint>
#include <cstring>
#include <cmath>
#include <limits>
#include <string>
#include <vector>
#include <map>
#include <tuple>
#include <optional>
#include <stdexcept>
#include <algorithm>
#include <memory>

#include "YgorLog.h"
#include "YgorMath.h"
#include "YgorString.h"

#include "DCMA_DICOM.h"
#include "DCMA_DICOM_Contours.h"
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
static std::string read_text(const Node &root, uint16_t group, uint16_t tag){
    const auto *n = root.find(group, tag);
    if(n == nullptr) return "";
    return strip_padding(n->val);
}

// Read a DS (Decimal String) VR value from a node and parse all values into a vector of doubles.
// DS values are backslash-separated per DICOM PS3.5, Section 6.2.
static std::vector<double> read_DS_vector(const Node &node){
    std::vector<double> out;
    const auto val = strip_padding(node.val);
    if(val.empty()) return out;

    // Split by backslash (DICOM value multiplicity separator).
    auto tokens = SplitStringToVector(val, '\\', 'd');
    for(const auto &tok : tokens){
        try{
            std::string t = tok;
            // Trim leading/trailing whitespace.
            while(!t.empty() && t.front() == ' ') t.erase(t.begin());
            while(!t.empty() && t.back() == ' ') t.pop_back();
            if(!t.empty()){
                out.push_back(std::stod(t));
            }
        }catch(const std::exception &){
            // Skip malformed values.
        }
    }
    return out;
}

// Read an IS (Integer String) VR value as int64_t.
static std::optional<int64_t> read_IS(const Node &node){
    const auto val = strip_padding(node.val);
    if(val.empty()) return std::nullopt;

    try{
        return std::stol(val);
    }catch(const std::exception &){
        return std::nullopt;
    }
}

// Insert a metadata value if it is non-empty.
static void insert_if_nonempty(metadata_map_t &m, const std::string &key, const std::string &val){
    const auto trimmed = strip_padding(val);
    if(!trimmed.empty()){
        m[key] = trimmed;
    }
}

// Insert or append a metadata value with a separator if already present.
static void insert_or_append(metadata_map_t &m, const std::string &key, const std::string &val){
    const auto trimmed = strip_padding(val);
    if(trimmed.empty()) return;
    auto &v = m[key];
    v += (v.empty()) ? trimmed : ", " + trimmed;
}


// ============================================================================
// RT Structure Set extraction.
// ============================================================================

metadata_map_t extract_common_metadata(const Node &root){
    metadata_map_t m;

    // Patient module (DICOM PS3.3, C.7.1.1).
    insert_if_nonempty(m, "PatientName", read_text(root, 0x0010, 0x0010));
    insert_if_nonempty(m, "PatientID", read_text(root, 0x0010, 0x0020));
    insert_if_nonempty(m, "PatientBirthDate", read_text(root, 0x0010, 0x0030));
    insert_if_nonempty(m, "PatientSex", read_text(root, 0x0010, 0x0040));

    // General Study module (DICOM PS3.3, C.7.2.1).
    insert_if_nonempty(m, "StudyInstanceUID", read_text(root, 0x0020, 0x000D));
    insert_if_nonempty(m, "StudyDate", read_text(root, 0x0008, 0x0020));
    insert_if_nonempty(m, "StudyTime", read_text(root, 0x0008, 0x0030));
    insert_if_nonempty(m, "StudyDescription", read_text(root, 0x0008, 0x1030));
    insert_if_nonempty(m, "StudyID", read_text(root, 0x0020, 0x0010));

    // General Series module (DICOM PS3.3, C.7.3.1).
    insert_if_nonempty(m, "SeriesInstanceUID", read_text(root, 0x0020, 0x000E));
    insert_if_nonempty(m, "SeriesDate", read_text(root, 0x0008, 0x0021));
    insert_if_nonempty(m, "SeriesTime", read_text(root, 0x0008, 0x0031));
    insert_if_nonempty(m, "SeriesDescription", read_text(root, 0x0008, 0x103E));
    insert_if_nonempty(m, "SeriesNumber", read_text(root, 0x0020, 0x0011));
    insert_if_nonempty(m, "Modality", read_text(root, 0x0008, 0x0060));

    // SOP Common module (DICOM PS3.3, C.12.1).
    insert_if_nonempty(m, "SOPClassUID", read_text(root, 0x0008, 0x0016));
    insert_if_nonempty(m, "SOPInstanceUID", read_text(root, 0x0008, 0x0018));
    insert_if_nonempty(m, "InstanceNumber", read_text(root, 0x0020, 0x0013));

    // Frame of Reference module (DICOM PS3.3, C.7.4.1).
    insert_if_nonempty(m, "FrameOfReferenceUID", read_text(root, 0x0020, 0x0052));

    // Structure Set module (DICOM PS3.3, C.8.8.5).
    insert_if_nonempty(m, "StructureSetLabel", read_text(root, 0x3006, 0x0002));
    insert_if_nonempty(m, "StructureSetName", read_text(root, 0x3006, 0x0004));
    insert_if_nonempty(m, "StructureSetDate", read_text(root, 0x3006, 0x0008));
    insert_if_nonempty(m, "StructureSetTime", read_text(root, 0x3006, 0x0009));

    // Manufacturer info.
    insert_if_nonempty(m, "Manufacturer", read_text(root, 0x0008, 0x0070));
    insert_if_nonempty(m, "InstitutionName", read_text(root, 0x0008, 0x0080));
    insert_if_nonempty(m, "StationName", read_text(root, 0x0008, 0x1010));

    return m;
}


std::map<int64_t, std::string> extract_roi_names(const Node &root){
    std::map<int64_t, std::string> roi_map;

    // StructureSetROISequence (3006,0020).
    // DICOM PS3.3 2026b, C.8.8.5: Contains one item per ROI.
    // Each item contains:
    //   - ROI Number (3006,0022): IS, unique identifier for the ROI.
    //   - ROI Name (3006,0026): LO, human-readable name.
    //   - Referenced Frame of Reference UID (3006,0024): UI.
    //   - ROI Generation Algorithm (3006,0036): CS.
    //   - ROI Generation Description (3006,0038): LO.

    // find_all is recursive; we want only direct children of sequences.
    // The StructureSetROISequence is a single tag (3006,0020) with child items.
    const auto *seq_node = root.find(0x3006, 0x0020);
    if(seq_node == nullptr){
        return roi_map;
    }

    // Each child of the sequence node is a sequence item.
    for(const auto &item : seq_node->children){
        // Within each item, find ROI Number and ROI Name.
        std::optional<int64_t> roi_number;
        std::string roi_name;

        for(const auto &child : item.children){
            if(child.key.group == 0x3006 && child.key.tag == 0x0022){
                // ROI Number (IS VR).
                roi_number = read_IS(child);
            }else if(child.key.group == 0x3006 && child.key.tag == 0x0026){
                // ROI Name (LO VR).
                roi_name = strip_padding(child.val);
            }
        }

        if(roi_number.has_value()){
            roi_map[roi_number.value()] = roi_name;
        }
    }

    return roi_map;
}


std::unique_ptr<Contour_Data> extract_contour_data(const Node &root){
    auto output = std::make_unique<Contour_Data>();

    // Verify modality is RTSTRUCT.
    const auto modality = read_text(root, 0x0008, 0x0060);
    if(modality != "RTSTRUCT"){
        YLOGWARN("Expected modality RTSTRUCT but found '" << modality << "'");
        return nullptr;
    }

    // Extract top-level metadata.
    const auto file_metadata = extract_common_metadata(root);

    // Extract ROI name mapping.
    const auto roi_names = extract_roi_names(root);

    // Map to accumulate contours by (ROI name, ROI number).
    std::map<std::tuple<std::string, int64_t>, contour_collection<double>> mapcache;

    // -------------------------------------------------------------------------
    // ROIContourSequence (3006,0039).
    // DICOM PS3.3 2026b, C.8.8.6: Contains contour data for each ROI.
    // Each item contains:
    //   - Referenced ROI Number (3006,0084): IS, links to StructureSetROISequence.
    //   - ROI Display Color (3006,002A): IS, optional RGB triplet.
    //   - ContourSequence (3006,0040): Sequence of contour items.
    //       Each contour item contains:
    //         - Contour Geometric Type (3006,0042): CS (e.g., CLOSED_PLANAR, POINT).
    //         - Number of Contour Points (3006,0046): IS.
    //         - Contour Data (3006,0050): DS, x\y\z triplets.
    // -------------------------------------------------------------------------

    const auto *roi_contour_seq = root.find(0x3006, 0x0039);
    if(roi_contour_seq == nullptr){
        YLOGWARN("ROIContourSequence (3006,0039) not found");
        return nullptr;
    }

    for(const auto &roi_item : roi_contour_seq->children){
        // Get Referenced ROI Number.
        std::optional<int64_t> ref_roi_number;
        for(const auto &child : roi_item.children){
            if(child.key.group == 0x3006 && child.key.tag == 0x0084){
                ref_roi_number = read_IS(child);
                break;
            }
        }

        if(!ref_roi_number.has_value()){
            YLOGWARN("ROIContourSequence item missing Referenced ROI Number");
            continue;
        }

        const int64_t roi_number = ref_roi_number.value();
        const auto roi_name_it = roi_names.find(roi_number);
        const std::string roi_name = (roi_name_it != roi_names.end()) ? roi_name_it->second : "";
        const auto cache_key = std::make_tuple(roi_name, roi_number);

        // Find ContourSequence (3006,0040) within this ROI item.
        const Node *contour_seq = nullptr;
        for(const auto &child : roi_item.children){
            if(child.key.group == 0x3006 && child.key.tag == 0x0040){
                contour_seq = &child;
                break;
            }
        }

        if(contour_seq == nullptr){
            // No contours for this ROI (may be intentional for empty ROIs).
            continue;
        }

        // Iterate over contour items.
        for(const auto &contour_item : contour_seq->children){
            // Find Contour Data (3006,0050).
            const Node *contour_data_node = nullptr;
            std::string contour_geometric_type;

            for(const auto &child : contour_item.children){
                if(child.key.group == 0x3006 && child.key.tag == 0x0050){
                    contour_data_node = &child;
                }else if(child.key.group == 0x3006 && child.key.tag == 0x0042){
                    contour_geometric_type = strip_padding(child.val);
                }
            }

            if(contour_data_node == nullptr){
                continue;
            }

            // Parse contour data as DS (Decimal String) with backslash-separated values.
            const auto coords = read_DS_vector(*contour_data_node);
            if(coords.size() < 3 || (coords.size() % 3) != 0){
                YLOGWARN("Invalid contour data: expected multiple of 3 coordinates, got " << coords.size());
                continue;
            }

            // Build contour_of_points.
            contour_of_points<double> contour;

            // Determine if contour is closed based on geometric type.
            // CLOSED_PLANAR is the most common; OPEN_PLANAR and POINT are also valid.
            if(contour_geometric_type == "CLOSED_PLANAR" || contour_geometric_type.empty()){
                contour.closed = true;
            }else if(contour_geometric_type == "OPEN_PLANAR" || contour_geometric_type == "OPEN_NONPLANAR"){
                contour.closed = false;
            }else if(contour_geometric_type == "POINT"){
                contour.closed = false;
            }else{
                // Default to closed for unknown types.
                contour.closed = true;
            }

            // Add points.
            for(size_t i = 0; i + 2 < coords.size(); i += 3){
                const double x = coords[i + 0];
                const double y = coords[i + 1];
                const double z = coords[i + 2];
                contour.points.emplace_back(x, y, z);
            }

            // Attach metadata.
            contour.metadata = file_metadata;
            contour.metadata["ROINumber"] = std::to_string(roi_number);
            contour.metadata["ROIName"] = roi_name;

            // Add to cache.
            mapcache[cache_key].contours.push_back(std::move(contour));
        }
    }

    // -------------------------------------------------------------------------
    // RTROIObservationsSequence (3006,0080).
    // DICOM PS3.3 2026b, C.8.8.8: Contains observation metadata for each ROI.
    // Each item contains:
    //   - Observation Number (3006,0082): IS.
    //   - Referenced ROI Number (3006,0084): IS.
    //   - ROI Observation Description (3006,0088): ST.
    //   - RT ROI Interpreted Type (3006,00A4): CS (e.g., ORGAN, PTV, CTV, EXTERNAL).
    //   - ROI Interpreter (3006,00A6): PN.
    //   - RTROIIdentificationCodeSequence (3006,0086): Sequence of code items.
    // -------------------------------------------------------------------------

    const auto *obs_seq = root.find(0x3006, 0x0080);
    if(obs_seq != nullptr){
        for(const auto &obs_item : obs_seq->children){
            // Get Referenced ROI Number.
            std::optional<int64_t> ref_roi_number;
            for(const auto &child : obs_item.children){
                if(child.key.group == 0x3006 && child.key.tag == 0x0084){
                    ref_roi_number = read_IS(child);
                    break;
                }
            }

            if(!ref_roi_number.has_value()){
                continue;
            }

            const int64_t roi_number = ref_roi_number.value();
            const auto roi_name_it = roi_names.find(roi_number);
            const std::string roi_name = (roi_name_it != roi_names.end()) ? roi_name_it->second : "";
            const auto cache_key = std::make_tuple(roi_name, roi_number);

            auto cc_it = mapcache.find(cache_key);
            if(cc_it == mapcache.end()){
                continue;
            }

            // Extract observation metadata.
            std::string observation_number;
            std::string roi_observation_description;
            std::string rt_roi_interpreted_type;
            std::string roi_interpreter;

            for(const auto &child : obs_item.children){
                if(child.key.group == 0x3006 && child.key.tag == 0x0082){
                    observation_number = strip_padding(child.val);
                }else if(child.key.group == 0x3006 && child.key.tag == 0x0088){
                    roi_observation_description = strip_padding(child.val);
                }else if(child.key.group == 0x3006 && child.key.tag == 0x00A4){
                    rt_roi_interpreted_type = strip_padding(child.val);
                }else if(child.key.group == 0x3006 && child.key.tag == 0x00A6){
                    roi_interpreter = strip_padding(child.val);
                }
            }

            // Apply to all contours in this ROI.
            for(auto &c : cc_it->second.contours){
                insert_or_append(c.metadata, "RTROIObservationsSequence/ObservationNumber", observation_number);
                insert_or_append(c.metadata, "RTROIObservationsSequence/ROIObservationDescription", roi_observation_description);
                insert_or_append(c.metadata, "RTROIObservationsSequence/RTROIInterpretedType", rt_roi_interpreted_type);
                insert_or_append(c.metadata, "RTROIObservationsSequence/ROIInterpreter", roi_interpreter);
            }

            // RTROIIdentificationCodeSequence (3006,0086).
            // DICOM PS3.3 2026b, C.8.8.8: Contains coded ROI identification.
            // Each item contains Code Value (0008,0100), Coding Scheme Designator (0008,0102),
            // Coding Scheme Version (0008,0103), Code Meaning (0008,0104), etc.
            for(const auto &child : obs_item.children){
                if(child.key.group == 0x3006 && child.key.tag == 0x0086){
                    // This is the RTROIIdentificationCodeSequence.
                    for(const auto &code_item : child.children){
                        std::string code_value;
                        std::string coding_scheme_designator;
                        std::string coding_scheme_version;
                        std::string code_meaning;
                        std::string mapping_resource;
                        std::string context_group_version;
                        std::string context_identifier;
                        std::string context_uid;
                        std::string mapping_resource_uid;
                        std::string mapping_resource_name;

                        for(const auto &code_child : code_item.children){
                            if(code_child.key.group == 0x0008 && code_child.key.tag == 0x0100){
                                code_value = strip_padding(code_child.val);
                            }else if(code_child.key.group == 0x0008 && code_child.key.tag == 0x0102){
                                coding_scheme_designator = strip_padding(code_child.val);
                            }else if(code_child.key.group == 0x0008 && code_child.key.tag == 0x0103){
                                coding_scheme_version = strip_padding(code_child.val);
                            }else if(code_child.key.group == 0x0008 && code_child.key.tag == 0x0104){
                                code_meaning = strip_padding(code_child.val);
                            }else if(code_child.key.group == 0x0008 && code_child.key.tag == 0x0105){
                                mapping_resource = strip_padding(code_child.val);
                            }else if(code_child.key.group == 0x0008 && code_child.key.tag == 0x0106){
                                context_group_version = strip_padding(code_child.val);
                            }else if(code_child.key.group == 0x0008 && code_child.key.tag == 0x010F){
                                context_identifier = strip_padding(code_child.val);
                            }else if(code_child.key.group == 0x0008 && code_child.key.tag == 0x0117){
                                context_uid = strip_padding(code_child.val);
                            }else if(code_child.key.group == 0x0008 && code_child.key.tag == 0x0118){
                                mapping_resource_uid = strip_padding(code_child.val);
                            }else if(code_child.key.group == 0x0008 && code_child.key.tag == 0x0122){
                                mapping_resource_name = strip_padding(code_child.val);
                            }
                        }

                        // Apply to all contours in this ROI.
                        for(auto &c : cc_it->second.contours){
                            insert_or_append(c.metadata, "RTROIObservationsSequence/RTROIIdentificationCodeSequence/CodeValue", code_value);
                            insert_or_append(c.metadata, "RTROIObservationsSequence/RTROIIdentificationCodeSequence/CodingSchemeDesignator", coding_scheme_designator);
                            insert_or_append(c.metadata, "RTROIObservationsSequence/RTROIIdentificationCodeSequence/CodingSchemeVersion", coding_scheme_version);
                            insert_or_append(c.metadata, "RTROIObservationsSequence/RTROIIdentificationCodeSequence/CodeMeaning", code_meaning);
                            insert_or_append(c.metadata, "RTROIObservationsSequence/RTROIIdentificationCodeSequence/MappingResource", mapping_resource);
                            insert_or_append(c.metadata, "RTROIObservationsSequence/RTROIIdentificationCodeSequence/ContextGroupVersion", context_group_version);
                            insert_or_append(c.metadata, "RTROIObservationsSequence/RTROIIdentificationCodeSequence/ContextIdentifier", context_identifier);
                            insert_or_append(c.metadata, "RTROIObservationsSequence/RTROIIdentificationCodeSequence/ContextUID", context_uid);
                            insert_or_append(c.metadata, "RTROIObservationsSequence/RTROIIdentificationCodeSequence/MappingResourceUID", mapping_resource_uid);
                            insert_or_append(c.metadata, "RTROIObservationsSequence/RTROIIdentificationCodeSequence/MappingResourceName", mapping_resource_name);
                        }
                    }
                }
            }
        }
    }

    // Move contour collections from the cache to the output.
    for(auto &kv : mapcache){
        output->ccs.emplace_back(std::move(kv.second));
    }

    // -------------------------------------------------------------------------
    // Compute minimum separation between contours.
    // This is used downstream for interpolation and other processing.
    // -------------------------------------------------------------------------
    double min_spacing = 1.0E30;
    for(auto &cc : output->ccs){
        if(cc.contours.size() < 2) continue;

        for(auto c1_it = cc.contours.begin(); c1_it != cc.contours.end(); ++c1_it){
            auto c2_it = c1_it;
            ++c2_it;
            if(c2_it == cc.contours.end()) break;

            const double height1 = c1_it->Average_Point().Dot(vec3<double>(0.0, 0.0, 1.0));
            const double height2 = c2_it->Average_Point().Dot(vec3<double>(0.0, 0.0, 1.0));
            const double spacing = std::abs(height2 - height1);

            if((spacing < min_spacing) && (spacing > 1.0E-3)){
                min_spacing = spacing;
            }
        }
    }

    // Attach minimum separation to all contours.
    for(auto &cc : output->ccs){
        for(auto &c : cc.contours){
            c.metadata["MinimumSeparation"] = std::to_string(min_spacing);
        }
    }

    return output;
}


} // namespace DCMA_DICOM

