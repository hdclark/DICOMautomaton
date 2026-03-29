// DCMA_DICOM_PixelData.h - A part of DICOMautomaton 2026. Written by hal clark.
//
// This file contains routines for extracting and interpreting pixel data from a parsed DICOM
// Node tree. It supports native (uncompressed) pixel data and provides stubs for encapsulated
// (compressed) formats.
//
// References:
//   - DICOM PS3.5 2026b, Section 8:   Encoding of Pixel, Overlay and Waveform Data.
//   - DICOM PS3.5 2026b, Section 8.2: Native or Encapsulated Format Encoding.

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <optional>

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
// Extracted pixel data.
// ============================================================================

// Raw pixel data extracted from a DICOM file, with per-sample values promoted to a uniform
// type for convenience. This is a lightweight container; no colour-space conversion or
// windowing is performed.
struct ExtractedPixelData {
    // Pixel sample values stored contiguously. For multi-frame images, frames are stored
    // sequentially. For multi-sample (e.g., RGB) pixels, the sample ordering follows the
    // planar configuration:
    //   planar_configuration == 0 → interleaved: R0 G0 B0 R1 G1 B1 ...
    //   planar_configuration == 1 → planar:      R0 R1 ... G0 G1 ... B0 B1 ...
    //
    // The total number of elements is: number_of_frames * rows * columns * samples_per_pixel.
    std::vector<double> samples;

    // The descriptor used during extraction (for reference).
    PixelDataDesc desc;
};


// ============================================================================
// Native (uncompressed) pixel data extraction.
// ============================================================================

// Extract native (uncompressed) pixel data from a parsed DICOM Node tree.
//
// This function locates the pixel data tag (one of 7FE0,0010 / 7FE0,0008 / 7FE0,0009),
// reads its raw byte payload, and unpacks samples according to the Image Pixel module
// attributes (Bits Allocated, Bits Stored, High Bit, Pixel Representation, etc.).
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
std::optional<ExtractedPixelData> extract_native_pixel_data(const Node &root);


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
// Encapsulated (compressed) pixel data extraction -- stubs.
// ============================================================================

// Extract encapsulated (compressed) pixel data from a parsed DICOM Node tree.
//
// *** THIS IS A STUB. It is not yet implemented. ***
//
// When implemented, this function will:
//   1. Locate the Pixel Data tag (7FE0,0010) whose raw value contains the concatenated
//      encapsulated fragments (as assembled by read_encapsulated_data() during parsing).
//   2. Parse the Basic Offset Table (first fragment, per DICOM PS3.5, Table A.4-1) to
//      identify per-frame fragment boundaries in multi-frame images.
//   3. Dispatch each frame's compressed byte stream to an appropriate codec based on the
//      Transfer Syntax UID:
//
//      a) JPEG Baseline / Extended (Transfer Syntax UIDs 1.2.840.10008.1.2.4.50,
//         1.2.840.10008.1.2.4.51):
//         - Decode using a JPEG codec. The bundled stb_image.h library supports baseline
//           JPEG (8-bit, Huffman-coded, sequential DCT) and could serve as a backend for
//           8-bit lossy JPEG data. However, it does NOT support:
//             * 12-bit or 16-bit JPEG (used in some medical images)
//             * JPEG lossless modes
//             * Arithmetic coding
//           Therefore stb_image.h is suitable only for 8-bit baseline JPEG. Higher bit-depth
//           or lossless JPEG will require a dedicated library (e.g., libjpeg-turbo or OpenJPEG).
//         - Reference: DICOM PS3.5 2026b, Annex A.4 (Transfer Syntaxes For Encapsulation of
//           Encoded Pixel Data), Table A.4-1.
//         - Reference: ISO/IEC 10918-1 (JPEG baseline and extended).
//
//      b) JPEG Lossless (Transfer Syntax UIDs 1.2.840.10008.1.2.4.57,
//         1.2.840.10008.1.2.4.70):
//         - Requires a JPEG lossless decoder (Huffman-coded, first-order prediction).
//           stb_image.h does NOT support lossless JPEG.
//         - Reference: DICOM PS3.5 2026b, Annex A.4.
//         - Reference: ISO/IEC 10918-1, Section 14.1 (Lossless mode).
//
//      c) JPEG 2000 (Transfer Syntax UIDs 1.2.840.10008.1.2.4.90 lossless,
//         1.2.840.10008.1.2.4.91 lossy):
//         - Requires a JPEG 2000 codec (e.g., OpenJPEG).
//         - Neither stb_image.h nor any other bundled library supports JPEG 2000.
//         - Reference: DICOM PS3.5 2026b, Annex A.4.
//         - Reference: ISO/IEC 15444-1 (JPEG 2000 Part 1).
//
//      d) JPEG-LS (Transfer Syntax UIDs 1.2.840.10008.1.2.4.80 lossless,
//         1.2.840.10008.1.2.4.81 near-lossless):
//         - Requires a JPEG-LS codec (e.g., CharLS).
//         - Not supported by any bundled library.
//         - Reference: DICOM PS3.5 2026b, Annex A.4.
//         - Reference: ISO/IEC 14495-1 (JPEG-LS).
//
//      e) RLE Lossless (Transfer Syntax UID 1.2.840.10008.1.2.5):
//         - RLE decoding as specified in DICOM PS3.5, Annex G (RLE Compression).
//         - Each RLE segment corresponds to one byte plane of a single frame.
//         - The RLE algorithm is simple (PackBits variant) and can be implemented without
//           external dependencies:
//             * Read the RLE header: number of segments (uint32_t) followed by up to 15
//               segment offsets (uint32_t each), totalling a 64-byte header.
//             * For each segment, decode the PackBits-style byte stream.
//             * Interleave byte planes to reconstruct the final pixel data.
//         - Reference: DICOM PS3.5 2026b, Annex G (RLE Compression).
//
//      f) Deflated Explicit VR Little Endian (Transfer Syntax UID 1.2.840.10008.1.2.1.99):
//         - The entire Data Set (beyond the meta information header) is deflated using zlib.
//           This is handled at the transport level, not at the pixel data level -- the pixel
//           data itself, once the Data Set is inflated, is native/uncompressed.
//         - Reference: DICOM PS3.5 2026b, Section A.5.
//
//      g) High-Throughput JPEG 2000 (HTJ2K) (Transfer Syntax UIDs
//         1.2.840.10008.1.2.4.201 lossless, 1.2.840.10008.1.2.4.202 rpcl lossless,
//         1.2.840.10008.1.2.4.203 lossy):
//         - Requires an HTJ2K-capable codec (e.g., OpenJPEG >= 2.5 or OpenHTJ2K).
//         - Not supported by any bundled library.
//         - Reference: DICOM PS3.5 2026b, Annex A.4.
//         - Reference: ISO/IEC 15444-15 (HTJ2K).
//
//      h) JPEG XL (Transfer Syntax UIDs 1.2.840.10008.1.2.4.110 lossless,
//         1.2.840.10008.1.2.4.111 lossy, 1.2.840.10008.1.2.4.112 lossless recompressed):
//         - Requires a JPEG XL codec (e.g., libjxl).
//         - Not supported by any bundled library.
//         - Reference: ISO/IEC 18181 (JPEG XL).
//
//   4. Populate the ExtractedPixelData output from the decoded frame samples.
//
// Returns std::nullopt unconditionally in this stub implementation.
std::optional<ExtractedPixelData> extract_encapsulated_pixel_data(const Node &root);


} // namespace DCMA_DICOM
