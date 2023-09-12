// XML_Tools.cc - A part of DICOMautomaton 2023. Written by hal clark.
//
// The code in this file can be used to parse XML files into a nested tree of objects.
//

#include <string>
#include <map>
#include <vector>
#include <list>
#include <fstream>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <exception>
#include <numeric>
#include <memory>
#include <stdexcept>
#include <cstdlib>
#include <cstdint>

//#include "YgorIO.h"
//#include "YgorTime.h"
//#include "YgorString.h"
//#include "YgorMath.h"         //Needed for vec3 class.
//#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
//#include "YgorLog.h"

#include "Metadata.h"
#include "Structs.h"

#include "XML_Tools.h"


void
dcma::xml::read_node( std::istream &is,
                      dcma::xml::node &root ){
    // Buffer parsing:
    // - look for enclosing '<? ?>'. If present, treat this as a special node by *not* recursing.
    // - look for enclosing '</ >'. If present, look for the name.
    // - look for enclosing '< >'. If present, look for name and metadata.
    // - otherwise, the buffer only contains content. 
    enum class tag_type_t {
        header,   // Like '<? ... ?>'.
        opening,  // Like '< ... >'.
        closing,  // Like '</ ... >'.
        combo,    // Like '<... />'.
    } tag_type = tag_type_t::opening;

    constexpr bool verbose = false; // For debugging only.

    // Make routine to parse a tag to get the name and any metadata.
    bool escaped = false;
    bool inside_tag = false; // Between '<' and '>'.
    std::vector<char> quotes;
    std::string buf;
    std::string key; // Used to temporarily store key=value statements.

    std::list<dcma::xml::node> work_nodes;
    work_nodes.emplace_back();

    const auto trim_outer_whitespace = [&](const std::string &buf){
        std::string out(buf);
        const std::string space = " \n\r\t\f\v";

        auto pos_A = buf.find_first_not_of(space);
        if(pos_A == std::string::npos){
            out.clear();
        }else{
            auto pos_B = buf.find_last_not_of(space);
            out = buf.substr(pos_A, 1 + pos_B - pos_A);
        }

        return out;
    };

    const auto clear_buf = [&](){
        buf = trim_outer_whitespace(buf);
        
        if(buf.empty()){
            // Do nothing.
        }else if(!key.empty()){
            work_nodes.back().metadata[key] = buf;
        }else if(work_nodes.back().name.empty()){
            work_nodes.back().name = buf;
        }else{
            throw std::runtime_error("Unrecognized structure");
        }
        key.clear();
        buf.clear();
        return;
    };

    // Remove preceding whitespace.
    is >> std::ws;

    // Read until you have an *unescaped* and *unquoted* '<' or '>' char. Add every char to the buffer (for later sub-parsing).
    char c = '\0';
    char prev_c = '\0';
    while(is.get(c)){
        if(verbose) std::cout << "Parsing char '" << c << "'" << std::endl;

        // Handle quotes.
        if(false){
        }else if( inside_tag && !escaped && ( (c == '"') || (c == '\'') ) ){
            if(verbose) std::cout << "   Handling quote" << std::endl;
            if(!quotes.empty() && (quotes.back() == c)){
                quotes.pop_back();
            }else{
                quotes.push_back(c);
            }
            //buf.push_back(c);

        // Handle escapes.
        }else if( inside_tag && !escaped && !quotes.empty() && (c == '\\') ){
            if(verbose) std::cout << "   Handling escape" << std::endl;
            escaped = true;
            //buf.push_back(c);

        // Handle '<' opening a new tag.
        }else if( !inside_tag && !escaped && quotes.empty() && (c == '<') ){
            if(verbose) std::cout << "   Handling '<'" << std::endl;
            // If there is something in the buffer, assume it is enclosed content for the preceding tag.
            buf = trim_outer_whitespace(buf);
            if(!buf.empty()){
                root.content += buf;
                buf.clear();
            }
            //buf.push_back(c);
            inside_tag = true;
            tag_type = tag_type_t::opening;

        // Handle '>' closing a tag.
        }else if( inside_tag && !escaped && quotes.empty() && (c == '>') ){
            if(verbose) std::cout << "   Handling '>'" << std::endl;
            // Handle any outstanding content in the buffer.
            clear_buf();

            if(false){
            }else if(tag_type == tag_type_t::opening){
                // Handle "<abc>" tags.
                //
                // Create tag in the root, then recurse to read the child.
                root.children.splice( root.children.end(), work_nodes );
                work_nodes.clear();
                work_nodes.emplace_back();

                read_node( is, root.children.back() );

            }else if(tag_type == tag_type_t::header){
                // Handle "<? abc ?>" tags.
                //
                // Create tag in the root, but do not recurse since there is no corresponding closing tag.
                root.children.splice( root.children.end(), work_nodes );
                work_nodes.clear();
                work_nodes.emplace_back();

            }else if(tag_type == tag_type_t::closing){
                // Handle closing tags where the current name "</abc>" matches the parent's name "<abc>".
                //
                // Ensure the tag name matches the root, and then return control to the parent in case there are siblings
                // to this node.
                const auto l_name = work_nodes.back().name;
                const auto r_name = root.name;
                if(l_name != r_name){
                    throw std::runtime_error("Mismatched opening/closing tags");
                }
                return;

            }else if(tag_type == tag_type_t::combo){
                // Handle "<abc />" tags.
                //
                // Create a tag in the root, but do not recurse since there is no corresponding closing tag.
                root.children.splice( root.children.end(), work_nodes );
                work_nodes.clear();
                work_nodes.emplace_back();
            }
            inside_tag = false;

        // Handle '=' inside a tag.
        }else if( inside_tag && !escaped && quotes.empty() && (c == '=') ){
            if(verbose) std::cout << "   Handling '='" << std::endl;
            if(buf.empty()){
                throw std::runtime_error("Key-value metadata assignment attempted without a valid key");
            }else if(!key.empty()){
                throw std::runtime_error("Key-value metadata assignment attempted with existing key");
            }
            key = buf;
            buf.clear();

        // Handle ' ' inside a tag.
        }else if( inside_tag && !escaped && quotes.empty() && (c == ' ') ){
            if(verbose) std::cout << "   Handling ' '" << std::endl;
            clear_buf();

        // Handle '?' inside a tag.
        }else if( inside_tag && !escaped && quotes.empty() && (c == '?') ){
            if(verbose) std::cout << "   Handling '?'" << std::endl;
            // This is to handle tags like "<?xml version='1.0' encoding='UTF-8' standalone='yes' ?>".
            tag_type = tag_type_t::header;

        // Handle '/' inside a tag.
        }else if( inside_tag && !escaped && quotes.empty() && (c == '/') ){
            if(verbose) std::cout << "   Handling '/'" << std::endl;
            
            // This is to handle closing tags like "</abc>".
            if(prev_c == '<'){
                tag_type = tag_type_t::closing;

            // This is to handle 'combo' tags like "<abc />".
            }else{
                tag_type = tag_type_t::combo;
            }

        // Plain input.
        }else{
            if(verbose) std::cout << "   Handling plain input" << std::endl;
            buf.push_back(c);
            escaped = false;
        }
        
        prev_c = c;
    }

    return;
}

void
dcma::xml::print_node( std::ostream &os,
                       const dcma::xml::node &root,
                       std::string indent){
    os << indent << "---------" << std::endl;
    os << indent << "Name    = '" << root.name << "'" << std::endl;
    if(!root.content.empty()){
        os << indent << "Content = '" << root.content << "'" << std::endl;
    }
    if(!root.metadata.empty()){
        os << indent << "Metadata:  " << std::endl;
        for(const auto& kv : root.metadata){
            os << indent << "  '" << kv.first << "' = '" << kv.second << "'" << std::endl;
        }
    }
    if(!root.children.empty()){
        os << indent << "Children:  " << std::endl;
        for(const auto& c : root.children){
            print_node(os, c, indent + "    ");
        }
    }
    return;
}

/*
int main(int argc, char **argv){

    std::ifstream ifs("in2.xml");
    dcma::xml::node root;
    try{
        dcma::xml::read_node(ifs, root);
    }catch(const std::exception &e){
        std::cout << "Exception: '" << e.what() << "'" << std::endl;
    }
    dcma::xml::print_node(std::cout, root, "");
    return 0;


    bool permit_recursive_search = true;
    std::vector<std::string> names;
    names.push_back("gpx");
    names.push_back("trk");
    names.push_back("trkseg");
    names.push_back("trkpt");

    dcma::xml::search_callback_t f_one = [&](const dcma::xml::node_chain_t &nc) -> bool {
        std::cout << "Found a node: " << std::endl;
        dcma::xml::print_node(std::cout, nc.back().get(), "");
        return false;
    };
    dcma::xml::search_by_names(root, std::begin(names), std::end(names), f_one, permit_recursive_search);

    long int N_nodes = 0;
    dcma::xml::search_callback_t f_all = [&](const dcma::xml::node_chain_t &nc) -> bool {
        ++N_nodes;
        return true;
    };
    dcma::xml::search_by_names(root, std::begin(names), std::end(names), f_all, permit_recursive_search);
    std::cout << "Exhaustive search found " << N_nodes << " nodes." << std::endl;

    return 0;
}
*/


