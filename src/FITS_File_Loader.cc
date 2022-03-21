//FITS_File_Loader.cc - A part of DICOMautomaton 2016. Written by hal clark.
//
// This program loads image data FITS files. It is basic and can only currently deal with FITS files 
// containing a single image slice (and probably only images exported using the Ygor exporter).
//

#include <cmath>
#include <cstdint>
#include <exception>
#include <fstream>
#include <list>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>    
//#include <cfenv>              //Needed for std::feclearexcept(FE_ALL_EXCEPT).

#include <filesystem>
#include <cstdlib>            //Needed for exit() calls.

#include "Metadata.h"
#include "Structs.h"
#include "YgorImages.h"
#include "YgorImagesIO.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.


bool Load_From_FITS_Files( Drover &DICOM_data,
                           std::map<std::string,std::string> & /* InvocationMetadata */,
                           const std::string &,
                           std::list<std::filesystem::path> &Filenames ){

    //This routine will attempt to load FITS images on an individual file basis. Files that are not successfully loaded
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
        FUNCINFO("Parsing file #" << i+1 << "/" << N << " = " << 100*(i+1)/N << "%");
        ++i;
        const auto Filename = *bfit;

        //First, try planar_images that have been exported in the expected format.
        try{
            auto animg = ReadFromFITS<float,double>(Filename.string());

            //Set some default parameters if none were included in the file metadata.
            if(!std::isfinite(animg.pxl_dx) 
            || !std::isfinite(animg.pxl_dy)
            || !std::isfinite(animg.pxl_dz)
            || (animg.pxl_dx <= 0.0)
            || (animg.pxl_dy <= 0.0) 
            || (animg.pxl_dz <= 0.0) 
            || !std::isfinite(animg.anchor.length())
            || !std::isfinite(animg.offset.length()) ){
                animg.init_spatial( 1.0, 1.0, 1.0, vec3<double>(0.0, 0.0, 0.0), vec3<double>(0.0, 0.0, 0.0));
            }
            if(!std::isfinite(animg.row_unit.length())
            || !std::isfinite(animg.col_unit.length())
            || (animg.row_unit.length() < 1E-5) 
            || (animg.col_unit.length() < 1E-5)  ){
                animg.init_orientation( vec3<double>(0.0, 1.0, 0.0), vec3<double>(1.0, 0.0, 0.0) );
            }
            if(!std::isfinite(animg.rows)
            || !std::isfinite(animg.columns)
            || !std::isfinite(animg.channels) ){
                throw std::runtime_error("FITS file missing key image parameters. Cannot continue.");
            }

            auto ll_meta = l_meta;
            inject_metadata(animg.metadata, std::move(ll_meta));
            animg.metadata["Filename"] = Filename.string();

            FUNCINFO("Loaded FITS file with dimensions " 
                     << animg.rows << " x " << animg.columns
                     << " and " << animg.channels << " channels");

            DICOM_data.image_data.back()->imagecoll.images.emplace_back( animg );
            bfit = Filenames.erase( bfit ); 
            continue;
        }catch(const std::exception &e){
            FUNCINFO("Unable to load as FITS file with float,double types: '" << e.what() << "'");
        };

        //Then try the most likely format as exported by other programs.
        try{
            auto animg = ReadFromFITS<uint8_t,double>(Filename.string());

            //Set some default parameters if none were included in the file metadata.
            if(!std::isfinite(animg.pxl_dx) 
            || !std::isfinite(animg.pxl_dy)
            || !std::isfinite(animg.pxl_dz)
            || (animg.pxl_dx <= 0.0)
            || (animg.pxl_dy <= 0.0) 
            || (animg.pxl_dz <= 0.0) 
            || !std::isfinite(animg.anchor.length())
            || !std::isfinite(animg.offset.length()) ){
                animg.init_spatial( 1.0, 1.0, 1.0, vec3<double>(0.0, 0.0, 0.0), vec3<double>(0.0, 0.0, 0.0));
            }
            if(!std::isfinite(animg.row_unit.length())
            || !std::isfinite(animg.col_unit.length())
            || (animg.row_unit.length() < 1E-5) 
            || (animg.col_unit.length() < 1E-5)  ){
                animg.init_orientation( vec3<double>(0.0, 1.0, 0.0), vec3<double>(1.0, 0.0, 0.0) );
            }
            if(!std::isfinite(animg.rows)
            || !std::isfinite(animg.columns)
            || !std::isfinite(animg.channels) ){
                throw std::runtime_error("FITS file missing key image parameters. Cannot continue.");
            }

            planar_image<float,double> animg2;
            animg2.cast_from(animg);
            auto ll_meta = l_meta;
            inject_metadata(animg2.metadata, std::move(ll_meta));
            animg2.metadata["Filename"] = Filename.string();

            FUNCINFO("Loaded FITS file with dimensions " 
                     << animg2.rows << " x " << animg2.columns
                     << " and " << animg2.channels << " channels");

            DICOM_data.image_data.back()->imagecoll.images.emplace_back( animg2 );
            bfit = Filenames.erase( bfit ); 
            continue;
        }catch(const std::exception &e){
            FUNCINFO("Unable to load as FITS file with uint8_t,double types: '" << e.what() << "'");
        };

        // Iterate metadata for next file.
        l_meta = coalesce_metadata_for_basic_image(l_meta, meta_evolve::iterate);

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
