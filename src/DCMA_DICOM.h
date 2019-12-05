// DCMA_DICOM.h.

#pragma once

#include <iosfwd>
#include <functional>

#include <list>

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

struct Node {

    // Data members.
    NodeKey key;

    std::string VR;    // DICOM VR type. Controls how the tag is serialized and interpretted.
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

};


} // namespace DCMA_DICOM

