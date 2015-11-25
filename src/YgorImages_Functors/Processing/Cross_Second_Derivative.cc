
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


bool CrossSecondDerivative(
                    planar_image_collection<float,double>::images_list_it_t first_img_it,
                    std::list<planar_image_collection<float,double>::images_list_it_t> selected_img_its,
                    std::list<std::reference_wrapper<planar_image_collection<float,double>>>,
                    std::list<std::reference_wrapper<contour_collection<double>>>, 
                    std::experimental::any ){

    //This routine computes a 'cross' second-order partial derivative along the row- and column-axes on the
    // pixel values (ignoring pixel shape and real-space coordinates). The partial derivative is:
    //    $\frac{\partial^{2} P(row,col)}{\partial_{row} \partial_{col}}$
    // It might be useful for helping to visualize boundaries, but isn't suitable for physical calculations.

    if(selected_img_its.size() != 1) FUNCERR("This routine operates on individual images only");
 

    //Make a destination image that has twice the linear dimensions as the input image.
    planar_image<float,double> working = *first_img_it;

    //Paint all pixels black.
    //working.fill_pixels(static_cast<floatt>(0));

    //Record the min and max actual pixel values for windowing purposes.
    Stats::Running_MinMax<float> minmax_pixel;

    //Loop over the rows, columns, and channels.
    for(auto row = 0; row < working.rows; ++row){
        for(auto col = 0; col < working.columns; ++col){
            for(auto chan = 0; chan < working.channels; ++chan){
                const auto newval_R = first_img_it->cross_second_derivative_centered_finite_difference(row, col, chan);
                const auto newval = static_cast<float>(std::floor(newval_R));
                working.reference(row, col, chan) = newval;
                minmax_pixel.Digest(newval);
            }//Loop over channels.
        } //Loop over cols
    } //Loop over rows

    //Replace the old image data with the new image data.
    *first_img_it = working;


    UpdateImageDescription( std::ref(*first_img_it), "Cross 2nd Partial Deriv. (Pixel Coord. Space)" );
    UpdateImageWindowCentreWidth( std::ref(*first_img_it), minmax_pixel );

    return true;
}

