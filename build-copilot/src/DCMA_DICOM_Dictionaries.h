// DCMA_DICOM_Dictionaries.h.

#pragma once

#include <cstdint>
#include <iosfwd>
#include <string>
#include <map>
#include <vector>
#include <utility>

namespace DCMA_DICOM {

//////////////
// DICOM Dictionary support.
//
// DICOM tags in files with implicit VR encoding lack an explicit VR field.
// A dictionary is needed to determine the VR for such tags. Multiple dictionaries
// can be layered: a built-in default, optional imported dictionaries, and an
// optional mutable dictionary that learns VRs encountered in explicit-VR files.

struct DICOMDictEntry {
    std::string VR;       // Value representation (e.g., "CS", "UI", "LO").
    std::string keyword;  // Human-readable name (e.g., "Modality"), may be empty.
    std::string VM;       // Value multiplicity (e.g., "1", "1-n", "2", "3-3n"), may be empty.
    bool retired = false; // Whether the tag is retired in the DICOM standard.
};

using dict_key_t = std::pair<uint16_t, uint16_t>;
using DICOMDictionary = std::map<dict_key_t, DICOMDictEntry>;

// Get the built-in default dictionary. Hardcoded for performance. Covers tags used
// throughout DICOMautomaton for common modalities (CT, MR, RT Structure Sets, etc.).
const DICOMDictionary& get_default_dictionary();

// Read a dictionary from a text stream. Supports both the new format (with VM and
// retirement status) and the legacy format (VR and keyword only):
//   New:    "GGGG,EEEE VR VM Keyword [RETIRED]"
//   Legacy: "GGGG,EEEE VR Keyword"
// Lines starting with '#' are comments, blank lines are skipped.
DICOMDictionary read_dictionary(std::istream &is);

// Write a dictionary to a text stream in the format accepted by read_dictionary.
void write_dictionary(std::ostream &os, const DICOMDictionary &dict);

// Look up a VR from layered dictionaries. Additional dictionaries in 'dicts' are
// searched in reverse order, so entries in later dictionaries take precedence over
// earlier ones and over the built-in default dictionary, which is consulted last
// as a fallback. Returns an empty string if the tag is unknown.
std::string lookup_VR(uint16_t group, uint16_t element,
                      const std::vector<const DICOMDictionary*> &dicts = {});

// Look up a VM from layered dictionaries (same precedence rules as lookup_VR).
// Returns an empty string if the tag is unknown or the VM is not recorded.
std::string lookup_VM(uint16_t group, uint16_t element,
                      const std::vector<const DICOMDictionary*> &dicts = {});

// Look up a keyword from layered dictionaries (same precedence rules as lookup_VR).
// Returns an empty string if the tag is unknown or the keyword is not recorded.
std::string lookup_keyword(uint16_t group, uint16_t element,
                           const std::vector<const DICOMDictionary*> &dicts = {});

} // namespace DCMA_DICOM

