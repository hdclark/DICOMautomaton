// DCMA_DICOM_Transform.cc - A part of DICOMautomaton 2026. Written by hal clark.
//
// This file contains routines for extracting spatial registration (transform) data
// from a parsed DICOM Node tree.
//
// References:
//   - DICOM PS3.3 2026b, Section A.39: Spatial Registration IOD.
//   - DICOM PS3.3 2026b, Section C.20.2: Spatial Registration Module.
//   - DICOM PS3.3 2026b, Section C.20.3: Deformable Spatial Registration Module.

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
#include "YgorImages.h"

#include "DCMA_DICOM.h"
#include "DCMA_DICOM_Transform.h"
#include "Structs.h"
#include "Metadata.h"
#include "Alignment_Rigid.h"
#include "Alignment_Field.h"


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

// Find a top-level child node with the given group/tag (non-recursive).
static const Node* find_top_level_tag(const Node &root, uint16_t group, uint16_t tag){
    for(const auto &child : root.children){
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

// Read a vec3<int64_t> from an IS tag with 3 values.
static std::optional<vec3<int64_t>> read_IS_vec3(const std::string &val){
    std::vector<int64_t> out;
    const auto s = strip_padding(val);
    if(s.empty()) return std::nullopt;

    auto tokens = SplitStringToVector(s, '\\', 'd');
    for(const auto &tok : tokens){
        auto i = read_IS(tok);
        if(i.has_value()){
            out.push_back(i.value());
        }
    }
    if(out.size() < 3) return std::nullopt;
    return vec3<int64_t>(out[0], out[1], out[2]);
}

// Insert a metadata value if it is non-empty.
static void insert_if_nonempty(metadata_map_t &m, const std::string &key, const std::string &val){
    const auto trimmed = strip_padding(val);
    if(!trimmed.empty()){
        m[key] = trimmed;
    }
}


// ============================================================================
// Affine transform extraction.
// ============================================================================

affine_transform<double> extract_affine_from_matrix(const std::vector<double> &matrix){
    // DICOM PS3.3 2026b, C.20.2.1.1: Frame of Reference Transformation Matrix.
    // The matrix is a 4x4 homogeneous transformation matrix in row-major order:
    //   [ R00 R01 R02 Tx ]
    //   [ R10 R11 R12 Ty ]
    //   [ R20 R21 R22 Tz ]
    //   [  0   0   0   1 ]
    //
    // Index layout: [0..3] = row 0, [4..7] = row 1, [8..11] = row 2, [12..15] = row 3.
    //
    // For an affine_transform<double>, we only store the upper 3x4 portion.

    if(matrix.size() != 16){
        throw std::runtime_error("Unanticipated matrix transformation dimensions: expected 16 elements, got "
                                 + std::to_string(matrix.size()));
    }

    // Validate the last row is [0, 0, 0, 1].
    if(matrix[12] != 0.0 || matrix[13] != 0.0 || matrix[14] != 0.0 || matrix[15] != 1.0){
        throw std::runtime_error("Transformation cannot be represented using an Affine matrix: "
                                 "last row is not [0, 0, 0, 1]");
    }

    affine_transform<double> t;
    for(size_t row = 0; row < 3; ++row){
        for(size_t col = 0; col < 4; ++col){
            const size_t idx = col + row * 4;
            t.coeff(row, col) = matrix[idx];
        }
    }

    return t;
}


// ============================================================================
// Transform extraction.
// ============================================================================

// Extract common top-level metadata.
static metadata_map_t extract_reg_metadata(const Node &root){
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

    // Manufacturer info.
    insert_if_nonempty(m, "Manufacturer", read_text_tag(root, 0x0008, 0x0070));
    insert_if_nonempty(m, "InstitutionName", read_text_tag(root, 0x0008, 0x0080));
    insert_if_nonempty(m, "StationName", read_text_tag(root, 0x0008, 0x1010));

    return m;
}


std::unique_ptr<Transform3> extract_transform(const Node &root){
    auto out = std::make_unique<Transform3>();

    // Verify modality is REG.
    const auto modality = read_text_tag(root, 0x0008, 0x0060);
    if(modality != "REG"){
        YLOGWARN("Expected modality REG but found '" << modality << "'");
        return nullptr;
    }

    // Extract top-level metadata.
    out->metadata = extract_reg_metadata(root);

    // -------------------------------------------------------------------------
    // RegistrationSequence (0070,0308).
    // DICOM PS3.3 2026b, C.20.2: Spatial Registration Module.
    // Used for rigid/affine matrix-based registrations.
    // -------------------------------------------------------------------------
    const auto *reg_seq = find_top_level_tag(root, 0x0070, 0x0308);
    if(reg_seq != nullptr){
        int i = 0;
        for(const auto &reg_item : reg_seq->children){
            const std::string prfx = "RegistrationSequence" + std::to_string(i) + "/";

            // Frame of Reference UID for this registration.
            insert_if_nonempty(out->metadata, prfx + "FrameOfReferenceUID",
                               read_text_shallow(reg_item, 0x0020, 0x0052));

            // MatrixRegistrationSequence (0070,0309).
            // DICOM PS3.3 2026b, C.20.2.1: Contains matrix transformation data.
            const auto *mat_reg_seq = find_child_shallow(reg_item, 0x0070, 0x0309);
            if(mat_reg_seq != nullptr){
                int j = 0;
                for(const auto &mat_item : mat_reg_seq->children){
                    const std::string pprfx = prfx + "MatrixRegistrationSequence" + std::to_string(j) + "/";

                    // MatrixSequence (0070,030A).
                    // DICOM PS3.3 2026b, C.20.2.1.1: Contains the 4x4 matrix.
                    const auto *mat_seq = find_child_shallow(mat_item, 0x0070, 0x030A);
                    if(mat_seq != nullptr){
                        int k = 0;
                        for(const auto &m_item : mat_seq->children){
                            const std::string ppprfx = pprfx + "MatrixSequence" + std::to_string(k) + "/";

                            // Frame of Reference Transformation Matrix Type (0070,030C).
                            insert_if_nonempty(out->metadata, ppprfx + "FrameOfReferenceTransformationMatrixType",
                                               read_text_shallow(m_item, 0x0070, 0x030C));

                            // Frame of Reference Transformation Matrix (3006,00C6).
                            // 16 DS values representing a 4x4 homogeneous transformation matrix.
                            const auto mat_str = read_text_shallow(m_item, 0x3006, 0x00C6);
                            const auto mat_vec = read_DS_vector(mat_str);

                            if(mat_vec.size() != 16){
                                YLOGWARN("Invalid FrameOfReferenceTransformationMatrix: expected 16 values, got "
                                         << mat_vec.size());
                                ++k;
                                continue;
                            }

                            try{
                                auto t = extract_affine_from_matrix(mat_vec);
                                out->transform = t;
                                YLOGINFO("Loaded affine transform from RegistrationSequence");
                            }catch(const std::exception &e){
                                YLOGWARN("Failed to extract affine transform: " << e.what());
                            }
                            ++k;
                        }
                    }
                    ++j;
                }
            }
            ++i;
        }
    }

    // -------------------------------------------------------------------------
    // DeformableRegistrationSequence (0064,0002).
    // DICOM PS3.3 2026b, C.20.3: Deformable Spatial Registration Module.
    // Used for grid-based deformable registrations.
    // -------------------------------------------------------------------------
    const auto *def_reg_seq = find_top_level_tag(root, 0x0064, 0x0002);
    if(def_reg_seq != nullptr){
        int i = 0;
        for(const auto &def_item : def_reg_seq->children){
            const std::string prfx = "DeformableRegistrationSequence" + std::to_string(i) + "/";

            // Source Frame of Reference UID (0064,0003).
            insert_if_nonempty(out->metadata, prfx + "SourceFrameOfReferenceUID",
                               read_text_shallow(def_item, 0x0064, 0x0003));

            // DeformableRegistrationGridSequence (0064,0005).
            // DICOM PS3.3 2026b, C.20.3.1.1: Contains displacement field data.
            const auto *grid_seq = find_child_shallow(def_item, 0x0064, 0x0005);
            if(grid_seq != nullptr){
                int j = 0;
                for(const auto &grid_item : grid_seq->children){
                    const std::string pprfx = prfx + "DeformableRegistrationGridSequence" + std::to_string(j) + "/";

                    // ImagePositionPatient (0020,0032): Origin of the grid.
                    const auto img_pos_str = read_text_shallow(grid_item, 0x0020, 0x0032);
                    const auto img_pos = read_DS_vec3(img_pos_str);

                    // ImageOrientationPatient (0020,0037): Row and column directions.
                    const auto img_orient_str = read_text_shallow(grid_item, 0x0020, 0x0037);
                    const auto img_orient = read_DS_vector(img_orient_str);

                    // GridDimensions (0064,0007): Number of grid points (cols, rows, frames).
                    const auto grid_dim_str = read_text_shallow(grid_item, 0x0064, 0x0007);
                    const auto grid_dim = read_IS_vec3(grid_dim_str);

                    // GridResolution (0064,0008): Spacing between grid points.
                    const auto grid_res_str = read_text_shallow(grid_item, 0x0064, 0x0008);
                    const auto grid_res = read_DS_vec3(grid_res_str);

                    // VectorGridData (0064,0009): Displacement vectors (N*3 doubles).
                    const auto vec_data_str = read_text_shallow(grid_item, 0x0064, 0x0009);
                    const auto vec_data = read_DS_vector(vec_data_str);

                    // Validate required fields.
                    if(!img_pos.has_value()){
                        YLOGWARN("Missing ImagePositionPatient for deformable registration grid");
                        ++j;
                        continue;
                    }
                    if(img_orient.size() != 6){
                        YLOGWARN("Invalid ImageOrientationPatient: expected 6 values, got " << img_orient.size());
                        ++j;
                        continue;
                    }
                    if(!grid_dim.has_value()){
                        YLOGWARN("Missing GridDimensions for deformable registration grid");
                        ++j;
                        continue;
                    }
                    if(!grid_res.has_value()){
                        YLOGWARN("Missing GridResolution for deformable registration grid");
                        ++j;
                        continue;
                    }

                    const auto image_pos = img_pos.value();
                    const auto image_orien_r = vec3<double>(img_orient[0], img_orient[1], img_orient[2]).unit();
                    const auto image_orien_c = vec3<double>(img_orient[3], img_orient[4], img_orient[5]).unit();
                    const auto image_ortho = image_orien_c.Cross(image_orien_r).unit();

                    const auto image_cols = grid_dim.value().x;
                    const auto image_rows = grid_dim.value().y;
                    const auto image_imgs = grid_dim.value().z;
                    const int64_t image_chns = 3; // Displacement vector has 3 components.

                    const auto image_pxldx = grid_res.value().x;
                    const auto image_pxldy = grid_res.value().y;
                    const auto image_pxldz = grid_res.value().z;

                    const auto voxel_volume = image_pxldx * image_pxldy * image_pxldz;
                    if(voxel_volume <= 0.0){
                        YLOGWARN("Invalid grid voxel dimensions (non-positive volume)");
                        ++j;
                        continue;
                    }

                    const int64_t expected_count = image_rows * image_cols * image_chns * image_imgs;
                    if(static_cast<int64_t>(vec_data.size()) != expected_count){
                        YLOGWARN("VectorGridData size mismatch: expected " << expected_count
                                 << " values, got " << vec_data.size());
                        ++j;
                        continue;
                    }

                    // Build deformation field as a planar_image_collection.
                    const vec3<double> image_anchor(0.0, 0.0, 0.0);
                    auto v_it = vec_data.begin();
                    planar_image_collection<double, double> pic;

                    for(int64_t n = 0; n < image_imgs; ++n){
                        pic.images.emplace_back();
                        pic.images.back().init_orientation(image_orien_r, image_orien_c);
                        pic.images.back().init_buffer(image_rows, image_cols, image_chns);
                        const auto image_offset = image_pos + image_ortho * (static_cast<double>(n) * image_pxldz);
                        pic.images.back().init_spatial(image_pxldx, image_pxldy, image_pxldz,
                                                       image_anchor, image_offset);

                        for(int64_t row = 0; row < image_rows; ++row){
                            for(int64_t col = 0; col < image_cols; ++col){
                                for(int64_t chn = 0; chn < image_chns; ++chn, ++v_it){
                                    pic.images.back().reference(row, col, chn) = *v_it;
                                }
                            }
                        }
                    }

                    YLOGINFO("Loaded deformation field with dimensions "
                             << image_rows << " x " << image_cols << " x " << image_imgs);

                    deformation_field t(std::move(pic));
                    out->transform = t;

                    // PreDeformationMatrixRegistrationSequence (0064,000F).
                    // DICOM PS3.3 2026b, C.20.3.1.1: Optional pre-deformation affine.
                    const auto *pre_mat_seq = find_child_shallow(grid_item, 0x0064, 0x000F);
                    if(pre_mat_seq != nullptr){
                        int k = 0;
                        for(const auto &pm_item : pre_mat_seq->children){
                            const std::string ppprfx = pprfx + "PreDeformationMatrixRegistrationSequence" + std::to_string(k) + "/";

                            insert_if_nonempty(out->metadata, ppprfx + "FrameOfReferenceTransformationMatrixType",
                                               read_text_shallow(pm_item, 0x0070, 0x030C));

                            const auto pre_mat_str = read_text_shallow(pm_item, 0x3006, 0x00C6);
                            const auto pre_mat_vec = read_DS_vector(pre_mat_str);
                            if(pre_mat_vec.size() == 16){
                                try{
                                    auto pre_t = extract_affine_from_matrix(pre_mat_vec);
                                    // Note: pre_transform not currently stored in Transform3.
                                    // Could be added as a future enhancement.
                                    (void)pre_t;
                                }catch(const std::exception &e){
                                    YLOGWARN("Failed to extract pre-deformation affine: " << e.what());
                                }
                            }
                            ++k;
                        }
                    }

                    // PostDeformationMatrixRegistrationSequence (0064,0010).
                    // DICOM PS3.3 2026b, C.20.3.1.1: Optional post-deformation affine.
                    const auto *post_mat_seq = find_child_shallow(grid_item, 0x0064, 0x0010);
                    if(post_mat_seq != nullptr){
                        int k = 0;
                        for(const auto &pm_item : post_mat_seq->children){
                            const std::string ppprfx = pprfx + "PostDeformationMatrixRegistrationSequence" + std::to_string(k) + "/";

                            insert_if_nonempty(out->metadata, ppprfx + "FrameOfReferenceTransformationMatrixType",
                                               read_text_shallow(pm_item, 0x0070, 0x030C));

                            const auto post_mat_str = read_text_shallow(pm_item, 0x3006, 0x00C6);
                            const auto post_mat_vec = read_DS_vector(post_mat_str);
                            if(post_mat_vec.size() == 16){
                                try{
                                    auto post_t = extract_affine_from_matrix(post_mat_vec);
                                    // Note: post_transform not currently stored in Transform3.
                                    // Could be added as a future enhancement.
                                    (void)post_t;
                                }catch(const std::exception &e){
                                    YLOGWARN("Failed to extract post-deformation affine: " << e.what());
                                }
                            }
                            ++k;
                        }
                    }
                    ++j;
                }
            }
            ++i;
        }
    }

    // Check if we extracted a valid transform.
    if(std::holds_alternative<std::monostate>(out->transform)){
        YLOGWARN("No valid transform found in REG file");
        // Still return the object with metadata; caller can check if transform is empty.
    }

    return out;
}


} // namespace DCMA_DICOM
