//STB_File_Loader.cc - A part of DICOMautomaton 2023. Written by hal clark.
//
// This program loads many common 8-bit image files (jpg, png, bmp, etc.) using the stb/nothings image loader.

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


#include "Metadata.h"
#include "Structs.h"
#include "YgorImages.h"
#include "YgorImagesIO.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorLog.h"

// One or more DCMA dependencies appear to bundle a version of the STB libraries. However, I'm not sure which dependency
// (SFML?), which version is bundled, and whether I can rely on it to be available reliably. Instead, we include another
// potentially separate version wrapped in a namespace to avoid linking issues.
namespace dcma_stb {
    #define STB_IMAGE_IMPLEMENTATION
    #include "stbnothings20230607/stb_image.h"
} // namespace dcma_stb.

static
planar_image_collection<float, double>
ReadUsingSTB(const std::string &fname){
    planar_image_collection<float, double> cc;
	int width = 0;
    int height = 0;
    int channels_actual = 0;
    const int channels_requested = 0;
	unsigned char* pixels = dcma_stb::stbi_load(fname.c_str(), &width, &height, &channels_actual, channels_requested);

    if( (pixels != nullptr)
    &&  (0 < width)
    &&  (0 < height)
    &&  (0 < channels_actual) ){
        const int64_t rows = static_cast<int64_t>(height);
        const int64_t cols = static_cast<int64_t>(width);
        const int64_t chns = static_cast<int64_t>(channels_actual);
        const double pxl_dx = 1.0;
        const double pxl_dy = 1.0;
        const double pxl_dz = 1.0;
        const vec3<double> anchor(0.0, 0.0, 0.0);
        const vec3<double> offset(0.0, 0.0, 0.0);
        const vec3<double> row_unit(0.0, 1.0, 0.0);
        const vec3<double> col_unit(1.0, 0.0, 0.0);

        cc.images.emplace_back();
        cc.images.back().init_buffer( rows, cols, chns );
        cc.images.back().init_spatial( pxl_dx, pxl_dy, pxl_dz, anchor, offset );
        cc.images.back().init_orientation( row_unit, col_unit );

        unsigned char* l_pixels = pixels;
        for(int64_t row = 0; row < rows; ++row){
            for(int64_t col = 0; col < cols; ++col){
                for(int64_t chn = 0; chn < chns; ++chn){
                    cc.images.back().reference(row, col, chn) = static_cast<float>(*l_pixels);
                    ++l_pixels;
                }
            }
        }
    }

	dcma_stb::stbi_image_free(pixels);
	return cc;
}

bool Load_From_STB_Files( Drover &DICOM_data,
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

        auto imgcoll = ReadUsingSTB(Filename.string());
        bool read_successfully = !imgcoll.images.empty();
        if(read_successfully){

            // Fill in any missing metadata in a consistent way, but honour any existing metadata that might be present.
            // Evolve the metadata so images loaded together stay linked, but allow existing metadata to take
            // precedent.
            for(auto& animg : imgcoll.images){
                auto ll_meta = animg.metadata;
                inject_metadata( l_meta, std::move(ll_meta) ); // ll_meta takes priority.
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
            YLOGINFO("Unable to load file using STB library");
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
