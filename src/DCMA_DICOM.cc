// DCMA_DICOM.ccc - A part of DICOMautomaton 2019. Written by hal clark.
//
// This file contains routines for writing DICOM files.
//


#include <iostream>
#include <fstream>
#include <list>
//#include <utility>
#include <tuple>
#include <functional>
#include <utility>

#include <YgorMisc.h>
#include <YgorString.h>

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
    if(false){
    }else if(enc == Encoding::ILE){
        // Deal with sequences separately.
        if(false){
        }else if( node.VR == "SQ" ){
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
        if(false){
        }else if( node.VR == "SQ" ){
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
    if(false){
    }else if(is_root_node){
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
            if(16 < token.length()) throw std::invalid_argument("Code string is too long. Cannot continue.");
        }

        if(this->val.find_first_not_of(upper_case + number_digits + multiplicity + "_ ") != std::string::npos){
            throw std::invalid_argument("Invalid character found in code string. Cannot continue.");
        }
        cumulative_length += emit_DICOM_tag(os, enc, *this, this->val);

    }else if( this->VR == "SH" ){ //Short string.
        if(16 < this->val.length()) throw std::runtime_error("Short string is too long. Consider using a longer VR. Cannot continue.");
        cumulative_length += emit_DICOM_tag(os, enc, *this, this->val);

    }else if( this->VR == "LO" ){ //Long strings.
        if(64 < this->val.length()) throw std::runtime_error("Long string is too long. Consider using a longer VR. Cannot continue.");
        cumulative_length += emit_DICOM_tag(os, enc, *this, this->val);

    }else if( this->VR == "ST" ){ //Short text.
        if(1024 < this->val.length()) throw std::runtime_error("Short text is too long. Consider using a longer VR. Cannot continue.");
        cumulative_length += emit_DICOM_tag(os, enc, *this, this->val);

    }else if( this->VR == "LT" ){ //Long text.
        if(10240 < this->val.length()) throw std::runtime_error("Long text is too long. Consider using a longer VR. Cannot continue.");
        cumulative_length += emit_DICOM_tag(os, enc, *this, this->val);

    }else if( this->VR == "UT" ){ //Unlimited text.
        if(4'294'967'294 < this->val.length()) throw std::runtime_error("Unlimited text is too long. Cannot continue.");
        cumulative_length += emit_DICOM_tag(os, enc, *this, this->val);


    // Name types.
    }else if( this->VR == "AE" ){ //Application entity.
        if(16 < this->val.length()) throw std::runtime_error("Application entity is too long. Cannot continue.");
        cumulative_length += emit_DICOM_tag(os, enc, *this, this->val);

    }else if( this->VR == "PN" ){ //Person name.
        if(64 < this->val.length()) throw std::runtime_error("Person name is too long. Cannot continue.");
        cumulative_length += emit_DICOM_tag(os, enc, *this, this->val);

    }else if( this->VR == "UI" ){ //Unique Identifier (UID).
        if(64 < this->val.length()) throw std::runtime_error("UID is too long. Cannot continue.");
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

        if(8 < digits_only.length()) throw std::runtime_error("Date is too long. Cannot continue.");
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

        if(16 < digits_only.length()) throw std::runtime_error("Time is too long. Cannot continue.");
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

        if(26 < digits_only.length()) throw std::runtime_error("Date-time is too long. Cannot continue.");
        if(digits_only.find_first_not_of(number_digits + "+-.") != std::string::npos){
            throw std::invalid_argument("Invalid character found in date-time. Cannot continue.");
        }
        cumulative_length += emit_DICOM_tag(os, enc, *this, digits_only);

    }else if( this->VR == "AS" ){ //Age string.
        if(4 < this->val.length()) throw std::runtime_error("Age string is too long. Cannot continue.");
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
        if(65534 < this->val.length()) throw std::invalid_argument("Decimal string is too long. Cannot continue.");

        auto tokens = SplitStringToVector(this->val,'\\','d');
        for(const auto &token : tokens){
            // Maximum length per decimal number: 16 bytes.
            if(12 < token.length()) throw std::invalid_argument("Integer string element is too long. Cannot continue.");

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
        if(65534 < this->val.length()) throw std::invalid_argument("Decimal string is too long. Cannot continue.");

        auto tokens = SplitStringToVector(this->val,'\\','d');
        for(const auto &token : tokens){
            // Maximum length per decimal number: 16 bytes.
            if(16 < token.length()) throw std::invalid_argument("Decimal string element is too long. Cannot continue.");

            // Ensure that, if an element is present it parses as a number.
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
        if(tokens.size() != 2) throw std::runtime_error("Invalid number of integers for AT type tag; exactly 2 are needed.");
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

} // namespace DCMA_DICOM

