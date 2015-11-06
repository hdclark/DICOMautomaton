
#include <list>
#include <functional>
#include <limits>
#include <map>
#include <cmath>

#include "YgorMisc.h"
#include "YgorMath.h"
#include "YgorStats.h"
#include "YgorAlgorithms.h"
#include "YgorImages.h"
#include "YgorString.h"


//Computes a S0 (or "M0" map) from spoiled-gradient echo images where TR >> T1 (where the steady-state 
// magentization formula is valid).
//
// This routine requires two or more image sets (collected at distinct flip angles) to properly work.
// Ideally, you should average several images together to reduce noise to produce these 'measurements'.
// For DCE-MRI purposes, you want to average as many of the pre-contrast injection images together as
// you can; typically amounting to 15s-45s worth of images.
//
// Internal calculations can be performed in a few ways. See below.
//
// This routine gets called once per frame, which can be very costly, but only needs to be called at
// the beginning of the time course.
//
// The calculation performed here also, necessarily, computes a T1 map which is not saved. This is a 
// limitation of the Ygor processing framework, which is presently not idomatically able to return
// two or more images. In practice, these computations will be performed twice for each voxel!
//
bool DCEMRIS0MapV2(planar_image_collection<float,double>::images_list_it_t  local_img_it,
                   std::list<std::reference_wrapper<planar_image_collection<float,double>>> external_imgs,
                   std::list<std::reference_wrapper<contour_collection<double>>>, 
                   std::experimental::any ){

    //Verify there are enough images available for the computation.
    //
    // NOTE: This does not imply the images are valid or have distinct flip angles!
    const auto N = external_imgs.size();
    if(N < 2){
        FUNCWARN("This routine require two or more images with distinct flip angles to produce an S0 map. "
                 "The operation_functor was handed " << external_imgs.size() << " images. Cannot continue");
        return false;
    }

    std::vector<std::reference_wrapper<planar_image_collection<float,double>>> ex_imgs(external_imgs.begin(),
                                                                                       external_imgs.end());

    //Select the images which spatially overlap with this image.
    const auto img_cntr  = local_img_it->center();
    const auto img_ortho = local_img_it->row_unit.Cross( local_img_it->col_unit ).unit();
    const std::list<vec3<double>> points = { img_cntr, img_cntr + img_ortho * local_img_it->pxl_dz * 0.25,
                                                       img_cntr - img_ortho * local_img_it->pxl_dz * 0.25 };
    std::vector<planar_image_collection<float,double>::images_list_it_t> overlapping_imgs;
    for(auto imgs : external_imgs){ 
        //Push back all overlapping images. If there is more than one we could either disregard all but 
        // the first or whine to the user that there should not be duplicates. The latter is safer. 
        auto overlapping_imgs_list = imgs.get().get_images_which_encompass_all_points(points);
        if(overlapping_imgs_list.size() != 1) FUNCERR("There should be exactly one spatially overlapping image"
                                                      " in the provided variable FlipAngle image sets. You'll"
                                                      " need to average spatially-overlapping images before"
                                                      " running this routine");
        overlapping_imgs.push_back( overlapping_imgs_list.front() );
    }

    //Verify that flip angle and repetition time data are present.
    std::vector<double> FlipAngle; //Stored in radians, not degrees.
    std::vector<double> sinFA;
    std::vector<double> cosFA;
    std::vector<double> RepTime;
    for(auto img_it : overlapping_imgs){
        auto FA = img_it->GetMetadataValueAs<double>("FlipAngle"); //Units: degrees.
        if(!FA) FUNCERR("Image is missing 'FlipAngle', which is needed to compute S0. Cannot continue");
        auto l_FA = FA.value() * M_PI/180.0;
        for(auto PrevFlipAngle : FlipAngle){
            if(RELATIVE_DIFF(PrevFlipAngle, l_FA) <= (1.0*M_PI/180.0)){
                FUNCERR("Encountered 'FlipAngle's that differ by less than one degree. "
                        " The computation will most likely be invalid due to numerical issues.");
                // NOTE: It is actually OK to continue if you need to, but be aware that numerical issues are likely
                //       to happen unless (1) the input data is low-noise, or (2) you have a lot of data at other
                //       flip angles. This catch is here to protect against accidental things like loading the same 
                //       image set more than once.
            }
        }
        FlipAngle.push_back(l_FA);

        auto RT = img_it->GetMetadataValueAs<double>("RepetitionTime"); //Units: msec.
        if(!RT) FUNCERR("Image is missing 'RepetitionTime', which is needed to compute S0. Cannot continue");
        auto l_RT = RT.value() * (1E-3); //Stored in seconds, not milliseconds.
        for(auto PrevRepTime : RepTime){
            if(RELATIVE_DIFF(PrevRepTime, l_RT) > (1E-6)){
                FUNCERR("Encountered 'RepititionTime's that differ by more than one microsecond. Cannot continue");
                // NOTE: Of the routines that follow, only the most brain-dead (numerical optimization) can handle 
                //       this situation. Enable iff you must!
            }
        }
        RepTime.push_back(l_RT);

        sinFA.push_back( std::sin(l_FA) );
        cosFA.push_back( std::cos(l_FA) );
    }

    //Record the min and max actual pixel values for windowing purposes.
    float curr_min_pixel = std::numeric_limits<float>::max();
    float curr_max_pixel = std::numeric_limits<float>::min();

    //Loop over the rows, columns, and channels.
    for(auto row = 0; row < local_img_it->rows; ++row){
        for(auto col = 0; col < local_img_it->columns; ++col){
            for(auto chan = 0; chan < local_img_it->channels; ++chan){
                double T1val(0.0), S0val(0.0);

                std::vector<double> S; //Pixel intensity. Could also be 'M' for magnetization.
                for(auto img_it : overlapping_imgs){
                    S.push_back( static_cast<double>( img_it->value(row, col, chan) ) );
                }

                // --------------- Analytic approach with two datum ---------------------
                // This solution solves the unbounded least-squares problem exactly for two datum:
                // --> minimize the sum_i of (S_i - S0*(1-k)*sin(FA_i)/(1-k*cos(FA_i)))^2 via [S0,k]
                // [where k = exp(-TR/T1)] resulting in a tidy analytic solution.
                //
                // No change of variables is performed; an action that can potentially bias the result
                // giving the datum unequal or inappropriate implicit weighting.
                //
                // This calculation was independently derived and checked around 20151012.
                //
                if(true && (N == 2)){
                    const auto knumer = S[0]*sinFA[1] - S[1]*sinFA[0];
                    const auto kdenom = S[0]*sinFA[1]*cosFA[0] - S[1]*sinFA[0]*cosFA[1];
                    const auto k = knumer/kdenom;
                    T1val = -RepTime[0] / std::log(k);

                    const auto decayL = ((1.0-k)*sinFA[0])/(1.0-k*cosFA[0]);
                    const auto decayR = ((1.0-k)*sinFA[1])/(1.0-k*cosFA[1]);
                    const auto S0numer = (S[0]*decayL + S[1]*decayR);
                    const auto S0denom = (std::pow(decayL,2.0) + std::pow(decayR,2.0));
                    S0val = S0numer/S0denom;

                // ------------ Pseudo-analytic approach with N>=2 datum ----------------
                // This solution alters the unbounded least-squares problem by linearizing the
                // objective function. In particular, it takes the TR>>T1 spoiled gradient echo
                // pixel intensity relation:
                //   S(FA,T1,S0,TR) = S0*(1-k)*sin(FA)/(1-k*cos(FA))
                // [where k = exp(-TR/T1)] and 'measurements' are pairs of (S_i, FA_i) at some
                // fixed T1 and TR, and rearranges it so that measurements are pairs of
                // (y_i, x_i) == (S_i*cos(FA_i)/sin(FA_i), S_i/sin(FA_i)) and the model is 
                // modified to be:
                //   y = (1/k)*x + S0*(1-(1/k))
                //
                // This approach is tractable for any N>=2, but messes up the uncertainty bias, which 
                // effectively weights the true measuremements. Thus, in the face of noise, this 
                // approach is not actually valid! However, it does not suffer from problems that 
                // the numerical optimization has, such as local minima and can at least guarantee
                // to be the optimal solution if the new measurements have normally-distributed 
                // measurement error (n.b., you don't know this for sure!).
                }else if(true && (N >= 2)){
                    std::vector<double> y, x;
                    for(size_t i = 0; i < N; ++i){
                        x.push_back( S[i]*(cosFA[i]/sinFA[i]) );
                        y.push_back( S[i]/sinFA[i] );
                    }

                    std::vector<double> yx, xx;
                    for(size_t i = 0; i < N; ++i){
                        yx.push_back(y[i]*x[i]);
                        xx.push_back(x[i]*x[i]);
                    }
                    const auto Sum_yx = Stats::Sum(yx);
                    const auto Sum_x  = Stats::Sum(x);
                    const auto Sum_y  = Stats::Sum(y);
                    const auto Sum_xx = Stats::Sum(xx);
                   
                    const auto m =  (Sum_yx - Sum_x*Sum_y/static_cast<double>(N)) 
                                   /(Sum_xx - Sum_x*Sum_x/static_cast<double>(N)); //Slope.
                    std::vector<double> y_min_mx;
                    for(size_t i = 0; i < N; ++i){
                        y_min_mx.push_back( y[i] - m*x[i] );
                    }
                    const auto b = Stats::Sum(y_min_mx)/static_cast<double>(N);
                    const auto k = m;

                    if(std::isfinite(m) && std::isfinite(b)){
                        S0val = b/(1.0 - m);
                        T1val = -RepTime[0] / std::log(k);
                    }else{
                        S0val = std::numeric_limits<float>::quiet_NaN();
                        T1val = std::numeric_limits<float>::quiet_NaN();
                    }

                // ------------- Numerical least-squares with N>=2 datum ----------------
                // This approach solves the unbounded least-squares problem:
                // --> minimize the sum_i of (S_i - S0*(1-k)*sin(FA_i)/(1-k*cos(FA_i)))^2 via [S0,k]
                // [where k = exp(-TR/T1)] directly and numerically. 
                // 
                // This is the least reasonable approach and should be used only when needed. However,
                // no change of variables is performed; an action that can potentially bias the result
                // giving the datum unequal or inappropriate implicit weighting. Compared with 
                // linearization, this approach will not introduce any extra bias.
                //
                // A deterministic version of this would be better, but I haven't had any luck working
                // it out yet. Here I have reduced the 2D optimization problem to a 1D root-finding 
                // problem, which should at least help keep the relationship between S0 and T1 correct.
                //
                // NOTE: A reasonable initial guess would be to use the linearized LSS solutions.
                //
                // NOTE: This routine could be switched to do least-median-squares fitting even though
                //       a LSS relation between S0 and T1 is used. The relation should enforce the LSS
                //       nature, whereas LMS will simply be used to find the optimal T1.
                //
                //       However, LMS would probably only be useful if there were many measurements 
                //       (say, ~five or more), which is unlikely.
                //
                }else if(false && (N >= 2)){

                    auto S0_from_k = [=](double k) -> double {
                        double numer = 0.0;
                        double denom = 0.0;
                        for(size_t i = 0; i < N; ++i){
                            numer += S[i]*(1.0-k)*sinFA[i]/(1.0-k*cosFA[i]);
                            denom += std::pow((1.0-k)*sinFA[i]/(1.0-k*cosFA[i]), 2.0);
                        }
                        return numer/denom; // == S0.
                    };

                    auto T1_from_k = [=](double k) -> double {
                        return -RepTime[0]/std::log(k); // == T1.
                    };

                    //Objective function (i.e., "F" in min[sum_i[(Datum_i - F_i)^2]] ) at the coordinate FA_i 
                    // with the variable k. Note this is "F" in min: sum_i (Datum_i - F_i)^2.
                    auto obj_func = [=](const std::list<double> &X, const std::list<double> &Vars) -> double {
                        const auto BigValue = static_cast<double>(std::numeric_limits<float>::max());
                        const auto l_k =  Vars.front();
                        if((l_k <= 0.0) || (l_k >= 1.0)) return BigValue;
                        const auto l_theta = X.front(); // Should be in radians.
                        const auto l_S0 = S0_from_k(l_k);
                        if(l_S0 < 0.0) return BigValue;

                        const auto out =  l_S0 * (1.0 - l_k)*std::sin(l_theta)
                                               / (1.0 - l_k*std::cos(l_theta));
                        return std::isfinite(out) ? out : BigValue;
                    };

                    //Pack the data as it is needed.
                    std::list<std::list<double>> FitData;
                    for(size_t i = 0; i < N; ++i) FitData.emplace_back(std::list<double>({ FlipAngle[i], S[i] }));
                    
                    bool wasOK = false;
                    auto res = Ygor_Fit_LSS(&wasOK, obj_func, FitData, { 0.5 }, 2, false, 0.2, 3000, 1E-6);
                    //NOTE: input is (wasOK, F, data, vars, dim, verbose, char_len, max_iters, Ftol).
                    //NOTE: output is (best_params, chi_sq, Qvalue, DOF, red_chi_sq, raw_coeff_deter, mod_coeff_deter, best_SRs).
                    if(wasOK){
                        const auto k = std::get<0>(res).back();
                        T1val = T1_from_k( k );
                        S0val = S0_from_k( k );
                    }else{
                        S0val = std::numeric_limits<double>::quiet_NaN();
                        T1val = std::numeric_limits<double>::quiet_NaN();
                    }

                }

                 
                //Handle errors in reconstruction due to missing tissues (air), uncertainty, 
                // numerical instabilities, etc..
                //
                // NOTE: What are 'sufficiently small' and 'sufficiently large' signals that
                //       reliably signal an issue? It depends on the pixel unit and tissue,
                //       so maybe a better approach should be taken?
                //
                //       How about looking at the least-squares residual?
                //
                if( !std::isfinite(T1val) || !std::isfinite(S0val) ){
                    S0val = std::numeric_limits<double>::quiet_NaN();
                    T1val = std::numeric_limits<double>::quiet_NaN();
                }
/*
                if( std::all_of(S.begin(),S.end(),[](double l_S){ return (l_S < 10.0); }) ){
                    // If the signals S_i are simply too small for effective reconstruction, we 
                    // cannot find T1 or S0. So, it makes the most sense to assume there was nothing
                    // in the voxel, which means S0 is zero and T1 can be anything.
                    S0val = 0.0;
                    T1val = std::numeric_limits<double>::quiet_NaN();
                }
*/

                //Write the value to the map's pixel.
                const auto S0val_f = static_cast<float>(S0val);
                if(std::isfinite(S0val_f)){
                    const auto newval = S0val_f;
                    local_img_it->reference(row, col, chan) = newval;
                    if(isininc(5'000.0, newval, 70'000.0)){
                        curr_min_pixel = std::min(curr_min_pixel, newval);
                        curr_max_pixel = std::max(curr_max_pixel, newval);
                    }
                }else{
                    local_img_it->reference(row, col, chan) = std::numeric_limits<float>::quiet_NaN();
                }
            }
        }
    }

    //Alter the first image's metadata to reflect that averaging has occurred. You might want to consider
    // a selective whitelist approach so that unique IDs are not duplicated accidentally.
    local_img_it->metadata["Description"] = "S0 map";

    //Specify a reasonable default window.
    const float WindowCenter = (curr_min_pixel/2.0) + (curr_max_pixel/2.0);
    const float WindowWidth  = 2.0 + curr_max_pixel - curr_min_pixel;
    local_img_it->metadata["WindowValidFor"] = local_img_it->metadata["Description"];
    local_img_it->metadata["WindowCenter"]   = Xtostring(WindowCenter);
    local_img_it->metadata["WindowWidth"]    = Xtostring(WindowWidth);

    return true;
}




