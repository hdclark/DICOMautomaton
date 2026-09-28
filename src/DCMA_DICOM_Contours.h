// DCMA_DICOM_Contours.h - A part of DICOMautomaton 2026. Written by hal clark.
//
// This file contains routines for extracting RT Structure Set (contour) data from a parsed
// DICOM Node tree. The extracted contours are returned as a Contour_Data object with full
// metadata including ROI names, ROI numbers, and observation metadata.
//
// References:
//   - DICOM PS3.3 2026b, Section A.19.3: RT Structure Set IOD Module Table.
//   - DICOM PS3.3 2026b, Section C.8.8.5: Structure Set Module.
//   - DICOM PS3.3 2026b, Section C.8.8.6: ROI Contour Module.
//   - DICOM PS3.3 2026b, Section C.8.8.8: RT ROI Observations Module.

#pragma once

#include <cstdint>
#include <memory>
#include <map>
#include <string>
#include <optional>

#include "DCMA_DICOM.h"
#include "Structs.h"
#include "Metadata.h"

namespace DCMA_DICOM {


// ============================================================================
// RT Structure Set (Contour) data extraction.
// ============================================================================

// Extract contour data from a parsed DICOM RT Structure Set Node tree.
//
// This function extracts planar contour data from an RT Structure Set DICOM file
// (Modality "RTSTRUCT") that has been parsed into a Node tree. It reads:
//
//   - StructureSetROISequence (3006,0020): ROI names and numbers.
//   - ROIContourSequence (3006,0039): Contour geometric data.
//   - RTROIObservationsSequence (3006,0080): ROI observation metadata.
//
// Each contour is annotated with metadata including:
//   - ROIName, ROINumber
//   - RTROIObservationsSequence/* metadata (observation number, interpreted type, etc.)
//   - Top-level file metadata (patient info, study/series info, etc.)
//
// Returns nullptr if:
//   - The node tree does not contain required structure set tags.
//   - The modality is not "RTSTRUCT".
//   - Critical data (e.g., contour points) is missing or malformed.
//
// The returned Contour_Data contains one contour_collection per ROI, with each
// contour_collection containing multiple contour_of_points representing individual
// planar contours.
//
// DICOM References:
//   - Structure Set Module: DICOM PS3.3 2026b, C.8.8.5.
//   - ROI Contour Module: DICOM PS3.3 2026b, C.8.8.6.
//   - RT ROI Observations Module: DICOM PS3.3 2026b, C.8.8.8.
//
std::unique_ptr<Contour_Data> extract_contour_data(const Node &root);


// Extract a mapping of ROI numbers to ROI names from a parsed DICOM Node tree.
//
// This function reads the StructureSetROISequence (3006,0020) and extracts:
//   - ROI Number (3006,0022): Integer identifier for the ROI.
//   - ROI Name (3006,0026): Human-readable name for the ROI.
//
// Returns a map from ROI number (int64_t) to ROI name (string). If the sequence
// is missing or empty, an empty map is returned.
//
// DICOM Reference: Structure Set Module, DICOM PS3.3 2026b, C.8.8.5.
//
std::map<int64_t, std::string> extract_roi_names(const Node &root);


// Extract top-level metadata tags commonly used for contour data.
//
// This function extracts a subset of top-level DICOM tags that are commonly
// attached to contour metadata, including patient info, study/series info,
// frame of reference, and structure set identifiers.
//
// Returns a metadata_map_t (std::map<std::string, std::string>) with DICOM
// keyword keys (e.g., "PatientID", "StudyInstanceUID", "FrameOfReferenceUID").
//
metadata_map_t extract_common_metadata(const Node &root);


} // namespace DCMA_DICOM

