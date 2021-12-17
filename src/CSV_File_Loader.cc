// CSV_File_Loader.cc - A part of DICOMautomaton 2021. Written by hal clark.
//
// This program loads CSV-formatted 2D tables.
//

#include <string>
#include <map>
#include <vector>
#include <list>
#include <fstream>
#include <sstream>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <exception>
#include <numeric>
#include <memory>
#include <stdexcept>
#include <filesystem>
#include <cstdlib>            //Needed for exit() calls.

#include "YgorIO.h"
#include "YgorTime.h"
#include "YgorImages.h"
#include "YgorImagesIO.h"
#include "YgorString.h"
#include "YgorMath.h"         //Needed for vec3 class.
//#include "YgorStats.h"        //Needed for Median().
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.

#include "Metadata.h"
#include "Structs.h"
//#include "Imebra_Shim.h"      //Needed for Collate_Image_Arrays().

static
tables::table2
read_csv_file( std::istream &is ){
    tables::table2 t;

    const std::string quotes = "\"";  // Characters that open a quote at beginning of line only.
    const std::string escs   = "\\";  // The escape character(s) inside quotes.
    const std::string pseps  = "\t";  // 'Priority' separation characters. If detected, these take priority over others.
    std::string seps   = ",";   // The characters that separate cells.

    std::stringstream ss;

    // --- Automatic separator detection ---
    const long int autodetect_separator_rows = 10;

    // Buffer the input stream into a stringstream, checking for the presence of priority separators.
    bool use_pseps = false;
    for(long int i = 0; i < autodetect_separator_rows; ++i){
        std::string line;
        if(std::getline(is, line)){
            // Avoid newline at end, which can appear as an extra empty row later.
            ss << ((i==0) ? "" : "\n") << line;

            const bool has_pseps = (line.find_first_of(pseps) != std::string::npos);
            if(has_pseps){
                use_pseps = true;
                break;
            }
        }
    }
    if(use_pseps){
        seps = pseps;
        FUNCINFO("Detected alternative separators, switching acceptable separators");
    }

    // -------------------------------------

    const auto clean_string = [](const std::string &in){
        return Canonicalize_String2(in, CANONICALIZE::TRIM_ENDS);
    };

    int64_t row_num = -1;
    std::string line;
    while(std::getline(ss, line) || std::getline(is, line)){
        ++row_num;
        bool inside_quote;
        std::string cell;

        int64_t col_num = 0;
        auto c_it = std::begin(line);
        const auto end = std::end(line);
        for( ; c_it != end; ++c_it){
            const bool is_quote = (quotes.find_first_of(*c_it) != std::string::npos);
            auto c_next_it = std::next(c_it);

            if(inside_quote){
                const bool is_esc = (escs.find_first_of(*c_it) != std::string::npos);

                if(is_quote){
                    // Close the quote.
                    inside_quote = false;

                }else if(is_esc){
                    // Implement escape of next character.
                    if(c_next_it == end){
                        throw std::runtime_error("Nothing to escape (row "_s + std::to_string(row_num) + ")");
                    }
                    cell.push_back( *c_next_it );
                    ++c_it;
                    ++c_next_it;

                }else{
                    cell.push_back( *c_it );
                }
                
            }else{
                const bool is_sep = (seps.find_first_of(*c_it) != std::string::npos);

                if(is_quote){
                    // Open the quote.
                    inside_quote = true;

                }else if( is_sep ){
                    // Push cell into table.
                    cell = clean_string(cell);
                    if(!cell.empty()) t.inject(row_num, col_num, cell);
                    ++col_num;
                    cell.clear();

                }else{
                    cell.push_back( *c_it );
                }
            }

            if( c_next_it == end ){
                // Push cell into table.
                cell = clean_string(cell);
                if(!cell.empty()) t.inject(row_num, col_num, cell);
                ++col_num;
                cell.clear();
            }
        }

        // Add any outstanding contents as a cell.
        cell = clean_string(cell);
        if( !cell.empty()
        ||  inside_quote ){
            throw std::invalid_argument("Unable to parse row "_s + std::to_string(row_num));
        }
    }

    if(t.data.empty()){
        throw std::runtime_error("Unable to extract any data from file");
    }
    return t;
}


bool Load_From_CSV_Files( Drover &DICOM_data,
                          std::map<std::string,std::string> & /* InvocationMetadata */,
                          const std::string &,
                          std::list<std::filesystem::path> &Filenames ){

    // This routine will attempt to load CSV images on an individual file basis. Files that are not successfully loaded
    // are not consumed so that they can be passed on to the next loading stage as needed. 
    //
    // Note: This routine returns false only iff a file is suspected of being suited for this loader, but could not be
    //       loaded (e.g., the file seems appropriate, but a parsing failure was encountered).
    //
    if(Filenames.empty()) return true;

    size_t i = 0;
    const size_t N = Filenames.size();

    auto bfit = Filenames.begin();
    while(bfit != Filenames.end()){
        FUNCINFO("Parsing file #" << i+1 << "/" << N << " = " << 100*(i+1)/N << "%");
        ++i;
        const auto Filename = *bfit;

        try{
            std::ifstream is(Filename.string(), std::ios::in | std::ios::binary);
            auto tab = read_csv_file(is);

            // Ensure a minimal amount of metadata is present for image purposes.
            auto l_meta = coalesce_metadata_for_basic_table(tab.metadata);
            l_meta.merge(tab.metadata);
            tab.metadata = l_meta;
            tab.metadata["Filename"] = Filename.string();

            FUNCINFO("Loaded CSV file with " << tab.data.size() << " rows"); 

            DICOM_data.table_data.emplace_back( std::make_shared<Sparse_Table>() );
            DICOM_data.table_data.back()->table = tab;

            bfit = Filenames.erase( bfit ); 
            continue;
        }catch(const std::exception &e){
            FUNCINFO("Unable to load as CSV file: '" << e.what() << "'");
        };

        //Skip the file. It might be destined for some other loader.
        ++bfit;
    }
            
    //If nothing was loaded, do not post-process.
    const size_t N2 = Filenames.size();
    if(N == N2){
        return true;
    }

    return true;
}

