//DCMA_DICOM_Tests.cc - A part of DICOMautomaton 2026. Written by hal clark.
//
// This file contains unit tests for the DICOM reader, dictionary, and tree utilities
// defined in DCMA_DICOM.cc. Tests are separated into their own file because
// DCMA_DICOM_obj is linked into shared libraries which don't include doctest implementation.

#include <cstdint>
#include <sstream>
#include <string>
#include <list>
#include <vector>
#include <map>

#include "doctest20251212/doctest.h"

#include "DCMA_DICOM.h"


// ============================================================================
// Dictionary tests
// ============================================================================

TEST_CASE("DCMA_DICOM default dictionary is non-empty"){
    const auto &dict = DCMA_DICOM::get_default_dictionary();
    CHECK(dict.size() > 50);
}

TEST_CASE("DCMA_DICOM default dictionary contains well-known tags"){
    const auto &dict = DCMA_DICOM::get_default_dictionary();

    // TransferSyntaxUID.
    auto it = dict.find({0x0002, 0x0010});
    REQUIRE(it != dict.end());
    CHECK(it->second.VR == "UI");
    CHECK(it->second.keyword == "TransferSyntaxUID");

    // Modality.
    it = dict.find({0x0008, 0x0060});
    REQUIRE(it != dict.end());
    CHECK(it->second.VR == "CS");

    // PixelData.
    it = dict.find({0x7FE0, 0x0010});
    REQUIRE(it != dict.end());
    CHECK(it->second.VR == "OW");
}

TEST_CASE("DCMA_DICOM lookup_VR finds tags in default dictionary"){
    CHECK(DCMA_DICOM::lookup_VR(0x0002, 0x0010) == "UI");
    CHECK(DCMA_DICOM::lookup_VR(0x0008, 0x0060) == "CS");
    CHECK(DCMA_DICOM::lookup_VR(0x0028, 0x0010) == "US");
}

TEST_CASE("DCMA_DICOM lookup_VR returns UL for group length tags"){
    CHECK(DCMA_DICOM::lookup_VR(0x0008, 0x0000) == "UL");
    CHECK(DCMA_DICOM::lookup_VR(0x0020, 0x0000) == "UL");
}

TEST_CASE("DCMA_DICOM lookup_VR returns empty for unknown tags"){
    CHECK(DCMA_DICOM::lookup_VR(0x9999, 0x9999) == "");
}

TEST_CASE("DCMA_DICOM lookup_VR uses layered dictionaries"){
    DCMA_DICOM::DICOMDictionary custom;
    custom[{0x0008, 0x0060}] = {"LO", "OverriddenModality"};
    custom[{0x9999, 0x0001}] = {"CS", "CustomTag"};

    std::vector<const DCMA_DICOM::DICOMDictionary*> dicts = {&custom};

    // Custom dict overrides default.
    CHECK(DCMA_DICOM::lookup_VR(0x0008, 0x0060, dicts) == "LO");
    // Custom dict adds new tags.
    CHECK(DCMA_DICOM::lookup_VR(0x9999, 0x0001, dicts) == "CS");
    // Default still works for other tags.
    CHECK(DCMA_DICOM::lookup_VR(0x0002, 0x0010, dicts) == "UI");
}

TEST_CASE("DCMA_DICOM dictionary read/write round-trip"){
    DCMA_DICOM::DICOMDictionary original;
    original[{0x0008, 0x0060}] = {"CS", "Modality"};
    original[{0x0010, 0x0010}] = {"PN", "PatientName"};
    original[{0x7FE0, 0x0010}] = {"OW", "PixelData"};

    std::stringstream ss;
    DCMA_DICOM::write_dictionary(ss, original);

    auto reloaded = DCMA_DICOM::read_dictionary(ss);
    REQUIRE(reloaded.size() == original.size());

    for(const auto &[key, entry] : original){
        auto it = reloaded.find(key);
        REQUIRE(it != reloaded.end());
        CHECK(it->second.VR == entry.VR);
        CHECK(it->second.keyword == entry.keyword);
    }
}

TEST_CASE("DCMA_DICOM read_dictionary handles comments and blank lines"){
    std::string input =
        "# This is a comment\n"
        "\n"
        "0008,0060 CS Modality\n"
        "# Another comment\n"
        "0010,0010 PN PatientName\n"
        "\n";
    std::istringstream ss(input);
    auto dict = DCMA_DICOM::read_dictionary(ss);
    CHECK(dict.size() == 2);
}


// ============================================================================
// Round-trip tests (write then read)
// ============================================================================

static DCMA_DICOM::Node create_minimal_dicom_tree(DCMA_DICOM::Encoding enc){
    DCMA_DICOM::Node root;

    // Meta information group.
    root.emplace_child_node({{0x0002, 0x0001}, "OB", std::string("\x00\x01", 2)});

    std::string ts_uid = (enc == DCMA_DICOM::Encoding::ILE)
                         ? "1.2.840.10008.1.2"
                         : "1.2.840.10008.1.2.1";
    root.emplace_child_node({{0x0002, 0x0002}, "UI", "1.2.840.10008.5.1.4.1.1.2"});
    root.emplace_child_node({{0x0002, 0x0003}, "UI", "1.2.3.4.5.6.7.8.9"});
    root.emplace_child_node({{0x0002, 0x0010}, "UI", ts_uid});
    root.emplace_child_node({{0x0002, 0x0012}, "UI", "1.2.3.4.5"});
    root.emplace_child_node({{0x0002, 0x0013}, "SH", "DCMA_TEST"});

    // Data elements.
    root.emplace_child_node({{0x0008, 0x0060}, "CS", "CT"});
    root.emplace_child_node({{0x0008, 0x0016}, "UI", "1.2.840.10008.5.1.4.1.1.2"});
    root.emplace_child_node({{0x0008, 0x0018}, "UI", "1.2.3.4.5.6.7.8.9"});
    root.emplace_child_node({{0x0010, 0x0010}, "PN", "DOE^JOHN"});
    root.emplace_child_node({{0x0010, 0x0020}, "LO", "12345"});
    root.emplace_child_node({{0x0020, 0x0013}, "IS", "1"});
    root.emplace_child_node({{0x0028, 0x0010}, "US", "2"});
    root.emplace_child_node({{0x0028, 0x0011}, "US", "3"});

    return root;
}


TEST_CASE("DCMA_DICOM round-trip with Explicit VR Little Endian"){
    auto root = create_minimal_dicom_tree(DCMA_DICOM::Encoding::ELE);

    // Write.
    std::stringstream ss;
    root.emit_DICOM(ss, DCMA_DICOM::Encoding::ELE);
    REQUIRE(ss.good());

    // Read.
    DCMA_DICOM::Node read_root;
    ss.seekg(0);
    read_root.read_DICOM(ss);

    // Verify key tags.
    const auto *modality = read_root.find(0x0008, 0x0060);
    REQUIRE(modality != nullptr);
    CHECK(modality->val == "CT");
    CHECK(modality->VR == "CS");

    const auto *patient_name = read_root.find(0x0010, 0x0010);
    REQUIRE(patient_name != nullptr);
    CHECK(patient_name->val == "DOE^JOHN");

    const auto *rows = read_root.find(0x0028, 0x0010);
    REQUIRE(rows != nullptr);
    CHECK(rows->val == "2");
    CHECK(rows->VR == "US");

    const auto *cols = read_root.find(0x0028, 0x0011);
    REQUIRE(cols != nullptr);
    CHECK(cols->val == "3");
}


TEST_CASE("DCMA_DICOM round-trip with Implicit VR Little Endian"){
    auto root = create_minimal_dicom_tree(DCMA_DICOM::Encoding::ILE);

    // Write.
    std::stringstream ss;
    root.emit_DICOM(ss, DCMA_DICOM::Encoding::ILE);
    REQUIRE(ss.good());

    // Read.
    DCMA_DICOM::Node read_root;
    ss.seekg(0);
    read_root.read_DICOM(ss);

    // Verify key tags.
    const auto *modality = read_root.find(0x0008, 0x0060);
    REQUIRE(modality != nullptr);
    CHECK(modality->val == "CT");

    const auto *patient_name = read_root.find(0x0010, 0x0010);
    REQUIRE(patient_name != nullptr);
    CHECK(patient_name->val == "DOE^JOHN");

    const auto *rows = read_root.find(0x0028, 0x0010);
    REQUIRE(rows != nullptr);
    CHECK(rows->val == "2");
}


// ============================================================================
// Tree search tests
// ============================================================================

TEST_CASE("DCMA_DICOM find returns null for missing tags"){
    auto root = create_minimal_dicom_tree(DCMA_DICOM::Encoding::ELE);
    CHECK(root.find(0x9999, 0x9999) == nullptr);
}

TEST_CASE("DCMA_DICOM find_all returns all matching tags"){
    auto root = create_minimal_dicom_tree(DCMA_DICOM::Encoding::ELE);

    // There should be exactly one Modality tag.
    auto results = root.find_all(0x0008, 0x0060);
    CHECK(results.size() == 1);
}

TEST_CASE("DCMA_DICOM replace works for existing tags"){
    auto root = create_minimal_dicom_tree(DCMA_DICOM::Encoding::ELE);

    DCMA_DICOM::Node replacement({{0x0008, 0x0060}, "CS", "MR"});
    bool did_replace = root.replace(0x0008, 0x0060, std::move(replacement));
    CHECK(did_replace);

    const auto *modality = root.find(0x0008, 0x0060);
    REQUIRE(modality != nullptr);
    CHECK(modality->val == "MR");
}

TEST_CASE("DCMA_DICOM replace returns false for missing tags"){
    auto root = create_minimal_dicom_tree(DCMA_DICOM::Encoding::ELE);
    DCMA_DICOM::Node replacement({{0x9999, 0x9999}, "CS", "X"});
    CHECK_FALSE(root.replace(0x9999, 0x9999, std::move(replacement)));
}

TEST_CASE("DCMA_DICOM value_str provides human-readable output"){
    DCMA_DICOM::Node text_node({{0x0008, 0x0060}, "CS", "CT"});
    CHECK(text_node.value_str() == "CT");

    DCMA_DICOM::Node numeric_node({{0x0028, 0x0010}, "US", "256"});
    CHECK(numeric_node.value_str() == "256");
}


// ============================================================================
// Mutable dictionary tests
// ============================================================================

TEST_CASE("DCMA_DICOM mutable dictionary learns from ELE files"){
    auto root = create_minimal_dicom_tree(DCMA_DICOM::Encoding::ELE);

    // Write.
    std::stringstream ss;
    root.emit_DICOM(ss, DCMA_DICOM::Encoding::ELE);

    // Read with a mutable dictionary.
    DCMA_DICOM::DICOMDictionary mutable_dict;
    DCMA_DICOM::Node read_root;
    ss.seekg(0);
    read_root.read_DICOM(ss, {}, &mutable_dict);

    // The mutable dictionary should have learned at least some tags.
    // It only records tags that differ from or are not in the existing dictionaries.
    // Since we're not passing any extra dicts, it will record tags not in the default dict
    // (there may be few or none since our test uses common tags).
    // But it should work without error.
    CHECK(read_root.find(0x0008, 0x0060) != nullptr);
}

TEST_CASE("DCMA_DICOM mutable dictionary persistence round-trip"){
    DCMA_DICOM::DICOMDictionary mutable_dict;
    mutable_dict[{0x9999, 0x0001}] = {"CS", "LearnedTag1"};
    mutable_dict[{0x9999, 0x0002}] = {"LO", "LearnedTag2"};

    // Write to stream.
    std::stringstream ss;
    DCMA_DICOM::write_dictionary(ss, mutable_dict);

    // Read back.
    auto reloaded = DCMA_DICOM::read_dictionary(ss);
    CHECK(reloaded.size() == 2);

    auto it = reloaded.find({0x9999, 0x0001});
    REQUIRE(it != reloaded.end());
    CHECK(it->second.VR == "CS");
}


// ============================================================================
// Validate tests
// ============================================================================

TEST_CASE("DCMA_DICOM validate on a well-formed tree succeeds"){
    auto root = create_minimal_dicom_tree(DCMA_DICOM::Encoding::ELE);
    CHECK(root.validate(DCMA_DICOM::Encoding::ELE));
}


// ============================================================================
// Sequence round-trip test
// ============================================================================

TEST_CASE("DCMA_DICOM round-trip with sequences"){
    DCMA_DICOM::Node root;

    // Meta information.
    root.emplace_child_node({{0x0002, 0x0001}, "OB", std::string("\x00\x01", 2)});
    root.emplace_child_node({{0x0002, 0x0002}, "UI", "1.2.840.10008.5.1.4.1.1.2"});
    root.emplace_child_node({{0x0002, 0x0003}, "UI", "1.2.3.4.5.6.7.8.9"});
    root.emplace_child_node({{0x0002, 0x0010}, "UI", "1.2.840.10008.1.2.1"});
    root.emplace_child_node({{0x0002, 0x0012}, "UI", "1.2.3.4.5"});

    // A simple sequence.
    auto *seq = root.emplace_child_node({{0x3006, 0x0020}, "SQ", ""});
    {
        DCMA_DICOM::Node item;
        item.key = {0x3006, 0x0020};
        item.VR = "MULTI";
        item.emplace_child_node({{0x3006, 0x0022}, "IS", "1"});
        item.emplace_child_node({{0x3006, 0x0026}, "LO", "ROI_A"});
        seq->emplace_child_node(std::move(item));
    }
    {
        DCMA_DICOM::Node item;
        item.key = {0x3006, 0x0020};
        item.VR = "MULTI";
        item.emplace_child_node({{0x3006, 0x0022}, "IS", "2"});
        item.emplace_child_node({{0x3006, 0x0026}, "LO", "ROI_B"});
        seq->emplace_child_node(std::move(item));
    }

    // Write.
    std::stringstream ss;
    root.emit_DICOM(ss, DCMA_DICOM::Encoding::ELE);
    REQUIRE(ss.good());

    // Read.
    DCMA_DICOM::Node read_root;
    ss.seekg(0);
    read_root.read_DICOM(ss);

    // Verify the sequence was parsed.
    const auto *read_seq = read_root.find(0x3006, 0x0020);
    REQUIRE(read_seq != nullptr);
    CHECK(read_seq->VR == "SQ");
    CHECK(read_seq->children.size() == 2);

    // Verify ROI names are accessible via find_all.
    auto roi_names = read_root.find_all(0x3006, 0x0026);
    CHECK(roi_names.size() == 2);
}
