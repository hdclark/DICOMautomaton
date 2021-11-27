//Imebra_Shim.cc - DICOMautomaton 2012-2021. Written by hal clark.
//
// This file is supposed to 'shim' or wrap the Imebra library. It is used to abstract the Imebra library so that it can
// eventually be replaced with an alternative library, and also reduce the number of template instantiations generated
// in calling translation units.
//
// NOTE: Support for unicode is absent. All text is marchalled into std::strings.
//

#include <algorithm>      //Needed for std::sort.
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <deque>
#include <exception>
#include <optional>
#include <functional>
#include <iostream>
#include <limits>
#include <list>
#include <map>
#include <memory>         //Needed for std::unique_ptr.
#include <random>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>        //Needed for std::pair.
#include <vector>

#include "YgorContainers.h" //Needed for 'bimap' class.
#include "YgorMath.h"       //Needed for 'vec3' class.
#include "YgorMisc.h"       //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorString.h"     //Needed for Canonicalize_String2().
#include "YgorImages.h"
#include "YgorTAR.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#pragma GCC diagnostic ignored "-Wunused-but-set-parameter"
#include "imebra.h"

#pragma GCC diagnostic pop

#include "Imebra_Shim.h"

#include "DCMA_DICOM.h"
#include "Structs.h"
#include "Alignment_Rigid.h"
#include "Alignment_Field.h"

//----------------- Accessors ---------------------

// seq_group,seq_tag,seq_name or tag_group,tag_tag,tag_name.
struct path_node {
    uint16_t group   = 0; // The first number in common DICOM tag parlance.
    uint16_t tag     = 0; // The second number in common DICOM tag parlance.

    uint32_t order   = 0; // Rarely used in modern DICOM. Almost always going to be zero.
    uint32_t element = 0; // Used to enumerate items in lists.

};


static
std::vector<std::string>
extract_tag_as_string( const puntoexe::ptr<puntoexe::imebra::dataSet>& base_node_ptr,
                       path_node path ){

    // This routine extracts DICOM tags (multiple, if multiple exist at the level of the pointed-to data frame) as
    // strings. Multi-element data are a little trickier, especially with Imebra apparently unable to get the whole
    // element in the raw DICOM representation as string. We break these items into individual elements.
    //
    // Note: If there are multiple elements available after the given element, all following elements will be returned.
    //       (If only the first is needed, discard the following.)
    //
    // TODO: It would be better to handle some VR's directly rather than converting to string and back. For example
    //       doubles. There is currently a lot of unnecessary loss of precision, and I doubt NaN's and Inf's are handled
    //       correctly, since they are locale-dependent in several ways.
    //
    std::vector<std::string> out;

    if(base_node_ptr == nullptr) throw std::logic_error("Passed invalid base node. Cannot continue.");
    
    //Check if the tag is present in the file.
    const bool create_if_not_found = false;
    const auto ptr = base_node_ptr->getTag(path.group, path.order, path.tag, create_if_not_found);
    if(ptr == nullptr) return out;

    //Add the first element.
    const auto str = base_node_ptr->getString(path.group, path.order, path.tag, path.element);
    //const auto ctrim = CANONICALIZE::TRIM_ENDS;
    //const auto trimmed = Canonicalize_String2(str, ctrim);
    //if(!trimmed.empty()) out.emplace_back(trimmed);
    out.emplace_back(str);

    //Check if there are additional elements.
    try{
        const uint32_t buffer_id = 0;
        auto dh = base_node_ptr->getDataHandler(path.group, path.order, path.tag, buffer_id, create_if_not_found);
        //FUNCINFO("Encountered " << dh->getSize() << " elements");
        for(uint32_t i = 1 ; i < dh->getSize(); ++i){
            const auto str = base_node_ptr->getString(path.group, path.order, path.tag, path.element + i);
            //const auto trimmed = Canonicalize_String2(str, ctrim);
            //if(!trimmed.empty()){
            //    out.emplace_back(trimmed);
            //}else{
            //    return out;
            //}
            out.emplace_back(str);
        }
    }catch(const std::exception &){ }

    return out;
}

static
std::vector<std::string>
extract_seq_tag_as_string( const puntoexe::ptr<puntoexe::imebra::dataSet>& base_node_ptr,
                           std::deque<path_node> apath ){
                           //uint16_t seq_group, uint16_t seq_tag,
                           //uint16_t tag_group, uint16_t tag_tag ){

    // This routine extracts a DICOM tag that is part of a sequence. Only a single sequence item is consulted, but there
    // may be multiple elements (i.e., a 3-vector in which each coordinate is individually accessible as an element).
    //
    // This routine is not suitable if all items in a *sequence* need to be extracted, but is suitable if multiple
    // *elements* need to be extracted from a single tag. In practice, this routine works best for extracting
    // tags like 'ReferencedBeamNumber' which is expected to be a single item in the 'ReferencedBeamSequence'.
    //
    // TODO: It would be better to handle some VR's directly rather than converting to string and back. For example
    //       doubles. There is currently a lot of unnecessary loss of precision, and I doubt NaN's and Inf's are handled
    //       correctly, since they are locale-dependent in several ways.
    //
    std::vector<std::string> out;

    if(base_node_ptr == nullptr) throw std::logic_error("Passed invalid base node. Cannot continue.");
    if(apath.empty()) throw std::logic_error("Reached DICOM path terminus node -- verify element group/tag are valid.");

    // Extract info about the current node.
    const auto this_node = apath.front();
    apath.pop_front();

    //If this is a sequence, jump to the sequence node as the new base and recurse.
    if(!apath.empty()){

        //Check if the sequence can be found.
        auto seq_ptr = base_node_ptr->getSequenceItem(this_node.group, this_node.order,
                                                      this_node.tag,   this_node.element);
        if(seq_ptr != nullptr){
            auto res = extract_seq_tag_as_string(seq_ptr, apath);
            out.insert(std::end(out), std::begin(res), std::end(res));
        }

    //Otherwise, this is a leaf node.
    }else{
        //Check if the tag is present in the sequence.
        const bool create_if_not_found = false;
        const auto tag_ptr = base_node_ptr->getTag(this_node.group, this_node.order,
                                                   this_node.tag,
                                                   create_if_not_found);
        if(tag_ptr == nullptr) return out;

        //Add the first element outright.
        const auto str = base_node_ptr->getString(this_node.group, this_node.order,
                                                  this_node.tag,   this_node.element);
        //const auto ctrim = CANONICALIZE::TRIM_ENDS;
        //const auto trimmed = Canonicalize_String2(str, ctrim);
        //if(!trimmed.empty()) out.emplace_back(trimmed);
        out.emplace_back(str);

        //Check if there are additional elements.
        try{
            const uint32_t buffer_id = 0;
            auto dh = base_node_ptr->getDataHandler(this_node.group, this_node.order,
                                                    this_node.tag,
                                                    buffer_id, create_if_not_found);
            //FUNCINFO("Encountered " << dh->getSize() << " elements");

            for(uint32_t i = 1 ; i < dh->getSize(); ++i){
                const auto str = base_node_ptr->getString(this_node.group, this_node.order,
                                                          this_node.tag,   this_node.element + i);
                //const auto trimmed = Canonicalize_String2(str, ctrim);
                //if(!trimmed.empty()){
                //    out.emplace_back(trimmed);
                //}else{
                //    return out;
                //}
                out.emplace_back(str);
            }
        }catch(const std::exception &){ }
    }

    return out;
}


static
std::vector<std::string>
extract_seq_vec_tag_as_string( const puntoexe::ptr<puntoexe::imebra::dataSet>& base_node_ptr,
                               //Remaining path elements (relative to base_node_ptr).
                               std::deque<path_node> apath ){

    // This routine extracts DICOM tags, iterating over both sequence items and elements that match the given
    // hierarchial path. This routine can access tags with deeply-nested sequences in their path, including paths with
    // multiple items.
    //
    // TODO: It would be better to handle some VR's directly rather than converting to string and back. For example
    //       doubles. There is currently a lot of unnecessary loss of precision, and I doubt NaN's and Inf's are handled
    //       correctly, since they are locale-dependent in several ways.
    //
    std::vector<std::string> out;

    if(base_node_ptr == nullptr) throw std::logic_error("Passed invalid base node. Cannot continue.");
    if(apath.empty()) throw std::logic_error("Reached DICOM path terminus node -- verify element group/tag are valid.");

    // Extract info about the current node.
    const auto this_node = apath.front();
    apath.pop_front();

    //If this is a sequence, jump to the sequence node as the new base and recurse.
    if(!apath.empty()){

        //Cycle through all sequence items, breaking when no items remain.
        for(uint32_t i = 0; i < 100000; ++i){
            auto seq_ptr = base_node_ptr->getSequenceItem(this_node.group, this_node.order,
                                                          this_node.tag,   this_node.element + i);
            if(seq_ptr != nullptr){
                auto res = extract_seq_vec_tag_as_string(seq_ptr, apath);
                out.insert(std::end(out), std::begin(res), std::end(res));
            }else{
                break;
            }
        }

    //Otherwise, this is a leaf node.
    }else{
        //Check if the tag is present in the sequence.
        const bool create_if_not_found = false;
        const auto tag_ptr = base_node_ptr->getTag(this_node.group, this_node.order,
                                                   this_node.tag,
                                                   create_if_not_found);
        if(tag_ptr == nullptr) return out;

        //Add the first element outright.
        const auto str = base_node_ptr->getString(this_node.group, this_node.order,
                                                  this_node.tag,   this_node.element);
        //const auto ctrim = CANONICALIZE::TRIM_ENDS;
        //const auto trimmed = Canonicalize_String2(str, ctrim);
        //if(!trimmed.empty()) out.emplace_back(trimmed);
        out.emplace_back(str);

        //Check if there are additional elements.
        try{
            const uint32_t buffer_id = 0;
            auto dh = base_node_ptr->getDataHandler(this_node.group, this_node.order,
                                                    this_node.tag,
                                                    buffer_id, create_if_not_found);
            //FUNCINFO("Encountered " << dh->getSize() << " elements");

            for(uint32_t i = 1 ; i < dh->getSize(); ++i){
                const auto str = base_node_ptr->getString(this_node.group, this_node.order,
                                                          this_node.tag,   this_node.element + i);
                //const auto trimmed = Canonicalize_String2(str, ctrim);
                //if(!trimmed.empty()){
                //    out.emplace_back(trimmed);
                //}else{
                //    return out;
                //}
                out.emplace_back(str);
            }
        }catch(const std::exception &){ }
    }

    return out;
}

// This is an ad-hoc Siemens CSA2 header parser. I wasn't able to find any specifications, so I had to rely on
// sample files as examples. Seems to work for the samples I have, but may not be accurate!
//
// Note: to quickly extract and display CSA2 tag data:
//     dcmdump +L in.dcm +P 0029,1010 |
//       sed -e 's/.* OB //' |
//       sed -e 's/ .*//' |
//       tr '\\' '\n' |
//       while read c ; do
//           # Interpret the text as hex and emit hex bytes.
//           printf -- "\x${c}"
//       done |
//       xxd -c 32 - |
//       less
//
std::list<std::pair<std::string, std::string>>
parse_CSA2_binary(const std::vector<unsigned char> &header){
    const bool debug = false; // Whether to print extra information during parsing.

    std::list<std::pair<std::string, std::string>> out;
    std::stringstream ss( std::string( std::begin(header), std::end(header) ) );

    // Wrap stream reads to track if the entire header is consumed (for debugging).
    uint64_t total_bytes_read = 0;
    const auto read_bytes = [&](char *ptr, uint64_t N_bytes) -> bool {
        // Assuming we always have to read little-endian?
        // TODO

        const bool OK = !!ss.read(ptr, N_bytes);
        if(OK) total_bytes_read += N_bytes;
        return OK;
    };

    // There appears to be a lot of 'garbage' bytes trailing string names. Also, values encoded in the items seem to
    // alway have trailing whitespace padded to 8 characters plus a null byte. This subroutine trims the garbage.
    const auto crop_to_printable = [](const std::string &s){
        return std::string( std::begin(s),
                            std::find_if(std::begin(s), std::end(s),
                                         [](unsigned char c) -> bool {
                                             return !std::isprint(c) || std::isspace(c);
                                         }) );
    };
    const auto crop_to_null = [](const std::string &s){
        return std::string( std::begin(s),
                            std::find_if(std::begin(s), std::end(s),
                                         [](char c) -> bool {
                                             return (c == '\0');
                                         }) );
    };

    // Check if this is a Siemens CSAv2-formatted header.
    if( std::string shtl(4, '\0');
        !read_bytes(const_cast<char*>(shtl.data()), 4)
    ||  (shtl != "SV10")
    ||  !read_bytes(const_cast<char*>(shtl.data()), 4)
    ||  (shtl != "\4\3\2\1") ){
        FUNCWARN("Tag does not contain a CSA2 header. Verify tag is valid");
        return out;
    }

    // Read the number of tags present.
    uint32_t N_tags = 0;
    if( std::string shtl(4, '\0');
        !read_bytes(reinterpret_cast<char*>(&N_tags), sizeof(N_tags))
    ||  !isininc(1,N_tags,1'000)
    ||  !read_bytes(const_cast<char*>(shtl.data()), 4) ){
        FUNCWARN("Cannot read CSA2 tag count");
        return out;
    }

    if(debug) FUNCINFO("There are " << N_tags << " tags");
    for(uint32_t i = 0; i < N_tags; ++i){

        // Parse the next tag.
        std::string name(64, '\0');
        uint32_t vm = 0;
        std::string vr(4, '\0');
        uint32_t syngo_dt = 0;
        uint32_t N_items = 0;
        uint32_t layout = 0;
        uint32_t unused = 0;
        if( !read_bytes(const_cast<char*>(name.data()), 64)
        ||  ( name = crop_to_printable(name) ).empty()
        ||  !read_bytes(reinterpret_cast<char*>(&vm), sizeof(vm))
        ||  !read_bytes(const_cast<char*>(vr.data()), 3)
        ||  ( vr = crop_to_printable(vr) ).empty()
        ||  !read_bytes(reinterpret_cast<char*>(&unused), 1)
        ||  !read_bytes(reinterpret_cast<char*>(&syngo_dt), sizeof(syngo_dt))
        ||  !read_bytes(reinterpret_cast<char*>(&N_items), sizeof(N_items))
        ||  !read_bytes(reinterpret_cast<char*>(&layout), sizeof(layout))
        ||  !isininc(0,vm,1000)
        ||  !isininc(0,syngo_dt,1'000'000)
        ||  !isininc(0,N_items,1000)
        ||  ((layout != 77) && (layout != 205))
        ||  ((layout == 77) && (N_items == 0))
        ||  ((layout == 205) && (N_items != 0)) ){
            FUNCWARN("Failed to parse tag");
            if(debug) FUNCINFO("    Tag name = '" << name << "'");
            if(debug) FUNCINFO("    Tag vm = '" << vm << "'");
            if(debug) FUNCINFO("    Tag vr = '" << vr << "'");
            if(debug) FUNCINFO("    Tag syngo_dt = '" << syngo_dt << "'");
            if(debug) FUNCINFO("    Tag N_items = '" << N_items << "'");
            if(debug) FUNCINFO("    Tag unused = '" << unused << "'");
            if(debug) FUNCINFO(" Total bytes read: " << total_bytes_read);
            out.clear();
            return out;
        }
        if(debug) FUNCINFO("Parsed tag " << i << " OK");
        if(debug) FUNCINFO("    Tag name = '" << name << "'");
        if(debug) FUNCINFO("    Tag vm = '" << vm << "'");
        if(debug) FUNCINFO("    Tag vr = '" << vr << "'");
        if(debug) FUNCINFO("    Tag syngo_dt = '" << syngo_dt << "'");
        if(debug) FUNCINFO("    Tag N_items = '" << N_items << "'");
        if(debug) FUNCINFO("    Tag unused = '" << unused << "'");

        // Parse the items.
        std::list<std::string> items;
        for(uint32_t j = 0; j < N_items; ++j){
            int32_t l_item_length = 0;
            if( !read_bytes(reinterpret_cast<char*>(&l_item_length), sizeof(l_item_length))
            ||  !read_bytes(reinterpret_cast<char*>(&unused), sizeof(unused))
            ||  (l_item_length != unused)
            ||  !read_bytes(reinterpret_cast<char*>(&layout), sizeof(layout))
            ||  ((layout != 77) && (layout != 205))
            ||  !read_bytes(reinterpret_cast<char*>(&unused), sizeof(unused))
            ||  (l_item_length != unused)
            ||  !isininc(0,l_item_length,1'000'000) ){
                FUNCWARN("Encountered extremely long CSA2 item ('" << l_item_length << "'). Refusing to continue");
                out.clear();
                return out;
            }
            if(debug) FUNCINFO("Item length = " << l_item_length);
            std::string val(l_item_length, '\0');
            if( !read_bytes(const_cast<char*>(val.data()), l_item_length) ){
                FUNCWARN("Unable to read CSA2 item. Refusing to continue");
                out.clear();
                return out;
            }
            if( (vm == 1)
            &&  (    (vr == "IS")
                 ||  (vr == "DS")
                 ||  (vr == "FL")
                 ||  (vr == "UL")
                 ||  (vr == "US")
                 ||  (vr == "LO")
                 ||  (vr == "FD")
                 ||  (vr == "SH") ) ){
                val = crop_to_printable(val);
            }else{ // All 'xT' VR's, which can be multi-line,
                   // 'UN', which in some cases contains embedded XML.
                val = crop_to_null(val);
            }
            if(!val.empty()){
                if(debug) FUNCINFO("    Item " << j << " of " << N_items << " has value = '" << val << "'");
                items.emplace_back(val);
            }

            // Entries seem to be aligned to 4-byte word boundaries, similar to DICOM padding.
            // Consume bytes as needed to achieve proper alignment.
            const auto boundary_reset = (4 - l_item_length % 4) % 4;
            if(!read_bytes(reinterpret_cast<char*>(&unused), boundary_reset )){
                FUNCWARN("Unable to reset stream to word boundary. Refusing to continue");
                out.clear();
                return out;
            }
        }

        // Record items.
        if(!items.empty()){
            out.emplace_back();
            out.back().first = name;
            bool first = true;
            for(const auto &s : items){
                // Note: data with multiplicity != uses backslash, so use something else to delineate here...
                out.back().second += std::string( (first) ? "" : R"***(,)***" ) + s;
                first = false;
            }
        }

    }
    return out;
}

// This routine writes an OB-type DICOM tag to the provided data set.
static
void
ds_OB_insert(puntoexe::ptr<puntoexe::imebra::dataSet> &ds, 
             path_node pn,
//             uint16_t group, uint16_t tag, 
             std::string i_val){
//    const uint16_t order = 0;
    
    //For OB type, we simply copy the string's buffer as-is. 
    const auto d_t = ds->getDefaultDataType(pn.group, pn.tag);

    if( d_t == "OB" ){
        auto tag_ptr = ds->getTag(pn.group, pn.order, pn.tag, true);
        //const auto next_buff = tag_ptr->getBuffersCount() - 1;
        //auto rdh_ptr = tag_ptr->getDataHandlerRaw( next_buff, true, d_t );
        auto rdh_ptr = tag_ptr->getDataHandlerRaw( 0, true, d_t );
        rdh_ptr->copyFromMemory(reinterpret_cast<const uint8_t *>(i_val.data()),
                                static_cast<uint32_t>(i_val.size()));
    }else{
        throw std::runtime_error("A non-OB VR type was passed to the OB VR type writer.");
    }
    return;
}

// This routine writes a given tag into the given DICOM data set in the default encoding style for the tag.
static
void
ds_insert(puntoexe::ptr<puntoexe::imebra::dataSet> &ds,
          path_node pn,
//          uint16_t group, uint16_t tag,
          const std::string& i_val){
//    const uint16_t order = 0;
//    uint32_t element = 0;

//    pn.order = 0;
//    pn.element = 0;

    //Search for '\' characters. If present, split the string up and register each token separately.
    auto tokens = SplitStringToVector(i_val, '\\', 'd');
    for(auto &val : tokens){
        //Attempt to convert to the default DICOM data type.
        const auto d_t = ds->getDefaultDataType(pn.group, pn.tag);

        //Types not requiring conversion from a string.
        if( ( d_t == "AE") || ( d_t == "AS") || ( d_t == "AT") ||
            ( d_t == "CS") || ( d_t == "DS") || ( d_t == "DT") ||
            ( d_t == "LO") || ( d_t == "LT") || ( d_t == "OW") ||
            ( d_t == "PN") || ( d_t == "SH") || ( d_t == "ST") ||
            ( d_t == "UT")   ){
                ds->setString(pn.group, pn.order, pn.tag, pn.element++, val, d_t);

        //UIDs.
        }else if( d_t == "UI" ){   //UIDs.
            //UIDs were being altered in funny ways sometimes. Write raw bytes instead.
            auto tag_ptr = ds->getTag(pn.group, pn.order, pn.tag, true);
            auto rdh_ptr = tag_ptr->getDataHandlerRaw( 0, true, d_t );
            rdh_ptr->copyFromMemory(reinterpret_cast<const uint8_t *>(val.data()),
                                    static_cast<uint32_t>(val.size()));

        //Time.
        }else if( ( d_t == "TM" ) ||   //Time.
                  ( d_t == "DA" )   ){ //Date.
            //Strip away colons. Also strip away everything after the leading non-numeric char.
            std::string digits_only(val);
            digits_only = PurgeCharsFromString(digits_only,":-");
            auto avec = SplitStringToVector(digits_only,'.','d');
            avec.resize(1);
            digits_only = Lineate_Vector(avec, "");

            //The 'easy' way resulted in non-printable garbage fouling the times. Have to write raw ASCII chars manually...
            auto tag_ptr = ds->getTag(pn.group, pn.order, pn.tag, true);
            auto rdh_ptr = tag_ptr->getDataHandlerRaw( 0, true, d_t );
            rdh_ptr->copyFromMemory(reinterpret_cast<const uint8_t *>(digits_only.data()),
                                    static_cast<uint32_t>(digits_only.size()));

        //Binary types.
        }else if( d_t == "OB" ){
            return ds_OB_insert(ds, pn, i_val);

        //Numeric types.
        }else if(
            ( d_t == "FL") ||   //Floating-point.
            ( d_t == "FD") ||   //Floating-point double.
            ( d_t == "OF") ||   //"Other" floating-point.
            ( d_t == "OD")   ){ //"Other" floating-point double.
                ds->setString(pn.group, pn.order, pn.tag, pn.element++, val, "DS"); //Try keep it as a string.
        }else if( ( d_t == "SL" ) ||   //Signed long int (32bit).
                  ( d_t == "SS" )   ){ //Signed short int (16bit).
            const auto conv = static_cast<int32_t>(std::stol(val));
            ds->setSignedLong(pn.group, pn.order, pn.tag, pn.element++, conv, d_t);

        }else if( ( d_t == "UL" ) ||   //Unsigned long int (32bit).
                  ( d_t == "US" )   ){ //Unsigned short int (16bit).
            const auto conv = static_cast<uint32_t>(std::stoul(val));
            ds->setUnsignedLong(pn.group, pn.order, pn.tag, pn.element++, conv, d_t);

        }else if( d_t == "IS" ){ //Integer string.
                ds->setString(pn.group, pn.order, pn.tag, pn.element++, val, "IS");

        //Types we cannot process because they are special (e.g., sequences) or don't currently support.
        }else if( d_t == "SQ"){ //Sequence.
            throw std::runtime_error("Unable to write VR type SQ (sequence) with this routine.");
        }else if( d_t == "UN"){ //Unknown.
            throw std::runtime_error("Unable to write VR type UN (unknown) with this routine.");
        }else{
            throw std::runtime_error("Unknown VR type. Cannot write to tag.");
        }
    }

    return;
}

// This routine writes a sequence tag into the given DICOM data set in the default encoding style for the tag.
//
// Note that an existing sequence will be used if present. If a new sequence should be used, create a new data set,
// insert the sequence, and then join with the top-level data set (or recursively, as needed).
static
void
ds_seq_insert(puntoexe::ptr<puntoexe::imebra::dataSet> &ds, 
              path_node seq_pn,
              path_node tag_pn,
//              uint16_t seq_group, uint16_t seq_tag, 
//              uint16_t tag_group, uint16_t tag_tag,
              std::string tag_val){
//    const uint32_t first_order = 0; // Always zero for modern DICOM files.
    seq_pn.order = 0; // Always zero for modern DICOM files.

    //Get a reference to an existing sequence item, or create one if needed.
    const bool create_if_not_found = true;
    auto tag_ptr = ds->getTag(seq_pn.group, seq_pn.order, seq_pn.tag, create_if_not_found);
    if(tag_ptr == nullptr) return;

    //Prefer to append to an existing dataSet rather than creating an additional one.
    auto lds = tag_ptr->getDataSet(0);
    if( lds == nullptr ) lds = puntoexe::ptr<puntoexe::imebra::dataSet>(new puntoexe::imebra::dataSet);
    ds_insert(lds, tag_pn, std::move(tag_val));
    tag_ptr->setDataSet( 0, lds );

    return;
}


/*
// This routine writes a sequence tag into the given DICOM data set in the default encoding style for the tag.
//
// Note that the element number will be honoured and treated as a sequence item number for each sequence node.
// If sequence numbers are not continuous, then nullptr sequence items will be allocated. This could be confusing
// to work with afterward.
static
void
ds_seq_insert(puntoexe::ptr<puntoexe::imebra::dataSet> &ds, 
              std::deque<path_node> apath,
              std::string tag_val){
    //seq_pn.order = 0; // Always zero for modern DICOM files.

    if(ds == nullptr) throw std::logic_error("Passed invalid base node. Cannot continue.");
    if(apath.empty()) throw std::logic_error("Reached DICOM path terminus node -- verify element group/tag are valid.");

    // Extract info about the current node.
    const auto this_node = apath.front();
    apath.pop_front();

FUNCINFO("ds_seq_insert() to " << std::hex << this_node.group   << ", "
                               << std::hex << this_node.order   << ", "
                               << std::hex << this_node.tag     << ", "
                               << std::hex << this_node.element << ", "
                               << "'" << tag_val << "'");

//    //If this is a sequence, jump to the sequence node as the new base and recurse.
//    if(!apath.empty()){
//FUNCINFO("    This node is NOT a leaf node. Creating a sequence tag");
//
//        // Request the sequence item with the item number == the element number.
//        // If it does not exist it will not be created automatically.
//        auto seq_ptr = ds->getSequenceItem(this_node.group, this_node.order,
//                                           this_node.tag,   this_node.element);
//        if(seq_ptr == nullptr){
//FUNCINFO("        The sequence tag does NOT yet exist. Creating a new sequence tag and data set");
//            // The sequence tag does not yet exist, so we need to create it.
//            const bool create_if_not_found = true;
//            auto tag_ptr = ds->getTag(this_node.group, this_node.order, 
//                                      this_node.tag, create_if_not_found);
//            if(tag_ptr == nullptr) throw std::logic_error("Failed to create a sequence tag");
//
//            // Associate a data set with the sequence tag, creating a numbered 'item' in the sequence and permitting the
//            // leaf node to be written.
//            //
//            // Note: If data sets with smaller element numbers do not already exist, space for them will be allocated.
//            //       Avoid supplying excessively large element numbers.
//            //
//            // Note: See imebra/library/imebra/src/dataSet.cpp, line 893 (getSequenceItem()) for more info
//            //
//            auto lds = puntoexe::ptr<puntoexe::imebra::dataSet>(new puntoexe::imebra::dataSet);
//
//            // Ensure the character set matches that of the (soon-to-be) parent node.
//            //
//            // Note: If the character sets do not correctly match Imebra will throw 'Different default charsets'.
//            {
//                puntoexe::imebra::charsetsList::tCharsetsList csl;
//                ds->getCharsetsList(&csl);
//                lds->setCharsetsList(&csl);
//            }
//
//            // Recurse, writing to the pristine data set.
//            ds_seq_insert(lds, apath, tag_val);
//
//            // Attach the data set to the sequence item.
//            //tag_ptr->appendDataSet( lds );
//            tag_ptr->setDataSet( this_node.element, lds );
//            //tag_ptr->setDataSet( 0, lds );
//            //seq_ptr = lds;
//        }else{
//
//FUNCINFO("        The sequence tag DOES exist. Reusing it directly");
//            ds_seq_insert(seq_ptr, apath, tag_val);
//        }
//
//    //Otherwise, this is a leaf node. Insert the tag into the current sequence.
//    }else{
//FUNCINFO("    This node is a leaf node. Creating a leaf tag");
//        ds_insert(ds, this_node, tag_val);
//    }

    //If this is a sequence, jump to the sequence node as the new base and recurse.
    if(!apath.empty()){
FUNCINFO("    This node is NOT a leaf node. Creating a sequence tag");

        const bool create_if_not_found = true;
        auto tag_ptr = ds->getTag(this_node.group, this_node.order, 
                                  this_node.tag, create_if_not_found);
        if(tag_ptr == nullptr) throw std::logic_error("Failed to locate or create a sequence tag");

        auto lds = tag_ptr->getDataSet(this_node.element);
        if(lds == nullptr){
            // Associate a data set with the sequence tag, creating a numbered 'item' in the sequence and permitting the
            // leaf node to be written.
            //
            // Note: If data sets with smaller element numbers do not already exist, space for them will be allocated.
            //       Avoid supplying excessively large element numbers.
            //
            // Note: See imebra/library/imebra/src/dataSet.cpp, line 893 (getSequenceItem()) for more info
            //
            lds = puntoexe::ptr<puntoexe::imebra::dataSet>(new puntoexe::imebra::dataSet);

            // Ensure the character set matches that of the (soon-to-be) parent node.
            //
            // Note: If the character sets do not correctly match Imebra will throw 'Different default charsets'.
            //{
            //    puntoexe::imebra::charsetsList::tCharsetsList csl;
            //    ds->getCharsetsList(&csl);
            //    lds->setCharsetsList(&csl);
            //}
            tag_ptr->setDataSet( this_node.element, lds );
        }

        ds_seq_insert(lds, apath, tag_val);

    //Otherwise, this is a leaf node. Insert the tag into the current sequence.
    }else{
FUNCINFO("    This node is a leaf node. Creating a leaf tag");
        ds_insert(ds, this_node, tag_val);
    }
    return;
}
*/



//------------------ General ----------------------
//This is used to grab the contents of a single DICOM tag. It can be used for whatever. Some routines
// use it to grab specific things. Each invocation involves disk access and file parsing.
//
//NOTE: On error, the output will be an empty string.
std::string get_tag_as_string(const std::filesystem::path &filename, size_t U, size_t L){
    using namespace puntoexe;
    ptr<puntoexe::stream> readStream(new puntoexe::stream);
    readStream->openFile(filename.c_str(), std::ios::in);
    if(readStream == nullptr) return std::string("");

    ptr<puntoexe::streamReader> reader(new puntoexe::streamReader(readStream));
    ptr<imebra::dataSet> TopDataSet = imebra::codecs::codecFactory::getCodecFactory()->load(reader);
    if(TopDataSet == nullptr) return std::string("");
    return TopDataSet->getString(U, 0, L, 0);
}

std::string get_modality(const std::filesystem::path &filename){
    //Should exist in each DICOM file.
    return get_tag_as_string(filename,0x0008,0x0060);
}

std::string get_patient_ID(const std::filesystem::path &filename){
    //Should exist in each DICOM file.
    return get_tag_as_string(filename,0x0010,0x0020);
}

//Mass top-level tag enumeration, for ingress into database.
//
//NOTE: May not be complete. Add additional tags as needed!
std::map<std::string,std::string> get_metadata_top_level_tags(const std::filesystem::path &filename){
    std::map<std::string,std::string> out;
    const auto ctrim = CANONICALIZE::TRIM_ENDS;

    //Attempt to parse the DICOM file and harvest the elements of interest. We are only interested in
    // top-level elements specifying metadata (i.e., not pixel data) and will not need to recurse into 
    // any DICOM sequences.
    puntoexe::ptr<puntoexe::stream> readStream(new puntoexe::stream);
    readStream->openFile(filename.c_str(), std::ios::in);
    if(readStream == nullptr){
        FUNCWARN("Could not parse file '" << filename << "'. Is it valid DICOM? Cannot continue");
        return out;
    }

    puntoexe::ptr<puntoexe::streamReader> reader(new puntoexe::streamReader(readStream));
    puntoexe::ptr<puntoexe::imebra::dataSet> tds = puntoexe::imebra::codecs::codecFactory::getCodecFactory()->load(reader);

    //We pull out all the data we need as strings. For single element strings, the SQL engine can directly perform
    // the type casting. The benefit of this is twofold: (1) the SQL engine hides the checking code, simplifying
    // this implementation, and (2) the SQL engine can appropriately handle SQL NULLs without extra conditionals
    // and workarounds here.
    //
    // Multi-element data are a little trickier, especially with Imebra apparently unable to get the whole element
    // in the raw DICOM representation as string. We break these items into individual elements and then delimit 
    // such that they appear just as they would if read from the DICOM file (with '\'s separating) as needed.
    //
    // To ensure that there are no duplicated tags (at this single level) we exclusively use 'emplace'.
    //
    // TODO: Properly handle missing strings. In many cases, Imebra will feel obligated to return a NON-EMPTY string
    //       if a tag is missing. For example, the date '0000-00-00' is returned instead of an empty string for date
    //       fields. Instead of handling these issues specifically, use the Imebra member function
    //           ptr< data > puntoexe::imebra::dataSet::getTag(imbxUint16 groupId, imbxUint16 order,
    //                                                         imbxUint16 tagId, bool bCreate = false)
    //       to check if the tag is present before asking for a string. Then, of course, we need to check later if
    //       the item is present. If using a map, I think the default empty string will suffice to communicate this
    //       info.
    //
    // TODO: It would be better to handle some VR's directly rather than converting to string and back. For example
    //       doubles. There is currently a lot of unnecessary loss of precision, and I doubt NaN's and Inf's are handled
    //       correctly, since they are locale-dependent in several ways.
    //
    
    auto insert_as_string_if_nonempty = [&out,&ctrim,&tds](uint16_t group, uint16_t tag,
                                                           std::string name ) -> void {
        const uint32_t first_order = 0; // Always zero for modern DICOM files.
        const uint32_t first_element = 0; // Usually zero, but several tags can store multiple elements.

        //Check if the tag has already been found.
        if(out.count(name) != 0) return;

        //Check if the tag is present in the file.
        const bool create_if_not_found = false;
        const auto ptr = tds->getTag(group, first_order, tag, create_if_not_found);
        if(ptr == nullptr) return;

        //Add the first element.
        const auto str = tds->getString(group, first_order, tag, first_element);
        const auto trimmed = Canonicalize_String2(str, ctrim);
        if(!trimmed.empty()) out.emplace(name, trimmed);

        //Check if there are additional elements.
        try{
            const uint32_t buffer_id = 0;
            auto dh = tds->getDataHandler(group, first_order, tag, buffer_id, create_if_not_found);
            for(uint32_t i = 1 ; i < dh->getSize(); ++i){
                const auto str = tds->getString(group, first_order, tag, first_element + i);
                const auto trimmed = Canonicalize_String2(str, ctrim);
                if(!trimmed.empty()){
                    out[name] += R"***(\)***"_s + trimmed;
                }else{
                    return;
                }
            }
        }catch(const std::exception &){ }

        return;
    };

    auto insert_seq_tag_as_string_if_nonempty = [&out,&ctrim,&tds](uint16_t seq_group, uint16_t seq_tag, std::string seq_name,
                                                                   uint16_t tag_group, uint16_t tag_tag, std::string tag_name) -> void {
        const uint32_t first_order = 0; // Always zero for modern DICOM files.
        const uint32_t first_element = 0; // Usually zero, but several tags can store multiple elements.
        const auto full_name = seq_name + R"***(/)***"_s + tag_name;

        //Check if the tag has already been found.
        if(out.count(full_name) != 0) return;

        //Check if the sequence can be found.
        const auto seq_ptr = tds->getSequenceItem(seq_group, first_order, seq_tag, first_element);
        if(seq_ptr == nullptr) return;

        //Check if the tag is present in the sequence.
        const bool create_if_not_found = false;
        const auto tag_ptr = seq_ptr->getTag(tag_group, first_order, tag_tag, create_if_not_found);
        if(tag_ptr == nullptr) return;

        //Add the first element.
        const auto str = seq_ptr->getString(tag_group, first_order, tag_tag, first_element);
        const auto trimmed = Canonicalize_String2(str, ctrim);
        if(!trimmed.empty()) out.emplace(full_name, trimmed);

        //Check if there are additional elements.
        try{
            const uint32_t buffer_id = 0;
            auto dh = seq_ptr->getDataHandler(tag_group, first_order, tag_tag, buffer_id, create_if_not_found);
            for(uint32_t i = 1 ; i < dh->getSize(); ++i){
                const auto str = seq_ptr->getString(tag_group, first_order, tag_tag, first_element + i);
                const auto trimmed = Canonicalize_String2(str, ctrim);
                if(!trimmed.empty()){
                    out[full_name] += R"***(\)***"_s + trimmed;
                }else{
                    return;
                }
            }
        }catch(const std::exception &){ }

        return;
    };
    
    auto extract_as_binary = [&out,&tds](uint16_t group, uint16_t tag) -> std::vector<uint8_t> {
        std::vector<uint8_t> out;

        const uint32_t first_order = 0; // Always zero for modern DICOM files.
        const uint32_t first_element = 0; // Usually zero, but several tags can store multiple elements.

        //Check if the tag is present in the file.
        const bool create_if_not_found = false;
        const auto ptr = tds->getTag(group, first_order, tag, create_if_not_found);
        if(ptr == nullptr) return out;

        // Copy the data.
        try{
            auto rdh_ptr = ptr->getDataHandlerRaw( 0, create_if_not_found, "OB" );
            const auto N_bytes = rdh_ptr->getSize(); // Gives # of elements, but each element is one byte (int8_t).
            if(!isininc(0, N_bytes, 1'000'000'000)){
                FUNCWARN("Excessively large memory requested. Refusing to extract as binary element");
                return out;
            }
            out.resize(N_bytes, static_cast<uint8_t>(0x0));
            rdh_ptr->copyToMemory(reinterpret_cast<uint8_t *>(out.data()),
                                  static_cast<uint32_t>(out.size()));
        }catch(const std::exception &){ }

        return out;
    };

    // seq_group,seq_tag,seq_name or tag_group,tag_tag,tag_name.
    struct path_node {
        uint16_t group   = 0; // The first number in common DICOM tag parlance.
        uint16_t tag     = 0; // The second number in common DICOM tag parlance.

        std::string name = "";

        uint32_t order   = 0; // Rarely used in modern DICOM. Almost always going to be zero.
        uint32_t element = 0; // Used to enumerate items in lists.

    };

    std::function< void (std::deque<path_node>,
                         puntoexe::ptr<puntoexe::imebra::dataSet>,
                         std::string) > insert_seq_vec_tag_as_string_if_nonempty_proto;

    insert_seq_vec_tag_as_string_if_nonempty_proto = [&out,&ctrim,&tds,&insert_seq_vec_tag_as_string_if_nonempty_proto](
            std::deque<path_node> apath, //Remaining path elements (relative to base_node).
            puntoexe::ptr<puntoexe::imebra::dataSet> base_node = nullptr, // The current spot in the DICOM path hierarchy.
            std::string base_fullpath = "" // The current full path name for the base node.
         ) -> void {

        if(base_node == nullptr) base_node = tds; // Default to DICOM file top level.

        // This routine can access tags with deeply-nested sequences in their path, including paths with multiple items.

        if(apath.empty()) throw std::logic_error("Reached DICOM path terminus node.");

        // Extract info about the current node.
        const auto this_node = apath.front();
        apath.pop_front();

        //Extract the full path name of the tag.
        if(!base_fullpath.empty()) base_fullpath += R"***(/)***"_s;
        base_fullpath += this_node.name;

        //If this is a sequence, jump to the sequence node as the new base and recurse.
        if(!apath.empty()){

            //Check if there is a second element.
            auto next_seq_ptr = base_node->getSequenceItem(this_node.group, this_node.order,
                                                           this_node.tag,   this_node.element + 1);
            if(next_seq_ptr != nullptr){

                //Cycle through all elements, suffixing with the element number.
                for(uint32_t i = 0; i < 10000 ; ++i){
                    const auto this_fullpath = base_fullpath + "#" + std::to_string(i);
                    auto seq_ptr = base_node->getSequenceItem(this_node.group, this_node.order,
                                                              this_node.tag,   this_node.element + i);
                    if(seq_ptr != nullptr){
                        insert_seq_vec_tag_as_string_if_nonempty_proto(apath, seq_ptr, this_fullpath);
                    }else{
                        break;
                    }
                }
            }else{
                //This sequence does not appear to be part of a list. So just jump to it.
                auto seq_ptr = base_node->getSequenceItem(this_node.group, this_node.order,
                                                          this_node.tag,   this_node.element);
                insert_seq_vec_tag_as_string_if_nonempty_proto(apath, seq_ptr, base_fullpath);
            }

        //Otherwise, this is a leaf node.
        }else{
            //Check if the tag has already been found.
            if(out.count(base_fullpath) != 0) return;
            
            //Check if the tag is present in the sequence.
            const bool create_if_not_found = false;
            const auto tag_ptr = base_node->getTag(this_node.group, this_node.order,
                                                   this_node.tag,
                                                   create_if_not_found);
            if(tag_ptr == nullptr) return;

            //Add the first element.
            const auto str = base_node->getString(this_node.group, this_node.order,
                                                  this_node.tag,   this_node.element);
            const auto trimmed = Canonicalize_String2(str, ctrim);
            if(!trimmed.empty()) out.emplace(base_fullpath, trimmed);

            //Check if there are additional elements.
            for(uint32_t i = 1 ; i < 10000 ; ++i){
                try{
                    const auto str = base_node->getString(this_node.group, this_node.order,
                                                          this_node.tag,   this_node.element + i);
                    const auto trimmed = Canonicalize_String2(str, ctrim);
                    if(!trimmed.empty()){
                        out[base_fullpath] += R"***(\)***"_s + trimmed;
                    }else{
                        return;
                    }
                }catch(const std::exception &){
                    return;
                }
            }
        }
        return;
    };

    auto insert_seq_vec_tag_as_string_if_nonempty = std::bind(insert_seq_vec_tag_as_string_if_nonempty_proto,
                                                              std::placeholders::_1,
                                                              tds, "");
    
    //Misc.
    out["Filename"] = filename.string();

    //SOP Common Module.
    insert_as_string_if_nonempty(0x0008, 0x0016, "SOPClassUID");
    insert_as_string_if_nonempty(0x0008, 0x0018, "SOPInstanceUID");
    insert_as_string_if_nonempty(0x0008, 0x0005, "SpecificCharacterSet");
    insert_as_string_if_nonempty(0x0008, 0x0012, "InstanceCreationDate");
    insert_as_string_if_nonempty(0x0008, 0x0013, "InstanceCreationTime");
    insert_as_string_if_nonempty(0x0008, 0x0014, "InstanceCreatorUID");
    insert_as_string_if_nonempty(0x0008, 0x0114, "CodingSchemeExternalUID");
    insert_as_string_if_nonempty(0x0020, 0x0013, "InstanceNumber");

    //Patient Module.
    insert_as_string_if_nonempty(0x0010, 0x0010, "PatientsName");
    insert_as_string_if_nonempty(0x0010, 0x0020, "PatientID");
    insert_as_string_if_nonempty(0x0010, 0x0030, "PatientsBirthDate");
    insert_as_string_if_nonempty(0x0010, 0x0040, "PatientsGender");

    //General Study Module.
    insert_as_string_if_nonempty(0x0020, 0x000D, "StudyInstanceUID");
    insert_as_string_if_nonempty(0x0008, 0x0020, "StudyDate");
    insert_as_string_if_nonempty(0x0008, 0x0030, "StudyTime");
    insert_as_string_if_nonempty(0x0008, 0x0090, "ReferringPhysiciansName");
    insert_as_string_if_nonempty(0x0020, 0x0010, "StudyID"); // i.e., "Course"
    insert_as_string_if_nonempty(0x0008, 0x0050, "AccessionNumber");
    insert_as_string_if_nonempty(0x0008, 0x1030, "StudyDescription");


    //General Series Module.
    insert_as_string_if_nonempty(0x0008, 0x0060, "Modality");
    insert_as_string_if_nonempty(0x0020, 0x000E, "SeriesInstanceUID");
    insert_as_string_if_nonempty(0x0020, 0x0011, "SeriesNumber");
    insert_as_string_if_nonempty(0x0008, 0x0021, "SeriesDate");
    insert_as_string_if_nonempty(0x0008, 0x0031, "SeriesTime");
    insert_as_string_if_nonempty(0x0008, 0x103E, "SeriesDescription");
    insert_as_string_if_nonempty(0x0018, 0x0015, "BodyPartExamined");
    insert_as_string_if_nonempty(0x0018, 0x5100, "PatientPosition");
    insert_as_string_if_nonempty(0x0040, 0x1001, "RequestedProcedureID");
    insert_as_string_if_nonempty(0x0040, 0x0009, "ScheduledProcedureStepID");
    insert_as_string_if_nonempty(0x0008, 0x1070, "OperatorsName");

    //Patient Study Module.
    insert_as_string_if_nonempty(0x0010, 0x1010, "PatientsAge");
    insert_as_string_if_nonempty(0x0010, 0x1020, "PatientsSize"); // in m, so actually patient height. :(
    insert_as_string_if_nonempty(0x0010, 0x1030, "PatientsWeight"); // in kg, so actually patient mass. :(

    //Frame of Reference Module.
    insert_as_string_if_nonempty(0x0020, 0x0052, "FrameOfReferenceUID");
    insert_as_string_if_nonempty(0x0020, 0x1040, "PositionReferenceIndicator");
    insert_seq_tag_as_string_if_nonempty( 0x3006, 0x0010, "ReferencedFrameOfReferenceSequence",
                                          0x0020, 0x0052, "FrameOfReferenceUID");
    if(out.count("ReferencedFrameOfReferenceSequence/FrameOfReferenceUID") != 0){
        // Allow a newer-style FrameOfReferenceUID tag to supercede an earlier-style tag, if present.
        //
        // Note that each contour can have a separate FrameOfReferenceUID. This simple mapping won't work in those cases.
        out["FrameOfReferenceUID"] = out["ReferencedFrameOfReferenceSequence/FrameOfReferenceUID"];
    }

    //General Equipment Module.
    insert_as_string_if_nonempty(0x0008, 0x0070, "Manufacturer");
    insert_as_string_if_nonempty(0x0008, 0x0080, "InstitutionName");
    insert_as_string_if_nonempty(0x0018, 0x1000, "DeviceSerialNumber");
    insert_as_string_if_nonempty(0x0008, 0x1010, "StationName");
    insert_as_string_if_nonempty(0x0008, 0x1040, "InstitutionalDepartmentName");
    insert_as_string_if_nonempty(0x0008, 0x1090, "ManufacturersModelName");
    insert_as_string_if_nonempty(0x0018, 0x1020, "SoftwareVersions");

    //VOI LUT Module.
    insert_as_string_if_nonempty(0x0028, 0x1050, "WindowCenter");
    insert_as_string_if_nonempty(0x0028, 0x1051, "WindowWidth");

    //General Image Module.
    insert_as_string_if_nonempty(0x0020, 0x0013, "InstanceNumber");
    insert_as_string_if_nonempty(0x0020, 0x0020, "PatientOrientation");
    insert_as_string_if_nonempty(0x0008, 0x0023, "ContentDate");
    insert_as_string_if_nonempty(0x0008, 0x0033, "ContentTime");
    insert_as_string_if_nonempty(0x0008, 0x0008, "ImageType");
    insert_as_string_if_nonempty(0x0020, 0x0012, "AcquisitionNumber");
    insert_as_string_if_nonempty(0x0008, 0x0022, "AcquisitionDate");
    insert_as_string_if_nonempty(0x0008, 0x0032, "AcquisitionTime");
    insert_as_string_if_nonempty(0x0008, 0x2111, "DerivationDescription");
    //insert_as_string_if_nonempty(0x0008, 0x9215, "DerivationCodeSequence");
    insert_as_string_if_nonempty(0x0020, 0x1002, "ImagesInAcquisition");
    insert_as_string_if_nonempty(0x0020, 0x4000, "ImageComments");
    insert_as_string_if_nonempty(0x0028, 0x0300, "QualityControlImage");

    //Image Plane Module.
    insert_as_string_if_nonempty(0x0028, 0x0030, "PixelSpacing");
    insert_as_string_if_nonempty(0x0020, 0x0037, "ImageOrientationPatient");
    insert_as_string_if_nonempty(0x0020, 0x0032, "ImagePositionPatient");
    insert_as_string_if_nonempty(0x0018, 0x0050, "SliceThickness");
    insert_as_string_if_nonempty(0x0020, 0x1041, "SliceLocation");

    //Image Pixel Module.
    insert_as_string_if_nonempty(0x0028, 0x0002, "SamplesPerPixel");
    insert_as_string_if_nonempty(0x0028, 0x0004, "PhotometricInterpretation");
    insert_as_string_if_nonempty(0x0028, 0x0010, "Rows");
    insert_as_string_if_nonempty(0x0028, 0x0011, "Columns");
    insert_as_string_if_nonempty(0x0028, 0x0100, "BitsAllocated");
    insert_as_string_if_nonempty(0x0028, 0x0101, "BitsStored");
    insert_as_string_if_nonempty(0x0028, 0x0102, "HighBit");
    insert_as_string_if_nonempty(0x0028, 0x0103, "PixelRepresentation");
    //insert_as_string_if_nonempty(0x7FE0, 0x0010, "PixelData"); //Raw pixel bytes.
    insert_as_string_if_nonempty(0x0028, 0x0006, "PlanarConfiguration");
    insert_as_string_if_nonempty(0x0028, 0x0034, "PixelAspectRatio");

    //Multi-Frame Module.
    insert_as_string_if_nonempty(0x0028, 0x0008, "NumberOfFrames");
    insert_as_string_if_nonempty(0x0028, 0x0009, "FrameIncrementPointer");

    //Modality LUT Module.
    //insert_as_string_if_nonempty(0x0028, 0x3000, "ModalityLUTSequence");
    insert_as_string_if_nonempty(0x0028, 0x3002, "LUTDescriptor");
    insert_as_string_if_nonempty(0x0028, 0x3004, "ModalityLUTType");
    insert_as_string_if_nonempty(0x0028, 0x3006, "LUTData");
    insert_as_string_if_nonempty(0x0028, 0x1052, "RescaleIntercept");
    insert_as_string_if_nonempty(0x0028, 0x1053, "RescaleSlope");
    insert_as_string_if_nonempty(0x0028, 0x1054, "RescaleType");

    //RT Dose Module.
    insert_as_string_if_nonempty(0x0028, 0x0002, "SamplesPerPixel");
    insert_as_string_if_nonempty(0x0028, 0x0004, "PhotometricInterpretation");
    insert_as_string_if_nonempty(0x0028, 0x0100, "BitsAllocated");
    insert_as_string_if_nonempty(0x0028, 0x0101, "BitsStored");
    insert_as_string_if_nonempty(0x0028, 0x0102, "HighBit");
    insert_as_string_if_nonempty(0x0028, 0x0103, "PixelRepresentation");
    insert_as_string_if_nonempty(0x3004, 0x0002, "DoseUnits");
    insert_as_string_if_nonempty(0x3004, 0x0004, "DoseType");
    insert_as_string_if_nonempty(0x3004, 0x000a, "DoseSummationType");
    insert_as_string_if_nonempty(0x3004, 0x000e, "DoseGridScaling");

    insert_seq_tag_as_string_if_nonempty( 0x300C, 0x0002, "ReferencedRTPlanSequence",
                                          0x0008, 0x1150, "ReferencedSOPClassUID");
    insert_seq_tag_as_string_if_nonempty( 0x300C, 0x0002, "ReferencedRTPlanSequence",
                                          0x0008, 0x1155, "ReferencedSOPInstanceUID");
    insert_seq_tag_as_string_if_nonempty( 0x300C, 0x0020, "ReferencedFractionGroupSequence",
                                          0x300C, 0x0022, "ReferencedFractionGroupNumber");
    insert_seq_tag_as_string_if_nonempty( 0x300C, 0x0004, "ReferencedBeamSequence",
                                          0x300C, 0x0006, "ReferencedBeamNumber");

    //insert_as_string_if_nonempty(0x300C, 0x0002, "ReferencedRTPlanSequence");
    //insert_as_string_if_nonempty(0x0008, 0x1150, "ReferencedSOPClassUID");
    //insert_as_string_if_nonempty(0x0008, 0x1155, "ReferencedSOPInstanceUID");
    //insert_as_string_if_nonempty(0x300C, 0x0020, "ReferencedFractionGroupSequence");
    //insert_as_string_if_nonempty(0x300C, 0x0022, "ReferencedFractionGroupNumber");
    //insert_as_string_if_nonempty(0x300C, 0x0004, "ReferencedBeamSequence");
    //insert_as_string_if_nonempty(0x300C, 0x0006, "ReferencedBeamNumber");
    
    insert_seq_vec_tag_as_string_if_nonempty( std::deque<path_node>(
                                              { { 0x300C, 0x0002, "ReferencedRTPlanSequence" },
                                                { 0x300C, 0x0020, "ReferencedFractionGroupSequence" },
                                                { 0x300C, 0x0004, "ReferencedBeamSequence" },
                                                { 0x300C, 0x0006, "ReferencedBeamNumber" } }) );

    //CT Image Module.
    insert_as_string_if_nonempty(0x0018, 0x0060, "KVP");

    //RT Image Module.
    insert_as_string_if_nonempty(0x3002, 0x0002, "RTImageLabel");
    insert_as_string_if_nonempty(0x3002, 0x0004, "RTImageDescription");
    insert_as_string_if_nonempty(0x3002, 0x000a, "ReportedValuesOrigin");
    insert_as_string_if_nonempty(0x3002, 0x000c, "RTImagePlane");
    insert_as_string_if_nonempty(0x3002, 0x000d, "XRayImageReceptorTranslation");
    insert_as_string_if_nonempty(0x3002, 0x000e, "XRayImageReceptorAngle");
    insert_as_string_if_nonempty(0x3002, 0x0010, "RTImageOrientation");
    insert_as_string_if_nonempty(0x3002, 0x0011, "ImagePlanePixelSpacing");
    insert_as_string_if_nonempty(0x3002, 0x0012, "RTImagePosition");
    insert_as_string_if_nonempty(0x3002, 0x0020, "RadiationMachineName");
    insert_as_string_if_nonempty(0x3002, 0x0022, "RadiationMachineSAD");
    insert_as_string_if_nonempty(0x3002, 0x0026, "RTImageSID");
    insert_as_string_if_nonempty(0x3002, 0x0029, "FractionNumber");

    insert_as_string_if_nonempty(0x300a, 0x00b3, "PrimaryDosimeterUnit");
    insert_as_string_if_nonempty(0x300a, 0x011e, "GantryAngle");
    insert_as_string_if_nonempty(0x300a, 0x0120, "BeamLimitingDeviceAngle");
    insert_as_string_if_nonempty(0x300a, 0x0122, "PatientSupportAngle");
    insert_as_string_if_nonempty(0x300a, 0x0128, "TableTopVerticalPosition");
    insert_as_string_if_nonempty(0x300a, 0x0129, "TableTopLongitudinalPosition");
    insert_as_string_if_nonempty(0x300a, 0x012a, "TableTopLateralPosition");
    insert_as_string_if_nonempty(0x300a, 0x012c, "IsocenterPosition");

    insert_as_string_if_nonempty(0x300c, 0x0006, "ReferencedBeamNumber");
    insert_as_string_if_nonempty(0x300c, 0x0008, "StartCumulativeMetersetWeight");
    insert_as_string_if_nonempty(0x300c, 0x0009, "EndCumulativeMetersetWeight");
    insert_as_string_if_nonempty(0x300c, 0x0022, "ReferencedFractionGroupNumber");

    insert_seq_tag_as_string_if_nonempty( 0x3002, 0x0030, "ExposureSequence",
                                          0x0018, 0x0060, "KVP");
    insert_seq_tag_as_string_if_nonempty( 0x3002, 0x0030, "ExposureSequence",
                                          0x0018, 0x1150, "ExposureTime");
    insert_seq_tag_as_string_if_nonempty( 0x3002, 0x0030, "ExposureSequence",
                                          0x3002, 0x0032, "MetersetExposure");
    insert_seq_vec_tag_as_string_if_nonempty( std::deque<path_node>(
                                              { { 0x3002, 0x0030, "ExposureSequence" },
                                                { 0x300A, 0x00B6, "BeamLimitingDeviceSequence" },
                                                { 0x300A, 0x00B8, "RTBeamLimitingDeviceType" } }) );
    insert_seq_vec_tag_as_string_if_nonempty( std::deque<path_node>(
                                              { { 0x3002, 0x0030, "ExposureSequence" },
                                                { 0x300A, 0x00B6, "BeamLimitingDeviceSequence" },
                                                { 0x300A, 0x00BC, "NumberOfLeafJawPairs" } }) );
    insert_seq_vec_tag_as_string_if_nonempty( std::deque<path_node>(
                                              { { 0x3002, 0x0030, "ExposureSequence" },
                                                { 0x300A, 0x00B6, "BeamLimitingDeviceSequence" },
                                                { 0x300A, 0x011C, "LeafJawPositions" } }) );

    //RT Plan Module.
    insert_as_string_if_nonempty(0x300a, 0x0002, "RTPlanLabel");
    insert_as_string_if_nonempty(0x300a, 0x0003, "RTPlanName");
    insert_as_string_if_nonempty(0x300a, 0x0004, "RTPlanDescription");
    insert_as_string_if_nonempty(0x300a, 0x0006, "RTPlanDate");
    insert_as_string_if_nonempty(0x300a, 0x0007, "RTPlanTime");
    insert_as_string_if_nonempty(0x300a, 0x000c, "RTPlanGeometry");

    // MR Image Module
    insert_as_string_if_nonempty(0x0018, 0x0020, "ScanningSequence");
    insert_as_string_if_nonempty(0x0018, 0x0021, "SequenceVariant");
    insert_as_string_if_nonempty(0x0018, 0x0024, "SequenceName");
    insert_as_string_if_nonempty(0x0018, 0x0022, "ScanOptions");
    insert_as_string_if_nonempty(0x0018, 0x0023, "MRAcquisitionType");
    insert_as_string_if_nonempty(0x0018, 0x0080, "RepetitionTime");
    insert_as_string_if_nonempty(0x0018, 0x0081, "EchoTime");
    insert_as_string_if_nonempty(0x0018, 0x0091, "EchoTrainLength");
    insert_as_string_if_nonempty(0x0018, 0x0082, "InversionTime");
    insert_as_string_if_nonempty(0x0018, 0x1060, "TriggerTime");

    insert_as_string_if_nonempty(0x0018, 0x0025, "AngioFlag");
    insert_as_string_if_nonempty(0x0018, 0x1062, "NominalInterval");
    insert_as_string_if_nonempty(0x0018, 0x1090, "CardiacNumberofImages");

    insert_as_string_if_nonempty(0x0018, 0x0083, "NumberofAverages");
    insert_as_string_if_nonempty(0x0018, 0x0084, "ImagingFrequency");
    insert_as_string_if_nonempty(0x0018, 0x0085, "ImagedNucleus");
    insert_as_string_if_nonempty(0x0018, 0x0086, "EchoNumbers");
    insert_as_string_if_nonempty(0x0018, 0x0087, "MagneticFieldStrength");

    insert_as_string_if_nonempty(0x0018, 0x0088, "SpacingBetweenSlices");
    insert_as_string_if_nonempty(0x0018, 0x0089, "NumberofPhaseEncodingSteps");
    insert_as_string_if_nonempty(0x0018, 0x0093, "PercentSampling");
    insert_as_string_if_nonempty(0x0018, 0x0094, "PercentPhaseFieldofView");
    insert_as_string_if_nonempty(0x0018, 0x0095, "PixelBandwidth");

    insert_as_string_if_nonempty(0x0018, 0x1250, "ReceiveCoilName");
    insert_as_string_if_nonempty(0x0018, 0x1251, "TransmitCoilName");
    insert_as_string_if_nonempty(0x0018, 0x1310, "AcquisitionMatrix");
    insert_as_string_if_nonempty(0x0018, 0x1312, "InplanePhaseEncodingDirection");
    insert_as_string_if_nonempty(0x0018, 0x1314, "FlipAngle");
    insert_as_string_if_nonempty(0x0018, 0x1315, "VariableFlipAngleFlag");
    insert_as_string_if_nonempty(0x0018, 0x1316, "SAR");
    insert_as_string_if_nonempty(0x0018, 0x1318, "dB_dt");

    //MR Diffusion Macro Attributes.
    insert_seq_vec_tag_as_string_if_nonempty( std::deque<path_node>(
                                              { { 0x0018, 0x9117, "MRDiffusionSequence" },
                                                { 0x0018, 0x9087, "DiffusionBValue" } }) );
    insert_seq_vec_tag_as_string_if_nonempty( std::deque<path_node>(
                                              { { 0x0018, 0x9117, "MRDiffusionSequence" },
                                                { 0x0018, 0x9075, "DiffusionDirection" } }) );
    insert_seq_vec_tag_as_string_if_nonempty( std::deque<path_node>(
                                              { { 0x0018, 0x9117, "MRDiffusionSequence" },
                                                { 0x0018, 0x9076, "DiffusionGradientDirectionSequence" },
                                                { 0x0018, 0x9089, "DiffusionGradientOrientation" } }) );
    insert_seq_vec_tag_as_string_if_nonempty( std::deque<path_node>(
                                              { { 0x0018, 0x9117, "MRDiffusionSequence" },
                                                { 0x0018, 0x9147, "DiffusionAnisotropyType" } }) );

    //MR Image and Spectroscopy Instance Macro.
    insert_as_string_if_nonempty(0x0018, 0x9073, "AcquisitionDuration");

    //Siemens MR Private Diffusion Module, as detailed in syngo(R) MR E11 conformance statement.
    insert_as_string_if_nonempty(0x0019, 0x0010, "SiemensMRHeader");
    insert_as_string_if_nonempty(0x0019, 0x100c, "DiffusionBValue");
    insert_as_string_if_nonempty(0x0019, 0x100d, "DiffusionDirection");
    insert_as_string_if_nonempty(0x0019, 0x100e, "DiffusionGradientVector");
    insert_as_string_if_nonempty(0x0019, 0x1027, "DiffusionBMatrix");  // multiplicity = 3.
    insert_as_string_if_nonempty(0x0019, 0x0103, "PixelRepresentation"); // multiplicity = 6.

    // PET Image Module.
    insert_as_string_if_nonempty(0x0054, 0x1001, "Units");

    // PET Isotope Module.
    insert_seq_vec_tag_as_string_if_nonempty( std::deque<path_node>(
                                              { { 0x0054, 0x0016, "RadiopharmaceuticalInformationSequence" },
                                                { 0x0054, 0x0300, "RadionuclideCodeSequence" } }) );
    insert_seq_vec_tag_as_string_if_nonempty( std::deque<path_node>(
                                              { { 0x0054, 0x0016, "RadiopharmaceuticalInformationSequence" },
                                                { 0x0018, 0x1070, "RadiopharmaceuticalRoute" } }) );
    insert_seq_vec_tag_as_string_if_nonempty( std::deque<path_node>(
                                              { { 0x0054, 0x0016, "RadiopharmaceuticalInformationSequence" },
                                                { 0x0018, 0x1071, "RadiopharmaceuticalVolume" } }) );
    insert_seq_vec_tag_as_string_if_nonempty( std::deque<path_node>(
                                              { { 0x0054, 0x0016, "RadiopharmaceuticalInformationSequence" },
                                                { 0x0018, 0x1074, "RadionuclideTotalDose" } }) ); // initially administered, in Bq
    insert_seq_vec_tag_as_string_if_nonempty( std::deque<path_node>(
                                              { { 0x0054, 0x0016, "RadiopharmaceuticalInformationSequence" },
                                                { 0x0018, 0x1075, "RadionuclideHalfLife" } }) );
    insert_seq_vec_tag_as_string_if_nonempty( std::deque<path_node>(
                                              { { 0x0054, 0x0016, "RadiopharmaceuticalInformationSequence" },
                                                { 0x0018, 0x1077, "RadiopharmaceuticalSpecificActivity" } }) );
    insert_seq_vec_tag_as_string_if_nonempty( std::deque<path_node>(
                                              { { 0x0054, 0x0016, "RadiopharmaceuticalInformationSequence" },
                                                { 0x0018, 0x0031, "Radiopharmaceutical" } }) );

    //Unclassified others...
    insert_as_string_if_nonempty(0x0029, 0x0010, "SiemensCSAHeaderVersion");
    insert_as_string_if_nonempty(0x0029, 0x0011, "SiemensMEDCOMHeaderVersion");

    insert_as_string_if_nonempty(0x0029, 0x1008, "CSAImageHeaderType"); // CS type
    insert_as_string_if_nonempty(0x0029, 0x1009, "CSAImageHeaderVersion"); // LO type

    if(const auto header = extract_as_binary(0x0029, 0x1010); header.size() != 0 ){ // "CSAImageHeaderInfo", OB type
        const auto parsed = parse_CSA2_binary(header);
        for(const auto &p : parsed) out["CSAImage/" + p.first] = p.second;
    }

    insert_as_string_if_nonempty(0x0029, 0x1018, "CSASeriesHeaderType"); // CS type
    insert_as_string_if_nonempty(0x0029, 0x1019, "CSASeriesHeaderVersion"); // LO type
    if(const auto header = extract_as_binary(0x0029, 0x1020); header.size() != 0 ){ // "CSASeriesHeaderInfo", OB type
        const auto parsed = parse_CSA2_binary(header);
        for(const auto &p : parsed) out["CSASeries/" + p.first] = p.second;
    }

    insert_as_string_if_nonempty(0x2001, 0x100a, "SliceNumber");
    insert_as_string_if_nonempty(0x0054, 0x1330, "ImageIndex");

    insert_as_string_if_nonempty(0x3004, 0x000C, "GridFrameOffsetVector");

    insert_as_string_if_nonempty(0x0020, 0x0100, "TemporalPositionIdentifier");
    insert_as_string_if_nonempty(0x0020, 0x9128, "TemporalPositionIndex");
    //insert_seq_vec_tag_as_string_if_nonempty(std::deque<path_node>(
    //                                         { { 0x0020, 0x9128, "TemporalPositionIndex" } }) );
    insert_as_string_if_nonempty(0x0020, 0x0105, "NumberofTemporalPositions");

    insert_as_string_if_nonempty(0x0020, 0x0110, "TemporalResolution");
    insert_as_string_if_nonempty(0x0054, 0x1300, "FrameReferenceTime");
    insert_as_string_if_nonempty(0x0018, 0x1063, "FrameTime");
    insert_as_string_if_nonempty(0x0018, 0x1060, "TriggerTime");
    insert_as_string_if_nonempty(0x0018, 0x1069, "TriggerTimeOffset");

    insert_as_string_if_nonempty(0x0040, 0x0244, "PerformedProcedureStepStartDate");
    insert_as_string_if_nonempty(0x0040, 0x0245, "PerformedProcedureStepStartTime");
    insert_as_string_if_nonempty(0x0040, 0x0250, "PerformedProcedureStepEndDate");
    insert_as_string_if_nonempty(0x0040, 0x0251, "PerformedProcedureStepEndTime");

    insert_as_string_if_nonempty(0x0018, 0x1152, "Exposure");
    insert_as_string_if_nonempty(0x0018, 0x1150, "ExposureTime");
    insert_as_string_if_nonempty(0x0018, 0x1153, "ExposureInMicroAmpereSeconds");
    insert_as_string_if_nonempty(0x0018, 0x1151, "XRayTubeCurrent");


    insert_as_string_if_nonempty(0x0018, 0x1030, "ProtocolName");

    insert_as_string_if_nonempty(0x0008, 0x0090, "ReferringPhysicianName");


    return out;
}



//------------------ Contours ---------------------

//Returns a bimap with the (raw) ROI tags and their corresponding ROI numbers. The ROI numbers are
// arbitrary identifiers used within the DICOM file to identify contours more conveniently.
bimap<std::string,long int> get_ROI_tags_and_numbers(const std::filesystem::path &FilenameIn){
    using namespace puntoexe;
    ptr<puntoexe::stream> readStream(new puntoexe::stream);
    readStream->openFile(FilenameIn.c_str(), std::ios::in);

    ptr<puntoexe::streamReader> reader(new puntoexe::streamReader(readStream));
    ptr<imebra::dataSet> TopDataSet = imebra::codecs::codecFactory::getCodecFactory()->load(reader);
    ptr<imebra::dataSet> SecondDataSet;

    size_t i=0, j;
    std::string ROI_name;
    long int ROI_number;
    bimap<std::string,long int> the_pairs;
 
    do{
         //See gdcmdump output of an RS file  OR  Dicom documentation for these
         // numbers. 0x3006,0x0020 defines the top-level ROI sequence. 0x3006,0x0026
         // defines the name for each item (of which there might be many with the same number..)
         SecondDataSet = TopDataSet->getSequenceItem(0x3006, 0, 0x0020, i);
         if(SecondDataSet != nullptr){
            //Loop over all items within this data set. There should not be more than one, but I am 
            // suspect of 'data from the wild.'
            j=0;
            do{
                ROI_name   = SecondDataSet->getString(0x3006, 0, 0x0026, j);
                ROI_number = static_cast<long int>(SecondDataSet->getSignedLong(0x3006, 0, 0x0022, j));
                if(ROI_name.size()){
                    the_pairs[ROI_number] = ROI_name;
                }
                ++j;
            }while((ROI_name.size() != 0));
        }

        ++i;
    }while(SecondDataSet != nullptr);
    return the_pairs;
}


//Returns contour data from a DICOM RTSTRUCT file sorted into ROI-specific collections.
std::unique_ptr<Contour_Data> get_Contour_Data(const std::filesystem::path &filename){
    auto output = std::make_unique<Contour_Data>();
    bimap<std::string,long int> tags_names_and_numbers = get_ROI_tags_and_numbers(filename);

    auto FileMetadata = get_metadata_top_level_tags(filename);

    using namespace puntoexe;
    ptr<puntoexe::stream> readStream(new puntoexe::stream);
    readStream->openFile(filename.c_str(), std::ios::in);
    ptr<puntoexe::streamReader> reader(new puntoexe::streamReader(readStream));
    ptr<imebra::dataSet> TopDataSet = imebra::codecs::codecFactory::getCodecFactory()->load(reader);
    ptr<imebra::dataSet> SecondDataSet, ThirdDataSet;

    //Collect the data into a container of contours with meta info. It may be unordered (within the file).
    std::map<std::tuple<std::string,long int>, contour_collection<double>> mapcache;
    for(size_t i=0; (SecondDataSet = TopDataSet->getSequenceItem(0x3006, 0, 0x0039, i)) != nullptr; ++i){
        long int Last_ROI_Numb = 0;
        for(size_t j=0; (ThirdDataSet = SecondDataSet->getSequenceItem(0x3006, 0, 0x0040, j)) != nullptr; ++j){
            auto ROI_number = static_cast<long int>(SecondDataSet->getSignedLong(0x3006, 0, 0x0084, j));
            if(ROI_number == 0){
                ROI_number = Last_ROI_Numb;
            }else{
                Last_ROI_Numb = ROI_number;
            }

            if((ThirdDataSet = SecondDataSet->getSequenceItem(0x3006, 0, 0x0040, j)) == nullptr){
                continue;
            }

            ptr<puntoexe::imebra::handlers::dataHandler> the_data_handler;
            for(size_t k=0; (the_data_handler = ThirdDataSet->getDataHandler(0x3006, 0, 0x0050, k, false)) != nullptr; ++k){
                contour_of_points<double> shtl;
                shtl.closed = true;

                //This is the number of coordinates we will get (ie. the number of doubles).
                const long int numb_of_coordinates = the_data_handler->getSize();
                for(long int N = 0; N < numb_of_coordinates; N += 3){
                    const double x = the_data_handler->getDouble(N + 0);
                    const double y = the_data_handler->getDouble(N + 1);
                    const double z = the_data_handler->getDouble(N + 2);
                    shtl.points.emplace_back(x,y,z);
                }
                shtl.Reorient_Counter_Clockwise();
                shtl.metadata = FileMetadata;

                auto ROIName = tags_names_and_numbers[ROI_number];
                shtl.metadata["ROINumber"] = std::to_string(ROI_number);
                shtl.metadata["ROIName"] = ROIName;
                
                const auto key = std::make_tuple(ROIName, ROI_number);
                mapcache[key].contours.push_back(std::move(shtl));
            }
        }
    }


    //Now sort the contours into contour_with_metas. We sort based on ROI number.
    for(auto & m_it : mapcache){
        output->ccs.emplace_back( ); //std::move(m_it->second) ) );
        output->ccs.back() = m_it.second; 
    }

    //Find the minimum separation between contours (which isn't zero).
    double min_spacing = 1E30;
    for(auto & cc : output->ccs){ 
        if(cc.contours.size() < 2) continue;

        for(auto c1_it = cc.contours.begin(); c1_it != --(cc.contours.end()); ++c1_it){
            auto c2_it = c1_it;
            ++c2_it;

            const double height1 = c1_it->Average_Point().Dot(vec3<double>(0.0,0.0,1.0));
            const double height2 = c2_it->Average_Point().Dot(vec3<double>(0.0,0.0,1.0));
            const double spacing = YGORABS(height2-height1);

            if((spacing < min_spacing) && (spacing > 1E-3)) min_spacing = spacing;
        }
    }
    //FUNCINFO("The minimum spacing found was " << min_spacing);
    for(auto & cc_it : output->ccs){
        for(auto & cc : cc_it.contours) cc.metadata["MinimumSeparation"] = std::to_string(min_spacing);
//        output->ccs.back().metadata["MinimumSeparation"] = std::to_string(min_spacing);
    }

    return output;
}


//-------------------- Images ----------------------
//This routine will often result in an array with only a single image. So collate output as needed.
//
// NOTE: I believe this routine is only valid for single frame images, like common CT and MR images.
//       PT and US have not been tested. RTDOSE files should use the Load_Dose_Array code, which 
//       handles multi-frame images (and thus might be adaptable for other non-RTDOSE multi-frame 
//       images).
std::unique_ptr<Image_Array> Load_Image_Array(const std::filesystem::path &FilenameIn){
    auto out = std::make_unique<Image_Array>();

    using namespace puntoexe;
    ptr<puntoexe::stream> readStream(new puntoexe::stream);
    readStream->openFile(FilenameIn.c_str(), std::ios::in);

    ptr<puntoexe::streamReader> reader(new puntoexe::streamReader(readStream));
    ptr<imebra::dataSet> TopDataSet = imebra::codecs::codecFactory::getCodecFactory()->load(reader);

    //Helper routines that do not create tags when they are missing.
    //
    // Note: Issuing the following:
    //    const auto image_pos_x = static_cast<double>(TopDataSet->getDouble(0x0020, 0, 0x0032, 0)); 
    //       will result in a new tag with a default value being created if it did not previously exist!
    auto retrieve_as_string = [&TopDataSet](uint16_t group, 
                                            uint16_t tag, 
                                            uint32_t element = 0) 
                                            -> std::optional<std::string> {
        const uint32_t first_order = 0; // Always zero for modern DICOM files.

        //Check if the tag is present in the file. If not, bail.
        const bool create_if_not_found = false;
        const auto ptr = TopDataSet->getTag(group, first_order, tag, create_if_not_found);
        if(ptr == nullptr) return std::optional<std::string>();

        //Retrieve the element.
        const auto str = TopDataSet->getString(group, first_order, tag, element);
        return str;
    };
    auto retrieve_as_long_int = [&TopDataSet,&retrieve_as_string](uint16_t group, 
                                              uint16_t tag, 
                                              uint32_t element = 0) 
                                              -> std::optional<double> {
        auto o = retrieve_as_string(group,tag,element);
        if(!o) return std::nullopt;
        return std::stol(o.value());
    };                                            
    auto retrieve_as_double = [&TopDataSet,&retrieve_as_string](uint16_t group, 
                                            uint16_t tag, 
                                            uint32_t element = 0) 
                                            -> std::optional<double> {
        auto o = retrieve_as_string(group,tag,element);

        std::optional<double> out = std::nullopt;
        if(!o) return out;
        try{
            out = std::stod(o.value());
        }catch(const std::exception &){}
        return out;
    };                                            
    auto retrieve_coalesce_as_string = [&TopDataSet,&retrieve_as_string](std::list<std::array<uint32_t,3>> qs) 
                                                    -> std::optional<std::string> {
        for(const auto &q : qs){
            const auto group = static_cast<uint16_t>(q[0]);
            const auto tag = static_cast<uint16_t>(q[1]);
            const auto element = q[2];
            auto o = retrieve_as_string(group,tag,element);
            if(o) return o;
        }

        //None were available.
        return std::nullopt;
    };
    auto retrieve_coalesce_as_long_int = [&TopDataSet,&retrieve_coalesce_as_string](std::list<std::array<uint32_t,3>> qs)
                                                       -> std::optional<double> {
        auto o = retrieve_coalesce_as_string(qs);

        std::optional<double> out = std::nullopt;
        if(!o) return out;
        try{
            out = std::stol(o.value());
        }catch(const std::exception &){}
        return out;
    };
    auto retrieve_coalesce_as_double = [&TopDataSet,&retrieve_coalesce_as_string](std::list<std::array<uint32_t,3>> qs)
                                                     -> std::optional<double> {
        auto o = retrieve_coalesce_as_string(qs);

        std::optional<double> out = std::nullopt;
        if(!o) return out;
        try{
            out = std::stod(o.value());
        }catch(const std::exception &){}
        return out;
    };

    auto metadata_coalesce_as_double = [](const decltype(get_metadata_top_level_tags("")) &tlm,
                                          std::list<std::tuple<std::string, uint32_t>> qs ) -> std::optional<double> {
        // Note: the DICOM multiplicity is accepted with the key name.
        std::optional<double> out = std::nullopt;
        for(const auto &t : qs){
            const auto res = tlm.find(std::get<std::string>(t));
            if(res != std::end(tlm)){
                try{
                    auto tokens = SplitStringToVector(res->second, '\\', 'd');
                    tokens = SplitVector(tokens, ',', 'd');
                    out = std::stod( tokens.at( std::get<uint32_t>(t) ) );
                    break;
                }catch(const std::exception &){}
            }
        }
        return out;
    };                                            
        

    // ------------------------------------------- General --------------------------------------------------
    const auto modality = retrieve_as_string(0x0008, 0x0060).value();


    // ---------------------------------------- Image Metadata ----------------------------------------------
    const auto tlm = get_metadata_top_level_tags(FilenameIn);

    //These should exist in all files. They appear to be the same for CT and DS files of the same set. Not sure
    // if this is *always* the case.
    const auto image_pos_x = retrieve_coalesce_as_double({ {0x0020, 0x0032, 0}, //"ImagePositionPatient".
                                                           {0x3002, 0x0012, 0}  //"RTImagePosition".
                                                         }).value_or(0.0);
    const auto image_pos_y = retrieve_coalesce_as_double({ {0x0020, 0x0032, 1}, //"ImagePositionPatient".
                                                           {0x3002, 0x0012, 1}  //"RTImagePosition".
                                                         }).value_or(0.0);

    auto image_pos_z = retrieve_coalesce_as_double({ {0x0020, 0x0032, 2}, //"ImagePositionPatient".
                                                     {0x3002, 0x000d, 2}  //"XRayImageReceptorTranslation".
                                                   }).value_or(std::numeric_limits<double>::quiet_NaN());
    if(!std::isfinite(image_pos_z)){ // Try derived values.
        // This is useful for RTIMAGES.
        const auto RTImageSID = retrieve_coalesce_as_double({ {0x3002, 0x0026, 0} }).value_or(1000.0);
        const auto RadMchnSAD = retrieve_coalesce_as_double({ {0x3002, 0x0022, 0} }).value_or(1000.0);
        image_pos_z = (RadMchnSAD - RTImageSID); // For consistency with XRayImageReceptorTranslation z-coord.
    }
    const vec3<double> image_pos(image_pos_x,image_pos_y,image_pos_z); //Only for first image!


    const auto image_orien_c_x = retrieve_coalesce_as_double({ {0x0020, 0x0037, 0}, //"ImagePositionPatient".
                                                               {0x3002, 0x0010, 0}  //"RTImageOrientation".
                                                             }).value_or(1.0);
    const auto image_orien_c_y = retrieve_coalesce_as_double({ {0x0020, 0x0037, 1}, //"ImagePositionPatient".
                                                               {0x3002, 0x0010, 1}  //"RTImageOrientation".
                                                             }).value_or(0.0);
    const auto image_orien_c_z = retrieve_coalesce_as_double({ {0x0020, 0x0037, 2}, //"ImagePositionPatient".
                                                               {0x3002, 0x0010, 2}  //"RTImageOrientation".
                                                             }).value_or(0.0);
    const vec3<double> image_orien_c = vec3<double>(image_orien_c_x,image_orien_c_y,image_orien_c_z).unit();


    const auto image_orien_r_x = retrieve_coalesce_as_double({ {0x0020, 0x0037, 3}, //"ImageOrientationPatient".
                                                               {0x3002, 0x0010, 3}  //"RTImageOrientation".
                                                             }).value_or(0.0);
    const auto image_orien_r_y = retrieve_coalesce_as_double({ {0x0020, 0x0037, 4}, //"ImageOrientationPatient".
                                                               {0x3002, 0x0010, 4}  //"RTImageOrientation".
                                                             }).value_or(1.0);
    const auto image_orien_r_z = retrieve_coalesce_as_double({ {0x0020, 0x0037, 5}, //"ImageOrientationPatient".
                                                               {0x3002, 0x0010, 5}  //"RTImageOrientation".
                                                             }).value_or(0.0);
    const vec3<double> image_orien_r = vec3<double>(image_orien_r_x,image_orien_r_y,image_orien_r_z).unit();

    const vec3<double> image_anchor  = vec3<double>(0.0,0.0,0.0); //Could us RTIMAGE IsocenterPosition (300a,012c) ?

    //Determine how many frames there are in the pixel data. A CT scan may just be a 2d jpeg or something, 
    // but dose pixel data is 3d data composed of 'frames' of stacked 2d data.
    const auto frame_count = retrieve_coalesce_as_long_int({ {0x0028, 0x0008, 0} }).value_or(0.0);
    if(frame_count != 0) throw std::domain_error("This routine only supports 2D images."
                                                 " Adapt the dose array loading code. Cannot continue");

    const auto image_rows  = retrieve_coalesce_as_long_int({ {0x0028, 0x0010, 0} }).value();
    const auto image_cols  = retrieve_coalesce_as_long_int({ {0x0028, 0x0011, 0} }).value();

    const auto image_pxldy = metadata_coalesce_as_double(tlm, { { "PixelSpacing", 0 }, // Spacing between adjacent rows.
                                                                { "ImagePlanePixelSpacing", 0 },
                                                                { "CSAImage/PixelSpacing", 0 } }).value_or(1.0);

    const auto image_pxldx = metadata_coalesce_as_double(tlm, { { "PixelSpacing", 1 }, // Spacing between adjacent columns.
                                                                { "ImagePlanePixelSpacing", 1 },
                                                                { "CSAImage/PixelSpacing", 1 } }).value_or(image_pxldy);

    //For 2D images, there is often no thickness given. For CT we might have to compare to other files to figure this out.
    // For MR images, the thickness should be specified.
    //
    // Note: In general, images should be given a non-zero thickness since many core routines are build around slices of
    //       finite thickness.
    const auto image_thickness = retrieve_coalesce_as_double({ {0x0018, 0x0050, 0} }).value_or(1.0); //"SliceThickness"

    // -------------------------------------- Pixel Interpretation ------------------------------------------
    if( (TopDataSet->getTag(0x0040,0,0x9212) != nullptr)
    ||  (TopDataSet->getTag(0x0040,0,0x9216) != nullptr)
    ||  (TopDataSet->getTag(0x0040,0,0x9096) != nullptr)
    ||  (TopDataSet->getTag(0x0040,0,0x9211) != nullptr)
    ||  (TopDataSet->getTag(0x0040,0,0x9224) != nullptr)
    ||  (TopDataSet->getTag(0x0040,0,0x9225) != nullptr)
    ||  (TopDataSet->getTag(0x0040,0,0x9212) != nullptr)
    ||  (TopDataSet->getTag(0x0040,0,0x9210) != nullptr)
    ||  (TopDataSet->getTag(0x0028,0,0x3003) != nullptr)
    ||  (TopDataSet->getTag(0x0040,0,0x08EA) != nullptr) ){ 
        throw std::domain_error("This image contains a 'Real World Value' LUT (Look-Up Table), which is presently"
                                " not supported. You will need to fix the code to handle this");
        // NOTE: See DICOM Supplement 49 "Enhanced MR Image Storage SOP Class" at 
        //       ftp://medical.nema.org/medical/dicom/final/sup49_ft.pdf 
        //       (or another document if it has been superceded) for more info.
        //
        //       It should be rather easy to implement this. I just haven't needed to yet.
        //       You could potentially just get Imebra to do it for you. See the LUT code elsewhere
        //       in this routine.
    }

    // --------------------------------------- Image Pixel Data ---------------------------------------------
    {    
        out->imagecoll.images.emplace_back();

        //--------------------------------------------------------------------------------------------------
        //Retrieve the pixel data from file. This is an excessively long exercise!
        ptr<puntoexe::imebra::image> firstImage;
        try{
            firstImage = TopDataSet->getImage(0);
        }catch(const std::exception &e){
            throw std::domain_error("This file does not have accessible pixel data."
                                    " The DICOM image loader should not be called for this file");
        }
    
        //Process image using modalityVOILUT transform to convert its pixel values into meaningful values.
        // From what I can tell, this conversion is necessary to transform the raw data from a possibly
        // manufacturer-specific, proprietary format into something physically meaningful for us. 
        //
        // Note that the modality conversion will use the rescale slope and rescale intercept tags for linear
        // transformations. If nonlinear, the Modality LUT Sequence describe the transformation.
        //
        // I have not experimented with disabling this conversion. Leaving it intact causes the datum from
        // a Philips "Interra" machine's PAR/REC format to coincide with the exported DICOM data.
        ptr<imebra::transforms::transform> modVOILUT(new imebra::transforms::modalityVOILUT(TopDataSet));
        imbxUint32 width, height;
        firstImage->getSize(&width, &height);
        ptr<imebra::image> convertedImage(modVOILUT->allocateOutputImage(firstImage, width, height));
        modVOILUT->runTransform(firstImage, 0, 0, width, height, convertedImage, 0, 0);

    
        //Convert the 'convertedImage' into an image suitable for the viewing on screen. The VOILUT transform 
        // applies the contrast suggested by the dataSet to the image. Apply the first one we find. Relevant
        // DICOM tags reside around (0x0028,0x3010) and (0x0028,0x1050). This lookup generally applies window
        // and level factors, but can also apply non-linear VOI LUT Sequence transformations.
        //
        // This conversion uses the first suggested transformation found in the DICOM file, and will vary
        // from file to file. Generally, the transformation scales the pixel values to cover the range of the
        // available pixel range (i.e., u16). The transformation *CAN* induce clipping or truncation which 
        // cannot be recovered from!
        //
        // Therefore, in my opinion, it is never worthwhile to perform this conversion. If you want to window
        // or scale the values, you should do so as needed using the WindowCenter and WindowWidth values 
        // directly.
        //
        // Report available conversions:
        if(false){
            ptr<imebra::transforms::VOILUT> myVoiLut(new imebra::transforms::VOILUT(TopDataSet));
            std::vector<imbxUint32> VoiLutIds;
            for(imbxUint32 i = 0;  ; ++i){
                const auto VoiLutId = myVoiLut->getVOILUTId(i);
                if(VoiLutId == 0) break;
                VoiLutIds.push_back(VoiLutId);
            }
            //auto VoiLutIds = myVoiLut->getVOILUTIds();
            for(auto VoiLutId : VoiLutIds){
                const std::wstring VoiLutDescriptionWS = myVoiLut->getVOILUTDescription(VoiLutId);
                const std::string VoiLutDescription(VoiLutDescriptionWS.begin(), VoiLutDescriptionWS.end());
                FUNCINFO("Found 'presentation' VOI/LUT with description '" << VoiLutDescription << "' (not applying it!)");

                //Print the center and width of the VOI/LUT.
                imbxInt32 VoiLutCenter = std::numeric_limits<imbxInt32>::max();
                imbxInt32 VoiLutWidth  = std::numeric_limits<imbxInt32>::max();
                myVoiLut->getCenterWidth(&VoiLutCenter, &VoiLutWidth);
                if((VoiLutCenter != std::numeric_limits<imbxInt32>::max())
                || (VoiLutWidth  != std::numeric_limits<imbxInt32>::max())){
                    FUNCINFO("    - 'Presentation' VOI/LUT has centre = " << VoiLutCenter << " and width = " << VoiLutWidth);
                }
            }
        }
        //
        // Disable Imebra conversion:
        ptr<imebra::image> presImage(convertedImage);
        //
        // Enable Imebra conversion:
        //ptr<imebra::transforms::VOILUT> myVoiLut(new imebra::transforms::VOILUT(TopDataSet));
        //imbxUint32 lutId = myVoiLut->getVOILUTId(0);
        //myVoiLut->setVOILUT(lutId);
        //ptr<imebra::image> presImage(myVoiLut->allocateOutputImage(convertedImage, width, height));
        //myVoiLut->runTransform(convertedImage, 0, 0, width, height, presImage, 0, 0);
        //{
        //  //Print a description of the VOI/LUT if available.
        //  //const std::wstring VoiLutDescriptionWS = myVoiLut->getVOILUTDescription(lutId);
        //  //const std::string VoiLutDescription(VoiLutDescriptionWS.begin(), VoiLutDescriptionWS.end());
        //  //FUNCINFO("Using VOI/LUT with description '" << VoiLutDescription << "'");
        //
        //  //Print the center and width of the VOI/LUT.
        //  imbxInt32 VoiLutCenter = std::numeric_limits<imbxInt32>::max();
        //  imbxInt32 VoiLutWidth  = std::numeric_limits<imbxInt32>::max();
        //  myVoiLut->getCenterWidth(&VoiLutCenter, &VoiLutWidth);
        //  if((VoiLutCenter != std::numeric_limits<imbxInt32>::max())
        //  || (VoiLutWidth  != std::numeric_limits<imbxInt32>::max())){
        //      FUNCINFO("Using VOI/LUT with centre = " << VoiLutCenter << " and width = " << VoiLutWidth);
        //  }
        //}

 
        //Get the image in terms of 'RGB'/'MONOCHROME1'/'MONOCHROME2'/'YBR_FULL'/etc.. channels.
        //
        // This allows up to transform the data into a desired format before allocating any space.
        //
        // NOTE: The 'Photometric Interpretation' is specified in the DICOM file at 0x0028,0x0004 as a
        //       string. For instance "MONOCHROME2" is present in some MR images at the time of writing.
        //       It's not clear that I will want Imebra to transform the data under any circumstances, but
        //       to simplify things for now I'll assume we always want 'MONOCHROME2' format.
        //
        // NOTE: After some further digging, I believe letting Imebra convert to monochrome will allow
        //       us to handle compressed images without any extra work.
        puntoexe::imebra::transforms::colorTransforms::colorTransformsFactory*  pFactory = 
            puntoexe::imebra::transforms::colorTransforms::colorTransformsFactory::getColorTransformsFactory();
        ptr<puntoexe::imebra::transforms::transform> myColorTransform = 
            pFactory->getTransform(presImage->getColorSpace(), L"MONOCHROME2");//L"RGB");
        if(myColorTransform != nullptr){ //If we get a nullptr, we do not need to transform the image.
            ptr<puntoexe::imebra::image> rgbImage(myColorTransform->allocateOutputImage(presImage,width,height));
            myColorTransform->runTransform(presImage, 0, 0, width, height, rgbImage, 0, 0);
            presImage = rgbImage;
        }
    
        //Get a 'dataHandler' to access the image data waiting in 'presImage.' Get some image metadata.
        imbxUint32 rowSize, channelPixelSize, channelsNumber, sizeX, sizeY;
        //Select the image to use.
        // firstImage     -- Displays RTIMAGE, and CT(MR?) but neither CT nor RTIMAGE values are in HU.
        // convertedImage -- Works for CT (MR?) but not RTIMAGE.
        // presImage      -- Works for CT and MR, but not RTIMAGE.
        ptr<puntoexe::imebra::image> switchImage = ( modality == "RTIMAGE" ) ? firstImage : presImage;
        ptr<puntoexe::imebra::handlers::dataHandlerNumericBase> myHandler = 
            switchImage->getDataHandler(false, &rowSize, &channelPixelSize, &channelsNumber);
        presImage->getSize(&sizeX, &sizeY);
        //----------------------------------------------------------------------------------------------------

        if((static_cast<long int>(sizeX) != image_cols) || (static_cast<long int>(sizeY) != image_rows)){
            FUNCWARN("sizeX = " << sizeX << ", sizeY = " << sizeY << " and image_cols = " << image_cols << ", image_rows = " << image_rows);
            throw std::domain_error("The number of rows and columns in the image data differ when comparing sizeX/Y and img_rows/cols. Please verify");
            //If this issue arises, I have likely confused definition of X and Y. The DICOM standard specifically calls (0028,0010) 
            // a 'row'. Perhaps I've got many things backward...
        }

        out->imagecoll.images.back().metadata = tlm;
        out->imagecoll.images.back().init_orientation(image_orien_r,image_orien_c);

        const auto img_chnls = static_cast<long int>(channelsNumber);
        out->imagecoll.images.back().init_buffer(image_rows, image_cols, img_chnls); //Underlying type specifies per-pixel space allocated.

        const auto img_pxldz = image_thickness;
        out->imagecoll.images.back().init_spatial(image_pxldx,image_pxldy,img_pxldz, image_anchor, image_pos);

        //Sometimes Imebra returns a different number of bits than the DICOM header specifies. Presumably this
        // is for some reason (maybe even simplification of implementation, which is fair). Since I convert to
        // a float or uint32_t, the only practical concern is whether or not it will fit.
        const auto img_bits  = static_cast<unsigned int>(channelPixelSize*8); //16 bit, 32 bit, 8 bit, etc..
        if(img_bits > 32){
            throw std::domain_error("The number of bits returned by Imebra is too large to fit in uint32_t"
                                    " You can increase this if needed, or try to scale down to 32 bits");
        }

        //Write the data to our allocated memory. We do it pixel-by-pixel because the 'PixelRepresentation' could mean
        // the pixel locality is laid out in various ways (two ways?). This approach abstracts the issue away.
        imbxUint32 data_index = 0;
        for(long int row = 0; row < image_rows; ++row){
            for(long int col = 0; col < image_cols; ++col){
                for(long int chnl = 0; chnl < img_chnls; ++chnl){
                    //Let Imebra work out the conversion by asking for a double. Hope it can be narrowed if necessary!
                    const auto DoubleChannelValue = myHandler->getDouble(data_index);
                    const auto OutgoingPixelValue = static_cast<float>(DoubleChannelValue);

                    out->imagecoll.images.back().reference(row,col,chnl) = OutgoingPixelValue;
                    ++data_index;
                } //Loop over channels.
            } //Loop over columns.
        } //Loop over rows.
    }
    return out;
}

//These 'shared' pointers will actually be unique. This routine just converts from unique to shared for you.
std::list<std::shared_ptr<Image_Array>>  Load_Image_Arrays(const std::list<std::filesystem::path> &filenames){
    std::list<std::shared_ptr<Image_Array>> out;
    for(const auto & filename : filenames){
        out.push_back(Load_Image_Array(filename));
    }
    return out;
}

//Since many images must be loaded individually from a file, we will often have to collate them together.
//
//Note: Returns a nullptr if the collation was not successful. The input data will not be restored to the
//      exact way it was passed in. Returns a valid pointer to an empty Image_Array if there was no data
//      to collate.
//
//Note: Despite using shared_ptrs, if the collation fails some images may be collated while others weren't. 
//      Deep-copy images beforehand if this is something you aren't prepared to deal with.
//
std::unique_ptr<Image_Array> Collate_Image_Arrays(std::list<std::shared_ptr<Image_Array>> &in){
    std::unique_ptr<Image_Array> out(new Image_Array);
    if(in.empty()) return out;

    //Start from the end and work toward the beginning so we can easily pop the end. Keep all images in
    // the original list to ease collating to the first element.
    while(!in.empty()){
        auto pic_it = in.begin();
        const bool GeometricalOverlapOK = true;
        if(!out->imagecoll.Collate_Images((*pic_it)->imagecoll, GeometricalOverlapOK)){
            //We've encountered an issue and the images won't collate. Push the successfully collated
            // images back into the list and return a nullptr.
            in.push_back(std::move(out));
            return nullptr;
        }
        pic_it = in.erase(pic_it);
    }
    return out;
}


//--------------------- Dose -----------------------
//This routine reads a single DICOM dose file.
std::unique_ptr<Image_Array>  Load_Dose_Array(const std::filesystem::path &FilenameIn){
    auto metadata = get_metadata_top_level_tags(FilenameIn);
    metadata["Modality"] = "RTDOSE";

    auto out = std::make_unique<Image_Array>();

    using namespace puntoexe;
    ptr<puntoexe::stream> readStream(new puntoexe::stream);
    readStream->openFile(FilenameIn.c_str(), std::ios::in);

    ptr<puntoexe::streamReader> reader(new puntoexe::streamReader(readStream));
    ptr<imebra::dataSet> TopDataSet = imebra::codecs::codecFactory::getCodecFactory()->load(reader);

    //These should exist in all files. They appear to be the same for CT and DS files of the same set. Not sure
    // if this is *always* the case.
    const auto image_pos_x = static_cast<double>(TopDataSet->getDouble(0x0020, 0, 0x0032, 0));
    const auto image_pos_y = static_cast<double>(TopDataSet->getDouble(0x0020, 0, 0x0032, 1));
    const auto image_pos_z = static_cast<double>(TopDataSet->getDouble(0x0020, 0, 0x0032, 2));
    const vec3<double> image_pos(image_pos_x,image_pos_y,image_pos_z); //Only for first image!

    const auto image_orien_c_x = static_cast<double>(TopDataSet->getDouble(0x0020, 0, 0x0037, 0)); 
    const auto image_orien_c_y = static_cast<double>(TopDataSet->getDouble(0x0020, 0, 0x0037, 1));
    const auto image_orien_c_z = static_cast<double>(TopDataSet->getDouble(0x0020, 0, 0x0037, 2));
    const vec3<double> image_orien_c = vec3<double>(image_orien_c_x,image_orien_c_y,image_orien_c_z).unit();

    const auto image_orien_r_x = static_cast<double>(TopDataSet->getDouble(0x0020, 0, 0x0037, 3));
    const auto image_orien_r_y = static_cast<double>(TopDataSet->getDouble(0x0020, 0, 0x0037, 4));
    const auto image_orien_r_z = static_cast<double>(TopDataSet->getDouble(0x0020, 0, 0x0037, 5));
    const vec3<double> image_orien_r = vec3<double>(image_orien_r_x,image_orien_r_y,image_orien_r_z).unit();

    const vec3<double> image_stack_unit = (image_orien_c.Cross(image_orien_r)).unit(); //Unit vector denoting direction to stack images.
    const vec3<double> image_anchor  = vec3<double>(0.0,0.0,0.0);

    //Determine how many frames there are in the pixel data. A CT scan may just be a 2d jpeg or something, 
    // but dose pixel data is 3d data composed of 'frames' of stacked 2d data.
    const auto frame_count = static_cast<unsigned long int>(TopDataSet->getUnsignedLong(0x0028, 0, 0x0008, 0));
    if(frame_count == 0) throw std::domain_error("No frames were found in file '"_s + FilenameIn.string() + "'. Is it a valid dose file?");

    //This is a redirection to another tag. I've never seen it be anything but (0x3004,0x000c).
    const auto frame_inc_pntrU  = static_cast<long int>(TopDataSet->getUnsignedLong(0x0028, 0, 0x0009, 0));
    const auto frame_inc_pntrL  = static_cast<long int>(TopDataSet->getUnsignedLong(0x0028, 0, 0x0009, 1));
    if((frame_inc_pntrU != static_cast<long int>(0x3004)) || (frame_inc_pntrL != static_cast<long int>(0x000c)) ){
        FUNCWARN(" frame increment pointer U,L = " << frame_inc_pntrU << "," << frame_inc_pntrL);
        throw std::domain_error("Dose file contains a frame increment pointer which we have not encountered before."
                                " Please ensure we can handle it properly");
    }

    std::list<double> gfov;
    for(unsigned long int i=0; i<frame_count; ++i){
        const auto val = static_cast<double>(TopDataSet->getDouble(0x3004, 0, 0x000c, i));
        gfov.push_back(val);
    }

    const double image_thickness = (gfov.size() > 1) ? ( *(++gfov.begin()) - *(gfov.begin()) ) : 1.0; //*NOT* the image separation!

    const auto image_rows  = static_cast<long int>(TopDataSet->getUnsignedLong(0x0028, 0, 0x0010, 0));
    const auto image_cols  = static_cast<long int>(TopDataSet->getUnsignedLong(0x0028, 0, 0x0011, 0));
    const auto image_pxldy = static_cast<double>(TopDataSet->getDouble(0x0028, 0, 0x0030, 0)); //Spacing between adjacent rows.
    const auto image_pxldx = static_cast<double>(TopDataSet->getDouble(0x0028, 0, 0x0030, 1)); //Spacing between adjacent columns.
    const auto image_bits  = static_cast<unsigned long int>(TopDataSet->getUnsignedLong(0x0028, 0, 0x0101, 0));
    const auto grid_scale  = static_cast<double>(TopDataSet->getDouble(0x3004, 0, 0x000e, 0));

    //Grab the image data for each individual frame.
    auto gfov_it = gfov.begin();
    for(unsigned long int curr_frame = 0; (curr_frame < frame_count) && (gfov_it != gfov.end()); ++curr_frame, ++gfov_it){
        out->imagecoll.images.emplace_back();

        //--------------------------------------------------------------------------------------------------
        //Retrieve the pixel data from file. This is an excessively long exercise!
        ptr<puntoexe::imebra::image> firstImage = TopDataSet->getImage(curr_frame); 	
        if(firstImage == nullptr) throw std::domain_error("This file does not have accessible pixel data. Double check the file");
    
        //Process image using modalityVOILUT transform to convert its pixel values into meaningful values.
        ptr<imebra::transforms::transform> modVOILUT(new imebra::transforms::modalityVOILUT(TopDataSet));
        imbxUint32 width, height;
        firstImage->getSize(&width, &height);
        ptr<imebra::image> convertedImage(modVOILUT->allocateOutputImage(firstImage, width, height));
        modVOILUT->runTransform(firstImage, 0, 0, width, height, convertedImage, 0, 0);
    
        //Convert the 'convertedImage' into an image suitable for the viewing on screen. The VOILUT transform 
        // applies the contrast suggested by the dataSet to the image. Apply the first one we find.
        //
        // I'm not sure how this affects dose values, if at all, so I've disabled it for now.
        //ptr<imebra::transforms::VOILUT> myVoiLut(new imebra::transforms::VOILUT(TopDataSet));
        //imbxUint32 lutId = myVoiLut->getVOILUTId(0);
        //myVoiLut->setVOILUT(lutId);
        //ptr<imebra::image> presImage(myVoiLut->allocateOutputImage(convertedImage, width, height));
        ptr<imebra::image> presImage = convertedImage;
        //myVoiLut->runTransform(convertedImage, 0, 0, width, height, presImage, 0, 0);
 
        //Get the image in terms of 'RGB'/'MONOCHROME1'/'MONOCHROME2'/'YBR_FULL'/etc.. channels.
        //
        // This allows up to transform the data into a desired format before allocating any space.
        puntoexe::imebra::transforms::colorTransforms::colorTransformsFactory*  pFactory = 
             puntoexe::imebra::transforms::colorTransforms::colorTransformsFactory::getColorTransformsFactory();
        ptr<puntoexe::imebra::transforms::transform> myColorTransform = 
             pFactory->getTransform(presImage->getColorSpace(), L"MONOCHROME2");//L"RGB");
        if(myColorTransform != nullptr){ //If we get a '0', we do not need to transform the image.
            ptr<puntoexe::imebra::image> rgbImage(myColorTransform->allocateOutputImage(presImage,width,height));
            myColorTransform->runTransform(presImage, 0, 0, width, height, rgbImage, 0, 0);
            presImage = rgbImage;
        }
    
        //Get a 'dataHandler' to access the image data waiting in 'presImage.' Get some image metadata.
        imbxUint32 rowSize, channelPixelSize, channelsNumber, sizeX, sizeY;
        ptr<puntoexe::imebra::handlers::dataHandlerNumericBase> myHandler = 
            presImage->getDataHandler(false, &rowSize, &channelPixelSize, &channelsNumber);
        presImage->getSize(&sizeX, &sizeY);
        //----------------------------------------------------------------------------------------------------

        if((static_cast<long int>(sizeX) != image_cols) || (static_cast<long int>(sizeY) != image_rows)){
            FUNCWARN("sizeX = " << sizeX << ", sizeY = " << sizeY << " and image_cols = " << image_cols << ", image_rows = " << image_rows);
            throw std::domain_error("The number of rows and columns in the image data differ when comparing sizeX/Y and img_rows/cols. Please verify");
            //If this issue arises, I have likely confused definition of X and Y. The DICOM standard specifically calls (0028,0010) 
            // a 'row'. Perhaps I've got many things backward...
        }

        out->imagecoll.images.back().metadata = metadata;
        out->imagecoll.images.back().init_orientation(image_orien_r,image_orien_c);

        const auto img_chnls = static_cast<long int>(channelsNumber);
        out->imagecoll.images.back().init_buffer(image_rows, image_cols, img_chnls);

        const auto img_pxldz = image_thickness;
        const auto gvof_offset = static_cast<double>(*gfov_it);  //Offset along \hat{z} from 
        const auto img_offset = image_pos + image_stack_unit * gvof_offset;
        out->imagecoll.images.back().init_spatial(image_pxldx,image_pxldy,img_pxldz, image_anchor, img_offset);

        out->imagecoll.images.back().metadata["GridFrameOffset"] = std::to_string(gvof_offset);
        out->imagecoll.images.back().metadata["Frame"] = std::to_string(curr_frame);
        out->imagecoll.images.back().metadata["ImagePositionPatient"] = img_offset.to_string();


        const auto img_bits  = static_cast<unsigned int>(channelPixelSize*8); //16 bit, 32 bit, 8 bit, etc..
        if(img_bits != image_bits){
            throw std::domain_error("The number of bits in each channel varies between the DICOM header ("_s
                                   + std::to_string(image_bits) 
                                   + ") and the transformed image data ("_s
                                   + std::to_string(img_bits)
                                   + ")");
            //Not sure what to do if this happens. Perhaps just go with the imebra result?
        }

        //Write the data to our allocated memory.
        imbxUint32 data_index = 0;
        for(long int row = 0; row < image_rows; ++row){
            for(long int col = 0; col < image_cols; ++col){
                for(long int chnl = 0; chnl < img_chnls; ++chnl){
                    const auto DoubleChannelValue = myHandler->getDouble(data_index);
                    const float OutgoingPixelValue = static_cast<float>(DoubleChannelValue) 
                                                     * static_cast<float>(grid_scale);
                    out->imagecoll.images.back().reference(row,col,chnl) = OutgoingPixelValue;

                    ++data_index;
                } //Loop over channels.
            } //Loop over columns.
        } //Loop over rows.
    } //Loop over frames.

    return out;
}

//These 'shared' pointers will actually be unique. This routine just converts from unique to shared for you.
std::list<std::shared_ptr<Image_Array>>  Load_Dose_Arrays(const std::list<std::filesystem::path> &filenames){
    std::list<std::shared_ptr<Image_Array>> out;
    for(const auto & filename : filenames){
        out.push_back(Load_Dose_Array(filename));
    }
    return out;
}

//-------------------- Plans ------------------------
//This routine loads a radiotherapy plan file.
//
// See DICOM standard, RT Beams module (C.8.8.14).

std::unique_ptr<TPlan_Config> 
Load_TPlan_Config(const std::filesystem::path &FilenameIn){
    std::unique_ptr<TPlan_Config> out(new TPlan_Config());

    using namespace puntoexe;
    ptr<puntoexe::stream> readStream(new puntoexe::stream);
    readStream->openFile(FilenameIn.c_str(), std::ios::in);

    ptr<puntoexe::streamReader> reader(new puntoexe::streamReader(readStream));
    ptr<imebra::dataSet> base_node_ptr = imebra::codecs::codecFactory::getCodecFactory()->load(reader);


    const auto convert_first_to_string = [](const std::vector<std::string> &in) -> std::optional<std::string> {
        if(!in.empty()){
            return std::optional<std::string>(in.front());
        }
        return std::optional<std::string>();
    };

    const auto convert_first_to_long_int = [](const std::vector<std::string> &in) -> std::optional<long int> {
        if(!in.empty()){
            try{
                const auto res = std::stol(in.front());
                return std::optional<long int>(res);
            }catch(const std::exception &){}
        }
        return std::optional<long int>();
    };

    const auto convert_first_to_double = [](const std::vector<std::string> &in) -> std::optional<double> {
        if(!in.empty()){
            try{
                const auto res = std::stod(in.front());
                return std::optional<double>(res);
            }catch(const std::exception &){}
        }
        return std::optional<double>();
    };

    const auto convert_to_vec3_double = [](const std::vector<std::string> &in) -> std::optional<vec3<double>> {
        if(in.size() == 3){
            try{
                const auto x = std::stod(in.at(0));
                const auto y = std::stod(in.at(1));
                const auto z = std::stod(in.at(2));
                return std::optional<vec3<double>>( vec3<double>(x,y,z) );
            }catch(const std::exception &){}
        }
        return std::optional<vec3<double>>();
    };

    const auto convert_to_vector_double = [](const std::vector<std::string> &in) -> std::vector<double> {
        std::vector<double> out;
        for(const auto &x : in){
            try{
                const auto y = std::stod(x);
                out.emplace_back(y);
            }catch(const std::exception &){}
        }
        return out;
    };

    const auto insert_as_string_if_nonempty = []( std::map<std::string, std::string> &metadata,
                                                  puntoexe::ptr<puntoexe::imebra::dataSet> base_node_ptr,
                                                  path_node dicom_path,
                                                  const std::string name) -> void {

        auto res_str_vec = extract_tag_as_string(base_node_ptr, dicom_path);

        const auto ctrim = CANONICALIZE::TRIM_ENDS;
        bool add_sep = (metadata.count(name) != 0); // Controls whether a separator will be added.
        for(const auto &s : res_str_vec){
            const auto trimmed = Canonicalize_String2(s, ctrim);
            if(trimmed.empty()) continue;
            metadata[name] += (add_sep ? R"***(\)***"_s : ""_s) + trimmed;
            add_sep = true;
        }
        return;
    };


    // ------------------------------------------- General --------------------------------------------------
    out->metadata = get_metadata_top_level_tags(FilenameIn);
    out->metadata["Modality"] = "RTPLAN";

    // DoseReferenceSequence
    for(uint32_t i = 0; i < 100000; ++i){
        ptr<imebra::dataSet> seq_item_ptr = base_node_ptr->getSequenceItem(0x300a, 0, 0x0010, i);
        if(seq_item_ptr == nullptr) break;

        const std::string prfx = R"***(DoseReferenceSequence)***"_s + std::to_string(i) + R"***(/)***"_s;

        insert_as_string_if_nonempty(out->metadata, seq_item_ptr, {0x300a, 0x0012}, prfx + "DoseReferenceNumber");
        insert_as_string_if_nonempty(out->metadata, seq_item_ptr, {0x300a, 0x0013}, prfx + "DoseReferenceUID");

        insert_as_string_if_nonempty(out->metadata, seq_item_ptr, {0x300a, 0x0014}, prfx + "DoseReferenceStructureType");
        insert_as_string_if_nonempty(out->metadata, seq_item_ptr, {0x300a, 0x0016}, prfx + "DoseReferenceDescription");
        insert_as_string_if_nonempty(out->metadata, seq_item_ptr, {0x300a, 0x0020}, prfx + "DoseReferenceType");
        insert_as_string_if_nonempty(out->metadata, seq_item_ptr, {0x300a, 0x0026}, prfx + "TargetPrescriptionDose");

    } // DoseReferenceSequence


    // ToleranceTableSequence
    for(uint32_t i = 0; i < 100000; ++i){
        ptr<imebra::dataSet> seq_item_ptr = base_node_ptr->getSequenceItem(0x300a, 0, 0x0040, i);
        if(seq_item_ptr == nullptr) break;

        const std::string prfx = R"***(ToleranceTableSequence)***"_s + std::to_string(i) + R"***(/)***"_s;

        insert_as_string_if_nonempty(out->metadata, seq_item_ptr, {0x300a, 0x0042}, prfx + "ToleranceTableNumber");
        insert_as_string_if_nonempty(out->metadata, seq_item_ptr, {0x300a, 0x0043}, prfx + "ToleranceTableLabel");

        insert_as_string_if_nonempty(out->metadata, seq_item_ptr, {0x300a, 0x0044}, prfx + "GantryAngleTolerance");
        insert_as_string_if_nonempty(out->metadata, seq_item_ptr, {0x300a, 0x0046}, prfx + "BeamLimitingDeviceAngleTolerance");
        insert_as_string_if_nonempty(out->metadata, seq_item_ptr, {0x300a, 0x004c}, prfx + "PatientSupportAngleTolerance");
        insert_as_string_if_nonempty(out->metadata, seq_item_ptr, {0x300a, 0x0051}, prfx + "TableTopVerticalPositionTolerance");
        insert_as_string_if_nonempty(out->metadata, seq_item_ptr, {0x300a, 0x0052}, prfx + "TableTopLongitudinalPositionTolerance");
        insert_as_string_if_nonempty(out->metadata, seq_item_ptr, {0x300a, 0x0053}, prfx + "TableTopLateralPositionTolerance");

        // BeamLimitingDeviceToleranceSequence
        for(uint32_t j = 0; j < 100000; ++j){
            ptr<imebra::dataSet> sseq_item_ptr = seq_item_ptr->getSequenceItem(0x300a, 0, 0x0048, j);
            if(sseq_item_ptr == nullptr) break;

            const std::string pprfx = prfx + R"***(BeamLimitingDeviceToleranceSequence)***"_s + std::to_string(j) + R"***(/)***"_s;

            insert_as_string_if_nonempty(out->metadata, sseq_item_ptr, {0x300a, 0x004a}, pprfx + "BeamLimitingDevicePositionTolerance");
            insert_as_string_if_nonempty(out->metadata, sseq_item_ptr, {0x300a, 0x00b8}, pprfx + "RTBeamLimitingDeviceType");

        } // BeamLimitingDeviceToleranceSequence
    } // ToleranceTableSequence


    // FractionGroupSequence
    for(uint32_t i = 0; i < 100000; ++i){
        ptr<imebra::dataSet> seq_item_ptr = base_node_ptr->getSequenceItem(0x300a, 0, 0x0070, i);
        if(seq_item_ptr == nullptr) break;

        const std::string prfx = R"***(FractionGroupSequence)***"_s + std::to_string(i) + R"***(/)***"_s;

        insert_as_string_if_nonempty(out->metadata, seq_item_ptr, {0x300a, 0x0071}, prfx + "FractionGroupNumber");
        insert_as_string_if_nonempty(out->metadata, seq_item_ptr, {0x300a, 0x0078}, prfx + "NumberOfFractionsPlanned");
        insert_as_string_if_nonempty(out->metadata, seq_item_ptr, {0x300a, 0x0080}, prfx + "NumberOfBeams");
        insert_as_string_if_nonempty(out->metadata, seq_item_ptr, {0x300a, 0x00a0}, prfx + "NumberOfBrachyApplicationSetups");

        // ReferencedBeamSequence
        for(uint32_t j = 0; j < 100000; ++j){
            ptr<imebra::dataSet> sseq_item_ptr = seq_item_ptr->getSequenceItem(0x300c, 0, 0x0004, j);
            if(sseq_item_ptr == nullptr) break;

            const std::string pprfx = prfx + R"***(ControlPointSequence)***"_s + std::to_string(j) + R"***(/)***"_s;

            insert_as_string_if_nonempty(out->metadata, sseq_item_ptr, {0x300a, 0x0084}, pprfx + "BeamDose");
            insert_as_string_if_nonempty(out->metadata, sseq_item_ptr, {0x300a, 0x0086}, pprfx + "BeamMeterset");
            insert_as_string_if_nonempty(out->metadata, sseq_item_ptr, {0x300c, 0x0006}, pprfx + "ReferencedBeamNumber");

        } // ReferencedBeamSequence.
    } // FractionGroupSequence.


    // BeamSequence.
    for(uint32_t i = 0; i < 100000; ++i){
        ptr<imebra::dataSet> seq_item_ptr = base_node_ptr->getSequenceItem(0x300a, 0, 0x00b0, i);
        if(seq_item_ptr == nullptr) break;

        const std::string prfx = R"***(BeamSequence)***"_s + std::to_string(i) + R"***(/)***"_s;

        out->dynamic_states.emplace_back();
        Dynamic_Machine_State *dms = &(out->dynamic_states.back());

        insert_as_string_if_nonempty(dms->metadata, seq_item_ptr, {0x0008, 0x0070}, /*prfx +*/ "Manufacturer");
        insert_as_string_if_nonempty(dms->metadata, seq_item_ptr, {0x0008, 0x0080}, /*prfx +*/ "InstitutionName");
        insert_as_string_if_nonempty(dms->metadata, seq_item_ptr, {0x0008, 0x1040}, /*prfx +*/ "InstitutionalDepartmentName");
        insert_as_string_if_nonempty(dms->metadata, seq_item_ptr, {0x0008, 0x1090}, /*prfx +*/ "ManufacturerModelName");
        insert_as_string_if_nonempty(dms->metadata, seq_item_ptr, {0x0018, 0x1000}, /*prfx +*/ "DeviceSerialNumber");

        insert_as_string_if_nonempty(dms->metadata, seq_item_ptr, {0x300a, 0x00b2}, /*prfx +*/ "TreatmentMachineName");
        insert_as_string_if_nonempty(dms->metadata, seq_item_ptr, {0x300a, 0x00b3}, /*prfx +*/ "PrimaryDosimeterUnit"); // generally HU.
        insert_as_string_if_nonempty(dms->metadata, seq_item_ptr, {0x300a, 0x00b4}, /*prfx +*/ "SourceAxisDistance"); // in mm.

        insert_as_string_if_nonempty(dms->metadata, seq_item_ptr, {0x300a, 0x00c0}, /*prfx +*/ "BeamNumber");
        auto BeamNumberOpt  = convert_first_to_long_int( extract_tag_as_string(seq_item_ptr, {0x300a, 0x00c0}) );

        insert_as_string_if_nonempty(dms->metadata, seq_item_ptr, {0x300a, 0x00c2}, /*prfx +*/ "BeamName");
        auto BeamNameOpt = convert_first_to_string(   extract_tag_as_string(seq_item_ptr, {0x300a, 0x00c2}) );

        insert_as_string_if_nonempty(dms->metadata, seq_item_ptr, {0x300a, 0x00c4}, /*prfx +*/ "BeamType");
        insert_as_string_if_nonempty(dms->metadata, seq_item_ptr, {0x300a, 0x00c6}, /*prfx +*/ "RadiationType");
        insert_as_string_if_nonempty(dms->metadata, seq_item_ptr, {0x300a, 0x00ce}, /*prfx +*/ "TreatmentDeliveryType");

        insert_as_string_if_nonempty(dms->metadata, seq_item_ptr, {0x300a, 0x00d0}, /*prfx +*/ "NumberOfWedges");
        insert_as_string_if_nonempty(dms->metadata, seq_item_ptr, {0x300a, 0x00e0}, /*prfx +*/ "NumberOfCompensators");
        insert_as_string_if_nonempty(dms->metadata, seq_item_ptr, {0x300a, 0x00ed}, /*prfx +*/ "NumberOfBoli");
        insert_as_string_if_nonempty(dms->metadata, seq_item_ptr, {0x300a, 0x00f0}, /*prfx +*/ "NumberOfBlocks");

        insert_as_string_if_nonempty(dms->metadata, seq_item_ptr, {0x300a, 0x010e}, /*prfx +*/ "FinalCumulativeMetersetWeight");
        auto FinalCumulativeMetersetWeightOpt = convert_first_to_double(   extract_tag_as_string(seq_item_ptr, {0x300a, 0x010e}) );

        insert_as_string_if_nonempty(dms->metadata, seq_item_ptr, {0x300a, 0x0110}, /*prfx +*/ "NumberOfControlPoints");
        auto NumberOfControlPointsOpt = convert_first_to_long_int( extract_tag_as_string(seq_item_ptr, {0x300a, 0x0110}) );

        insert_as_string_if_nonempty(dms->metadata, seq_item_ptr, {0x300c, 0x006a}, /*prfx +*/ "ReferencedPatientSetupNumber");
        insert_as_string_if_nonempty(dms->metadata, seq_item_ptr, {0x300c, 0x00a0}, /*prfx +*/ "ReferencedToleranceTableNumber");

        if( !BeamNumberOpt
        ||  !BeamNameOpt 
        ||  !FinalCumulativeMetersetWeightOpt
        ||  !NumberOfControlPointsOpt ){
            throw std::runtime_error("RTPLAN is missing required data. Refusing to continue.");
        }

        dms->BeamNumber = BeamNumberOpt.value();
        dms->FinalCumulativeMetersetWeight = FinalCumulativeMetersetWeightOpt.value();

        // BeamLimitingDeviceSequence
        for(uint32_t j = 0; j < 100000; ++j){
            ptr<imebra::dataSet> sseq_item_ptr = seq_item_ptr->getSequenceItem(0x300a, 0, 0x00b6, j);
            if(sseq_item_ptr == nullptr) break;

            const std::string pprfx = prfx + R"***(BeamLimitingDeviceSequence)***"_s + std::to_string(j) + R"***(/)***"_s;

            insert_as_string_if_nonempty(dms->metadata, sseq_item_ptr, {0x300a, 0x00b8}, pprfx + "RTBeamLimitingDeviceType");
            insert_as_string_if_nonempty(dms->metadata, sseq_item_ptr, {0x300a, 0x00bc}, pprfx + "NumberOfLeafJawPairs");
            insert_as_string_if_nonempty(dms->metadata, sseq_item_ptr, {0x300a, 0x00be}, pprfx + "LeafPositionBoundaries");

        } // BeamLimitingDeviceSequence

        // PrimaryFluenceModeSequence
        for(uint32_t j = 0; j < 100000; ++j){
            ptr<imebra::dataSet> sseq_item_ptr = seq_item_ptr->getSequenceItem(0x3002, 0, 0x0050, j);
            if(sseq_item_ptr == nullptr) break;

            const std::string pprfx = prfx + R"***(PrimaryFluenceModeSequence)***"_s + std::to_string(j) + R"***(/)***"_s;

            insert_as_string_if_nonempty(dms->metadata, sseq_item_ptr, {0x3002, 0x0051}, pprfx + "FluenceMode");
            insert_as_string_if_nonempty(dms->metadata, sseq_item_ptr, {0x3002, 0x0052}, pprfx + "FluenceModeID");

        } // PrimaryFluenceModeSequence

        // ControlPointSequence.
        for(uint32_t j = 0; j < 100000; ++j){
            ptr<imebra::dataSet> sseq_item_ptr = seq_item_ptr->getSequenceItem(0x300a, 0, 0x0111, j);
            if(sseq_item_ptr == nullptr) break;

            const std::string pprfx = prfx + R"***(ControlPointSequence)***"_s + std::to_string(j) + R"***(/)***"_s;

            dms->static_states.emplace_back();
            Static_Machine_State *sms = &(dms->static_states.back());

            const auto nan = std::numeric_limits<double>::quiet_NaN();

            // Necessary elements.
            auto ControlPointIndexOpt        = convert_first_to_long_int( extract_tag_as_string(sseq_item_ptr, {0x300a, 0x0112}) );
            auto CumulativeMetersetWeightOpt = convert_first_to_double(   extract_tag_as_string(sseq_item_ptr, {0x300a, 0x0134}) );

            if( !ControlPointIndexOpt
            ||  !CumulativeMetersetWeightOpt ){
                throw std::runtime_error("RTPLAN has an invalid control point. Refusing to continue.");
            }

            // If the following elements are missing, they are considered unchanged from the previous time the element was present.

            insert_as_string_if_nonempty(sms->metadata, sseq_item_ptr, {0x300a, 0x0114}, pprfx + "NominalBeamEnergy");
            insert_as_string_if_nonempty(sms->metadata, sseq_item_ptr, {0x300a, 0x0115}, pprfx + "DoseRateSet");

            auto GantryAngleOpt                         = convert_first_to_double( extract_tag_as_string(sseq_item_ptr, {0x300a, 0x011e}) );
            auto GantryRotationDirectionOpt             = convert_first_to_string( extract_tag_as_string(sseq_item_ptr, {0x300a, 0x011f}) );

            auto BeamLimitingDeviceAngleOpt             = convert_first_to_double( extract_tag_as_string(sseq_item_ptr, {0x300a, 0x0120}) );
            auto BeamLimitingDeviceRotationDirectionOpt = convert_first_to_string( extract_tag_as_string(sseq_item_ptr, {0x300a, 0x0121}) );

            auto PatientSupportAngleOpt                 = convert_first_to_double( extract_tag_as_string(sseq_item_ptr, {0x300a, 0x0122}) );
            auto PatientSupportRotationDirectionOpt     = convert_first_to_string( extract_tag_as_string(sseq_item_ptr, {0x300a, 0x0123}) );

            auto TableTopEccentricAngleOpt              = convert_first_to_double( extract_tag_as_string(sseq_item_ptr, {0x300a, 0x0125}) );
            auto TableTopEccentricRotationDirectionOpt  = convert_first_to_string( extract_tag_as_string(sseq_item_ptr, {0x300a, 0x0126}) );
            auto TableTopVerticalPositionOpt            = convert_first_to_double( extract_tag_as_string(sseq_item_ptr, {0x300a, 0x0128}) );
            auto TableTopLongitudinalPositionOpt        = convert_first_to_double( extract_tag_as_string(sseq_item_ptr, {0x300a, 0x0129}) );
            auto TableTopLateralPositionOpt             = convert_first_to_double( extract_tag_as_string(sseq_item_ptr, {0x300a, 0x012a}) );

            auto IsocentrePositionOpt                   = convert_to_vec3_double(  extract_tag_as_string(sseq_item_ptr, {0x300a, 0x012c}) );

            auto TableTopPitchAngleOpt                  = convert_first_to_double( extract_tag_as_string(sseq_item_ptr, {0x300a, 0x0140}) );
            auto TableTopPitchRotationDirectionOpt      = convert_first_to_string( extract_tag_as_string(sseq_item_ptr, {0x300a, 0x0142}) );

            auto TableTopRollAngleOpt                   = convert_first_to_double( extract_tag_as_string(sseq_item_ptr, {0x300a, 0x0144}) );
            auto TableTopRollRotationDirectionOpt       = convert_first_to_string( extract_tag_as_string(sseq_item_ptr, {0x300a, 0x0146}) );

            const auto dir_str_to_double = [](const std::string &s) -> double {
                const auto ctrim = CANONICALIZE::TRIM_ENDS | CANONICALIZE::TO_UPPER;
                const auto trimmed = Canonicalize_String2(s, ctrim);
                double out = std::numeric_limits<double>::quiet_NaN();
                if(trimmed == "NONE"){
                    out = 0.0;
                }else if(trimmed == "CCW"){
                    out = 1.0;
                }else if(trimmed == "CW"){
                    out = -1.0;
                }
                return out;
            };


            sms->ControlPointIndex = ControlPointIndexOpt.value();
            sms->CumulativeMetersetWeight = CumulativeMetersetWeightOpt.value();

            sms->GantryAngle = GantryAngleOpt.value_or(nan);
            sms->GantryRotationDirection = dir_str_to_double( GantryRotationDirectionOpt.value_or("") );

            sms->BeamLimitingDeviceAngle = BeamLimitingDeviceAngleOpt.value_or(nan);
            sms->BeamLimitingDeviceRotationDirection = dir_str_to_double( BeamLimitingDeviceRotationDirectionOpt.value_or("") );

            sms->PatientSupportAngle = PatientSupportAngleOpt.value_or(nan);
            sms->PatientSupportRotationDirection = dir_str_to_double( PatientSupportRotationDirectionOpt.value_or("") );

            sms->TableTopEccentricAngle = TableTopEccentricAngleOpt.value_or(nan);
            sms->TableTopEccentricRotationDirection = dir_str_to_double( TableTopEccentricRotationDirectionOpt.value_or("") );

            sms->TableTopVerticalPosition     = TableTopVerticalPositionOpt.value_or(nan);
            sms->TableTopLongitudinalPosition = TableTopLongitudinalPositionOpt.value_or(nan);
            sms->TableTopLateralPosition      = TableTopLateralPositionOpt.value_or(nan);

            sms->TableTopPitchAngle = TableTopPitchAngleOpt.value_or(nan);
            sms->TableTopPitchRotationDirection = dir_str_to_double( TableTopPitchRotationDirectionOpt.value_or("") );

            sms->TableTopRollAngle = TableTopRollAngleOpt.value_or(nan);
            sms->TableTopRollRotationDirection = dir_str_to_double( TableTopRollRotationDirectionOpt.value_or("") );

            sms->IsocentrePosition = IsocentrePositionOpt.value_or(vec3<double>(nan,nan,nan));

            // BeamLimitingDevicePositionSequence.
            for(uint32_t k = 0; k < 100000; ++k){
                ptr<imebra::dataSet> ssseq_item_ptr = sseq_item_ptr->getSequenceItem(0x300a, 0, 0x011a, k);
                if(ssseq_item_ptr == nullptr) break;

                auto RTBeamLimitingDeviceTypeOpt = convert_first_to_string(   extract_tag_as_string(ssseq_item_ptr, {0x300a, 0x00b8}) );
                auto LeafJawPositionsVec         = convert_to_vector_double(  extract_tag_as_string(ssseq_item_ptr, {0x300a, 0x011c}) );

                if( !RTBeamLimitingDeviceTypeOpt
                ||  LeafJawPositionsVec.empty()  ){
                    throw std::runtime_error("RTPLAN has an invalid beam limiting device position. Refusing to continue.");
                }

                const auto ctrim = CANONICALIZE::TRIM_ENDS | CANONICALIZE::TO_UPPER;
                const auto trimmed = Canonicalize_String2(RTBeamLimitingDeviceTypeOpt.value(), ctrim);

                if((trimmed == "ASYMX") || (trimmed == "X")){
                    sms->JawPositionsX = LeafJawPositionsVec;

                }else if((trimmed == "ASYMY") || (trimmed == "Y")){
                    sms->JawPositionsY = LeafJawPositionsVec;

                }else if(trimmed == "MLCX"){
                    sms->MLCPositionsX = LeafJawPositionsVec;

                }
            } // BeamLimitingDevicePositionSequence.

            // ReferencedDoseReferenceSequence
            for(uint32_t k = 0; k < 100000; ++k){
                ptr<imebra::dataSet> ssseq_item_ptr = sseq_item_ptr->getSequenceItem(0x300c, 0, 0x0050, k);
                if(ssseq_item_ptr == nullptr) break;

                const std::string ppprfx = pprfx + R"***(ReferencedDoseReferenceSequence)***"_s + std::to_string(k) + R"***(/)***"_s;

                insert_as_string_if_nonempty(sms->metadata, ssseq_item_ptr, {0x300a, 0x010c}, ppprfx + "CumulativeDoseReferenceCoefficient");
                insert_as_string_if_nonempty(sms->metadata, ssseq_item_ptr, {0x300c, 0x0051}, ppprfx + "ReferencedDoseReferenceNumber");

                // TODO: should implement this as a map<string,double> for easier differential comparison.

            } // ReferencedDoseReferenceSequence

        } // ControlPointSequence

    } // BeamSequence


    // Order the beams.
    std::sort( std::begin(out->dynamic_states),
               std::end(out->dynamic_states),
               [](const Dynamic_Machine_State &L, const Dynamic_Machine_State &R) -> bool {
                   return (L.BeamNumber < R.BeamNumber);
               });

    // Ensure control points are ordered correctly.
    for(auto &ds : out->dynamic_states){
        ds.sort_states();
    }

    // Ensure there are no missing control points.
    for(const auto &ds : out->dynamic_states){
        if(ds.static_states.size() < 2){
            throw std::runtime_error("Insufficient number of control points. Refusing to continue.");
        }
        if(!ds.verify_states_are_ordered()){
            throw std::runtime_error("A control point is missing. Refusing to continue.");
        }
    }

    // Propagate control point state explicitly.
    for(auto &ds : out->dynamic_states){
        ds.normalize_states();
    }

    // TODO: verify all states are finite (except for missing components, e.g., MLC positions on a machine not equipped with MLCs.

    return std::move(out);
}

//----------------- Registrations -------------------
//This routine loads a spatial registration.
//
// See DICOM standard, Spatial Registration Module (C.20.2).
std::unique_ptr<Transform3>
Load_Transform(const std::filesystem::path &FilenameIn){
    std::unique_ptr<Transform3> out(new Transform3());

    using namespace puntoexe;
    ptr<puntoexe::stream> readStream(new puntoexe::stream);
    readStream->openFile(FilenameIn.c_str(), std::ios::in);

    ptr<puntoexe::streamReader> reader(new puntoexe::streamReader(readStream));
    ptr<imebra::dataSet> base_node_ptr = imebra::codecs::codecFactory::getCodecFactory()->load(reader);


    const auto convert_first_to_string = [](const std::vector<std::string> &in) -> std::optional<std::string> {
        if(!in.empty()){
            return std::optional<std::string>(in.front());
        }
        return std::optional<std::string>();
    };

    const auto convert_first_to_long_int = [](const std::vector<std::string> &in) -> std::optional<long int> {
        if(!in.empty()){
            try{
                const auto res = std::stol(in.front());
                return std::optional<long int>(res);
            }catch(const std::exception &){}
        }
        return std::optional<long int>();
    };

    const auto convert_first_to_double = [](const std::vector<std::string> &in) -> std::optional<double> {
        if(!in.empty()){
            try{
                const auto res = std::stod(in.front());
                return std::optional<double>(res);
            }catch(const std::exception &){}
        }
        return std::optional<double>();
    };

    const auto convert_to_vec3_double = [](const std::vector<std::string> &in) -> std::optional<vec3<double>> {
        if(in.size() == 3){
            try{
                const auto x = std::stod(in.at(0));
                const auto y = std::stod(in.at(1));
                const auto z = std::stod(in.at(2));
                return std::optional<vec3<double>>( vec3<double>(x,y,z) );
            }catch(const std::exception &){}
        }
        return std::optional<vec3<double>>();
    };

    const auto convert_to_vec3_int64 = [](const std::vector<std::string> &in) -> std::optional<vec3<int64_t>> {
        if(in.size() == 3){
            try{
                const auto x = std::stol(in.at(0));
                const auto y = std::stol(in.at(1));
                const auto z = std::stol(in.at(2));
                return std::optional<vec3<int64_t>>( vec3<int64_t>(x,y,z) );
            }catch(const std::exception &){}
        }
        return std::optional<vec3<int64_t>>();
    };

    const auto convert_to_vector_double = [](const std::vector<std::string> &in) -> std::vector<double> {
        std::vector<double> out;
        for(const auto &x : in){
            try{
                const auto y = std::stod(x);
                out.emplace_back(y);
            }catch(const std::exception &){}
        }
        return out;
    };

    const auto insert_as_string_if_nonempty = []( std::map<std::string, std::string> &metadata,
                                                  puntoexe::ptr<puntoexe::imebra::dataSet> base_node_ptr,
                                                  path_node dicom_path,
                                                  const std::string name) -> void {

        auto res_str_vec = extract_tag_as_string(base_node_ptr, dicom_path);

        const auto ctrim = CANONICALIZE::TRIM_ENDS;
        bool add_sep = (metadata.count(name) != 0); // Controls whether a separator will be added.
        for(const auto &s : res_str_vec){
            const auto trimmed = Canonicalize_String2(s, ctrim);
            if(trimmed.empty()) continue;
            metadata[name] += (add_sep ? R"***(\)***"_s : ""_s) + trimmed;
            add_sep = true;
        }
        return;
    };

    const auto extract_affine_transform = [](const std::vector<double> &v) -> affine_transform<double> {
        affine_transform<double> t;
        if(v.size() != 16){
            throw std::runtime_error("Unanticipated matrix transformation dimensions");
        }
        for(size_t row = 0; row < 3; ++row){
            for(size_t col = 0; col < 4; ++col){
                const size_t indx = col + row * 4;
                t.coeff(row,col) = v[indx];
            }
        }
        if(  (v[12] != 0.0)
        ||   (v[13] != 0.0)
        ||   (v[14] != 0.0)
        ||   (v[15] != 1.0) ){
            throw std::runtime_error("Transformation cannot be represented using an Affine matrix");
        }
        return t;
    };

    // ------------------------------------------- General --------------------------------------------------
    out->metadata = get_metadata_top_level_tags(FilenameIn);
    out->metadata["Modality"] = "REG";

    // --------------------------------------- Rigid / Affine -----------------------------------------------
    // RegistrationSequence
    for(uint32_t i = 0; i < 100000; ++i){
        ptr<imebra::dataSet> seq_item_ptr = base_node_ptr->getSequenceItem(0x0070, 0, 0x0308, i);
        if(seq_item_ptr == nullptr) break;
        const std::string prfx = R"***(RegistrationSequence)***"_s + std::to_string(i) + R"***(/)***"_s;

        insert_as_string_if_nonempty(out->metadata, seq_item_ptr, {0x0020, 0x0052}, prfx + "FrameOfReferenceUID");

        // MatrixRegistrationSequence
        for(uint32_t j = 0; j < 100000; ++j){
            ptr<imebra::dataSet> sseq_item_ptr = seq_item_ptr->getSequenceItem(0x0070, 0, 0x0309, j);
            if(sseq_item_ptr == nullptr) break;
            const std::string pprfx = prfx + R"***(MatrixRegistrationSequence)***"_s + std::to_string(j) + R"***(/)***"_s;

            // MatrixSequence
            for(uint32_t k = 0; k < 100000; ++k){
                ptr<imebra::dataSet> ssseq_item_ptr = sseq_item_ptr->getSequenceItem(0x0070, 0, 0x030a, k);
                if(ssseq_item_ptr == nullptr) break;
                const std::string ppprfx = pprfx + R"***(MatrixSequence)***"_s + std::to_string(k) + R"***(/)***"_s;

                insert_as_string_if_nonempty(out->metadata, ssseq_item_ptr, {0x0070, 0x030c}, ppprfx + "FrameOfReferenceTransformationMatrixType");

                const auto FrameOfReferenceTransformationMatrix = convert_to_vector_double( extract_tag_as_string(ssseq_item_ptr, {0x3006, 0x00c6}) );
                if(FrameOfReferenceTransformationMatrix.size() != 16) continue;

                auto t = extract_affine_transform(FrameOfReferenceTransformationMatrix);
                out->transform = t;

            } // MatrixSequence.
        } // MatrixRegistrationSequence.
    } // RegistrationSequence.

    // ------------------------------------------- Deformable -----------------------------------------------
    // DeformableRegistrationSequence
    for(uint32_t i = 0; i < 100000; ++i){
        ptr<imebra::dataSet> seq_item_ptr = base_node_ptr->getSequenceItem(0x0064, 0, 0x0002, i);
        if(seq_item_ptr == nullptr) break;
        const std::string prfx = R"***(DeformableRegistrationSequence)***"_s + std::to_string(i) + R"***(/)***"_s;

        insert_as_string_if_nonempty(out->metadata, seq_item_ptr, {0x0064, 0x0003}, prfx + "SourceFrameOfReferenceUID");

        // DeformableRegistrationGridSequence
        for(uint32_t j = 0; j < 100000; ++j){
            ptr<imebra::dataSet> sseq_item_ptr = seq_item_ptr->getSequenceItem(0x0064, 0, 0x0005, j);
            if(sseq_item_ptr == nullptr) break;
            const std::string pprfx = prfx + R"***(DeformableRegistrationGridSequence)***"_s + std::to_string(j) + R"***(/)***"_s;

            //insert_as_string_if_nonempty(out->metadata, sseq_item_ptr, {0x0020, 0x0032}, pprfx + "ImagePositionPatient");
            //insert_as_string_if_nonempty(out->metadata, sseq_item_ptr, {0x0020, 0x0037}, pprfx + "ImageOrientationPatient");
            //insert_as_string_if_nonempty(out->metadata, sseq_item_ptr, {0x0064, 0x0007}, pprfx + "GridDimensions");
            //insert_as_string_if_nonempty(out->metadata, sseq_item_ptr, {0x0064, 0x0008}, pprfx + "GridResolution");
            const auto ImagePositionPatient    = convert_to_vec3_double( extract_tag_as_string(sseq_item_ptr, {0x0020, 0x0032}) );
            const auto ImageOrientationPatient = convert_to_vector_double( extract_tag_as_string(sseq_item_ptr, {0x0020, 0x0037}) );
            const auto GridDimensions = convert_to_vec3_int64( extract_tag_as_string(sseq_item_ptr, {0x0064, 0x0007}) );
            const auto GridResolution = convert_to_vec3_double( extract_tag_as_string(sseq_item_ptr, {0x0064, 0x0008}) );

            // Prepare data in format similar to image loading.
            const auto zero = vec3<double>(0.0, 0.0, 0.0);
            const auto zeroL = vec3<int64_t>(0, 0, 0);
            const auto image_pos = ImagePositionPatient.value();

            if(ImageOrientationPatient.size() != 6){
                throw std::runtime_error("Invalid ImageOrientationPatient tag");
            }
            const auto image_orien_c = vec3<double>(ImageOrientationPatient[0],
                                                    ImageOrientationPatient[1],
                                                    ImageOrientationPatient[2]).unit();
            const auto image_orien_r = vec3<double>(ImageOrientationPatient[3],
                                                    ImageOrientationPatient[4],
                                                    ImageOrientationPatient[5]).unit();
            const auto image_ortho = image_orien_c.Cross(image_orien_r).unit();

            const auto image_rows = GridDimensions.value_or(zeroL).y;
            const auto image_cols = GridDimensions.value_or(zeroL).x;
            const auto image_chns = static_cast<int64_t>(3);
            const auto image_imgs = GridDimensions.value_or(zeroL).z;

            const auto image_buffer_length = image_rows * image_cols * image_chns * image_imgs;
            if(image_buffer_length <= 0){
                throw std::runtime_error("Invalid image buffer dimensions");
            }

            const auto image_pxldy = GridResolution.value_or(zero).x;
            const auto image_pxldx = GridResolution.value_or(zero).y;
            const auto image_pxldz = GridResolution.value_or(zero).z;

            const auto voxel_volume = image_pxldy * image_pxldx * image_pxldz;
            if(voxel_volume <= 0.0){
                throw std::runtime_error("Invalid grid voxel dimensions");
            }

            const vec3<double> image_anchor = vec3<double>(0.0,0.0,0.0);

            //insert_as_string_if_nonempty(out->metadata, sseq_item_ptr, {0x0064, 0x0009}, pprfx + "VectorGridData");
            // ...
            // TODO: convert the VectorGridData to a deformation_field.
            // ...
            //out->transform = t;
            const auto VectorGridData = convert_to_vector_double( extract_tag_as_string(sseq_item_ptr, {0x0064, 0x0009}) );
            if(static_cast<int64_t>(VectorGridData.size()) != image_buffer_length){
                throw std::runtime_error("Encountered incomplete VectorGridData tag");
            }

            auto v_it = std::begin(VectorGridData);
            planar_image_collection<double,double> pic;
            for(int64_t n = 0; n < image_imgs; ++n){
                pic.images.emplace_back();
                pic.images.back().init_orientation(image_orien_r, image_orien_c);
                pic.images.back().init_buffer(image_rows, image_cols, image_chns);
                const auto image_offset = image_pos + image_ortho * n;
                pic.images.back().init_spatial(image_pxldx, image_pxldy, image_pxldz, image_anchor, image_offset);

                for(int64_t row = 0; row < image_rows; ++row){
                    for(int64_t col = 0; col < image_cols; ++col){
                        for(int64_t chn = 0; chn < image_chns; ++chn, ++v_it){
                            pic.images.back().reference(row, col, chn) = *v_it;
                        }
                    }
                }
            }
            FUNCINFO("Loaded deformation field with dimensions " << image_rows << " x " << image_cols << " x " << image_imgs);
            deformation_field t(std::move(pic));
            out->transform = t;

            // PreDeformationMatrixRegistrationSequence
            for(uint32_t k = 0; k < 100000; ++k){
                ptr<imebra::dataSet> ssseq_item_ptr = sseq_item_ptr->getSequenceItem(0x0064, 0, 0x000f, k);
                if(ssseq_item_ptr == nullptr) break;
                const std::string ppprfx = pprfx + R"***(PreDeformationMatrixRegistrationSequence)***"_s + std::to_string(k) + R"***(/)***"_s;

                insert_as_string_if_nonempty(out->metadata, ssseq_item_ptr, {0x0070, 0x030c}, ppprfx + "FrameOfReferenceTransformationMatrixType");

                const auto FrameOfReferenceTransformationMatrix = convert_to_vector_double( extract_tag_as_string(ssseq_item_ptr, {0x3006, 0x00c6}) );
                if(FrameOfReferenceTransformationMatrix.size() != 16) continue;
                auto t = extract_affine_transform(FrameOfReferenceTransformationMatrix);
                //out->pre_transform = t;
            } // PreDeformationMatrixRegistrationSequence.

            // PostDeformationMatrixRegistrationSequence
            for(uint32_t k = 0; k < 100000; ++k){
                ptr<imebra::dataSet> ssseq_item_ptr = sseq_item_ptr->getSequenceItem(0x0064, 0, 0x0010, k);
                if(ssseq_item_ptr == nullptr) break;
                const std::string ppprfx = pprfx + R"***(PostDeformationMatrixRegistrationSequence)***"_s + std::to_string(k) + R"***(/)***"_s;

                insert_as_string_if_nonempty(out->metadata, ssseq_item_ptr, {0x0070, 0x030c}, ppprfx + "FrameOfReferenceTransformationMatrixType");

                const auto FrameOfReferenceTransformationMatrix = convert_to_vector_double( extract_tag_as_string(ssseq_item_ptr, {0x3006, 0x00c6}) );
                if(FrameOfReferenceTransformationMatrix.size() != 16) continue;
                auto t = extract_affine_transform(FrameOfReferenceTransformationMatrix);
                //out->post_transform = t;
            } // PostDeformationMatrixRegistrationSequence.

        } // DeformableRegistrationGridSequence.
    } // DeformableRegistrationSequence.

    return std::move(out);
}


//This routine writes contiguous images to a single DICOM dose file.
//
// NOTE: Images are assumed to be contiguous and non-overlapping. They are also assumed to share image characteristics,
//       such as number of rows, number of columns, voxel dimensions/extent, orientation, and geometric origin.
// 
// NOTE: Currently only the first channel is considered. Additional channels were not needed at the time of writing.
//       It would probably be best to export one channel per file if multiple channels were needed. Though it is
//       possible to have up to 3 samples per voxel, IIRC, it may complicate encoding significantly. (Not sure.)
// 
// NOTE: This routine will reorder images.
//
// NOTE: Images containing NaN's will probably be rejected by most programs! Filter them out beforehand.
//
// NOTE: Exported files were tested successfully with Varian Eclipse v11. A valid DICOM file is needed to link
//       existing UIDs. Images created from scratch and lacking, e.g., a valid FrameOfReferenceUID, have not been
//       tested.
//
// NOTE: Some round-off should be expected. It is necessary because the TransferSyntax appears to require integer voxel
//       intensities which are scaled by a floating-point number to get the final dose. There are facilities for
//       exchanging floating-point-valued images in DICOM, but portability would be suspect.
//
void Write_Dose_Array(const std::shared_ptr<Image_Array>& IA, const std::filesystem::path &FilenameOut, ParanoiaLevel Paranoia){
    if( (IA == nullptr) 
    ||  IA->imagecoll.images.empty()){
        throw std::runtime_error("No images provided for export. Cannot continue.");
    }

    using namespace puntoexe;
    ptr<imebra::dataSet> tds(new imebra::dataSet);

    //Gather some basic info. Note that the following dimensions must be identical for all images for a multi-frame
    // RTDOSE file.
    const auto num_of_imgs = IA->imagecoll.images.size();
    const auto row_count = IA->imagecoll.images.front().rows;
    const auto col_count = IA->imagecoll.images.front().columns;

    auto max_dose = -std::numeric_limits<float>::infinity();
    for(const auto &p_img : IA->imagecoll.images){
        const long int channel = 0; // Ignore other channels for now. TODO.
        for(long int r = 0; r < row_count; r++){
            for(long int c = 0; c < col_count; c++){
                const auto val = p_img.value(r, c, channel);
                if(!std::isfinite(val)) throw std::domain_error("Found non-finite dose. Refusing to export.");
                if(val < 0.0f ) throw std::domain_error("Found a voxel with negative dose. Refusing to continue.");
                if(max_dose < val) max_dose = val;
            }
        }
    }
    if( max_dose < 0.0f ) throw std::invalid_argument("No voxels were found to export. Cannot continue.");
    const double full_dose_scaling = max_dose / static_cast<double>(std::numeric_limits<uint32_t>::max());
    const double dose_scaling = std::max(full_dose_scaling, 1.0E-5); //Because excess bits might get truncated!

    const auto pxl_dx = IA->imagecoll.images.front().pxl_dx;
    const auto pxl_dy = IA->imagecoll.images.front().pxl_dy;
    const auto PixelSpacing = std::to_string(pxl_dy) + R"***(\)***"_s + std::to_string(pxl_dx);

    const auto row_unit = IA->imagecoll.images.front().row_unit;
    const auto col_unit = IA->imagecoll.images.front().col_unit;
    const auto ortho_unit = col_unit.Cross(row_unit);
    const auto ImageOrientationPatient = std::to_string(col_unit.x) + R"***(\)***"_s
                                       + std::to_string(col_unit.y) + R"***(\)***"_s
                                       + std::to_string(col_unit.z) + R"***(\)***"_s
                                       + std::to_string(row_unit.x) + R"***(\)***"_s
                                       + std::to_string(row_unit.y) + R"***(\)***"_s
                                       + std::to_string(row_unit.z);

    //Re-order images so they are in spatial order with the 'bottom' defined in terms of row and column units.
    IA->imagecoll.Stable_Sort([&ortho_unit](const planar_image<float,double> &lhs, const planar_image<float,double> &rhs) -> bool {
        //Project each image onto the line defined by the ortho unit running through the origin. Sort by distance along
        //this line (i.e., the magnitude of the projection).
        if( (lhs.rows < 1) || (lhs.columns < 1) || (rhs.rows < 1) || (rhs.columns < 1) ){
            throw std::invalid_argument("Found an image containing no voxels. Refusing to continue."); //Just trim it?
        }
        return ( lhs.position(0,0).Dot(ortho_unit) < rhs.position(0,0).Dot(ortho_unit) );
    });

    const auto image_pos = IA->imagecoll.images.front().offset - IA->imagecoll.images.front().anchor;
    const auto ImagePositionPatient = std::to_string(image_pos.x) + R"***(\)***"_s
                                    + std::to_string(image_pos.y) + R"***(\)***"_s
                                    + std::to_string(image_pos.z);

    //Assume images abut (i.e., are contiguous) and perfectly parallel (i.e., the voxels form a rectilinear grid).
    const auto pxl_dz = IA->imagecoll.images.front().pxl_dz;
    const auto SliceThickness = std::to_string(pxl_dz);
    std::string GridFrameOffsetVector;
    for(size_t i = 0; i < num_of_imgs; ++i){
        const double z = pxl_dz*i;
        if(GridFrameOffsetVector.empty()){
            GridFrameOffsetVector += std::to_string(z);
        }else{
            GridFrameOffsetVector += R"***(\)***"_s + std::to_string(z);
        }
    }

    auto fne = [](std::vector<std::string> l) -> std::string {
        //fne == "First non-empty". Note this routine will throw if all provided strings are empty.
        for(auto &s : l) if(!s.empty()) return s;
        throw std::runtime_error("All inputs were empty -- unable to provide a nonempty string.");
        return std::string();
    };

    auto foe = [](std::vector<std::string> l) -> std::string {
        //foe == "First non-empty Or Empty". (i.e., will not throw if all provided strings are empty.)
        for(auto &s : l) if(!s.empty()) return s;
        return std::string(); 
    };

    //Specify the list of acceptable character sets.
    {
        imebra::charsetsList::tCharsetsList suitableCharsets;
        suitableCharsets.emplace_back(L"ISO_IR 100"); // "Latin alphabet 1"
        //suitableCharsets.push_back(L"ISO_IR 192"); // utf-8
        tds->setCharsetsList(&suitableCharsets);
    }

    //Top-level stuff: metadata shared by all images.
    {
        auto cm = IA->imagecoll.get_common_metadata({});

        //Replace any metadata that might be used to underhandedly link patients, if requested.
        if((Paranoia == ParanoiaLevel::Medium) || (Paranoia == ParanoiaLevel::High)){
            //SOP Common Module.
            cm["InstanceCreationDate"] = "";
            cm["InstanceCreationTime"] = "";
            cm["InstanceCreatorUID"]   = Generate_Random_UID(60);

            //Patient Module.
            cm["PatientsBirthDate"] = "";
            cm["PatientsGender"]    = "";
            cm["PatientsBirthTime"] = "";

            //General Study Module.
            cm["StudyInstanceUID"] = "";
            cm["StudyDate"] = "";
            cm["StudyTime"] = "";
            cm["ReferringPhysiciansName"] = "";
            cm["StudyID"] = "";
            cm["AccessionNumber"] = "";
            cm["StudyDescription"] = "";

            //General Series Module.
            cm["SeriesInstanceUID"] = "";
            cm["SeriesNumber"] = "";
            cm["SeriesDate"] = "";
            cm["SeriesTime"] = "";
            cm["SeriesDescription"] = "";
            cm["RequestedProcedureID"] = "";
            cm["ScheduledProcedureStepID"] = "";
            cm["OperatorsName"] = "";

            //Patient Study Module.
            cm["PatientsWeight"] = "";

            //Frame of Reference Module.
            cm["PositionReferenceIndicator"] = "";

            //General Equipment Module.
            cm["Manufacturer"] = "";
            cm["InstitutionName"] = "";
            cm["StationName"] = "";
            cm["InstitutionalDepartmentName"] = "";
            cm["ManufacturersModelName"] = "";
            cm["SoftwareVersions"] = "";

            //General Image Module.
            cm["ContentDate"] = "";
            cm["ContentTime"] = "";
            cm["AcquisitionNumber"] = "";
            cm["AcquisitionDate"] = "";
            cm["AcquisitionTime"] = "";
            cm["DerivationDescription"] = "";
            cm["ImagesInAcquisition"] = "";

            //RT Dose Module.
            cm[R"***(ReferencedRTPlanSequence/ReferencedSOPInstanceUID)***"] = "";
            if(0 != cm.count(R"***(ReferencedFractionGroupSequence/ReferencedFractionGroupNumber)***")){
                cm[R"***(ReferencedFractionGroupSequence/ReferencedFractionGroupNumber)***"] = "";
            }
            if(0 != cm.count(R"***(ReferencedBeamSequence/ReferencedBeamNumber)***")){
                cm[R"***(ReferencedBeamSequence/ReferencedBeamNumber)***"] = "";
            }
        }
        if(Paranoia == ParanoiaLevel::High){
            //Patient Module.
            cm["PatientsName"]      = "";
            cm["PatientID"]         = "";

            //Frame of Reference Module.
            cm["FrameOfReferenceUID"] = "";
        }


        //Generate some UIDs that need to be duplicated.
        const auto SOPInstanceUID = Generate_Random_UID(60);

        //DICOM Header Metadata.
        ds_OB_insert(tds, {0x0002, 0x0001},  std::string(1,static_cast<char>(0))
                                           + std::string(1,static_cast<char>(1)) ); //"FileMetaInformationVersion".
        //ds_insert(tds, {0x0002, 0x0001}, R"***(2/0/0/0/0/1)***"); //shtl); //"FileMetaInformationVersion".
        ds_insert(tds, {0x0002, 0x0002}, "1.2.840.10008.5.1.4.1.1.481.2"); //"MediaStorageSOPClassUID" (Radiation Therapy Dose Storage)
        ds_insert(tds, {0x0002, 0x0003}, SOPInstanceUID); //"MediaStorageSOPInstanceUID".
        ds_insert(tds, {0x0002, 0x0010}, "1.2.840.10008.1.2.1"); //"TransferSyntaxUID".
        ds_insert(tds, {0x0002, 0x0013}, "DICOMautomaton"); //"ImplementationVersionName".
        ds_insert(tds, {0x0002, 0x0012}, "1.2.513.264.765.1.1.578"); //"ImplementationClassUID".

        //SOP Common Module.
        ds_insert(tds, {0x0008, 0x0016}, "1.2.840.10008.5.1.4.1.1.481.2"); // "SOPClassUID"
        ds_insert(tds, {0x0008, 0x0018}, SOPInstanceUID); // "SOPInstanceUID"
        //ds_insert(tds, {0x0008, 0x0005}, "ISO_IR 100"); //fne({ cm["SpecificCharacterSet"], "ISO_IR 100" })); // Set above!
        ds_insert(tds, {0x0008, 0x0012}, fne({ cm["InstanceCreationDate"], "19720101" }));
        ds_insert(tds, {0x0008, 0x0013}, fne({ cm["InstanceCreationTime"], "010101" }));
        ds_insert(tds, {0x0008, 0x0014}, foe({ cm["InstanceCreatorUID"] }));
        ds_insert(tds, {0x0008, 0x0114}, foe({ cm["CodingSchemeExternalUID"] }));
        ds_insert(tds, {0x0020, 0x0013}, foe({ cm["InstanceNumber"] }));

        //Patient Module.
        ds_insert(tds, {0x0010, 0x0010}, fne({ cm["PatientsName"], "DICOMautomaton^DICOMautomaton" }));
        ds_insert(tds, {0x0010, 0x0020}, fne({ cm["PatientID"], "DCMA_"_s + Generate_Random_String_of_Length(10) }));
        ds_insert(tds, {0x0010, 0x0030}, fne({ cm["PatientsBirthDate"], "19720101" }));
        ds_insert(tds, {0x0010, 0x0040}, fne({ cm["PatientsGender"], "O" }));
        ds_insert(tds, {0x0010, 0x0032}, fne({ cm["PatientsBirthTime"], "010101" }));

        //General Study Module.
        ds_insert(tds, {0x0020, 0x000D}, fne({ cm["StudyInstanceUID"], Generate_Random_UID(31) }));
        ds_insert(tds, {0x0008, 0x0020}, fne({ cm["StudyDate"], "19720101" }));
        ds_insert(tds, {0x0008, 0x0030}, fne({ cm["StudyTime"], "010101" }));
        ds_insert(tds, {0x0008, 0x0090}, fne({ cm["ReferringPhysiciansName"], "UNSPECIFIED^UNSPECIFIED" }));
        ds_insert(tds, {0x0020, 0x0010}, fne({ cm["StudyID"], "DCMA_"_s + Generate_Random_String_of_Length(10) })); // i.e., "Course"
        ds_insert(tds, {0x0008, 0x0050}, fne({ cm["AccessionNumber"], Generate_Random_String_of_Length(14) }));
        ds_insert(tds, {0x0008, 0x1030}, fne({ cm["StudyDescription"], "UNSPECIFIED" }));

        //General Series Module.
        ds_insert(tds, {0x0008, 0x0060}, "RTDOSE");
        ds_insert(tds, {0x0020, 0x000E}, fne({ cm["SeriesInstanceUID"], Generate_Random_UID(31) }));
        ds_insert(tds, {0x0020, 0x0011}, fne({ cm["SeriesNumber"], Generate_Random_Int_Str(5000, 32767) })); // Upper: 2^15 - 1.
        ds_insert(tds, {0x0008, 0x0021}, foe({ cm["SeriesDate"] }));
        ds_insert(tds, {0x0008, 0x0031}, foe({ cm["SeriesTime"] }));
        ds_insert(tds, {0x0008, 0x103E}, fne({ cm["SeriesDescription"], "UNSPECIFIED" }));
        ds_insert(tds, {0x0018, 0x0015}, foe({ cm["BodyPartExamined"] }));
        ds_insert(tds, {0x0018, 0x5100}, foe({ cm["PatientPosition"] }));
        ds_insert(tds, {0x0040, 0x1001}, fne({ cm["RequestedProcedureID"], "UNSPECIFIED" }));
        ds_insert(tds, {0x0040, 0x0009}, fne({ cm["ScheduledProcedureStepID"], "UNSPECIFIED" }));
        ds_insert(tds, {0x0008, 0x1070}, fne({ cm["OperatorsName"], "UNSPECIFIED" }));

        //Patient Study Module.
        ds_insert(tds, {0x0010, 0x1030}, foe({ cm["PatientsWeight"] }));

        //Frame of Reference Module.
        ds_insert(tds, {0x0020, 0x0052}, fne({ cm["FrameOfReferenceUID"], Generate_Random_UID(32) }));
        ds_insert(tds, {0x0020, 0x1040}, fne({ cm["PositionReferenceIndicator"], "BB" }));

        //General Equipment Module.
        ds_insert(tds, {0x0008, 0x0070}, fne({ cm["Manufacturer"], "UNSPECIFIED" }));
        ds_insert(tds, {0x0008, 0x0080}, fne({ cm["InstitutionName"], "UNSPECIFIED" }));
        ds_insert(tds, {0x0008, 0x1010}, fne({ cm["StationName"], "UNSPECIFIED" }));
        ds_insert(tds, {0x0008, 0x1040}, fne({ cm["InstitutionalDepartmentName"], "UNSPECIFIED" }));
        ds_insert(tds, {0x0008, 0x1090}, fne({ cm["ManufacturersModelName"], "UNSPECIFIED" }));
        ds_insert(tds, {0x0018, 0x1020}, fne({ cm["SoftwareVersions"], "UNSPECIFIED" }));

        //General Image Module.
        ds_insert(tds, {0x0020, 0x0013}, foe({ cm["InstanceNumber"] }));
        //ds_insert(tds, {0x0020, 0x0020}, fne({ cm["PatientOrientation"], "UNSPECIFIED" }));
        ds_insert(tds, {0x0008, 0x0023}, foe({ cm["ContentDate"] }));
        ds_insert(tds, {0x0008, 0x0033}, foe({ cm["ContentTime"] }));
        //ds_insert(tds, {0x0008, 0x0008}, fne({ cm["ImageType"], "UNSPECIFIED" }));
        ds_insert(tds, {0x0020, 0x0012}, foe({ cm["AcquisitionNumber"] }));
        ds_insert(tds, {0x0008, 0x0022}, foe({ cm["AcquisitionDate"] }));
        ds_insert(tds, {0x0008, 0x0032}, foe({ cm["AcquisitionTime"] }));
        ds_insert(tds, {0x0008, 0x2111}, foe({ cm["DerivationDescription"] }));
        //insert_as_string_if_nonempty({0x0008, 0x9215}, "DerivationCodeSequence"], "" }));
        ds_insert(tds, {0x0020, 0x1002}, foe({ cm["ImagesInAcquisition"] }));
        ds_insert(tds, {0x0020, 0x4000}, "Research image generated by DICOMautomaton. Not for clinical use!" ); //"ImageComments".
        ds_insert(tds, {0x0028, 0x0300}, foe({ cm["QualityControlImage"] }));

        //Image Plane Module.
        ds_insert(tds, {0x0028, 0x0030}, PixelSpacing );
        ds_insert(tds, {0x0020, 0x0037}, ImageOrientationPatient );
        ds_insert(tds, {0x0020, 0x0032}, ImagePositionPatient );
        ds_insert(tds, {0x0018, 0x0050}, SliceThickness );
        ds_insert(tds, {0x0020, 0x1041}, "" ); // foe({ cm["SliceLocation"] }));

        //Image Pixel Module.
        ds_insert(tds, {0x0028, 0x0002}, fne({ cm["SamplesPerPixel"], "1" }));
        ds_insert(tds, {0x0028, 0x0004}, fne({ cm["PhotometricInterpretation"], "MONOCHROME2" }));
        ds_insert(tds, {0x0028, 0x0010}, fne({ std::to_string(row_count) })); // "Rows"
        ds_insert(tds, {0x0028, 0x0011}, fne({ std::to_string(col_count) })); // "Columns"
        ds_insert(tds, {0x0028, 0x0100}, "32" ); //fne({ cm["BitsAllocated"], "32" }));
        ds_insert(tds, {0x0028, 0x0101}, "32" ); //fne({ cm["BitsStored"], "32" }));
        ds_insert(tds, {0x0028, 0x0102}, "31" ); //fne({ cm["HighBit"], "31" }));
        ds_insert(tds, {0x0028, 0x0103}, "0" ); // Unsigned.   fne({ cm["PixelRepresentation"], "0" }));
        ds_insert(tds, {0x0028, 0x0006}, foe({ cm["PlanarConfiguration"] }));
        ds_insert(tds, {0x0028, 0x0034}, foe({ cm["PixelAspectRatio"] }));

        //Multi-Frame Module.
        ds_insert(tds, {0x0028, 0x0008}, fne({ std::to_string(num_of_imgs) })); // "NumberOfFrames".
        ds_insert(tds, {0x0028, 0x0009}, fne({ cm["FrameIncrementPointer"], // Default to (3004,000c).
                                               R"***(12292\12)***" })); // Imebra default deserialization, but is brittle and depends on endianness.
                                               //"\x04\x30\x0c\x00" })); // Imebra won't accept this...
        ds_insert(tds, {0x3004, 0x000c}, GridFrameOffsetVector );

        //Modality LUT Module.
        //insert_as_string_if_nonempty(0x0028, 0x3000, "ModalityLUTSequence"], "" }));
        ds_insert(tds, {0x0028, 0x3002}, foe({ cm["LUTDescriptor"] }));
        ds_insert(tds, {0x0028, 0x3004}, foe({ cm["ModalityLUTType"] }));
        ds_insert(tds, {0x0028, 0x3006}, foe({ cm["LUTData"] }));
        //ds_insert(tds, {0x0028, 0x1052}, foe({ cm["RescaleIntercept"] })); // These force interpretation by Imebra
        //ds_insert(tds, {0x0028, 0x1053}, foe({ cm["RescaleSlope"] }));     //  as 8 byte pixel depth, regardless of
        //ds_insert(tds, {0x0028, 0x1054}, foe({ cm["RescaleType"] }));      //  the actual depth (@ current settings).

        //RT Dose Module.
        //ds_insert(tds, {0x0028, 0x0002}, fne({ cm["SamplesPerPixel"], "1" }));
        //ds_insert(tds, {0x0028, 0x0004}, fne({ cm["PhotometricInterpretation"], "MONOCHROME2" }));
        //ds_insert(tds, {0x0028, 0x0100}, fne({ cm["BitsAllocated"], "32" }));
        //ds_insert(tds, {0x0028, 0x0101}, fne({ cm["BitsStored"], "32" }));
        //ds_insert(tds, {0x0028, 0x0102}, fne({ cm["HighBit"], "31" }));
        //ds_insert(tds, {0x0028, 0x0103}, fne({ cm["PixelRepresentation"], "0" }));
        ds_insert(tds, {0x3004, 0x0002}, fne({ cm["DoseUnits"], "GY" }));
        ds_insert(tds, {0x3004, 0x0004}, fne({ cm["DoseType"], "PHYSICAL" }));
        ds_insert(tds, {0x3004, 0x000a}, fne({ cm["DoseSummationType"], "PLAN" }));
        ds_insert(tds, {0x3004, 0x000e}, std::to_string(dose_scaling) ); //"DoseGridScaling"

        ds_seq_insert(tds, {0x300C, 0x0002},   // "ReferencedRTPlanSequence" 
                           {0x0008, 0x1150}, // "ReferencedSOPClassUID"
                           fne({ cm[R"***(ReferencedRTPlanSequence/ReferencedSOPClassUID)***"],
                                 "1.2.840.10008.5.1.4.1.1.481.5" }) ); // "RTPlanStorage". Prefer existing UID.
        ds_seq_insert(tds, {0x300C, 0x0002},   // "ReferencedRTPlanSequence"
                           {0x0008, 0x1155}, // "ReferencedSOPInstanceUID"
                           fne({ cm[R"***(ReferencedRTPlanSequence/ReferencedSOPInstanceUID)***"],
                                 Generate_Random_UID(32) }) );
  
        if(0 != cm.count(R"***(ReferencedFractionGroupSequence/ReferencedFractionGroupNumber)***")){
            ds_seq_insert(tds, {0x300C, 0x0020},   // "ReferencedFractionGroupSequence"
                               {0x300C, 0x0022}, // "ReferencedFractionGroupNumber"
                               foe({ cm[R"***(ReferencedFractionGroupSequence/ReferencedFractionGroupNumber)***"] }) );
        }

        if(0 != cm.count(R"***(ReferencedBeamSequence/ReferencedBeamNumber)***")){
            ds_seq_insert(tds, {0x300C, 0x0004},   // "ReferencedBeamSequence"
                               {0x300C, 0x0006}, // "ReferencedBeamNumber"
                               foe({ cm[R"***(ReferencedBeamSequence/ReferencedBeamNumber)***"] }) );
        }
    }

    //Insert the raw pixel data.
    std::vector<uint32_t> shtl;
    shtl.reserve(num_of_imgs * col_count * row_count);
    for(const auto &p_img : IA->imagecoll.images){

        //Convert each pixel to the required format, scaling by the dose factor as needed.
        const long int channel = 0; // Ignore other channels for now. TODO.
        for(long int r = 0; r < row_count; r++){
            for(long int c = 0; c < col_count; c++){
                const auto val = p_img.value(r, c, channel);
                const auto scaled = std::round( std::abs(val/dose_scaling) );
                auto as_uint = static_cast<uint32_t>(scaled);
                shtl.push_back(as_uint);
            }
        }
    }
    {
        auto tag_ptr = tds->getTag(0x7FE0, 0, 0x0010, true);
        //FUNCINFO("Re-reading the tag.  Type is " << tag_ptr->getDataType() << ",  #_of_buffers = " <<
        //     tag_ptr->getBuffersCount() << ",   buffer_0 has size = " << tag_ptr->getBufferSize(0));

        auto rdh_ptr = tag_ptr->getDataHandlerRaw(0, true, "OW");
        rdh_ptr->copyFromMemory(reinterpret_cast<const uint8_t *>(shtl.data()),
                                4*static_cast<uint32_t>(shtl.size()));
    }

    // Write the file.
    {  
        ptr<puntoexe::stream> outputStream(new puntoexe::stream);
        outputStream->openFile(FilenameOut, std::ios::out);
        ptr<streamWriter> writer(new streamWriter(outputStream));
        ptr<imebra::codecs::dicomCodec> writeCodec(new imebra::codecs::dicomCodec);
        writeCodec->write(writer, tds);
    }

    return;
}


//This routine writes an image array to several DICOM CT-modality files.
//
// Note: the user callback will be called once per file.
//
void Write_CT_Images(const std::shared_ptr<Image_Array>& IA, 
                     const std::function<void(std::istream &is,
                                        long int filesize)>& file_handler,
                     ParanoiaLevel Paranoia){
    if( (IA == nullptr) 
    ||  IA->imagecoll.images.empty()){
        throw std::invalid_argument("No images provided for export. Cannot continue.");
    }
    if(!file_handler){
        throw std::invalid_argument("File handler is invalid. Refusing to continue.");
    }

    auto fne = [](std::vector<std::string> l) -> std::string {
        //fne == "First non-empty". Note this routine will throw if all provided strings are empty.
        for(auto &s : l) if(!s.empty()) return s;
        throw std::runtime_error("All inputs were empty -- unable to provide a nonempty string.");
        return std::string();
    };

    auto foe = [](std::vector<std::string> l) -> std::string {
        //foe == "First non-empty Or Empty". (i.e., will not throw if all provided strings are empty.)
        for(auto &s : l) if(!s.empty()) return s;
        return std::string(); 
    };

    //Generate UIDs and IDs that need to be duplicated across all images.
    const auto FrameOfReferenceUID = Generate_Random_UID(60);
    const auto StudyInstanceUID = Generate_Random_UID(31);
    const auto SeriesInstanceUID = Generate_Random_UID(31);
    const auto InstanceCreatorUID = Generate_Random_UID(60);

    const auto PatientID = "DCMA_"_s + Generate_Random_String_of_Length(10);
    const auto StudyID = "DCMA_"_s + Generate_Random_String_of_Length(10); // i.e., "Course"
    const auto SeriesNumber = Generate_Random_Int_Str(5000, 32767); // Upper: 2^15 - 1.

    // TODO: Sample any existing UID (ReferencedFrameOfReferenceUID or FrameOfReferenceUID). 
    // Probably OK to use only the first in this case though...

    long int InstanceNumber = -1;
    for(const auto &animg : IA->imagecoll.images){
        if( (animg.rows <= 0) || (animg.columns <= 0) || (animg.channels <= 0) ){
            continue;
        }

        ++InstanceNumber;

        DCMA_DICOM::Encoding enc = DCMA_DICOM::Encoding::ELE;
        DCMA_DICOM::Node root_node;

        const auto SOPInstanceUID = Generate_Random_UID(60);

        //auto cm = IA->imagecoll.get_common_metadata({});
        auto cm = animg.metadata;

        //Replace any metadata that might be used to underhandedly link patients, if requested.
        if((Paranoia == ParanoiaLevel::Medium) || (Paranoia == ParanoiaLevel::High)){
            //SOP Common Module.
            cm["InstanceCreationDate"] = "";
            cm["InstanceCreationTime"] = "";
            cm["InstanceCreatorUID"] = InstanceCreatorUID;

            //Patient Module.
            cm["PatientsBirthDate"] = "";
            cm["PatientsGender"]    = "";
            cm["PatientsBirthTime"] = "";

            //General Study Module.
            cm["StudyInstanceUID"] = StudyInstanceUID;
            cm["StudyDate"] = "";
            cm["StudyTime"] = "";
            cm["ReferringPhysiciansName"] = "";
            cm["StudyID"] = StudyID;
            cm["AccessionNumber"] = "";
            cm["StudyDescription"] = "";

            //General Series Module.
            cm["SeriesInstanceUID"] = SeriesInstanceUID;
            cm["SeriesNumber"] = "";
            cm["SeriesDate"] = "";
            cm["SeriesTime"] = "";
            cm["SeriesDescription"] = "";
            cm["RequestedProcedureID"] = "";                          // Appropriate?
            cm["ScheduledProcedureStepID"] = "";                          // Appropriate?
            cm["OperatorsName"] = "";                          // Appropriate?

            //Patient Study Module.
            cm["PatientsWeight"] = "";                          // Appropriate?

            //Frame of Reference Module.
            cm["PositionReferenceIndicator"] = "";              // Appropriate?

            //General Equipment Module.
            cm["Manufacturer"] = "";
            cm["InstitutionName"] = "";             // Appropriate?
            cm["StationName"] = "";             // Appropriate?
            cm["InstitutionalDepartmentName"] = "";             // Appropriate?
            cm["ManufacturersModelName"] = "";
            cm["SoftwareVersions"] = "";
        }
        if(Paranoia == ParanoiaLevel::High){
            //Patient Module.
            cm["PatientsName"]      = "";
            cm["PatientID"]         = PatientID;

            //Frame of Reference Module.
            cm["FrameOfReferenceUID"] = FrameOfReferenceUID;
        }

        //-------------------------------------------------------------------------------------------------
        //DICOM Header Metadata.
        root_node.emplace_child_node({{0x0002, 0x0001}, "OB", std::string("\x0\x1", 2)}); // FileMetaInformationVersion
        root_node.emplace_child_node({{0x0002, 0x0002}, "UI", "1.2.840.10008.5.1.4.1.1.2"}); // MediaStorageSOPClassUID -- CT Image Storage.
        root_node.emplace_child_node({{0x0002, 0x0003}, "UI", SOPInstanceUID}); // MediaStorageSOPInstanceUID
        std::string TransferSyntaxUID;
        if(enc == DCMA_DICOM::Encoding::ELE){
            TransferSyntaxUID = "1.2.840.10008.1.2.1";
        }else if(enc == DCMA_DICOM::Encoding::ILE){
            TransferSyntaxUID = "1.2.840.10008.1.2";
        }else{
            throw std::runtime_error("Unsupported transfer syntax requested. Cannot continue.");
        }
        root_node.emplace_child_node({{0x0002, 0x0010}, "UI", TransferSyntaxUID}); // TransferSyntaxUID

        root_node.emplace_child_node({{0x0002, 0x0012}, "UI", "1.2.513.264.765.1.1.578"}); // ImplementationClassUID
        root_node.emplace_child_node({{0x0002, 0x0013}, "SH", "DICOMautomaton"}); // ImplementationVersionName

        //-------------------------------------------------------------------------------------------------
        //SOP Common Module.
        root_node.emplace_child_node({{0x0008, 0x0016}, "UI", "1.2.840.10008.5.1.4.1.1.2"}); // SOPClassUID -- CT Image Storage.
        root_node.emplace_child_node({{0x0008, 0x0018}, "UI", SOPInstanceUID}); // SOPInstanceUID
        root_node.emplace_child_node({{0x0008, 0x0005}, "CS", "ISO_IR 192"}); // 'ISO_IR 192' = UTF-8.
        root_node.emplace_child_node({{0x0008, 0x0012}, "DA", fne({ cm["InstanceCreationDate"], "19720101" }) });
        root_node.emplace_child_node({{0x0008, 0x0013}, "TM", fne({ cm["InstanceCreationTime"], "010101" }) });
        root_node.emplace_child_node({{0x0008, 0x0014}, "UI", foe({ cm["InstanceCreatorUID"] }) });
        //root_node.emplace_child_node({{0x0008, 0x0114}, "UI", foe({ cm["CodingSchemeExternalUID"] }) });                 // Appropriate?
        //root_node.emplace_child_node({{0x0020, 0x0013}, "IS", std::to_string(InstanceNumber) });

        //-------------------------------------------------------------------------------------------------
        //Patient Module.
        root_node.emplace_child_node({{0x0010, 0x0010}, "PN", fne({ cm["PatientsName"], "DICOMautomaton^DICOMautomaton" }) });
        root_node.emplace_child_node({{0x0010, 0x0020}, "LO", fne({ cm["PatientID"], PatientID }) });
        root_node.emplace_child_node({{0x0010, 0x0030}, "DA", fne({ cm["PatientsBirthDate"], "19720101" }) });
        root_node.emplace_child_node({{0x0010, 0x0040}, "CS", foe({ cm["PatientsSex"] }) });
        //root_node.emplace_child_node({{0x0010, 0x0032}, "TM", fne({ cm["PatientsBirthTime"], "010101" }) });

        //-------------------------------------------------------------------------------------------------
        //General Study Module.
        root_node.emplace_child_node({{0x0020, 0x000D}, "UI", fne({ cm["StudyInstanceUID"], StudyInstanceUID }) });
        root_node.emplace_child_node({{0x0008, 0x0020}, "DA", fne({ cm["StudyDate"], "19720101" }) });
        root_node.emplace_child_node({{0x0008, 0x0030}, "TM", fne({ cm["StudyTime"], "010101" }) });
        root_node.emplace_child_node({{0x0008, 0x0090}, "PN", fne({ cm["ReferringPhysiciansName"], "UNSPECIFIED^UNSPECIFIED" }) });
        root_node.emplace_child_node({{0x0020, 0x0010}, "SH", fne({ cm["StudyID"], StudyID }) });
        //root_node.emplace_child_node({{0x0008, 0x0050}, "SH", fne({ cm["AccessionNumber"], Generate_Random_String_of_Length(14) }) });
        root_node.emplace_child_node({{0x0008, 0x0050}, "SH", "" });
        root_node.emplace_child_node({{0x0008, 0x1030}, "LO", fne({ cm["StudyDescription"], "UNSPECIFIED" }) });

        //-------------------------------------------------------------------------------------------------
        //General Series Module.
        root_node.emplace_child_node({{0x0008, 0x0060}, "CS", "CT" }); // "Modality"
        root_node.emplace_child_node({{0x0020, 0x000E}, "UI", fne({ cm["SeriesInstanceUID"], SeriesInstanceUID }) });
        root_node.emplace_child_node({{0x0020, 0x0011}, "IS", fne({ cm["SeriesNumber"], SeriesNumber }) }); // Upper: 2^15 - 1.
        root_node.emplace_child_node({{0x0008, 0x0021}, "DA", foe({ cm["SeriesDate"] }) });
        root_node.emplace_child_node({{0x0008, 0x0031}, "TM", foe({ cm["SeriesTime"] }) });
        root_node.emplace_child_node({{0x0008, 0x103E}, "LO", fne({ cm["SeriesDescription"], "UNSPECIFIED" }) });
        //root_node.emplace_child_node({{0x0008, 0x1070}, "PN", fne({ cm["OperatorsName"], "UNSPECIFIED" }) });
        root_node.emplace_child_node({{0x0020, 0x0060}, "CS", "" }); // Laterality.
        root_node.emplace_child_node({{0x0018, 0x5100}, "CS", foe({ cm["PatientPosition"] }) });


        //-------------------------------------------------------------------------------------------------
        //General Equipment Module.
        root_node.emplace_child_node({{0x0008, 0x0070}, "LO", fne({ cm["Manufacturer"], "UNSPECIFIED" }) });
        //root_node.emplace_child_node({{0x0008, 0x0080}, "LO", fne({ cm["InstitutionName"], "UNSPECIFIED" }) });
        //root_node.emplace_child_node({{0x0008, 0x1010}, "SH", fne({ cm["StationName"], "UNSPECIFIED" }) });
        //root_node.emplace_child_node({{0x0008, 0x1040}, "LO", fne({ cm["InstitutionalDepartmentName"], "UNSPECIFIED" }) });
        //root_node.emplace_child_node({{0x0008, 0x1090}, "LO", fne({ cm["ManufacturersModelName"], "UNSPECIFIED" }) });
        //root_node.emplace_child_node({{0x0018, 0x1020}, "LO", fne({ cm["SoftwareVersions"], "UNSPECIFIED" }) });

        //-------------------------------------------------------------------------------------------------
        //Frame of Reference Module.
        root_node.emplace_child_node({{0x0020, 0x0052}, "UI", fne({ cm["FrameOfReferenceUID"], FrameOfReferenceUID }) });
        root_node.emplace_child_node({{0x0020, 0x1040}, "LO", "" }); //PositionReferenceIndicator (TODO).

        //-------------------------------------------------------------------------------------------------
        //General Image Module.
        root_node.emplace_child_node({{0x0020, 0x0013}, "IS", std::to_string(InstanceNumber) });
        root_node.emplace_child_node({{0x0020, 0x0020}, "CS", "" }); // PatientOrientation.
        root_node.emplace_child_node({{0x0008, 0x0023}, "DA", foe({ cm["ContentDate"] }) });
        root_node.emplace_child_node({{0x0008, 0x0033}, "TM", foe({ cm["ContentTime"] }) });
        root_node.emplace_child_node({{0x0008, 0x0008}, "CS", R"***(DERIVED\SECONDARY\AXIAL)***" }); //ImageType, note AXIAL can also mean coronal or transverse.
        root_node.emplace_child_node({{0x0008, 0x0022}, "DA", foe({ cm["AcquisitionDate"] }) });
        root_node.emplace_child_node({{0x0008, 0x0032}, "TM", foe({ cm["AcquisitionTime"] }) });
        root_node.emplace_child_node({{0x0020, 0x0012}, "IS", foe({ cm["AcquisitionNumber"] }) });

        //-------------------------------------------------------------------------------------------------
        //Image Plane Module.
        root_node.emplace_child_node({{0x0028, 0x0030}, "DS", std::to_string(animg.pxl_dx)  //PixelSpacing.
                                             + R"***(\)***" + std::to_string(animg.pxl_dy) });
        root_node.emplace_child_node({{0x0020, 0x0037}, "DS", std::to_string(animg.col_unit.x) //ImageOrientationPatient.
                                             + R"***(\)***" + std::to_string(animg.col_unit.y) 
                                             + R"***(\)***" + std::to_string(animg.col_unit.z) 
                                             + R"***(\)***" + std::to_string(animg.row_unit.x) 
                                             + R"***(\)***" + std::to_string(animg.row_unit.y) 
                                             + R"***(\)***" + std::to_string(animg.row_unit.z) });
        root_node.emplace_child_node({{0x0020, 0x0032}, "DS", std::to_string(animg.position(0,0).x) //ImagePositionPatient.
                                             + R"***(\)***" + std::to_string(animg.position(0,0).y) 
                                             + R"***(\)***" + std::to_string(animg.position(0,0).z) });
        root_node.emplace_child_node({{0x0018, 0x0050}, "DS", std::to_string(animg.pxl_dz) }); //SliceThickness.

        //-------------------------------------------------------------------------------------------------
        //Image Pixel Module.
        root_node.emplace_child_node({{0x0028, 0x0002}, "US", std::to_string(animg.channels) }); //SamplesPerPixel, aka, number of channels.

        std::string PhotometricInterpretation = "OTHER";
        if(animg.channels == 1){
            //PhotometricInterpretation = "MONOCHROME1"; // Minimum valued pixel = white.
            PhotometricInterpretation = "MONOCHROME2"; // Minimum valued pixel = black.
        }else if(animg.channels == 3){
            PhotometricInterpretation = "RGB";
        }else{
            PhotometricInterpretation = "OTHER"; // Non-standard ... what to do here? TODO.
        }
        root_node.emplace_child_node({{0x0028, 0x0004}, "CS", PhotometricInterpretation });
        root_node.emplace_child_node({{0x0028, 0x0010}, "US", std::to_string(animg.rows) });
        root_node.emplace_child_node({{0x0028, 0x0011}, "US", std::to_string(animg.columns) });

        root_node.emplace_child_node({{0x0028, 0x0100}, "US", "16" }); // BitsAllocated, per sample (i.e., per channel).
        root_node.emplace_child_node({{0x0028, 0x0101}, "US", "16" }); // BitsStored.
        root_node.emplace_child_node({{0x0028, 0x0102}, "US", "15" }); // HighBit, should be BitsStored-1.
        root_node.emplace_child_node({{0x0028, 0x0103}, "US", "1" }); // PixelRepresentation, 0 for unsigned, 1 for 2's complement.
        if(animg.channels != 1){
            root_node.emplace_child_node({{0x0028, 0x0006}, "US", "0" }); // PlanarConfiguration, 0 for R1, G1, B1, R2, G2, ..., and 1 for R1 R2 R3 ....
        }

        {
            std::stringstream ss_pixels;
            for(long int row = 0; row != animg.rows; ++row){
                for(long int col = 0; col != animg.columns; ++col){
                    for(long int chnl = 0; chnl != animg.channels; ++chnl){
                        auto val = static_cast<int16_t>( std::round( animg.value(row, col, chnl ) ) );
                        ss_pixels.write( reinterpret_cast<const char *>(&val), sizeof(val) );
                    }
                }
            }
            root_node.emplace_child_node({{0x7FE0, 0x0010}, "OB", ss_pixels.str() }); // PixelData.

            // Note: the standard mentions that:
            //
            //   "This Attribute does not apply when Float Pixel Data (7FE0,0008) or Double Float Pixel Data (7FE0,0009) are used
            //   instead of Pixel Data (7FE0,0010); Float Pixel Padding Value (0028,0122) or Double Float Pixel Padding Value 
            //   (0028,0123), respectively, are used instead, and defined at the Image, not the Equipment, level."
            //
            // Could I use a floating-point pixel data in lieu of compressing into 16 bits? It seems to only apply to parametric images module. TODO.
        }

        //-------------------------------------------------------------------------------------------------
        //CT Image Module.
        //
        // Note: many elements in this module are duplicated in other modules. Omitted here if they appear above.
        //
        root_node.emplace_child_node({{0x0028, 0x1052}, "DS", "0" }); //RescaleIntercept.
        root_node.emplace_child_node({{0x0028, 0x1053}, "DS", "1" }); //RescaleSlope.
        root_node.emplace_child_node({{0x0028, 0x1054}, "LO", "HU" }); //RescaleType, 'HU' for Hounsfield units, or 'US' for unspecified.
        root_node.emplace_child_node({{0x0018, 0x0060}, "DS", foe({ cm["KVP"] }) });

        //-------------------------------------------------------------------------------------------------
        //VOI LUT Module.
        //
        root_node.emplace_child_node({{0x0028, 0x1050}, "DS", "0" }); //WindowCenter.
        root_node.emplace_child_node({{0x0028, 0x1051}, "DS", "1000" }); //WindowWidth

        // Send the file to the user's handler.
        {
            std::stringstream ss;
            const auto bytes_reqd = root_node.emit_DICOM(ss, enc);
            if(!ss) throw std::runtime_error("Stream not in good state after emitting DICOM file");
            if(bytes_reqd <= 0) throw std::runtime_error("Not enough DICOM data available for valid file");

            const auto fsize = static_cast<long int>(bytes_reqd);
            file_handler(ss, fsize);
        }
    }

    return;
}


//This routine writes a collection of planar contours to a DICOM RTSTRUCT-modality file.
//
void Write_Contours(std::list<std::reference_wrapper<contour_collection<double>>> CC,
                    const std::function<void(std::istream &is,
                                       long int filesize)>& file_handler,
                    ParanoiaLevel Paranoia){
    if( CC.empty() ){
        throw std::invalid_argument("No contours provided for export. Cannot continue.");
    }
    if(!file_handler){
        throw std::invalid_argument("File handler is invalid. Refusing to continue.");
    }

    auto fne = [](std::vector<std::string> l) -> std::string {
        //fne == "First non-empty". Note this routine will throw if all provided strings are empty.
        for(auto &s : l) if(!s.empty()) return s;
        throw std::runtime_error("All inputs were empty -- unable to provide a nonempty string.");
        return std::string();
    };

    auto foe = [](std::vector<std::string> l) -> std::string {
        //foe == "First non-empty Or Empty". (i.e., will not throw if all provided strings are empty.)
        for(auto &s : l) if(!s.empty()) return s;
        return std::string(); 
    };

    DCMA_DICOM::Encoding enc = DCMA_DICOM::Encoding::ELE;
    DCMA_DICOM::Node root_node;

    //Generate some UIDs that need to be duplicated.
    const auto SOPInstanceUID = Generate_Random_UID(60);
    const auto FrameOfReferenceUID = Generate_Random_UID(60);
    // TODO: Sample any existing UID (ReferencedFrameOfReferenceUID or FrameOfReferenceUID). Probably OK to use only
    // the first in this case though...


    //Top-level stuff: metadata shared by all images.
    {
        auto cm = contour_collection<double>().get_common_metadata( CC, {} );

        //Replace any metadata that might be used to underhandedly link patients, if requested.
        if((Paranoia == ParanoiaLevel::Medium) || (Paranoia == ParanoiaLevel::High)){
            //SOP Common Module.
            cm["InstanceCreationDate"] = "";
            cm["InstanceCreationTime"] = "";
            cm["InstanceCreatorUID"]   = Generate_Random_UID(60);

            //Patient Module.
            cm["PatientsBirthDate"] = "";
            cm["PatientsGender"]    = "";
            cm["PatientsBirthTime"] = "";

            //General Study Module.
            cm["StudyInstanceUID"] = "";
            cm["StudyDate"] = "";
            cm["StudyTime"] = "";
            cm["ReferringPhysiciansName"] = "";
            cm["StudyID"] = "";
            cm["AccessionNumber"] = "";
            cm["StudyDescription"] = "";

            //General Series Module.
            cm["SeriesInstanceUID"] = "";
            cm["SeriesNumber"] = "";
            cm["SeriesDate"] = "";
            cm["SeriesTime"] = "";
            cm["SeriesDescription"] = "";
            cm["RequestedProcedureID"] = "";                          // Appropriate?
            cm["ScheduledProcedureStepID"] = "";                          // Appropriate?
            cm["OperatorsName"] = "";                          // Appropriate?

            //Patient Study Module.
            cm["PatientsWeight"] = "";                          // Appropriate?

            //Frame of Reference Module.
            cm["PositionReferenceIndicator"] = "";              // Appropriate?

            //General Equipment Module.
            cm["Manufacturer"] = "";
            cm["InstitutionName"] = "";             // Appropriate?
            cm["StationName"] = "";             // Appropriate?
            cm["InstitutionalDepartmentName"] = "";             // Appropriate?
            cm["ManufacturersModelName"] = "";
            cm["SoftwareVersions"] = "";

            //Structure Set Module.
            cm["StructureSetDescription"] = "";
            cm["StructureSetDate"] = "";
            cm["StructureSetTime"] = "";
        }
        if(Paranoia == ParanoiaLevel::High){
            //Patient Module.
            cm["PatientsName"]      = "";
            cm["PatientID"]         = "";

            //Frame of Reference Module.
            cm["FrameOfReferenceUID"] = "";

            //Structure Set Module.
            cm["StructureSetLabel"] = "UNSPECIFIED";
            cm["StructureSetName"] = "UNSPECIFIED";
        }

        //if((Paranoia == ParanoiaLevel::Medium) || (Paranoia == ParanoiaLevel::High)){
        //    ReferencedFrameOfReferenceUID = Generate_Random_UID(60);
        //}

        //-------------------------------------------------------------------------------------------------
        //DICOM Header Metadata.
        root_node.emplace_child_node({{0x0002, 0x0001}, "OB", std::string("\x0\x1", 2)}); // FileMetaInformationVersion
        root_node.emplace_child_node({{0x0002, 0x0002}, "UI", "1.2.840.10008.5.1.4.1.1.481.3"}); // MediaStorageSOPClassUID
        root_node.emplace_child_node({{0x0002, 0x0003}, "UI", SOPInstanceUID}); // MediaStorageSOPInstanceUID
        std::string TransferSyntaxUID;
        if(enc == DCMA_DICOM::Encoding::ELE){
            TransferSyntaxUID = "1.2.840.10008.1.2.1";
        }else if(enc == DCMA_DICOM::Encoding::ILE){
            TransferSyntaxUID = "1.2.840.10008.1.2";
        }else{
            throw std::runtime_error("Unsupported transfer syntax requested. Cannot continue.");
        }
        root_node.emplace_child_node({{0x0002, 0x0010}, "UI", TransferSyntaxUID}); // TransferSyntaxUID

        root_node.emplace_child_node({{0x0002, 0x0012}, "UI", "1.2.513.264.765.1.1.578"}); // ImplementationClassUID
        root_node.emplace_child_node({{0x0002, 0x0013}, "SH", "DICOMautomaton"}); // ImplementationVersionName

        //-------------------------------------------------------------------------------------------------
        //SOP Common Module.
        root_node.emplace_child_node({{0x0008, 0x0016}, "UI", "1.2.840.10008.5.1.4.1.1.481.3"}); // "SOPClassUID" (Radiation Therapy Structure Set Storage)
        root_node.emplace_child_node({{0x0008, 0x0018}, "UI", SOPInstanceUID}); // SOPInstanceUID
        root_node.emplace_child_node({{0x0008, 0x0005}, "CS", "ISO_IR 192"}); //fne({ cm["SpecificCharacterSet"], "ISO_IR 192" }));
        root_node.emplace_child_node({{0x0008, 0x0012}, "DA", fne({ cm["InstanceCreationDate"], "19720101" }) });
        root_node.emplace_child_node({{0x0008, 0x0013}, "TM", fne({ cm["InstanceCreationTime"], "010101" }) });
        root_node.emplace_child_node({{0x0008, 0x0014}, "UI", foe({ cm["InstanceCreatorUID"] }) });
        //root_node.emplace_child_node({{0x0008, 0x0114}, "UI", foe({ cm["CodingSchemeExternalUID"] }) });                 // Appropriate?
        root_node.emplace_child_node({{0x0020, 0x0013}, "IS", foe({ cm["InstanceNumber"], "0" }) });

        //-------------------------------------------------------------------------------------------------
        //Patient Module.
        root_node.emplace_child_node({{0x0010, 0x0010}, "PN", fne({ cm["PatientsName"], "DICOMautomaton^DICOMautomaton" }) });
        root_node.emplace_child_node({{0x0010, 0x0020}, "LO", fne({ cm["PatientID"], "DCMA_"_s + Generate_Random_String_of_Length(10) }) });
        root_node.emplace_child_node({{0x0010, 0x0030}, "DA", fne({ cm["PatientsBirthDate"], "19720101" }) });
        root_node.emplace_child_node({{0x0010, 0x0040}, "CS", foe({ cm["PatientsSex"] }) });
        //root_node.emplace_child_node({{0x0010, 0x0032}, "TM", fne({ cm["PatientsBirthTime"], "010101" }) });

        //-------------------------------------------------------------------------------------------------
        //General Study Module.
        root_node.emplace_child_node({{0x0020, 0x000D}, "UI", fne({ cm["StudyInstanceUID"], Generate_Random_UID(31) }) });
        root_node.emplace_child_node({{0x0008, 0x0020}, "DA", fne({ cm["StudyDate"], "19720101" }) });
        root_node.emplace_child_node({{0x0008, 0x0030}, "TM", fne({ cm["StudyTime"], "010101" }) });
        root_node.emplace_child_node({{0x0008, 0x0090}, "PN", fne({ cm["ReferringPhysiciansName"], "UNSPECIFIED^UNSPECIFIED" }) });
        root_node.emplace_child_node({{0x0020, 0x0010}, "SH", fne({ cm["StudyID"], "DCMA_"_s + Generate_Random_String_of_Length(10) }) });
        root_node.emplace_child_node({{0x0008, 0x0050}, "SH", fne({ cm["AccessionNumber"], Generate_Random_String_of_Length(14) }) });
        root_node.emplace_child_node({{0x0008, 0x1030}, "LO", fne({ cm["StudyDescription"], "UNSPECIFIED" }) });

        //-------------------------------------------------------------------------------------------------
        //RT Series Module.
        root_node.emplace_child_node({{0x0008, 0x0060}, "CS", "RTSTRUCT" }); // "Modality"
        root_node.emplace_child_node({{0x0020, 0x000E}, "UI", fne({ cm["SeriesInstanceUID"], Generate_Random_UID(31) }) });
        root_node.emplace_child_node({{0x0020, 0x0011}, "IS", fne({ cm["SeriesNumber"], Generate_Random_Int_Str(5000, 32767) }) }); // Upper: 2^15 - 1.
        root_node.emplace_child_node({{0x0008, 0x0021}, "DA", foe({ cm["SeriesDate"] }) });
        root_node.emplace_child_node({{0x0008, 0x0031}, "TM", foe({ cm["SeriesTime"] }) });
        root_node.emplace_child_node({{0x0008, 0x103E}, "LO", fne({ cm["SeriesDescription"], "UNSPECIFIED" }) });
        root_node.emplace_child_node({{0x0008, 0x1070}, "PN", fne({ cm["OperatorsName"], "UNSPECIFIED" }) });


        //-------------------------------------------------------------------------------------------------
        //General Equipment Module.
        root_node.emplace_child_node({{0x0008, 0x0070}, "LO", fne({ cm["Manufacturer"], "UNSPECIFIED" }) });
        //root_node.emplace_child_node({{0x0008, 0x0080}, "LO", fne({ cm["InstitutionName"], "UNSPECIFIED" }) });
        //root_node.emplace_child_node({{0x0008, 0x1010}, "SH", fne({ cm["StationName"], "UNSPECIFIED" }) });
        //root_node.emplace_child_node({{0x0008, 0x1040}, "LO", fne({ cm["InstitutionalDepartmentName"], "UNSPECIFIED" }) });
        //root_node.emplace_child_node({{0x0008, 0x1090}, "LO", fne({ cm["ManufacturersModelName"], "UNSPECIFIED" }) });
        //root_node.emplace_child_node({{0x0018, 0x1020}, "LO", fne({ cm["SoftwareVersions"], "UNSPECIFIED" }) });

        //-------------------------------------------------------------------------------------------------
        //Frame of Reference Module.
        root_node.emplace_child_node({{0x0020, 0x0052}, "UI", FrameOfReferenceUID}); //FrameOfReferenceUID 
        root_node.emplace_child_node({{0x0020, 0x1040}, "LO", "" }); //PositionReferenceIndicator (TODO).

/*
        // Emit the Referenced Frame of Reference Sequence.
        {
            DCMA_DICOM::Node *seq_node_ptr = root_node.emplace_child_node({{0x3006, 0x0010}, "SQ", ""});  // ReferencedFrameOfReferenceSequence
            seq_node_ptr->emplace_child_node({{0x0020, 0x0052}, "UI", FrameOfReferenceUID}); //FrameOfReferenceUID 
        }
*/

        //-------------------------------------------------------------------------------------------------
        //Structure Set Module.
        root_node.emplace_child_node({{0x3006, 0x0002}, "SH", fne({ cm["StructureSetLabel"], "UNSPECIFIED" }) }); // "Structure Set Label"
        root_node.emplace_child_node({{0x3006, 0x0004}, "LO", fne({ cm["StructureSetName"], "UNSPECIFIED" }) }); // "Structure Set Name"
        root_node.emplace_child_node({{0x3006, 0x0006}, "ST", fne({ cm["StructureSetDescription"], "UNSPECIFIED" }) });
        root_node.emplace_child_node({{0x3006, 0x0008}, "DA", foe({ cm["StructureSetDate"] }) });
        root_node.emplace_child_node({{0x3006, 0x0009}, "TM", foe({ cm["StructureSetTime"] }) });

        // (The following Structure Set ROI Sequence is part of the Structure Set Module.)
    }


    // Emit the Structure Set ROI Sequence, which maps an integer number to names, FrameOfReferenceUID, and creation
    // method.
    {
        DCMA_DICOM::Node *ssr_seq_ptr = root_node.emplace_child_node({{0x3006, 0x0020}, "SQ", ""});  // StructureSetROISequence
        //uint32_t seq_n = 0;
        uint32_t seq_n = 1;
        for(const auto cc_refw : CC){
            auto cm = cc_refw.get().get_common_metadata({}, {});

            DCMA_DICOM::Node *multi_seq_ptr = ssr_seq_ptr->emplace_child_node({{0x0000, 0x0000}, "MULTI", ""});

            multi_seq_ptr->emplace_child_node({{0x3006, 0x0022}, "IS", std::to_string(seq_n) }); // ROINumber (Does this need to be 1-based? TODO)
            multi_seq_ptr->emplace_child_node({{0x3006, 0x0024}, "UI", fne({ FrameOfReferenceUID }) }); // ReferencedFrameOfReferenceUID
            multi_seq_ptr->emplace_child_node({{0x3006, 0x0026}, "LO", // ROIName
                 fne({ cm["ROIName"], 
                       cm["ROILabel"],
                       cm["NormalizedROIName"],
                       "UNSPECIFIED"})   }); 
            multi_seq_ptr->emplace_child_node({{0x3006, 0x0028}, "ST", // ROIDescription
                  foe({ cm["ROIDescription"], 
                        cm["Description"],
                        "UNSPECIFIED" }) });
            multi_seq_ptr->emplace_child_node({{0x3006, 0x0036}, "CS", "MANUAL" }); // ROIGenerationAlgorithm (MANUAL, AUTOMATIC, or SEMIAUTOMATIC).
            multi_seq_ptr->emplace_child_node({{0x3006, 0x0038}, "LO", foe({ cm["GenerationDescription"] }) }); // ROIGenerationDescription

            //ds_seq_insert(tds, {0x3006, 0x0020, 0, seq_n},        // "StructureSetROISequence", item #n
            //                   {0x0008, 0x9215},                  // "DerivationCodeSequence"
            //                     { ... } },
            //                   foe({ cm["GenerationDescription"] }));
            //  See http://dicom.nema.org/dicom/2013/output/chtml/part03/sect_8.8.html#table_8.8-1 for this sequence.

            ++seq_n;
        }
    }

    //-------------------------------------------------------------------------------------------------
    // ROI Contour Module.

    // Emit the ROI Contour Sequence.
    {
        DCMA_DICOM::Node *rc_seq_ptr = root_node.emplace_child_node({{0x3006, 0x0039}, "SQ", ""});  // ROIContourSequence
        //uint32_t roi_seq_n = 0;
        uint32_t roi_seq_n = 1;
        for(const auto cc_refw : CC){
            //auto cm = cc_refw.get().get_common_metadata({}, {});

            DCMA_DICOM::Node *multi_rc_seq_ptr = rc_seq_ptr->emplace_child_node({{0x0000, 0x0000}, "MULTI", ""});

            multi_rc_seq_ptr->emplace_child_node({{0x3006, 0x0084}, "IS", std::to_string(roi_seq_n) }); // ReferencedROINumber (Does this need to be 1-based? TODO)
            //multi_rc_seq_ptr->emplace_child_node({{0x3006, 0x002A}, "IS", R"***(255\0\0)***" }); // ROIDisplayColor
            DCMA_DICOM::Node *c_seq_ptr = multi_rc_seq_ptr->emplace_child_node({{0x3006, 0x0040}, "SQ", ""});  // ContourSequence

            //uint32_t contour_seq_n = 0;
            uint32_t contour_seq_n = 1;
            for(const auto& c : cc_refw.get().contours){
                DCMA_DICOM::Node *multi_c_seq_ptr = c_seq_ptr->emplace_child_node({{0x0000, 0x0000}, "MULTI", ""});

                multi_c_seq_ptr->emplace_child_node({{0x3006, 0x0048}, "IS", std::to_string(contour_seq_n) }); // ContourNumber
                multi_c_seq_ptr->emplace_child_node({{0x3006, 0x0042}, "CS", "CLOSED_PLANAR" }); // ContourGeometricType
                //multi_c_seq_ptr->emplace_child_node({{0x3006, 0x0044}, "DS", "1.0" }); // ContourSlabThickness (in DICOM units; mm)
                //multi_c_seq_ptr->emplace_child_node({{0x3006, 0x0045}, "DS", R"***(0.0\0.0\1.0)***" }); // ContourOffsetVector
                multi_c_seq_ptr->emplace_child_node({{0x3006, 0x0046}, "IS", std::to_string(c.points.size()) }); // NumberOfControlPoints

                //DCMA_DICOM::Node *ac_seq_ptr = rc_seq_ptr->emplace_child_node({{0x3006, 0x0049}, "SQ", ""});  // AttachedContours  ???
                //for(attached_contours ...){
                //    DCMA_DICOM::Node *multi_ac_seq_ptr = ac_seq_ptr->emplace_child_node({{0x0000, 0x0000}, "MULTI", ""});
                //    multi_ac_seq_ptr->emplace_child_node({{0x3006, 0x0049}, "???", std::to_string(c.points.size()) }); // AttachedContours
                //} 

                // Emit the actual contour data.
                //
                // Note: If explicit VR transfer syntax is used, each contour should not exceed 65534 bytes!
                std::stringstream ss;
                bool isnew = true;
                for(const auto & p : c.points){
                    if(!isnew){
                        ss << R"***(\)***"; // Delimit from the previous coordinates.
                    }else{
                        isnew = false;
                    }
                    ss << p.x <<  R"***(\)***" << p.y << R"***(\)***" << p.z;
                }
                if(65534 < ss.str().size()) throw std::runtime_error("Contour too large, data loss may occur. Refusing to proceed.");
                multi_c_seq_ptr->emplace_child_node({{0x3006, 0x0050}, "DS", ss.str() }); // ContourData

                //FUNCINFO("Emitted contour " << contour_seq_n << " of " << cc_refw.get().contours.size() 
                //     << " from ROI " << roi_seq_n << " of " << CC.size() );

                ++contour_seq_n;
            }

            ++roi_seq_n;
        }
    }

    //-------------------------------------------------------------------------------------------------
    // RT ROI Observations Module.

    // Emit the RT ROI Observations Sequence.
    {
        DCMA_DICOM::Node *rro_seq_ptr = root_node.emplace_child_node({{0x3006, 0x0080}, "SQ", ""});  // RTROIObservationsSequence
        //uint32_t roi_seq_n = 0;
        uint32_t roi_seq_n = 1;
        for(const auto cc_refw : CC){
            //auto cm = cc_refw.get().get_common_metadata({}, {});

            DCMA_DICOM::Node *multi_seq_ptr = rro_seq_ptr->emplace_child_node({{0x0000, 0x0000}, "MULTI", ""});

            multi_seq_ptr->emplace_child_node({{0x3006, 0x0082}, "IS", std::to_string(roi_seq_n) }); // ObservationNumber
            multi_seq_ptr->emplace_child_node({{0x3006, 0x0084}, "IS", std::to_string(roi_seq_n) }); // ReferencedROINumber
            multi_seq_ptr->emplace_child_node({{0x3006, 0x00A4}, "CS", "ORGAN" }); // RTROIInterpretedType
                               // EXTERNAL, PTV, CTV, GTV, BOLUS, AVOIDANCE, ORGAN, MARKER, CONTROL, etc..
                               // See http://dicom.nema.org/medical/dicom/current/output/chtml/part03/sect_C.8.8.8.html .
            multi_seq_ptr->emplace_child_node({{0x3006, 0x00A6}, "PN", "UNSPECIFIED" }); // ROIInterpreter

            ++roi_seq_n;
        }
    }

    //-------------------------------------------------------------------------------------------------
    // Approval Module.
    {
        root_node.emplace_child_node({{0x300e, 0x0002}, "CS", "UNAPPROVED"  });  // "ApprovalStatus". Can be 'UNAPPROVED', 'APPROVED', or 'REJECTED'.
        //root_node.emplace_child_node({{0x300e, 0x0004}, "DA", "19720101"  });    // "ReviewDate"
        //root_node.emplace_child_node({{0x300e, 0x0005}, "TM", "010101"  });      // "ReviewTime"
        //root_node.emplace_child_node({{0x300e, 0x0008}, "PN", "UNSPECIFIED"  }); // "ReviewerName"
    }

    // Send the file to the user's handler.
    {
        std::stringstream ss;
        const auto bytes_reqd = root_node.emit_DICOM(ss, enc);
        if(!ss) throw std::runtime_error("Stream not in good state after emitting DICOM file");
        if(bytes_reqd <= 0) throw std::runtime_error("Not enough DICOM data available for valid file");

        const auto fsize = static_cast<long int>(bytes_reqd);
        file_handler(ss, fsize);
    }

    return;
}

