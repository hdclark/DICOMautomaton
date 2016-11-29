
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


bool DCEMRISigDiffC( planar_image_collection<float,double>::images_list_it_t  local_img_it,
                     std::list<std::reference_wrapper<planar_image_collection<float,double>>> external_imgs,
                     std::list<std::reference_wrapper<contour_collection<double>>>, 
                     std::experimental::any ){

    //This image-processing functor takes the long running DCE-MRI temporal series, and the temporally-averaged, pre-contrast
    // long running DCE-MRI signal from the same series, and produces a "poor-man's contrast" map like:
    //      pixel values ~ (S(t) - baseline)/baseline
    // where "baseline" images are pre-contrast measurement images which have been temporally averaged.
    //
    // NOTE: This routine ignores T1 changes due to presence of gadolinium and is therefore not suitable for many things.
    //       It is fairly robust, though, and might be OK for qualitative purposes. If in doubt, ALWAYS prefer the proper
    //       T1 calculation instead.

    //Verify and name the <pre-contrast S(t)> map.
    if(external_imgs.size() != 1){
        FUNCWARN("This routine must only be passed a single external image. Bailing");
        return false;
    }
    planar_image_collection<float,double> &S_avgd_map = external_imgs.front().get();

    //Record the min and max actual pixel values for windowing purposes.
    Stats::Running_MinMax<float> minmax_pixel;

    //Select the baseline images which spatially overlap with this image.
    const auto img_cntr  = local_img_it->center();
    const auto img_ortho = local_img_it->row_unit.Cross( local_img_it->col_unit ).unit();
    const std::list<vec3<double>> points = { img_cntr, img_cntr + img_ortho * local_img_it->pxl_dz * 0.25,
                                                       img_cntr - img_ortho * local_img_it->pxl_dz * 0.25 };
    auto S_avgd_imgs = S_avgd_map.get_images_which_encompass_all_points(points);

    if(S_avgd_imgs.size() != 1){
        FUNCWARN("More than one image spatially overlaps with the present image. We can only handle a single image. Bailing");
        return false;
    }
    const auto S_avgd_img_it = S_avgd_imgs.front();

    //Loop over the rows, columns, and channels.
    for(auto row = 0; row < local_img_it->rows; ++row){
        for(auto col = 0; col < local_img_it->columns; ++col){
            for(auto chan = 0; chan < local_img_it->channels; ++chan){

                auto Sval       = static_cast<double>( local_img_it->value(row, col, chan) );
                auto S_avgd_val = static_cast<double>( S_avgd_img_it->value(row, col, chan) );

                const auto numer = Sval - S_avgd_val;
                const auto denom = S_avgd_val;
                const double C = numer / denom;
                const auto C_f = static_cast<float>(C);

                //Handle errors in reconstruction due to missing tissues (air), uncertainty, 
                // numerical instabilities, etc..
                if( std::isfinite(C_f) ){ 
                    const auto newval = C_f;
                    local_img_it->reference(row, col, chan) = newval;
                    if(isininc(-0.5,C_f,5.0)){
                        minmax_pixel.Digest(newval);
                    }
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



