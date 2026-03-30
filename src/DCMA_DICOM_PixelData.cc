// DCMA_DICOM_PixelData.cc - A part of DICOMautomaton 2026. Written by hal clark.
//
// This file contains routines for extracting and interpreting pixel data from a parsed DICOM
// Node tree, supporting native (uncompressed) pixel data, encapsulated 8-bit baseline JPEG
// pixel data, overlay data, and a composable pixel transformation pipeline.
//
// References:
//   - DICOM PS3.4 2026b, Section N.2:  Softcopy Presentation State Display Pipeline.
//   - DICOM PS3.5 2026b, Section 8:    Encoding of Pixel, Overlay and Waveform Data.
//   - DICOM PS3.5 2026b, Section 8.2:  Native or Encapsulated Format Encoding.


#include <cstdint>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <optional>
#include <stdexcept>
#include <algorithm>

#include "YgorLog.h"
#include "YgorImages.h"
#include "YgorMath.h"

#include "DCMA_DICOM.h"
#include "DCMA_DICOM_PixelData.h"

// Bundled stb_image for JPEG baseline decoding. STB_IMAGE_STATIC ensures all symbols are
// local to this translation unit, avoiding clashes with the copy in STB_Shim.cc.
namespace dcma_stb_px {
    #define STB_IMAGE_STATIC
    #define STB_IMAGE_IMPLEMENTATION
    #include "stbnothings20230607/stb_image.h"
} // namespace dcma_stb_px

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
    // The Data Set is deflated on disk, but once inflated the pixel data is native/uncompressed.
    // For pixel data decoding we therefore classify it as NativeUncompressed.
    // See DICOM PS3.5 2026b, Section A.5.
    if(ts == "1.2.840.10008.1.2.1.99"){
        return TransferSyntaxType::NativeUncompressed;
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

// Helper: read a DS-valued (Decimal String) tag from the tree and return as double.
static std::optional<double> read_DS(const Node &root, uint16_t group, uint16_t tag){
    auto s = read_text(root, group, tag);
    if(s.empty()) return std::nullopt;

    // DS may have leading/trailing whitespace per DICOM PS3.5, VR definition.
    while(!s.empty() && s.front() == ' ') s.erase(s.begin());
    while(!s.empty() && s.back() == ' ') s.pop_back();
    if(s.empty()) return std::nullopt;

    try{
        return std::stod(s);
    }catch(const std::exception &){
        return std::nullopt;
    }
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
// Pixel transformation pipeline stages.
// ============================================================================

std::optional<ModalityLUTParams> get_modality_lut_params(const Node &root){
    auto slope     = read_DS(root, 0x0028, 0x1053);
    auto intercept = read_DS(root, 0x0028, 0x1052);
    if(!slope && !intercept) return std::nullopt;

    ModalityLUTParams p;
    p.rescale_slope     = slope.value_or(1.0);
    p.rescale_intercept = intercept.value_or(0.0);
    return p;
}

void apply_modality_lut(planar_image<float,double> &img, const ModalityLUTParams &params){
    for(int64_t row = 0; row < img.rows; ++row){
        for(int64_t col = 0; col < img.columns; ++col){
            for(int64_t chn = 0; chn < img.channels; ++chn){
                auto &v = img.reference(row, col, chn);
                v = static_cast<float>(params.rescale_slope * static_cast<double>(v)
                                       + params.rescale_intercept);
            }
        }
    }
}


std::optional<VOILUTParams> get_voi_lut_params(const Node &root){
    auto wc = read_DS(root, 0x0028, 0x1050);
    auto ww = read_DS(root, 0x0028, 0x1051);

    // In DICOM, Window Center and Window Width form a paired parameter set.
    // Only return parameters if BOTH are present and the width is valid.
    if(!wc || !ww){
        return std::nullopt;
    }

    // Guard against non-positive window widths, which would lead to invalid windowing.
    if(*ww <= 0.0){
        return std::nullopt;
    }

    VOILUTParams p;
    p.window_center = *wc;
    p.window_width  = *ww;
    return p;
}

void apply_voi_lut(planar_image<float,double> &img, const VOILUTParams &params,
                   double out_min, double out_max){
    // Linear exact windowing per DICOM PS3.3, C.11.2.1.2.
    // The formula maps [c - (w-1)/2, c + (w-1)/2] to [out_min, out_max].
    const double c = params.window_center;
    const double w = params.window_width;
    const double c_adj = c - 0.5;
    const double half_wm1 = (w - 1.0) / 2.0;
    const double low  = c_adj - half_wm1;
    const double high = c_adj + half_wm1;
    const double range = out_max - out_min;
    const double wm1 = w - 1.0;
    const double inv_wm1 = (wm1 != 0.0) ? (1.0 / wm1) : 0.0;

    for(int64_t row = 0; row < img.rows; ++row){
        for(int64_t col = 0; col < img.columns; ++col){
            for(int64_t chn = 0; chn < img.channels; ++chn){
                auto &v = img.reference(row, col, chn);
                const double x = static_cast<double>(v);
                double y = 0.0;
                if(x <= low){
                    y = out_min;
                }else if(x > high){
                    y = out_max;
                }else{
                    y = ((x - c_adj) * inv_wm1 + 0.5) * range + out_min;
                }
                v = static_cast<float>(y);
            }
        }
    }
}


PresentationLUTShape get_presentation_lut_shape(const Node &root){
    // Presentation LUT Shape (2050,0020).
    auto s = read_text(root, 0x2050, 0x0020);
    if(s == "INVERSE") return PresentationLUTShape::Inverse;
    return PresentationLUTShape::Identity;
}

void apply_presentation_lut(planar_image<float,double> &img, PresentationLUTShape shape,
                            double max_output_val){
    if(shape == PresentationLUTShape::Identity) return;

    // Inverse: output = max_output_val - input.
    for(int64_t row = 0; row < img.rows; ++row){
        for(int64_t col = 0; col < img.columns; ++col){
            for(int64_t chn = 0; chn < img.channels; ++chn){
                auto &v = img.reference(row, col, chn);
                v = static_cast<float>(max_output_val - static_cast<double>(v));
            }
        }
    }
}


bool convert_photometric_to_rgb(planar_image<float,double> &img,
                                const std::string &from_photometric,
                                const Node *root){
    // YBR_FULL or YBR_FULL_422 → RGB.
    // DICOM PS3.3, C.7.6.3.1.2:
    //   R = Y + 1.402 * (Cr - 128)
    //   G = Y - 0.344136 * (Cb - 128) - 0.714136 * (Cr - 128)
    //   B = Y + 1.772 * (Cb - 128)
    if(from_photometric == "YBR_FULL" || from_photometric == "YBR_FULL_422"){
        if(img.channels != 3) return false;

        for(int64_t row = 0; row < img.rows; ++row){
            for(int64_t col = 0; col < img.columns; ++col){
                const double Y  = static_cast<double>(img.value(row, col, 0));
                const double Cb = static_cast<double>(img.value(row, col, 1));
                const double Cr = static_cast<double>(img.value(row, col, 2));

                const double R = Y + 1.402   * (Cr - 128.0);
                const double G = Y - 0.344136 * (Cb - 128.0) - 0.714136 * (Cr - 128.0);
                const double B = Y + 1.772   * (Cb - 128.0);

                img.reference(row, col, 0) = static_cast<float>(std::clamp(R, 0.0, 255.0));
                img.reference(row, col, 1) = static_cast<float>(std::clamp(G, 0.0, 255.0));
                img.reference(row, col, 2) = static_cast<float>(std::clamp(B, 0.0, 255.0));
            }
        }
        return true;
    }

    // PALETTE COLOR → RGB.
    // Requires palette LUT descriptor and data from the DICOM tree.
    // Descriptor tags: (0028,1101), (0028,1102), (0028,1103) — each is US[3].
    // Data tags:       (0028,1201), (0028,1202), (0028,1203) — each is OW.
    if(from_photometric == "PALETTE COLOR"){
        if(root == nullptr) return false;
        if(img.channels != 1) return false;

        // Read LUT data for each colour channel. The raw value of the data tag contains
        // packed uint16_t LUT entries (one per 2 bytes, little-endian).
        auto read_lut_data = [&](uint16_t data_tag) -> std::vector<uint16_t> {
            const auto *n = root->find(0x0028, data_tag);
            if(n == nullptr) return {};
            const auto &raw = n->val;
            const size_t count = raw.size() / 2u;
            std::vector<uint16_t> lut(count);
            for(size_t i = 0; i < count; ++i){
                uint16_t v = 0;
                std::memcpy(&v, raw.data() + i * 2u, 2);
                lut[i] = v;
            }
            return lut;
        };

        auto red_lut   = read_lut_data(0x1201);
        auto green_lut = read_lut_data(0x1202);
        auto blue_lut  = read_lut_data(0x1203);

        if(red_lut.empty() || green_lut.empty() || blue_lut.empty()) return false;

        // Determine bit depth of LUT entries. If the max entry value exceeds 255,
        // entries are 16-bit and need to be scaled to 8-bit. Otherwise 8-bit entries
        // are stored in the low byte of each uint16_t.
        const uint16_t max_entry = std::max({
            *std::max_element(red_lut.begin(), red_lut.end()),
            *std::max_element(green_lut.begin(), green_lut.end()),
            *std::max_element(blue_lut.begin(), blue_lut.end())});
        const bool is_16bit = (max_entry > 255u);

        const size_t lut_size = std::min({red_lut.size(), green_lut.size(), blue_lut.size()});

        // Expand from 1-channel (index) to 3-channel (RGB).
        // Create a new image with 3 channels and copy the converted pixel values.
        planar_image<float,double> rgb_img;
        rgb_img.init_buffer(img.rows, img.columns, 3);
        rgb_img.init_spatial(img.pxl_dx, img.pxl_dy, img.pxl_dz,
                             img.anchor, img.offset);
        rgb_img.init_orientation(img.row_unit, img.col_unit);
        rgb_img.metadata = img.metadata;

        for(int64_t row = 0; row < img.rows; ++row){
            for(int64_t col = 0; col < img.columns; ++col){
                const size_t idx = static_cast<size_t>(std::clamp(
                    static_cast<double>(img.value(row, col, 0)),
                    0.0,
                    static_cast<double>(lut_size - 1u)));

                double r = static_cast<double>(red_lut[idx]);
                double g = static_cast<double>(green_lut[idx]);
                double b = static_cast<double>(blue_lut[idx]);

                if(is_16bit){
                    r = r * 255.0 / 65535.0;
                    g = g * 255.0 / 65535.0;
                    b = b * 255.0 / 65535.0;
                }

                rgb_img.reference(row, col, 0) = static_cast<float>(r);
                rgb_img.reference(row, col, 1) = static_cast<float>(g);
                rgb_img.reference(row, col, 2) = static_cast<float>(b);
            }
        }

        img = std::move(rgb_img);
        return true;
    }

    // RGB and MONOCHROME2 are already in the expected form — no conversion needed.
    if(from_photometric == "RGB" || from_photometric == "MONOCHROME2"){
        return true;
    }

    return false;
}


// ============================================================================
// Helper: build a planar_image from a flat vector of sample values.
// ============================================================================

// Populate a planar_image_collection from unpacked samples and a PixelDataDesc.
// Handles multi-frame images (one image per frame) and planar → interleaved rearrangement.
// Default spatial parameters are used (1mm spacing, identity orientation).
static std::optional<planar_image_collection<float,double>>
samples_to_images(const std::vector<double> &samples,
                  const PixelDataDesc &desc){

    const int64_t rows = static_cast<int64_t>(desc.rows);
    const int64_t cols = static_cast<int64_t>(desc.columns);
    const int64_t chns = static_cast<int64_t>(desc.samples_per_pixel);
    const uint32_t nf  = desc.number_of_frames;
    const uint64_t pixels_per_frame = static_cast<uint64_t>(rows) * static_cast<uint64_t>(cols);
    const uint64_t samples_per_frame = pixels_per_frame * static_cast<uint64_t>(chns);

    if(samples.size() != static_cast<size_t>(nf) * samples_per_frame){
        YLOGWARN("Sample count mismatch: expected "
                 << (static_cast<uint64_t>(nf) * samples_per_frame)
                 << ", got " << samples.size());
        return std::nullopt;
    }

    planar_image_collection<float,double> pic;

    const double pxl_dx = 1.0;
    const double pxl_dy = 1.0;
    const double pxl_dz = 1.0;
    const vec3<double> anchor(0.0, 0.0, 0.0);
    const vec3<double> offset(0.0, 0.0, 0.0);
    const vec3<double> row_unit(1.0, 0.0, 0.0);
    const vec3<double> col_unit(0.0, 1.0, 0.0);

    for(uint32_t f = 0; f < nf; ++f){
        const auto frame_offset = static_cast<size_t>(f) * samples_per_frame;

        pic.images.emplace_back();
        auto &img = pic.images.back();
        img.init_buffer(rows, cols, chns);
        img.init_spatial(pxl_dx, pxl_dy, pxl_dz, anchor, offset);
        img.init_orientation(row_unit, col_unit);

        if(desc.planar_configuration == 0 || chns == 1){
            // Interleaved: R0 G0 B0 R1 G1 B1 ...
            for(int64_t row = 0; row < rows; ++row){
                for(int64_t col = 0; col < cols; ++col){
                    for(int64_t chn = 0; chn < chns; ++chn){
                        const auto idx = frame_offset
                                       + static_cast<size_t>((row * cols + col) * chns + chn);
                        img.reference(row, col, chn) = static_cast<float>(samples[idx]);
                    }
                }
            }
        }else{
            // Planar: R0 R1 ... G0 G1 ... B0 B1 ...
            // Rearrange to interleaved for planar_image.
            for(int64_t chn = 0; chn < chns; ++chn){
                const auto plane_offset = frame_offset
                                        + static_cast<size_t>(chn) * pixels_per_frame;
                for(int64_t row = 0; row < rows; ++row){
                    for(int64_t col = 0; col < cols; ++col){
                        const auto idx = plane_offset
                                       + static_cast<size_t>(row * cols + col);
                        img.reference(row, col, chn) = static_cast<float>(samples[idx]);
                    }
                }
            }
        }
    }

    return pic;
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


std::optional<planar_image_collection<float,double>> extract_native_pixel_data(const Node &root){
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

    std::vector<double> samples;

    if(is_double_float){
        // Double Float Pixel Data: each sample is a 64-bit IEEE 754 double.
        // DICOM PS3.5 2026b, Section 8.4.
        const uint64_t expected_bytes = total_samples * 8u;
        if(raw.size() < expected_bytes){
            YLOGWARN("Double Float Pixel Data buffer too small: expected "
                     << expected_bytes << " bytes, got " << raw.size());
            return std::nullopt;
        }
        samples.resize(total_samples);
        for(uint64_t i = 0; i < total_samples; ++i){
            double v = 0.0;
            std::memcpy(&v, raw.data() + i * 8u, 8);
            samples[i] = v;
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
        samples.resize(total_samples);
        for(uint64_t i = 0; i < total_samples; ++i){
            float v = 0.0f;
            std::memcpy(&v, raw.data() + i * 4u, 4);
            samples[i] = static_cast<double>(v);
        }
    }else{
        // General Pixel Data (7FE0,0010): unpack according to Bits Allocated / Bits Stored / High Bit.
        samples = unpack_native_samples(raw, desc, total_samples);
        if(samples.size() != total_samples){
            YLOGWARN("Unpacked native Pixel Data sample count mismatch: expected "
                     << total_samples << ", got " << samples.size());
            return std::nullopt;
        }
    }

    return samples_to_images(samples, desc);
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
// Encapsulated (compressed) pixel data extraction.
// ============================================================================

// Find the position of the first JPEG SOI marker (0xFF 0xD8) in a byte buffer.
// Returns the offset from the start, or std::string::npos if not found.
static size_t find_jpeg_soi(const std::string &data){
    for(size_t i = 0; i + 1 < data.size(); ++i){
        if(static_cast<uint8_t>(data[i]) == 0xFF
        && static_cast<uint8_t>(data[i + 1]) == 0xD8){
            return i;
        }
    }
    return std::string::npos;
}

std::optional<planar_image_collection<float,double>> extract_encapsulated_pixel_data(const Node &root){
    auto desc_opt = get_pixel_data_desc(root);
    if(!desc_opt){
        YLOGWARN("Encapsulated pixel data extraction failed: pixel data descriptor could not be determined");
        return std::nullopt;
    }
    const auto &desc = *desc_opt;

    // Currently only JPEG baseline 8-bit is supported.
    if(desc.transfer_syntax != TransferSyntaxType::EncapsulatedJPEG){
        YLOGWARN("Encapsulated pixel data extraction is not yet implemented for transfer syntax type "
                 << static_cast<int>(desc.transfer_syntax));
        return std::nullopt;
    }

    // Verify 8-bit baseline requirements for stb_image.
    // stb_image only supports 8-bit baseline JPEG (sequential DCT, Huffman-coded).
    // JPEG Extended (TS 1.2.840.10008.1.2.4.51) can use 12-bit samples.
    // JPEG Lossless (TS 1.2.840.10008.1.2.4.57, .70) is not DCT-based.
    const auto ts_str = read_text(root, 0x0002, 0x0010);
    const auto ts_stripped = strip_ts_padding(ts_str);
    if(ts_stripped == "1.2.840.10008.1.2.4.57"
    || ts_stripped == "1.2.840.10008.1.2.4.70"){
        YLOGWARN("JPEG Lossless is not supported by the bundled JPEG decoder (stb_image)");
        return std::nullopt;
    }
    if(desc.bits_allocated != 8 || desc.bits_stored != 8){
        YLOGWARN("Only 8-bit JPEG baseline is supported by the bundled JPEG decoder (stb_image);"
                 " BitsAllocated=" << desc.bits_allocated << ", BitsStored=" << desc.bits_stored);
        return std::nullopt;
    }

    // Multi-frame encapsulated images are not yet supported.
    if(desc.number_of_frames > 1){
        YLOGWARN("Multi-frame encapsulated pixel data is not yet supported");
        return std::nullopt;
    }

    // Locate the Pixel Data tag (7FE0,0010).
    const Node *pd_node = root.find(0x7FE0, 0x0010);
    if(pd_node == nullptr){
        YLOGWARN("No Pixel Data tag (7FE0,0010) found for encapsulated data");
        return std::nullopt;
    }

    const auto &raw = pd_node->val;
    if(raw.size() < 4){
        YLOGWARN("Pixel Data tag too small for encapsulated JPEG data");
        return std::nullopt;
    }

    // The raw value may contain a Basic Offset Table prepended to the JPEG bitstream
    // (see read_encapsulated_data in DCMA_DICOM.cc). Prefer to skip the BOT by using
    // its item length, and only scan for the JPEG SOI marker in the subsequent data.
    std::size_t search_offset = 0U;
    if(raw.size() >= 8U){
        const auto *data = reinterpret_cast<const unsigned char*>(raw.data());
        // Check for an Item tag (FFFE,E000) marking the Basic Offset Table.
        if(data[0] == 0xFE && data[1] == 0xFF && data[2] == 0x00 && data[3] == 0xE0){
            const std::uint32_t bot_len =
                static_cast<std::uint32_t>(data[4]) |
                (static_cast<std::uint32_t>(data[5]) << 8) |
                (static_cast<std::uint32_t>(data[6]) << 16) |
                (static_cast<std::uint32_t>(data[7]) << 24);
            const std::size_t bot_item_end = 8U + static_cast<std::size_t>(bot_len);
            // Only adjust the search offset if the computed end lies within the buffer.
            if(bot_item_end < raw.size()){
                search_offset = bot_item_end;
            }
        }
    }

    std::string jpeg_search_region;
    if(search_offset == 0U){
        jpeg_search_region.assign(raw.begin(), raw.end());
    } else {
        jpeg_search_region.assign(raw.begin() + static_cast<std::ptrdiff_t>(search_offset), raw.end());
    }

    const auto sub_soi_pos = find_jpeg_soi(jpeg_search_region);
    if(sub_soi_pos == std::string::npos){
        YLOGWARN("No JPEG SOI marker (0xFF 0xD8) found in encapsulated pixel data");
        return std::nullopt;
    }

    const auto soi_pos = search_offset + sub_soi_pos;
    const auto *jpeg_data = reinterpret_cast<const dcma_stb_px::stbi_uc*>(raw.data() + soi_pos);
    const int jpeg_len = static_cast<int>(raw.size() - soi_pos);

    int width = 0;
    int height = 0;
    int channels_actual = 0;
    const int channels_requested = 0; // Keep original channel count.

    unsigned char *pixels = dcma_stb_px::stbi_load_from_memory(
        jpeg_data, jpeg_len, &width, &height, &channels_actual, channels_requested);

    if(pixels == nullptr){
        YLOGWARN("stb_image JPEG decoding failed: " << dcma_stb_px::stbi_failure_reason());
        return std::nullopt;
    }

    // Validate decoded dimensions against DICOM descriptor.
    if(width != static_cast<int>(desc.columns) || height != static_cast<int>(desc.rows)){
        YLOGWARN("JPEG decoded dimensions (" << width << "x" << height
                 << ") do not match DICOM descriptor (" << desc.columns << "x" << desc.rows << ")");
        dcma_stb_px::stbi_image_free(pixels);
        return std::nullopt;
    }

    const int64_t rows = static_cast<int64_t>(height);
    const int64_t cols = static_cast<int64_t>(width);
    const int64_t chns = static_cast<int64_t>(channels_actual);

    planar_image_collection<float,double> pic;
    pic.images.emplace_back();
    auto &img = pic.images.back();
    img.init_buffer(rows, cols, chns);
    img.init_spatial(1.0, 1.0, 1.0,
                     vec3<double>(0.0, 0.0, 0.0),
                     vec3<double>(0.0, 0.0, 0.0));
    img.init_orientation(vec3<double>(1.0, 0.0, 0.0),
                         vec3<double>(0.0, 1.0, 0.0));

    const unsigned char *p = pixels;
    for(int64_t row = 0; row < rows; ++row){
        for(int64_t col = 0; col < cols; ++col){
            for(int64_t chn = 0; chn < chns; ++chn){
                img.reference(row, col, chn) = static_cast<float>(*p);
                ++p;
            }
        }
    }

    dcma_stb_px::stbi_image_free(pixels);
    return pic;
}


} // namespace DCMA_DICOM
