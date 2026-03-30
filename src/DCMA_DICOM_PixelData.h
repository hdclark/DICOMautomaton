// DCMA_DICOM_PixelData.h - A part of DICOMautomaton 2026. Written by hal clark.
//
// This file contains routines for extracting and interpreting pixel data from a parsed DICOM
// Node tree. It supports native (uncompressed) pixel data and encapsulated 8-bit baseline
// JPEG pixel data. It also provides a composable pixel transformation pipeline for the DICOM
// grayscale and colour image transformation model.
//
// Extracted pixel data is stored directly in planar_image / planar_image_collection objects
// (from the Ygor library) to avoid intermediate marshalling.
//
// References:
//   - DICOM PS3.4 2026b, Section N.2:  Softcopy Presentation State Display Pipeline.
//   - DICOM PS3.5 2026b, Section 8:    Encoding of Pixel, Overlay and Waveform Data.
//   - DICOM PS3.5 2026b, Section 8.2:  Native or Encapsulated Format Encoding.

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <optional>
#include "YgorImages.h"
#include "YgorMath.h"
#include "DCMA_DICOM.h"

namespace DCMA_DICOM {


// ============================================================================
// Transfer syntax classification.
// ============================================================================

// Broad classification of a DICOM Transfer Syntax for the purposes of pixel data decoding.
//
// See DICOM PS3.5 2026b, Section 10: Transfer Syntax, and PS3.6, Annex A for the full
// registry of Transfer Syntax UIDs.
enum class TransferSyntaxType {
    NativeUncompressed,   // Pixel data stored natively (e.g., ILE, ELE, EBE).
    EncapsulatedJPEG,     // JPEG baseline or extended lossy (ISO/IEC 10918-1).
    EncapsulatedJPEG2000, // JPEG 2000 lossless or lossy (ISO/IEC 15444-1).
    EncapsulatedJPEGLS,   // JPEG-LS lossless or near-lossless (ISO/IEC 14495-1).
    EncapsulatedRLE,      // RLE Lossless (DICOM PS3.5, Annex G).
    EncapsulatedDeflated, // Deflated (zlib) transfer syntax.
    EncapsulatedHTJ2K,    // High-Throughput JPEG 2000 (ISO/IEC 15444-15).
    EncapsulatedJPEGXL,   // JPEG XL (ISO/IEC 18181).
    Unknown               // Transfer syntax not recognized or not yet classified.
};

// Classify a Transfer Syntax UID string into a TransferSyntaxType.
// Trailing padding (nulls and spaces) is stripped before comparison.
TransferSyntaxType classify_transfer_syntax(const std::string &ts_uid);

// Return true if the transfer syntax type uses native (uncompressed) pixel encoding.
bool is_native_transfer_syntax(TransferSyntaxType tst);

// Return true if the transfer syntax type uses encapsulated (compressed) pixel encoding.
bool is_encapsulated_transfer_syntax(TransferSyntaxType tst);


// ============================================================================
// Pixel data descriptor.
// ============================================================================

// Metadata gathered from the DICOM tree that describes how pixel data is encoded.
//
// These fields correspond to the "Image Pixel" module (DICOM PS3.3, C.7.6.3).
// Not all fields are required for every image type -- optional fields are represented
// using std::optional.
struct PixelDataDesc {
    // Sampling parameters (from the Image Pixel module).
    uint16_t rows            = 0;   // (0028,0010) Number of rows.
    uint16_t columns         = 0;   // (0028,0011) Number of columns.
    uint16_t bits_allocated  = 0;   // (0028,0100) Number of bits allocated per sample.
    uint16_t bits_stored     = 0;   // (0028,0101) Number of bits stored per sample.
    uint16_t high_bit        = 0;   // (0028,0102) Most significant bit position (0-based from LSB).
    uint16_t samples_per_pixel = 1; // (0028,0002) Number of samples (channels) per pixel.
    uint16_t pixel_representation = 0; // (0028,0103) 0 = unsigned, 1 = two's complement signed.

    // Photometric interpretation (0028,0004). Examples: "MONOCHROME1", "MONOCHROME2", "RGB",
    // "PALETTE COLOR", "YBR_FULL", "YBR_FULL_422", etc.
    std::string photometric_interpretation;

    // Planar configuration (0028,0006). 0 = interleaved (R1G1B1R2G2B2...),
    // 1 = planar (R1R2...G1G2...B1B2...). Only meaningful when samples_per_pixel > 1.
    uint16_t planar_configuration = 0;

    // Number of frames (0028,0008). For single-frame images this is 1.
    uint32_t number_of_frames = 1;

    // Transfer syntax classification.
    TransferSyntaxType transfer_syntax = TransferSyntaxType::Unknown;
};

// Attempt to populate a PixelDataDesc from a parsed DICOM Node tree.
// Returns std::nullopt if critical tags are missing or inconsistent.
std::optional<PixelDataDesc> get_pixel_data_desc(const Node &root);


// ============================================================================
// Pixel transformation pipeline stages (composable).
// ============================================================================
//
// These functions implement individual stages of the DICOM pixel transformation pipeline
// as described in DICOM PS3.4, Section N.2 (Softcopy Presentation State Display Pipeline).
// Each stage is an independent, composable function that operates on a planar_image in-place.
//
// The grayscale pipeline is:
//   Stored Pixel Values → Modality LUT → [Mask] → VOI LUT → Presentation LUT → P-Values
//
// Colour-space conversion is handled separately and is applicable to colour images (e.g.,
// YBR_FULL → RGB).
//
// Each get_*() function reads parameters from the DICOM tree. Each apply_*() function
// applies the transformation to a planar_image in-place. Callers compose the pipeline
// by applying only the stages that are applicable for a given use case.


// --- Modality LUT (linear rescale) ---
//
// Applies: output = rescale_slope * stored_value + rescale_intercept.
// See DICOM PS3.3, C.11.1 (Modality LUT Module) and C.7.6.16.2.9.
struct ModalityLUTParams {
    double rescale_slope     = 1.0;   // (0028,1053) Rescale Slope.
    double rescale_intercept = 0.0;   // (0028,1052) Rescale Intercept.
};

// Extract Modality LUT parameters from a DICOM tree.
// Returns std::nullopt if neither Rescale Slope/Intercept is present.
std::optional<ModalityLUTParams> get_modality_lut_params(const Node &root);

// Apply the Modality LUT (linear rescale) to all pixel values in the image in-place.
void apply_modality_lut(planar_image<float,double> &img, const ModalityLUTParams &params);


// --- VOI LUT / Windowing ---
//
// Linear windowing using Window Center (0028,1050) and Window Width (0028,1051).
// See DICOM PS3.3, C.11.2 (VOI LUT Module) and C.11.2.1.2.
struct VOILUTParams {
    double window_center = 0.0;   // (0028,1050) Window Center.
    double window_width  = 1.0;   // (0028,1051) Window Width.
};

// Extract the first Window Center / Window Width pair from the DICOM tree.
// Returns std::nullopt if neither is present.
std::optional<VOILUTParams> get_voi_lut_params(const Node &root);

// Apply VOI LUT (linear windowing) to all pixel values in the image in-place.
// Output values are mapped to [out_min, out_max].
// See DICOM PS3.3, C.11.2.1.2 for the linear exact mapping formula.
void apply_voi_lut(planar_image<float,double> &img, const VOILUTParams &params,
                   double out_min = 0.0, double out_max = 255.0);


// --- Presentation LUT ---
//
// See DICOM PS3.3, C.11.6 (Softcopy Presentation LUT Module).
enum class PresentationLUTShape {
    Identity,   // No change (DICOM "IDENTITY").
    Inverse     // Invert (DICOM "INVERSE").
};

// Get the Presentation LUT shape from the DICOM tree (tag 2050,0020).
// Returns Identity if not specified.
PresentationLUTShape get_presentation_lut_shape(const Node &root);

// Apply the Presentation LUT to all pixel values in the image in-place.
// For Inverse: output = max_output_val - input.
void apply_presentation_lut(planar_image<float,double> &img, PresentationLUTShape shape,
                            double max_output_val = 255.0);


// --- Colour-space conversion ---
//
// Convert pixel data from one photometric interpretation to another.
//
// Supported conversions:
//   YBR_FULL      → RGB   (3-channel images; DICOM PS3.3 C.7.6.3.1.2)
//   YBR_FULL_422  → RGB   (3-channel images; treats stored samples as full YCbCr)
//   PALETTE COLOR → RGB   (1-channel → 3-channel; requires root Node for LUT data)
//
// Returns true on success, false if the conversion is not supported.
// When root is non-null it is used to read Palette Color LUT Descriptors and Data.
bool convert_photometric_to_rgb(planar_image<float,double> &img,
                                const std::string &from_photometric,
                                const Node *root = nullptr);


// ============================================================================
// Native (uncompressed) pixel data extraction.
// ============================================================================

// Extract native (uncompressed) pixel data from a parsed DICOM Node tree.
//
// This function locates the pixel data tag (one of 7FE0,0010 / 7FE0,0008 / 7FE0,0009),
// reads its raw byte payload, and unpacks samples according to the Image Pixel module
// attributes (Bits Allocated, Bits Stored, High Bit, Pixel Representation, etc.).
//
// The result is returned directly as a planar_image_collection. Each frame produces one
// planar_image with rows, columns, and channels matching the DICOM descriptor. Planar-
// configured (planar_configuration == 1) data is rearranged to interleaved order.
//
// No pixel transformations (Modality LUT, VOI LUT, etc.) are applied; the caller may
// compose them as needed.
//
// Returns std::nullopt if the tree does not contain the expected pixel data or if the
// parameters are inconsistent (e.g., compressed transfer syntax with native pixel data).
//
// Supported Bits Allocated values: 1, 8, 16, 32, 64.
// For Float Pixel Data (7FE0,0008) the samples are 32-bit IEEE 754 floats.
// For Double Float Pixel Data (7FE0,0009) the samples are 64-bit IEEE 754 doubles.
//
// References:
//   - DICOM PS3.5 2026b, Section 8.1.1: Pixel Data Encoding of Related Data Elements.
//   - DICOM PS3.5 2026b, Section 8.2.1: Native Format Encoding.
std::optional<planar_image_collection<float,double>> extract_native_pixel_data(const Node &root);


// ============================================================================
// Overlay data extraction.
// ============================================================================

// Metadata for a single DICOM overlay plane (group 60xx).
//
// See DICOM PS3.3, C.9.2 Overlay Plane Module.
struct OverlayDesc {
    uint16_t group          = 0;  // The overlay group (0x6000, 0x6002, ..., 0x601E).
    uint16_t rows           = 0;  // (60xx,0010) Overlay rows.
    uint16_t columns        = 0;  // (60xx,0011) Overlay columns.
    uint16_t bits_allocated = 1;  // (60xx,0100) Bits allocated (always 1 for standalone overlays).
    int16_t  origin_row     = 1;  // (60xx,0050) Row origin (1-based).
    int16_t  origin_col     = 1;  // (60xx,0050) Column origin (1-based).
    std::string type;             // (60xx,0040) Overlay type: "G" (graphics) or "R" (ROI).
};

// Extract overlay data for all overlay planes present in the DICOM tree.
// Each overlay is a 1-bit-per-pixel bitmap packed in the Overlay Data (60xx,3000) tag.
// Returns a vector of (OverlayDesc, bitmap) pairs. The bitmap is unpacked to one byte per
// pixel (0 or 1). Returns an empty vector if no overlays are present.
//
// References:
//   - DICOM PS3.5 2026b, Section 8.1.2: Overlay Data Encoding.
//   - DICOM PS3.3 2026b, C.9.2: Overlay Plane Module.
std::vector<std::pair<OverlayDesc, std::vector<uint8_t>>>
extract_overlay_data(const Node &root);


// ============================================================================
// Encapsulated (compressed) pixel data extraction.
// ============================================================================

// Extract encapsulated (compressed) pixel data from a parsed DICOM Node tree.
//
// Currently supports:
//   - JPEG Baseline (Transfer Syntax UID 1.2.840.10008.1.2.4.50): 8-bit lossy JPEG
//     decoded using the bundled stb_image.h library. Only 8-bit baseline (sequential DCT,
//     Huffman-coded) JPEG is supported; 12-bit, lossless, and arithmetic-coded JPEG are
//     not supported and will cause this function to return std::nullopt.
//
// Unsupported transfer syntaxes (JPEG Extended 12-bit, JPEG Lossless, JPEG 2000, JPEG-LS,
// RLE, HTJ2K, JPEG XL) return std::nullopt. These may be added in the future.
//
// The result is returned directly as a planar_image_collection. For single-frame images,
// one planar_image is produced. Multi-frame encapsulated images are not yet supported.
//
// No pixel transformations (Modality LUT, VOI LUT, etc.) are applied; the caller may
// compose them as needed. The JPEG decoder produces RGB or grayscale output; no further
// colour-space conversion from YBR to RGB is needed for decoded JPEG data.
//
// Returns std::nullopt if the transfer syntax is unsupported or decoding fails.
//
// References:
//   - DICOM PS3.5 2026b, Annex A.4: Transfer Syntaxes For Encapsulation of Encoded Pixel Data.
//   - ISO/IEC 10918-1 (JPEG baseline and extended).
std::optional<planar_image_collection<float,double>> extract_encapsulated_pixel_data(const Node &root);


} // namespace DCMA_DICOM
