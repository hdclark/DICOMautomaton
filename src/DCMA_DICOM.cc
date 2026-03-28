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

#include "DCMA_DICOM.h"

namespace DCMA_DICOM {

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
               const std::string& val){

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
                const uint64_t child_length = n.emit_DICOM(child_ss, enc, false);
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
            const auto space_char = (node.VR == "UI") ? static_cast<unsigned char>('\0')
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
                const uint64_t child_length = n.emit_DICOM(child_ss, enc, false);
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

        // Some tags have reserved space.
        }else if( (node.VR == "OB")
              ||  (node.VR == "OW")
              ||  (node.VR == "OF")
              ||  (node.VR == "UT")
              ||  (node.VR == "UN") ){
            const auto length    = static_cast<uint32_t>(val.length());
            const auto add_space = static_cast<uint32_t>(length % 2);
            const uint32_t full_length = (length + add_space);
            const uint16_t zero_16 = 0;
            const uint8_t space_char = 0;

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
                          bool is_root_node) const {

    // Used to search for forbidden characters.
    const std::string upper_case("ABCDEFGHIJKLMNOPQRSTUVWXYZ");
    const std::string lower_case("abcdefghijklmnopqrstuvwxyz");
    const std::string number_digits("0123456789");
    const std::string multiplicity(R"***(\)***"); // Value multiplicity separator.

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
            group_length += child_it->emit_DICOM(child_ss, child_enc, false);

            // Evaluate whether the following node will be from a different group.
            // If so, emit the group length tag and all children in the buffer.
            const auto next_child_it = std::next(child_it);
            if( (next_child_it == end_child) 
            ||  (child_it->key.group != next_child_it->key.group) ){

                // Emit the group length tag.
                if( (child_it->key.group <= 0x0002)
                &&  (child_enc == Encoding::ELE) ){  // TODO: Should I bother with this after group 0x0002? It is deprecated...
                    Node group_length_node({child_it->key.group, 0x0000}, "UL", std::to_string(group_length));
                    cumulative_length += group_length_node.emit_DICOM(os, child_enc, false);
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
            throw std::logic_error("'MULTI' nodes can not have any data associated with them. (Is it intentional?)");
        }

        // Process children nodes serially, without any boilerplate or markers between children.
        for(const auto & c : this->children){
            cumulative_length += c.emit_DICOM(os, enc, false);
        }

    // Text types.
    }else if( this->VR == "CS" ){ //Code strings.
        // Value multiplicity embiggens the maximum permissable length, but each individual element should be <= 16 chars.
        auto tokens = SplitStringToVector(this->val,'\\','d');
        for(const auto &token : tokens){
            if(16ULL < token.length()) throw std::invalid_argument("Code string is too long. Cannot continue.");
        }

        if(this->val.find_first_not_of(upper_case + number_digits + multiplicity + "_ ") != std::string::npos){
            throw std::invalid_argument("Invalid character found in code string. Cannot continue.");
        }
        cumulative_length += emit_DICOM_tag(os, enc, *this, this->val);

    }else if( this->VR == "SH" ){ //Short string.
        if(16ULL < this->val.length()) throw std::runtime_error("Short string is too long. Consider using a longer VR. Cannot continue.");
        cumulative_length += emit_DICOM_tag(os, enc, *this, this->val);

    }else if( this->VR == "LO" ){ //Long strings.
        if(64ULL < this->val.length()) throw std::runtime_error("Long string is too long. Consider using a longer VR. Cannot continue.");
        cumulative_length += emit_DICOM_tag(os, enc, *this, this->val);

    }else if( this->VR == "ST" ){ //Short text.
        if(1024ULL < this->val.length()) throw std::runtime_error("Short text is too long. Consider using a longer VR. Cannot continue.");
        cumulative_length += emit_DICOM_tag(os, enc, *this, this->val);

    }else if( this->VR == "LT" ){ //Long text.
        if(10240ULL < this->val.length()) throw std::runtime_error("Long text is too long. Consider using a longer VR. Cannot continue.");
        cumulative_length += emit_DICOM_tag(os, enc, *this, this->val);

    }else if( this->VR == "UT" ){ //Unlimited text.
        if(4'294'967'294ULL < this->val.length()) throw std::runtime_error("Unlimited text is too long. Cannot continue.");
        cumulative_length += emit_DICOM_tag(os, enc, *this, this->val);


    // Name types.
    }else if( this->VR == "AE" ){ //Application entity.
        if(16ULL < this->val.length()) throw std::runtime_error("Application entity is too long. Cannot continue.");
        cumulative_length += emit_DICOM_tag(os, enc, *this, this->val);

    }else if( this->VR == "PN" ){ //Person name.
        if(64ULL < this->val.length()) throw std::runtime_error("Person name is too long. Cannot continue.");
        cumulative_length += emit_DICOM_tag(os, enc, *this, this->val);

    }else if( this->VR == "UI" ){ //Unique Identifier (UID).
        if(64ULL < this->val.length()) throw std::runtime_error("UID is too long. Cannot continue.");
        // Does value multiplicity embiggen the maximum permissable length? TODO
        if(this->val.find_first_not_of(number_digits + multiplicity + ".") != std::string::npos){
            throw std::invalid_argument("Invalid character found in UID. Cannot continue.");
        }

        // Ensure there are no leading insignificant zeros.
        auto tokens = SplitStringToVector(this->val,'.','d');
        for(const auto &token : tokens){
            if( (1 < token.size()) && (token.at(0) == '0') ){
                throw std::invalid_argument("UID contains an insignificant leading zero. Refusing to continue.");
            }
        }
        cumulative_length += emit_DICOM_tag(os, enc, *this, this->val);


    //Date and Time.
    }else if( this->VR == "DA" ){  //Date.
        //Strip away colons. Also strip away everything after the leading non-numeric char.
        std::string digits_only(val);
        digits_only = PurgeCharsFromString(digits_only,":-");
        auto avec = SplitStringToVector(digits_only,'.','d');
        avec.resize(1);
        digits_only = Lineate_Vector(avec, "");

        if(8ULL < digits_only.length()) throw std::runtime_error("Date is too long. Cannot continue.");
        if(digits_only.find_first_not_of(number_digits) != std::string::npos){
            throw std::invalid_argument("Invalid character found in date. Cannot continue.");
        }
        cumulative_length += emit_DICOM_tag(os, enc, *this, digits_only);

    }else if( this->VR == "TM" ){  //Time.
        //Strip away colons. Also strip away everything after the leading non-numeric char.
        std::string digits_only(val);
        digits_only = PurgeCharsFromString(digits_only,":-");
        auto avec = SplitStringToVector(digits_only,'.','d');
        avec.resize(1);
        digits_only = Lineate_Vector(avec, "");

        if(16ULL < digits_only.length()) throw std::runtime_error("Time is too long. Cannot continue.");
        if(digits_only.find_first_not_of(number_digits + ".") != std::string::npos){
            throw std::invalid_argument("Invalid character found in time. Cannot continue.");
        }
        cumulative_length += emit_DICOM_tag(os, enc, *this, digits_only);

    }else if( this->VR == "DT" ){  //Date Time.
        //Strip away colons. Also strip away everything after the leading non-numeric char.
        std::string digits_only(val);
        digits_only = PurgeCharsFromString(digits_only,":-");
        auto avec = SplitStringToVector(digits_only,'.','d');
        avec.resize(1);
        digits_only = Lineate_Vector(avec, "");

        if(26ULL < digits_only.length()) throw std::runtime_error("Date-time is too long. Cannot continue.");
        if(digits_only.find_first_not_of(number_digits + "+-.") != std::string::npos){
            throw std::invalid_argument("Invalid character found in date-time. Cannot continue.");
        }
        cumulative_length += emit_DICOM_tag(os, enc, *this, digits_only);

    }else if( this->VR == "AS" ){ //Age string.
        if(4ULL < this->val.length()) throw std::runtime_error("Age string is too long. Cannot continue.");
        if(this->val.find_first_not_of(number_digits + "DWMY") != std::string::npos){
            throw std::invalid_argument("Invalid character found in age string. Cannot continue.");
        }
        if(this->val.find_first_of("DWMY") == std::string::npos){
            throw std::invalid_argument("Age string is missing one of 'DWMY' characters. Cannot continue.");
        }
        cumulative_length += emit_DICOM_tag(os, enc, *this, this->val);


    //Binary types.
    }else if( this->VR == "OB" ){ //'Other' binary string: a string of bytes that doesn't fit any other VR.
        cumulative_length += emit_DICOM_tag(os, enc, *this, this->val);

    }else if( this->VR == "OW" ){ //'Other word string': a string of 16bit values.
        // Note: Assuming here that the list is represented as a string of unsigned integers (e.g., '123\234\0\25').
        std::ostringstream ss(std::ios_base::ate | std::ios_base::binary);
        auto tokens = SplitStringToVector(this->val, '\\', 'd');
        if(tokens.empty()) throw std::runtime_error("No values found for encoding OW tag. Cannot continue.");
        for(auto &token_val : tokens){
            const auto val_u = static_cast<uint16_t>(std::stoul(token_val));
            write_to_stream(ss, val_u, 2, enc);
        }
        cumulative_length += emit_DICOM_tag(os, enc, *this, ss.str());


    //Numeric types that are written as a string of characters.
    }else if( this->VR == "IS" ){ //Integer string.
        // I'm not sure if what the upper limit is for this VR type. Assuming 65534 for consistency with DS. TODO.
        if( ( (enc == Encoding::ELE) && (65'534ULL < this->val.length()) )
        ||  ( (enc == Encoding::ILE) && (4'294'967'295ULL < this->val.length()) ) ){
            throw std::invalid_argument("Integer string is too long. Cannot continue.");
        }

        auto tokens = SplitStringToVector(this->val,'\\','d');
        for(const auto &token : tokens){
            // Maximum length per decimal number: 16 bytes.
            if(12ULL < token.length()) throw std::invalid_argument("Integer string element is too long. Cannot continue.");

            // Ensure that, if an element is present it parses as a number.
            try{
                if(!token.empty()) [[maybe_unused]] auto r = std::stoll(token);
            }catch(const std::exception &e){
                throw std::runtime_error("Unable to convert '"_s + token + "' to IS. Cannot continue.");
            }
        }

        if(this->val.find_first_not_of(number_digits + multiplicity + "+-") != std::string::npos){
            throw std::invalid_argument("Invalid character found in integer string. Cannot continue.");
        }
        cumulative_length += emit_DICOM_tag(os, enc, *this, this->val);

    }else if( this->VR == "DS" ){ //Decimal string.
        // Maximum length for entire string (when multiple values are encoded and each is <= 16 bytes): 65534 bytes
        if( ( (enc == Encoding::ELE) && (65'534ULL < this->val.length()) )
        ||  ( (enc == Encoding::ILE) && (4'294'967'295ULL < this->val.length()) ) ){
            throw std::invalid_argument("Decimal string is too long. Cannot continue.");
        }

        auto tokens = SplitStringToVector(this->val,'\\','d');
        for(const auto &token : tokens){
            // Maximum length per decimal number: 16 bytes.
            if(16ULL < token.length()) throw std::invalid_argument("Decimal string element is too long. Cannot continue.");

            // Ensure that if an element is present it parses as a number.
            try{
                if(!token.empty()) [[maybe_unused]] auto r = std::stod(token);
            }catch(const std::exception &e){
                throw std::runtime_error("Unable to convert '"_s + token + "' to DS. Cannot continue.");
            }
        }

        if(this->val.find_first_not_of(number_digits + multiplicity + "+-eE.") != std::string::npos){
            throw std::invalid_argument("Invalid character found in decimal string. Cannot continue.");
        }
        cumulative_length += emit_DICOM_tag(os, enc, *this, this->val);


    //Numeric types that must be binary encoded.
    }else if( this->VR == "FL" ){ //Floating-point.
        std::ostringstream ss(std::ios_base::ate | std::ios_base::binary);
        const float val_f = std::stof(this->val);
        write_to_stream(ss, val_f, 4, enc);
        cumulative_length += emit_DICOM_tag(os, enc, *this, ss.str());
        // TODO: Ensure IEEE 754:1985 32-bit format.

    }else if( this->VR == "FD" ){ //Floating-point double.
        std::ostringstream ss(std::ios_base::ate | std::ios_base::binary);
        const double val_d = std::stod(this->val);
        write_to_stream(ss, val_d, 8, enc);
        cumulative_length += emit_DICOM_tag(os, enc, *this, ss.str());
        // TODO: Ensure IEEE 754:1985 64-bit format.

    }else if( this->VR == "OF" ){ //"Other" floating-point.
        //The value payload may contain multiple floats separated by some partitioning character.
        // For example, '1.23\2.34\0.00\25E25\-1.23'.
        std::ostringstream ss(std::ios_base::ate | std::ios_base::binary);
        auto tokens = SplitStringToVector(this->val, '\\', 'd');
        for(auto &token_val : tokens){
            const float val_f = std::stof(token_val);
            write_to_stream(ss, val_f, 4, enc);
        }
        cumulative_length += emit_DICOM_tag(os, enc, *this, ss.str());
        // TODO: Ensure IEEE 754:1985 32-bit format.

    }else if( this->VR == "OD" ){ //"Other" floating-point double.
        //The value payload may contain multiple floats separated by some partitioning character.
        // For example, '1.23\2.34\0.00\25E25\-1.23'.
        std::ostringstream ss(std::ios_base::ate | std::ios_base::binary);
        auto tokens = SplitStringToVector(this->val, '\\', 'd');
        for(auto &token_val : tokens){
            const float val_f = std::stod(token_val);
            write_to_stream(ss, val_f, 8, enc);
        }
        cumulative_length += emit_DICOM_tag(os, enc, *this, ss.str());
        // TODO: Ensure IEEE 754:1985 64-bit format.

    }else if( this->VR == "SS" ){ //Signed short (16bit).
        std::ostringstream ss(std::ios_base::ate | std::ios_base::binary);
        const int16_t val_i = std::stoi(this->val);
        write_to_stream(ss, val_i, 2, enc);
        cumulative_length += emit_DICOM_tag(os, enc, *this, ss.str());

    }else if( this->VR == "US" ){ //Unsigned short (16bit).
        std::ostringstream ss(std::ios_base::ate | std::ios_base::binary);
        const auto val_u = static_cast<uint16_t>(std::stoul(this->val));
        write_to_stream(ss, val_u, 2, enc);
        cumulative_length += emit_DICOM_tag(os, enc, *this, ss.str());

    }else if( this->VR == "SL" ){ //Signed long (32bit).
        std::ostringstream ss(std::ios_base::ate | std::ios_base::binary);
        const int32_t val_l = std::stol(this->val);
        write_to_stream(ss, val_l, 4, enc);
        cumulative_length += emit_DICOM_tag(os, enc, *this, ss.str());

    }else if( this->VR == "UL" ){ //Unsigned long (32bit).
        std::ostringstream ss(std::ios_base::ate | std::ios_base::binary);
        const uint32_t val_ul = std::stoul(this->val);
        write_to_stream(ss, val_ul, 4, enc);
        cumulative_length += emit_DICOM_tag(os, enc, *this, ss.str());

    }else if( this->VR == "AT" ){ //Attribute tag (2x unsigned shorts representing a DICOM data tag).
        // Assuming the value payload contains exactly two unsigned integers, e.g., '123\234'.
        std::ostringstream ss(std::ios_base::ate | std::ios_base::binary);
        auto tokens = SplitStringToVector(this->val, '\\', 'd');
        if(tokens.size() != 2ULL) throw std::runtime_error("Invalid number of integers for AT type tag; exactly 2 are needed.");
        for(auto &token_val : tokens){
            const auto val_u = static_cast<uint16_t>(std::stoul(token_val));
            write_to_stream(ss, val_u, 2, enc);
        }
        cumulative_length += emit_DICOM_tag(os, enc, *this, ss.str());


    //Other types.
    }else if( this->VR == "UN" ){ //Unknown. Often needed for handling private DICOM tags.
        cumulative_length += emit_DICOM_tag(os, enc, *this, this->val);

    }else if( this->VR == "SQ" ){ //Sequence.
        // Verify the node does not have any data associated with it. If it does, it probably indicates a logic
        // error since only children nodes should contain data.
        if(!this->val.empty()){
            throw std::logic_error("Nodes with 'SQ' VR can not have any data associated with them. (Is it intentional?)");
        }

        // Recursive calls happen in the following routine.
        cumulative_length += emit_DICOM_tag(os, enc, *this, this->val);

    }else{
        throw std::runtime_error("Unknown VR type. Cannot write to tag.");
    }

    return cumulative_length;
}


bool validate_VR_conformance(const std::string &VR,
                             const std::string &val,
                             DCMA_DICOM::Encoding enc ){
    // In many cases validation can only be done when actually writing the DICOM.
    // To avoid duplicating the validation checks during emission, we simulate writing a DICOM file with the given
    // content. This results in a slow runtime, but avoids tricky code duplication.
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
// DICOM Dictionary.
///////////////////////////////////////////////////////////////////////////////

const DICOMDictionary& get_default_dictionary(){
    // This dictionary covers tags used throughout DICOMautomaton for common modalities (CT, MR, RT Structure Sets,
    // dose, plans, etc.). It is seeded from the tags referenced in the DICOM write functions and common read tags.
    // A full DICOM dictionary can be imported via read_dictionary() if needed.
    static const DICOMDictionary dict = {
        // File Meta Information (group 0x0002).
        {{0x0002, 0x0000}, {"UL", "FileMetaInformationGroupLength"}},
        {{0x0002, 0x0001}, {"OB", "FileMetaInformationVersion"}},
        {{0x0002, 0x0002}, {"UI", "MediaStorageSOPClassUID"}},
        {{0x0002, 0x0003}, {"UI", "MediaStorageSOPInstanceUID"}},
        {{0x0002, 0x0010}, {"UI", "TransferSyntaxUID"}},
        {{0x0002, 0x0012}, {"UI", "ImplementationClassUID"}},
        {{0x0002, 0x0013}, {"SH", "ImplementationVersionName"}},
        {{0x0002, 0x0016}, {"AE", "SourceApplicationEntityTitle"}},

        // SOP Common, General Study, General Series, Patient (group 0x0008, 0x0010).
        {{0x0008, 0x0005}, {"CS", "SpecificCharacterSet"}},
        {{0x0008, 0x0008}, {"CS", "ImageType"}},
        {{0x0008, 0x0012}, {"DA", "InstanceCreationDate"}},
        {{0x0008, 0x0013}, {"TM", "InstanceCreationTime"}},
        {{0x0008, 0x0014}, {"UI", "InstanceCreatorUID"}},
        {{0x0008, 0x0016}, {"UI", "SOPClassUID"}},
        {{0x0008, 0x0018}, {"UI", "SOPInstanceUID"}},
        {{0x0008, 0x0020}, {"DA", "StudyDate"}},
        {{0x0008, 0x0021}, {"DA", "SeriesDate"}},
        {{0x0008, 0x0022}, {"DA", "AcquisitionDate"}},
        {{0x0008, 0x0023}, {"DA", "ContentDate"}},
        {{0x0008, 0x0030}, {"TM", "StudyTime"}},
        {{0x0008, 0x0031}, {"TM", "SeriesTime"}},
        {{0x0008, 0x0032}, {"TM", "AcquisitionTime"}},
        {{0x0008, 0x0033}, {"TM", "ContentTime"}},
        {{0x0008, 0x0050}, {"SH", "AccessionNumber"}},
        {{0x0008, 0x0060}, {"CS", "Modality"}},
        {{0x0008, 0x0070}, {"LO", "Manufacturer"}},
        {{0x0008, 0x0080}, {"LO", "InstitutionName"}},
        {{0x0008, 0x0090}, {"PN", "ReferringPhysicianName"}},
        {{0x0008, 0x0114}, {"UI", "CodingSchemeExternalID"}},
        {{0x0008, 0x1010}, {"SH", "StationName"}},
        {{0x0008, 0x1030}, {"LO", "StudyDescription"}},
        {{0x0008, 0x103E}, {"LO", "SeriesDescription"}},
        {{0x0008, 0x1040}, {"LO", "InstitutionalDepartmentName"}},
        {{0x0008, 0x1070}, {"PN", "OperatorsName"}},
        {{0x0008, 0x1090}, {"LO", "ManufacturerModelName"}},
        {{0x0008, 0x1150}, {"UI", "ReferencedSOPClassUID"}},
        {{0x0008, 0x1155}, {"UI", "ReferencedSOPInstanceUID"}},

        // Patient (group 0x0010).
        {{0x0010, 0x0010}, {"PN", "PatientName"}},
        {{0x0010, 0x0020}, {"LO", "PatientID"}},
        {{0x0010, 0x0030}, {"DA", "PatientBirthDate"}},
        {{0x0010, 0x0032}, {"TM", "PatientBirthTime"}},
        {{0x0010, 0x0040}, {"CS", "PatientSex"}},
        {{0x0010, 0x1010}, {"AS", "PatientAge"}},
        {{0x0010, 0x1020}, {"DS", "PatientSize"}},
        {{0x0010, 0x1030}, {"DS", "PatientWeight"}},

        // Acquisition parameters (group 0x0018).
        {{0x0018, 0x0015}, {"CS", "BodyPartExamined"}},
        {{0x0018, 0x0020}, {"CS", "ScanningSequence"}},
        {{0x0018, 0x0021}, {"CS", "SequenceVariant"}},
        {{0x0018, 0x0022}, {"CS", "ScanOptions"}},
        {{0x0018, 0x0023}, {"CS", "MRAcquisitionType"}},
        {{0x0018, 0x0024}, {"SH", "SequenceName"}},
        {{0x0018, 0x0025}, {"CS", "AngioFlag"}},
        {{0x0018, 0x0050}, {"DS", "SliceThickness"}},
        {{0x0018, 0x0060}, {"DS", "KVP"}},
        {{0x0018, 0x0080}, {"DS", "RepetitionTime"}},
        {{0x0018, 0x0081}, {"DS", "EchoTime"}},
        {{0x0018, 0x0082}, {"DS", "InversionTime"}},
        {{0x0018, 0x0083}, {"DS", "NumberOfAverages"}},
        {{0x0018, 0x0084}, {"DS", "ImagingFrequency"}},
        {{0x0018, 0x0085}, {"SH", "ImagedNucleus"}},
        {{0x0018, 0x0086}, {"IS", "EchoNumbers"}},
        {{0x0018, 0x0087}, {"DS", "MagneticFieldStrength"}},
        {{0x0018, 0x0088}, {"DS", "SpacingBetweenSlices"}},
        {{0x0018, 0x0089}, {"IS", "NumberOfPhaseEncodingSteps"}},
        {{0x0018, 0x0090}, {"DS", "DataCollectionDiameter"}},
        {{0x0018, 0x0091}, {"IS", "EchoTrainLength"}},
        {{0x0018, 0x0093}, {"DS", "PercentSampling"}},
        {{0x0018, 0x0094}, {"DS", "PercentPhaseFieldOfView"}},
        {{0x0018, 0x0095}, {"DS", "PixelBandwidth"}},
        {{0x0018, 0x1000}, {"LO", "DeviceSerialNumber"}},
        {{0x0018, 0x1020}, {"LO", "SoftwareVersions"}},
        {{0x0018, 0x1030}, {"LO", "ProtocolName"}},
        {{0x0018, 0x1060}, {"DS", "TriggerTime"}},
        {{0x0018, 0x5100}, {"CS", "PatientPosition"}},
        {{0x0018, 0x9073}, {"FD", "AcquisitionDuration"}},
        {{0x0018, 0x9075}, {"CS", "DiffusionDirectionality"}},
        {{0x0018, 0x9076}, {"SQ", "DiffusionGradientDirectionSequence"}},
        {{0x0018, 0x9087}, {"FD", "DiffusionBValue"}},
        {{0x0018, 0x9089}, {"FD", "DiffusionGradientOrientation"}},
        {{0x0018, 0x9117}, {"SQ", "MRDiffusionSequence"}},
        {{0x0018, 0x9147}, {"CS", "DiffusionAnisotropyType"}},
        {{0x0018, 0x9601}, {"SQ", "DiffusionBMatrixSequence"}},
        {{0x0018, 0x9602}, {"FD", "DiffusionBValueXX"}},
        {{0x0018, 0x9603}, {"FD", "DiffusionBValueXY"}},
        {{0x0018, 0x9604}, {"FD", "DiffusionBValueXZ"}},
        {{0x0018, 0x9605}, {"FD", "DiffusionBValueYY"}},
        {{0x0018, 0x9606}, {"FD", "DiffusionBValueYZ"}},
        {{0x0018, 0x9607}, {"FD", "DiffusionBValueZZ"}},

        // Frame of Reference, General Image (group 0x0020).
        {{0x0020, 0x000D}, {"UI", "StudyInstanceUID"}},
        {{0x0020, 0x000E}, {"UI", "SeriesInstanceUID"}},
        {{0x0020, 0x0010}, {"SH", "StudyID"}},
        {{0x0020, 0x0011}, {"IS", "SeriesNumber"}},
        {{0x0020, 0x0012}, {"IS", "AcquisitionNumber"}},
        {{0x0020, 0x0013}, {"IS", "InstanceNumber"}},
        {{0x0020, 0x0020}, {"CS", "PatientOrientation"}},
        {{0x0020, 0x0032}, {"DS", "ImagePositionPatient"}},
        {{0x0020, 0x0037}, {"DS", "ImageOrientationPatient"}},
        {{0x0020, 0x0052}, {"UI", "FrameOfReferenceUID"}},
        {{0x0020, 0x0060}, {"CS", "Laterality"}},
        {{0x0020, 0x1040}, {"LO", "PositionReferenceIndicator"}},
        {{0x0020, 0x1041}, {"DS", "SliceLocation"}},

        // Image Pixel (group 0x0028).
        {{0x0028, 0x0002}, {"US", "SamplesPerPixel"}},
        {{0x0028, 0x0004}, {"CS", "PhotometricInterpretation"}},
        {{0x0028, 0x0006}, {"US", "PlanarConfiguration"}},
        {{0x0028, 0x0008}, {"IS", "NumberOfFrames"}},
        {{0x0028, 0x0010}, {"US", "Rows"}},
        {{0x0028, 0x0011}, {"US", "Columns"}},
        {{0x0028, 0x0030}, {"DS", "PixelSpacing"}},
        {{0x0028, 0x0100}, {"US", "BitsAllocated"}},
        {{0x0028, 0x0101}, {"US", "BitsStored"}},
        {{0x0028, 0x0102}, {"US", "HighBit"}},
        {{0x0028, 0x0103}, {"US", "PixelRepresentation"}},
        {{0x0028, 0x1050}, {"DS", "WindowCenter"}},
        {{0x0028, 0x1051}, {"DS", "WindowWidth"}},
        {{0x0028, 0x1052}, {"DS", "RescaleIntercept"}},
        {{0x0028, 0x1053}, {"DS", "RescaleSlope"}},
        {{0x0028, 0x1054}, {"LO", "RescaleType"}},

        // RT Dose (group 0x3004).
        {{0x3004, 0x0002}, {"CS", "DoseType"}},
        {{0x3004, 0x0004}, {"CS", "DoseUnits"}},
        {{0x3004, 0x000A}, {"CS", "DoseSummationType"}},
        {{0x3004, 0x000C}, {"DS", "GridFrameOffsetVector"}},
        {{0x3004, 0x000E}, {"DS", "DoseGridScaling"}},

        // RT Structure Set (group 0x3006).
        {{0x3006, 0x0002}, {"SH", "StructureSetLabel"}},
        {{0x3006, 0x0004}, {"LO", "StructureSetName"}},
        {{0x3006, 0x0006}, {"ST", "StructureSetDescription"}},
        {{0x3006, 0x0008}, {"DA", "StructureSetDate"}},
        {{0x3006, 0x0009}, {"TM", "StructureSetTime"}},
        {{0x3006, 0x0010}, {"SQ", "ReferencedFrameOfReferenceSequence"}},
        {{0x3006, 0x0012}, {"SQ", "RTReferencedStudySequence"}},
        {{0x3006, 0x0014}, {"SQ", "RTReferencedSeriesSequence"}},
        {{0x3006, 0x0016}, {"SQ", "ContourImageSequence"}},
        {{0x3006, 0x0020}, {"SQ", "StructureSetROISequence"}},
        {{0x3006, 0x0022}, {"IS", "ROINumber"}},
        {{0x3006, 0x0024}, {"UI", "ReferencedFrameOfReferenceUID"}},
        {{0x3006, 0x0026}, {"LO", "ROIName"}},
        {{0x3006, 0x0028}, {"ST", "ROIDescription"}},
        {{0x3006, 0x002A}, {"IS", "ROIDisplayColor"}},
        {{0x3006, 0x0036}, {"CS", "ROIGenerationAlgorithm"}},
        {{0x3006, 0x0038}, {"LO", "ROIGenerationDescription"}},
        {{0x3006, 0x0039}, {"SQ", "ROIContourSequence"}},
        {{0x3006, 0x0040}, {"SQ", "ContourSequence"}},
        {{0x3006, 0x0042}, {"CS", "ContourGeometricType"}},
        {{0x3006, 0x0044}, {"DS", "ContourSlabThickness"}},
        {{0x3006, 0x0045}, {"DS", "ContourOffsetVector"}},
        {{0x3006, 0x0046}, {"IS", "NumberOfContourPoints"}},
        {{0x3006, 0x0048}, {"IS", "ContourNumber"}},
        {{0x3006, 0x0049}, {"SQ", "AttachedContours"}},
        {{0x3006, 0x0050}, {"DS", "ContourData"}},
        {{0x3006, 0x0080}, {"SQ", "ROIObservationsSequence"}},
        {{0x3006, 0x0082}, {"IS", "ObservationNumber"}},
        {{0x3006, 0x0084}, {"IS", "ReferencedROINumber"}},
        {{0x3006, 0x00A4}, {"CS", "RTROIInterpretedType"}},
        {{0x3006, 0x00A6}, {"PN", "ROIInterpreter"}},

        // RT Plan (group 0x300A).
        {{0x300A, 0x0002}, {"SH", "RTPlanLabel"}},
        {{0x300A, 0x0003}, {"LO", "RTPlanName"}},
        {{0x300A, 0x0006}, {"DA", "RTPlanDate"}},
        {{0x300A, 0x0007}, {"TM", "RTPlanTime"}},
        {{0x300A, 0x000C}, {"CS", "RTPlanGeometry"}},

        // Approval (group 0x300E).
        {{0x300E, 0x0002}, {"CS", "ApprovalStatus"}},
        {{0x300E, 0x0004}, {"DA", "ReviewDate"}},
        {{0x300E, 0x0005}, {"TM", "ReviewTime"}},
        {{0x300E, 0x0008}, {"PN", "ReviewerName"}},

        // Pixel Data.
        {{0x7FE0, 0x0010}, {"OW", "PixelData"}},
    };
    return dict;
}


DICOMDictionary read_dictionary(std::istream &is){
    DICOMDictionary dict;
    std::string line;
    while(std::getline(is, line)){
        // Skip comments and blank lines.
        if(line.empty()) continue;
        if(line.front() == '#') continue;

        // Expected format: "GGGG,EEEE VR Keyword"
        // At minimum: "GGGG,EEEE VR" which is 12 characters.
        if(line.size() < 12) continue;

        uint16_t group = 0;
        uint16_t element = 0;
        try{
            group   = static_cast<uint16_t>(std::stoul(line.substr(0, 4), nullptr, 16));
            element = static_cast<uint16_t>(std::stoul(line.substr(5, 4), nullptr, 16));
        }catch(const std::exception &){
            continue; // Skip malformed lines.
        }

        if(line.at(4) != ',') continue;
        if(line.at(9) != ' ') continue;

        std::string vr = line.substr(10, 2);
        std::string keyword;
        if(line.size() > 13){
            keyword = line.substr(13);
            // Trim leading/trailing whitespace.
            const auto first = keyword.find_first_not_of(" \t\r\n");
            const auto last  = keyword.find_last_not_of(" \t\r\n");
            keyword = (first == std::string::npos) ? "" : keyword.substr(first, last - first + 1);
        }

        dict[{group, element}] = {vr, keyword};
    }
    return dict;
}


void write_dictionary(std::ostream &os, const DICOMDictionary &dict){
    // Save and restore stream formatting state so subsequent writes to the same
    // stream are not affected by the hex/uppercase/fill settings used here.
    const auto prev_flags = os.flags();
    const auto prev_fill  = os.fill();

    os << "# DICOM Dictionary\n";
    os << "# Format: GGGG,EEEE VR Keyword\n";
    for(const auto &[key, entry] : dict){
        os << std::hex << std::setfill('0') << std::uppercase
           << std::setw(4) << key.first << ","
           << std::setw(4) << key.second
           << std::dec << " " << entry.VR;
        if(!entry.keyword.empty()){
            os << " " << entry.keyword;
        }
        os << "\n";
    }

    os.flags(prev_flags);
    os.fill(prev_fill);
}


std::string lookup_VR(uint16_t group, uint16_t element,
                      const std::vector<const DICOMDictionary*> &dicts){
    // Search additional dictionaries in reverse order (later entries take precedence).
    for(auto it = dicts.rbegin(); it != dicts.rend(); ++it){
        if(*it == nullptr) continue;
        auto entry = (*it)->find({group, element});
        if(entry != (*it)->end()) return entry->second.VR;
    }

    // Consult the built-in default dictionary.
    const auto &def = get_default_dictionary();
    auto entry = def.find({group, element});
    if(entry != def.end()) return entry->second.VR;

    // Special case: group length tags always have VR "UL".
    if(element == 0x0000) return "UL";

    return ""; // Unknown.
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


// Returns true if the VR uses a 4-byte length field with 2 reserved bytes in explicit encoding.
bool vr_has_extended_length(const std::string &vr){
    return (vr == "OB") || (vr == "OD") || (vr == "OF") || (vr == "OL")
        || (vr == "OW") || (vr == "SQ") || (vr == "UC") || (vr == "UN")
        || (vr == "UR") || (vr == "UT");
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
// For simple binary numeric VRs (US, SS, UL, SL, FL, FD, AT), this produces a string representation.
// For multi-valued binary VRs (OW, OF, OD, OB, UN), the raw bytes are left unchanged.
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
    }

    // OB, OW, OF, OD, UN, and any other binary VRs: leave raw.
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
          || vr == "FL" || vr == "FD" || vr == "AT"){
        node.val = decode_binary_value(raw, vr);
    }else{
        // OB, OW, OF, OD, UN, and others: store raw bytes.
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


std::string Node::value_str() const {
    if(vr_is_text(this->VR)){
        return this->val;
    }else if(this->VR == "US" || this->VR == "SS" || this->VR == "UL" || this->VR == "SL"
          || this->VR == "FL" || this->VR == "FD" || this->VR == "AT"){
        // Already decoded to string representation during reading.
        return this->val;
    }else if(this->VR == "SQ"){
        return "(sequence with "_s + std::to_string(this->children.size()) + " items)";
    }else{
        // Binary blob VRs (OB, OW, OF, OD, UN): return a summary.
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


bool Node::validate(Encoding enc) const {
    // Validate by attempting to emit the tree and checking for exceptions.
    try{
        std::ostringstream ss(std::ios_base::ate | std::ios_base::binary);
        this->emit_DICOM(ss, enc);
        return ss.good();
    }catch(const std::exception &){
        return false;
    }
}


} // namespace DCMA_DICOM

