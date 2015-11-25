
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

#include "../ConvenienceRoutines.h"

bool LogScalePixels(planar_image_collection<float,double>::images_list_it_t first_img_it,
                    std::list<planar_image_collection<float,double>::images_list_it_t> selected_img_its,
                    std::list<std::reference_wrapper<planar_image_collection<float,double>>>,
                    std::list<std::reference_wrapper<contour_collection<double>>>, 
                    std::experimental::any ){

    //This routine scales pixels logarithmically, leaving pixels with intensity 0 as-is.
    // Logarithmic scaling is frequently used to help discern relative intensities, similar
    // to how humans perceive sound logarithmically (i.e., Decibels).

    if(selected_img_its.size() != 1) FUNCERR("This routine operates on individual images only");


    //Record the min and max (outgoing) pixel values for windowing purposes.
    Stats::Running_MinMax<float> minmax_pixel;

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
                    minmax_pixel.Digest(newval);
                }
            }//Loop over channels.
        } //Loop over cols
    } //Loop over rows

    UpdateImageDescription( std::ref(*first_img_it), "Log-Scaled Pixels" );
    UpdateImageWindowCentreWidth( std::ref(*first_img_it), minmax_pixel );

    return true;
}

