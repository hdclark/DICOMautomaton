
#include <list>
#include <functional>
#include <limits>
#include <map>
#include <cmath>

#include "YgorMisc.h"
#include "YgorMath.h"
#include "YgorImages.h"
#include "YgorStats.h"       //Needed for Stats:: namespace.
#include "YgorAlgorithms.h"  //Needed for For_Each_In_Parallel<..>(...)
#include "YgorString.h"      //Needed for GetFirstRegex(...)
#include "YgorPlot.h"

bool CTPerfEnormousPixelFilter(planar_image_collection<float,double>::images_list_it_t first_img_it,
                               std::list<planar_image_collection<float,double>::images_list_it_t> selected_img_its,
                               std::list<std::reference_wrapper<planar_image_collection<float,double>>>,
                               std::list<std::reference_wrapper<contour_collection<double>>>,
                               std::experimental::any ){

    //This routine filters out outrageously high-value pixels which are emitted for whatever reason by some CT scanners.
    // There is no way I can see to guarantee that legitimate pixels will not be caught in the cross-fire. You'll have 
    // to test.

    //Ensure only single images are grouped together.
    if(selected_img_its.size() != 1){
        FUNCWARN("This routine works on single images. It cannot deal with grouped images");
        return false;
    }

    //Record the min and max actual pixel values for windowing purposes.
    float curr_min_pixel = std::numeric_limits<float>::max();
    float curr_max_pixel = std::numeric_limits<float>::min();

    //Loop over the rows, columns, channels, and finally selected_images in the time course.
    for(auto row = 0; row < first_img_it->rows; ++row){
        for(auto col = 0; col < first_img_it->columns; ++col){
            for(auto chan = 0; chan < first_img_it->channels; ++chan){

                //We assume the images will be in Hounsfield units. Air is -1000 while bone can be 3000-4000. 
                // Therefore, a reasonable upper bound on the raw pixel value will be something like 2E4.
                const auto val = first_img_it->value(row, col, chan);
                const auto newval = (val < static_cast<float>(2E4)) ? val : std::numeric_limits<float>::quiet_NaN();

                first_img_it->reference(row, col, chan) = newval;
                if(std::isfinite(newval)){
                    curr_min_pixel = std::min(curr_min_pixel, newval);
                    curr_max_pixel = std::max(curr_max_pixel, newval);
                }

            }//Loop over channels.
        } //Loop over cols
    } //Loop over rows

    //Specify a reasonable default window.
    const float WindowCenter = (curr_min_pixel/2.0) + (curr_max_pixel/2.0);
    const float WindowWidth  = 2.0 + curr_max_pixel - curr_min_pixel;
    first_img_it->metadata["WindowValidFor"] = "Enormous pixel filtered image";
    first_img_it->metadata["WindowCenter"]   = Xtostring(WindowCenter);
    first_img_it->metadata["WindowWidth"]    = Xtostring(WindowWidth);

    //Alter the first image's metadata to reflect that averaging has occurred. You might want to consider
    // a selective whitelist approach so that unique IDs are not duplicated accidentally.
    first_img_it->metadata["Description"] = "Enormous pixel filtered image";

    return true;
}

