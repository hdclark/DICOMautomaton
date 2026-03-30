// DCMA_DICOM.cc - A part of DICOMautomaton 2019, 2026. Written by hal clark.
//
// This file contains routines for reading and writing DICOM files.
//


#include <cstdint>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <list>
#include <map>
#include <array>
#include <vector>
#include <tuple>
#include <functional>
#include <utility>
#include <algorithm>
#include <limits>
#include <cstring>
#include <stdexcept>
#include <cctype>

#include "YgorMisc.h"
#include "YgorLog.h"
#include "YgorString.h"
#include "YgorTime.h"

#include "Metadata.h"
#include "DCMA_DICOM.h"

namespace DCMA_DICOM {

// IEEE 754:1985 compliance checks for DICOM floating-point VRs (FL, FD, OF, OD).
static_assert(std::numeric_limits<float>::is_iec559 && sizeof(float) == 4,
              "DICOM requires IEEE 754:1985 32-bit single-precision floats.");
static_assert(std::numeric_limits<double>::is_iec559 && sizeof(double) == 8,
              "DICOM requires IEEE 754:1985 64-bit double-precision floats.");

// All non-retired transfer syntaxs require the use of little-endian byte ordering.
// The readers and writers in this file assume the host is little-endian, so verify.
static_assert(YgorEndianness::Host == YgorEndianness::Little, 
              "All non-retired DICOM transfer syntaxes require little-endian encoding.");


// Format a tag (group, element) as a hex string like "(0028,1052)" for diagnostic messages.
static std::string tag_diag(uint16_t group, uint16_t tag){
    std::ostringstream ss;
    ss << "(" << std::hex << std::setfill('0')
       << std::setw(4) << group << ","
       << std::setw(4) << tag << ")";
    return ss.str();
}

// Describe a character for diagnostic purposes. Printable chars are quoted; others shown as hex.
static std::string char_diag(char c){
    if(std::isprint(static_cast<unsigned char>(c))){
        return std::string("'") + c + "'";
    }
    std::ostringstream ss;
    ss << "0x" << std::hex << std::setfill('0')
       << std::setw(2) << static_cast<unsigned>(static_cast<unsigned char>(c));
    return ss.str();
}

// Find the first character in 'val' not in 'allowed' and describe it for diagnostics.
static std::string first_invalid_char_diag(const std::string &val, const std::string &allowed){
    auto pos = val.find_first_not_of(allowed);
    if(pos != std::string::npos){
        return char_diag(val[pos]);
    }
    return "(unknown)";
}

// DICOM PS3.5 Section 6.2: Check if string contains any control characters (0x00-0x1F, 0x7F).
// Used for text VR validation (e.g., AE) where all control characters are forbidden.
// Note: NULL (0x00) is included -- while it is used as a padding character for UI/OB,
// it is not valid within text VR values themselves.
static bool has_control_char(const std::string &val){
    for(const auto c : val){
        const auto uc = static_cast<unsigned char>(c);
        if(uc <= 0x1F || uc == 0x7F) return true;
    }
    return false;
}

// DICOM PS3.5 Section 6.2: Check for control characters excluding ESC (0x1B).
// Used for VRs SH, LO, PN, UC which allow ESC for ISO/IEC 2022 escape sequences.
static bool has_control_char_except_esc(const std::string &val){
    for(const auto c : val){
        const auto uc = static_cast<unsigned char>(c);
        if((uc <= 0x1F || uc == 0x7F) && uc != 0x1B) return true;
    }
    return false;
}

// DICOM PS3.5 Section 6.2: Check for control characters excluding TAB, LF, FF, CR, ESC.
// Used for VRs ST, LT, UT which allow text formatting control characters.
static bool has_control_char_except_text(const std::string &val){
    for(const auto c : val){
        const auto uc = static_cast<unsigned char>(c);
        if((uc <= 0x1F || uc == 0x7F)
           && uc != 0x09   // TAB
           && uc != 0x0A   // LF
           && uc != 0x0C   // FF
           && uc != 0x0D   // CR
           && uc != 0x1B)  // ESC
            return true;
    }
    return false;
}

struct Node;

Node::Node() = default;

Node::Node(NodeKey key,
           std::string VR,
           std::string val) 
         : key(key),
           VR(std::move(VR)),
           val(std::move(val)) {}


bool Node::operator==(const Node &rhs) const {
    if(this == &rhs) return true;

    auto l = std::make_tuple(this->key.group, this->key.order, 
                             this->key.tag,   this->key.element);
    auto r = std::make_tuple(rhs.key.group,   rhs.key.order, 
                             rhs.key.tag,     rhs.key.element);
    return (l == r);
}

bool Node::operator!=(const Node &rhs) const {
    return !(*this == rhs);
}

bool Node::operator<(const Node &rhs) const {
    if(this == &rhs) return false;

    auto l = std::make_tuple(this->key.group, this->key.order, 
                             this->key.tag,   this->key.element);
    auto r = std::make_tuple(rhs.key.group,   rhs.key.order, 
                             rhs.key.tag,     rhs.key.element);
    return (l < r);
}

Node *
Node::emplace_child_node(Node &&n){
    Node *child_node = &( this->children.emplace_back(std::forward<Node>(n)) ); // Requires C++17.

    if( (child_node->VR == "MULTI")
    &&  (this->VR != "SQ")
    &&  (1 < this->children.size()) ){
        throw std::invalid_argument("'MULTI' nodes should only have siblings when the parent is a 'SQ' node. Refusing to continue.");
        // Note: Improper use of this tag can result in invalid DICOM files (i.e., DICOM tags can be disordered).
        // This issue can be mitigated by ensuring MULTI nodes do not have any sibling nodes (except when the parent is a sequence node).
        //
        // If this functionality is truly needed (e.g., to support DICOM modularization) then siblings not under a 'SQ'
        // node will have to be emitted in the correct monotonically-increasing DICOM group ordering. This will require
        // 'peeking' into sibling nodes when nodes are being emitted to a stream.
    }

    this->children.sort(); // Ensure the nodes are sorted as per DICOM standard.
                           // Note: This enables the DICOM write function to be const.

    return child_node;
}

// This routine writes the provided type to the provided stream. It verifies encoding, the expected length (in bytes)
// are available to be written (and there are no extras). The number of bytes written is returned.
template<class T>
uint64_t
write_to_stream( std::ostream &os,
                 const T &x,
                 uint64_t expected_length,
                 Encoding enc ){

    // Verify encoding can be handled.
    if( (enc != Encoding::ILE) 
    &&  (enc != Encoding::ELE) ){
        throw std::runtime_error("Encoding is not little-endian. This is not currently supported.");
    }

    // Verify this is a little-endian machine.
    //
    // Note: When C++20 is available we can use std::endian at compile time here instead.
    //
    // Note: We could emit DICOM files in little- or big-endian using a technique independent of the computer, but for
    //       simplicity this is not currently done.
    {
        uint16_t test { 0x01 };
        auto * first_byte = reinterpret_cast<unsigned char *>(&test);
        //static_assert((static_cast<uint16_t>(*first_byte) == test), "This computer is not little-endian. This is not supported.");
        if(static_cast<uint16_t>(*first_byte) != test){
            throw std::runtime_error("This computer is not little-endian. This is not supported.");
        }
    }

    // Ensure the correct number of bytes can be written.
    if(sizeof(T) != expected_length){
        throw std::runtime_error("Expected number of bytes does not match type size. (Is this intentional?)");
    }

    // Write the bytes.
    uint64_t written_length = 0;

    os.write(reinterpret_cast<const char *>(&x), sizeof(x));
    written_length = expected_length;

    return written_length;
}

// Explicit instantiation for writing raw bytes via std::string.
//
// Note: The contents of the string will be written in byte order, so endian conversion is not relevant for this
// routine.
template<>
uint64_t
write_to_stream( std::ostream &os,
                 const std::string &x,
                 uint64_t expected_length,
                 Encoding enc ){

    // Verify encoding can be handled.
    if( (enc != Encoding::ILE) 
    &&  (enc != Encoding::ELE) ){
        throw std::runtime_error("Encoding is not little-endian. This is not currently supported.");
    }

    // Ensure the correct number of bytes can be written.
    //
    // Note: An exact length match is required here to try catch errors between expected lengths (e.g., 2-byte VR's) and
    //       actual string contents. If the entire string should be written, supply the length of the string.
    const auto available_length = static_cast<uint64_t>(x.length());
    if(available_length != expected_length){
        throw std::runtime_error("Expected number of bytes in string does not match type size. (Is this intentional?)");
    }

    // Write the bytes.
    uint64_t written_length = 0;

    os.write(reinterpret_cast<const char *>(x.data()), available_length);
    written_length = available_length;

    return written_length;
}



// This routine emits a DICOM tag using the provided string of bytes payload. This routine handles writing the DICOM
// structure.
//
// NOTE: The payload is treated as a string of bytes and is not interpretted or adjusted for endianness.
//       All pre-processing should be taken care of before this routine is invoked.
uint64_t
emit_DICOM_tag(std::ostream &os,
               Encoding enc,
               const Node &node,   // Does not use the node's val member, to allow pre-processing.
               const std::string& val,
               bool lenient = false){

    uint64_t written_length = 0;

    written_length += write_to_stream(os, node.key.group, 2, enc);
    written_length += write_to_stream(os, node.key.tag, 2, enc);

    // With implicit encoding all tags are written in the same way.
    if(enc == Encoding::ILE){
        // Deal with sequences separately.
        if( node.VR == "SQ" ){
            if(!val.empty()){
                throw std::logic_error("'SQ' VR node passed data, but they can not have any data associated with them. (Is it intentional?)");
            }

            // Recursively emit the children to determine their lengths.
            uint64_t seq_length = 0;
            std::ostringstream seq_ss(std::ios_base::ate | std::ios_base::binary);

            for(const auto &n : node.children){
                std::ostringstream child_ss(std::ios_base::ate | std::ios_base::binary);
                const uint64_t child_length = n.emit_DICOM(child_ss, enc, false, lenient);
                const auto child_length_32 = static_cast<uint32_t>(child_length);

                seq_length += write_to_stream(seq_ss, static_cast<uint16_t>(0xFFFE), 2, enc); // group.
                seq_length += write_to_stream(seq_ss, static_cast<uint16_t>(0xE000), 2, enc); // tag.
                seq_length += write_to_stream(seq_ss, child_length_32, 4, enc);
                seq_length += write_to_stream(seq_ss, child_ss.str(), child_length, enc);
            }

            // Emit the full child lengths and serialized children.
            const auto seq_length_32 = static_cast<uint32_t>(seq_length);
            written_length += write_to_stream(os, seq_length_32, 4, enc);
            written_length += write_to_stream(os, seq_ss.str(), seq_length, enc);

        // All others.
        }else{
            const auto length    = static_cast<uint32_t>(val.length());
            const auto add_space = static_cast<uint32_t>(length % 2);
            const uint32_t full_length = (length + add_space);
            // DICOM PS3.5 Section 6.2: UI and binary/byte-stream VRs are padded with
            // NULL (0x00); all other (text) VRs are padded with SPACE (0x20).
            const bool is_null_padded = (node.VR == "UI" || node.VR == "OB" || node.VR == "OW"
                                      || node.VR == "OF" || node.VR == "OD" || node.VR == "OL"
                                      || node.VR == "OV" || node.VR == "UN" || node.VR == "SV"
                                      || node.VR == "UV");
            const auto space_char = is_null_padded ? static_cast<unsigned char>('\0')
                                                   : static_cast<unsigned char>(' ');

            written_length += write_to_stream(os, full_length, 4, enc);
            written_length += write_to_stream(os, val, length, enc);
            if(0 < add_space) written_length += write_to_stream(os, space_char, 1, enc); // Ensure length is divisible by 2.
        }

    // With explicit encoding the VR is explicitly mentioned.
    }else if(enc == Encoding::ELE){

        // Deal with sequences separately.
        if( node.VR == "SQ" ){
            if(!val.empty()){
                throw std::logic_error("'SQ' VR node passed data, but they can not have any data associated with them. (Is it intentional?)");
            }

            const uint16_t zero_16 = 0;
            written_length += write_to_stream(os, node.VR, 2, enc);
            written_length += write_to_stream(os, zero_16, 2, enc); // "Reserved" space.

            // Recursively emit the children to determine their lengths.
            uint64_t seq_length = 0;
            std::ostringstream seq_ss(std::ios_base::ate | std::ios_base::binary);

            for(const auto &n : node.children){
                std::ostringstream child_ss(std::ios_base::ate | std::ios_base::binary);
                const uint64_t child_length = n.emit_DICOM(child_ss, enc, false, lenient);
                const auto child_length_32 = static_cast<uint32_t>(child_length);

                // Emit a tag containing the length of the child.
                seq_length += write_to_stream(seq_ss, static_cast<uint16_t>(0xFFFE), 2, enc); // group.
                seq_length += write_to_stream(seq_ss, static_cast<uint16_t>(0xE000), 2, enc); // tag.
                seq_length += write_to_stream(seq_ss, child_length_32, 4, enc);
                seq_length += write_to_stream(seq_ss, child_ss.str(), child_length, enc);
            }

            // Emit the full child lengths and serialized children.
            const auto seq_length_32 = static_cast<uint32_t>(seq_length);
            written_length += write_to_stream(os, seq_length_32, 4, enc);
            written_length += write_to_stream(os, seq_ss.str(), seq_length, enc);

        // DICOM PS3.5 Section 7.1.2, Table 7.1-1: VRs not listed in Table 7.1-2 use
        // 2 reserved bytes (set to 0000H) followed by a 32-bit unsigned length field.
        // This includes: OB, OD, OF, OL, OV, OW, SV, UC, UN, UR, UT, UV.
        }else if( (node.VR == "OB")
              ||  (node.VR == "OD")
              ||  (node.VR == "OF")
              ||  (node.VR == "OL")
              ||  (node.VR == "OV")
              ||  (node.VR == "OW")
              ||  (node.VR == "SV")
              ||  (node.VR == "UC")
              ||  (node.VR == "UN")
              ||  (node.VR == "UR")
              ||  (node.VR == "UT")
              ||  (node.VR == "UV") ){
            const auto length    = static_cast<uint32_t>(val.length());
            const auto add_space = static_cast<uint32_t>(length % 2);
            const uint32_t full_length = (length + add_space);
            const uint16_t zero_16 = 0;
            // DICOM PS3.5 Section 6.2: Text VRs (UC, UR, UT) are padded with SPACE (0x20);
            // binary/byte-stream VRs (OB, OD, OF, OL, OV, OW, SV, UN, UV) with NULL (0x00).
            const auto space_char = static_cast<uint8_t>(
                (node.VR == "UC" || node.VR == "UR" || node.VR == "UT") ? 0x20 : 0x00);

            written_length += write_to_stream(os, node.VR, 2, enc);
            written_length += write_to_stream(os, zero_16, 2, enc); // "Reserved" space.
            written_length += write_to_stream(os, full_length, 4, enc);

            written_length += write_to_stream(os, val, length, enc);
            if(0 < add_space) written_length += write_to_stream(os, space_char, 1, enc); // Ensure length is divisible by 2.

        // All others do not.
        }else{
            const auto length    = static_cast<uint16_t>(val.length());
            const auto add_space = static_cast<uint16_t>(length % 2);
            const uint16_t full_length = (length + add_space);
            const auto space_char = (node.VR == "UI") ? static_cast<unsigned char>('\0')
                                                      : static_cast<unsigned char>(' ');

            written_length += write_to_stream(os, node.VR, 2, enc);
            written_length += write_to_stream(os, full_length, 2, enc);

            written_length += write_to_stream(os, val, length, enc);
            if(0 < add_space) written_length += write_to_stream(os, space_char, 1, enc); // Ensure length is divisible by 2.
        }
         
    }else{
        throw std::runtime_error("Unsupported encoding specified. Refusing to continue.");
    }

    return written_length;
}

// This routine will recursively write a DICOM file from this node and all of its children.
uint64_t Node::emit_DICOM(std::ostream &os,
                          Encoding enc,
                          bool is_root_node,
                          bool lenient) const {

    YLOGDEBUG("emit_DICOM: tag " << tag_diag(this->key.group, this->key.tag)
              << " VR='" << this->VR << "'"
              << " is_root=" << is_root_node
              << " lenient=" << lenient);

    // Used to search for forbidden characters.
    const std::string upper_case("ABCDEFGHIJKLMNOPQRSTUVWXYZ");
    const std::string lower_case("abcdefghijklmnopqrstuvwxyz");
    const std::string number_digits("0123456789");
    const std::string multiplicity(R"***(\)***"); // Value multiplicity separator.

    // Convenience string identifying this tag for diagnostic messages.
    const auto get_tag_str = [&](){
        return tag_diag(this->key.group, this->key.tag);
    };
    const auto get_tag_and_val_str = [&](){
        const bool is_printable = std::all_of(std::begin(this->val), std::end(this->val),
                                              [](unsigned char c) {
                                                  return std::isprint(c);
                                              });

        const auto display_val = is_printable ? this->val : "(not printable)";
        return tag_diag(this->key.group, this->key.tag) + "='" + display_val + "'";
    };


    uint64_t cumulative_length = 0;

    // If this is the root node, ignore the VR and treat it as a simple container of children.
    if(is_root_node){
        // Verify the node does not have any data associated with it. If it does, it probably indicates a logic
        // error since only children nodes should contain data.
        if(!this->val.empty()){
            throw std::logic_error("Nodes with 'SQ' VR can not have any data associated with them. (Is it intentional?)");
        }

        // Emit the DICM header before processing any nodes.
        const std::string header = std::string(128, '\0') + std::string("DICM");
        cumulative_length += write_to_stream(os, header, 132, enc);

        // Process children nodes. To generate group lengths we need to emit them in bunches.
        std::ostringstream child_ss(std::ios_base::ate | std::ios_base::binary);
        uint64_t group_length = 0;
        const auto end_child = std::end(this->children);
        for(auto child_it = std::begin(this->children); child_it != end_child; ++child_it){
            
            // Always emit the meta information header tags (group = 0x0002) with little endian explicit encoding.
            Encoding child_enc = (child_it->key.group <= 0x0002) ? Encoding::ELE : enc;
                
            // Emit this node into the temp buffer.
            group_length += child_it->emit_DICOM(child_ss, child_enc, false, lenient);

            // Evaluate whether the following node will be from a different group.
            // If so, emit the group length tag and all children in the buffer.
            const auto next_child_it = std::next(child_it);
            if( (next_child_it == end_child) 
            ||  (child_it->key.group != next_child_it->key.group) ){

                // Emit the group length tag.
                if( (child_it->key.group <= 0x0002)
                &&  (child_enc == Encoding::ELE) ){  // TODO: Should I bother with this after group 0x0002? It is deprecated...
                    Node group_length_node({child_it->key.group, 0x0000}, "UL", std::to_string(group_length));
                    cumulative_length += group_length_node.emit_DICOM(os, child_enc, false, lenient);
                }

                // Emit all the children from the buffer.
                os.write(reinterpret_cast<const char*>(child_ss.str().data()), group_length);
                cumulative_length += group_length;

                // Reset the children buffer.
                group_length = 0;
                std::ostringstream new_ss(std::ios_base::ate | std::ios_base::binary);
                child_ss.swap(new_ss);
            }
        }

    }else if( this->VR == "MULTI" ){ 
        // Not a true DICOM VR. Used to emit children without any boilerplate (cf. the 'SQ' VR).
        
        // Verify the node does not have any data associated with it. If it does, it probably indicates a logic
        // error since only children nodes should contain data.
        if(!this->val.empty()){
            throw std::logic_error("'MULTI' nodes can not have any data associated with them. (Is it intentional?) " + get_tag_and_val_str());
        }

        // Process children nodes serially, without any boilerplate or markers between children.
        for(const auto & c : this->children){
            cumulative_length += c.emit_DICOM(os, enc, false, lenient);
        }

    // Text types.
    }else if( this->VR == "CS" ){ //Code strings.
        // DICOM PS3.5 Section 6.2: CS - Code String.
        // Character repertoire: Uppercase letters, "0"-"9", SPACE, underscore "_".
        // Maximum length: 16 bytes per value.
        if(!lenient){
            // Value multiplicity embiggens the maximum permissable length, but each individual element should be <= 16 chars.
            auto tokens = SplitStringToVector(this->val,'\\','d');
            for(const auto &token : tokens){
                if(16ULL < token.length()){
                    throw std::invalid_argument("Code string is too long at tag " + get_tag_and_val_str() + ". Cannot continue.");
                }
            }

            const auto allowed_cs = upper_case + number_digits + multiplicity + "_ ";
            if(this->val.find_first_not_of(allowed_cs) != std::string::npos){
                throw std::invalid_argument("Invalid character " + first_invalid_char_diag(this->val, allowed_cs)
                                            + " found in code string at tag " + get_tag_and_val_str() + ". Cannot continue.");
            }
        }
        cumulative_length += emit_DICOM_tag(os, enc, *this, this->val, lenient);

    }else if( this->VR == "SH" ){ //Short string.
        // DICOM PS3.5 Section 6.2: SH - Short String.
        // Character repertoire: Default, excluding backslash (5CH) and all control characters except ESC.
        // Maximum length: 16 characters.
        if(!lenient){
            if(16ULL < this->val.length()){
                throw std::runtime_error("Short string is too long at tag " + get_tag_and_val_str() + ". Consider using a longer VR. Cannot continue.");
            }
            if(this->val.find('\\') != std::string::npos){
                throw std::invalid_argument("Backslash (value multiplicity delimiter) found in SH at tag " + get_tag_and_val_str() + ". Cannot continue.");
            }
            if(has_control_char_except_esc(this->val)){
                throw std::invalid_argument("Forbidden control character found in SH at tag " + get_tag_and_val_str() + ". Cannot continue.");
            }
        }
        cumulative_length += emit_DICOM_tag(os, enc, *this, this->val, lenient);

    }else if( this->VR == "LO" ){ //Long strings.
        // DICOM PS3.5 Section 6.2: LO - Long String.
        // Character repertoire: Default, excluding backslash (5CH) and all control characters except ESC.
        // Maximum length: 64 characters.
        if(!lenient){
            if(64ULL < this->val.length()){
                throw std::runtime_error("Long string is too long at tag " + get_tag_and_val_str() + ". Consider using a longer VR. Cannot continue.");
            }
            if(this->val.find('\\') != std::string::npos){
                throw std::invalid_argument("Backslash (value multiplicity delimiter) found in LO at tag " + get_tag_and_val_str() + ". Cannot continue.");
            }
            if(has_control_char_except_esc(this->val)){
                throw std::invalid_argument("Forbidden control character found in LO at tag " + get_tag_and_val_str() + ". Cannot continue.");
            }
        }
        cumulative_length += emit_DICOM_tag(os, enc, *this, this->val, lenient);

    }else if( this->VR == "ST" ){ //Short text.
        // DICOM PS3.5 Section 6.2: ST - Short Text.
        // Character repertoire: Default, excluding control characters except TAB, LF, FF, CR, ESC.
        // Maximum length: 1024 characters. Not multi-valued (backslash allowed).
        if(!lenient){
            if(1024ULL < this->val.length()){
                throw std::runtime_error("Short text is too long at tag " + get_tag_str() + ". Consider using a longer VR. Cannot continue.");
            }
            if(has_control_char_except_text(this->val)){
                throw std::invalid_argument("Forbidden control character found in ST at tag " + get_tag_str() + ". Cannot continue.");
            }
        }
        cumulative_length += emit_DICOM_tag(os, enc, *this, this->val, lenient);

    }else if( this->VR == "LT" ){ //Long text.
        // DICOM PS3.5 Section 6.2: LT - Long Text.
        // Character repertoire: Default, excluding control characters except TAB, LF, FF, CR, ESC.
        // Maximum length: 10240 characters. Not multi-valued (backslash allowed).
        if(!lenient){
            if(10240ULL < this->val.length()){
                throw std::runtime_error("Long text is too long at tag " + get_tag_str() + ". Consider using a longer VR. Cannot continue.");
            }
            if(has_control_char_except_text(this->val)){
                throw std::invalid_argument("Forbidden control character found in LT at tag " + get_tag_str() + ". Cannot continue.");
            }
        }
        cumulative_length += emit_DICOM_tag(os, enc, *this, this->val, lenient);

    }else if( this->VR == "UT" ){ //Unlimited text.
        // DICOM PS3.5 Section 6.2: UT - Unlimited Text.
        // Character repertoire: Default, excluding control characters except TAB, LF, FF, CR, ESC.
        // Maximum length: 2^32-2 bytes. Not multi-valued (backslash allowed).
        if(!lenient){
            if(4'294'967'294ULL < this->val.length()){
                throw std::runtime_error("Unlimited text is too long at tag " + get_tag_str() + ". Cannot continue.");
            }
            if(has_control_char_except_text(this->val)){
                throw std::invalid_argument("Forbidden control character found in UT at tag " + get_tag_str() + ". Cannot continue.");
            }
        }
        cumulative_length += emit_DICOM_tag(os, enc, *this, this->val, lenient);

    }else if( this->VR == "UC" ){ //Unlimited characters.
        // DICOM PS3.5 Section 6.2: UC - Unlimited Characters.
        // Character repertoire: Default, excluding backslash (5CH) and all control characters except ESC.
        // Maximum length: 2^32-2 bytes.
        if(!lenient){
            if(4'294'967'294ULL < this->val.length()){
                throw std::runtime_error("Unlimited characters is too long at tag " + get_tag_str() + ". Cannot continue.");
            }
            if(has_control_char_except_esc(this->val)){
                throw std::invalid_argument("Forbidden control character found in UC at tag " + get_tag_str() + ". Cannot continue.");
            }
            // Note: backslash is the value multiplicity delimiter -- individual values are
            // separated by the caller, so the presence of a backslash is not itself forbidden
            // here (it is part of the encoding for multi-valued UC).
        }
        cumulative_length += emit_DICOM_tag(os, enc, *this, this->val, lenient);

    }else if( this->VR == "UR" ){ //Universal Resource Identifier or Locator (URI/URL).
        // DICOM PS3.5 Section 6.2: UR - URI/URL.
        // Character repertoire: Subset of Default required for URIs per RFC 3986 Section 2,
        //   plus trailing SPACE for padding. Leading spaces are not allowed.
        //   Not multi-valued (backslash is disallowed per RFC 3986).
        // Maximum length: 2^32-2 bytes.
        if(!lenient){
            if(4'294'967'294ULL < this->val.length()){
                throw std::runtime_error("URI is too long at tag " + get_tag_str() + ". Cannot continue.");
            }
            if(!this->val.empty() && this->val.front() == ' '){
                throw std::invalid_argument("Leading space found in UR at tag " + get_tag_str() + ". Cannot continue.");
            }
            if(this->val.find('\\') != std::string::npos){
                throw std::invalid_argument("Backslash found in UR at tag " + get_tag_str() + ". Not permitted per RFC 3986. Cannot continue.");
            }
        }
        cumulative_length += emit_DICOM_tag(os, enc, *this, this->val, lenient);


    // Name types.
    }else if( this->VR == "AE" ){ //Application entity.
        // DICOM PS3.5 Section 6.2: AE - Application Entity.
        // Character repertoire: Default, excluding backslash (5CH) and all control characters.
        // Maximum length: 16 bytes. Leading/trailing spaces are non-significant.
        // A value consisting solely of spaces shall not be used.
        if(!lenient){
            if(16ULL < this->val.length()){
                throw std::runtime_error("Application entity is too long at tag " + get_tag_and_val_str() + ". Cannot continue.");
            }
            if(this->val.find('\\') != std::string::npos){
                throw std::invalid_argument("Backslash found in AE at tag " + get_tag_and_val_str() + ". Cannot continue.");
            }
            if(has_control_char(this->val)){
                throw std::invalid_argument("Control character found in AE at tag " + get_tag_and_val_str() + ". Cannot continue.");
            }
        }
        cumulative_length += emit_DICOM_tag(os, enc, *this, this->val, lenient);

    }else if( this->VR == "PN" ){ //Person name.
        // DICOM PS3.5 Section 6.2: PN - Person Name.
        // Character repertoire: Default, excluding backslash (5CH) and all control characters except ESC.
        // Maximum length: 64 characters per component group (up to 3 groups delimited by '=').
        // Components within a group are delimited by '^' (up to 5 components).
        if(!lenient){
            // Check per-value and per-component-group limits.
            auto values = SplitStringToVector(this->val, '\\', 'd');
            for(const auto &pn_val : values){
                auto groups = SplitStringToVector(pn_val, '=', 'd');
                if(3ULL < groups.size()){
                    throw std::invalid_argument("Person name has more than 3 component groups at tag " + get_tag_str() + ". Cannot continue.");
                }
                for(const auto &group : groups){
                    if(64ULL < group.length()){
                        throw std::runtime_error("Person name component group exceeds 64 characters at tag " + get_tag_str() + ". Cannot continue.");
                    }
                }
            }
            if(has_control_char_except_esc(this->val)){
                throw std::invalid_argument("Forbidden control character found in PN at tag " + get_tag_str() + ". Cannot continue.");
            }
        }
        cumulative_length += emit_DICOM_tag(os, enc, *this, this->val, lenient);

    }else if( this->VR == "UI" ){ //Unique Identifier (UID).
        // DICOM PS3.5 Section 6.2: UI - Unique Identifier.
        // Character repertoire: "0"-"9" and "." of Default Character Repertoire.
        // Maximum length: 64 bytes per UID value. Padded with trailing NULL (0x00).
        if(!lenient){
            // Value multiplicity: each UID component separated by backslash is limited to 64 bytes.
            auto uid_values = SplitStringToVector(this->val, '\\', 'd');
            for(const auto &uid_val : uid_values){
                if(64ULL < uid_val.length()){
                    throw std::runtime_error("UID is too long at tag " + get_tag_and_val_str() + ". Cannot continue.");
                }
            }
            const auto allowed_ui = number_digits + multiplicity + ".";
            if(this->val.find_first_not_of(allowed_ui) != std::string::npos){
                throw std::invalid_argument("Invalid character " + first_invalid_char_diag(this->val, allowed_ui)
                                            + " found in UID at tag " + get_tag_and_val_str() + ". Cannot continue.");
            }

            // Ensure there are no leading insignificant zeros.
            auto tokens = SplitStringToVector(this->val,'.','d');
            for(const auto &token : tokens){
                if( (1 < token.size()) && (token.at(0) == '0') ){
                    throw std::invalid_argument("UID contains an insignificant leading zero at tag " + get_tag_and_val_str() + ". Refusing to continue.");
                }
            }
        }
        cumulative_length += emit_DICOM_tag(os, enc, *this, this->val, lenient);


    //Date and Time.
    }else if( this->VR == "DA" ){  //Date.
        // DICOM PS3.5 Section 6.2: DA - Date.
        // Character repertoire: "0"-"9". Format: YYYYMMDD.
        // Maximum length: 8 bytes fixed.
        //
        // Note: legacy ACR-NEMA format (YYYY.MM.DD) is stripped during pre-processing below.
        std::string digits_only(val);
        digits_only = PurgeCharsFromString(digits_only,":-");
        auto avec = SplitStringToVector(digits_only,'.','d');
        avec.resize(1);
        digits_only = Lineate_Vector(avec, "");

        if(!lenient){
            if(8ULL < digits_only.length()){
                throw std::runtime_error("Date is too long at tag " + get_tag_and_val_str() + ". Cannot continue.");
            }
            if(digits_only.find_first_not_of(number_digits) != std::string::npos){
                throw std::invalid_argument("Invalid character " + first_invalid_char_diag(digits_only, number_digits)
                                            + " found in date at tag " + get_tag_and_val_str() + ". Cannot continue.");
            }
        }
        cumulative_length += emit_DICOM_tag(os, enc, *this, digits_only, lenient);

    }else if( this->VR == "TM" ){  //Time.
        // DICOM PS3.5 Section 6.2: TM - Time.
        // Character repertoire: "0"-"9", ".", and SPACE (trailing padding only).
        // Format: HHMMSS.FFFFFF (2+2+2+1+6 = 13 chars, plus optional trailing space = 14).
        // Maximum length: 14 bytes.
        //
        // Note: legacy ACR-NEMA format (HH:MM:SS.frac) is stripped during pre-processing below.
        std::string digits_only(val);
        digits_only = PurgeCharsFromString(digits_only,":-");
        auto avec = SplitStringToVector(digits_only,'.','d');
        avec.resize(1);
        digits_only = Lineate_Vector(avec, "");

        if(!lenient){
            if(14ULL < digits_only.length()){
                throw std::runtime_error("Time is too long at tag " + get_tag_and_val_str() + ". Cannot continue.");
            }
            const auto allowed_tm = number_digits + ". ";
            if(digits_only.find_first_not_of(allowed_tm) != std::string::npos){
                throw std::invalid_argument("Invalid character " + first_invalid_char_diag(digits_only, allowed_tm)
                                            + " found in time at tag " + get_tag_and_val_str() + ". Cannot continue.");
            }
        }
        cumulative_length += emit_DICOM_tag(os, enc, *this, digits_only, lenient);

    }else if( this->VR == "DT" ){  //Date Time.
        // DICOM PS3.5 Section 6.2: DT - Date Time.
        // Character repertoire: "0"-"9", "+", "-", ".", and SPACE (trailing padding only).
        // Format: YYYYMMDDHHMMSS.FFFFFF&ZZXX. Maximum length: 26 bytes.
        //
        // Note: legacy format normalization is performed during pre-processing below.
        std::string digits_only(val);
        digits_only = PurgeCharsFromString(digits_only,":-");
        auto avec = SplitStringToVector(digits_only,'.','d');
        avec.resize(1);
        digits_only = Lineate_Vector(avec, "");

        if(!lenient){
            if(26ULL < digits_only.length()){
                throw std::runtime_error("Date-time is too long at tag " + get_tag_and_val_str() + ". Cannot continue.");
            }
            const auto allowed_dt = number_digits + "+-. ";
            if(digits_only.find_first_not_of(allowed_dt) != std::string::npos){
                throw std::invalid_argument("Invalid character " + first_invalid_char_diag(digits_only, allowed_dt)
                                            + " found in date-time at tag " + get_tag_and_val_str() + ". Cannot continue.");
            }
        }
        cumulative_length += emit_DICOM_tag(os, enc, *this, digits_only, lenient);

    }else if( this->VR == "AS" ){ //Age string.
        // DICOM PS3.5 Section 6.2: AS - Age String.
        // Character repertoire: "0"-"9", "D", "W", "M", "Y".
        // Length: 4 bytes fixed. Format: nnnD, nnnW, nnnM, or nnnY.
        if(!lenient){
            if(4ULL < this->val.length()){
                throw std::runtime_error("Age string is too long at tag " + get_tag_and_val_str() + ". Cannot continue.");
            }
            const auto allowed_as = number_digits + "DWMY";
            if(this->val.find_first_not_of(allowed_as) != std::string::npos){
                throw std::invalid_argument("Invalid character " + first_invalid_char_diag(this->val, allowed_as)
                                            + " found in age string at tag " + get_tag_and_val_str() + ". Cannot continue.");
            }
            if(!this->val.empty() && this->val.find_first_of("DWMY") == std::string::npos){
                throw std::invalid_argument("Age string is missing one of 'DWMY' characters at tag " + get_tag_and_val_str() + ". Cannot continue.");
            }
        }
        cumulative_length += emit_DICOM_tag(os, enc, *this, this->val, lenient);


    //Binary types.
    }else if( this->VR == "OB" ){ //'Other' binary string: a string of bytes that doesn't fit any other VR.
        // DICOM PS3.5 Section 6.2: OB - Other Byte.
        // An octet-stream. Insensitive to byte ordering.
        // Maximum length: 2^32-2 bytes. Padded with trailing NULL (0x00) to even length.
        if(!lenient){
            if( 4'294'967'294ULL < this->val.length() ){ // 2^32 - 2
                throw std::invalid_argument("Other byte string is too long at tag " + get_tag_str() + ". Cannot continue.");
            }
        }
        cumulative_length += emit_DICOM_tag(os, enc, *this, this->val, lenient);

    }else if( this->VR == "OW" ){ //'Other word string': a string of 16bit values.
        // DICOM PS3.5 Section 6.2: OW - Other Word.
        // A stream of 16-bit words. Requires byte swapping per byte ordering.
        // Maximum length: 2^32-2 bytes. Must be a multiple of 2 bytes.
        if(!lenient){
            if( 4'294'967'294ULL < this->val.length() ){ // 2^32 - 2
                throw std::invalid_argument("Other word string is too long at tag " + get_tag_str() + ". Cannot continue.");
            }
            if( (this->val.length() % 2) != 0 ){
                throw std::invalid_argument("Other word string does not seem to contain 16-bit words at tag " + get_tag_str() + ". Cannot continue.");
            }
        }

        cumulative_length += emit_DICOM_tag(os, enc, *this, this->val, lenient);

    }else if( this->VR == "OL" ){ //'Other long': a stream of 32-bit words.
        // DICOM PS3.5 Section 6.2: OL - Other Long.
        // A stream of 32-bit words. Requires byte swapping per byte ordering.
        // Maximum length: 2^32-4 bytes. Must be a multiple of 4 bytes.
        if(!lenient){
            if( 4'294'967'292ULL < this->val.length() ){ // 2^32 - 4
                throw std::invalid_argument("Other long is too long at tag " + get_tag_str() + ". Cannot continue.");
            }
            if( (this->val.length() % 4) != 0 ){
                throw std::invalid_argument("Other long does not contain an integral number of 32-bit words at tag " + get_tag_str() + ". Cannot continue.");
            }
        }
        cumulative_length += emit_DICOM_tag(os, enc, *this, this->val, lenient);

    }else if( this->VR == "OV" ){ //'Other 64-bit very long': a stream of 64-bit words.
        // DICOM PS3.5 Section 6.2: OV - Other 64-bit Very Long.
        // A stream of 64-bit words. Requires byte swapping per byte ordering.
        // Maximum length: 2^32-8 bytes. Must be a multiple of 8 bytes.
        if(!lenient){
            if( 4'294'967'288ULL < this->val.length() ){ // 2^32 - 8
                throw std::invalid_argument("Other 64-bit very long is too long at tag " + get_tag_str() + ". Cannot continue.");
            }
            if( (this->val.length() % 8) != 0 ){
                throw std::invalid_argument("Other 64-bit very long does not contain an integral number of 64-bit words at tag " + get_tag_str() + ". Cannot continue.");
            }
        }
        cumulative_length += emit_DICOM_tag(os, enc, *this, this->val, lenient);


    //Numeric types that are written as a string of characters.
    }else if( this->VR == "IS" ){ //Integer string.
        // DICOM PS3.5 Section 6.2: IS - Integer String.
        // Character repertoire: "0"-"9", "+", "-", and SPACE (leading/trailing padding only).
        // Maximum length: 12 bytes per value. Range: -2^31 to 2^31-1.
        if(!lenient){
            // Overall string length limit based on encoding (IS uses short VR in ELE: 16-bit length field).
            if( ( (enc == Encoding::ELE) && (65'534ULL < this->val.length()) )
            ||  ( (enc == Encoding::ILE) && (4'294'967'295ULL < this->val.length()) ) ){
                throw std::invalid_argument("Integer string is too long at tag " + get_tag_and_val_str() + ". Cannot continue.");
            }

            auto tokens = SplitStringToVector(this->val,'\\','d');
            for(const auto &token : tokens){
                // Maximum length per value: 12 bytes.
                if(12ULL < token.length()){
                    throw std::invalid_argument("Integer string element is too long at tag " + get_tag_and_val_str() + ". Cannot continue.");
                }

                // Ensure that, if an element is present it parses as a number.
                try{
                    if(!token.empty()) [[maybe_unused]] auto r = std::stoll(token);
                }catch(const std::exception &){
                    throw std::runtime_error("Unable to convert '"_s + token + "' to IS at tag " + get_tag_and_val_str() + ". Cannot continue.");
                }
            }

            const auto allowed_is = number_digits + multiplicity + "+-" + " ";
            if(this->val.find_first_not_of(allowed_is) != std::string::npos){
                throw std::invalid_argument("Invalid character " + first_invalid_char_diag(this->val, allowed_is)
                                            + " found in integer string at tag " + get_tag_and_val_str() + ". Cannot continue.");
            }
        }
        cumulative_length += emit_DICOM_tag(os, enc, *this, this->val, lenient);

    }else if( this->VR == "DS" ){ //Decimal string.
        // DICOM PS3.5 Section 6.2: DS - Decimal String.
        // Character repertoire: "0"-"9", "+", "-", "E", "e", ".", and SPACE.
        // Maximum length: 16 bytes per value.
        if(!lenient){
            // Overall string length limit based on encoding (DS uses short VR in ELE: 16-bit length field).
            if( ( (enc == Encoding::ELE) && (65'534ULL < this->val.length()) )
            ||  ( (enc == Encoding::ILE) && (4'294'967'295ULL < this->val.length()) ) ){
                throw std::invalid_argument("Decimal string is too long at tag " + get_tag_and_val_str() + ". Cannot continue.");
            }

            auto tokens = SplitStringToVector(this->val,'\\','d');
            for(const auto &token : tokens){
                // Maximum length per decimal number: 16 bytes.
                if(16ULL < token.length()){
                    throw std::invalid_argument("Decimal string element is too long at tag " + get_tag_and_val_str() + ". Cannot continue.");
                }

                // Ensure that if an element is present it parses as a number.
                try{
                    if(!token.empty()) [[maybe_unused]] auto r = std::stod(token);
                }catch(const std::exception &){
                    throw std::runtime_error("Unable to convert '"_s + token + "' to DS at tag " + get_tag_and_val_str() + ". Cannot continue.");
                }
            }

            const auto allowed_ds = number_digits + multiplicity + "+-eE." + " ";
            if(this->val.find_first_not_of(allowed_ds) != std::string::npos){
                throw std::invalid_argument("Invalid character " + first_invalid_char_diag(this->val, allowed_ds)
                                            + " found in decimal string at tag " + get_tag_and_val_str() + ". Cannot continue.");
            }
        }
        cumulative_length += emit_DICOM_tag(os, enc, *this, this->val, lenient);


    //Numeric types that must be binary encoded.
    // Note: IEEE 754:1985 compliance for float and double is verified via static_assert at the top of this file.
    }else if( this->VR == "FL" ){ //Floating-point (IEEE 754:1985 32-bit).
        // DICOM PS3.5 Section 6.2: FL - Floating Point Single.
        // IEEE 754 binary32. Length: 4 bytes fixed.
        YLOGDEBUG("emit_DICOM: encoding FL at tag " << get_tag_and_val_str() << " val='" << this->val << "'");
        if(lenient){
            try{
                std::ostringstream ss(std::ios_base::ate | std::ios_base::binary);
                const float val_f = std::stof(this->val);
                write_to_stream(ss, val_f, 4, enc);
                cumulative_length += emit_DICOM_tag(os, enc, *this, ss.str(), lenient);
            }catch(const std::exception &e){
                YLOGDEBUG("emit_DICOM: lenient fallback to raw bytes for FL at " << get_tag_and_val_str() << ": " << e.what());
                cumulative_length += emit_DICOM_tag(os, enc, *this, this->val, lenient);
            }
        }else{
            std::ostringstream ss(std::ios_base::ate | std::ios_base::binary);
            const float val_f = std::stof(this->val);
            write_to_stream(ss, val_f, 4, enc);
            cumulative_length += emit_DICOM_tag(os, enc, *this, ss.str(), lenient);
        }

    }else if( this->VR == "FD" ){ //Floating-point double (IEEE 754:1985 64-bit).
        // DICOM PS3.5 Section 6.2: FD - Floating Point Double.
        // IEEE 754 binary64. Length: 8 bytes fixed.
        YLOGDEBUG("emit_DICOM: encoding FD at tag " << get_tag_and_val_str() << " val='" << this->val << "'");
        if(lenient){
            try{
                std::ostringstream ss(std::ios_base::ate | std::ios_base::binary);
                const double val_d = std::stod(this->val);
                write_to_stream(ss, val_d, 8, enc);
                cumulative_length += emit_DICOM_tag(os, enc, *this, ss.str(), lenient);
            }catch(const std::exception &e){
                YLOGDEBUG("emit_DICOM: lenient fallback to raw bytes for FD at " << get_tag_and_val_str() << ": " << e.what());
                cumulative_length += emit_DICOM_tag(os, enc, *this, this->val, lenient);
            }
        }else{
            std::ostringstream ss(std::ios_base::ate | std::ios_base::binary);
            const double val_d = std::stod(this->val);
            write_to_stream(ss, val_d, 8, enc);
            cumulative_length += emit_DICOM_tag(os, enc, *this, ss.str(), lenient);
        }

    }else if( this->VR == "OF" ){ //"Other" floating-point (IEEE 754:1985 32-bit).
        // DICOM PS3.5 Section 6.2: OF - Other Float.
        // A stream of IEEE 754 binary32 values. Maximum length: 2^32-4 bytes.
        //The value payload may contain multiple floats separated by some partitioning character.
        // For example, '1.23\2.34\0.00\25E25\-1.23'.
        YLOGDEBUG("emit_DICOM: encoding OF at tag " << get_tag_and_val_str());
        if(lenient){
            // Lenient: emit raw bytes directly, skipping token parsing.
            cumulative_length += emit_DICOM_tag(os, enc, *this, this->val, lenient);
        }else{
            std::ostringstream ss(std::ios_base::ate | std::ios_base::binary);
            auto tokens = SplitStringToVector(this->val, '\\', 'd');
            for(auto &token_val : tokens){
                const float val_f = std::stof(token_val);
                write_to_stream(ss, val_f, 4, enc);
            }
            cumulative_length += emit_DICOM_tag(os, enc, *this, ss.str(), lenient);
        }

    }else if( this->VR == "OD" ){ //"Other" floating-point double (IEEE 754:1985 64-bit).
        // DICOM PS3.5 Section 6.2: OD - Other Double.
        // A stream of IEEE 754 binary64 values. Maximum length: 2^32-8 bytes.
        //The value payload may contain multiple doubles separated by some partitioning character.
        // For example, '1.23\2.34\0.00\25E25\-1.23'.
        YLOGDEBUG("emit_DICOM: encoding OD at tag " << get_tag_and_val_str());
        if(lenient){
            // Lenient: emit raw bytes directly, skipping token parsing.
            cumulative_length += emit_DICOM_tag(os, enc, *this, this->val, lenient);
        }else{
            std::ostringstream ss(std::ios_base::ate | std::ios_base::binary);
            auto tokens = SplitStringToVector(this->val, '\\', 'd');
            for(auto &token_val : tokens){
                const double val_d = std::stod(token_val);
                write_to_stream(ss, val_d, 8, enc);
            }
            cumulative_length += emit_DICOM_tag(os, enc, *this, ss.str(), lenient);
        }

    }else if( this->VR == "SS" ){ //Signed short (16bit).
        // DICOM PS3.5 Section 6.2: SS - Signed Short.
        // Signed binary integer 16 bits. Length: 2 bytes fixed. Range: -2^15 to 2^15-1.
        if(lenient){
            try{
                std::ostringstream ss(std::ios_base::ate | std::ios_base::binary);
                const int16_t val_i = std::stoi(this->val);
                write_to_stream(ss, val_i, 2, enc);
                cumulative_length += emit_DICOM_tag(os, enc, *this, ss.str(), lenient);
            }catch(const std::exception &e){
                YLOGDEBUG("emit_DICOM: lenient fallback to raw bytes for SS at " << get_tag_and_val_str() << ": " << e.what());
                cumulative_length += emit_DICOM_tag(os, enc, *this, this->val, lenient);
            }
        }else{
            std::ostringstream ss(std::ios_base::ate | std::ios_base::binary);
            const int16_t val_i = std::stoi(this->val);
            write_to_stream(ss, val_i, 2, enc);
            cumulative_length += emit_DICOM_tag(os, enc, *this, ss.str(), lenient);
        }

    }else if( this->VR == "US" ){ //Unsigned short (16bit).
        // DICOM PS3.5 Section 6.2: US - Unsigned Short.
        // Unsigned binary integer 16 bits. Length: 2 bytes fixed. Range: 0 to 2^16-1.
        if(lenient){
            try{
                std::ostringstream ss(std::ios_base::ate | std::ios_base::binary);
                const auto val_u = static_cast<uint16_t>(std::stoul(this->val));
                write_to_stream(ss, val_u, 2, enc);
                cumulative_length += emit_DICOM_tag(os, enc, *this, ss.str(), lenient);
            }catch(const std::exception &e){
                YLOGDEBUG("emit_DICOM: lenient fallback to raw bytes for US at " << get_tag_and_val_str() << ": " << e.what());
                cumulative_length += emit_DICOM_tag(os, enc, *this, this->val, lenient);
            }
        }else{
            std::ostringstream ss(std::ios_base::ate | std::ios_base::binary);
            const auto val_u = static_cast<uint16_t>(std::stoul(this->val));
            write_to_stream(ss, val_u, 2, enc);
            cumulative_length += emit_DICOM_tag(os, enc, *this, ss.str(), lenient);
        }

    }else if( this->VR == "SL" ){ //Signed long (32bit).
        // DICOM PS3.5 Section 6.2: SL - Signed Long.
        // Signed binary integer 32 bits. Length: 4 bytes fixed. Range: -2^31 to 2^31-1.
        if(lenient){
            try{
                std::ostringstream ss(std::ios_base::ate | std::ios_base::binary);
                const int32_t val_l = std::stol(this->val);
                write_to_stream(ss, val_l, 4, enc);
                cumulative_length += emit_DICOM_tag(os, enc, *this, ss.str(), lenient);
            }catch(const std::exception &e){
                YLOGDEBUG("emit_DICOM: lenient fallback to raw bytes for SL at " << get_tag_and_val_str() << ": " << e.what());
                cumulative_length += emit_DICOM_tag(os, enc, *this, this->val, lenient);
            }
        }else{
            std::ostringstream ss(std::ios_base::ate | std::ios_base::binary);
            const int32_t val_l = std::stol(this->val);
            write_to_stream(ss, val_l, 4, enc);
            cumulative_length += emit_DICOM_tag(os, enc, *this, ss.str(), lenient);
        }

    }else if( this->VR == "UL" ){ //Unsigned long (32bit).
        // DICOM PS3.5 Section 6.2: UL - Unsigned Long.
        // Unsigned binary integer 32 bits. Length: 4 bytes fixed. Range: 0 to 2^32-1.
        if(lenient){
            try{
                std::ostringstream ss(std::ios_base::ate | std::ios_base::binary);
                const uint32_t val_ul = std::stoul(this->val);
                write_to_stream(ss, val_ul, 4, enc);
                cumulative_length += emit_DICOM_tag(os, enc, *this, ss.str(), lenient);
            }catch(const std::exception &e){
                YLOGDEBUG("emit_DICOM: lenient fallback to raw bytes for UL at " << get_tag_and_val_str() << ": " << e.what());
                cumulative_length += emit_DICOM_tag(os, enc, *this, this->val, lenient);
            }
        }else{
            std::ostringstream ss(std::ios_base::ate | std::ios_base::binary);
            const uint32_t val_ul = std::stoul(this->val);
            write_to_stream(ss, val_ul, 4, enc);
            cumulative_length += emit_DICOM_tag(os, enc, *this, ss.str(), lenient);
        }

    }else if( this->VR == "AT" ){ //Attribute tag (2x unsigned shorts representing a DICOM data tag).
        // DICOM PS3.5 Section 6.2: AT - Attribute Tag.
        // Ordered pair of 16-bit unsigned integers. Length: 4 bytes fixed.
        if(lenient){
            // Lenient: attempt conversion, fall back to raw bytes.
            try{
                std::ostringstream ss(std::ios_base::ate | std::ios_base::binary);
                auto tokens = SplitStringToVector(this->val, '\\', 'd');
                if(tokens.size() != 2ULL){
                    throw std::runtime_error("AT token count mismatch");
                }
                for(auto &token_val : tokens){
                    const auto val_u = static_cast<uint16_t>(std::stoul(token_val));
                    write_to_stream(ss, val_u, 2, enc);
                }
                cumulative_length += emit_DICOM_tag(os, enc, *this, ss.str(), lenient);
            }catch(const std::exception &e){
                YLOGDEBUG("emit_DICOM: lenient fallback to raw bytes for AT at " << get_tag_and_val_str() << ": " << e.what());
                cumulative_length += emit_DICOM_tag(os, enc, *this, this->val, lenient);
            }
        }else{
            // Assuming the value payload contains exactly two unsigned integers, e.g., '123\234'.
            std::ostringstream ss(std::ios_base::ate | std::ios_base::binary);
            auto tokens = SplitStringToVector(this->val, '\\', 'd');
            if(tokens.size() != 2ULL){
                throw std::runtime_error("Invalid number of integers for AT type tag at " + get_tag_and_val_str() + "; exactly 2 are needed.");
            }
            for(auto &token_val : tokens){
                const auto val_u = static_cast<uint16_t>(std::stoul(token_val));
                write_to_stream(ss, val_u, 2, enc);
            }
            cumulative_length += emit_DICOM_tag(os, enc, *this, ss.str(), lenient);
        }

    }else if( this->VR == "SV" ){ //Signed 64-bit very long.
        // DICOM PS3.5 Section 6.2: SV - Signed 64-bit Very Long.
        // Signed binary integer 64 bits. Length: 8 bytes fixed. Range: -2^63 to 2^63-1.
        if(lenient){
            try{
                std::ostringstream ss(std::ios_base::ate | std::ios_base::binary);
                const int64_t val_sv = std::stoll(this->val);
                write_to_stream(ss, val_sv, 8, enc);
                cumulative_length += emit_DICOM_tag(os, enc, *this, ss.str(), lenient);
            }catch(const std::exception &e){
                YLOGDEBUG("emit_DICOM: lenient fallback to raw bytes for SV at " << get_tag_and_val_str() << ": " << e.what());
                cumulative_length += emit_DICOM_tag(os, enc, *this, this->val, lenient);
            }
        }else{
            std::ostringstream ss(std::ios_base::ate | std::ios_base::binary);
            const int64_t val_sv = std::stoll(this->val);
            write_to_stream(ss, val_sv, 8, enc);
            cumulative_length += emit_DICOM_tag(os, enc, *this, ss.str(), lenient);
        }

    }else if( this->VR == "UV" ){ //Unsigned 64-bit very long.
        // DICOM PS3.5 Section 6.2: UV - Unsigned 64-bit Very Long.
        // Unsigned binary integer 64 bits. Length: 8 bytes fixed. Range: 0 to 2^64-1.
        if(lenient){
            try{
                std::ostringstream ss(std::ios_base::ate | std::ios_base::binary);
                const uint64_t val_uv = std::stoull(this->val);
                write_to_stream(ss, val_uv, 8, enc);
                cumulative_length += emit_DICOM_tag(os, enc, *this, ss.str(), lenient);
            }catch(const std::exception &e){
                YLOGDEBUG("emit_DICOM: lenient fallback to raw bytes for UV at " << get_tag_and_val_str() << ": " << e.what());
                cumulative_length += emit_DICOM_tag(os, enc, *this, this->val, lenient);
            }
        }else{
            std::ostringstream ss(std::ios_base::ate | std::ios_base::binary);
            const uint64_t val_uv = std::stoull(this->val);
            write_to_stream(ss, val_uv, 8, enc);
            cumulative_length += emit_DICOM_tag(os, enc, *this, ss.str(), lenient);
        }


    //Other types.
    }else if( this->VR == "UN" ){ //Unknown. Often needed for handling private DICOM tags.
        // DICOM PS3.5 Section 6.2: UN - Unknown.
        // An octet-stream where the encoding of the contents is unknown.
        cumulative_length += emit_DICOM_tag(os, enc, *this, this->val, lenient);

    }else if( this->VR == "SQ" ){ //Sequence.
        // DICOM PS3.5 Section 6.2, Section 7.5: SQ - Sequence of Items.
        // Value is a Sequence of zero or more Items. Not data-bearing itself.
        // Verify the node does not have any data associated with it. If it does, it probably indicates a logic
        // error since only children nodes should contain data.
        if(!this->val.empty()){
            throw std::logic_error("Nodes with 'SQ' VR can not have any data associated with them at tag " + get_tag_and_val_str() + ". (Is it intentional?)");
        }

        // Recursive calls happen in the following routine.
        cumulative_length += emit_DICOM_tag(os, enc, *this, this->val, lenient);

    }else{
        throw std::runtime_error("Unknown VR type '" + this->VR + "' at tag " + get_tag_and_val_str() + ". Cannot write to tag.");
    }

    return cumulative_length;
}


bool validate_VR_conformance(const std::string &VR,
                             const std::string &val,
                             DCMA_DICOM::Encoding enc ){
    // In many cases validation can only be done when actually writing the DICOM.
    // To avoid duplicating the validation checks during emission, we simulate writing a DICOM file with the given
    // content. This results in a slow runtime, but avoids tricky code duplication.
    //
    // Note: lenient mode is intentionally *not* used here -- the purpose of this function is to check strict
    // conformance.
    YLOGDEBUG("validate_VR_conformance: VR='" << VR << "' val_length=" << val.size());
    Node root_node;
    root_node.emplace_child_node({{0x9999, 0x9999}, VR, val});

    bool valid = false;
    try{
        std::stringstream ss;
        root_node.emit_DICOM(ss, enc);
        if(ss) valid = true;
    }catch(const std::exception &){};
    return valid;
}


///////////////////////////////////////////////////////////////////////////////
// DICOM Reading helpers.
///////////////////////////////////////////////////////////////////////////////

namespace {

// Verify the machine is little-endian.
void verify_little_endian(){
    uint16_t test { 0x01 };
    auto *first_byte = reinterpret_cast<unsigned char *>(&test);
    if(static_cast<uint16_t>(*first_byte) != test){
        throw std::runtime_error("This computer is not little-endian. This is not supported.");
    }
}

uint16_t read_uint16_le(std::istream &is){
    uint16_t val = 0;
    is.read(reinterpret_cast<char *>(&val), 2);
    if(!is) throw std::runtime_error("Unexpected end of DICOM stream while reading uint16.");
    return val;
}

uint32_t read_uint32_le(std::istream &is){
    uint32_t val = 0;
    is.read(reinterpret_cast<char *>(&val), 4);
    if(!is) throw std::runtime_error("Unexpected end of DICOM stream while reading uint32.");
    return val;
}

std::string read_bytes(std::istream &is, uint32_t count){
    std::string buf(count, '\0');
    if(count > 0){
        is.read(buf.data(), count);
        if(!is) throw std::runtime_error("Unexpected end of DICOM stream while reading "_s
                                         + std::to_string(count) + " bytes.");
    }
    return buf;
}


// DICOM PS3.5 Section 7.1.2: Returns true if the VR uses a 4-byte length field with
// 2 reserved bytes in explicit encoding (i.e., VRs not in Table 7.1-2).
bool vr_has_extended_length(const std::string &vr){
    return (vr == "OB") || (vr == "OD") || (vr == "OF") || (vr == "OL")
        || (vr == "OV") || (vr == "OW") || (vr == "SQ") || (vr == "SV")
        || (vr == "UC") || (vr == "UN") || (vr == "UR") || (vr == "UT")
        || (vr == "UV");
}

// Returns true if the VR represents a text-based encoding (value is human-readable text).
bool vr_is_text(const std::string &vr){
    return (vr == "AE") || (vr == "AS") || (vr == "CS") || (vr == "DA")
        || (vr == "DS") || (vr == "DT") || (vr == "IS") || (vr == "LO")
        || (vr == "LT") || (vr == "PN") || (vr == "SH") || (vr == "ST")
        || (vr == "TM") || (vr == "UC") || (vr == "UI") || (vr == "UR")
        || (vr == "UT");
}

// Strip trailing padding from a text value. UI uses null padding; all others use space.
std::string strip_text_padding(const std::string &val, const std::string &vr){
    std::string out = val;
    if(vr == "UI"){
        while(!out.empty() && out.back() == '\0') out.pop_back();
    }else{
        while(!out.empty() && (out.back() == ' ' || out.back() == '\0')) out.pop_back();
    }
    return out;
}

// Decode a binary numeric value to a human-readable string, matching the format expected by the emitter.
// For simple binary numeric VRs (US, SS, UL, SL, FL, FD, AT, SV, UV), this produces a string representation.
// For multi-valued binary VRs (OW, OF, OD, OL, OV, OB, UN), the raw bytes are left unchanged.
//
// Note: memcpy is used to interpret raw bytes in native byte order. This is correct because:
// (1) the machine is verified to be little-endian (see verify_little_endian()), and
// (2) big-endian transfer syntax is explicitly rejected by the reader.
std::string decode_binary_value(const std::string &raw, const std::string &vr){
    if(vr == "US"){
        if(raw.size() < 2) return raw;
        std::string result;
        for(size_t i = 0; i + 1 < raw.size(); i += 2){
            uint16_t v = 0;
            std::memcpy(&v, raw.data() + i, 2);
            if(!result.empty()) result += "\\";
            result += std::to_string(v);
        }
        return result;
    }else if(vr == "SS"){
        if(raw.size() < 2) return raw;
        std::string result;
        for(size_t i = 0; i + 1 < raw.size(); i += 2){
            int16_t v = 0;
            std::memcpy(&v, raw.data() + i, 2);
            if(!result.empty()) result += "\\";
            result += std::to_string(v);
        }
        return result;
    }else if(vr == "UL"){
        if(raw.size() < 4) return raw;
        std::string result;
        for(size_t i = 0; i + 3 < raw.size(); i += 4){
            uint32_t v = 0;
            std::memcpy(&v, raw.data() + i, 4);
            if(!result.empty()) result += "\\";
            result += std::to_string(v);
        }
        return result;
    }else if(vr == "SL"){
        if(raw.size() < 4) return raw;
        std::string result;
        for(size_t i = 0; i + 3 < raw.size(); i += 4){
            int32_t v = 0;
            std::memcpy(&v, raw.data() + i, 4);
            if(!result.empty()) result += "\\";
            result += std::to_string(v);
        }
        return result;
    }else if(vr == "FL"){
        if(raw.size() < 4) return raw;
        std::ostringstream ss;
        ss << std::setprecision(std::numeric_limits<float>::max_digits10);
        bool first = true;
        for(size_t i = 0; i + 3 < raw.size(); i += 4){
            float v = 0.0f;
            std::memcpy(&v, raw.data() + i, 4);
            if(!first) ss << "\\";
            ss << v;
            first = false;
        }
        return ss.str();
    }else if(vr == "FD"){
        if(raw.size() < 8) return raw;
        std::ostringstream ss;
        ss << std::setprecision(std::numeric_limits<double>::max_digits10);
        bool first = true;
        for(size_t i = 0; i + 7 < raw.size(); i += 8){
            double v = 0.0;
            std::memcpy(&v, raw.data() + i, 8);
            if(!first) ss << "\\";
            ss << v;
            first = false;
        }
        return ss.str();
    }else if(vr == "AT"){
        if(raw.size() < 4) return raw;
        std::string result;
        for(size_t i = 0; i + 3 < raw.size(); i += 4){
            uint16_t g = 0;
            uint16_t e = 0;
            std::memcpy(&g, raw.data() + i, 2);
            std::memcpy(&e, raw.data() + i + 2, 2);
            if(!result.empty()) result += "\\";
            result += std::to_string(g);
            result += "\\";
            result += std::to_string(e);
        }
        return result;
    }else if(vr == "SV"){
        if(raw.size() < 8) return raw;
        std::string result;
        for(size_t i = 0; i + 7 < raw.size(); i += 8){
            int64_t v = 0;
            std::memcpy(&v, raw.data() + i, 8);
            if(!result.empty()) result += "\\";
            result += std::to_string(v);
        }
        return result;
    }else if(vr == "UV"){
        if(raw.size() < 8) return raw;
        std::string result;
        for(size_t i = 0; i + 7 < raw.size(); i += 8){
            uint64_t v = 0;
            std::memcpy(&v, raw.data() + i, 8);
            if(!result.empty()) result += "\\";
            result += std::to_string(v);
        }
        return result;
    }

    // OB, OW, OF, OD, OL, OV, UN, and any other binary VRs: leave raw.
    return raw;
}


// Forward declaration.
DCMA_DICOM::Node read_data_element(std::istream &is,
                                   DCMA_DICOM::Encoding enc,
                                   const std::vector<const DCMA_DICOM::DICOMDictionary*> &dicts,
                                   DCMA_DICOM::DICOMDictionary *mutable_dict);

void read_sequence_items_defined(std::istream &is,
                                 DCMA_DICOM::Node &seq_node,
                                 DCMA_DICOM::Encoding enc,
                                 const std::vector<const DCMA_DICOM::DICOMDictionary*> &dicts,
                                 DCMA_DICOM::DICOMDictionary *mutable_dict,
                                 uint32_t seq_length);

void read_sequence_items_undefined(std::istream &is,
                                   DCMA_DICOM::Node &seq_node,
                                   DCMA_DICOM::Encoding enc,
                                   const std::vector<const DCMA_DICOM::DICOMDictionary*> &dicts,
                                   DCMA_DICOM::DICOMDictionary *mutable_dict);


// Read the contents of a single DICOM sequence item (between item tag and item end).
DCMA_DICOM::Node read_item_contents(std::istream &is,
                                    DCMA_DICOM::Encoding enc,
                                    const std::vector<const DCMA_DICOM::DICOMDictionary*> &dicts,
                                    DCMA_DICOM::DICOMDictionary *mutable_dict,
                                    uint32_t item_length,
                                    uint32_t item_number){
    DCMA_DICOM::Node item_node;
    item_node.key.group = 0xFFFE;
    item_node.key.tag = 0xE000;
    item_node.key.element = item_number;
    item_node.VR = "MULTI"; // Item is a container.

    if(item_length == 0xFFFFFFFF){
        // Undefined length item: read until item delimiter (FFFE,E00D).
        while(is.good()){
            auto pos = is.tellg();
            uint16_t g = read_uint16_le(is);
            uint16_t e = read_uint16_le(is);

            if(g == 0xFFFE && e == 0xE00D){
                // Item delimiter. Length field should be zero per DICOM standard.
                uint32_t delim_len = read_uint32_le(is);
                if(delim_len != 0){
                    YLOGWARN("Item delimiter length is non-zero (" << delim_len << "), expected 0");
                }
                break;
            }
            // Seek back and read as data element.
            is.seekg(pos);
            auto child = read_data_element(is, enc, dicts, mutable_dict);
            item_node.children.push_back(std::move(child));
        }
    }else{
        // Defined length item.
        auto item_start = is.tellg();
        while(is.good()){
            auto current = is.tellg();
            auto bytes_read = static_cast<uint32_t>(current - item_start);
            if(bytes_read >= item_length) break;

            auto child = read_data_element(is, enc, dicts, mutable_dict);
            item_node.children.push_back(std::move(child));
        }
    }
    return item_node;
}


void read_sequence_items_defined(std::istream &is,
                                 DCMA_DICOM::Node &seq_node,
                                 DCMA_DICOM::Encoding enc,
                                 const std::vector<const DCMA_DICOM::DICOMDictionary*> &dicts,
                                 DCMA_DICOM::DICOMDictionary *mutable_dict,
                                 uint32_t seq_length){
    auto seq_start = is.tellg();
    uint32_t item_number = 0;

    while(is.good()){
        auto current = is.tellg();
        auto bytes_consumed = static_cast<uint32_t>(current - seq_start);
        if(bytes_consumed >= seq_length) break;

        uint16_t g = read_uint16_le(is);
        uint16_t e = read_uint16_le(is);
        uint32_t item_length = read_uint32_le(is);

        if(g == 0xFFFE && e == 0xE000){
            auto item = read_item_contents(is, enc, dicts, mutable_dict, item_length, item_number);
            seq_node.children.push_back(std::move(item));
            ++item_number;
        }else if(g == 0xFFFE && e == 0xE0DD){
            break; // Sequence delimiter (shouldn't happen with defined length, but handle gracefully).
        }else{
            throw std::runtime_error("Expected item tag (FFFE,E000) in sequence, got ("
                                     + std::to_string(g) + "," + std::to_string(e) + ").");
        }
    }
}


void read_sequence_items_undefined(std::istream &is,
                                   DCMA_DICOM::Node &seq_node,
                                   DCMA_DICOM::Encoding enc,
                                   const std::vector<const DCMA_DICOM::DICOMDictionary*> &dicts,
                                   DCMA_DICOM::DICOMDictionary *mutable_dict){
    uint32_t item_number = 0;

    while(is.good()){
        uint16_t g = read_uint16_le(is);
        uint16_t e = read_uint16_le(is);
        uint32_t item_length = read_uint32_le(is);

        if(g == 0xFFFE && e == 0xE000){
            auto item = read_item_contents(is, enc, dicts, mutable_dict, item_length, item_number);
            seq_node.children.push_back(std::move(item));
            ++item_number;
        }else if(g == 0xFFFE && e == 0xE0DD){
            break; // Sequence delimiter.
        }else{
            throw std::runtime_error("Expected item tag (FFFE,E000) or sequence delimiter in sequence, got ("
                                     + std::to_string(g) + "," + std::to_string(e) + ").");
        }
    }
}


// Read encapsulated pixel data (undefined length OB/OW pixel data with fragments).
void read_encapsulated_data(std::istream &is, DCMA_DICOM::Node &node){
    // Encapsulated data is a sequence of items. The first item is the basic offset table.
    // Subsequent items are data fragments. We concatenate all data fragments into the node value.
    std::string accumulated;

    while(is.good()){
        uint16_t g = read_uint16_le(is);
        uint16_t e = read_uint16_le(is);
        uint32_t frag_length = read_uint32_le(is);

        if(g == 0xFFFE && e == 0xE000){
            // Data fragment (or offset table for the first item).
            if(frag_length > 0){
                std::string frag = read_bytes(is, frag_length);
                accumulated += frag;
            }
        }else if(g == 0xFFFE && e == 0xE0DD){
            break; // Sequence delimiter.
        }else{
            throw std::runtime_error("Unexpected tag in encapsulated pixel data.");
        }
    }

    node.val = std::move(accumulated);
}


DCMA_DICOM::Node read_data_element(std::istream &is,
                                   DCMA_DICOM::Encoding enc,
                                   const std::vector<const DCMA_DICOM::DICOMDictionary*> &dicts,
                                   DCMA_DICOM::DICOMDictionary *mutable_dict){
    DCMA_DICOM::Node node;

    node.key.group = read_uint16_le(is);
    node.key.tag   = read_uint16_le(is);

    std::string vr;
    uint32_t length = 0;

    if(enc == DCMA_DICOM::Encoding::ELE){
        // Explicit VR: read VR from stream.
        char vr_buf[2];
        is.read(vr_buf, 2);
        if(!is) throw std::runtime_error("Unexpected end of DICOM stream while reading VR.");
        vr = std::string(vr_buf, 2);

        if(vr_has_extended_length(vr)){
            // 2 bytes reserved (DICOM standard specifies 0x0000, but not validated for
            // compatibility with non-conformant files) + 4 byte length.
            [[maybe_unused]] uint16_t reserved = read_uint16_le(is);
            length = read_uint32_le(is);
        }else{
            // 2 byte length.
            length = static_cast<uint32_t>(read_uint16_le(is));
        }

        // Update the mutable dictionary if provided.
        if(mutable_dict != nullptr){
            DCMA_DICOM::dict_key_t dkey = {node.key.group, node.key.tag};
            // Look up the expected VR from existing dictionaries.
            std::string expected_vr = DCMA_DICOM::lookup_VR(node.key.group, node.key.tag, dicts);
            if(expected_vr.empty() || expected_vr != vr){
                (*mutable_dict)[dkey] = {vr, ""};
            }
        }

    }else if(enc == DCMA_DICOM::Encoding::ILE){
        // Implicit VR: look up VR from dictionaries.
        length = read_uint32_le(is);
        vr = DCMA_DICOM::lookup_VR(node.key.group, node.key.tag, dicts);
        if(vr.empty()) vr = "UN"; // Unknown tags default to UN.
    }else{
        throw std::runtime_error("Unsupported encoding for DICOM reading.");
    }

    node.VR = vr;

    // Handle sequences.
    if(vr == "SQ"){
        if(length == 0xFFFFFFFF){
            read_sequence_items_undefined(is, node, enc, dicts, mutable_dict);
        }else{
            read_sequence_items_defined(is, node, enc, dicts, mutable_dict, length);
        }
        return node;
    }

    // Handle undefined length for non-SQ (encapsulated pixel data).
    if(length == 0xFFFFFFFF){
        const bool is_pixel_data_tag =
            (node.key.group == static_cast<uint16_t>(0x7FE0u) &&
             node.key.tag   == static_cast<uint16_t>(0x0010u));
        const bool vr_allows_encapsulation =
            (vr == "OB" || vr == "OW" || vr == "UN");

        if(is_pixel_data_tag && vr_allows_encapsulation){
            read_encapsulated_data(is, node);
            return node;
        }

        throw std::runtime_error(
            "Unsupported undefined-length non-sequence DICOM element: "
            "only PixelData (7FE0,0010) with VR OB/OW/UN may be encapsulated.");
    }

    // Read raw value bytes.
    std::string raw = read_bytes(is, length);

    // Decode the value based on VR.
    if(vr_is_text(vr)){
        node.val = strip_text_padding(raw, vr);
    }else if(vr == "US" || vr == "SS" || vr == "UL" || vr == "SL"
          || vr == "FL" || vr == "FD" || vr == "AT"
          || vr == "SV" || vr == "UV"){
        node.val = decode_binary_value(raw, vr);
    }else{
        // OB, OW, OF, OD, OL, OV, UN, and others: store raw bytes.
        node.val = std::move(raw);
    }

    return node;
}

} // anonymous namespace


///////////////////////////////////////////////////////////////////////////////
// Node::read_DICOM
///////////////////////////////////////////////////////////////////////////////

void Node::read_DICOM(std::istream &is,
                      const std::vector<const DICOMDictionary*> &dicts,
                      DICOMDictionary *mutable_dict){
    verify_little_endian();

    // Initialize this node as root.
    this->VR = "SQ";
    this->val.clear();
    this->children.clear();

    // Read the 128-byte preamble.
    {
        char preamble[128];
        is.read(preamble, 128);
        if(!is) throw std::runtime_error("Unable to read DICOM preamble.");
    }

    // Read the 4-byte "DICM" magic.
    {
        char magic[4];
        is.read(magic, 4);
        if(!is) throw std::runtime_error("Unable to read DICOM magic bytes.");
        if(std::string(magic, 4) != "DICM"){
            throw std::runtime_error("Not a valid DICOM Part 10 file (missing 'DICM' prefix).");
        }
    }

    // Parse the meta information group (0x0002). Always uses Explicit VR Little Endian encoding.
    while(is.good() && (is.peek() != std::char_traits<char>::eof())){
        auto pos = is.tellg();
        uint16_t g = read_uint16_le(is);
        uint16_t e = read_uint16_le(is);
        // Seek back before the full element read.
        is.seekg(pos);

        if(g != 0x0002) break; // End of meta information.

        auto node = read_data_element(is, Encoding::ELE, dicts, mutable_dict);
        this->children.push_back(std::move(node));
    }

    // Determine the data encoding from the TransferSyntaxUID (0002,0010).
    Encoding data_enc = Encoding::ELE; // Default if not specified.
    {
        const auto *ts_node = this->find(0x0002, 0x0010);
        if(ts_node != nullptr){
            std::string ts = ts_node->val;
            // Strip trailing padding (nulls/spaces).
            while(!ts.empty() && (ts.back() == '\0' || ts.back() == ' ')) ts.pop_back();

            if(ts == "1.2.840.10008.1.2"){
                data_enc = Encoding::ILE;
            }else if(ts == "1.2.840.10008.1.2.1"){
                data_enc = Encoding::ELE;
            }else if(ts == "1.2.840.10008.1.2.2"){
                throw std::runtime_error("Big-endian DICOM transfer syntax is not supported.");
            }
            // Other transfer syntaxes (e.g., compressed JPEG, RLE) parse the top-level
            // tags with ELE. The actual pixel data may be encapsulated and is stored as raw bytes.
        }
    }

    // Parse remaining data elements using the determined encoding.
    while(is.good() && (is.peek() != std::char_traits<char>::eof())){
        auto node = read_data_element(is, data_enc, dicts, mutable_dict);
        this->children.push_back(std::move(node));
    }
}


///////////////////////////////////////////////////////////////////////////////
// Tree search and modification utilities.
///////////////////////////////////////////////////////////////////////////////

Node* Node::find(uint16_t group, uint16_t tag){
    for(auto &child : this->children){
        if(child.key.group == group && child.key.tag == tag) return &child;
        auto *found = child.find(group, tag);
        if(found != nullptr) return found;
    }
    return nullptr;
}

const Node* Node::find(uint16_t group, uint16_t tag) const {
    for(const auto &child : this->children){
        if(child.key.group == group && child.key.tag == tag) return &child;
        const auto *found = child.find(group, tag);
        if(found != nullptr) return found;
    }
    return nullptr;
}

std::list<Node*> Node::find_all(uint16_t group, uint16_t tag){
    std::list<Node*> results;
    for(auto &child : this->children){
        if(child.key.group == group && child.key.tag == tag) results.push_back(&child);
        auto child_results = child.find_all(group, tag);
        results.splice(results.end(), std::move(child_results));
    }
    return results;
}

std::list<const Node*> Node::find_all(uint16_t group, uint16_t tag) const {
    std::list<const Node*> results;
    for(const auto &child : this->children){
        if(child.key.group == group && child.key.tag == tag) results.push_back(&child);
        auto child_results = child.find_all(group, tag);
        results.splice(results.end(), std::move(child_results));
    }
    return results;
}

bool Node::replace(uint16_t group, uint16_t tag, Node replacement){
    for(auto it = this->children.begin(); it != this->children.end(); ++it){
        if(it->key.group == group && it->key.tag == tag){
            *it = std::move(replacement);
            return true;
        }
        if(it->replace(group, tag, replacement)){
            return true;
        }
    }
    return false;
}

bool Node::remove(uint16_t group, uint16_t tag){
    for(auto it = this->children.begin(); it != this->children.end(); ++it){
        if(it->key.group == group && it->key.tag == tag){
            this->children.erase(it);
            return true;
        }
        if(it->remove(group, tag)){
            return true;
        }
    }
    return false;
}

int64_t Node::remove_all(uint16_t group, uint16_t tag){
    int64_t count = 0;
    for(auto it = this->children.begin(); it != this->children.end(); ){
        if(it->key.group == group && it->key.tag == tag){
            it = this->children.erase(it);
            ++count;
        }else{
            count += it->remove_all(group, tag);
            ++it;
        }
    }
    return count;
}


int64_t Node::remove_structural_tags(){
    // Remove GroupLength tags (gggg,0000) which are deprecated per DICOM PS3.5 Section 7.2
    // and are dynamically recomputed by emit_DICOM() for groups that require them (e.g.,
    // the File Meta Information group 0x0002). Retaining stale GroupLength tags from a
    // previously-read file would result in duplicate and/or out-of-date entries in the output.
    //
    // Note: Sequence-related structural tags (FFFE,E000 / FFFE,E00D / FFFE,E0DD) are not
    // stored as regular children in the tree -- they are handled implicitly by the SQ emission
    // logic -- so they do not need to be removed here.
    int64_t count = 0;
    for(auto it = this->children.begin(); it != this->children.end(); ){
        if(it->key.tag == 0x0000){
            it = this->children.erase(it);
            ++count;
        }else{
            count += it->remove_structural_tags();
            ++it;
        }
    }
    return count;
}


std::string Node::value_str() const {
    if(vr_is_text(this->VR)){
        return this->val;
    }else if(this->VR == "US" || this->VR == "SS" || this->VR == "UL" || this->VR == "SL"
          || this->VR == "FL" || this->VR == "FD" || this->VR == "AT"
          || this->VR == "SV" || this->VR == "UV"){
        // Already decoded to string representation during reading.
        return this->val;
    }else if(this->VR == "SQ"){
        return "(sequence with "_s + std::to_string(this->children.size()) + " items)";
    }else{
        // Binary blob VRs (OB, OW, OF, OD, OL, OV, UN): return a summary.
        if(this->val.size() <= 64){
            // Short enough to show as hex.
            std::ostringstream ss;
            ss << std::hex << std::setfill('0');
            for(size_t i = 0; i < this->val.size(); ++i){
                if(i > 0) ss << " ";
                ss << std::setw(2) << (static_cast<unsigned int>(static_cast<unsigned char>(this->val[i])));
            }
            return ss.str();
        }
        return "(" + std::to_string(this->val.size()) + " bytes)";
    }
}


bool Node::validate(Encoding enc,
                    const std::vector<const DICOMDictionary*> &dicts) const {
    YLOGDEBUG("validate: checking tree conformance with encoding "
              << (enc == Encoding::ELE ? "ELE" : (enc == Encoding::ILE ? "ILE" : "Other")));

    // Check each tag's VR against the dictionary VR and warn about mismatches,
    // but only if at least one dictionary has been explicitly provided.
    if(!dicts.empty()){
        std::function<void(const Node &)> check_vrs = [&](const Node &n) -> void {
            if(!n.VR.empty() && n.VR != "SQ" && n.VR != "MULTI"){
                const auto dict_vr = lookup_VR(n.key.group, n.key.tag, dicts);
                if(!dict_vr.empty() && dict_vr != n.VR){
                    YLOGWARN("Tag " << tag_diag(n.key.group, n.key.tag)
                             << " has VR '" << n.VR
                             << "' but dictionary specifies '" << dict_vr << "'");
                }
            }
            for(const auto &child : n.children){
                check_vrs(child);
            }
        };
        for(const auto &child : this->children){
            check_vrs(child);
        }
    }

    // Validate by attempting to emit the tree and checking for exceptions.
    // Note: lenient mode is intentionally *not* used -- the purpose of validation is to check strict conformance.
    try{
        std::ostringstream ss(std::ios_base::ate | std::ios_base::binary);
        this->emit_DICOM(ss, enc);
        return ss.good();
    }catch(const std::exception &e){
        YLOGDEBUG("validate: emission failed: " << e.what());
        return false;
    }
}


// Helper: strip trailing null and space padding from a DICOM value string.
static std::string strip_trailing_padding(const std::string &s){
    std::string out = s;
    while(!out.empty() && (out.back() == '\0' || out.back() == ' ')) out.pop_back();
    return out;
}

// Helper: look up or create a UID mapping. If the original UID is already in the map,
// return the mapped value; otherwise generate a new UID, store it, and return it.
static std::string map_uid(const std::string &original, uid_mapping_t &uid_map){
    const auto key = strip_trailing_padding(original);

    if(key.empty()) return key;

    auto it = uid_map.find(key);
    if(it != uid_map.end()){
        return it->second;
    }
    auto new_uid = Generate_Random_UID(60);
    uid_map[key] = new_uid;
    return new_uid;
}

// Helper: set the value of all nodes matching (group, tag) anywhere in the tree.
// The existing VR of each node is preserved. If no matching node exists, no action is taken.
static void set_tag_value_all(Node &root, uint16_t group, uint16_t tag,
                              const std::string &new_val){
    auto nodes = root.find_all(group, tag);
    for(auto *n : nodes){
        n->val = new_val;
    }
}

// Helper: remap all UID-valued nodes matching (group, tag) using the uid mapping.
static void remap_uid_tag(Node &root, uint16_t group, uint16_t tag, uid_mapping_t &uid_map){
    auto nodes = root.find_all(group, tag);
    for(auto *n : nodes){
        const auto old_val = strip_trailing_padding(n->val);
        if(!old_val.empty()){
            n->val = map_uid(old_val, uid_map);
        }
    }
}

// Tags to erase per DICOM Supplement 142 (may contain PHI).
static constexpr std::pair<uint16_t, uint16_t> tags_to_erase[] = {
    {0x0000, 0x1000}, // Affected SOP Instance UID
    {0x0008, 0x0024}, // Overlay Date
    {0x0008, 0x0025}, // Curve Date
    {0x0008, 0x0034}, // Overlay Time
    {0x0008, 0x0035}, // Curve Time
    {0x0008, 0x0081}, // Institution Address
    {0x0008, 0x0092}, // Referring Physician's Address
    {0x0008, 0x0094}, // Referring Physician's Telephone Numbers
    {0x0008, 0x0096}, // Referring Physician's Identification Sequence
    {0x0008, 0x0201}, // Timezone Offset From UTC
    {0x0008, 0x1040}, // Institutional Department Name
    {0x0008, 0x1048}, // Physician(s) of Record
    {0x0008, 0x1049}, // Physician(s) of Record Identification Sequence
    {0x0008, 0x1050}, // Performing Physicians' Name
    {0x0008, 0x1052}, // Performing Physician Identification Sequence
    {0x0008, 0x1060}, // Name of Physician(s) Reading Study
    {0x0008, 0x1062}, // Physician(s) Reading Study Identification Sequence
    {0x0008, 0x1072}, // Operators' Identification Sequence
    {0x0008, 0x0082}, // Institution Code Sequence
    {0x0008, 0x1080}, // Admitting Diagnoses Description
    {0x0008, 0x1084}, // Admitting Diagnoses Code Sequence
    {0x0008, 0x1110}, // Referenced Study Sequence
    {0x0008, 0x1111}, // Referenced Performed Procedure Step Sequence
    {0x0008, 0x1120}, // Referenced Patient Sequence
    {0x0008, 0x1140}, // Referenced Image Sequence
    {0x0008, 0x2111}, // Derivation Description
    {0x0008, 0x2112}, // Source Image Sequence
    {0x0008, 0x1032}, // Procedure Code Sequence
    {0x0008, 0x4000}, // Identifying Comments

    {0x0010, 0x0021}, // Issuer of Patient ID
    {0x0010, 0x0032}, // Patient's Birth Time
    {0x0010, 0x0050}, // Patient's Insurance Plan Code Sequence
    {0x0010, 0x0101}, // Patient's Primary Language Code Sequence
    {0x0010, 0x0102}, // Patient's Primary Language Modifier Code Sequence
    {0x0010, 0x1000}, // Other Patient IDs
    {0x0010, 0x1001}, // Other Patient Names
    {0x0010, 0x1002}, // Other Patient IDs Sequence
    {0x0010, 0x1005}, // Patient's Birth Name
    {0x0010, 0x1010}, // Patient's Age
    {0x0010, 0x1020}, // Patient's Size
    {0x0010, 0x1030}, // Patient's Weight
    {0x0010, 0x1040}, // Patient Address
    {0x0010, 0x1050}, // Insurance Plan Identification
    {0x0010, 0x1060}, // Patient's Mother's Birth Name
    {0x0010, 0x1080}, // Military Rank
    {0x0010, 0x1081}, // Branch of Service
    {0x0010, 0x1090}, // Medical Record Locator
    {0x0010, 0x2000}, // Medical Alerts
    {0x0010, 0x2110}, // Allergies
    {0x0010, 0x2150}, // Country of Residence
    {0x0010, 0x2152}, // Region of Residence
    {0x0010, 0x2154}, // Patient's Telephone Numbers
    {0x0010, 0x2160}, // Ethnic Group
    {0x0010, 0x2180}, // Occupation
    {0x0010, 0x21A0}, // Smoking Status
    {0x0010, 0x21B0}, // Additional Patient's History
    {0x0010, 0x21C0}, // Pregnancy Status
    {0x0010, 0x21D0}, // Last Menstrual Date
    {0x0010, 0x21F0}, // Patient's Religious Preference
    {0x0010, 0x2297}, // Responsible Person
    {0x0010, 0x2299}, // Responsible Organization
    {0x0010, 0x4000}, // Patient Comments

    {0x0012, 0x0062}, // Patient Identity Removed
    {0x0012, 0x0063}, // Deidentification Method

    {0x0018, 0x1004}, // Plate ID
    {0x0018, 0x1005}, // Generator ID
    {0x0018, 0x1007}, // Cassette ID
    {0x0018, 0x1008}, // Gantry ID
    {0x0018, 0x4000}, // Acquisition Comments
    {0x0018, 0x9424}, // Acquisition Protocol Description
    {0x0018, 0xA003}, // Contribution Description

    {0x0020, 0x3401}, // Modifying Device ID
    {0x0020, 0x3404}, // Modifying Device Manufacturer
    {0x0020, 0x3406}, // Modified Image Description
    {0x0020, 0x4000}, // Image Comments
    {0x0020, 0x9158}, // Frame Comments

    {0x0028, 0x4000}, // Image Presentation Comments

    {0x0032, 0x0012}, // Study ID Issuer
    {0x0032, 0x1020}, // Scheduled Study Location
    {0x0032, 0x1021}, // Scheduled Study Location AE Title
    {0x0032, 0x1030}, // Reason for Study
    {0x0032, 0x1032}, // Requesting Physician
    {0x0032, 0x1033}, // Requesting Service
    {0x0032, 0x1070}, // Requested Contrast Agent
    {0x0032, 0x4000}, // Study Comments

    {0x0038, 0x0004}, // Referenced Patient Alias Sequence
    {0x0038, 0x0010}, // Admission ID
    {0x0038, 0x0011}, // Issuer of Admission ID
    {0x0038, 0x001E}, // Scheduled Patient Institution Residence
    {0x0038, 0x0020}, // Admitting Date
    {0x0038, 0x0021}, // Admitting Time
    {0x0038, 0x0040}, // Discharge Diagnosis Description
    {0x0038, 0x0050}, // Special Needs
    {0x0038, 0x0060}, // Service Episode ID
    {0x0038, 0x0061}, // Issuer of Service Episode ID
    {0x0038, 0x0062}, // Service Episode Description
    {0x0038, 0x0300}, // Current Patient Location
    {0x0038, 0x0400}, // Patient's Institution Residence
    {0x0038, 0x0500}, // Patient State
    {0x0038, 0x4000}, // Visit Comments

    {0x0040, 0x0001}, // Scheduled Station AE Title
    {0x0040, 0x0002}, // Scheduled Procedure Step Start Date
    {0x0040, 0x0003}, // Scheduled Procedure Step Start Time
    {0x0040, 0x0004}, // Scheduled Procedure Step End Date
    {0x0040, 0x0005}, // Scheduled Procedure Step End Time
    {0x0040, 0x0006}, // Scheduled Performing Physician Name
    {0x0040, 0x0007}, // Scheduled Procedure Step Description
    {0x0040, 0x000B}, // Scheduled Performing Physician Identification Sequence
    {0x0040, 0x0010}, // Scheduled Station Name
    {0x0040, 0x0011}, // Scheduled Procedure Step Location
    {0x0040, 0x0012}, // Pre-Medication
    {0x0040, 0x0241}, // Performed Station AE Title
    {0x0040, 0x0242}, // Performed Station Name
    {0x0040, 0x0243}, // Performed Location
    {0x0040, 0x0250}, // Performed Procedure Step End Date
    {0x0040, 0x0251}, // Performed Procedure Step End Time
    {0x0040, 0x0275}, // Request Attributes Sequence
    {0x0040, 0x0280}, // Comments on the Performed Procedure Step
    {0x0040, 0x0555}, // Acquisition Context Sequence
    {0x0040, 0x1001}, // Requested Procedure ID
    {0x0040, 0x1004}, // Patient Transport Arrangements
    {0x0040, 0x1005}, // Requested Procedure Location
    {0x0040, 0x1010}, // Names of Intended Recipient of Results
    {0x0040, 0x1011}, // Intended Recipients of Results Identification Sequence
    {0x0040, 0x1101}, // Person Identification Code Sequence
    {0x0040, 0x1102}, // Person Address
    {0x0040, 0x1103}, // Person Telephone Numbers
    {0x0040, 0x1400}, // Requested Procedure Comments
    {0x0040, 0x2001}, // Reason for the Imaging Service Request
    {0x0040, 0x2008}, // Order Entered By
    {0x0040, 0x2009}, // Order Enterer Location
    {0x0040, 0x2010}, // Order Callback Phone Number
    {0x0040, 0x2400}, // Imaging Service Request Comments
    {0x0040, 0x3001}, // Confidentiality Constraint on Patient Data Description
    {0x0040, 0x4025}, // Scheduled Station Name Code Sequence
    {0x0040, 0x4027}, // Scheduled Station Geographic Location Code Sequence
    {0x0040, 0x4028}, // Performed Station Name Code Sequence
    {0x0040, 0x4030}, // Performed Station Geographic Location Code Sequence
    {0x0040, 0x4034}, // Scheduled Human Performers Sequence
    {0x0040, 0x4035}, // Actual Human Performers Sequence
    {0x0040, 0x4036}, // Human Performers Organization
    {0x0040, 0x4037}, // Human Performers Name
    {0x0040, 0xA027}, // Verifying Organization
    {0x0040, 0xA073}, // Verifying Observer Sequence
    {0x0040, 0xA078}, // Author Observer Sequence
    {0x0040, 0xA07A}, // Participant Sequence
    {0x0040, 0xA07C}, // Custodial Organization Sequence
    {0x0040, 0xA088}, // Verifying Observer Identification Code Sequence
    {0x0040, 0xA730}, // Content Sequence

    {0x0070, 0x0001}, // Graphic Annotation Sequence
    {0x0070, 0x0086}, // Content Creator's Identification Code Sequence

    {0x0088, 0x0200}, // Icon Image Sequence
    {0x0088, 0x0904}, // Topic Title
    {0x0088, 0x0906}, // Topic Subject
    {0x0088, 0x0910}, // Topic Author
    {0x0088, 0x0912}, // Topic Keywords

    {0x0400, 0x0100}, // Digital Signature UID
    {0x0400, 0x0402}, // Referenced Digital Signature Sequence
    {0x0400, 0x0403}, // Referenced SOP Instance MAC Sequence
    {0x0400, 0x0404}, // MAC
    {0x0400, 0x0500}, // Encrypted Attributes Sequence
    {0x0400, 0x0550}, // Modified Attributes Sequence
    {0x0400, 0x0561}, // Original Attributes Sequence

    {0x2030, 0x0020}, // Text String

    {0x4000, 0x0010}, // Arbitrary
    {0x4000, 0x4000}, // Text Comments

    {0x4008, 0x0042}, // Results ID Issuer
    {0x4008, 0x0102}, // Interpretation Recorder
    {0x4008, 0x010A}, // Interpretation Transcriber
    {0x4008, 0x010B}, // Interpretation Text
    {0x4008, 0x010C}, // Interpretation Author
    {0x4008, 0x0111}, // Interpretation Approver Sequence
    {0x4008, 0x0114}, // Physician Approving Interpretation
    {0x4008, 0x0115}, // Interpretation Diagnosis Description
    {0x4008, 0x0118}, // Results Distribution List Sequence
    {0x4008, 0x0119}, // Distribution Name
    {0x4008, 0x011A}, // Distribution Address
    {0x4008, 0x0202}, // Interpretation ID Issuer
    {0x4008, 0x0300}, // Impressions
    {0x4008, 0x4000}, // Results Comments

    {0xFFFA, 0xFFFA}, // Digital Signatures Sequence
    {0xFFFC, 0xFFFC}, // Data Set Trailing Padding
};

// UID tags to remap during de-identification.
static constexpr std::pair<uint16_t, uint16_t> uid_tags_to_remap[] = {
    {0x0002, 0x0003}, // Media Storage SOP Instance UID
    {0x0008, 0x0014}, // Instance Creator UID
    {0x0008, 0x0018}, // SOP Instance UID
    {0x0008, 0x0058}, // Failed SOP Instance UID List
    {0x0008, 0x010D}, // Context Group Extension Creator UID
    {0x0008, 0x1155}, // Referenced SOP Instance UID
    {0x0008, 0x1195}, // Transaction UID
    {0x0008, 0x3010}, // Irradiation Event UID
    {0x0008, 0x9123}, // Creator Version UID
    {0x0000, 0x1001}, // Requested SOP Instance UID
    {0x0004, 0x1511}, // Referenced SOP Instance UID in File
    {0x0018, 0x1002}, // Device UID
    {0x0020, 0x000D}, // Study Instance UID
    {0x0020, 0x000E}, // Series Instance UID
    {0x0020, 0x0052}, // Frame of Reference UID
    {0x0020, 0x0200}, // Synchronization Frame of Reference UID
    {0x0020, 0x9161}, // Concatenation UID
    {0x0020, 0x9164}, // Dimension Organization UID
    {0x0028, 0x1199}, // Palette Color Lookup Table UID
    {0x0028, 0x1214}, // Large Palette Color Lookup Table UID
    {0x003A, 0x0310}, // Multiplex Group UID
    {0x0040, 0x4023}, // Referenced General Purpose Sched. Procedure Step Transaction UID
    {0x0040, 0xA124}, // UID
    {0x0040, 0xDB0C}, // Template Extension Organization UID
    {0x0040, 0xDB0D}, // Template Extension Creator UID
    {0x0070, 0x031A}, // Fiducial UID
    {0x0088, 0x0140}, // Storage Media File-set UID
    {0x3006, 0x0024}, // Referenced Frame of Reference UID
    {0x3006, 0x00C2}, // Related Frame of Reference UID
    {0x300A, 0x0013}, // Dose Reference UID
};


void deidentify(Node &root,
                const DeidentifyParams &params,
                uid_mapping_t &uid_map){

    YLOGDEBUG("deidentify: starting de-identification with patient_id='" << params.patient_id
              << "' patient_name='" << params.patient_name
              << "' study_id='" << params.study_id << "'");

    // Get today's date and time strings in DICOM format using Ygor's time_mark.
    // Dump_as_postgres_string() returns "YYYY-MM-DD HH:MM:SS" (19 characters).
    const auto datetime_str = time_mark().Dump_as_postgres_string();
    // Convert to DICOM date format: YYYYMMDD
    std::string todays_date = "19700101";
    if(datetime_str.size() >= 10 && datetime_str[4] == '-' && datetime_str[7] == '-'){
        todays_date = datetime_str.substr(0, 4) + datetime_str.substr(5, 2) + datetime_str.substr(8, 2);
    }
    // Convert to DICOM time format: HHMMSS
    std::string todays_time = "000000";
    if(datetime_str.size() >= 19 && datetime_str[13] == ':' && datetime_str[16] == ':'){
        todays_time = datetime_str.substr(11, 2) + datetime_str.substr(14, 2) + datetime_str.substr(17, 2);
    }
    const std::string todays_datetime = todays_date + todays_time;

    // -----------------------------------------------------------------------
    // Step 1: Erase tags per DICOM Supplement 142.
    //
    // These tags may contain PHI and are removed entirely.
    // -----------------------------------------------------------------------
    YLOGDEBUG("deidentify: step 1 -- erasing PHI tags");
    for(const auto &t : tags_to_erase){
        root.remove_all(t.first, t.second);
    }

    // Also erase overlay data and overlay comments (groups 0x6000-0x60FF).
    for(uint16_t g = 0x6000; g <= 0x60FF; ++g){
        root.remove_all(g, 0x3000); // Overlay Data
        root.remove_all(g, 0x4000); // Overlay Comments
    }

    // Always erase study description, series description, and study ID.
    // If the caller provides replacements, they will be added back below.
    root.remove_all(0x0008, 0x1030); // Study Description
    root.remove_all(0x0008, 0x103E); // Series Description
    root.remove_all(0x0020, 0x0010); // Study ID

    // -----------------------------------------------------------------------
    // Step 2: Modify tags -- replace with de-identified values.
    //
    // These tags are replaced with anonymized/generic values if present.
    // The existing VR from the parsed file is preserved.
    // -----------------------------------------------------------------------
    YLOGDEBUG("deidentify: step 2 -- replacing tags with de-identified values");

    // Dates and times.
    set_tag_value_all(root, 0x0008, 0x0020, todays_date);   // Study Date
    set_tag_value_all(root, 0x0008, 0x0021, todays_date);   // Series Date
    set_tag_value_all(root, 0x0008, 0x0022, todays_date);   // Acquisition Date
    set_tag_value_all(root, 0x0008, 0x0023, todays_date);   // Content Date
    set_tag_value_all(root, 0x0008, 0x0030, todays_time);   // Study Time
    set_tag_value_all(root, 0x0008, 0x0031, todays_time);   // Series Time
    set_tag_value_all(root, 0x0008, 0x0032, todays_time);   // Acquisition Time
    set_tag_value_all(root, 0x0008, 0x0033, todays_time);   // Content Time
    set_tag_value_all(root, 0x0008, 0x002A, todays_datetime); // Acquisition DateTime
    set_tag_value_all(root, 0x0008, 0x0012, todays_date);   // Instance Creation Date
    set_tag_value_all(root, 0x0008, 0x0013, todays_time);   // Instance Creation Time

    set_tag_value_all(root, 0x0010, 0x0030, todays_date);   // Patient's Birth Date

    set_tag_value_all(root, 0x0040, 0x0244, todays_date);   // Performed Procedure Step Start Date
    set_tag_value_all(root, 0x0040, 0x0245, todays_time);   // Performed Procedure Step Start Time

    set_tag_value_all(root, 0x300A, 0x0006, todays_date);   // RT Plan Date
    set_tag_value_all(root, 0x300A, 0x0007, todays_time);   // RT Plan Time

    // Anonymous replacement values for names, institutions, devices, etc.
    const std::string anon = "Anonymous";

    set_tag_value_all(root, 0x0008, 0x0070, anon);   // Manufacturer
    set_tag_value_all(root, 0x0008, 0x0080, anon);   // Institution Name
    set_tag_value_all(root, 0x0008, 0x0090, anon);   // Referring Physician's Name
    set_tag_value_all(root, 0x0008, 0x1010, anon);   // Station Name
    set_tag_value_all(root, 0x0008, 0x1070, anon);   // Operators' Name
    set_tag_value_all(root, 0x0008, 0x1090, anon);   // Manufacturer Model Name
    set_tag_value_all(root, 0x0018, 0x0010, anon);   // Contrast/Bolus Agent
    set_tag_value_all(root, 0x0018, 0x1000, anon);   // Device Serial Number
    set_tag_value_all(root, 0x0018, 0x1020, anon);   // Software Versions
    set_tag_value_all(root, 0x0018, 0x1030, anon);   // Protocol Name
    set_tag_value_all(root, 0x0018, 0x1400, anon);   // Acquisition Device Processing Description
    set_tag_value_all(root, 0x0018, 0x700A, anon);   // Detector ID
    set_tag_value_all(root, 0x0032, 0x1060, anon);   // Requested Procedure Description
    set_tag_value_all(root, 0x0040, 0x0253, anon);   // Performed Procedure Step ID
    set_tag_value_all(root, 0x0040, 0x0254, anon);   // Performed Procedure Step Description
    set_tag_value_all(root, 0x0040, 0xA075, anon);   // Verifying Observer Name
    set_tag_value_all(root, 0x0040, 0xA123, anon);   // Person Name
    set_tag_value_all(root, 0x0070, 0x0084, anon);   // Content Creator's Name
    set_tag_value_all(root, 0x0010, 0x2203, anon);   // Patient's Sex Neutered
    set_tag_value_all(root, 0x300E, 0x0008, anon);   // Reviewer Name

    // RT-specific anonymization.
    set_tag_value_all(root, 0x3002, 0x0004, anon);   // RT Image Description
    set_tag_value_all(root, 0x3002, 0x0020, anon);   // Radiation Machine Name
    set_tag_value_all(root, 0x300A, 0x00B2, anon);   // Treatment Machine Name
    set_tag_value_all(root, 0x300A, 0x0002, anon);   // RT Plan Label
    set_tag_value_all(root, 0x300A, 0x0003, anon);   // RT Plan Name
    set_tag_value_all(root, 0x300A, 0x0016, anon);   // Dose Reference Description

    // Clear these tags (set to empty string).
    set_tag_value_all(root, 0x0010, 0x0040, "");  // Patient's Sex
    set_tag_value_all(root, 0x0008, 0x0050, "");  // Accession Number
    set_tag_value_all(root, 0x0040, 0x2016, "");  // Placer Order Number
    set_tag_value_all(root, 0x0040, 0x2017, "");  // Filler Order Number

    // -----------------------------------------------------------------------
    // Step 3: Set patient/study/series identification.
    // -----------------------------------------------------------------------
    YLOGDEBUG("deidentify: step 3 -- setting patient/study/series identification");
    set_tag_value_all(root, 0x0010, 0x0020, params.patient_id);    // Patient ID
    set_tag_value_all(root, 0x0010, 0x0010, params.patient_name);  // Patient's Name
    // Ensure Patient ID and Patient's Name tags exist exactly once at the root.
    // First remove any existing root-level instances of these tags, then insert
    // a single anonymized value for each.
    root.remove_all(0x0010, 0x0020);
    root.emplace_child_node({{0x0010, 0x0020}, "LO", params.patient_id});    // Patient ID
    root.remove_all(0x0010, 0x0010);
    root.emplace_child_node({{0x0010, 0x0010}, "PN", params.patient_name});  // Patient's Name

    // Study ID is required and always inserted.
    root.emplace_child_node({{0x0020, 0x0010}, "SH", params.study_id});  // Study ID

    if(params.study_description.has_value()){
        root.emplace_child_node({{0x0008, 0x1030}, "LO", params.study_description.value()});  // Study Description
    }
    if(params.series_description.has_value()){
        root.emplace_child_node({{0x0008, 0x103E}, "LO", params.series_description.value()});  // Series Description
    }

    // -----------------------------------------------------------------------
    // Step 4: Remap UIDs consistently.
    //
    // Each UID encountered in these tags is replaced with a mapped UID.
    // If the same original UID appears again (in this file or future files),
    // the same replacement UID is used.
    // -----------------------------------------------------------------------
    YLOGDEBUG("deidentify: step 4 -- remapping UIDs");
    for(const auto &t : uid_tags_to_remap){
        remap_uid_tag(root, t.first, t.second, uid_map);
    }
}


} // namespace DCMA_DICOM
