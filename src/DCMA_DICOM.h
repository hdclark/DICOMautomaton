// DCMA_DICOM.h.

#pragma once

#include <cstdint>
#include <iosfwd>
#include <string>
#include <list>
#include <functional>
#include <map>
#include <vector>
#include <utility>
#include <optional>

#include "DCMA_DICOM_Dictionaries.h"

namespace DCMA_DICOM {

struct NodeKey;
struct Node;

//////////////

enum class Encoding {
    ILE,        // Implicit little-endian.
    ELE,        // Explicit little-endian.
    Other       // Other encodings (e.g., big-endian).
};

struct NodeKey {
    uint16_t group   = 0; // The first number in common DICOM tag parlance.
    uint16_t tag     = 0; // The second number in common DICOM tag parlance.
    uint32_t element = 0; // Sequence number (i.e., item number) or element number.
    uint32_t order   = 0; // Rarely used in modern DICOM. Almost always going to be zero.
                          // The instance of the tag. (Modern DICOM prefers explicit sequences.)
};

//////////////

struct Node {

    // Data members.
    NodeKey key;

    std::string VR;    // DICOM Value Representation type. Controls how the tag is serialized and interpretted.
                       // Note that DICOM tags have a default, but the VR doesn't necessarily need to be the default.

    std::string val;   // Payload value for this tag serialized to a string of bytes.

    std::list<Node> children; // Children nodes if this is a sequence tag.

    // Constructors.
    Node();
    Node( NodeKey key,
          std::string VR,
          std::string val );

    // Member functions.
    bool operator==(const Node &) const;
    bool operator!=(const Node &) const;
    bool operator<(const Node &) const;
 
    Node* emplace_child_node(Node &&);

    uint64_t emit_DICOM(std::ostream &os,
                        Encoding enc = Encoding::Other,
                        bool is_root_node = true) const; // Write a DICOM file.

    // Read a DICOM file from the provided stream, populating this node as the root.
    // Optional dictionaries are used for implicit-VR tag lookup (see lookup_VR).
    // If 'mutable_dict' is non-null, it is updated with VRs encountered in
    // explicit-VR files: unknown tags are added, and different-than-expected VRs
    // are recorded. The mutable dictionary can be persisted via write_dictionary.
    void read_DICOM(std::istream &is,
                    const std::vector<const DICOMDictionary*> &dicts = {},
                    DICOMDictionary *mutable_dict = nullptr);

    // Find the first descendant node matching (group, tag).
    Node* find(uint16_t group, uint16_t tag);
    const Node* find(uint16_t group, uint16_t tag) const;

    // Find all descendant nodes matching (group, tag).
    std::list<Node*> find_all(uint16_t group, uint16_t tag);
    std::list<const Node*> find_all(uint16_t group, uint16_t tag) const;

    // Replace the first descendant node matching (group, tag) with the given replacement.
    // Returns true if a replacement was made.
    bool replace(uint16_t group, uint16_t tag, Node replacement);

    // Remove the first descendant node matching (group, tag).
    // Returns true if a node was removed.
    bool remove(uint16_t group, uint16_t tag);

    // Remove all descendant nodes matching (group, tag).
    // Returns the number of nodes removed.
    int64_t remove_all(uint16_t group, uint16_t tag);

    // Get a human-readable string representation of the value, decoding binary VRs as needed.
    // For text VRs this returns the value directly; for binary numeric VRs (US, SS, UL, SL,
    // FL, FD, AT) it decodes to a string; for binary blob VRs (OB, OW, OF, OD, UN) it returns
    // a hex summary or the raw byte count.
    std::string value_str() const;

    // Validate the tree structure as DICOM-conformant. Returns true if the tree is valid.
    // If dictionaries are provided, each tag's VR is compared against the dictionary VR
    // and a warning is emitted when they differ.
    bool validate(Encoding enc = Encoding::ELE,
                  const std::vector<const DICOMDictionary*> &dicts = {}) const;

};

// Evaluate whether the contents fit the DICOM VR.
bool validate_VR_conformance( const std::string &VR,
                              const std::string &val,
                              DCMA_DICOM::Encoding enc = DCMA_DICOM::Encoding::ELE );

//////////////
// DICOM Clinical Trial De-Identification (based on DICOM Supplement 142).
//
// This function performs de-identification of a parsed DICOM Node tree, erasing or
// replacing tags that may contain protected health information (PHI). UID tags are
// consistently remapped using a caller-provided mapping that is updated in-place,
// ensuring cross-file UID consistency.

struct DeidentifyParams {
    // Required parameters.
    std::string patient_id;    // New patient ID to assign.
    std::string patient_name;  // New patient name to assign.
    std::string study_id;      // New study ID to assign.

    // Optional parameters -- if provided (non-empty), override the tag in the output;
    // if not provided, the tag is erased.
    std::optional<std::string> study_description;
    std::optional<std::string> series_description;
};

using uid_mapping_t = std::map<std::string, std::string>;

// De-identify a DICOM Node tree according to DICOM Supplement 142.
//
// 'root' is the parsed DICOM tree (modified in-place).
// 'params' specifies the replacement patient info and optional overrides.
// 'uid_map' is a UID mapping (old UID -> new UID) that is consulted and updated.
//   - If an existing UID is found in uid_map, the mapped value is reused.
//   - If an existing UID is not in uid_map, a new UID is generated and stored.
//   - The updated uid_map should be passed to subsequent invocations to ensure
//     UID consistency across files.
void deidentify(Node &root,
                const DeidentifyParams &params,
                uid_mapping_t &uid_map);


} // namespace DCMA_DICOM

