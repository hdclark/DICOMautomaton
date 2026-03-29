// DCMA_DICOM_PixelData.cc - A part of DICOMautomaton 2026. Written by hal clark.
//
// This file contains routines for extracting and interpreting pixel data from a parsed DICOM
// Node tree, supporting native (uncompressed) pixel data and overlay data.
//
// References:
//   - DICOM PS3.5 2026b, Section 8:   Encoding of Pixel, Overlay and Waveform Data.
//   - DICOM PS3.5 2026b, Section 8.2: Native or Encapsulated Format Encoding.


#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <optional>
#include <stdexcept>
#include <algorithm>

#include "YgorLog.h"

#include "DCMA_DICOM.h"
#include "DCMA_DICOM_PixelData.h"

namespace DCMA_DICOM {

// ============================================================================
// Transfer syntax classification.
// ============================================================================

static std::string strip_ts_padding(const std::string &s){
    std::string out = s;
    while(!out.empty() && (out.back() == '\0' || out.back() == ' ')) out.pop_back();
    return out;
}

TransferSyntaxType classify_transfer_syntax(const std::string &ts_uid){
    const auto ts = strip_ts_padding(ts_uid);

    // Native (uncompressed) transfer syntaxes.
    // See DICOM PS3.5 2026b, Section 10, and PS3.6, Annex A.
    if(ts == "1.2.840.10008.1.2"      // Implicit VR Little Endian
    || ts == "1.2.840.10008.1.2.1"    // Explicit VR Little Endian
    || ts == "1.2.840.10008.1.2.2"){  // Explicit VR Big Endian (retired)
        return TransferSyntaxType::NativeUncompressed;
    }

    // Deflated Explicit VR Little Endian.
    // The Data Set is deflated, but once inflated the pixel data is native.
    // See DICOM PS3.5 2026b, Section A.5.
    if(ts == "1.2.840.10008.1.2.1.99"){
        return TransferSyntaxType::EncapsulatedDeflated;
    }

    // RLE Lossless. See DICOM PS3.5 2026b, Annex G.
    if(ts == "1.2.840.10008.1.2.5"){
        return TransferSyntaxType::EncapsulatedRLE;
    }

    // JPEG Baseline (Process 1) and Extended (Processes 2 & 4).
    // See DICOM PS3.5 2026b, Annex A.4; ISO/IEC 10918-1.
    if(ts == "1.2.840.10008.1.2.4.50"   // JPEG Baseline (Process 1): 8-bit lossy.
    || ts == "1.2.840.10008.1.2.4.51"   // JPEG Extended (Process 2 & 4): 8/12-bit lossy.
    || ts == "1.2.840.10008.1.2.4.57"   // JPEG Lossless (Process 14).
    || ts == "1.2.840.10008.1.2.4.70"){ // JPEG Lossless, first-order prediction (Process 14 SV1).
        return TransferSyntaxType::EncapsulatedJPEG;
    }

    // JPEG-LS. See ISO/IEC 14495-1.
    if(ts == "1.2.840.10008.1.2.4.80"   // JPEG-LS Lossless.
    || ts == "1.2.840.10008.1.2.4.81"){ // JPEG-LS Near-Lossless.
        return TransferSyntaxType::EncapsulatedJPEGLS;
    }

    // JPEG 2000. See ISO/IEC 15444-1.
    if(ts == "1.2.840.10008.1.2.4.90"   // JPEG 2000 Lossless Only.
    || ts == "1.2.840.10008.1.2.4.91"){ // JPEG 2000 (lossy or lossless).
        return TransferSyntaxType::EncapsulatedJPEG2000;
    }

    // High-Throughput JPEG 2000 (HTJ2K). See ISO/IEC 15444-15.
    if(ts == "1.2.840.10008.1.2.4.201"  // HTJ2K Lossless Only.
    || ts == "1.2.840.10008.1.2.4.202"  // HTJ2K RPCL Lossless Only.
    || ts == "1.2.840.10008.1.2.4.203"){ // HTJ2K (lossy or lossless).
        return TransferSyntaxType::EncapsulatedHTJ2K;
    }

    // JPEG XL. See ISO/IEC 18181.
    if(ts == "1.2.840.10008.1.2.4.110"  // JPEG XL Lossless.
    || ts == "1.2.840.10008.1.2.4.111"  // JPEG XL Lossy.
    || ts == "1.2.840.10008.1.2.4.112"){ // JPEG XL Lossless Recompressed.
        return TransferSyntaxType::EncapsulatedJPEGXL;
    }

    return TransferSyntaxType::Unknown;
}

bool is_native_transfer_syntax(TransferSyntaxType tst){
    return (tst == TransferSyntaxType::NativeUncompressed);
}

bool is_encapsulated_transfer_syntax(TransferSyntaxType tst){
    switch(tst){
        case TransferSyntaxType::EncapsulatedJPEG:
        case TransferSyntaxType::EncapsulatedJPEG2000:
        case TransferSyntaxType::EncapsulatedJPEGLS:
        case TransferSyntaxType::EncapsulatedRLE:
        case TransferSyntaxType::EncapsulatedDeflated:
        case TransferSyntaxType::EncapsulatedHTJ2K:
        case TransferSyntaxType::EncapsulatedJPEGXL:
            return true;
        default:
            return false;
    }
}


// ============================================================================
// Pixel data descriptor.
// ============================================================================

// Helper: read a US-valued tag from the tree and return as uint16_t.
// The Node stores decoded numeric VR values as their string representation.
static std::optional<uint16_t> read_US(const Node &root, uint16_t group, uint16_t tag){
    const auto *n = root.find(group, tag);
    if(n == nullptr) return std::nullopt;

    try{
        const auto v = std::stoul(n->val);
        return static_cast<uint16_t>(v);
    }catch(const std::exception &){
        return std::nullopt;
    }
}

// Helper: read an IS-valued tag from the tree and return as uint32_t.
static std::optional<uint32_t> read_IS_as_uint32(const Node &root, uint16_t group, uint16_t tag){
    const auto *n = root.find(group, tag);
    if(n == nullptr) return std::nullopt;

    try{
        const auto v = std::stoul(n->val);
        return static_cast<uint32_t>(v);
    }catch(const std::exception &){
        return std::nullopt;
    }
}

// Helper: read a text-valued tag from the tree.
static std::string read_text(const Node &root, uint16_t group, uint16_t tag){
    const auto *n = root.find(group, tag);
    if(n == nullptr) return "";

    // Strip trailing padding.
    std::string out = n->val;
    while(!out.empty() && (out.back() == '\0' || out.back() == ' ')) out.pop_back();
    return out;
}


std::optional<PixelDataDesc> get_pixel_data_desc(const Node &root){
    PixelDataDesc desc;

    // Required tags.
    auto rows = read_US(root, 0x0028, 0x0010);
    auto cols = read_US(root, 0x0028, 0x0011);
    auto ba   = read_US(root, 0x0028, 0x0100);
    auto bs   = read_US(root, 0x0028, 0x0101);
    auto hb   = read_US(root, 0x0028, 0x0102);

    if(!rows || !cols || !ba || !bs || !hb){
        YLOGWARN("Missing critical Image Pixel module tags (Rows, Columns, BitsAllocated, BitsStored, or HighBit)");
        return std::nullopt;
    }
    desc.rows           = *rows;
    desc.columns        = *cols;
    desc.bits_allocated = *ba;
    desc.bits_stored    = *bs;
    desc.high_bit       = *hb;

    // Optional tags with defaults.
    auto spp = read_US(root, 0x0028, 0x0002);
    desc.samples_per_pixel = spp.value_or(1);

    auto pr = read_US(root, 0x0028, 0x0103);
    desc.pixel_representation = pr.value_or(0);

    auto pc = read_US(root, 0x0028, 0x0006);
    desc.planar_configuration = pc.value_or(0);

    auto nf = read_IS_as_uint32(root, 0x0028, 0x0008);
    desc.number_of_frames = nf.value_or(1);

    desc.photometric_interpretation = read_text(root, 0x0028, 0x0004);

    // Determine transfer syntax.
    const auto ts_str = read_text(root, 0x0002, 0x0010);
    if(ts_str.empty()){
        // If no TransferSyntaxUID, assume native (many non-Part 10 files are native).
        desc.transfer_syntax = TransferSyntaxType::NativeUncompressed;
    }else{
        desc.transfer_syntax = classify_transfer_syntax(ts_str);
    }

    // Sanity checks.
    if(desc.bits_stored > desc.bits_allocated){
        YLOGWARN("BitsStored (" << desc.bits_stored << ") exceeds BitsAllocated (" << desc.bits_allocated << ")");
        return std::nullopt;
    }
    if(desc.high_bit >= desc.bits_allocated){
        YLOGWARN("HighBit (" << desc.high_bit << ") is not consistent with BitsAllocated (" << desc.bits_allocated << ")");
        return std::nullopt;
    }
    if(desc.bits_stored == 0){
        YLOGWARN("BitsStored is zero; at least 1 bit must be stored");
        return std::nullopt;
    }
    {
        const auto hb_plus1 = static_cast<unsigned int>(desc.high_bit) + 1u;
        const auto bs_u     = static_cast<unsigned int>(desc.bits_stored);
        if(hb_plus1 < bs_u){
            YLOGWARN("HighBit (" << desc.high_bit << ") is not consistent with BitsStored (" << desc.bits_stored
                     << "): HighBit + 1 is less than BitsStored");
            return std::nullopt;
        }
    }
    if(desc.rows == 0 || desc.columns == 0){
        YLOGWARN("Rows or Columns is zero");
        return std::nullopt;
    }

    return desc;
}


// ============================================================================
// Native (uncompressed) pixel data extraction.
// ============================================================================

// Unpack native pixel samples from a raw byte buffer.
//
// For each pixel sample, Bits Allocated determines the stride (1, 8, 16, 32, or 64 bits).
// The stored value occupies Bits Stored contiguous bits, with the most significant stored
// bit at the position given by High Bit (0-based from LSB).
//
// See DICOM PS3.5 2026b, Section 8.1.1 and 8.2.1.
static std::vector<double>
unpack_native_samples(const std::string &raw,
                      const PixelDataDesc &desc,
                      uint64_t total_samples){

    std::vector<double> out;
    out.reserve(total_samples);

    const uint16_t ba = desc.bits_allocated;
    const uint16_t bs = desc.bits_stored;
    const uint16_t hb = desc.high_bit;
    const bool is_signed = (desc.pixel_representation != 0);

    // The low bit position of the stored value within the allocated bits.
    const uint16_t low_bit = static_cast<uint16_t>(hb + 1u - bs);

    if(ba == 1){
        // 1-bit pixels: packed 8 pixels per byte, LSB first within each byte.
        // See DICOM PS3.5 2026b, Section 8.1.1.
        const auto *data = reinterpret_cast<const uint8_t*>(raw.data());
        const auto data_size = raw.size();
        for(uint64_t i = 0; i < total_samples; ++i){
            const uint64_t byte_idx = i / 8u;
            const uint64_t bit_idx  = i % 8u;
            if(byte_idx >= data_size){
                YLOGWARN("Pixel data buffer underflow at sample " << i);
                break;
            }
            out.push_back(static_cast<double>((data[byte_idx] >> bit_idx) & 1u));
        }
    }else if(ba == 8){
        const auto *data = reinterpret_cast<const uint8_t*>(raw.data());
        const uint8_t mask = static_cast<uint8_t>((1u << bs) - 1u);
        for(uint64_t i = 0; i < total_samples; ++i){
            if(i >= raw.size()){
                YLOGWARN("Pixel data buffer underflow at sample " << i);
                break;
            }
            uint8_t v = static_cast<uint8_t>((data[i] >> low_bit) & mask);
            if(is_signed){
                // Sign-extend from bs bits.
                if(v & (1u << (bs - 1u))){
                    v |= static_cast<uint8_t>(~mask);
                }
                out.push_back(static_cast<double>(static_cast<int8_t>(v)));
            }else{
                out.push_back(static_cast<double>(v));
            }
        }
    }else if(ba == 16){
        for(uint64_t i = 0; i < total_samples; ++i){
            const uint64_t offset = i * 2u;
            if(offset + 2u > raw.size()){
                YLOGWARN("Pixel data buffer underflow at sample " << i);
                break;
            }
            uint16_t v = 0;
            std::memcpy(&v, raw.data() + offset, 2);

            const uint16_t mask = static_cast<uint16_t>((1u << bs) - 1u);
            v = static_cast<uint16_t>((v >> low_bit) & mask);

            if(is_signed){
                if(v & (1u << (bs - 1u))){
                    v |= static_cast<uint16_t>(~mask);
                }
                out.push_back(static_cast<double>(static_cast<int16_t>(v)));
            }else{
                out.push_back(static_cast<double>(v));
            }
        }
    }else if(ba == 32){
        for(uint64_t i = 0; i < total_samples; ++i){
            const uint64_t offset = i * 4u;
            if(offset + 4u > raw.size()){
                YLOGWARN("Pixel data buffer underflow at sample " << i);
                break;
            }
            uint32_t v = 0;
            std::memcpy(&v, raw.data() + offset, 4);

            const uint32_t mask = (bs >= 32u) ? 0xFFFFFFFFu : ((1u << bs) - 1u);
            v = (v >> low_bit) & mask;

            if(is_signed){
                if((bs < 32u) && (v & (1u << (bs - 1u)))){
                    v |= ~mask;
                }
                out.push_back(static_cast<double>(static_cast<int32_t>(v)));
            }else{
                out.push_back(static_cast<double>(v));
            }
        }
    }else if(ba == 64){
        for(uint64_t i = 0; i < total_samples; ++i){
            const uint64_t offset = i * 8u;
            if(offset + 8u > raw.size()){
                YLOGWARN("Pixel data buffer underflow at sample " << i);
                break;
            }
            uint64_t v = 0;
            std::memcpy(&v, raw.data() + offset, 8);

            const uint64_t mask = (bs >= 64u) ? ~uint64_t(0) : ((uint64_t(1) << bs) - 1u);
            v = (v >> low_bit) & mask;

            if(is_signed){
                if((bs < 64u) && (v & (uint64_t(1) << (bs - 1u)))){
                    v |= ~mask;
                }
                out.push_back(static_cast<double>(static_cast<int64_t>(v)));
            }else{
                out.push_back(static_cast<double>(v));
            }
        }
    }else{
        YLOGWARN("Unsupported BitsAllocated value: " << ba);
        return {};
    }

    return out;
}


std::optional<ExtractedPixelData> extract_native_pixel_data(const Node &root){
    // Get pixel data descriptor.
    auto desc_opt = get_pixel_data_desc(root);
    if(!desc_opt) return std::nullopt;
    auto desc = *desc_opt;

    // Verify the transfer syntax is native (uncompressed).
    if(!is_native_transfer_syntax(desc.transfer_syntax)){
        YLOGWARN("Transfer syntax is not native/uncompressed -- cannot extract native pixel data");
        return std::nullopt;
    }

    // Locate the pixel data tag. Priority order:
    //   1. Double Float Pixel Data (7FE0,0009) -- 64-bit IEEE 754 doubles.
    //   2. Float Pixel Data         (7FE0,0008) -- 32-bit IEEE 754 floats.
    //   3. Pixel Data               (7FE0,0010) -- general pixel data (OB or OW).
    //
    // See DICOM PS3.5 2026b, Section 8.1.1 and Table 8.2-1.
    const Node *pd_node = root.find(0x7FE0, 0x0009);
    bool is_double_float = (pd_node != nullptr);

    bool is_single_float = false;
    if(pd_node == nullptr){
        pd_node = root.find(0x7FE0, 0x0008);
        is_single_float = (pd_node != nullptr);
    }

    if(pd_node == nullptr){
        pd_node = root.find(0x7FE0, 0x0010);
    }

    if(pd_node == nullptr){
        YLOGWARN("No pixel data tag found (7FE0,0010, 7FE0,0008, or 7FE0,0009)");
        return std::nullopt;
    }

    const auto &raw = pd_node->val;
    const uint64_t total_samples = static_cast<uint64_t>(desc.number_of_frames)
                                 * static_cast<uint64_t>(desc.rows)
                                 * static_cast<uint64_t>(desc.columns)
                                 * static_cast<uint64_t>(desc.samples_per_pixel);
    if(total_samples == 0){
        YLOGWARN("Computed total sample count is zero");
        return std::nullopt;
    }

    ExtractedPixelData epd;
    epd.desc = desc;

    if(is_double_float){
        // Double Float Pixel Data: each sample is a 64-bit IEEE 754 double.
        // DICOM PS3.5 2026b, Section 8.4.
        const uint64_t expected_bytes = total_samples * 8u;
        if(raw.size() < expected_bytes){
            YLOGWARN("Double Float Pixel Data buffer too small: expected "
                     << expected_bytes << " bytes, got " << raw.size());
            return std::nullopt;
        }
        epd.samples.resize(total_samples);
        for(uint64_t i = 0; i < total_samples; ++i){
            double v = 0.0;
            std::memcpy(&v, raw.data() + i * 8u, 8);
            epd.samples[i] = v;
        }
    }else if(is_single_float){
        // Float Pixel Data: each sample is a 32-bit IEEE 754 float.
        // DICOM PS3.5 2026b, Section 8.3.
        const uint64_t expected_bytes = total_samples * 4u;
        if(raw.size() < expected_bytes){
            YLOGWARN("Float Pixel Data buffer too small: expected "
                     << expected_bytes << " bytes, got " << raw.size());
            return std::nullopt;
        }
        epd.samples.resize(total_samples);
        for(uint64_t i = 0; i < total_samples; ++i){
            float v = 0.0f;
            std::memcpy(&v, raw.data() + i * 4u, 4);
            epd.samples[i] = static_cast<double>(v);
        }
    }else{
        // General Pixel Data (7FE0,0010): unpack according to Bits Allocated / Bits Stored / High Bit.
        epd.samples = unpack_native_samples(raw, desc, total_samples);
        if(epd.samples.empty()) return std::nullopt;
    }

    return epd;
}


// ============================================================================
// Overlay data extraction.
// ============================================================================

std::vector<std::pair<OverlayDesc, std::vector<uint8_t>>>
extract_overlay_data(const Node &root){
    std::vector<std::pair<OverlayDesc, std::vector<uint8_t>>> result;

    // Overlay groups are even numbers from 0x6000 to 0x601E (up to 16 overlay planes).
    // See DICOM PS3.3, C.9.2.
    for(uint16_t g = 0x6000; g <= 0x601E; g += 2){
        // Check for Overlay Data (60xx,3000).
        const auto *data_node = root.find(g, 0x3000);
        if(data_node == nullptr) continue;

        OverlayDesc od;
        od.group = g;

        // Read overlay rows and columns.
        auto orows = read_US(root, g, 0x0010);
        auto ocols = read_US(root, g, 0x0011);
        if(!orows || !ocols){
            YLOGWARN("Overlay group " << std::hex << g << std::dec << " missing Rows or Columns");
            continue;
        }
        od.rows    = *orows;
        od.columns = *ocols;

        // Bits Allocated for overlay data is always 1 for standalone overlays (DICOM PS3.5, 8.1.2).
        auto oba = read_US(root, g, 0x0100);
        od.bits_allocated = oba.value_or(1);

        // Overlay origin (60xx,0050): two SS values packed as "row\\column".
        const auto *origin_node = root.find(g, 0x0050);
        if(origin_node != nullptr){
            // The value is stored as "row\\col" in text representation.
            const auto &oval = origin_node->val;
            auto sep = oval.find('\\');
            if(sep != std::string::npos){
                try{
                    od.origin_row = static_cast<int16_t>(std::stoi(oval.substr(0, sep)));
                    od.origin_col = static_cast<int16_t>(std::stoi(oval.substr(sep + 1)));
                }catch(const std::exception &){}
            }
        }

        // Overlay type (60xx,0040).
        od.type = read_text(root, g, 0x0040);

        // Unpack the 1-bit overlay bitmap.
        // Each bit corresponds to one pixel. Bits are packed LSB-first within each byte.
        // See DICOM PS3.5 2026b, Section 8.1.2.
        const auto &raw = data_node->val;
        const uint64_t total_pixels = static_cast<uint64_t>(od.rows) * static_cast<uint64_t>(od.columns);
        const uint64_t required_bytes = (total_pixels + 7u) / 8u;

        if(raw.size() < required_bytes){
            YLOGWARN("Overlay Data for group " << std::hex << g << std::dec
                     << " is too small: expected at least " << required_bytes
                     << " bytes, got " << raw.size());
            continue;
        }

        std::vector<uint8_t> bitmap(total_pixels, 0);
        const auto *bytes = reinterpret_cast<const uint8_t*>(raw.data());
        for(uint64_t i = 0; i < total_pixels; ++i){
            const uint64_t byte_idx = i / 8u;
            const uint64_t bit_idx  = i % 8u;
            bitmap[i] = static_cast<uint8_t>((bytes[byte_idx] >> bit_idx) & 1u);
        }

        result.emplace_back(std::move(od), std::move(bitmap));
    }

    return result;
}


// ============================================================================
// Encapsulated (compressed) pixel data extraction -- stub.
// ============================================================================

std::optional<ExtractedPixelData> extract_encapsulated_pixel_data(const Node &root){
    // This function is a stub. Encapsulated pixel data decoding is not yet implemented.
    //
    // To implement this, the following steps are needed (see the header for full details):
    //
    //   1. Get the PixelDataDesc (transfer syntax, rows, columns, etc.).
    //   2. Locate Pixel Data tag (7FE0,0010) and its raw encapsulated fragments.
    //   3. Parse the Basic Offset Table (first fragment) per DICOM PS3.5 Table A.4-1
    //      to determine per-frame fragment boundaries.
    //   4. For each frame, dispatch the compressed byte stream to the appropriate codec:
    //      - JPEG Baseline/Extended: could use stb_image.h for 8-bit baseline only.
    //      - JPEG Lossless: requires external library (not bundled).
    //      - JPEG 2000: requires external library (not bundled).
    //      - JPEG-LS: requires external library (not bundled).
    //      - RLE Lossless: can be implemented in-house (PackBits variant, DICOM PS3.5 Annex G).
    //      - HTJ2K: requires external library (not bundled).
    //      - JPEG XL: requires external library (not bundled).
    //   5. Assemble decoded frame samples into ExtractedPixelData.
    //
    // For now, log a warning and return std::nullopt.

    auto desc_opt = get_pixel_data_desc(root);
    if(desc_opt){
        YLOGWARN("Encapsulated pixel data extraction is not yet implemented for transfer syntax type "
                 << static_cast<int>(desc_opt->transfer_syntax)
                 << " (photometric interpretation: " << desc_opt->photometric_interpretation << ")");
    }else{
        YLOGWARN("Encapsulated pixel data extraction is not yet implemented"
                 " (pixel data descriptor could not be determined)");
    }

    return std::nullopt;
}


} // namespace DCMA_DICOM
