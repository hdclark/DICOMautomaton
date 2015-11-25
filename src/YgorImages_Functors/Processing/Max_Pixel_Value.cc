
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


bool CondenseMaxPixel(planar_image_collection<float,double>::images_list_it_t first_img_it,
                      std::list<planar_image_collection<float,double>::images_list_it_t> selected_img_its,
                      std::list<std::reference_wrapper<planar_image_collection<float,double>>>,
                      std::list<std::reference_wrapper<contour_collection<double>>>, 
                      std::experimental::any ){

    //This routine computes the minimum pixel value for each group of voxels.

    //Record the min and max actual pixel values for windowing purposes.
    Stats::Running_MinMax<float> minmax_pixel;

    //Loop over the rows, columns, channels, and finally selected_images in the time course.
    for(auto row = 0; row < first_img_it->rows; ++row){
        for(auto col = 0; col < first_img_it->columns; ++col){
            for(auto chan = 0; chan < first_img_it->channels; ++chan){
                std::vector<float> pixel_vals;
                pixel_vals.reserve(selected_img_its.size());
                for(auto &an_img_it : selected_img_its){
                    const auto pixel_val = an_img_it->value(row, col, chan);
                    pixel_vals.push_back( pixel_val );
                }

                const auto newval = *std::max_element(pixel_vals.begin(), pixel_vals.end());
                first_img_it->reference(row, col, chan) = newval;
                minmax_pixel.Digest(newval);

            }//Loop over channels.
        } //Loop over cols
    } //Loop over rows

    UpdateImageDescription( std::ref(*first_img_it), "Max(pixel) Map" );
    UpdateImageWindowCentreWidth( std::ref(*first_img_it), minmax_pixel );

    return true;
}

