//XML_Tools.h -- A minimal, extremely simple, standards non-compliant XML reader.

#pragma once

#include <string>    
#include <map>
#include <list>
#include <vector>
#include <memory>
#include <functional>
#include <initializer_list>

#include <filesystem>

#include "Structs.h"
#include "Metadata.h"
//using metadata_map_t = std::map<std::string,std::string>; // key, value.


namespace dcma {
namespace xml {

// Parsed XML node structure.
struct node {
    std::string name;  // e.g., <name></name>
    std::string content; // e.g., <a>1.23</a>
    metadata_map_t metadata; // e.g., <a xyz="123"></a>

    std::list<node> children;
};

// Parse an XML document into a tree of nodes.
void
read_node( std::istream &is, node &root );

// Debugging routine to print a node and its children recursively.
void
print_node( std::ostream &os,
            const dcma::xml::node &root,
            std::string indent = "");


// Used to pass through a chain/list/string of nodes when searching.
using node_chain_t = std::list<std::reference_wrapper<node>>;

// Common callback signature.
//
// The user callback should return 'true' to continue searching. Otherwise, return 'false' to stop.
using search_callback_t = std::function<bool(const node_chain_t &)>;

// Search a tree via node names for the first occurence of the given pattern.
//
// A user callback is invoked for the matching node chain, i.e., the matching root node is the first node and the
// terminating child is the last node.
//
// Note: the return value indicates whether a full, exhaustive search was performed.
//
template <class InputIt> // iterators for container of std::string
[[maybe_unused]]
inline
bool
search_by_names( node &root,
                 InputIt it_names_first, // iterators for container of std::string
                 InputIt it_names_last,
                 const search_callback_t &f_user,
                 bool permit_recursive_search = true, // if false, the first name must occur in one of the root's children.
                 node_chain_t chain = {} ){

    bool continue_searching = true;
    if( it_names_first != it_names_last ){

        auto next = std::next(it_names_first);
        const bool is_bottom_search_node = (next == it_names_last);

        for(auto& c : root.children){
            // Search for the current fragment.
            if(c.name == *it_names_first){
                // Ensure the root node is added.
                if(chain.empty()){
                    chain.emplace_back(std::ref(root));
                }
                chain.emplace_back(std::ref(c));

                if(is_bottom_search_node){
                    continue_searching = f_user(chain);
                    if(!continue_searching) break;

                }else{
                    continue_searching = search_by_names(c, next, it_names_last, f_user, permit_recursive_search, chain);
                    if(!continue_searching) break;
                }
            }

            // Recursively search with the full search vector.
            if(permit_recursive_search){
                continue_searching = search_by_names(c, it_names_first, it_names_last, f_user, permit_recursive_search, chain);
                if(!continue_searching) break;
            }
        }

    }
    return continue_searching;
}

// Wrapper for the previous function, but supporting an initializer list.
template <class T>
[[maybe_unused]]
inline
bool
search_by_names( node &root,
                 std::initializer_list<T> l,
                 const search_callback_t &f_user,
                 bool permit_recursive_search = true,
                 node_chain_t chain = {} ){

    return search_by_names( root,
                            std::begin(l),
                            std::end(l),
                            f_user,
                            permit_recursive_search,
                            chain );
}

}; // namespace xml.
}; // namespace dcma.

