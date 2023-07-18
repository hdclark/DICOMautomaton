//STB_Shim.cc - A part of DICOMautomaton 2023. Written by hal clark.
//
// This program loads many common 8-bit image files (jpg, png, bmp, etc.) using the stb/nothings image loader.

#include <cmath>
#include <cstdint>
#include <exception>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>    
#include <vector>
#include <filesystem>
#include <cstdlib>

#include "YgorImages.h"
#include "YgorImagesIO.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorLog.h"

#include "Metadata.h"
#include "Structs.h"

// One or more DCMA dependencies appear to bundle a version of the STB libraries. However, I'm not sure which dependency
// (SFML?), which version is bundled, and whether I can rely on it to be available reliably. Instead, we include another
// potentially separate version wrapped in a namespace to avoid multiple definition issues. Compiling this shim file as
// a library should also reduce the likelihood of clash.
namespace dcma_stb {
    #define STB_IMAGE_STATIC
    #define STB_IMAGE_IMPLEMENTATION
    #include "stbnothings20230607/stb_image.h"
} // namespace dcma_stb.


planar_image_collection<float, double>
ReadImageUsingSTB(const std::string &fname){
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


planar_image_collection<float, double>
ReadImageUsingSTB(const std::vector<uint8_t> &blob){
    planar_image_collection<float, double> cc;
	int width = 0;
    int height = 0;
    int channels_actual = 0;
    const int channels_requested = 0;
    using uchar_t = const dcma_stb::stbi_uc *;
	unsigned char* pixels = dcma_stb::stbi_load_from_memory(static_cast<uchar_t>(blob.data()), blob.size(),
                                                            &width, &height, &channels_actual, channels_requested);


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

