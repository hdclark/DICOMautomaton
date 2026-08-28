// DCMA_DICOM_Loader.h - A part of DICOMautomaton 2026. Written by hal clark.
//
// This file contains routines for extracting semantic data from a parsed DICOM Node tree
// into the DICOMautomaton data model (Drover class hierarchy). It provides functionality
// equivalent to the Imebra-based loaders in Imebra_Shim.{h,cc}, but operates on the
// DCMA_DICOM::Node tree rather than the Imebra library.
//
// Supported DICOM modalities:
//   - CT and MR images (single-frame and multi-frame)
//   - RTDOSE (multi-frame dose grids)
//   - RTSTRUCT (contour data)
//   - RTPLAN (treatment plans with beam sequences)
//   - REG (spatial registrations: rigid/affine and deformable)

#pragma once

#include <cstdint>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <filesystem>
#include <optional>

#include "YgorContainers.h"  //Needed for bimap class.

#include "Structs.h"
#include "Metadata.h"
#include "Alignment_Rigid.h"
#include "Alignment_Field.h"
#include "DCMA_DICOM.h"

class Contour_Data;
class Image_Array;

namespace DCMA_DICOM {

// ============================================================================
// General tag accessors (Node-based equivalents of Imebra_Shim functions).
// ============================================================================

// Extract a single DICOM tag as a string from a Node tree.
std::string get_tag_as_string_from_node(const Node &root, uint16_t group, uint16_t tag);

// Extract the Modality (0008,0060) from a Node tree.
std::string get_modality_from_node(const Node &root);

// Extract the PatientID (0010,0020) from a Node tree.
std::string get_patient_ID_from_node(const Node &root);

// Extract top-level metadata tags from a Node tree.
metadata_map_t get_metadata_top_level_tags_from_node(const Node &root,
                                                     const std::string &filename = {});


// ============================================================================
// Contour data (RTSTRUCT).
// ============================================================================

bimap<std::string,int64_t> get_ROI_tags_and_numbers_from_node(const Node &root);

std::unique_ptr<Contour_Data> get_Contour_Data_from_node(const Node &root,
                                                         const std::string &filename = {});


// ============================================================================
// Image data (CT, MR, and other image modalities).
// ============================================================================

std::unique_ptr<Image_Array> Load_Image_Array_from_node(const Node &root,
                                                        const std::string &filename = {});


// ============================================================================
// Dose data (RTDOSE).
// ============================================================================

std::unique_ptr<Image_Array> Load_Dose_Array_from_node(const Node &root,
                                                       const std::string &filename = {});


// ============================================================================
// Treatment plans (RTPLAN).
// ============================================================================

std::unique_ptr<RTPlan> Load_RTPlan_from_node(const Node &root,
                                              const std::string &filename = {});


// ============================================================================
// Registrations (REG).
// ============================================================================

std::unique_ptr<Transform3> Load_Transform_from_node(const Node &root,
                                                     const std::string &filename = {});


// ============================================================================
// File-based convenience wrappers.
// ============================================================================

std::string node_get_tag_as_string(const std::filesystem::path &filename, uint16_t group, uint16_t tag);
std::string node_get_modality(const std::filesystem::path &filename);
std::string node_get_patient_ID(const std::filesystem::path &filename);
metadata_map_t node_get_metadata_top_level_tags(const std::filesystem::path &filename);

bimap<std::string,int64_t> node_get_ROI_tags_and_numbers(const std::filesystem::path &filename);
std::unique_ptr<Contour_Data> node_get_Contour_Data(const std::filesystem::path &filename);

std::unique_ptr<Image_Array> node_Load_Image_Array(const std::filesystem::path &filename);
std::list<std::shared_ptr<Image_Array>> node_Load_Image_Arrays(const std::list<std::filesystem::path> &filenames);
std::unique_ptr<Image_Array> node_Collate_Image_Arrays(std::list<std::shared_ptr<Image_Array>> &in);

std::unique_ptr<Image_Array> node_Load_Dose_Array(const std::filesystem::path &filename);
std::list<std::shared_ptr<Image_Array>> node_Load_Dose_Arrays(const std::list<std::filesystem::path> &filenames);

std::unique_ptr<RTPlan> node_Load_RTPlan(const std::filesystem::path &filename);
std::unique_ptr<Transform3> node_Load_Transform(const std::filesystem::path &filename);


} // namespace DCMA_DICOM

