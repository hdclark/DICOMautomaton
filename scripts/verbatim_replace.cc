
// Simple program to perform verbatim substring replacement, but ignoring insignificcant characters (e.g., whitespace).
//
// Compile:
//  g++ --std=c++17 scripts/verbatim_replace.cc -o verbatim_replace
//
// Run:
//  ./verbatim_replace pattern.txt replacement.txt file.cc
//
// Written by hal clark, 2024.
//

#include <string>
#include <iostream>
#include <fstream>
#include <exception>
#include <cctype>
#include <vector>
#include <optional>
#include <algorithm>
#include <map>


// Read and write bytes from/to a file.
std::vector<char> read_file(const std::string &filename){
    std::vector<char> out;

    std::ifstream FI(filename, std::ios::binary | std::ios::ate);
    if(!FI){
        throw std::runtime_error("Unable to open file for reading");
    }
    const auto N_bytes = FI.tellg();
    FI.seekg(0, std::ios::beg);

    out.resize(N_bytes);
    if(!FI.read(out.data(), out.size())){
        throw std::runtime_error("Unable to read contents of file");
    }
    return out;
}

bool
write_file(const std::string &filename, const std::vector<char> &v){
    std::ofstream FO(filename, std::ios::binary);
    if(!FO){
        throw std::runtime_error("Unable to open file for writing");
    }
    if(!FO.write(v.data(), v.size())){
        throw std::runtime_error("Unable to write to file");
    }
    return true;
}

void print(const std::vector<char> &v){
    std::cout.write(v.data(), v.size());
    std::cout.flush();
    return;
}

// Characters considered insignificant when matching the pattern in the target file.
bool is_insignificant(char c){
    // Note: this approach ignores quoted characters, which *could* be significant!
    return (c == ' ')  || (c == '\t')
        || (c == '\r') || (c == '\n');
}

// Split a byte vector into a vector of tokens.
// Eliminates irrelevant (e.g., whitespace) characters.
// Also returns a mappin of current character position back to original character position.
std::vector<size_t> tokenize(std::vector<char> &bytes){
    std::vector<char> shtl;
    std::vector<size_t> orig_pos;

    shtl.reserve(bytes.size());
    orig_pos.reserve(bytes.size());

    size_t i = 0UL;
    for(const auto &b : bytes){
        if(!is_insignificant(b)){
            shtl.emplace_back(b);
            orig_pos.emplace_back(i);
        }
        ++i;
    }

    // Ensure the final character is present and maps to the first-past-the-end.
    orig_pos.emplace_back(i);

    bytes = shtl;
    return orig_pos;
}

// Trim the insignificant characters off the end of a vector.
void trim_ends(std::vector<char> &bytes){
    auto it1 = std::find_if_not( std::begin(bytes), std::end(bytes), is_insignificant );
    auto it2 = std::find_if_not( std::rbegin(bytes), std::rend(bytes), is_insignificant );
    if( (it1 == std::end(bytes))
    ||  (it2 == std::rend(bytes)) ){
        bytes.clear();
        return;
    }
    const auto N_A = std::distance(std::begin(bytes), it1);
    const auto N_B = std::distance(it2, std::rend(bytes));

    bytes.erase( std::next( std::begin(bytes), N_B), std::end(bytes) );
    bytes.erase( std::begin(bytes), std::next( std::begin(bytes), N_A) );
    return;
}

int main(int argc, char **argv){
    if(argc != 4){
        throw std::runtime_error("usage: <file_with_pattern> <replacement> <file_to_edit>");
        return 1;
    }

    const std::string filename_pattern(argv[1]);
    const std::string filename_replace(argv[2]);
    const std::string filename_target(argv[3]);

    auto vec_target = read_file(filename_target);
    const auto vec_pattern = read_file(filename_pattern);
    const auto vec_replace = read_file(filename_replace);

    std::cout << "Contents of pattern file (" << vec_pattern.size() << " bytes):" << std::endl;
    print(vec_pattern);
    std::cout << std::endl;

    std::cout << "Contents of replacement file (" << vec_replace.size() << " bytes):" << std::endl;
    print(vec_replace);
    std::cout << std::endl;

    auto l_vec_pattern = vec_pattern;
    auto l_vec_replace = vec_replace;
    auto l_vec_target  = vec_target;

    auto map_pattern = tokenize(l_vec_pattern);
    auto map_target = tokenize(l_vec_target);

    // Trim insignificant chars from the front and back of the replacement.
    // This helps avoid adding extra newlines and irrelevant noise.
    trim_ends(l_vec_replace);

    std::cout << "Contents of trimmed replacement file (" << l_vec_replace.size() << " bytes):" << std::endl;
    print(l_vec_replace);
    std::cout << std::endl;

    // Enumerate all occurences of the pattern.
    using it_t = decltype(std::begin(vec_target));
    std::vector<size_t> matches;
    {
        const auto l_target_beg = std::begin(l_vec_target);
        const auto l_target_end = std::end(l_vec_target);

        const auto l_pattern_beg = std::begin(l_vec_pattern);
        const auto l_pattern_end = std::end(l_vec_pattern);

        std::optional<it_t> searched;
        while(true){
            auto it = std::search( searched.value_or(l_target_beg), l_target_end, 
                                   l_pattern_beg, l_pattern_end );
            if(it != l_target_end){
                searched = std::next(it , l_vec_pattern.size());
                matches.emplace_back(std::distance(l_target_beg,it));
            }else{
                break;
            }
        }
    }
    std::cout << "Found " << matches.size() << " matches." << std::endl;

    // TODO: optionally only replace the first occurence by trimming all but one.

    // Now process the occurences backward so we don't invalidate the vectors/mappings.
    std::reverse( std::begin(matches), std::end(matches) );
    for(auto &m : matches){
        const auto actual_target_beg = std::begin(vec_target);
        const auto actual_target_end = std::end(vec_target);

        const auto l_target_beg = std::begin(l_vec_target);
        const auto l_target_end = std::end(l_vec_target);

        auto it = std::next(l_target_beg, m);

        // Convert character positions back to the original target array.
        //
        // Note that we backtrack to the previous *significant* character so that we can
        // find the next *insignificant* character. Otherwise, we end up trimming trailing
        // insignificant characters (e.g., whitespace and newlines) that occur *after*
        // the pattern, which is often not useful.
        const auto actual_beg_offset = map_target.at( std::distance(l_target_beg, it));
        const auto actual_end_offset = map_target.at( std::distance(l_target_beg, it)
                                                    + l_vec_pattern.size() - 1UL ) + 1UL;

        // Remove the pattern from the (actual) target.
        auto it2 = vec_target.erase( std::next(actual_target_beg, actual_beg_offset),
                                     std::next(actual_target_beg, actual_end_offset) );

        // TODO: modify so that the replacement's indentation fits adjacent text.

        // Insert the replacement into the (actual) target.
        vec_target.insert(it2, std::begin(l_vec_replace), std::end(l_vec_replace));

        // Write the (actual) target back to file.
        std::cout << "Implemented replacement." << std::endl;
        write_file(filename_target, vec_target);
    }

    return 0;
}

