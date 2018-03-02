
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

#include "In_Image_Plane_Sharpen.h"

bool InPlaneImageSharpen(
                    planar_image_collection<float,double>::images_list_it_t first_img_it,
                    std::list<planar_image_collection<float,double>::images_list_it_t> selected_img_its,
                    std::list<std::reference_wrapper<planar_image_collection<float,double>>>,
                    std::list<std::reference_wrapper<contour_collection<double>>>, 
                    std::experimental::any user_data){

    InPlaneImageSharpenUserData *user_data_s;
    try{
        user_data_s = std::experimental::any_cast<InPlaneImageSharpenUserData *>(user_data);
    }catch(const std::exception &e){
        FUNCWARN("Unable to cast user_data to appropriate format. Cannot continue with computation");
        return false;
    }

    //This routine uses a selected estimator to approximate a sharpen operator.

    if(selected_img_its.size() != 1) throw std::invalid_argument("This routine operates on individual images only.");

    //Make a destination image that has twice the linear dimensions as the input image.
    planar_image<float,double> working = *first_img_it;

    //Record the min and max actual pixel values for windowing purposes.
    Stats::Running_MinMax<float> minmax_pixel;

    //Loop over the rows, columns, and channels.
    for(auto row = 0; row < working.rows; ++row){
        for(auto col = 0; col < working.columns; ++col){
            for(auto chan = 0; chan < working.channels; ++chan){
                auto newval = std::numeric_limits<float>::quiet_NaN();

                if(false){
                }else if(user_data_s->estimator == SharpenEstimator::sharpen_3x3){
                    newval = first_img_it->fixed_sharpen_3x3(row, col, chan);

                }else if(user_data_s->estimator == SharpenEstimator::unsharp_mask_5x5){
                    newval = first_img_it->fixed_unsharp_mask_5x5(row, col, chan);

                }else{
                    throw std::invalid_argument("Unrecognized user-provided sharpen estimator.");
                }

                working.reference(row, col, chan) = newval;
                minmax_pixel.Digest(newval);
            }//Loop over channels.
        } //Loop over cols
    } //Loop over rows


    //Replace the old image data with the new image data.
    *first_img_it = working;

    //Update the image metadata. 
    std::string img_desc;
    if(false){
    }else if(user_data_s->estimator == SharpenEstimator::sharpen_3x3){
        img_desc += "Box sharpen (fixed; 3x3)";

    }else if(user_data_s->estimator == SharpenEstimator::unsharp_mask_5x5){
        img_desc += "Unsharp masked (fixed; 5x5)";

    }else{
        throw std::invalid_argument("Unrecognized user-provided sharpen estimator.");
    }
    img_desc += " (in pixel coord.s)";

    UpdateImageDescription( std::ref(*first_img_it), img_desc );
    UpdateImageWindowCentreWidth( std::ref(*first_img_it), minmax_pixel );

    return true;
}

