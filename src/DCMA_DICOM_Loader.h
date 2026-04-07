// DCMA_DICOM_Loader.h - A part of DICOMautomaton 2026. Written by hal clark.
//
// This file contains routines for loading CT, MR, and RTDOSE images from a parsed
// DICOM Node tree. Each method extracts pixel data (using the pixel extraction routines
// in DCMA_DICOM_PixelData), populates spatial parameters (image position, orientation,
// pixel spacing, slice thickness), applies appropriate pixel transformations (Modality LUT,
// dose grid scaling), and imbues each image with key-value DICOM metadata.
//
// References:
//   - DICOM PS3.3 2026b, Section A.3.3:  CT Image IOD Module Table.
//   - DICOM PS3.3 2026b, Section A.4.3:  MR Image IOD Module Table.
//   - DICOM PS3.3 2026b, Section A.18.3: RT Dose IOD Module Table.

#pragma once

#include <memory>

#include "DCMA_DICOM.h"

class Image_Array; // Forward declaration (defined in Structs.h).

namespace DCMA_DICOM {


// Load CT image(s) from a parsed DICOM Node tree.
//
// Extracts pixel data and populates spatial parameters (image position, orientation,
// pixel spacing, slice thickness) and DICOM metadata from the Node tree. The Modality
// LUT (Rescale Slope / Intercept) is applied to convert stored values to Hounsfield Units.
//
// Returns nullptr if pixel data cannot be extracted.
//
// References:
//   - DICOM PS3.3 2026b, Section A.3.3: CT Image IOD Module Table.
std::unique_ptr<Image_Array> load_ct_images(const Node &root);


// Load MR image(s) from a parsed DICOM Node tree.
//
// Extracts pixel data and populates spatial parameters and DICOM metadata. The Modality
// LUT is applied if present. MR-specific metadata (echo time, repetition time, etc.) is
// included.
//
// Returns nullptr if pixel data cannot be extracted.
//
// References:
//   - DICOM PS3.3 2026b, Section A.4.3: MR Image IOD Module Table.
std::unique_ptr<Image_Array> load_mr_images(const Node &root);


// Load RTDOSE image(s) from a parsed DICOM Node tree.
//
// RTDOSE files are typically multi-frame. Per-frame spatial offsets are determined from
// the GridFrameOffsetVector (3004,000C). Pixel values are scaled by DoseGridScaling
// (3004,000E). RTDOSE-specific metadata (DoseUnits, DoseType, etc.) is included.
//
// Returns nullptr if pixel data cannot be extracted.
//
// References:
//   - DICOM PS3.3 2026b, Section A.18.3: RT Dose IOD Module Table.
std::unique_ptr<Image_Array> load_dose_images(const Node &root);


} // namespace DCMA_DICOM
