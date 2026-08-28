// DCMA_DICOM_RTPLAN.h - A part of DICOMautomaton 2026. Written by hal clark.
//
// This file contains routines for extracting RT Plan data from a parsed DICOM Node tree.
// The extracted plan is returned as an RTPlan object containing Dynamic_Machine_State
// (beams) and Static_Machine_State (control points) objects with full metadata.
//
// References:
//   - DICOM PS3.3 2026b, Section A.20.3: RT Plan IOD Module Table.
//   - DICOM PS3.3 2026b, Section C.8.8.9: RT General Plan Module.
//   - DICOM PS3.3 2026b, Section C.8.8.14: RT Beams Module.

#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "DCMA_DICOM.h"
#include "Structs.h"
#include "Metadata.h"

namespace DCMA_DICOM {


// ============================================================================
// RT Plan data extraction.
// ============================================================================

// Extract RT Plan data from a parsed DICOM RT Plan Node tree.
//
// This function extracts treatment plan data from an RT Plan DICOM file
// (Modality "RTPLAN") that has been parsed into a Node tree. It reads:
//
//   - General plan metadata (patient, study, series).
//   - DoseReferenceSequence (300A,0010): Dose reference points.
//   - ReferencedStructureSetSequence (300C,0060): Links to structure sets.
//   - ToleranceTableSequence (300A,0040): Tolerance tables.
//   - FractionGroupSequence (300A,0070): Fraction group info.
//   - BeamSequence (300A,00B0): Beam definitions → Dynamic_Machine_State.
//     - ControlPointSequence (300A,0111): Control points → Static_Machine_State.
//
// Each beam is represented as a Dynamic_Machine_State, and each control point
// as a Static_Machine_State. Machine angles, positions, and leaf/jaw positions
// are extracted and stored in the corresponding fields.
//
// Returns nullptr if:
//   - The node tree does not contain required RT Plan tags.
//   - The modality is not "RTPLAN".
//
// Throws std::runtime_error if:
//   - Critical beam or control point data (e.g., beam number, BeamName,
//     weights, or required ControlPointSequence fields) is missing,
//     malformed, or internally inconsistent.
//
// Callers should be prepared to both check for a nullptr return (for
// non-RTPLAN or obviously missing RT Plan context) and catch
// std::runtime_error for malformed or contradictory RT Plan content.
//
// DICOM References:
//   - RT General Plan Module: DICOM PS3.3 2026b, C.8.8.9.
//   - RT Beams Module: DICOM PS3.3 2026b, C.8.8.14.
//   - RT Fraction Scheme Module: DICOM PS3.3 2026b, C.8.8.13.
//
std::unique_ptr<RTPlan> extract_rtplan(const Node &root);


} // namespace DCMA_DICOM

