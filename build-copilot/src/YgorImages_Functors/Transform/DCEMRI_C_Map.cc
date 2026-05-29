
#include <YgorStats.h>
#include <cmath>
#include <any>
#include <optional>
#include <functional>
#include <limits>
#include <list>
#include <memory>

#include "../ConvenienceRoutines.h"
#include "YgorImages.h"
#include "YgorMath.h"
#include "YgorMisc.h"
#include "YgorLog.h"


//Computes a DCE C(t) contrast enhancement map using S0 and T1 maps. Gets called once per frame, which 
// can be very costly.
//
// NOTE: This solves an equation with a single unknown parameter: the steady-state MR spoiled gradient echo
//       equation where T1 >> TR. It is:  S = S0 * (1 - k')*sin(FA)/(1 - k'*cos(FA)) where k'=exp(-TR/T1') and
//       S is the measured pixel intensity. The values of S0 and T1 (no prime; baseline T1) are needed to 
//       work out the concentration, which is related to the difference of the multiplicative-inverse of 
//       each of T1 and T1'. Specifically, C_enhanced = (1/R1)*((1/T1') - (1/T1)).
//        
//       See the article "T1-weighted DCE Imaging Concepts: Modelling, Acquisition and Analysis" by Paul
//       Tofts in Siemens "MAGNETOM Flash · 3/2010" for a detailed introduction to this computation.
//
bool DCEMRICMap(planar_image_collection<float,double>::images_list_it_t  local_img_it,
                std::list<std::reference_wrapper<planar_image_collection<float,double>>> external_imgs,
                std::list<std::reference_wrapper<contour_collection<double>>>, 
                std::any ){

    //Verify and name the S0 and T1 maps.
    if(external_imgs.size() != 2) return false;
    planar_image_collection<float,double> &S0_map = external_imgs.front().get();
    planar_image_collection<float,double> &T1_map = external_imgs.back().get();

    //Select the S0 and T1 map images which spatially overlap with this image.
    const auto img_cntr  = local_img_it->center();
    const auto img_ortho = local_img_it->row_unit.Cross( local_img_it->col_unit ).unit();
    const std::list<vec3<double>> points = { img_cntr, img_cntr + img_ortho * local_img_it->pxl_dz * 0.25,
                                                       img_cntr - img_ortho * local_img_it->pxl_dz * 0.25 };
    auto S0_imgs = S0_map.get_images_which_encompass_all_points(points);
    auto T1_imgs = T1_map.get_images_which_encompass_all_points(points);

    if((S0_imgs.size() != 1) || (T1_imgs.size() != 1)) return false;
    const auto S0_img_it = S0_imgs.front();
    const auto T1_img_it = T1_imgs.front();

    //Verify that flip angle and repetition time data are present.
    auto FlipAngleOpt = local_img_it->GetMetadataValueAs<double>("FlipAngle"); //Units: degrees.
    auto RepTimeOpt   = local_img_it->GetMetadataValueAs<double>("RepetitionTime"); //Units: msec.
    if(!FlipAngleOpt || !RepTimeOpt) YLOGERR("Missing needed info for C map computation. Cannot continue");
    const auto pi = std::acos(-1.0);
    const auto RepTime = RepTimeOpt.value();
    const auto FlipAngle = FlipAngleOpt.value();
    const auto sinFA = std::sin(FlipAngle*pi/180.0);
    const auto cosFA = std::cos(FlipAngle*pi/180.0);

    //Record the min and max actual pixel values for windowing purposes.
    Stats::Running_MinMax<float> minmax_pixel;

    //Loop over the rows, columns, and channels.
    for(auto row = 0; row < local_img_it->rows; ++row){
        for(auto col = 0; col < local_img_it->columns; ++col){
            for(auto chan = 0; chan < local_img_it->channels; ++chan){

                auto Sval  = static_cast<double>( local_img_it->value(row, col, chan) );
                auto S0val = static_cast<double>( S0_img_it->value(row, col, chan) );
                auto T1val = static_cast<double>( T1_img_it->value(row, col, chan) );

                const auto numer = Sval*cosFA - S0val*sinFA;
                const auto denom = Sval       - S0val*sinFA;
                const auto T1prime = (RepTime * (1E-3)) / std::log(numer / denom);

                const double C = (1.0/0.0045)*((1.0/T1prime) - (1.0/T1val));
                const auto C_f = static_cast<float>(C);

                if( std::isfinite(C_f) 
//                &&  std::isnormal(C_f) 
//                &&  (C_f >= 0) 
//                &&  std::isfinite(S0val) 
//                &&  std::isnormal(S0val) 
//                &&  std::isfinite(T1val) 
//                &&  std::isnormal(T1val) ){
                            ){
//                &&  (T1val > 10.0)
//                &&  (Sval > 10.0)
//                &&  (S0val > 10.0) ){
                    //Update the value.
                    const auto newval = C_f;
                    local_img_it->reference(row, col, chan) = newval;
                    if(isininc(-0.5,C_f,20.0)){
                        minmax_pixel.Digest(newval);
                    }
                }else{
                    local_img_it->reference(row, col, chan) = std::numeric_limits<float>::quiet_NaN();
                }
/*
YLOGERR("Need to double check that you can delete the extra 1000.0x scaling factor in this C computation!");
                double C = 1000.0*(1.0/0.0045)*((1.0/T1prime) - (1.0/T1val));
                //double C = (1.0/4.5)*((1.0/T1prime) - (1.0/T1val));

                //Handle errors in reconstruction due to missing tissues (air), uncertainty, 
                // numerical instabilities, etc..
                if( (S0val < 10.0) || (T1val < 10.0)    || (Sval < 10.0)
                                   || (C < 1.0)         || (C > 1500.0)
                                   || !std::isfinite(C) || !std::isnormal(C) ){
                    C = 0.0;
                }

                //Update the value.
                const auto newval = static_cast<float>(C);
                local_img_it->reference(row, col, chan) = newval;
                minmax_pixel.Digest(newval);
*/
            }
        }
    }

    UpdateImageDescription( std::ref(*local_img_it), "C Map" );
    UpdateImageWindowCentreWidth( std::ref(*local_img_it), minmax_pixel );

    return true;
}




