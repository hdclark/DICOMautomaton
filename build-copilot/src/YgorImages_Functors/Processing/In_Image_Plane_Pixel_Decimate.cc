
#include <functional>
#include <list>
#include <map>
#include <stdexcept>
#include <string>
#include <cstdint>

#include "../ConvenienceRoutines.h"
#include "In_Image_Plane_Pixel_Decimate.h"
#include "YgorImages.h"
#include "YgorMath.h"
#include "YgorMisc.h"
#include "YgorLog.h"
#include "YgorStats.h"       //Needed for Stats:: namespace.
#include "YgorString.h"      //Needed for GetFirstRegex(...)

bool InImagePlanePixelDecimate(
                    planar_image_collection<float,double>::images_list_it_t first_img_it,
                    std::list<planar_image_collection<float,double>::images_list_it_t> selected_img_its,
                    std::list<std::reference_wrapper<planar_image_collection<float,double>>>,
                    std::list<std::reference_wrapper<contour_collection<double>>>, 
                    int64_t ScaleFactorR, int64_t ScaleFactorC,
                    std::any ){

    //This routine reduces the number of pixels in an image by computing some sort of aggregate of a
    // block of adjacent pixels. 
    //
    // This routine does NOT requires that the outgoing pixel edge length scaling factor be a 
    // clean divisor of the existing dimensions, but ensuring so will eliminate boundary 
    // short-sampling effects.
    //
    // If your incoming image size is 512x512, you can specify powers of 2 up to 512:
    //   • 1   (Should result in no averaging, but will probably result in NaN's due to the
    //          "average" of a single entity being kind of poorly defined),
    //   • 2   (Outgoing pixels are 2x2 as large as the original pixels.)
    //   • 4   (Outgoing pixels are 4x4 as large as the original pixels.)
    //   • 8   (Outgoing pixels are 8x8 as large as the original pixels.)
    //   • 16  (Outgoing pixels are 16x16 as large as the original pixels.)
    //   • 32  (32x32 etc..)
    //   • 64  (64x64 etc..) 
    //   • 128 (128x128 etc..)
    //   • 256 (256x256 etc..)
    //   • 512 (512x512 etc.. Will result in a single pixel!)
    //
    // If your incoming image size is 513x513, you can still specify powers of 2, but 
    // there will be a strip of pixels on the boundary that are effectively not averaged.
    //

    const auto NumberOfRowsRequired = static_cast<int64_t>( std::ceil( 1.0 * first_img_it->rows / ScaleFactorR )  );
    const auto NumberOfColsRequired = static_cast<int64_t>( std::ceil( 1.0 * first_img_it->columns / ScaleFactorC ) );

    if((NumberOfRowsRequired * ScaleFactorR) != first_img_it->rows){
        YLOGWARN("ScaleFactorR should be a clean divisor of the image size to avoid boundary effect: Rows = " +
                                std::to_string(first_img_it->rows));
    }else if((NumberOfColsRequired * ScaleFactorC) != first_img_it->columns){
        YLOGWARN("ScaleFactorC should be a clean divisor of the image size to avoid boundary effect: Columns = " +
                                std::to_string(first_img_it->columns));
    } 

    vec3<double> newOffset = first_img_it->offset;
    newOffset -= first_img_it->row_unit * first_img_it->pxl_dx * 0.5;
    newOffset -= first_img_it->col_unit * first_img_it->pxl_dy * 0.5;
    newOffset += first_img_it->row_unit * first_img_it->pxl_dx * ScaleFactorR * 0.5;
    newOffset += first_img_it->col_unit * first_img_it->pxl_dy * ScaleFactorR * 0.5;

    if(selected_img_its.size() != 1) YLOGERR("This routine operates on individual images only");
 
    //Make a destination image that has twice the linear dimensions as the input image.
    planar_image<float,double> working;
    working.init_buffer( NumberOfRowsRequired, 
                         NumberOfColsRequired, 
                         first_img_it->channels );
    working.init_spatial( first_img_it->pxl_dx * ScaleFactorR,
                          first_img_it->pxl_dy * ScaleFactorC,
                          first_img_it->pxl_dz,
                          first_img_it->anchor,
                          newOffset );
    working.init_orientation( first_img_it->row_unit,
                              first_img_it->col_unit );
    working.metadata = first_img_it->metadata;

    //Record the min and max actual pixel values for windowing purposes.
    Stats::Running_MinMax<float> minmax_pixel;

    //Loop over the rows, columns, and channels.
    for(auto row = 0; row < working.rows; ++row){
        for(auto col = 0; col < working.columns; ++col){
            for(auto chan = 0; chan < working.channels; ++chan){
                const int64_t old_row_min = row * ScaleFactorR;
                const int64_t old_row_max = old_row_min + ScaleFactorR - 1;
                const int64_t old_col_min = col * ScaleFactorC;
                const int64_t old_col_max = old_col_min + ScaleFactorC - 1;

                const auto newval = first_img_it->block_average(old_row_min, old_row_max, old_col_min, old_col_max, chan);

                working.reference(row, col, chan) = newval;
                minmax_pixel.Digest(newval);
            }//Loop over channels.
        } //Loop over cols
    } //Loop over rows

    //Replace the old image data with the new image data.
    *first_img_it = working;

    UpdateImageDescription( std::ref(*first_img_it), 
                            "In-plane Pixel Decimated "_s 
                            + std::to_string(ScaleFactorR) + "x "
                            + std::to_string(ScaleFactorC) + "x ");
    UpdateImageWindowCentreWidth( std::ref(*first_img_it), minmax_pixel );

    first_img_it->metadata["Rows"] = std::to_string(first_img_it->rows);
    first_img_it->metadata["Columns"] = std::to_string(first_img_it->columns);

    return true;
}

