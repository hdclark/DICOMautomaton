
#include <YgorStats.h>
#include <cmath>
#include <any>
#include <functional>
#include <limits>
#include <list>
#include <memory>

#include "../ConvenienceRoutines.h"
#include "YgorImages.h"
#include "YgorMath.h"
#include "YgorMisc.h"
#include "YgorLog.h"


bool CTPerfusionSigDiffC( planar_image_collection<float,double>::images_list_it_t  local_img_it,
                          std::list<std::reference_wrapper<planar_image_collection<float,double>>> external_imgs,
                          std::list<std::reference_wrapper<contour_collection<double>>>, 
                          std::any ){

    //This image-processing functor takes long running CT Perfusion temporal series, and a pre-contrast "baseline"
    // of the same tissue (the anatomy must be ~identical) and produces a difference of the pixel intensities, like:
    //      outgoing pixel values  =  (S(t) - S_baseline).
    //
    // So this routine shows signal loss or enhancement over time, compared to some baseline. The specification of
    // baseline is important. If the baseline is estimated to be too large, later enhancement may become negative.
    // 

    //Verify and name the <pre-contrast S(t)> map.
    if(external_imgs.size() != 1){
        YLOGWARN("This routine must only be passed a single external image. Bailing");
        return false;
    }
    planar_image_collection<float,double> &S_baseline_map = external_imgs.front().get();

    //Record the min and max actual pixel values for windowing purposes.
    Stats::Running_MinMax<float> minmax_pixel;

    //Select the S0 and T1 map images which spatially overlap with this image.
    const auto img_cntr  = local_img_it->center();
    const auto img_ortho = local_img_it->row_unit.Cross( local_img_it->col_unit ).unit();
    const std::list<vec3<double>> points = { img_cntr, img_cntr + img_ortho * local_img_it->pxl_dz * 0.25,
                                                       img_cntr - img_ortho * local_img_it->pxl_dz * 0.25 };
    auto S_baseline_imgs = S_baseline_map.get_images_which_encompass_all_points(points);

    if(S_baseline_imgs.size() != 1){
        YLOGWARN("More than one image spatially overlaps with the present image. We can only handle a single image. Bailing");
        return false;
    }
    const auto S_baseline_img_it = S_baseline_imgs.front();

    //Loop over the rows, columns, and channels.
    for(auto row = 0; row < local_img_it->rows; ++row){
        for(auto col = 0; col < local_img_it->columns; ++col){
            for(auto chan = 0; chan < local_img_it->channels; ++chan){

                auto Sval       = static_cast<double>( local_img_it->value(row, col, chan) );
                auto S_baseline_val = static_cast<double>( S_baseline_img_it->value(row, col, chan) );
                const auto C_f = static_cast<float>(Sval - S_baseline_val);

                //Handle errors in reconstruction due to missing tissues (air), uncertainty, 
                // numerical instabilities, etc..
                if( std::isfinite(C_f) ){ 
                    const auto newval = C_f;
                    local_img_it->reference(row, col, chan) = newval;
                    minmax_pixel.Digest(newval);
                }else{
                    local_img_it->reference(row, col, chan) = std::numeric_limits<float>::quiet_NaN();
                }
            }
        }
    }

    UpdateImageDescription( std::ref(*local_img_it), "dSignal C(t)" );
    UpdateImageWindowCentreWidth( std::ref(*local_img_it), minmax_pixel );

    return true;
}



