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
            DICOM_data.table_data.emplace_back( std::make_shared<Sparse_Table>() );
            auto* tab_ptr = &( DICOM_data.table_data.back()->table );

            std::ifstream is(Filename.string(), std::ios::in | std::ios::binary);
            tab_ptr->read_csv(is);

            // Ensure a minimal amount of metadata is present for image purposes.
            auto l_meta = coalesce_metadata_for_basic_table(tab_ptr->metadata);
            l_meta.merge(tab_ptr->metadata);
            tab_ptr->metadata = l_meta;
            tab_ptr->metadata["Filename"] = Filename.string();

            FUNCINFO("Loaded CSV file with " << tab_ptr->data.size() << " rows"); 

            bfit = Filenames.erase( bfit ); 
            continue;
        }catch(const std::exception &e){
            FUNCINFO("Unable to load as CSV file: '" << e.what() << "'");
            DICOM_data.table_data.pop_back();
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

