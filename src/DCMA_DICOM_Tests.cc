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
