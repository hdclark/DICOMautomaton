// DCMA_DICOM_Loader_Tests.cc - A part of DICOMautomaton 2026. Written by hal clark.
//
// This file contains unit tests for the DICOM image loaders defined in
// DCMA_DICOM_Loader.{h,cc}. Tests are separated into their own file because
// DCMA_DICOM_Loader_obj is linked into shared libraries which don't include
// doctest implementation.

#include <cstdint>
#include <cstring>
#include <cmath>
#include <string>
#include <memory>

#include "doctest20251212/doctest.h"

#include "YgorImages.h"
#include "YgorMath.h"

#include "DCMA_DICOM.h"
#include "DCMA_DICOM_PixelData.h"
#include "DCMA_DICOM_Loader.h"
#include "Structs.h"


// ============================================================================
// Helpers for building synthetic DICOM Node trees.
// ============================================================================

// Build a minimal single-frame image DICOM tree suitable for loader tests.
// The pixel data is a 2x2, 16-bit unsigned, MONOCHROME2, native (ELE) image
// with the given pixel values. Spatial tags are populated with the supplied
// parameters, and the Modality LUT tags (RescaleSlope/Intercept) are optionally
// added.
static DCMA_DICOM::Node create_loader_test_image(
        const std::string &modality,
        double pxl_dy, double pxl_dx,     // PixelSpacing: row\col spacing.
        double slice_thickness,
        const vec3<double> &position,      // ImagePositionPatient.
        const vec3<double> &row_dir,       // ImageOrientationPatient row direction.
        const vec3<double> &col_dir,       // ImageOrientationPatient col direction.
        const std::string &pixel_data,     // Raw pixel bytes (OW).
        uint16_t rows = 2,
        uint16_t cols = 2,
        uint16_t bits_alloc = 16,
        uint16_t bits_stored = 16,
        uint16_t high_bit = 15,
        uint16_t pixel_rep = 0){

    DCMA_DICOM::Node root;

    // File Meta Information (group 0x0002, always ELE).
    root.emplace_child_node({{0x0002, 0x0001}, "OB", std::string("\x00\x01", 2)});
    root.emplace_child_node({{0x0002, 0x0002}, "UI", "1.2.840.10008.5.1.4.1.1.2"});
    root.emplace_child_node({{0x0002, 0x0003}, "UI", "1.2.3.4.5.6.7.8.9"});
    root.emplace_child_node({{0x0002, 0x0010}, "UI", "1.2.840.10008.1.2.1"}); // ELE.
    root.emplace_child_node({{0x0002, 0x0012}, "UI", "1.2.3.4.5"});

    // SOP Common.
    root.emplace_child_node({{0x0008, 0x0016}, "UI", "1.2.840.10008.5.1.4.1.1.2"});
    root.emplace_child_node({{0x0008, 0x0018}, "UI", "1.2.3.4.5.6.7.8.9"});

    // General Series.
    root.emplace_child_node({{0x0008, 0x0060}, "CS", modality});

    // Patient.
    root.emplace_child_node({{0x0010, 0x0010}, "PN", "TEST^PATIENT"});

    // Image Plane Module.
    root.emplace_child_node({{0x0020, 0x0032}, "DS",
        std::to_string(position.x) + "\\" + std::to_string(position.y) + "\\" + std::to_string(position.z)});
    root.emplace_child_node({{0x0020, 0x0037}, "DS",
        std::to_string(row_dir.x) + "\\" + std::to_string(row_dir.y) + "\\" + std::to_string(row_dir.z) + "\\"
      + std::to_string(col_dir.x) + "\\" + std::to_string(col_dir.y) + "\\" + std::to_string(col_dir.z)});
    root.emplace_child_node({{0x0028, 0x0030}, "DS",
        std::to_string(pxl_dy) + "\\" + std::to_string(pxl_dx)});
    root.emplace_child_node({{0x0018, 0x0050}, "DS", std::to_string(slice_thickness)});

    // Image Pixel Module.
    root.emplace_child_node({{0x0028, 0x0002}, "US", "1"});
    root.emplace_child_node({{0x0028, 0x0004}, "CS", "MONOCHROME2"});
    root.emplace_child_node({{0x0028, 0x0010}, "US", std::to_string(rows)});
    root.emplace_child_node({{0x0028, 0x0011}, "US", std::to_string(cols)});
    root.emplace_child_node({{0x0028, 0x0100}, "US", std::to_string(bits_alloc)});
    root.emplace_child_node({{0x0028, 0x0101}, "US", std::to_string(bits_stored)});
    root.emplace_child_node({{0x0028, 0x0102}, "US", std::to_string(high_bit)});
    root.emplace_child_node({{0x0028, 0x0103}, "US", std::to_string(pixel_rep)});

    // Pixel Data (7FE0,0010).
    root.emplace_child_node({{0x7FE0, 0x0010}, "OW", pixel_data});

    return root;
}

// Build a 2x2 pixel data buffer from four uint16_t values.
static std::string make_pixel_data_2x2(uint16_t a, uint16_t b, uint16_t c, uint16_t d){
    std::string raw(8, '\0');
    std::memcpy(&raw[0], &a, 2);
    std::memcpy(&raw[2], &b, 2);
    std::memcpy(&raw[4], &c, 2);
    std::memcpy(&raw[6], &d, 2);
    return raw;
}


// ============================================================================
// CT image loader tests.
// ============================================================================

TEST_CASE("DCMA_DICOM_Loader load_ct_images sets spatial parameters from DICOM tags"){
    const auto raw = make_pixel_data_2x2(100, 200, 300, 400);
    const vec3<double> pos(10.0, 20.0, 30.0);
    const vec3<double> row_dir(1.0, 0.0, 0.0);
    const vec3<double> col_dir(0.0, 1.0, 0.0);

    auto root = create_loader_test_image("CT", 0.5, 0.5, 2.5, pos, row_dir, col_dir, raw);

    auto result = DCMA_DICOM::load_ct_images(root);
    REQUIRE(result != nullptr);
    REQUIRE(result->imagecoll.images.size() == 1);

    const auto &img = result->imagecoll.images.front();

    // Verify pixel spacing (pxl_dx / pxl_dy).
    CHECK(img.pxl_dx == doctest::Approx(0.5));
    CHECK(img.pxl_dy == doctest::Approx(0.5));
    CHECK(img.pxl_dz == doctest::Approx(2.5));

    // Verify position.
    CHECK(img.offset.x == doctest::Approx(10.0));
    CHECK(img.offset.y == doctest::Approx(20.0));
    CHECK(img.offset.z == doctest::Approx(30.0));

    // Verify orientation.
    CHECK(img.row_unit.x == doctest::Approx(1.0));
    CHECK(img.row_unit.y == doctest::Approx(0.0));
    CHECK(img.row_unit.z == doctest::Approx(0.0));
    CHECK(img.col_unit.x == doctest::Approx(0.0));
    CHECK(img.col_unit.y == doctest::Approx(1.0));
    CHECK(img.col_unit.z == doctest::Approx(0.0));
}


TEST_CASE("DCMA_DICOM_Loader load_ct_images applies Modality LUT (RescaleSlope/Intercept)"){
    // Stored value = 1000. Slope=1, Intercept=-1024. Expected HU = -24.
    const auto raw = make_pixel_data_2x2(1000, 1000, 1000, 1000);
    const vec3<double> pos(0.0, 0.0, 0.0);
    const vec3<double> row_dir(1.0, 0.0, 0.0);
    const vec3<double> col_dir(0.0, 1.0, 0.0);

    auto root = create_loader_test_image("CT", 1.0, 1.0, 1.0, pos, row_dir, col_dir, raw);
    root.emplace_child_node({{0x0028, 0x1053}, "DS", "1"});     // RescaleSlope.
    root.emplace_child_node({{0x0028, 0x1052}, "DS", "-1024"});  // RescaleIntercept.

    auto result = DCMA_DICOM::load_ct_images(root);
    REQUIRE(result != nullptr);
    REQUIRE(result->imagecoll.images.size() == 1);

    const auto &img = result->imagecoll.images.front();
    CHECK(img.value(0, 0, 0) == doctest::Approx(-24.0f));
    CHECK(img.value(0, 1, 0) == doctest::Approx(-24.0f));
    CHECK(img.value(1, 0, 0) == doctest::Approx(-24.0f));
    CHECK(img.value(1, 1, 0) == doctest::Approx(-24.0f));
}


TEST_CASE("DCMA_DICOM_Loader load_ct_images populates modality metadata"){
    const auto raw = make_pixel_data_2x2(0, 0, 0, 0);
    const vec3<double> pos(0.0, 0.0, 0.0);
    const vec3<double> row_dir(1.0, 0.0, 0.0);
    const vec3<double> col_dir(0.0, 1.0, 0.0);

    auto root = create_loader_test_image("CT", 1.0, 1.0, 1.0, pos, row_dir, col_dir, raw);
    root.emplace_child_node({{0x0018, 0x0060}, "DS", "120"});   // KVP.

    auto result = DCMA_DICOM::load_ct_images(root);
    REQUIRE(result != nullptr);
    REQUIRE(result->imagecoll.images.size() == 1);

    const auto &img = result->imagecoll.images.front();
    CHECK(img.metadata.count("Modality") == 1);
    CHECK(img.metadata.at("Modality") == "CT");
    CHECK(img.metadata.count("KVP") == 1);
    CHECK(img.metadata.at("KVP") == "120");
    CHECK(img.metadata.count("PatientsName") == 1);
    CHECK(img.metadata.at("PatientsName") == "TEST^PATIENT");
}


TEST_CASE("DCMA_DICOM_Loader load_ct_images returns nullptr when pixel data is missing"){
    DCMA_DICOM::Node root;
    root.emplace_child_node({{0x0002, 0x0010}, "UI", "1.2.840.10008.1.2.1"});
    root.emplace_child_node({{0x0008, 0x0060}, "CS", "CT"});
    // No pixel data tags.

    auto result = DCMA_DICOM::load_ct_images(root);
    CHECK(result == nullptr);
}


// ============================================================================
// MR image loader tests.
// ============================================================================

TEST_CASE("DCMA_DICOM_Loader load_mr_images sets spatial params and MR metadata"){
    const auto raw = make_pixel_data_2x2(500, 600, 700, 800);
    const vec3<double> pos(5.0, 10.0, 15.0);
    const vec3<double> row_dir(1.0, 0.0, 0.0);
    const vec3<double> col_dir(0.0, 1.0, 0.0);

    auto root = create_loader_test_image("MR", 0.8, 0.8, 3.0, pos, row_dir, col_dir, raw);
    root.emplace_child_node({{0x0018, 0x0081}, "DS", "25.5"});   // EchoTime.
    root.emplace_child_node({{0x0018, 0x0080}, "DS", "500"});    // RepetitionTime.

    auto result = DCMA_DICOM::load_mr_images(root);
    REQUIRE(result != nullptr);
    REQUIRE(result->imagecoll.images.size() == 1);

    const auto &img = result->imagecoll.images.front();

    // Spatial params.
    CHECK(img.pxl_dx == doctest::Approx(0.8));
    CHECK(img.pxl_dy == doctest::Approx(0.8));
    CHECK(img.pxl_dz == doctest::Approx(3.0));
    CHECK(img.offset.x == doctest::Approx(5.0));
    CHECK(img.offset.y == doctest::Approx(10.0));
    CHECK(img.offset.z == doctest::Approx(15.0));

    // MR-specific metadata.
    CHECK(img.metadata.count("EchoTime") == 1);
    CHECK(img.metadata.at("EchoTime") == "25.5");
    CHECK(img.metadata.count("RepetitionTime") == 1);
    CHECK(img.metadata.at("RepetitionTime") == "500");
}


// ============================================================================
// RTDOSE image loader tests.
// ============================================================================

TEST_CASE("DCMA_DICOM_Loader load_dose_images applies DoseGridScaling and GFOV positioning"){
    // 2-frame RTDOSE: 2x2 pixels per frame, 16-bit unsigned.
    // Frame 0 pixels: 100, 200, 300, 400.
    // Frame 1 pixels: 500, 600, 700, 800.
    std::string raw(16, '\0');
    {
        uint16_t vals[] = {100, 200, 300, 400, 500, 600, 700, 800};
        std::memcpy(&raw[0], vals, 16);
    }

    const vec3<double> pos(0.0, 0.0, 0.0);
    const vec3<double> row_dir(1.0, 0.0, 0.0);
    const vec3<double> col_dir(0.0, 1.0, 0.0);

    auto root = create_loader_test_image("RTDOSE", 2.5, 2.5, 2.5, pos, row_dir, col_dir, raw);

    // NumberOfFrames.
    root.emplace_child_node({{0x0028, 0x0008}, "IS", "2"});

    // GridFrameOffsetVector: frame 0 at offset 0, frame 1 at offset 5.0.
    root.emplace_child_node({{0x3004, 0x000C}, "DS", "0\\5.0"});

    // DoseGridScaling.
    root.emplace_child_node({{0x3004, 0x000E}, "DS", "0.001"});

    auto result = DCMA_DICOM::load_dose_images(root);
    REQUIRE(result != nullptr);
    REQUIRE(result->imagecoll.images.size() == 2);

    // Frame 0: position should be at (0,0,0), pixel values scaled by 0.001.
    const auto &f0 = result->imagecoll.images.front();
    CHECK(f0.offset.x == doctest::Approx(0.0));
    CHECK(f0.offset.y == doctest::Approx(0.0));
    CHECK(f0.offset.z == doctest::Approx(0.0));
    CHECK(f0.value(0, 0, 0) == doctest::Approx(100.0f * 0.001f));
    CHECK(f0.value(0, 1, 0) == doctest::Approx(200.0f * 0.001f));
    CHECK(f0.value(1, 0, 0) == doctest::Approx(300.0f * 0.001f));
    CHECK(f0.value(1, 1, 0) == doctest::Approx(400.0f * 0.001f));

    // Frame 1: position should be offset by 5.0 along the stack direction (0,0,1).
    const auto &f1 = result->imagecoll.images.back();
    CHECK(f1.offset.x == doctest::Approx(0.0));
    CHECK(f1.offset.y == doctest::Approx(0.0));
    CHECK(f1.offset.z == doctest::Approx(5.0));
    CHECK(f1.value(0, 0, 0) == doctest::Approx(500.0f * 0.001f));

    // Verify per-frame metadata.
    CHECK(f0.metadata.count("GridFrameOffset") == 1);
    CHECK(f0.metadata.at("Frame") == "0");
    CHECK(f1.metadata.at("Frame") == "1");

    // Verify thickness derived from GFOV deltas.
    CHECK(f0.pxl_dz == doctest::Approx(5.0));
    CHECK(f1.pxl_dz == doctest::Approx(5.0));
}


TEST_CASE("DCMA_DICOM_Loader load_dose_images throws when GFOV size mismatches frame count"){
    // 1-frame image but GFOV has 2 entries.
    const auto raw = make_pixel_data_2x2(100, 200, 300, 400);
    const vec3<double> pos(0.0, 0.0, 0.0);
    const vec3<double> row_dir(1.0, 0.0, 0.0);
    const vec3<double> col_dir(0.0, 1.0, 0.0);

    auto root = create_loader_test_image("RTDOSE", 1.0, 1.0, 1.0, pos, row_dir, col_dir, raw);
    // No NumberOfFrames → defaults to 1 frame. But GFOV has 2 entries.
    root.emplace_child_node({{0x3004, 0x000C}, "DS", "0\\5.0"});
    root.emplace_child_node({{0x3004, 0x000E}, "DS", "0.001"});

    CHECK_THROWS_AS(DCMA_DICOM::load_dose_images(root), std::runtime_error);
}


TEST_CASE("DCMA_DICOM_Loader load_dose_images populates RTDOSE metadata"){
    const auto raw = make_pixel_data_2x2(0, 0, 0, 0);
    const vec3<double> pos(0.0, 0.0, 0.0);
    const vec3<double> row_dir(1.0, 0.0, 0.0);
    const vec3<double> col_dir(0.0, 1.0, 0.0);

    auto root = create_loader_test_image("RTDOSE", 1.0, 1.0, 1.0, pos, row_dir, col_dir, raw);
    root.emplace_child_node({{0x3004, 0x0002}, "CS", "GY"});       // DoseUnits.
    root.emplace_child_node({{0x3004, 0x0004}, "CS", "PHYSICAL"});  // DoseType.
    root.emplace_child_node({{0x3004, 0x000E}, "DS", "0.001"});     // DoseGridScaling.

    auto result = DCMA_DICOM::load_dose_images(root);
    REQUIRE(result != nullptr);
    REQUIRE(result->imagecoll.images.size() == 1);

    const auto &img = result->imagecoll.images.front();
    CHECK(img.metadata.count("DoseUnits") == 1);
    CHECK(img.metadata.at("DoseUnits") == "GY");
    CHECK(img.metadata.count("DoseType") == 1);
    CHECK(img.metadata.at("DoseType") == "PHYSICAL");
    CHECK(img.metadata.count("DoseGridScaling") == 1);
    CHECK(img.metadata.at("DoseGridScaling") == "0.001");
}


// ============================================================================
// Multi-frame per-frame spatial extraction tests (Enhanced CT/MR).
// ============================================================================

TEST_CASE("DCMA_DICOM_Loader load_ct_images extracts per-frame spatial from Functional Groups"){
    // Build a 2-frame image with Per-Frame Functional Groups Sequence (5200,9230)
    // providing per-frame ImagePositionPatient.
    //
    // Frame 0: position (0,0,0)
    // Frame 1: position (0,0,5)
    std::string raw(16, '\0'); // 2 frames × 2×2 × 2 bytes = 16 bytes.
    {
        uint16_t vals[] = {10, 20, 30, 40, 50, 60, 70, 80};
        std::memcpy(&raw[0], vals, 16);
    }

    const vec3<double> pos(0.0, 0.0, 0.0);
    const vec3<double> row_dir(1.0, 0.0, 0.0);
    const vec3<double> col_dir(0.0, 1.0, 0.0);

    auto root = create_loader_test_image("CT", 1.0, 1.0, 2.5, pos, row_dir, col_dir, raw);
    root.emplace_child_node({{0x0028, 0x0008}, "IS", "2"}); // NumberOfFrames.

    // Shared Functional Groups Sequence (5200,9229): provides shared orientation.
    {
        auto *sfg_seq = root.emplace_child_node({{0x5200, 0x9229}, "SQ", ""});
        DCMA_DICOM::Node sfg_item;
        sfg_item.key = {0x5200, 0x9229};
        sfg_item.VR = "MULTI";

        // Plane Orientation Sequence (0020,9116).
        auto *orient_seq = sfg_item.emplace_child_node({{0x0020, 0x9116}, "SQ", ""});
        DCMA_DICOM::Node orient_item;
        orient_item.key = {0x0020, 0x9116};
        orient_item.VR = "MULTI";
        orient_item.emplace_child_node({{0x0020, 0x0037}, "DS", "1\\0\\0\\0\\1\\0"});
        orient_seq->emplace_child_node(std::move(orient_item));

        // Pixel Measures Sequence (0028,9110).
        auto *measures_seq = sfg_item.emplace_child_node({{0x0028, 0x9110}, "SQ", ""});
        DCMA_DICOM::Node measures_item;
        measures_item.key = {0x0028, 0x9110};
        measures_item.VR = "MULTI";
        measures_item.emplace_child_node({{0x0028, 0x0030}, "DS", "1.5\\1.5"});
        measures_item.emplace_child_node({{0x0018, 0x0050}, "DS", "5.0"});
        measures_seq->emplace_child_node(std::move(measures_item));

        sfg_seq->emplace_child_node(std::move(sfg_item));
    }

    // Per-Frame Functional Groups Sequence (5200,9230).
    {
        auto *pfg_seq = root.emplace_child_node({{0x5200, 0x9230}, "SQ", ""});

        // Frame 0: position (0,0,0).
        {
            DCMA_DICOM::Node frame_item;
            frame_item.key = {0x5200, 0x9230};
            frame_item.VR = "MULTI";

            auto *pp_seq = frame_item.emplace_child_node({{0x0020, 0x9113}, "SQ", ""});
            DCMA_DICOM::Node pp_item;
            pp_item.key = {0x0020, 0x9113};
            pp_item.VR = "MULTI";
            pp_item.emplace_child_node({{0x0020, 0x0032}, "DS", "0\\0\\0"});
            pp_seq->emplace_child_node(std::move(pp_item));

            pfg_seq->emplace_child_node(std::move(frame_item));
        }

        // Frame 1: position (0,0,5).
        {
            DCMA_DICOM::Node frame_item;
            frame_item.key = {0x5200, 0x9230};
            frame_item.VR = "MULTI";

            auto *pp_seq = frame_item.emplace_child_node({{0x0020, 0x9113}, "SQ", ""});
            DCMA_DICOM::Node pp_item;
            pp_item.key = {0x0020, 0x9113};
            pp_item.VR = "MULTI";
            pp_item.emplace_child_node({{0x0020, 0x0032}, "DS", "0\\0\\5"});
            pp_seq->emplace_child_node(std::move(pp_item));

            pfg_seq->emplace_child_node(std::move(frame_item));
        }
    }

    auto result = DCMA_DICOM::load_ct_images(root);
    REQUIRE(result != nullptr);
    REQUIRE(result->imagecoll.images.size() == 2);

    // Frame 0: position from Per-Frame FG, spacing from Shared FG.
    const auto &f0 = result->imagecoll.images.front();
    CHECK(f0.offset.x == doctest::Approx(0.0));
    CHECK(f0.offset.y == doctest::Approx(0.0));
    CHECK(f0.offset.z == doctest::Approx(0.0));
    CHECK(f0.pxl_dx == doctest::Approx(1.5));
    CHECK(f0.pxl_dy == doctest::Approx(1.5));
    CHECK(f0.pxl_dz == doctest::Approx(5.0));

    // Frame 1: different position.
    const auto &f1 = result->imagecoll.images.back();
    CHECK(f1.offset.x == doctest::Approx(0.0));
    CHECK(f1.offset.y == doctest::Approx(0.0));
    CHECK(f1.offset.z == doctest::Approx(5.0));
    CHECK(f1.pxl_dx == doctest::Approx(1.5));
    CHECK(f1.pxl_dy == doctest::Approx(1.5));
    CHECK(f1.pxl_dz == doctest::Approx(5.0));

    // Verify per-frame metadata is distinct.
    CHECK(f0.metadata.at("ImagePositionPatient") != f1.metadata.at("ImagePositionPatient"));
}
