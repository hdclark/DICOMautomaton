//Common_Image_File_Loader.cc - A part of DICOMautomaton 2023. Written by hal clark.
//
// This program loads many common 8-bit image files (jpg, png, bmp, etc.).

#include <cmath>
#include <cstdint>
#include <exception>
#include <fstream>
#include <list>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>    
#include <filesystem>
#include <cstdlib>

#include "YgorImages.h"
#include "YgorImagesIO.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorLog.h"

#include "Metadata.h"
#include "Structs.h"
#include "STB_Shim.h"

bool Load_From_Common_Image_Files( Drover &DICOM_data,
                                   std::map<std::string,std::string> & /* InvocationMetadata */,
                                   const std::string &,
                                   std::list<std::filesystem::path> &Filenames ){

    //This routine will attempt to load raster images on an individual file basis. Files that are not successfully loaded
    // are not consumed so that they can be passed on to the next loading stage as needed. 
    //
    // Note: This routine returns false only iff a file is suspected of being suited for this loader, but could not be
    //       loaded (e.g., the file seems appropriate, but a parsing failure was encountered).
    //
    if(Filenames.empty()) return true;

    DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>() );

    size_t i = 0;
    const size_t N = Filenames.size();
    auto l_meta = coalesce_metadata_for_basic_image({});

    auto bfit = Filenames.begin();
    while(bfit != Filenames.end()){
        YLOGINFO("Parsing file #" << i+1 << "/" << N << " = " << 100*(i+1)/N << "%");
        ++i;
        const auto Filename = *bfit;

        auto imgcoll = ReadImageUsingSTB(Filename.string());
        bool read_successfully = !imgcoll.images.empty();
        if(read_successfully){

            // Fill in any missing metadata in a consistent way, but honour any existing metadata that might be present.
            // Evolve the metadata so images loaded together stay linked, but allow existing metadata to take
            // precedent.
            for(auto& animg : imgcoll.images){
                auto ll_meta = animg.metadata;
                inject_metadata( l_meta, std::move(ll_meta), metadata_preprocessing::none ); // ll_meta takes priority.
                animg.metadata = l_meta;
                animg.metadata["Filename"] = Filename.string();
                l_meta = coalesce_metadata_for_basic_image(l_meta, meta_evolve::iterate); // Evolve for next image.

                YLOGINFO("Loaded raster image with dimensions " 
                         << animg.rows << " x " << animg.columns
                         << " and " << animg.channels << " channels");
            }

            DICOM_data.image_data.back()->imagecoll.images.splice(
                std::end(DICOM_data.image_data.back()->imagecoll.images),
                std::move(imgcoll.images) );
            imgcoll.images.clear();

            bfit = Filenames.erase( bfit ); 
            continue;
        }else{
            YLOGINFO("Unable to load file as a common raster image format");
        }

        //Skip the file. It might be destined for some other loader.
        ++bfit;
    }
            
    //If nothing was loaded, do not post-process.
    const size_t N2 = Filenames.size();
    if(N == N2){
        DICOM_data.image_data.pop_back();
        return true;
    }

    return true;
}
