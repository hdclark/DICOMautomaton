
#include <list>
#include <functional>
#include <limits>
#include <map>
#include <cmath>

#include "YgorMisc.h"
#include "YgorMath.h"
#include "YgorImages.h"
#include "YgorString.h"


bool DCEMRIAUCMap(planar_image_collection<float,double>::images_list_it_t first_img_it,
                  std::list<planar_image_collection<float,double>::images_list_it_t> selected_img_its,
                  std::list<std::reference_wrapper<planar_image_collection<float,double>>>,
                  std::list<std::reference_wrapper<contour_collection<double>>>, 
                  std::experimental::any ){

    //This routine integrates pixel channel values over time.
    const bool InhibitSort = true; //We will explicitly sort once to speed up data ingress.

    //Record the min and max actual pixel values for windowing purposes.
    float curr_min_pixel = std::numeric_limits<float>::max();
    float curr_max_pixel = std::numeric_limits<float>::min();

    //Loop over the rows, columns, channels, and finally images.
    for(auto row = 0; row < first_img_it->rows; ++row){
        for(auto col = 0; col < first_img_it->columns; ++col){
            for(auto chan = 0; chan < first_img_it->channels; ++chan){

                //Harvest the time course.
                samples_1D<double> channel_time_course;
                for(auto & img_it : selected_img_its){
                    const auto val = static_cast<double>(img_it->value(row, col, chan));
                    if(auto dt = img_it->GetMetadataValueAs<double>("dt")){
                        channel_time_course.push_back(dt.value(), val, InhibitSort);
                    }else{
                        FUNCERR("Image missing timestamp. Cannot integrate. Cannot continue");
                    }
                }

                //Integrate.
                channel_time_course.stable_sort();
                //const std::array<double,2> integ = channel_time_course.Integrate_Over_Kernel_unit(0.0, 600.0);
                const std::array<double,2> integ = channel_time_course.Integrate_Over_Kernel_unit();

                //Update the value.
                const auto newval = static_cast<float>( integ[0] );
                first_img_it->reference(row, col, chan) = newval;
                curr_min_pixel = std::min(curr_min_pixel, newval);
                curr_max_pixel = std::max(curr_max_pixel, newval);
            }
        }
    }


    //Alter the first image's metadata to reflect that averaging has occurred. You might want to consider
    // a selective whitelist approach so that unique IDs are not duplicated accidentally.
    first_img_it->metadata["Description"] = "IAUC map";

    //Specify a reasonable default window.
    const float WindowCenter = (curr_min_pixel/2.0) + (curr_max_pixel/2.0);
    const float WindowWidth  = 2.0 + curr_max_pixel - curr_min_pixel;
    first_img_it->metadata["WindowValidFor"] = first_img_it->metadata["Description"];
    first_img_it->metadata["WindowCenter"]   = Xtostring(WindowCenter);
    first_img_it->metadata["WindowWidth"]    = Xtostring(WindowWidth);

    return true;
}




