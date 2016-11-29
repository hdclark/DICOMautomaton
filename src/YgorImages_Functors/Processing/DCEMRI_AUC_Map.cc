
#include <list>
#include <functional>
#include <limits>
#include <map>
#include <cmath>

#include "YgorMisc.h"
#include "YgorMath.h"
#include "YgorImages.h"
#include "YgorString.h"

#include "../ConvenienceRoutines.h"

bool DCEMRIAUCMap(planar_image_collection<float,double>::images_list_it_t first_img_it,
                  std::list<planar_image_collection<float,double>::images_list_it_t> selected_img_its,
                  std::list<std::reference_wrapper<planar_image_collection<float,double>>>,
                  std::list<std::reference_wrapper<contour_collection<double>>>, 
                  std::experimental::any ){

    //This routine integrates pixel channel values over time.
    const bool InhibitSort = true; //We will explicitly sort once to speed up data ingress.

    //Record the min and max actual pixel values for windowing purposes.
    Stats::Running_MinMax<float> minmax_pixel;

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
                        throw std::invalid_argument("Image missing timestamp. Cannot integrate. Cannot continue");
                    }
                }

                //'Prime' the pixel to default to NaN.
                first_img_it->reference(row, col, chan) = std::numeric_limits<double>::quiet_NaN();
 
                //Purge NaNs (i.e., assume the data point is unknown but finite -- so interpolate to guess it).
                channel_time_course = channel_time_course.Purge_Nonfinite_Samples();

                //Integrate.
                if( channel_time_course.size() > 1 ){
                    channel_time_course.stable_sort();
                    //const std::array<double,2> integ = channel_time_course.Integrate_Over_Kernel_unit(0.0, 600.0);
                    const std::array<double,2> integ = channel_time_course.Integrate_Over_Kernel_unit();

                    //Update the pixel and minmax.
                    const auto newval = static_cast<float>( integ[0] );
                    first_img_it->reference(row, col, chan) = newval;
                    if(std::isfinite(newval)) minmax_pixel.Digest(newval);
                }
            }
        }
    }

    UpdateImageDescription( std::ref(*first_img_it), "IAUC" );
    UpdateImageWindowCentreWidth( std::ref(*first_img_it), minmax_pixel );

    return true;
}




