
#include <list>
#include <functional>
#include <limits>
#include <map>
#include <cmath>
#include <algorithm>
#include <string>

#include "YgorMisc.h"
#include "YgorMath.h"
#include "YgorImages.h"
#include "YgorStats.h"       //Needed for Stats:: namespace.
#include "YgorAlgorithms.h"  //Needed for For_Each_In_Parallel<..>(...)
#include "YgorString.h"      //Needed for GetFirstRegex(...)
#include "YgorPlot.h"

#include "../ConvenienceRoutines.h"

#include "In_Image_Plane_Pixel_Decimate.h"

bool InImagePlanePixelDecimate(
                    planar_image_collection<float,double>::images_list_it_t first_img_it,
                    std::list<planar_image_collection<float,double>::images_list_it_t> selected_img_its,
                    std::list<std::reference_wrapper<planar_image_collection<float,double>>>,
                    std::list<std::reference_wrapper<contour_collection<double>>>, 
                    long int ScaleFactorR, long int ScaleFactorC,
                    std::experimental::any ){

    //This routine reduces the number of pixels in an image by computing some sort of aggregate of a
    // block of adjacent pixels. 
    //
    // This routine presently requires that the outgoing pixel edge length scaling factor be a 
    // clean divisor of the existing dimensions so that pixels have well-defined membership in the
    // outgoing pixels. (The alternative would be to 'blur' pixels that straddle a boundary, which 
    // would take much longer -- but it very much possible if you need to implement it. Just use a
    // weighted average and prepare yourself for some fuzzy boundary calculations!)
    //
    // If your imcoming image size is 512x512, you can specify powers of 8 up to 512:
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

    const auto NumberOfRowsRequired = first_img_it->rows / ScaleFactorR;
    const auto NumberOfColsRequired = first_img_it->columns / ScaleFactorC;

    if((NumberOfRowsRequired * ScaleFactorR) != first_img_it->rows){
        throw std::logic_error("ScaleFactorR must be a clean divisor of the image size. Rows = " +
                                std::to_string(first_img_it->rows));
    }else if((NumberOfColsRequired * ScaleFactorC) != first_img_it->columns){
        throw std::logic_error("ScaleFactorC must be a clean divisor of the image size. Columns = " +
                                std::to_string(first_img_it->columns));
    } 

    vec3<double> newOffset = first_img_it->offset;
    newOffset -= first_img_it->row_unit * first_img_it->pxl_dx * 0.5;
    newOffset -= first_img_it->col_unit * first_img_it->pxl_dy * 0.5;
    newOffset += first_img_it->row_unit * first_img_it->pxl_dx * ScaleFactorR * 0.5;
    newOffset += first_img_it->col_unit * first_img_it->pxl_dy * ScaleFactorR * 0.5;

    if(selected_img_its.size() != 1) FUNCERR("This routine operates on individual images only");
 
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
                const long int old_row_min = row * ScaleFactorR;
                const long int old_row_max = old_row_min + ScaleFactorR - 1;
                const long int old_col_min = col * ScaleFactorC;
                const long int old_col_max = old_col_min + ScaleFactorC - 1;

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

