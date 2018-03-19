
#include <algorithm>
#include <experimental/any>
#include <functional>
#include <list>

#include "../ConvenienceRoutines.h"
#include "YgorImages.h"
#include "YgorMisc.h"
#include "YgorStats.h"       //Needed for Stats:: namespace.

template <class T> class contour_collection;


bool CTPerfusionSearchForLiverClips(planar_image_collection<float,double>::images_list_it_t first_img_it,
                                    std::list<planar_image_collection<float,double>::images_list_it_t> selected_img_its,
                                    std::list<std::reference_wrapper<planar_image_collection<float,double>>>,
                                    std::list<std::reference_wrapper<contour_collection<double>>>, 
                                    std::experimental::any ){

    //This routine searches for surgically-implanted liver markers or 'clips' which appear in some CT slices.
    // The region around clips is slightly distorted. The basic idea is to figure out a generic signature which 
    // describes the clip distortion, compute the difference from this signature for each voxel, and then either
    // return a map with the clip location likelihood OR simply provide a direct guess at the clip(s') location(s).

    //Make a 'working' image which we can edit. Start by duplicating the first image.
    planar_image<float,double> working;
    working = *first_img_it;

    //Ensure only single images are grouped together.
    if(selected_img_its.size() != 1){
        FUNCWARN("This routine works on single images. It cannot deal with grouped images");
        return false;
    }

    //Paint all pixels black.
    working.fill_pixels(static_cast<float>(0));

    //Record the min and max actual pixel values for windowing purposes.
    Stats::Running_MinMax<float> minmax_pixel;

    //Loop over the rows, columns, channels, and finally selected_images in the time course.
    auto boxr = 2; //The inclusive 'radius' of the square box to use to average nearby pixels.
    for(auto row = boxr; (row + boxr) < first_img_it->rows; ++row){
        for(auto col = boxr; (col + boxr) < first_img_it->columns; ++col){
            for(auto chan = 0; chan < first_img_it->channels; ++chan){

                //Collect the datum of voxels and nearby voxels for an average.
                std::list<double> in_pixs;
                for(auto lrow = (row-boxr); lrow <= (row+boxr); ++lrow){
                    for(auto lcol = (col-boxr); lcol <= (col+boxr); ++lcol){
                        if( !isininc(0,lrow,first_img_it->rows-1) || !isininc(0,lcol,first_img_it->columns-1) ){
                            FUNCERR("Check your boxr programming. Off by one?");
                        }

                        const auto val = static_cast<double>(first_img_it->value(lrow, lcol, chan));
                        in_pixs.push_back(val);
                    }
                }
                //const auto RawNewVal = std::sqrt(Stats::Unbiased_Var_Est(in_pixs));
                const auto RawNewVal = *std::max_element(in_pixs.begin(), in_pixs.end());

                const float newval = static_cast<float>(RawNewVal);
                working.reference(row, col, chan) = newval;
                minmax_pixel.Digest(newval);

            }//Loop over channels.
        } //Loop over cols
    } //Loop over rows

    //Swap the original image with the working image.
    *first_img_it = working;

    //Specify a reasonable default window.
    UpdateImageDescription( std::ref(*first_img_it), "Clip Location Likelihood" );
    UpdateImageWindowCentreWidth( std::ref(*first_img_it), minmax_pixel );

    return true;
}

