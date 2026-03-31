// DCMA_DICOM_Transform.h - A part of DICOMautomaton 2026. Written by hal clark.
//
// This file contains routines for extracting spatial registration (transform) data from
// a parsed DICOM Node tree. The extracted transform is returned as a Transform3 object
// which can hold affine transforms or deformation fields.
//
// References:
//   - DICOM PS3.3 2026b, Section A.39: Spatial Registration IOD.
//   - DICOM PS3.3 2026b, Section C.20.2: Spatial Registration Module.
//   - DICOM PS3.3 2026b, Section C.20.3: Deformable Spatial Registration Module.

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "DCMA_DICOM.h"
#include "Structs.h"
#include "Metadata.h"
#include "Alignment_Rigid.h"
#include "Alignment_Field.h"

namespace DCMA_DICOM {


// ============================================================================
// Spatial Registration (Transform) data extraction.
// ============================================================================

// Extract spatial registration data from a parsed DICOM Node tree.
//
// This function extracts transformation data from a Spatial Registration DICOM file
// (Modality "REG") that has been parsed into a Node tree. It reads:
//
//   - RegistrationSequence (0070,0308): Rigid/affine matrix registrations.
//     - MatrixRegistrationSequence (0070,0309): Matrix transformation data.
//       - MatrixSequence (0070,030A): 4x4 transformation matrix.
//
//   - DeformableRegistrationSequence (0064,0002): Deformable registrations.
//     - DeformableRegistrationGridSequence (0064,0005): Grid-based deformation.
//       - ImagePositionPatient, ImageOrientationPatient, GridDimensions, etc.
//       - VectorGridData (0064,0009): Displacement vectors.
//
// For rigid/affine registrations, the 4x4 transformation matrix is stored as an
// affine_transform<double>. For deformable registrations, the displacement field
// is stored as a deformation_field.
//
// Returns nullptr if:
//   - The node tree does not contain required registration tags.
//   - The modality is not "REG".
//   - The transformation cannot be parsed or validated.
//
// DICOM References:
//   - Spatial Registration Module: DICOM PS3.3 2026b, C.20.2.
//   - Deformable Spatial Registration Module: DICOM PS3.3 2026b, C.20.3.
//
std::unique_ptr<Transform3> extract_transform(const Node &root);


// Extract a 4x4 affine transformation matrix from a vector of 16 doubles.
//
// The input vector should contain 16 values in row-major order:
//   [m00 m01 m02 m03 | m10 m11 m12 m13 | m20 m21 m22 m23 | m30 m31 m32 m33]
//
// The DICOM Frame of Reference Transformation Matrix (3006,00C6) is stored
// in this format per DICOM PS3.3 2026b, C.20.2.1.1.
//
// The last row must be [0, 0, 0, 1] for a valid affine transformation.
// Only the upper 3x4 portion is extracted into the affine_transform.
//
// Throws std::runtime_error if the vector size is not 16 or the last row is invalid.
//
affine_transform<double> extract_affine_from_matrix(const std::vector<double> &matrix);


} // namespace DCMA_DICOM

