
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

#include "../ConvenienceRoutines.h"


bool TimeCourseSlopeMap(planar_image_collection<float,double>::images_list_it_t first_img_it,
                        std::list<planar_image_collection<float,double>::images_list_it_t> selected_img_its,
                        std::list<std::reference_wrapper<planar_image_collection<float,double>>>,
                        std::list<std::reference_wrapper<contour_collection<double>>>,
                        double tmin, double tmax, 
                        std::experimental::any ){

    //This routine collects voxel time series, fits a line (or computes a Spearman's rank correlation coefficient), and
    // produces a map of the resulting slope over the speecified time.
    const bool InhibitSort = true; //Disable continuous sorting (defer to single sort later) to speed up data ingress.

    //Make a 'working' image which we can edit. Start by duplicating the first image.
    planar_image<float,double> working;
    working = *first_img_it;

    //Paint all pixels black.
    working.fill_pixels(static_cast<float>(0));

    //Verify there are at least two images from which to extract a time course.
    if(selected_img_its.size() < 2){
        FUNCWARN("Not enough images to perform linear regression. Need at least two images. "
                 "Producing a blank image and continuing.");
        return false;
    }

    //Record the min and max actual pixel values for windowing purposes.
    Stats::Running_MinMax<float> minmax_pixel;

    //Loop over the rows, columns, channels, and finally selected_images in the time course.
    for(auto row = 0; row < first_img_it->rows; ++row){
        for(auto col = 0; col < first_img_it->columns; ++col){
            for(auto chan = 0; chan < first_img_it->channels; ++chan){
                //Cycle over the grouped images (temporal slices, or whatever the user has decided).
                // Harvest the time course or any other voxel-specific numbers.
                samples_1D<double> channel_time_course;
                channel_time_course.uncertainties_known_to_be_independent_and_random = true;
                for(auto & img_it : selected_img_its){
                    //Collect the datum of voxels and nearby voxels for an average.
                    std::list<double> in_pixs;
                    auto boxr = 1; //The inclusive 'radius' of the square box to use to average nearby pixels.
                    for(auto lrow = (row-boxr); lrow <= (row+boxr); ++lrow){
                        for(auto lcol = (col-boxr); lcol <= (col+boxr); ++lcol){
                            //Check if the coordinates are legal.
                            if( isininc(0,lrow,img_it->rows-1) 
                            &&  isininc(0,lcol,img_it->columns-1) ){ 
                                const auto val = static_cast<double>(img_it->value(lrow, lcol, chan));
                                in_pixs.push_back(val);
                            }
                        }
                    }
                    if(in_pixs.size() < 3) continue; //Too few to bother with.
                    const auto avg_val = Stats::Mean(in_pixs);
                    //const auto avg_val_sigma = std::sqrt(Stats::Unbiased_Var_Est(in_pixs))/std::sqrt(1.0*in_pixs.size());

                    if(auto dt = img_it->GetMetadataValueAs<double>("dt")){
                        //channel_time_course.push_back(dt.value(), 0.0, avg_val, avg_val_sigma, InhibitSort);
                        channel_time_course.push_back(dt.value(), 0.0, avg_val, 0.0, InhibitSort);
                    }else{
                        FUNCERR("Image is missing time metadata. Bailing");
                    }
                }
                channel_time_course.stable_sort();
                if(channel_time_course.empty()) continue;

                //Keep only the requested part of the time course.
                channel_time_course = channel_time_course.Select_Those_Within_Inc(tmin, tmax);

                bool wasOK = false;

                //Perform linear regression and record the slope.
                {
                    auto res = channel_time_course.Linear_Least_Squares_Regression(&wasOK);
                    if(wasOK){
                        //Update the pixel.

                        //const float newval = (res.slope > 0) ? 200 : 100; //Sign of the slope, pos or neg.

FUNCERR("Need to figure out if the numerical rebasing/positive shift is needed here. Probably not, but maybe for presicion?");
                        const auto scaled = (1E5) * res.slope + (1E6);
                        const auto trunc = std::max(0.0, scaled);
                        const auto newval = static_cast<float>(std::round(trunc));

                        working.reference(row, col, chan) = newval;
                        minmax_pixel.Digest(newval);
                    }
                }


/*
                //Compute the Spearman's rank correlation coefficient.
                {
                    auto res = channel_time_course.Spearmans_Rank_Correlation_Coefficient(&wasOK);
                    if(wasOK){
                        const auto coeff = res[0];

                        //Update the pixel by scaling and offsetting the coefficient. (1020 is divisible by 255.)
                        const float newval = static_cast<float>((1.0 + coeff) * 1020.0 + 1.0);
                        working.reference(row, col, chan) = newval;

                        minmax_pixel.Digest(newval);
                    }
                }
*/

            }//Loop over channels.
        } //Loop over cols
    } //Loop over rows

    //Swap the original image with the working image.
    *first_img_it = working;

    UpdateImageDescription( std::ref(*first_img_it), "Time Course Slope Map" );
    UpdateImageWindowCentreWidth( std::ref(*first_img_it), minmax_pixel );

    first_img_it->metadata["TimeCourseSlopeMaptmin"] = Xtostring(tmin);
    first_img_it->metadata["TimeCourseSlopeMaptmax"] = Xtostring(tmax);

    return true;
}

