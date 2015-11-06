
#include <list>
#include <functional>
#include <limits>
#include <map>
#include <cmath>
#include <algorithm>

#include "YgorMisc.h"
#include "YgorMath.h"
#include "YgorImages.h"
#include "YgorStats.h"       //Needed for Stats:: namespace.
#include "YgorAlgorithms.h"  //Needed for For_Each_In_Parallel<..>(...)
#include "YgorString.h"      //Needed for GetFirstRegex(...)
#include "YgorPlot.h"

bool LogScalePixels(planar_image_collection<float,double>::images_list_it_t first_img_it,
                    std::list<planar_image_collection<float,double>::images_list_it_t> selected_img_its,
                    std::list<std::reference_wrapper<contour_collection<double>>>, 
                    std::experimental::any ){

    //This routine scales pixels logarithmically, leaving pixels with intensity 0 as-is.
    // Logarithmic scaling is frequently used to help discern relative intensities, similar
    // to how humans perceive sound logarithmically (i.e., Decibels).

    if(selected_img_its.size() != 1) FUNCERR("This routine operates on individual images only");


    //Record the min and max (outgoing) pixel values for windowing purposes.
    float curr_min_pixel = std::numeric_limits<float>::max();
    float curr_max_pixel = std::numeric_limits<float>::min();

    //Loop over the rows, columns, and channels.
    for(auto row = 0; row < first_img_it->rows; ++row){
        for(auto col = 0; col < first_img_it->columns; ++col){
            for(auto chan = 0; chan < first_img_it->channels; ++chan){
                const auto pixel_val = first_img_it->value(row, col, chan);
                if(pixel_val <= static_cast<float>(0)){
                    first_img_it->reference(row, col, chan) = std::numeric_limits<float>::quiet_NaN();
                }else{
                    const auto pv_as_dbl = static_cast<double>(pixel_val);
                    const auto theln = std::log(pv_as_dbl) * 1'000'000.0;
                    const auto newval = static_cast<float>(theln);

                    first_img_it->reference(row, col, chan) = newval;
                    curr_min_pixel = std::min(curr_min_pixel, newval);
                    curr_max_pixel = std::max(curr_max_pixel, newval);
                }
            }//Loop over channels.
        } //Loop over cols
    } //Loop over rows

    //Alter the first image's metadata to reflect that averaging has occurred. You might want to consider
    // a selective whitelist approach so that unique IDs are not duplicated accidentally.
    first_img_it->metadata["Description"] = "Pixels logarithmically scaled";

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

