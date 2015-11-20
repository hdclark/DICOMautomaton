
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

bool InImagePlaneBilinearSupersample(
                    planar_image_collection<float,double>::images_list_it_t first_img_it,
                    std::list<planar_image_collection<float,double>::images_list_it_t> selected_img_its,
                    std::list<std::reference_wrapper<planar_image_collection<float,double>>>,
                    std::list<std::reference_wrapper<contour_collection<double>>>, 
                    std::experimental::any ){

    //This routine supersamples images, making them have ScaleFactorx as many pixels. It uses a simple
    // in-plane bilinear supersampling technique that is completely oblivious to the pixel dimensions.
    // Only nearest-neighbour adjacent pixels are used. "Mirror" boundaries are used.
    // This routine would probably only be useful for viewing purposes.

    if(selected_img_its.size() != 1) FUNCERR("This routine operates on individual images only");
 
    const auto ScaleFactor = 5; //Should be a positive integer.
    const auto ScaleFactorR = static_cast<double>(ScaleFactor);

    //Make a destination image that has twice the linear dimensions as the input image.
    planar_image<float,double> working;
    working.init_buffer( first_img_it->rows * ScaleFactor, 
                         first_img_it->columns * ScaleFactor, 
                         first_img_it->channels );
    working.init_spatial( first_img_it->pxl_dx / ScaleFactorR,
                          first_img_it->pxl_dy / ScaleFactorR,
                          first_img_it->pxl_dz,
                          first_img_it->anchor,
                          first_img_it->offset );
    working.init_orientation( first_img_it->row_unit,
                              first_img_it->col_unit );
    working.metadata = first_img_it->metadata;

    //Paint all pixels black.
    //working.fill_pixels(static_cast<float>(0));

    //Record the min and max actual pixel values for windowing purposes.
    float curr_min_pixel = std::numeric_limits<float>::max();
    float curr_max_pixel = std::numeric_limits<float>::min();

    //Loop over the rows, columns, and channels.
    for(auto row = 0; row < working.rows; ++row){
        for(auto col = 0; col < working.columns; ++col){
            for(auto chan = 0; chan < working.channels; ++chan){
                const double row_sample_pos = (2.0*row + 1.0 - ScaleFactorR)/(2.0*ScaleFactorR);
                const double col_sample_pos = (2.0*col + 1.0 - ScaleFactorR)/(2.0*ScaleFactorR);
                const auto newval = first_img_it->bilinearly_interpolate_in_pixel_number_space(row_sample_pos, col_sample_pos, chan);

                working.reference(row, col, chan) = newval;
                curr_min_pixel = std::min(curr_min_pixel, newval);
                curr_max_pixel = std::max(curr_max_pixel, newval);
            }//Loop over channels.
        } //Loop over cols
    } //Loop over rows

    //Replace the old image data with the new image data.
    *first_img_it = working;

    //Alter the first image's metadata to reflect that averaging has occurred. You might want to consider
    // a selective whitelist approach so that unique IDs are not duplicated accidentally.
    first_img_it->metadata["Description"] = "In-plane Bilinear Supersampled "; 
    first_img_it->metadata["Description"] += std::to_string(ScaleFactor) + "x";
    first_img_it->metadata["Rows"] = std::to_string(first_img_it->rows);
    first_img_it->metadata["Columns"] = std::to_string(first_img_it->columns);


    //Specify a reasonable default window.
    if( (curr_min_pixel != std::numeric_limits<float>::max())
    &&  (curr_max_pixel != std::numeric_limits<float>::min()) ){
        const float WindowCenter = (curr_min_pixel/2.0) + (curr_max_pixel/2.0);
        const float WindowWidth  = 2.0 + curr_max_pixel - curr_min_pixel;
        first_img_it->metadata["WindowValidFor"] = first_img_it->metadata["Description"];
        first_img_it->metadata["WindowCenter"]   = Xtostring(WindowCenter);
        first_img_it->metadata["WindowWidth"]    = Xtostring(WindowWidth);
    }

    return true;
}

