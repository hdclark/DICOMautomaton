
#include <list>
#include <functional>
#include <limits>
#include <map>
#include <cmath>

#include "YgorMisc.h"
#include "YgorMath.h"
#include "YgorImages.h"
#include "YgorString.h"



bool DCEMRIS0Map(planar_image_collection<float,double>::images_list_it_t first_img_it,
                 std::list<planar_image_collection<float,double>::images_list_it_t> selected_img_its,
                 std::list<std::reference_wrapper<planar_image_collection<float,double>>>,
                 std::list<std::reference_wrapper<contour_collection<double>>>, 
                 std::experimental::any ){

    //Verify there are merely two images selected.
    if(selected_img_its.size() != 2){
        FUNCWARN("This routine assumes two images will be combined to produce a S0 map. "
                 "The operation_functor was handed " << selected_img_its.size() << " images. Cannot continue");
        return false;
    }

    auto L_img_it = selected_img_its.front();
    auto R_img_it = selected_img_its.back();

    //Verify that flip angle and repetition time data are present.
    auto L_FlipAngle = L_img_it->GetMetadataValueAs<double>("FlipAngle"); //Units: degrees.
    auto R_FlipAngle = R_img_it->GetMetadataValueAs<double>("FlipAngle"); //Units: degrees.
    auto L_RepTime   = L_img_it->GetMetadataValueAs<double>("RepetitionTime"); //Units: msec.
    auto R_RepTime   = R_img_it->GetMetadataValueAs<double>("RepetitionTime"); //Units: msec.
    if(!L_FlipAngle || !R_FlipAngle || !L_RepTime || !R_RepTime) FUNCERR("Missing needed info for S0 map. Cannot continue");
    if(RELATIVE_DIFF(L_RepTime.value(), R_RepTime.value()) > 1E-3) FUNCERR("Encountered differing Repitition Times. Cannot continue");

    const auto RepTime = L_RepTime.value(); // or R_RepTime. They are ~equivalent.
    const auto sinFAL = std::sin(L_FlipAngle.value()*M_PI/180.0);
    const auto sinFAR = std::sin(R_FlipAngle.value()*M_PI/180.0);
    const auto cosFAL = std::cos(L_FlipAngle.value()*M_PI/180.0);
    const auto cosFAR = std::cos(R_FlipAngle.value()*M_PI/180.0);

    //Record the min and max actual pixel values for windowing purposes.
    float curr_min_pixel = std::numeric_limits<float>::max();
    float curr_max_pixel = std::numeric_limits<float>::min();

    //Loop over the rows, columns, and channels.
    for(auto row = 0; row < first_img_it->rows; ++row){
        for(auto col = 0; col < first_img_it->columns; ++col){
            for(auto chan = 0; chan < first_img_it->channels; ++chan){

                auto SL = static_cast<double>( L_img_it->value(row, col, chan) );
                auto SR = static_cast<double>( R_img_it->value(row, col, chan) );

                // --------------- Analytic approach with two datum ---------------------
                //Work out the T1 value, handling the special cases due to measurement error.
                double T1val(0.0), S0val(0.0);
                {
                  //This calculation independently derived and checked around 20151012.
                  const auto numer = SL*sinFAR*cosFAL - SR*sinFAL*cosFAR;
                  const auto denom = SL*sinFAR - SR*sinFAL;
                  T1val = RepTime / std::log(numer/denom);
                }
                {
                  //Based on a re-calculation and derivation around 20151012, I believe this solution
                  // to be incorrect. I think I've tried to calculate the exact fit for one of the datum
                  // while using the least-squares T1. This solution does not use SR!
                  //const auto numer = (1.0 - std::exp(-RepTime/T1val))*sinFAL;
                  //const auto denom = (1.0 - std::exp(-RepTime/T1val)*cosFAL);
                  //S0val = SL / (numer/denom);

                  //This solution was found using a CAS for assistance. It becomes quite nasty quite quickly,
                  // but should be correct unless I've made a typo somewhere.
                  const auto k = std::exp(-RepTime / T1val);
                  const auto decayL = ((1.0-k)*sinFAL)/(1.0-k*cosFAL);
                  const auto decayR = ((1.0-k)*sinFAR)/(1.0-k*cosFAR);
                  const auto numer = (SL*decayL + SR*decayR);
                  const auto denom = (std::pow(decayL,2.0) + std::pow(decayR,2.0));
                  S0val = numer/denom;

                }
                //Handle errors in reconstruction due to missing tissues (air), uncertainty, 
                // numerical instabilities, etc..
                if( (SL < 10.0) || (SR < 10.0) ){
                    //Assume that the signals L or R are simply too small for effective
                    // reconstruction. In this case, we cannot find T1 or S0 but we will
                    // assume there is nothing in the voxel.
                    S0val = 0.0; // <---- most important number to set to zero.
                    T1val = 0.0; // <---- what's here is immaterial if S0 is zero.
                }

                if( !std::isfinite(T1val) || !std::isfinite(S0val) ){
                    S0val = std::numeric_limits<double>::quiet_NaN();
                    T1val = std::numeric_limits<double>::quiet_NaN();
                }

                // --------------- Least-squares approach with N datum ------------------- 
                //Use the analytic value as seeds if it seems worthwhile.
                /*
                if((SL >= 10.0) || (SR >= 10.0)){
                    if(T1val < 100.0) T1val = 500.0;

                    //Objective function at the coordinates X with the variables Vars.
                    // Note this is "F" in min: sum_i (Datum_i - F_i)^2.
                    auto min_func = [=](const std::list<double> &X, const std::list<double> &Vars) -> double {
                        const auto l_S0 = std::abs( Vars.front() );
                        const auto l_T1 = std::abs( Vars.back() );
                        const auto l_theta = X.front()*M_PI/180.0;
                        const auto out =  l_S0 * (1.0 - std::exp(-RepTime/l_T1))*std::sin(l_theta)
                                               / (1.0 - std::exp(-RepTime/l_T1)*std::cos(l_theta));

                        return std::isfinite(out) ? out : static_cast<double>( std::numeric_limits<float>::max() );
                    };

                    bool wasOK = false;
                    auto res = Ygor_Fit_LSS(&wasOK, min_func, { { L_FlipAngle, SL }, { R_FlipAngle, SR } },
                                { 0.05*(SL+SR), T1val }, 2, false, 100.0, 3000, 1E-6);
                    //NOTE: input is (wasOK, F, data, vars, dim, verbose, char_len, max_iters, Ftol).
                    //NOTE: output is (best_params, chi_sq, Qvalue, DOF, red_chi_sq, raw_coeff_deter, mod_coeff_deter, best_SRs).

                    if(wasOK){
                        T1val = std::abs( std::get<0>(res).back() );
                    }else{
                        T1val = 0.0;
                    }
                    if(T1val > 15000.0) T1val = 0.0; 
                }
                */

                const auto S0val_f = static_cast<float>(S0val);
                if(std::isfinite(S0val_f)){
                    const auto newval = S0val_f;
                    first_img_it->reference(row, col, chan) = newval;
                    if(isininc(0.0, newval, 1000.0)){
                        curr_min_pixel = std::min(curr_min_pixel, newval);
                        curr_max_pixel = std::max(curr_max_pixel, newval);
                    }
                }else{
                    first_img_it->reference(row, col, chan) = std::numeric_limits<float>::quiet_NaN();
                }
            }
        }
    }

    //Alter the first image's metadata to reflect that averaging has occurred. You might want to consider
    // a selective whitelist approach so that unique IDs are not duplicated accidentally.
    first_img_it->metadata["Description"] = "S0 map";

    //Specify a reasonable default window.
    const float WindowCenter = (curr_min_pixel/2.0) + (curr_max_pixel/2.0);
    const float WindowWidth  = 2.0 + curr_max_pixel - curr_min_pixel;
    first_img_it->metadata["WindowValidFor"] = first_img_it->metadata["Description"];
    first_img_it->metadata["WindowCenter"]   = Xtostring(WindowCenter);
    first_img_it->metadata["WindowWidth"]    = Xtostring(WindowWidth);

    return true;
}




