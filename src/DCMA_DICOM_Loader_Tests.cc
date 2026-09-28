// DCMA_DICOM_Loader_Tests.cc - A part of DICOMautomaton 2026. Written by hal clark.
//
// This file contains unit tests for the Node-based DICOM semantic loaders defined
// in DCMA_DICOM_Loader.{h,cc}. Tests construct synthetic Node trees that mimic
// real DICOM files and verify the loaders extract the correct semantic data.

#include <cstdint>
#include <cmath>
#include <sstream>
#include <string>
#include <list>
#include <vector>
#include <memory>

#include "doctest20251212/doctest.h"

#include "YgorImages.h"
#include "YgorMath.h"

#include "DCMA_DICOM.h"
#include "DCMA_DICOM_Loader.h"


// ============================================================================
// Helper: build a minimal Node tree for a single-frame CT-like image.
// ============================================================================
static DCMA_DICOM::Node make_minimal_ct_node(){
    DCMA_DICOM::Node root;

    // File Meta header.
    root.emplace_child_node({{0x0002, 0x0010}, "UI", "1.2.840.10008.1.2.1"}); // TransferSyntaxUID (ELE)

    // SOP Common Module.
    root.emplace_child_node({{0x0008, 0x0016}, "UI", "1.2.840.10008.5.1.4.1.1.2"}); // SOPClassUID (CT)
    root.emplace_child_node({{0x0008, 0x0018}, "UI", "1.2.3.4.5.6.7.8.9"}); // SOPInstanceUID

    // Patient.
    root.emplace_child_node({{0x0010, 0x0010}, "PN", "Doe^John"}); // PatientsName
    root.emplace_child_node({{0x0010, 0x0020}, "LO", "PT001"}); // PatientID

    // Study/Series.
    root.emplace_child_node({{0x0020, 0x000D}, "UI", "1.2.3.4.5.100"}); // StudyInstanceUID
    root.emplace_child_node({{0x0020, 0x000E}, "UI", "1.2.3.4.5.200"}); // SeriesInstanceUID
    root.emplace_child_node({{0x0008, 0x0060}, "CS", "CT"});             // Modality
    root.emplace_child_node({{0x0020, 0x0052}, "UI", "1.2.3.4.5.300"}); // FrameOfReferenceUID

    // Image geometry.
    root.emplace_child_node({{0x0020, 0x0032}, "DS", "10.0\\20.0\\30.0"}); // ImagePositionPatient
    root.emplace_child_node({{0x0020, 0x0037}, "DS", "1.0\\0.0\\0.0\\0.0\\1.0\\0.0"}); // ImageOrientationPatient
    root.emplace_child_node({{0x0028, 0x0030}, "DS", "0.5\\0.5"}); // PixelSpacing
    root.emplace_child_node({{0x0018, 0x0050}, "DS", "2.5"}); // SliceThickness

    // Image Pixel Module.
    root.emplace_child_node({{0x0028, 0x0002}, "US", "1"});        // SamplesPerPixel
    root.emplace_child_node({{0x0028, 0x0004}, "CS", "MONOCHROME2"});  // PhotometricInterpretation
    root.emplace_child_node({{0x0028, 0x0010}, "US", "2"});        // Rows
    root.emplace_child_node({{0x0028, 0x0011}, "US", "3"});        // Columns
    root.emplace_child_node({{0x0028, 0x0100}, "US", "16"});       // BitsAllocated
    root.emplace_child_node({{0x0028, 0x0101}, "US", "16"});       // BitsStored
    root.emplace_child_node({{0x0028, 0x0102}, "US", "15"});       // HighBit
    root.emplace_child_node({{0x0028, 0x0103}, "US", "0"});        // PixelRepresentation (unsigned)

    // Raw pixel data: 2 rows x 3 cols x 1 channel x 2 bytes = 12 bytes.
    // Values: 100, 200, 300, 400, 500, 600 (little-endian uint16).
    std::string pixel_bytes;
    for(uint16_t v : {100, 200, 300, 400, 500, 600}){
        pixel_bytes.push_back(static_cast<char>(v & 0xFF));
        pixel_bytes.push_back(static_cast<char>((v >> 8) & 0xFF));
    }
    root.emplace_child_node({{0x7FE0, 0x0010}, "OW", pixel_bytes}); // PixelData

    return root;
}


// ============================================================================
// Tag accessors.
// ============================================================================

TEST_CASE("DCMA_DICOM_Loader get_tag_as_string_from_node"){
    auto root = make_minimal_ct_node();
    CHECK(DCMA_DICOM::get_tag_as_string_from_node(root, 0x0008, 0x0060) == "CT");
    CHECK(DCMA_DICOM::get_tag_as_string_from_node(root, 0x0010, 0x0020) == "PT001");
    CHECK(DCMA_DICOM::get_tag_as_string_from_node(root, 0x9999, 0x9999) == "");
}

TEST_CASE("DCMA_DICOM_Loader get_modality_from_node"){
    auto root = make_minimal_ct_node();
    CHECK(DCMA_DICOM::get_modality_from_node(root) == "CT");
}

TEST_CASE("DCMA_DICOM_Loader get_patient_ID_from_node"){
    auto root = make_minimal_ct_node();
    CHECK(DCMA_DICOM::get_patient_ID_from_node(root) == "PT001");
}


// ============================================================================
// Metadata extraction.
// ============================================================================

TEST_CASE("DCMA_DICOM_Loader get_metadata_top_level_tags_from_node extracts common tags"){
    auto root = make_minimal_ct_node();
    auto meta = DCMA_DICOM::get_metadata_top_level_tags_from_node(root, "test.dcm");

    CHECK(meta["Modality"] == "CT");
    CHECK(meta["PatientID"] == "PT001");
    CHECK(meta["SOPInstanceUID"] == "1.2.3.4.5.6.7.8.9");
    CHECK(meta["StudyInstanceUID"] == "1.2.3.4.5.100");
    CHECK(meta["SeriesInstanceUID"] == "1.2.3.4.5.200");
    CHECK(meta["FrameOfReferenceUID"] == "1.2.3.4.5.300");
    CHECK(meta["Filename"] == "test.dcm");
}

TEST_CASE("DCMA_DICOM_Loader metadata has expected pixel module tags"){
    auto root = make_minimal_ct_node();
    auto meta = DCMA_DICOM::get_metadata_top_level_tags_from_node(root);

    CHECK(meta["Rows"] == "2");
    CHECK(meta["Columns"] == "3");
    CHECK(meta["BitsAllocated"] == "16");
}


// ============================================================================
// ROI extraction.
// ============================================================================

static DCMA_DICOM::Node make_minimal_rtstruct_node(){
    DCMA_DICOM::Node root;

    root.emplace_child_node({{0x0002, 0x0010}, "UI", "1.2.840.10008.1.2.1"});
    root.emplace_child_node({{0x0008, 0x0060}, "CS", "RTSTRUCT"});
    root.emplace_child_node({{0x0010, 0x0020}, "LO", "PT001"});

    // StructureSetROISequence (3006,0020).
    auto *ss_sq = root.emplace_child_node({{0x3006, 0x0020}, "SQ", ""});
    auto *item1 = ss_sq->emplace_child_node({{0xFFFE, 0xE000}, "", ""});
    item1->emplace_child_node({{0x3006, 0x0022}, "IS", "1"});       // ROINumber
    item1->emplace_child_node({{0x3006, 0x0026}, "LO", "PTV"});     // ROIName
    auto *item2 = ss_sq->emplace_child_node({{0xFFFE, 0xE000}, "", ""});
    item2->emplace_child_node({{0x3006, 0x0022}, "IS", "2"});
    item2->emplace_child_node({{0x3006, 0x0026}, "LO", "Bladder"});

    return root;
}

TEST_CASE("DCMA_DICOM_Loader get_ROI_tags_and_numbers_from_node"){
    auto root = make_minimal_rtstruct_node();
    auto roi_map = DCMA_DICOM::get_ROI_tags_and_numbers_from_node(root);

    CHECK(roi_map[1] == "PTV");
    CHECK(roi_map[2] == "Bladder");
}


// ============================================================================
// RTSTRUCT contour extraction.
// ============================================================================

static DCMA_DICOM::Node make_rtstruct_with_contours(){
    auto root = make_minimal_rtstruct_node();

    // Add ReferencedFrameOfReferenceSequence so FrameOfReferenceUID is found.
    root.emplace_child_node({{0x0020, 0x0052}, "UI", "1.2.3.4.5.300"});

    // ROIContourSequence (3006,0039).
    auto *rc_sq = root.emplace_child_node({{0x3006, 0x0039}, "SQ", ""});
    auto *roi_item = rc_sq->emplace_child_node({{0xFFFE, 0xE000}, "", ""});
    roi_item->emplace_child_node({{0x3006, 0x0084}, "IS", "1"}); // ReferencedROINumber

    // ContourSequence (3006,0040).
    auto *c_sq = roi_item->emplace_child_node({{0x3006, 0x0040}, "SQ", ""});
    auto *contour = c_sq->emplace_child_node({{0xFFFE, 0xE000}, "", ""});
    contour->emplace_child_node({{0x3006, 0x0050}, "DS", "0.0\\0.0\\0.0\\1.0\\0.0\\0.0\\1.0\\1.0\\0.0\\0.0\\1.0\\0.0"});

    return root;
}

TEST_CASE("DCMA_DICOM_Loader get_Contour_Data_from_node extracts contour points"){
    auto root = make_rtstruct_with_contours();
    auto cd = DCMA_DICOM::get_Contour_Data_from_node(root, "test_struct.dcm");

    REQUIRE(cd != nullptr);
    REQUIRE(!cd->ccs.empty());
    REQUIRE(!cd->ccs.front().contours.empty());

    const auto &contour = cd->ccs.front().contours.front();
    CHECK(contour.points.size() == 4);  // 12 doubles / 3 = 4 points
    CHECK(contour.metadata.count("ROIName") == 1);
    CHECK(contour.metadata.at("ROIName") == "PTV");
}


// ============================================================================
// Image loading: single frame.
// ============================================================================

TEST_CASE("DCMA_DICOM_Loader Load_Image_Array_from_node loads single-frame CT"){
    auto root = make_minimal_ct_node();
    auto img_arr = DCMA_DICOM::Load_Image_Array_from_node(root, "test_ct.dcm");

    REQUIRE(img_arr != nullptr);
    REQUIRE(img_arr->imagecoll.images.size() == 1);

    const auto &img = img_arr->imagecoll.images.front();
    CHECK(img.rows == 2);
    CHECK(img.columns == 3);
    CHECK(img.channels == 1);

    // Check pixel values (uint16_t: 100, 200, 300, 400, 500, 600).
    CHECK(img.value(0, 0, 0) == doctest::Approx(100.0f));
    CHECK(img.value(0, 1, 0) == doctest::Approx(200.0f));
    CHECK(img.value(0, 2, 0) == doctest::Approx(300.0f));
    CHECK(img.value(1, 0, 0) == doctest::Approx(400.0f));
    CHECK(img.value(1, 1, 0) == doctest::Approx(500.0f));
    CHECK(img.value(1, 2, 0) == doctest::Approx(600.0f));
}

TEST_CASE("DCMA_DICOM_Loader Load_Image_Array_from_node geometry is correct"){
    auto root = make_minimal_ct_node();
    auto img_arr = DCMA_DICOM::Load_Image_Array_from_node(root, "test_ct.dcm");

    REQUIRE(img_arr != nullptr);
    const auto &img = img_arr->imagecoll.images.front();

    CHECK(img.pxl_dx == doctest::Approx(0.5));
    CHECK(img.pxl_dy == doctest::Approx(0.5));
    CHECK(img.pxl_dz == doctest::Approx(2.5));

    // Check metadata.
    CHECK(img.metadata.count("Modality") == 1);
    CHECK(img.metadata.at("Modality") == "CT");
}


// ============================================================================
// Image loading: enhanced multi-frame with PerFrameFunctionalGroupsSequence.
// ============================================================================

static DCMA_DICOM::Node make_enhanced_multiframe_node(){
    DCMA_DICOM::Node root;

    // File Meta.
    root.emplace_child_node({{0x0002, 0x0010}, "UI", "1.2.840.10008.1.2.1"});

    // SOP.
    root.emplace_child_node({{0x0008, 0x0016}, "UI", "1.2.840.10008.5.1.4.1.1.4.1"}); // Enhanced MR
    root.emplace_child_node({{0x0008, 0x0018}, "UI", "1.2.3.4.5.6.7.8.10"});
    root.emplace_child_node({{0x0008, 0x0060}, "CS", "MR"});
    root.emplace_child_node({{0x0010, 0x0020}, "LO", "PT002"});
    root.emplace_child_node({{0x0020, 0x000D}, "UI", "1.2.3.4.5.101"});
    root.emplace_child_node({{0x0020, 0x000E}, "UI", "1.2.3.4.5.201"});
    root.emplace_child_node({{0x0020, 0x0052}, "UI", "1.2.3.4.5.301"});

    // Image Pixel Module (shared across frames).
    root.emplace_child_node({{0x0028, 0x0002}, "US", "1"});
    root.emplace_child_node({{0x0028, 0x0004}, "CS", "MONOCHROME2"});
    root.emplace_child_node({{0x0028, 0x0010}, "US", "2"});       // Rows
    root.emplace_child_node({{0x0028, 0x0011}, "US", "2"});       // Columns
    root.emplace_child_node({{0x0028, 0x0100}, "US", "16"});      // BitsAllocated
    root.emplace_child_node({{0x0028, 0x0101}, "US", "16"});      // BitsStored
    root.emplace_child_node({{0x0028, 0x0102}, "US", "15"});      // HighBit
    root.emplace_child_node({{0x0028, 0x0103}, "US", "0"});       // unsigned
    root.emplace_child_node({{0x0028, 0x0008}, "IS", "2"});       // NumberOfFrames

    // PerFrameFunctionalGroupsSequence (5200,9230) with two items.
    auto *pfg_sq = root.emplace_child_node({{0x5200, 0x9230}, "SQ", ""});

    // --- Frame 0 ---
    auto *frame0 = pfg_sq->emplace_child_node({{0xFFFE, 0xE000}, "", ""});

    // PlanePositionSequence (0020,9113)
    auto *pp0_sq = frame0->emplace_child_node({{0x0020, 0x9113}, "SQ", ""});
    auto *pp0_item = pp0_sq->emplace_child_node({{0xFFFE, 0xE000}, "", ""});
    pp0_item->emplace_child_node({{0x0020, 0x0032}, "DS", "0.0\\0.0\\0.0"});

    // PlaneOrientationSequence (0020,9116)
    auto *po0_sq = frame0->emplace_child_node({{0x0020, 0x9116}, "SQ", ""});
    auto *po0_item = po0_sq->emplace_child_node({{0xFFFE, 0xE000}, "", ""});
    po0_item->emplace_child_node({{0x0020, 0x0037}, "DS", "1.0\\0.0\\0.0\\0.0\\1.0\\0.0"});

    // PixelMeasuresSequence (0028,9110)
    auto *pm0_sq = frame0->emplace_child_node({{0x0028, 0x9110}, "SQ", ""});
    auto *pm0_item = pm0_sq->emplace_child_node({{0xFFFE, 0xE000}, "", ""});
    pm0_item->emplace_child_node({{0x0028, 0x0030}, "DS", "1.0\\1.0"});
    pm0_item->emplace_child_node({{0x0018, 0x0050}, "DS", "3.0"});

    // --- Frame 1 ---
    auto *frame1 = pfg_sq->emplace_child_node({{0xFFFE, 0xE000}, "", ""});

    // PlanePositionSequence
    auto *pp1_sq = frame1->emplace_child_node({{0x0020, 0x9113}, "SQ", ""});
    auto *pp1_item = pp1_sq->emplace_child_node({{0xFFFE, 0xE000}, "", ""});
    pp1_item->emplace_child_node({{0x0020, 0x0032}, "DS", "0.0\\0.0\\3.0"});

    // PlaneOrientationSequence
    auto *po1_sq = frame1->emplace_child_node({{0x0020, 0x9116}, "SQ", ""});
    auto *po1_item = po1_sq->emplace_child_node({{0xFFFE, 0xE000}, "", ""});
    po1_item->emplace_child_node({{0x0020, 0x0037}, "DS", "1.0\\0.0\\0.0\\0.0\\1.0\\0.0"});

    // PixelMeasuresSequence
    auto *pm1_sq = frame1->emplace_child_node({{0x0028, 0x9110}, "SQ", ""});
    auto *pm1_item = pm1_sq->emplace_child_node({{0xFFFE, 0xE000}, "", ""});
    pm1_item->emplace_child_node({{0x0028, 0x0030}, "DS", "1.0\\1.0"});
    pm1_item->emplace_child_node({{0x0018, 0x0050}, "DS", "3.0"});

    // Pixel data: 2 frames x 2 rows x 2 cols = 8 pixels x 2 bytes = 16 bytes.
    std::string pixel_bytes;
    for(uint16_t v : {10, 20, 30, 40, 110, 120, 130, 140}){
        pixel_bytes.push_back(static_cast<char>(v & 0xFF));
        pixel_bytes.push_back(static_cast<char>((v >> 8) & 0xFF));
    }
    root.emplace_child_node({{0x7FE0, 0x0010}, "OW", pixel_bytes});

    return root;
}

TEST_CASE("DCMA_DICOM_Loader Load_Image_Array_from_node enhanced multi-frame per-frame positions"){
    auto root = make_enhanced_multiframe_node();
    auto img_arr = DCMA_DICOM::Load_Image_Array_from_node(root, "test_enh_mr.dcm");

    REQUIRE(img_arr != nullptr);
    REQUIRE(img_arr->imagecoll.images.size() == 2);

    // Frame 0: position should be (0,0,0).
    const auto &img0 = img_arr->imagecoll.images.front();
    CHECK(img0.rows == 2);
    CHECK(img0.columns == 2);
    CHECK(img0.pxl_dx == doctest::Approx(1.0));
    CHECK(img0.pxl_dy == doctest::Approx(1.0));
    CHECK(img0.pxl_dz == doctest::Approx(3.0));

    // Frame 1: position should be (0,0,3).
    const auto &img1 = img_arr->imagecoll.images.back();
    CHECK(img1.rows == 2);
    CHECK(img1.columns == 2);

    // Verify different positions per frame (check z-offset via metadata).
    CHECK(img0.metadata.at("ImagePositionPatient").find("0.000000\\0.000000\\0.000000") != std::string::npos);
    CHECK(img1.metadata.at("ImagePositionPatient").find("0.000000\\0.000000\\3.000000") != std::string::npos);
}

TEST_CASE("DCMA_DICOM_Loader enhanced multi-frame pixel values are correct per frame"){
    auto root = make_enhanced_multiframe_node();
    auto img_arr = DCMA_DICOM::Load_Image_Array_from_node(root, "test_enh_mr.dcm");

    REQUIRE(img_arr != nullptr);
    REQUIRE(img_arr->imagecoll.images.size() == 2);

    const auto &img0 = img_arr->imagecoll.images.front();
    CHECK(img0.value(0, 0, 0) == doctest::Approx(10.0f));
    CHECK(img0.value(0, 1, 0) == doctest::Approx(20.0f));
    CHECK(img0.value(1, 0, 0) == doctest::Approx(30.0f));
    CHECK(img0.value(1, 1, 0) == doctest::Approx(40.0f));

    const auto &img1 = img_arr->imagecoll.images.back();
    CHECK(img1.value(0, 0, 0) == doctest::Approx(110.0f));
    CHECK(img1.value(0, 1, 0) == doctest::Approx(120.0f));
    CHECK(img1.value(1, 0, 0) == doctest::Approx(130.0f));
    CHECK(img1.value(1, 1, 0) == doctest::Approx(140.0f));
}


// ============================================================================
// Image loading: modality LUT / rescale.
// ============================================================================

TEST_CASE("DCMA_DICOM_Loader Load_Image_Array_from_node applies RescaleSlope/Intercept"){
    auto root = make_minimal_ct_node();
    root.emplace_child_node({{0x0028, 0x1052}, "DS", "-1024.0"}); // RescaleIntercept
    root.emplace_child_node({{0x0028, 0x1053}, "DS", "1.0"});     // RescaleSlope

    auto img_arr = DCMA_DICOM::Load_Image_Array_from_node(root, "test_ct_hu.dcm");
    REQUIRE(img_arr != nullptr);
    REQUIRE(!img_arr->imagecoll.images.empty());

    const auto &img = img_arr->imagecoll.images.front();
    CHECK(img.value(0, 0, 0) == doctest::Approx(100.0f - 1024.0f));
    CHECK(img.value(0, 1, 0) == doctest::Approx(200.0f - 1024.0f));
}


// ============================================================================
// Dose loading.
// ============================================================================

static DCMA_DICOM::Node make_minimal_rtdose_node(){
    DCMA_DICOM::Node root;

    root.emplace_child_node({{0x0002, 0x0010}, "UI", "1.2.840.10008.1.2.1"});
    root.emplace_child_node({{0x0008, 0x0060}, "CS", "RTDOSE"});
    root.emplace_child_node({{0x0010, 0x0020}, "LO", "PT001"});
    root.emplace_child_node({{0x0020, 0x000D}, "UI", "1.2.3.4.5.100"});
    root.emplace_child_node({{0x0020, 0x000E}, "UI", "1.2.3.4.5.200"});

    root.emplace_child_node({{0x0020, 0x0032}, "DS", "0.0\\0.0\\0.0"});
    root.emplace_child_node({{0x0020, 0x0037}, "DS", "1.0\\0.0\\0.0\\0.0\\1.0\\0.0"});
    root.emplace_child_node({{0x0028, 0x0030}, "DS", "2.0\\2.0"}); // PixelSpacing
    root.emplace_child_node({{0x0018, 0x0050}, "DS", "2.0"});      // SliceThickness

    root.emplace_child_node({{0x0028, 0x0002}, "US", "1"});
    root.emplace_child_node({{0x0028, 0x0004}, "CS", "MONOCHROME2"});
    root.emplace_child_node({{0x0028, 0x0010}, "US", "2"});  // Rows
    root.emplace_child_node({{0x0028, 0x0011}, "US", "2"});  // Columns
    root.emplace_child_node({{0x0028, 0x0100}, "US", "16"});
    root.emplace_child_node({{0x0028, 0x0101}, "US", "16"});
    root.emplace_child_node({{0x0028, 0x0102}, "US", "15"});
    root.emplace_child_node({{0x0028, 0x0103}, "US", "0"});
    root.emplace_child_node({{0x0028, 0x0008}, "IS", "2"});  // NumberOfFrames

    root.emplace_child_node({{0x3004, 0x000e}, "DS", "0.001"});            // DoseGridScaling
    root.emplace_child_node({{0x3004, 0x000c}, "DS", "0.0\\2.0"});         // GridFrameOffsetVector

    // Pixel data: 2 frames x 2 rows x 2 cols = 8 pixels x 2 bytes = 16 bytes.
    std::string pixel_bytes;
    for(uint16_t v : {1000, 2000, 3000, 4000, 5000, 6000, 7000, 8000}){
        pixel_bytes.push_back(static_cast<char>(v & 0xFF));
        pixel_bytes.push_back(static_cast<char>((v >> 8) & 0xFF));
    }
    root.emplace_child_node({{0x7FE0, 0x0010}, "OW", pixel_bytes});

    return root;
}

TEST_CASE("DCMA_DICOM_Loader Load_Dose_Array_from_node loads multi-frame RTDOSE"){
    auto root = make_minimal_rtdose_node();
    auto img_arr = DCMA_DICOM::Load_Dose_Array_from_node(root, "test_dose.dcm");

    REQUIRE(img_arr != nullptr);
    CHECK(img_arr->imagecoll.images.size() == 2);

    // Frame 0 scaled by DoseGridScaling (0.001).
    const auto &img0 = img_arr->imagecoll.images.front();
    CHECK(img0.value(0, 0, 0) == doctest::Approx(1000.0f * 0.001f));
    CHECK(img0.value(0, 1, 0) == doctest::Approx(2000.0f * 0.001f));

    // Frame 1.
    const auto &img1 = img_arr->imagecoll.images.back();
    CHECK(img1.value(0, 0, 0) == doctest::Approx(5000.0f * 0.001f));
}


// ============================================================================
// Collation.
// ============================================================================

TEST_CASE("DCMA_DICOM_Loader node_Collate_Image_Arrays handles empty list"){
    std::list<std::shared_ptr<Image_Array>> empty;
    auto result = DCMA_DICOM::node_Collate_Image_Arrays(empty);
    REQUIRE(result != nullptr);
    CHECK(result->imagecoll.images.empty());
}
