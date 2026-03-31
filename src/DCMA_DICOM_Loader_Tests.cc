// DCMA_DICOM_Loader_Tests.cc - A part of DICOMautomaton 2026. Written by hal clark.
//
// This file contains unit tests for the DICOM data extraction modules:
// DCMA_DICOM_Contours, DCMA_DICOM_RTPLAN, and DCMA_DICOM_Transform.

#include <cstdint>
#include <cstring>
#include <cmath>
#include <sstream>
#include <string>
#include <list>
#include <vector>
#include <map>
#include <limits>

#include "doctest20251212/doctest.h"

#include "YgorImages.h"
#include "YgorMath.h"

#include "DCMA_DICOM.h"
#include "DCMA_DICOM_Contours.h"
#include "DCMA_DICOM_RTPLAN.h"
#include "DCMA_DICOM_Transform.h"
#include "Structs.h"


// ============================================================================
// Contour extraction tests
// ============================================================================

TEST_CASE("DCMA_DICOM_Contours extract_common_metadata extracts patient info"){
    // Build a minimal DICOM tree with patient metadata.
    DCMA_DICOM::Node root;
    root.VR = "SQ";

    auto *pat_name = root.emplace_child_node(DCMA_DICOM::Node{
        {0x0010, 0x0010, 0, 0}, "PN", "Test^Patient"});
    (void)pat_name;

    auto *pat_id = root.emplace_child_node(DCMA_DICOM::Node{
        {0x0010, 0x0020, 0, 0}, "LO", "PATIENT123"});
    (void)pat_id;

    auto *modality = root.emplace_child_node(DCMA_DICOM::Node{
        {0x0008, 0x0060, 0, 0}, "CS", "RTSTRUCT"});
    (void)modality;

    auto metadata = DCMA_DICOM::extract_common_metadata(root);

    CHECK(metadata["PatientName"] == "Test^Patient");
    CHECK(metadata["PatientID"] == "PATIENT123");
    CHECK(metadata["Modality"] == "RTSTRUCT");
}

TEST_CASE("DCMA_DICOM_Contours extract_roi_names extracts ROI names from sequence"){
    // Build a synthetic StructureSetROISequence.
    DCMA_DICOM::Node root;
    root.VR = "SQ";

    // StructureSetROISequence (3006,0020).
    auto *ss_roi_seq = root.emplace_child_node(DCMA_DICOM::Node{
        {0x3006, 0x0020, 0, 0}, "SQ", ""});

    // First ROI item.
    auto *roi1 = ss_roi_seq->emplace_child_node(DCMA_DICOM::Node{
        {0xFFFE, 0xE000, 0, 0}, "SQ", ""});
    roi1->emplace_child_node(DCMA_DICOM::Node{
        {0x3006, 0x0022, 0, 0}, "IS", "1"});
    roi1->emplace_child_node(DCMA_DICOM::Node{
        {0x3006, 0x0026, 0, 0}, "LO", "Liver"});

    // Second ROI item.
    auto *roi2 = ss_roi_seq->emplace_child_node(DCMA_DICOM::Node{
        {0xFFFE, 0xE000, 1, 0}, "SQ", ""});
    roi2->emplace_child_node(DCMA_DICOM::Node{
        {0x3006, 0x0022, 0, 0}, "IS", "2"});
    roi2->emplace_child_node(DCMA_DICOM::Node{
        {0x3006, 0x0026, 0, 0}, "LO", "Kidney"});

    auto roi_names = DCMA_DICOM::extract_roi_names(root);

    REQUIRE(roi_names.size() == 2);
    CHECK(roi_names.at(1) == "Liver");
    CHECK(roi_names.at(2) == "Kidney");
}

TEST_CASE("DCMA_DICOM_Contours extract_contour_data rejects non-RTSTRUCT modality"){
    DCMA_DICOM::Node root;
    root.VR = "SQ";

    root.emplace_child_node(DCMA_DICOM::Node{
        {0x0008, 0x0060, 0, 0}, "CS", "CT"});

    auto result = DCMA_DICOM::extract_contour_data(root);
    CHECK(result == nullptr);
}


// ============================================================================
// RTPLAN extraction tests
// ============================================================================

TEST_CASE("DCMA_DICOM_RTPLAN extract_rtplan rejects non-RTPLAN modality"){
    DCMA_DICOM::Node root;
    root.VR = "SQ";

    root.emplace_child_node(DCMA_DICOM::Node{
        {0x0008, 0x0060, 0, 0}, "CS", "CT"});

    auto result = DCMA_DICOM::extract_rtplan(root);
    CHECK(result == nullptr);
}

TEST_CASE("DCMA_DICOM_RTPLAN extract_rtplan rejects missing BeamSequence"){
    DCMA_DICOM::Node root;
    root.VR = "SQ";

    root.emplace_child_node(DCMA_DICOM::Node{
        {0x0008, 0x0060, 0, 0}, "CS", "RTPLAN"});

    auto result = DCMA_DICOM::extract_rtplan(root);
    CHECK(result == nullptr);
}


// ============================================================================
// Transform extraction tests
// ============================================================================

TEST_CASE("DCMA_DICOM_Transform extract_affine_from_matrix valid identity matrix"){
    std::vector<double> identity = {
        1.0, 0.0, 0.0, 0.0,
        0.0, 1.0, 0.0, 0.0,
        0.0, 0.0, 1.0, 0.0,
        0.0, 0.0, 0.0, 1.0
    };

    auto t = DCMA_DICOM::extract_affine_from_matrix(identity);

    // Check that the identity matrix is correctly extracted.
    CHECK(t.coeff(0, 0) == doctest::Approx(1.0));
    CHECK(t.coeff(0, 1) == doctest::Approx(0.0));
    CHECK(t.coeff(0, 2) == doctest::Approx(0.0));
    CHECK(t.coeff(0, 3) == doctest::Approx(0.0));
    CHECK(t.coeff(1, 0) == doctest::Approx(0.0));
    CHECK(t.coeff(1, 1) == doctest::Approx(1.0));
    CHECK(t.coeff(1, 2) == doctest::Approx(0.0));
    CHECK(t.coeff(1, 3) == doctest::Approx(0.0));
    CHECK(t.coeff(2, 0) == doctest::Approx(0.0));
    CHECK(t.coeff(2, 1) == doctest::Approx(0.0));
    CHECK(t.coeff(2, 2) == doctest::Approx(1.0));
    CHECK(t.coeff(2, 3) == doctest::Approx(0.0));
}

TEST_CASE("DCMA_DICOM_Transform extract_affine_from_matrix with translation"){
    std::vector<double> trans = {
        1.0, 0.0, 0.0, 10.0,
        0.0, 1.0, 0.0, 20.0,
        0.0, 0.0, 1.0, 30.0,
        0.0, 0.0, 0.0, 1.0
    };

    auto t = DCMA_DICOM::extract_affine_from_matrix(trans);

    // Check translation components.
    CHECK(t.coeff(0, 3) == doctest::Approx(10.0));
    CHECK(t.coeff(1, 3) == doctest::Approx(20.0));
    CHECK(t.coeff(2, 3) == doctest::Approx(30.0));
}

TEST_CASE("DCMA_DICOM_Transform extract_affine_from_matrix rejects wrong size"){
    std::vector<double> bad_size = {1.0, 0.0, 0.0};

    CHECK_THROWS_AS(DCMA_DICOM::extract_affine_from_matrix(bad_size), std::runtime_error);
}

TEST_CASE("DCMA_DICOM_Transform extract_affine_from_matrix rejects invalid last row"){
    std::vector<double> bad_last_row = {
        1.0, 0.0, 0.0, 0.0,
        0.0, 1.0, 0.0, 0.0,
        0.0, 0.0, 1.0, 0.0,
        1.0, 0.0, 0.0, 1.0  // Invalid: first element should be 0.
    };

    CHECK_THROWS_AS(DCMA_DICOM::extract_affine_from_matrix(bad_last_row), std::runtime_error);
}

TEST_CASE("DCMA_DICOM_Transform extract_transform rejects non-REG modality"){
    DCMA_DICOM::Node root;
    root.VR = "SQ";

    root.emplace_child_node(DCMA_DICOM::Node{
        {0x0008, 0x0060, 0, 0}, "CS", "CT"});

    auto result = DCMA_DICOM::extract_transform(root);
    CHECK(result == nullptr);
}

TEST_CASE("DCMA_DICOM_Transform extract_transform returns empty transform for REG without data"){
    DCMA_DICOM::Node root;
    root.VR = "SQ";

    root.emplace_child_node(DCMA_DICOM::Node{
        {0x0008, 0x0060, 0, 0}, "CS", "REG"});

    auto result = DCMA_DICOM::extract_transform(root);
    REQUIRE(result != nullptr);

    // Transform should be monostate (empty).
    CHECK(std::holds_alternative<std::monostate>(result->transform));
}

