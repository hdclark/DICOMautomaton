//DCMA_DICOM_Tests.cc - A part of DICOMautomaton 2026. Written by hal clark.
//
// This file contains unit tests for the DICOM reader, dictionary, and tree utilities
// defined in DCMA_DICOM.cc. Tests are separated into their own file because
// DCMA_DICOM_obj is linked into shared libraries which don't include doctest implementation.

#include <cstdint>
#include <cstring>
#include <cmath>
#include <sstream>
#include <string>
#include <list>
#include <vector>
#include <map>

#include "doctest20251212/doctest.h"

#include "YgorImages.h"
#include "YgorMath.h"

#include "DCMA_DICOM.h"
#include "DCMA_DICOM_PixelData.h"


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

TEST_CASE("DCMA_DICOM default dictionary entries have VM populated"){
    const auto &dict = DCMA_DICOM::get_default_dictionary();
    for(const auto &[key, entry] : dict){
        CHECK_MESSAGE(!entry.VM.empty(),
                      "Tag (" << std::hex << key.first << "," << key.second
                              << ") is missing VM");
    }
}

TEST_CASE("DCMA_DICOM default dictionary VM values for well-known tags"){
    const auto &dict = DCMA_DICOM::get_default_dictionary();

    // TransferSyntaxUID has VM=1.
    auto it = dict.find({0x0002, 0x0010});
    REQUIRE(it != dict.end());
    CHECK(it->second.VM == "1");

    // ImageType has VM=2-n.
    it = dict.find({0x0008, 0x0008});
    REQUIRE(it != dict.end());
    CHECK(it->second.VM == "2-n");

    // PixelSpacing has VM=2.
    it = dict.find({0x0028, 0x0030});
    REQUIRE(it != dict.end());
    CHECK(it->second.VM == "2");

    // ImageOrientationPatient has VM=6.
    it = dict.find({0x0020, 0x0037});
    REQUIRE(it != dict.end());
    CHECK(it->second.VM == "6");

    // ContourData has VM=3-3n.
    it = dict.find({0x3006, 0x0050});
    REQUIRE(it != dict.end());
    CHECK(it->second.VM == "3-3n");
}

TEST_CASE("DCMA_DICOM default dictionary retired tag"){
    const auto &dict = DCMA_DICOM::get_default_dictionary();

    // AttachedContours is retired.
    auto it = dict.find({0x3006, 0x0049});
    REQUIRE(it != dict.end());
    CHECK(it->second.retired == true);
    CHECK(it->second.VR == "IS");

    // Modality is not retired.
    it = dict.find({0x0008, 0x0060});
    REQUIRE(it != dict.end());
    CHECK(it->second.retired == false);
}

TEST_CASE("DCMA_DICOM lookup_VM finds tags in default dictionary"){
    CHECK(DCMA_DICOM::lookup_VM(0x0008, 0x0060) == "1");
    CHECK(DCMA_DICOM::lookup_VM(0x0020, 0x0032) == "3");
    CHECK(DCMA_DICOM::lookup_VM(0x0028, 0x0030) == "2");
}

TEST_CASE("DCMA_DICOM lookup_VM returns 1 for group length tags"){
    CHECK(DCMA_DICOM::lookup_VM(0x0008, 0x0000) == "1");
}

TEST_CASE("DCMA_DICOM lookup_VM returns empty for unknown tags"){
    CHECK(DCMA_DICOM::lookup_VM(0x9999, 0x9999) == "");
}

TEST_CASE("DCMA_DICOM lookup_keyword finds tags in default dictionary"){
    CHECK(DCMA_DICOM::lookup_keyword(0x0008, 0x0060) == "Modality");
    CHECK(DCMA_DICOM::lookup_keyword(0x0010, 0x0010) == "PatientName");
}

TEST_CASE("DCMA_DICOM lookup_keyword returns empty for unknown tags"){
    CHECK(DCMA_DICOM::lookup_keyword(0x9999, 0x9999) == "");
}

TEST_CASE("DCMA_DICOM dictionary round-trip with VM and retired"){
    DCMA_DICOM::DICOMDictionary original;
    original[{0x0008, 0x0060}] = {"CS", "Modality", "1", false};
    original[{0x0020, 0x0032}] = {"DS", "ImagePositionPatient", "3", false};
    original[{0x3006, 0x0049}] = {"IS", "AttachedContours", "1-n", true};

    std::stringstream ss;
    DCMA_DICOM::write_dictionary(ss, original);

    auto reloaded = DCMA_DICOM::read_dictionary(ss);
    REQUIRE(reloaded.size() == original.size());

    for(const auto &[key, entry] : original){
        auto it = reloaded.find(key);
        REQUIRE(it != reloaded.end());
        CHECK(it->second.VR == entry.VR);
        CHECK(it->second.keyword == entry.keyword);
        CHECK(it->second.VM == entry.VM);
        CHECK(it->second.retired == entry.retired);
    }
}

TEST_CASE("DCMA_DICOM read_dictionary parses legacy format without VM"){
    // Legacy format: "GGGG,EEEE VR Keyword"
    std::string input =
        "0008,0060 CS Modality\n"
        "0010,0010 PN PatientName\n";
    std::istringstream ss(input);
    auto dict = DCMA_DICOM::read_dictionary(ss);
    REQUIRE(dict.size() == 2);

    auto it = dict.find({0x0008, 0x0060});
    REQUIRE(it != dict.end());
    CHECK(it->second.VR == "CS");
    CHECK(it->second.keyword == "Modality");
    CHECK(it->second.VM == "");
    CHECK(it->second.retired == false);
}

TEST_CASE("DCMA_DICOM read_dictionary parses new format with VM and RETIRED"){
    std::string input =
        "0008,0060 CS 1 Modality\n"
        "3006,0049 IS 1-n AttachedContours RETIRED\n"
        "0020,0037 DS 6 ImageOrientationPatient\n";
    std::istringstream ss(input);
    auto dict = DCMA_DICOM::read_dictionary(ss);
    REQUIRE(dict.size() == 3);

    auto it = dict.find({0x0008, 0x0060});
    REQUIRE(it != dict.end());
    CHECK(it->second.VR == "CS");
    CHECK(it->second.VM == "1");
    CHECK(it->second.keyword == "Modality");
    CHECK(it->second.retired == false);

    it = dict.find({0x3006, 0x0049});
    REQUIRE(it != dict.end());
    CHECK(it->second.VR == "IS");
    CHECK(it->second.VM == "1-n");
    CHECK(it->second.keyword == "AttachedContours");
    CHECK(it->second.retired == true);

    it = dict.find({0x0020, 0x0037});
    REQUIRE(it != dict.end());
    CHECK(it->second.VM == "6");
    CHECK(it->second.retired == false);
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

TEST_CASE("DCMA_DICOM write_dictionary does not corrupt stream state"){
    DCMA_DICOM::DICOMDictionary dict;
    dict[{0x0008, 0x0060}] = {"CS", "Modality"};

    std::ostringstream ss;

    // Write a decimal number before calling write_dictionary.
    ss << std::dec << 42 << "\n";

    DCMA_DICOM::write_dictionary(ss, dict);

    // Record position immediately after write_dictionary returns.
    const auto pos_after_dict = ss.tellp();

    // Write a decimal number after. If write_dictionary corrupted the
    // stream state (e.g., left hex mode active), this would produce "ff"
    // instead of "255".
    ss << 255;

    const std::string appended = ss.str().substr(static_cast<size_t>(pos_after_dict));
    CHECK(appended == "255");
}


// ============================================================================
// Round-trip tests (write then read)
// ============================================================================

static DCMA_DICOM::Node create_minimal_dicom_tree(DCMA_DICOM::Encoding enc){
    DCMA_DICOM::Node root;

    // Meta information group (group 0x0002, always explicit VR little-endian).
    root.emplace_child_node({{0x0002, 0x0001}, "OB", std::string("\x00\x01", 2)}); // FileMetaInformationVersion: major=0, minor=1.

    std::string ts_uid = (enc == DCMA_DICOM::Encoding::ILE)
                         ? "1.2.840.10008.1.2"     // Implicit VR Little Endian.
                         : "1.2.840.10008.1.2.1";  // Explicit VR Little Endian.
    root.emplace_child_node({{0x0002, 0x0002}, "UI", "1.2.840.10008.5.1.4.1.1.2"}); // MediaStorageSOPClassUID: CT Image Storage.
    root.emplace_child_node({{0x0002, 0x0003}, "UI", "1.2.3.4.5.6.7.8.9"});         // MediaStorageSOPInstanceUID.
    root.emplace_child_node({{0x0002, 0x0010}, "UI", ts_uid});                       // TransferSyntaxUID.
    root.emplace_child_node({{0x0002, 0x0012}, "UI", "1.2.3.4.5"});                  // ImplementationClassUID.
    root.emplace_child_node({{0x0002, 0x0013}, "SH", "DCMA_TEST"});                  // ImplementationVersionName.

    // Data elements (encoded per the transfer syntax above).
    root.emplace_child_node({{0x0008, 0x0060}, "CS", "CT"});                          // Modality.
    root.emplace_child_node({{0x0008, 0x0016}, "UI", "1.2.840.10008.5.1.4.1.1.2"});  // SOPClassUID.
    root.emplace_child_node({{0x0008, 0x0018}, "UI", "1.2.3.4.5.6.7.8.9"});          // SOPInstanceUID.
    root.emplace_child_node({{0x0010, 0x0010}, "PN", "DOE^JOHN"});                    // PatientName.
    root.emplace_child_node({{0x0010, 0x0020}, "LO", "12345"});                       // PatientID.
    root.emplace_child_node({{0x0020, 0x0013}, "IS", "1"});                            // InstanceNumber.
    root.emplace_child_node({{0x0028, 0x0010}, "US", "2"});                            // Rows.
    root.emplace_child_node({{0x0028, 0x0011}, "US", "3"});                            // Columns.

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

    // Meta information (group 0x0002, always explicit VR little-endian).
    root.emplace_child_node({{0x0002, 0x0001}, "OB", std::string("\x00\x01", 2)}); // FileMetaInformationVersion.
    root.emplace_child_node({{0x0002, 0x0002}, "UI", "1.2.840.10008.5.1.4.1.1.2"}); // MediaStorageSOPClassUID.
    root.emplace_child_node({{0x0002, 0x0003}, "UI", "1.2.3.4.5.6.7.8.9"});         // MediaStorageSOPInstanceUID.
    root.emplace_child_node({{0x0002, 0x0010}, "UI", "1.2.840.10008.1.2.1"});        // TransferSyntaxUID (ELE).
    root.emplace_child_node({{0x0002, 0x0012}, "UI", "1.2.3.4.5"});                  // ImplementationClassUID.

    // A simple sequence: StructureSetROISequence with two items.
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


// ============================================================================
// Remove / remove_all tests
// ============================================================================

TEST_CASE("DCMA_DICOM remove erases a single tag"){
    auto root = create_minimal_dicom_tree(DCMA_DICOM::Encoding::ELE);
    CHECK(root.find(0x0010, 0x0010) != nullptr);  // PatientName exists.
    CHECK(root.remove(0x0010, 0x0010));
    CHECK(root.find(0x0010, 0x0010) == nullptr);   // PatientName gone.
}

TEST_CASE("DCMA_DICOM remove returns false for missing tags"){
    auto root = create_minimal_dicom_tree(DCMA_DICOM::Encoding::ELE);
    CHECK_FALSE(root.remove(0x9999, 0x9999));
}

TEST_CASE("DCMA_DICOM remove_all erases all matching tags"){
    DCMA_DICOM::Node root;
    root.emplace_child_node({{0x0002, 0x0001}, "OB", std::string("\x00\x01", 2)});
    root.emplace_child_node({{0x0002, 0x0002}, "UI", "1.2.840.10008.5.1.4.1.1.2"});
    root.emplace_child_node({{0x0002, 0x0003}, "UI", "1.2.3.4.5.6.7.8.9"});
    root.emplace_child_node({{0x0002, 0x0010}, "UI", "1.2.840.10008.1.2.1"});
    root.emplace_child_node({{0x0002, 0x0012}, "UI", "1.2.3.4.5"});

    // Add a sequence with nested Referenced SOP Instance UIDs.
    auto *seq = root.emplace_child_node({{0x3006, 0x0020}, "SQ", ""});
    {
        DCMA_DICOM::Node item;
        item.key = {0x3006, 0x0020};
        item.VR = "MULTI";
        item.emplace_child_node({{0x0008, 0x1155}, "UI", "1.2.3.1"});
        seq->emplace_child_node(std::move(item));
    }
    {
        DCMA_DICOM::Node item;
        item.key = {0x3006, 0x0020};
        item.VR = "MULTI";
        item.emplace_child_node({{0x0008, 0x1155}, "UI", "1.2.3.2"});
        seq->emplace_child_node(std::move(item));
    }

    CHECK(root.find_all(0x0008, 0x1155).size() == 2);
    auto removed = root.remove_all(0x0008, 0x1155);
    CHECK(removed == 2);
    CHECK(root.find_all(0x0008, 0x1155).size() == 0);
}


// ============================================================================
// De-identification tests
// ============================================================================

static DCMA_DICOM::Node create_dicom_for_deident(){
    DCMA_DICOM::Node root;

    // Meta information group.
    root.emplace_child_node({{0x0002, 0x0001}, "OB", std::string("\x00\x01", 2)});
    root.emplace_child_node({{0x0002, 0x0002}, "UI", "1.2.840.10008.5.1.4.1.1.2"});
    root.emplace_child_node({{0x0002, 0x0003}, "UI", "1.2.3.4.5.6.7.8.9"});          // MediaStorageSOPInstanceUID.
    root.emplace_child_node({{0x0002, 0x0010}, "UI", "1.2.840.10008.1.2.1"});
    root.emplace_child_node({{0x0002, 0x0012}, "UI", "1.2.3.4.5"});

    // Patient info (PHI).
    root.emplace_child_node({{0x0010, 0x0010}, "PN", "DOE^JOHN"});         // PatientName.
    root.emplace_child_node({{0x0010, 0x0020}, "LO", "12345"});             // PatientID.
    root.emplace_child_node({{0x0010, 0x0030}, "DA", "19800101"});          // PatientBirthDate.
    root.emplace_child_node({{0x0010, 0x0040}, "CS", "M"});                 // PatientSex.
    root.emplace_child_node({{0x0010, 0x1010}, "AS", "044Y"});              // PatientAge (should be erased).
    root.emplace_child_node({{0x0010, 0x1040}, "LO", "123 Main St"});       // PatientAddress (should be erased).

    // Study/series info.
    root.emplace_child_node({{0x0008, 0x0016}, "UI", "1.2.840.10008.5.1.4.1.1.2"}); // SOPClassUID.
    root.emplace_child_node({{0x0008, 0x0018}, "UI", "1.2.3.4.5.6.7.8.9"});          // SOPInstanceUID.
    root.emplace_child_node({{0x0008, 0x0020}, "DA", "20200101"});                     // StudyDate.
    root.emplace_child_node({{0x0008, 0x0060}, "CS", "CT"});                           // Modality.
    root.emplace_child_node({{0x0008, 0x0080}, "LO", "General Hospital"});             // InstitutionName.
    root.emplace_child_node({{0x0008, 0x0090}, "PN", "SMITH^DR"});                     // ReferringPhysicianName.
    root.emplace_child_node({{0x0008, 0x1030}, "LO", "Brain Study"});                  // StudyDescription.
    root.emplace_child_node({{0x0008, 0x103E}, "LO", "Axial T1"});                     // SeriesDescription.

    // UIDs.
    root.emplace_child_node({{0x0020, 0x000D}, "UI", "1.2.3.4.5.6.100"});  // StudyInstanceUID.
    root.emplace_child_node({{0x0020, 0x000E}, "UI", "1.2.3.4.5.6.200"});  // SeriesInstanceUID.
    root.emplace_child_node({{0x0020, 0x0052}, "UI", "1.2.3.4.5.6.300"});  // FrameOfReferenceUID.
    root.emplace_child_node({{0x0020, 0x0013}, "IS", "1"});                  // InstanceNumber.

    return root;
}

TEST_CASE("DCMA_DICOM deidentify erases PHI tags"){
    auto root = create_dicom_for_deident();

    DCMA_DICOM::DeidentifyParams params;
    params.patient_id = "ANON001";
    params.patient_name = "Anonymous";
    params.study_id = "STUDY01";

    DCMA_DICOM::uid_mapping_t uid_map;
    DCMA_DICOM::deidentify(root, params, uid_map);

    // PHI tags should be erased.
    CHECK(root.find(0x0010, 0x1010) == nullptr);  // PatientAge.
    CHECK(root.find(0x0010, 0x1040) == nullptr);  // PatientAddress.
}

TEST_CASE("DCMA_DICOM deidentify replaces patient info"){
    auto root = create_dicom_for_deident();

    DCMA_DICOM::DeidentifyParams params;
    params.patient_id = "ANON001";
    params.patient_name = "Anonymous";
    params.study_id = "STUDY01";

    DCMA_DICOM::uid_mapping_t uid_map;
    DCMA_DICOM::deidentify(root, params, uid_map);

    const auto *pid = root.find(0x0010, 0x0020);
    REQUIRE(pid != nullptr);
    CHECK(pid->val == "ANON001");

    const auto *pname = root.find(0x0010, 0x0010);
    REQUIRE(pname != nullptr);
    CHECK(pname->val == "Anonymous");
}

TEST_CASE("DCMA_DICOM deidentify anonymizes institution and physician"){
    auto root = create_dicom_for_deident();

    DCMA_DICOM::DeidentifyParams params;
    params.patient_id = "ANON001";
    params.patient_name = "Anonymous";
    params.study_id = "STUDY01";

    DCMA_DICOM::uid_mapping_t uid_map;
    DCMA_DICOM::deidentify(root, params, uid_map);

    const auto *inst = root.find(0x0008, 0x0080);
    REQUIRE(inst != nullptr);
    CHECK(inst->val == "Anonymous");

    const auto *ref = root.find(0x0008, 0x0090);
    REQUIRE(ref != nullptr);
    CHECK(ref->val == "Anonymous");
}

TEST_CASE("DCMA_DICOM deidentify clears patient sex"){
    auto root = create_dicom_for_deident();

    DCMA_DICOM::DeidentifyParams params;
    params.patient_id = "ANON001";
    params.patient_name = "Anonymous";
    params.study_id = "STUDY01";

    DCMA_DICOM::uid_mapping_t uid_map;
    DCMA_DICOM::deidentify(root, params, uid_map);

    const auto *sex = root.find(0x0010, 0x0040);
    REQUIRE(sex != nullptr);
    CHECK(sex->val == "");
}

TEST_CASE("DCMA_DICOM deidentify remaps UIDs consistently"){
    auto root = create_dicom_for_deident();

    DCMA_DICOM::DeidentifyParams params;
    params.patient_id = "ANON001";
    params.patient_name = "Anonymous";
    params.study_id = "STUDY01";

    DCMA_DICOM::uid_mapping_t uid_map;
    DCMA_DICOM::deidentify(root, params, uid_map);

    // The UIDs should have been remapped.
    const auto *study_uid = root.find(0x0020, 0x000D);
    REQUIRE(study_uid != nullptr);
    CHECK(study_uid->val != "1.2.3.4.5.6.100");

    const auto *series_uid = root.find(0x0020, 0x000E);
    REQUIRE(series_uid != nullptr);
    CHECK(series_uid->val != "1.2.3.4.5.6.200");

    // The mapping should be recorded.
    CHECK(uid_map.count("1.2.3.4.5.6.100") == 1);
    CHECK(uid_map.count("1.2.3.4.5.6.200") == 1);

    // SOPInstanceUID is shared between 0008,0018 and 0002,0003 -- both should map the same way.
    CHECK(uid_map.count("1.2.3.4.5.6.7.8.9") == 1);
    const auto *sop_uid = root.find(0x0008, 0x0018);
    REQUIRE(sop_uid != nullptr);
    const auto *media_uid = root.find(0x0002, 0x0003);
    REQUIRE(media_uid != nullptr);
    CHECK(sop_uid->val == media_uid->val);
}

TEST_CASE("DCMA_DICOM deidentify uid_map is reused across invocations"){
    auto root1 = create_dicom_for_deident();
    auto root2 = create_dicom_for_deident();

    DCMA_DICOM::DeidentifyParams params;
    params.patient_id = "ANON001";
    params.patient_name = "Anonymous";
    params.study_id = "STUDY01";

    DCMA_DICOM::uid_mapping_t uid_map;

    DCMA_DICOM::deidentify(root1, params, uid_map);
    const auto *study_uid1 = root1.find(0x0020, 0x000D);
    REQUIRE(study_uid1 != nullptr);

    // Second invocation with the same uid_map should produce the same mapped UID.
    DCMA_DICOM::deidentify(root2, params, uid_map);
    const auto *study_uid2 = root2.find(0x0020, 0x000D);
    REQUIRE(study_uid2 != nullptr);

    CHECK(study_uid1->val == study_uid2->val);
}

TEST_CASE("DCMA_DICOM deidentify handles optional study/series descriptions"){
    auto root = create_dicom_for_deident();

    DCMA_DICOM::DeidentifyParams params;
    params.patient_id = "ANON001";
    params.patient_name = "Anonymous";
    params.study_id = "STUDY01";
    params.study_description = "New Study Desc";
    params.series_description = "New Series Desc";

    DCMA_DICOM::uid_mapping_t uid_map;
    DCMA_DICOM::deidentify(root, params, uid_map);

    const auto *study_desc = root.find(0x0008, 0x1030);
    REQUIRE(study_desc != nullptr);
    CHECK(study_desc->val == "New Study Desc");

    const auto *series_desc = root.find(0x0008, 0x103E);
    REQUIRE(series_desc != nullptr);
    CHECK(series_desc->val == "New Series Desc");
}

TEST_CASE("DCMA_DICOM deidentify erases descriptions when not provided"){
    auto root = create_dicom_for_deident();

    DCMA_DICOM::DeidentifyParams params;
    params.patient_id = "ANON001";
    params.patient_name = "Anonymous";
    params.study_id = "STUDY01";
    // study_description and series_description are not set -- tags should be erased.

    DCMA_DICOM::uid_mapping_t uid_map;
    DCMA_DICOM::deidentify(root, params, uid_map);

    // Study and series descriptions should be completely removed.
    CHECK(root.find(0x0008, 0x1030) == nullptr);
    CHECK(root.find(0x0008, 0x103E) == nullptr);
}

TEST_CASE("DCMA_DICOM deidentify preserves modality"){
    auto root = create_dicom_for_deident();

    DCMA_DICOM::DeidentifyParams params;
    params.patient_id = "ANON001";
    params.patient_name = "Anonymous";
    params.study_id = "STUDY01";

    DCMA_DICOM::uid_mapping_t uid_map;
    DCMA_DICOM::deidentify(root, params, uid_map);

    // Modality should not be modified.
    const auto *modality = root.find(0x0008, 0x0060);
    REQUIRE(modality != nullptr);
    CHECK(modality->val == "CT");
}

TEST_CASE("DCMA_DICOM deidentify replaces StudyID with required value"){
    auto root = create_dicom_for_deident();
    root.emplace_child_node({{0x0020, 0x0010}, "SH", "ORIG_STUDY"});  // StudyID.

    DCMA_DICOM::DeidentifyParams params;
    params.patient_id = "ANON001";
    params.patient_name = "Anonymous";
    params.study_id = "NEW_STUDY_ID";

    DCMA_DICOM::uid_mapping_t uid_map;
    DCMA_DICOM::deidentify(root, params, uid_map);

    const auto *study_id = root.find(0x0020, 0x0010);
    REQUIRE(study_id != nullptr);
    CHECK(study_id->val == "NEW_STUDY_ID");
}

TEST_CASE("DCMA_DICOM deidentify inserts StudyID even when absent"){
    auto root = create_dicom_for_deident();
    // No StudyID tag present initially.
    CHECK(root.find(0x0020, 0x0010) == nullptr);

    DCMA_DICOM::DeidentifyParams params;
    params.patient_id = "ANON001";
    params.patient_name = "Anonymous";
    params.study_id = "INSERTED_ID";

    DCMA_DICOM::uid_mapping_t uid_map;
    DCMA_DICOM::deidentify(root, params, uid_map);

    const auto *study_id = root.find(0x0020, 0x0010);
    REQUIRE(study_id != nullptr);
    CHECK(study_id->val == "INSERTED_ID");
}

TEST_CASE("DCMA_DICOM validate warns on VR mismatch with dictionary"){
    DCMA_DICOM::Node root;
    root.emplace_child_node({{0x0002, 0x0001}, "OB", std::string("\x00\x01", 2)});
    root.emplace_child_node({{0x0002, 0x0010}, "UI", "1.2.840.10008.1.2.1"});
    // Modality has dictionary VR of "CS" -- use "LO" to trigger a mismatch warning.
    root.emplace_child_node({{0x0008, 0x0060}, "LO", "CT"});

    const auto &default_dict = DCMA_DICOM::get_default_dictionary();
    std::vector<const DCMA_DICOM::DICOMDictionary*> dicts = { &default_dict };

    // Validate should still succeed (it's a warning, not an error),
    // but should emit a warning about the VR mismatch.
    CHECK(root.validate(DCMA_DICOM::Encoding::ELE, dicts));
}


// ============================================================================
// Transfer syntax classification tests
// ============================================================================

TEST_CASE("DCMA_DICOM classify_transfer_syntax for native syntaxes"){
    CHECK(DCMA_DICOM::classify_transfer_syntax("1.2.840.10008.1.2")
          == DCMA_DICOM::TransferSyntaxType::NativeUncompressed);
    CHECK(DCMA_DICOM::classify_transfer_syntax("1.2.840.10008.1.2.1")
          == DCMA_DICOM::TransferSyntaxType::NativeUncompressed);
    CHECK(DCMA_DICOM::classify_transfer_syntax("1.2.840.10008.1.2.2")
          == DCMA_DICOM::TransferSyntaxType::NativeUncompressed);
}

TEST_CASE("DCMA_DICOM classify_transfer_syntax for encapsulated syntaxes"){
    CHECK(DCMA_DICOM::classify_transfer_syntax("1.2.840.10008.1.2.4.50")
          == DCMA_DICOM::TransferSyntaxType::EncapsulatedJPEG);
    CHECK(DCMA_DICOM::classify_transfer_syntax("1.2.840.10008.1.2.4.70")
          == DCMA_DICOM::TransferSyntaxType::EncapsulatedJPEG);
    CHECK(DCMA_DICOM::classify_transfer_syntax("1.2.840.10008.1.2.4.90")
          == DCMA_DICOM::TransferSyntaxType::EncapsulatedJPEG2000);
    CHECK(DCMA_DICOM::classify_transfer_syntax("1.2.840.10008.1.2.5")
          == DCMA_DICOM::TransferSyntaxType::EncapsulatedRLE);
    CHECK(DCMA_DICOM::classify_transfer_syntax("1.2.840.10008.1.2.4.80")
          == DCMA_DICOM::TransferSyntaxType::EncapsulatedJPEGLS);
    CHECK(DCMA_DICOM::classify_transfer_syntax("1.2.840.10008.1.2.4.201")
          == DCMA_DICOM::TransferSyntaxType::EncapsulatedHTJ2K);
}

TEST_CASE("DCMA_DICOM classify_transfer_syntax strips trailing padding"){
    // UID padded with a null byte (as in some DICOM files).
    CHECK(DCMA_DICOM::classify_transfer_syntax(std::string("1.2.840.10008.1.2.1\0", 20))
          == DCMA_DICOM::TransferSyntaxType::NativeUncompressed);
}

TEST_CASE("DCMA_DICOM classify_transfer_syntax returns Unknown for unrecognized UID"){
    CHECK(DCMA_DICOM::classify_transfer_syntax("1.2.3.4.5.6.7.8.9")
          == DCMA_DICOM::TransferSyntaxType::Unknown);
}

TEST_CASE("DCMA_DICOM is_native_transfer_syntax and is_encapsulated_transfer_syntax"){
    CHECK(DCMA_DICOM::is_native_transfer_syntax(DCMA_DICOM::TransferSyntaxType::NativeUncompressed));
    CHECK_FALSE(DCMA_DICOM::is_native_transfer_syntax(DCMA_DICOM::TransferSyntaxType::EncapsulatedJPEG));
    CHECK(DCMA_DICOM::is_encapsulated_transfer_syntax(DCMA_DICOM::TransferSyntaxType::EncapsulatedJPEG));
    CHECK(DCMA_DICOM::is_encapsulated_transfer_syntax(DCMA_DICOM::TransferSyntaxType::EncapsulatedRLE));
    CHECK_FALSE(DCMA_DICOM::is_encapsulated_transfer_syntax(DCMA_DICOM::TransferSyntaxType::NativeUncompressed));
}


// ============================================================================
// Pixel data descriptor tests
// ============================================================================

static DCMA_DICOM::Node create_image_dicom(uint16_t rows, uint16_t cols,
                                            uint16_t bits_alloc, uint16_t bits_stored,
                                            uint16_t high_bit, uint16_t pixel_rep,
                                            const std::string &photometric,
                                            const std::string &pixel_data_val,
                                            uint16_t samples_per_pixel = 1,
                                            uint16_t planar_config = 0){
    DCMA_DICOM::Node root;

    // Meta information group (always ELE).
    root.emplace_child_node({{0x0002, 0x0001}, "OB", std::string("\x00\x01", 2)});
    root.emplace_child_node({{0x0002, 0x0002}, "UI", "1.2.840.10008.5.1.4.1.1.2"});
    root.emplace_child_node({{0x0002, 0x0003}, "UI", "1.2.3.4.5.6.7.8.9"});
    root.emplace_child_node({{0x0002, 0x0010}, "UI", "1.2.840.10008.1.2.1"});     // ELE (native).
    root.emplace_child_node({{0x0002, 0x0012}, "UI", "1.2.3.4.5"});

    // Image Pixel module tags.
    root.emplace_child_node({{0x0028, 0x0002}, "US", std::to_string(samples_per_pixel)});
    root.emplace_child_node({{0x0028, 0x0004}, "CS", photometric});
    root.emplace_child_node({{0x0028, 0x0006}, "US", std::to_string(planar_config)});
    root.emplace_child_node({{0x0028, 0x0010}, "US", std::to_string(rows)});
    root.emplace_child_node({{0x0028, 0x0011}, "US", std::to_string(cols)});
    root.emplace_child_node({{0x0028, 0x0100}, "US", std::to_string(bits_alloc)});
    root.emplace_child_node({{0x0028, 0x0101}, "US", std::to_string(bits_stored)});
    root.emplace_child_node({{0x0028, 0x0102}, "US", std::to_string(high_bit)});
    root.emplace_child_node({{0x0028, 0x0103}, "US", std::to_string(pixel_rep)});

    // Pixel Data (7FE0,0010).
    root.emplace_child_node({{0x7FE0, 0x0010}, "OW", pixel_data_val});

    return root;
}


TEST_CASE("DCMA_DICOM get_pixel_data_desc populates fields correctly"){
    // 2x3 image, 16 bits allocated, 12 bits stored, high bit 11, unsigned, MONOCHROME2.
    std::string dummy_data(2 * 3 * 2, '\0'); // 2 rows x 3 cols x 2 bytes/pixel.
    auto root = create_image_dicom(2, 3, 16, 12, 11, 0, "MONOCHROME2", dummy_data);

    auto desc = DCMA_DICOM::get_pixel_data_desc(root);
    REQUIRE(desc.has_value());
    CHECK(desc->rows == 2);
    CHECK(desc->columns == 3);
    CHECK(desc->bits_allocated == 16);
    CHECK(desc->bits_stored == 12);
    CHECK(desc->high_bit == 11);
    CHECK(desc->pixel_representation == 0);
    CHECK(desc->photometric_interpretation == "MONOCHROME2");
    CHECK(desc->samples_per_pixel == 1);
    CHECK(desc->transfer_syntax == DCMA_DICOM::TransferSyntaxType::NativeUncompressed);
}

TEST_CASE("DCMA_DICOM get_pixel_data_desc returns nullopt for missing tags"){
    DCMA_DICOM::Node root;
    root.emplace_child_node({{0x0002, 0x0010}, "UI", "1.2.840.10008.1.2.1"});
    // No image pixel tags.
    auto desc = DCMA_DICOM::get_pixel_data_desc(root);
    CHECK_FALSE(desc.has_value());
}

TEST_CASE("DCMA_DICOM get_pixel_data_desc rejects BitsStored > BitsAllocated"){
    std::string dummy(4, '\0');
    auto root = create_image_dicom(1, 1, 8, 16, 7, 0, "MONOCHROME2", dummy);
    auto desc = DCMA_DICOM::get_pixel_data_desc(root);
    CHECK_FALSE(desc.has_value());
}


// ============================================================================
// Native pixel data extraction tests
// ============================================================================

TEST_CASE("DCMA_DICOM extract_native_pixel_data 16-bit unsigned"){
    // 2x2 image, 16-bit unsigned, MONOCHROME2.
    // Pixel values: 100, 200, 300, 400.
    std::string raw(8, '\0');
    uint16_t vals[] = { 100, 200, 300, 400 };
    std::memcpy(&raw[0], vals, 8);

    auto root = create_image_dicom(2, 2, 16, 16, 15, 0, "MONOCHROME2", raw);
    auto pics = DCMA_DICOM::extract_native_pixel_data(root);
    REQUIRE(pics.has_value());
    REQUIRE(pics->images.size() == 1);
    const auto &img = pics->images.front();
    CHECK(img.rows == 2);
    CHECK(img.columns == 2);
    CHECK(img.channels == 1);
    CHECK(img.value(0, 0, 0) == doctest::Approx(100.0f));
    CHECK(img.value(0, 1, 0) == doctest::Approx(200.0f));
    CHECK(img.value(1, 0, 0) == doctest::Approx(300.0f));
    CHECK(img.value(1, 1, 0) == doctest::Approx(400.0f));
}

TEST_CASE("DCMA_DICOM extract_native_pixel_data 16-bit signed"){
    // 1x2 image, 16-bit signed, MONOCHROME2.
    // Pixel values: -100, 32000.
    std::string raw(4, '\0');
    int16_t vals[] = { -100, 32000 };
    std::memcpy(&raw[0], vals, 4);

    auto root = create_image_dicom(1, 2, 16, 16, 15, 1, "MONOCHROME2", raw);
    auto pics = DCMA_DICOM::extract_native_pixel_data(root);
    REQUIRE(pics.has_value());
    REQUIRE(pics->images.size() == 1);
    const auto &img = pics->images.front();
    CHECK(img.value(0, 0, 0) == doctest::Approx(-100.0f));
    CHECK(img.value(0, 1, 0) == doctest::Approx(32000.0f));
}

TEST_CASE("DCMA_DICOM extract_native_pixel_data 8-bit unsigned"){
    // 2x2 image, 8-bit unsigned, MONOCHROME2.
    std::string raw = { '\x00', '\x7F', '\x80', '\xFF' };

    auto root = create_image_dicom(2, 2, 8, 8, 7, 0, "MONOCHROME2", raw);
    auto pics = DCMA_DICOM::extract_native_pixel_data(root);
    REQUIRE(pics.has_value());
    REQUIRE(pics->images.size() == 1);
    const auto &img = pics->images.front();
    CHECK(img.value(0, 0, 0) == doctest::Approx(0.0f));
    CHECK(img.value(0, 1, 0) == doctest::Approx(127.0f));
    CHECK(img.value(1, 0, 0) == doctest::Approx(128.0f));
    CHECK(img.value(1, 1, 0) == doctest::Approx(255.0f));
}

TEST_CASE("DCMA_DICOM extract_native_pixel_data 12-bit stored in 16-bit"){
    // 1x1 image, 16 bits allocated, 12 bits stored, high bit 11, unsigned.
    // Value = 0x0ABC (2748). Bottom 12 bits used, high bit at position 11.
    std::string raw(2, '\0');
    uint16_t val = 0x0ABC;
    std::memcpy(&raw[0], &val, 2);

    auto root = create_image_dicom(1, 1, 16, 12, 11, 0, "MONOCHROME2", raw);
    auto pics = DCMA_DICOM::extract_native_pixel_data(root);
    REQUIRE(pics.has_value());
    REQUIRE(pics->images.size() == 1);
    CHECK(pics->images.front().value(0, 0, 0) == doctest::Approx(2748.0f));
}

TEST_CASE("DCMA_DICOM extract_native_pixel_data 32-bit unsigned"){
    // 1x2 image, 32-bit unsigned.
    std::string raw(8, '\0');
    uint32_t vals[] = { 0, 1000000 };
    std::memcpy(&raw[0], vals, 8);

    auto root = create_image_dicom(1, 2, 32, 32, 31, 0, "MONOCHROME2", raw);
    auto pics = DCMA_DICOM::extract_native_pixel_data(root);
    REQUIRE(pics.has_value());
    REQUIRE(pics->images.size() == 1);
    const auto &img = pics->images.front();
    CHECK(img.value(0, 0, 0) == doctest::Approx(0.0f));
    CHECK(img.value(0, 1, 0) == doctest::Approx(1000000.0f));
}

TEST_CASE("DCMA_DICOM extract_native_pixel_data RGB interleaved"){
    // 1x2 RGB image, 8-bit, interleaved (planar config 0).
    // Pixel 0: R=10, G=20, B=30.  Pixel 1: R=40, G=50, B=60.
    std::string raw = { '\x0A', '\x14', '\x1E', '\x28', '\x32', '\x3C' };

    auto root = create_image_dicom(1, 2, 8, 8, 7, 0, "RGB", raw, 3, 0);
    auto pics = DCMA_DICOM::extract_native_pixel_data(root);
    REQUIRE(pics.has_value());
    REQUIRE(pics->images.size() == 1);
    const auto &img = pics->images.front();
    CHECK(img.rows == 1);
    CHECK(img.columns == 2);
    CHECK(img.channels == 3);
    CHECK(img.value(0, 0, 0) == doctest::Approx(10.0f));
    CHECK(img.value(0, 0, 1) == doctest::Approx(20.0f));
    CHECK(img.value(0, 0, 2) == doctest::Approx(30.0f));
    CHECK(img.value(0, 1, 0) == doctest::Approx(40.0f));
    CHECK(img.value(0, 1, 1) == doctest::Approx(50.0f));
    CHECK(img.value(0, 1, 2) == doctest::Approx(60.0f));
}

TEST_CASE("DCMA_DICOM extract_native_pixel_data returns nullopt for missing pixel data"){
    DCMA_DICOM::Node root;
    root.emplace_child_node({{0x0002, 0x0010}, "UI", "1.2.840.10008.1.2.1"});
    root.emplace_child_node({{0x0028, 0x0010}, "US", "2"});
    root.emplace_child_node({{0x0028, 0x0011}, "US", "2"});
    root.emplace_child_node({{0x0028, 0x0100}, "US", "16"});
    root.emplace_child_node({{0x0028, 0x0101}, "US", "16"});
    root.emplace_child_node({{0x0028, 0x0102}, "US", "15"});
    // No pixel data tag.
    auto pics = DCMA_DICOM::extract_native_pixel_data(root);
    CHECK_FALSE(pics.has_value());
}

TEST_CASE("DCMA_DICOM extract_native_pixel_data Float Pixel Data (7FE0,0008)"){
    // 1x2 image with Float Pixel Data.
    float vals[] = { 1.5f, -2.5f };
    std::string raw(8, '\0');
    std::memcpy(&raw[0], vals, 8);

    DCMA_DICOM::Node root;
    root.emplace_child_node({{0x0002, 0x0010}, "UI", "1.2.840.10008.1.2.1"});
    root.emplace_child_node({{0x0028, 0x0002}, "US", "1"});
    root.emplace_child_node({{0x0028, 0x0004}, "CS", "MONOCHROME2"});
    root.emplace_child_node({{0x0028, 0x0010}, "US", "1"});
    root.emplace_child_node({{0x0028, 0x0011}, "US", "2"});
    root.emplace_child_node({{0x0028, 0x0100}, "US", "32"});
    root.emplace_child_node({{0x0028, 0x0101}, "US", "32"});
    root.emplace_child_node({{0x0028, 0x0102}, "US", "31"});
    root.emplace_child_node({{0x0028, 0x0103}, "US", "0"});
    root.emplace_child_node({{0x7FE0, 0x0008}, "OF", raw});  // Float Pixel Data.

    auto pics = DCMA_DICOM::extract_native_pixel_data(root);
    REQUIRE(pics.has_value());
    REQUIRE(pics->images.size() == 1);
    const auto &img = pics->images.front();
    CHECK(img.value(0, 0, 0) == doctest::Approx(1.5f));
    CHECK(img.value(0, 1, 0) == doctest::Approx(-2.5f));
}

TEST_CASE("DCMA_DICOM extract_native_pixel_data Double Float Pixel Data (7FE0,0009)"){
    // 1x2 image with Double Float Pixel Data.
    double vals[] = { 3.14159265358979, -1.0e10 };
    std::string raw(16, '\0');
    std::memcpy(&raw[0], vals, 16);

    DCMA_DICOM::Node root;
    root.emplace_child_node({{0x0002, 0x0010}, "UI", "1.2.840.10008.1.2.1"});
    root.emplace_child_node({{0x0028, 0x0002}, "US", "1"});
    root.emplace_child_node({{0x0028, 0x0004}, "CS", "MONOCHROME2"});
    root.emplace_child_node({{0x0028, 0x0010}, "US", "1"});
    root.emplace_child_node({{0x0028, 0x0011}, "US", "2"});
    root.emplace_child_node({{0x0028, 0x0100}, "US", "64"});
    root.emplace_child_node({{0x0028, 0x0101}, "US", "64"});
    root.emplace_child_node({{0x0028, 0x0102}, "US", "63"});
    root.emplace_child_node({{0x0028, 0x0103}, "US", "0"});
    root.emplace_child_node({{0x7FE0, 0x0009}, "OD", raw});  // Double Float Pixel Data.

    auto pics = DCMA_DICOM::extract_native_pixel_data(root);
    REQUIRE(pics.has_value());
    REQUIRE(pics->images.size() == 1);
    const auto &img = pics->images.front();
    // Note: double → float truncates from 64-bit to 32-bit IEEE 754, losing
    // significant digits for large values. pi fits in float; -1e10 has ~7 digits
    // of float precision vs ~16 in double, so a relative epsilon of 1e-3 is needed.
    CHECK(img.value(0, 0, 0) == doctest::Approx(3.14159265f).epsilon(1e-5));
    CHECK(img.value(0, 1, 0) == doctest::Approx(-1.0e10f).epsilon(1e-3));
}


// ============================================================================
// Overlay data extraction tests
// ============================================================================

TEST_CASE("DCMA_DICOM extract_overlay_data basic overlay"){
    DCMA_DICOM::Node root;
    root.emplace_child_node({{0x0002, 0x0010}, "UI", "1.2.840.10008.1.2.1"});

    // Overlay plane in group 0x6000: 2x4 = 8 pixels → 1 byte.
    // Bitmap: 0b10101010 → pixels 0,2,4,6 off; pixels 1,3,5,7 on.
    root.emplace_child_node({{0x6000, 0x0010}, "US", "2"});        // Rows.
    root.emplace_child_node({{0x6000, 0x0011}, "US", "4"});        // Columns.
    root.emplace_child_node({{0x6000, 0x0040}, "CS", "G"});        // Type.
    root.emplace_child_node({{0x6000, 0x0050}, "SS", "1\\1"});     // Origin.
    root.emplace_child_node({{0x6000, 0x0100}, "US", "1"});        // BitsAllocated.
    root.emplace_child_node({{0x6000, 0x3000}, "OW", std::string("\xAA", 1)}); // Overlay Data.

    auto overlays = DCMA_DICOM::extract_overlay_data(root);
    REQUIRE(overlays.size() == 1);
    CHECK(overlays[0].first.group == 0x6000);
    CHECK(overlays[0].first.rows == 2);
    CHECK(overlays[0].first.columns == 4);
    CHECK(overlays[0].first.type == "G");
    REQUIRE(overlays[0].second.size() == 8);
    // 0xAA = 10101010 → LSB first: bit0=0, bit1=1, bit2=0, bit3=1, bit4=0, bit5=1, bit6=0, bit7=1.
    CHECK(overlays[0].second[0] == 0);
    CHECK(overlays[0].second[1] == 1);
    CHECK(overlays[0].second[2] == 0);
    CHECK(overlays[0].second[3] == 1);
    CHECK(overlays[0].second[4] == 0);
    CHECK(overlays[0].second[5] == 1);
    CHECK(overlays[0].second[6] == 0);
    CHECK(overlays[0].second[7] == 1);
}

TEST_CASE("DCMA_DICOM extract_overlay_data returns empty for no overlays"){
    DCMA_DICOM::Node root;
    root.emplace_child_node({{0x0002, 0x0010}, "UI", "1.2.840.10008.1.2.1"});
    auto overlays = DCMA_DICOM::extract_overlay_data(root);
    CHECK(overlays.empty());
}


// ============================================================================
// Encapsulated pixel data test (JPEG baseline 8-bit)
// ============================================================================

TEST_CASE("DCMA_DICOM extract_encapsulated_pixel_data JPEG baseline grayscale"){
    // A pre-generated 2x2 grayscale JPEG (quality 100) with pixel values [10, 80, 160, 240].
    // Generated using Pillow: Image.new('L', (2,2)); putpixel values; save JPEG q=100.
    static const uint8_t jpeg_data[] = {
        0xff, 0xd8, 0xff, 0xe0, 0x00, 0x10, 0x4a, 0x46, 0x49, 0x46, 0x00, 0x01,
        0x01, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0xff, 0xdb, 0x00, 0x43,
        0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0xff, 0xc0, 0x00, 0x0b, 0x08, 0x00, 0x02,
        0x00, 0x02, 0x01, 0x01, 0x11, 0x00, 0xff, 0xc4, 0x00, 0x1f, 0x00, 0x00,
        0x01, 0x05, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
        0x09, 0x0a, 0x0b, 0xff, 0xc4, 0x00, 0xb5, 0x10, 0x00, 0x02, 0x01, 0x03,
        0x03, 0x02, 0x04, 0x03, 0x05, 0x05, 0x04, 0x04, 0x00, 0x00, 0x01, 0x7d,
        0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12, 0x21, 0x31, 0x41, 0x06,
        0x13, 0x51, 0x61, 0x07, 0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xa1, 0x08,
        0x23, 0x42, 0xb1, 0xc1, 0x15, 0x52, 0xd1, 0xf0, 0x24, 0x33, 0x62, 0x72,
        0x82, 0x09, 0x0a, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x25, 0x26, 0x27, 0x28,
        0x29, 0x2a, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x43, 0x44, 0x45,
        0x46, 0x47, 0x48, 0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
        0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x73, 0x74, 0x75,
        0x76, 0x77, 0x78, 0x79, 0x7a, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
        0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3,
        0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6,
        0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9,
        0xca, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xe1, 0xe2,
        0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xf1, 0xf2, 0xf3, 0xf4,
        0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xff, 0xda, 0x00, 0x08, 0x01, 0x01,
        0x00, 0x00, 0x3f, 0x00, 0xfe, 0xa4, 0x7e, 0x09, 0x7e, 0xc4, 0x7f, 0xb1,
        0x83, 0x7c, 0x18, 0xf8, 0x44, 0xcd, 0xfb, 0x22, 0x7e, 0xcc, 0x0c, 0xcd,
        0xf0, 0xc3, 0xc0, 0x2c, 0xcc, 0xdf, 0x00, 0xbe, 0x14, 0x96, 0x66, 0x3e,
        0x14, 0xd2, 0x49, 0x66, 0x27, 0xc2, 0x64, 0x92, 0x49, 0x24, 0x92, 0x72,
        0x4f, 0x26, 0xbf, 0xff, 0xd9
    };

    std::string jpeg_str(reinterpret_cast<const char*>(jpeg_data), sizeof(jpeg_data));

    DCMA_DICOM::Node root;
    root.emplace_child_node({{0x0002, 0x0010}, "UI", "1.2.840.10008.1.2.4.50"});  // JPEG Baseline.
    root.emplace_child_node({{0x0028, 0x0002}, "US", "1"});
    root.emplace_child_node({{0x0028, 0x0004}, "CS", "MONOCHROME2"});
    root.emplace_child_node({{0x0028, 0x0010}, "US", "2"});
    root.emplace_child_node({{0x0028, 0x0011}, "US", "2"});
    root.emplace_child_node({{0x0028, 0x0100}, "US", "8"});
    root.emplace_child_node({{0x0028, 0x0101}, "US", "8"});
    root.emplace_child_node({{0x0028, 0x0102}, "US", "7"});
    root.emplace_child_node({{0x0028, 0x0103}, "US", "0"});
    root.emplace_child_node({{0x7FE0, 0x0010}, "OB", jpeg_str});

    auto pics = DCMA_DICOM::extract_encapsulated_pixel_data(root);
    REQUIRE(pics.has_value());
    REQUIRE(pics->images.size() == 1);
    const auto &img = pics->images.front();
    CHECK(img.rows == 2);
    CHECK(img.columns == 2);
    CHECK(img.channels == 1);

    // JPEG is lossy; check pixel values are within 1 intensity unit of the expected value.
    CHECK(std::abs(img.value(0, 0, 0) - 10.0f) <= 1.0f);
    CHECK(std::abs(img.value(0, 1, 0) - 80.0f) <= 1.0f);
    CHECK(std::abs(img.value(1, 0, 0) - 160.0f) <= 1.0f);
    CHECK(std::abs(img.value(1, 1, 0) - 240.0f) <= 1.0f);
}

TEST_CASE("DCMA_DICOM extract_encapsulated_pixel_data returns nullopt for unsupported TS"){
    // JPEG 2000 is not supported.
    DCMA_DICOM::Node root;
    root.emplace_child_node({{0x0002, 0x0010}, "UI", "1.2.840.10008.1.2.4.90"});
    root.emplace_child_node({{0x0028, 0x0010}, "US", "2"});
    root.emplace_child_node({{0x0028, 0x0011}, "US", "2"});
    root.emplace_child_node({{0x0028, 0x0100}, "US", "8"});
    root.emplace_child_node({{0x0028, 0x0101}, "US", "8"});
    root.emplace_child_node({{0x0028, 0x0102}, "US", "7"});
    root.emplace_child_node({{0x7FE0, 0x0010}, "OB", std::string(100, '\0')});

    auto pics = DCMA_DICOM::extract_encapsulated_pixel_data(root);
    CHECK_FALSE(pics.has_value());
}

TEST_CASE("DCMA_DICOM extract_encapsulated_pixel_data returns nullopt for JPEG lossless"){
    DCMA_DICOM::Node root;
    root.emplace_child_node({{0x0002, 0x0010}, "UI", "1.2.840.10008.1.2.4.70"});
    root.emplace_child_node({{0x0028, 0x0010}, "US", "2"});
    root.emplace_child_node({{0x0028, 0x0011}, "US", "2"});
    root.emplace_child_node({{0x0028, 0x0100}, "US", "8"});
    root.emplace_child_node({{0x0028, 0x0101}, "US", "8"});
    root.emplace_child_node({{0x0028, 0x0102}, "US", "7"});
    root.emplace_child_node({{0x7FE0, 0x0010}, "OB", std::string(100, '\0')});

    auto pics = DCMA_DICOM::extract_encapsulated_pixel_data(root);
    CHECK_FALSE(pics.has_value());
}

TEST_CASE("DCMA_DICOM extract_encapsulated_pixel_data returns nullopt for JPEG Extended"){
    // JPEG Extended (Process 2 & 4) TS 1.2.840.10008.1.2.4.51 is not baseline.
    DCMA_DICOM::Node root;
    root.emplace_child_node({{0x0002, 0x0010}, "UI", "1.2.840.10008.1.2.4.51"});
    root.emplace_child_node({{0x0028, 0x0010}, "US", "2"});
    root.emplace_child_node({{0x0028, 0x0011}, "US", "2"});
    root.emplace_child_node({{0x0028, 0x0100}, "US", "8"});
    root.emplace_child_node({{0x0028, 0x0101}, "US", "8"});
    root.emplace_child_node({{0x0028, 0x0102}, "US", "7"});
    root.emplace_child_node({{0x7FE0, 0x0010}, "OB", std::string(100, '\0')});

    auto pics = DCMA_DICOM::extract_encapsulated_pixel_data(root);
    CHECK_FALSE(pics.has_value());
}


// ============================================================================
// Pixel transformation pipeline tests
// ============================================================================

TEST_CASE("DCMA_DICOM get_modality_lut_params reads RescaleSlope and RescaleIntercept"){
    DCMA_DICOM::Node root;
    root.emplace_child_node({{0x0028, 0x1053}, "DS", "2.5"});
    root.emplace_child_node({{0x0028, 0x1052}, "DS", "-1024"});

    auto params = DCMA_DICOM::get_modality_lut_params(root);
    REQUIRE(params.has_value());
    CHECK(params->rescale_slope == doctest::Approx(2.5));
    CHECK(params->rescale_intercept == doctest::Approx(-1024.0));
}

TEST_CASE("DCMA_DICOM get_modality_lut_params returns nullopt when absent"){
    DCMA_DICOM::Node root;
    auto params = DCMA_DICOM::get_modality_lut_params(root);
    CHECK_FALSE(params.has_value());
}

TEST_CASE("DCMA_DICOM apply_modality_lut transforms pixel values"){
    // 1x2 image with stored values 100.0 and 200.0.
    // Slope = 2.0, Intercept = -50.
    // Expected: 2*100 - 50 = 150, 2*200 - 50 = 350.
    planar_image<float,double> img;
    img.init_buffer(1, 2, 1);
    img.init_spatial(1.0, 1.0, 1.0, vec3<double>(0,0,0), vec3<double>(0,0,0));
    img.init_orientation(vec3<double>(1,0,0), vec3<double>(0,1,0));
    img.reference(0, 0, 0) = 100.0f;
    img.reference(0, 1, 0) = 200.0f;

    DCMA_DICOM::ModalityLUTParams p;
    p.rescale_slope = 2.0;
    p.rescale_intercept = -50.0;
    DCMA_DICOM::apply_modality_lut(img, p);

    CHECK(img.value(0, 0, 0) == doctest::Approx(150.0f));
    CHECK(img.value(0, 1, 0) == doctest::Approx(350.0f));
}

TEST_CASE("DCMA_DICOM get_voi_lut_params reads WindowCenter and WindowWidth"){
    DCMA_DICOM::Node root;
    root.emplace_child_node({{0x0028, 0x1050}, "DS", "40"});
    root.emplace_child_node({{0x0028, 0x1051}, "DS", "400"});

    auto params = DCMA_DICOM::get_voi_lut_params(root);
    REQUIRE(params.has_value());
    CHECK(params->window_center == doctest::Approx(40.0));
    CHECK(params->window_width == doctest::Approx(400.0));
}

TEST_CASE("DCMA_DICOM get_voi_lut_params returns nullopt when absent"){
    DCMA_DICOM::Node root;
    auto params = DCMA_DICOM::get_voi_lut_params(root);
    CHECK_FALSE(params.has_value());
}

TEST_CASE("DCMA_DICOM apply_voi_lut clamps and maps pixel values"){
    // Window center = 128, width = 256. Maps [0, 255] → [0, 255].
    // Value below window → 0. Value above → 255. Value in range → proportional.
    planar_image<float,double> img;
    img.init_buffer(1, 3, 1);
    img.init_spatial(1.0, 1.0, 1.0, vec3<double>(0,0,0), vec3<double>(0,0,0));
    img.init_orientation(vec3<double>(1,0,0), vec3<double>(0,1,0));
    img.reference(0, 0, 0) = -100.0f;  // Below window → 0.
    img.reference(0, 1, 0) = 128.0f;   // At center → ~127.5.
    img.reference(0, 2, 0) = 500.0f;   // Above window → 255.

    DCMA_DICOM::VOILUTParams p;
    p.window_center = 128.0;
    p.window_width = 256.0;
    DCMA_DICOM::apply_voi_lut(img, p, 0.0, 255.0);

    CHECK(img.value(0, 0, 0) == doctest::Approx(0.0f));
    CHECK(img.value(0, 1, 0) == doctest::Approx(127.5f).epsilon(0.02));
    CHECK(img.value(0, 2, 0) == doctest::Approx(255.0f));
}

TEST_CASE("DCMA_DICOM get_presentation_lut_shape defaults to Identity"){
    DCMA_DICOM::Node root;
    auto shape = DCMA_DICOM::get_presentation_lut_shape(root);
    CHECK(shape == DCMA_DICOM::PresentationLUTShape::Identity);
}

TEST_CASE("DCMA_DICOM get_presentation_lut_shape reads INVERSE"){
    DCMA_DICOM::Node root;
    root.emplace_child_node({{0x2050, 0x0020}, "CS", "INVERSE"});
    auto shape = DCMA_DICOM::get_presentation_lut_shape(root);
    CHECK(shape == DCMA_DICOM::PresentationLUTShape::Inverse);
}

TEST_CASE("DCMA_DICOM apply_presentation_lut with Inverse"){
    planar_image<float,double> img;
    img.init_buffer(1, 2, 1);
    img.init_spatial(1.0, 1.0, 1.0, vec3<double>(0,0,0), vec3<double>(0,0,0));
    img.init_orientation(vec3<double>(1,0,0), vec3<double>(0,1,0));
    img.reference(0, 0, 0) = 50.0f;
    img.reference(0, 1, 0) = 200.0f;

    DCMA_DICOM::apply_presentation_lut(img, DCMA_DICOM::PresentationLUTShape::Inverse, 255.0);

    CHECK(img.value(0, 0, 0) == doctest::Approx(205.0f));
    CHECK(img.value(0, 1, 0) == doctest::Approx(55.0f));
}

TEST_CASE("DCMA_DICOM apply_presentation_lut with Identity is no-op"){
    planar_image<float,double> img;
    img.init_buffer(1, 1, 1);
    img.init_spatial(1.0, 1.0, 1.0, vec3<double>(0,0,0), vec3<double>(0,0,0));
    img.init_orientation(vec3<double>(1,0,0), vec3<double>(0,1,0));
    img.reference(0, 0, 0) = 42.0f;

    DCMA_DICOM::apply_presentation_lut(img, DCMA_DICOM::PresentationLUTShape::Identity, 255.0);

    CHECK(img.value(0, 0, 0) == doctest::Approx(42.0f));
}

TEST_CASE("DCMA_DICOM convert_photometric_to_rgb YBR_FULL"){
    // Y=128, Cb=128, Cr=128 → R≈128, G≈128, B≈128 (neutral gray).
    planar_image<float,double> img;
    img.init_buffer(1, 1, 3);
    img.init_spatial(1.0, 1.0, 1.0, vec3<double>(0,0,0), vec3<double>(0,0,0));
    img.init_orientation(vec3<double>(1,0,0), vec3<double>(0,1,0));
    img.reference(0, 0, 0) = 128.0f;  // Y
    img.reference(0, 0, 1) = 128.0f;  // Cb
    img.reference(0, 0, 2) = 128.0f;  // Cr

    bool ok = DCMA_DICOM::convert_photometric_to_rgb(img, "YBR_FULL");
    CHECK(ok);
    CHECK(img.value(0, 0, 0) == doctest::Approx(128.0f).epsilon(0.01));
    CHECK(img.value(0, 0, 1) == doctest::Approx(128.0f).epsilon(0.01));
    CHECK(img.value(0, 0, 2) == doctest::Approx(128.0f).epsilon(0.01));
}

TEST_CASE("DCMA_DICOM convert_photometric_to_rgb returns true for RGB (no-op)"){
    planar_image<float,double> img;
    img.init_buffer(1, 1, 3);
    img.init_spatial(1.0, 1.0, 1.0, vec3<double>(0,0,0), vec3<double>(0,0,0));
    img.init_orientation(vec3<double>(1,0,0), vec3<double>(0,1,0));
    img.reference(0, 0, 0) = 10.0f;
    img.reference(0, 0, 1) = 20.0f;
    img.reference(0, 0, 2) = 30.0f;

    bool ok = DCMA_DICOM::convert_photometric_to_rgb(img, "RGB");
    CHECK(ok);
    CHECK(img.value(0, 0, 0) == doctest::Approx(10.0f));
    CHECK(img.value(0, 0, 1) == doctest::Approx(20.0f));
    CHECK(img.value(0, 0, 2) == doctest::Approx(30.0f));
}

TEST_CASE("DCMA_DICOM composable pipeline: modality LUT + VOI LUT + presentation LUT"){
    // Simulate a CT image pipeline:
    // Stored value = 1000 (unsigned 16-bit).
    // Modality LUT: slope=1, intercept=-1024 → HU = -24.
    // VOI LUT: center=40, width=400 → maps [-160, 240] → [0, 255].
    // HU=-24: ((−24 − (40 − 0.5)) / (400 − 1) + 0.5) × 255 ≈ 86.67.
    // Presentation LUT: IDENTITY → no change.
    std::string raw(2, '\0');
    uint16_t stored_val = 1000;
    std::memcpy(&raw[0], &stored_val, 2);

    auto root = create_image_dicom(1, 1, 16, 16, 15, 0, "MONOCHROME2", raw);
    root.emplace_child_node({{0x0028, 0x1053}, "DS", "1"});
    root.emplace_child_node({{0x0028, 0x1052}, "DS", "-1024"});
    root.emplace_child_node({{0x0028, 0x1050}, "DS", "40"});
    root.emplace_child_node({{0x0028, 0x1051}, "DS", "400"});

    auto pics = DCMA_DICOM::extract_native_pixel_data(root);
    REQUIRE(pics.has_value());
    REQUIRE(pics->images.size() == 1);
    auto &img = pics->images.front();
    CHECK(img.value(0, 0, 0) == doctest::Approx(1000.0f));

    // Apply modality LUT.
    auto mlut = DCMA_DICOM::get_modality_lut_params(root);
    REQUIRE(mlut.has_value());
    DCMA_DICOM::apply_modality_lut(img, *mlut);
    CHECK(img.value(0, 0, 0) == doctest::Approx(-24.0f));

    // Apply VOI LUT.
    auto vlut = DCMA_DICOM::get_voi_lut_params(root);
    REQUIRE(vlut.has_value());
    DCMA_DICOM::apply_voi_lut(img, *vlut, 0.0, 255.0);

    // Expected: ((-24 - (40 - 0.5)) / (400 - 1) + 0.5) * 255 ≈ 86.67.
    const double expected = ((-24.0 - 39.5) / 399.0 + 0.5) * 255.0;
    CHECK(img.value(0, 0, 0) == doctest::Approx(static_cast<float>(expected)).epsilon(0.1));

    // Apply presentation LUT (identity → no change).
    auto plut_shape = DCMA_DICOM::get_presentation_lut_shape(root);
    CHECK(plut_shape == DCMA_DICOM::PresentationLUTShape::Identity);
    DCMA_DICOM::apply_presentation_lut(img, plut_shape, 255.0);
    CHECK(img.value(0, 0, 0) == doctest::Approx(static_cast<float>(expected)).epsilon(0.1));
}


// ============================================================================
// VR validation tests (strict mode)
// ============================================================================

TEST_CASE("DCMA_DICOM validate_VR_conformance rejects invalid CS characters"){
    // CS allows uppercase, digits, space, underscore only.
    CHECK(DCMA_DICOM::validate_VR_conformance("CS", "CT", DCMA_DICOM::Encoding::ELE));
    CHECK(DCMA_DICOM::validate_VR_conformance("CS", "ORIGINAL_PRIMARY", DCMA_DICOM::Encoding::ELE));
    CHECK_FALSE(DCMA_DICOM::validate_VR_conformance("CS", "lowercase", DCMA_DICOM::Encoding::ELE));
    CHECK_FALSE(DCMA_DICOM::validate_VR_conformance("CS", "ABC!DEF", DCMA_DICOM::Encoding::ELE));
}

TEST_CASE("DCMA_DICOM validate_VR_conformance rejects invalid AE characters"){
    CHECK(DCMA_DICOM::validate_VR_conformance("AE", "VALID_AE", DCMA_DICOM::Encoding::ELE));
    // Backslash forbidden in AE.
    CHECK_FALSE(DCMA_DICOM::validate_VR_conformance("AE", "ABC\\DEF", DCMA_DICOM::Encoding::ELE));
    // Control character forbidden in AE (adjacent string literals prevent \x01C being parsed as one escape).
    CHECK_FALSE(DCMA_DICOM::validate_VR_conformance("AE", std::string("AB\x01""CD"), DCMA_DICOM::Encoding::ELE));
}

TEST_CASE("DCMA_DICOM validate_VR_conformance rejects too-long AE"){
    CHECK(DCMA_DICOM::validate_VR_conformance("AE", "1234567890123456", DCMA_DICOM::Encoding::ELE));
    CHECK_FALSE(DCMA_DICOM::validate_VR_conformance("AE", "12345678901234567", DCMA_DICOM::Encoding::ELE));
}

TEST_CASE("DCMA_DICOM validate_VR_conformance rejects backslash in SH and LO"){
    CHECK(DCMA_DICOM::validate_VR_conformance("SH", "VALID", DCMA_DICOM::Encoding::ELE));
    CHECK_FALSE(DCMA_DICOM::validate_VR_conformance("SH", "AB\\CD", DCMA_DICOM::Encoding::ELE));
    CHECK(DCMA_DICOM::validate_VR_conformance("LO", "A valid long string", DCMA_DICOM::Encoding::ELE));
    CHECK_FALSE(DCMA_DICOM::validate_VR_conformance("LO", "AB\\CD", DCMA_DICOM::Encoding::ELE));
}

TEST_CASE("DCMA_DICOM validate_VR_conformance rejects control chars in SH/LO except ESC"){
    // ESC (0x1B) is allowed for ISO 2022 escape sequences.
    CHECK(DCMA_DICOM::validate_VR_conformance("SH", std::string("AB\x1B""CD"), DCMA_DICOM::Encoding::ELE));
    // Other control chars forbidden.
    CHECK_FALSE(DCMA_DICOM::validate_VR_conformance("SH", std::string("AB\x01""CD"), DCMA_DICOM::Encoding::ELE));
    CHECK_FALSE(DCMA_DICOM::validate_VR_conformance("LO", std::string("AB\x01""CD"), DCMA_DICOM::Encoding::ELE));
}

TEST_CASE("DCMA_DICOM validate_VR_conformance allows text control chars in ST/LT/UT"){
    // TAB (0x09), LF (0x0A), FF (0x0C), CR (0x0D), ESC (0x1B) are allowed.
    CHECK(DCMA_DICOM::validate_VR_conformance("ST", "Line1\nLine2", DCMA_DICOM::Encoding::ELE));
    CHECK(DCMA_DICOM::validate_VR_conformance("LT", "Col1\tCol2\r\n", DCMA_DICOM::Encoding::ELE));
    CHECK(DCMA_DICOM::validate_VR_conformance("UT", std::string("text\x0C""more"), DCMA_DICOM::Encoding::ELE));
    // Other control chars forbidden.
    CHECK_FALSE(DCMA_DICOM::validate_VR_conformance("ST", std::string("AB\x01""CD"), DCMA_DICOM::Encoding::ELE));
    CHECK_FALSE(DCMA_DICOM::validate_VR_conformance("LT", std::string("AB\x02""CD"), DCMA_DICOM::Encoding::ELE));
}

TEST_CASE("DCMA_DICOM validate_VR_conformance rejects too-long PN component groups"){
    // Each component group (separated by '=') must be <= 64 chars.
    std::string ok_pn = "DOE^JOHN";
    CHECK(DCMA_DICOM::validate_VR_conformance("PN", ok_pn, DCMA_DICOM::Encoding::ELE));

    std::string long_group(65, 'A');
    CHECK_FALSE(DCMA_DICOM::validate_VR_conformance("PN", long_group, DCMA_DICOM::Encoding::ELE));

    // Multiple component groups: each <= 64.
    std::string multi_group = std::string(60, 'A') + "=" + std::string(60, 'B') + "=" + std::string(60, 'C');
    CHECK(DCMA_DICOM::validate_VR_conformance("PN", multi_group, DCMA_DICOM::Encoding::ELE));

    // Too many component groups (>3).
    std::string four_groups = "A=B=C=D";
    CHECK_FALSE(DCMA_DICOM::validate_VR_conformance("PN", four_groups, DCMA_DICOM::Encoding::ELE));
}

TEST_CASE("DCMA_DICOM validate_VR_conformance checks UI per-value length"){
    // Single UID <= 64 bytes.
    CHECK(DCMA_DICOM::validate_VR_conformance("UI", "1.2.3.4.5.6", DCMA_DICOM::Encoding::ELE));
    // Multi-valued UIDs: each <= 64 bytes.
    std::string multi_uid = "1.2.3.4\\5.6.7.8";
    CHECK(DCMA_DICOM::validate_VR_conformance("UI", multi_uid, DCMA_DICOM::Encoding::ELE));

    // Single UID > 64 bytes.
    std::string long_uid(65, '1');
    CHECK_FALSE(DCMA_DICOM::validate_VR_conformance("UI", long_uid, DCMA_DICOM::Encoding::ELE));
}

TEST_CASE("DCMA_DICOM validate_VR_conformance validates UC"){
    CHECK(DCMA_DICOM::validate_VR_conformance("UC", "Some unlimited characters string", DCMA_DICOM::Encoding::ELE));
    // Control chars (except ESC) are forbidden.
    CHECK_FALSE(DCMA_DICOM::validate_VR_conformance("UC", std::string("AB\x01""CD"), DCMA_DICOM::Encoding::ELE));
    // ESC is allowed.
    CHECK(DCMA_DICOM::validate_VR_conformance("UC", std::string("AB\x1B""CD"), DCMA_DICOM::Encoding::ELE));
}

TEST_CASE("DCMA_DICOM validate_VR_conformance validates UR"){
    CHECK(DCMA_DICOM::validate_VR_conformance("UR", "https://example.com/path", DCMA_DICOM::Encoding::ELE));
    // No leading spaces.
    CHECK_FALSE(DCMA_DICOM::validate_VR_conformance("UR", " https://example.com", DCMA_DICOM::Encoding::ELE));
    // No backslash.
    CHECK_FALSE(DCMA_DICOM::validate_VR_conformance("UR", "https://example.com\\path", DCMA_DICOM::Encoding::ELE));
}

TEST_CASE("DCMA_DICOM validate_VR_conformance validates OL alignment"){
    // OL requires 4-byte alignment.
    std::string four_bytes(4, '\x00');
    CHECK(DCMA_DICOM::validate_VR_conformance("OL", four_bytes, DCMA_DICOM::Encoding::ELE));
    std::string three_bytes(3, '\x00');
    CHECK_FALSE(DCMA_DICOM::validate_VR_conformance("OL", three_bytes, DCMA_DICOM::Encoding::ELE));
}

TEST_CASE("DCMA_DICOM validate_VR_conformance validates OV alignment"){
    // OV requires 8-byte alignment.
    std::string eight_bytes(8, '\x00');
    CHECK(DCMA_DICOM::validate_VR_conformance("OV", eight_bytes, DCMA_DICOM::Encoding::ELE));
    std::string seven_bytes(7, '\x00');
    CHECK_FALSE(DCMA_DICOM::validate_VR_conformance("OV", seven_bytes, DCMA_DICOM::Encoding::ELE));
}

TEST_CASE("DCMA_DICOM validate_VR_conformance validates OB max length"){
    // OB should accept data within limits.
    std::string small_ob(100, '\xAB');
    CHECK(DCMA_DICOM::validate_VR_conformance("OB", small_ob, DCMA_DICOM::Encoding::ELE));
}

TEST_CASE("DCMA_DICOM SV round-trip"){
    DCMA_DICOM::Node root;
    root.emplace_child_node({{0x0002, 0x0001}, "OB", std::string("\x00\x01", 2)});
    root.emplace_child_node({{0x0002, 0x0002}, "UI", "1.2.840.10008.5.1.4.1.1.2"});
    root.emplace_child_node({{0x0002, 0x0003}, "UI", "1.2.3.4.5.6.7.8.9"});
    root.emplace_child_node({{0x0002, 0x0010}, "UI", "1.2.840.10008.1.2.1"});
    root.emplace_child_node({{0x0002, 0x0012}, "UI", "1.2.3.4.5"});
    root.emplace_child_node({{0x9999, 0x0001}, "SV", "-1234567890123"});

    std::stringstream ss;
    root.emit_DICOM(ss, DCMA_DICOM::Encoding::ELE);
    REQUIRE(ss.good());

    DCMA_DICOM::DICOMDictionary custom;
    custom[{0x9999, 0x0001}] = {"SV", "TestSV"};
    std::vector<const DCMA_DICOM::DICOMDictionary*> dicts = {&custom};

    DCMA_DICOM::Node read_root;
    ss.seekg(0);
    read_root.read_DICOM(ss, dicts);

    const auto *sv = read_root.find(0x9999, 0x0001);
    REQUIRE(sv != nullptr);
    CHECK(sv->VR == "SV");
    CHECK(sv->val == "-1234567890123");
}

TEST_CASE("DCMA_DICOM UV round-trip"){
    DCMA_DICOM::Node root;
    root.emplace_child_node({{0x0002, 0x0001}, "OB", std::string("\x00\x01", 2)});
    root.emplace_child_node({{0x0002, 0x0002}, "UI", "1.2.840.10008.5.1.4.1.1.2"});
    root.emplace_child_node({{0x0002, 0x0003}, "UI", "1.2.3.4.5.6.7.8.9"});
    root.emplace_child_node({{0x0002, 0x0010}, "UI", "1.2.840.10008.1.2.1"});
    root.emplace_child_node({{0x0002, 0x0012}, "UI", "1.2.3.4.5"});
    root.emplace_child_node({{0x9999, 0x0002}, "UV", "9876543210"});

    std::stringstream ss;
    root.emit_DICOM(ss, DCMA_DICOM::Encoding::ELE);
    REQUIRE(ss.good());

    DCMA_DICOM::DICOMDictionary custom;
    custom[{0x9999, 0x0002}] = {"UV", "TestUV"};
    std::vector<const DCMA_DICOM::DICOMDictionary*> dicts = {&custom};

    DCMA_DICOM::Node read_root;
    ss.seekg(0);
    read_root.read_DICOM(ss, dicts);

    const auto *uv = read_root.find(0x9999, 0x0002);
    REQUIRE(uv != nullptr);
    CHECK(uv->VR == "UV");
    CHECK(uv->val == "9876543210");
}


// ============================================================================
// remove_structural_tags tests
// ============================================================================

TEST_CASE("DCMA_DICOM remove_structural_tags removes GroupLength tags"){
    DCMA_DICOM::Node root;
    root.emplace_child_node({{0x0002, 0x0000}, "UL", "100"});  // GroupLength (should be removed).
    root.emplace_child_node({{0x0002, 0x0001}, "OB", std::string("\x00\x01", 2)});
    root.emplace_child_node({{0x0002, 0x0010}, "UI", "1.2.840.10008.1.2.1"});
    root.emplace_child_node({{0x0008, 0x0000}, "UL", "200"});  // GroupLength (should be removed).
    root.emplace_child_node({{0x0008, 0x0060}, "CS", "CT"});

    auto removed = root.remove_structural_tags();
    CHECK(removed == 2);

    // GroupLength tags should be gone.
    CHECK(root.find(0x0002, 0x0000) == nullptr);
    CHECK(root.find(0x0008, 0x0000) == nullptr);

    // Other tags should remain.
    CHECK(root.find(0x0002, 0x0001) != nullptr);
    CHECK(root.find(0x0008, 0x0060) != nullptr);
}

TEST_CASE("DCMA_DICOM remove_structural_tags on tree without GroupLength returns zero"){
    auto root = create_minimal_dicom_tree(DCMA_DICOM::Encoding::ELE);
    auto removed = root.remove_structural_tags();
    CHECK(removed == 0);
}

TEST_CASE("DCMA_DICOM remove_structural_tags removes nested GroupLength"){
    DCMA_DICOM::Node root;
    root.emplace_child_node({{0x0002, 0x0001}, "OB", std::string("\x00\x01", 2)});
    root.emplace_child_node({{0x0002, 0x0002}, "UI", "1.2.840.10008.5.1.4.1.1.2"});
    root.emplace_child_node({{0x0002, 0x0003}, "UI", "1.2.3.4.5.6.7.8.9"});
    root.emplace_child_node({{0x0002, 0x0010}, "UI", "1.2.840.10008.1.2.1"});
    root.emplace_child_node({{0x0002, 0x0012}, "UI", "1.2.3.4.5"});

    auto *seq = root.emplace_child_node({{0x3006, 0x0020}, "SQ", ""});
    {
        DCMA_DICOM::Node item;
        item.key = {0x3006, 0x0020};
        item.VR = "MULTI";
        item.emplace_child_node({{0x3006, 0x0000}, "UL", "50"});  // Nested GroupLength.
        item.emplace_child_node({{0x3006, 0x0022}, "IS", "1"});
        seq->emplace_child_node(std::move(item));
    }

    auto removed = root.remove_structural_tags();
    CHECK(removed == 1);  // The nested GroupLength.
    CHECK(root.find(0x3006, 0x0000) == nullptr);
    CHECK(root.find(0x3006, 0x0022) != nullptr);
}

TEST_CASE("DCMA_DICOM round-trip with remove_structural_tags eliminates duplicates"){
    // Create a tree that has a manually-added GroupLength tag.
    DCMA_DICOM::Node root;
    root.emplace_child_node({{0x0002, 0x0000}, "UL", "999"});  // Stale GroupLength.
    root.emplace_child_node({{0x0002, 0x0001}, "OB", std::string("\x00\x01", 2)});
    root.emplace_child_node({{0x0002, 0x0002}, "UI", "1.2.840.10008.5.1.4.1.1.2"});
    root.emplace_child_node({{0x0002, 0x0003}, "UI", "1.2.3.4.5.6.7.8.9"});
    root.emplace_child_node({{0x0002, 0x0010}, "UI", "1.2.840.10008.1.2.1"});
    root.emplace_child_node({{0x0002, 0x0012}, "UI", "1.2.3.4.5"});
    root.emplace_child_node({{0x0008, 0x0060}, "CS", "CT"});

    // Remove stale structural tags.
    auto removed = root.remove_structural_tags();
    CHECK(removed == 1);

    // Emit and re-read. The emitter will generate correct GroupLength.
    std::stringstream ss;
    root.emit_DICOM(ss, DCMA_DICOM::Encoding::ELE);
    REQUIRE(ss.good());

    DCMA_DICOM::Node read_root;
    ss.seekg(0);
    read_root.read_DICOM(ss);

    // Should have exactly one GroupLength for group 0x0002 (auto-generated by emitter).
    auto gl_nodes = read_root.find_all(0x0002, 0x0000);
    CHECK(gl_nodes.size() == 1);

    // The modality tag should still be present.
    const auto *modality = read_root.find(0x0008, 0x0060);
    REQUIRE(modality != nullptr);
    CHECK(modality->val == "CT");
}
