//Perfusion_Model.cc -- A part of DICOMautomaton 2020. Written by hal clark, ...
//
// This file is meant to contain an implementation of the SCDI blood perfusion kinetic model.

#include <exception>
#include <functional>
#include <algorithm>
#include <iostream>
#include <list>
#include <limits>
#include <map>
#include <memory>
#include <string>    
#include <vector>
#include <numeric>

#include <cstdlib>            //Needed for exit() calls.
#include <utility>            //Needed for std::pair.

#include <boost/filesystem.hpp> // Needed for Boost filesystem access.

#include <CL/sycl.hpp>        //Needed for SYCL routines.

#include <eigen3/Eigen/Dense> //Needed for Eigen library dense matrices.

#include "YgorFilesDirs.h"    //Needed for Does_File_Exist_And_Can_Be_Read(...), etc..
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorMath.h"         //Needed for samples_1D.
#include "YgorString.h"       //Needed for GetFirstRegex(...)

#include "Perfusion_SCDI.h"

// This SYCL-powered function performs vector summation. It can run on CPU, GPU, FPGA, etc.
// It is only here to demonstrate roughly how SYCL is used.
static
std::vector<double>
vec_add(cl::sycl::queue &q, const std::vector<double> &lhs, const std::vector<double> &rhs){
    const auto N = lhs.size();
    std::vector<double> dst(N); // Destination; buffer for the result.

    if(N != rhs.size()){
        throw std::runtime_error("This routine only supports same-size buffer addition. Cannot continue.");
    }
    if(N != dst.size()){
        throw std::runtime_error("This routine only supports same-size buffer outputs. Cannot continue.");
    }
  
    cl::sycl::range<1> work_items{ N };

    {
        cl::sycl::buffer<double> buff_lhs( lhs.data(), N );
        cl::sycl::buffer<double> buff_rhs( rhs.data(), N );
        cl::sycl::buffer<double> buff_dst( dst.data(), N );

        q.submit([&](cl::sycl::handler& cgh){
            auto access_lhs = buff_lhs.get_access< cl::sycl::access::mode::read  >(cgh);
            auto access_rhs = buff_rhs.get_access< cl::sycl::access::mode::read  >(cgh);
            auto access_dst = buff_dst.get_access< cl::sycl::access::mode::write >(cgh);

            cgh.parallel_for<class vector_add>(work_items,
                [=](cl::sycl::id<1> tid){
                    access_dst[tid] = access_lhs[tid] + access_rhs[tid];
                }
            );
        });
    }
    return dst;
}

// This function should be where the blood perfusion model is implemented.
void
Launch_SCDI( samples_1D<double> &AIF,
             samples_1D<double> &VIF, 
             std::vector<samples_1D<double>> &C ){

    // Prepare the AIF, VIF, and time courses in C for modeling.
    // Keep in mind that they are all irregularly sampled, so might need to be re-sampled.
    //
    // The samples_1D class is described at
    // https://github.com/hdclark/Ygor/blob/5cffc24f3c662db116cc132da033bbc279e19d56/src/YgorMath.h#L823
    // but in a nutshell it is a container of 'samples' where each sample is 4 numbers: { t, st, f, sf }
    // where 't' is the time (in seconds) and 'f' is the magnitude of contrast enhancement -- 'st' and 'sf'
    // are related to sampling uncertainties and can be safely ignored.
    // 
    // The following will re-sample all time courses using a fixed step size 'dt' using linear interpolation.
    // I think it will suffice for this project, but we might have to adjust it later.
    const auto resample = [](const samples_1D<double> &s) -> std::vector<float> {
        const double dt = 0.1; // units of seconds.
        const auto cropped = s.Select_Those_Within_Inc(0.0, std::numeric_limits<double>::infinity());
        const auto extrema_x = cropped.Get_Extreme_Datum_x();

        std::vector<float> resampled;
        long int N = 0;

        while(true){
            const double t = 0.0 + static_cast<double>(N) * dt;
            if(t < extrema_x.first[0]){
                throw std::logic_error("Time courses should start at 0. Please adjust the time course.");
            }
            if(extrema_x.second[0] < t) break;
            if(1'000'000 < N){
                throw std::runtime_error("Excessive number of samples detected. Is this intended?");
            }
            
            resampled.emplace_back( cropped.Interpolate_Linearly(t)[2] );
            ++N;
        }
        return resampled;
    };

    // These now only contain contrast signal without 't' information. All implicitly start at t=0, and all are
    // regularly sampled. They might be easier to work with.
    std::vector<float> resampled_aif = resample(AIF);
    std::vector<float> resampled_vif = resample(VIF);

    // TODO: Re-evaluate vector of a vector
    std::vector<std::vector<float>> resampled_c;
    for(const auto &c : C) resampled_c.emplace_back( resample(c) );
    


    // Perfusion model implementation should be placed here using resampled time courses.

    // Calculate the DC gain, first equation
    int sum_of_aif, sum_of_vif, sum_of_c;
    sum_of_aif = std::accumulate(resampled_aif.begin(), resampled_aif.end(),
                               decltype(resampled_aif)::value_type(0));
    sum_of_vif = std::accumulate(resampled_vif.begin(), resampled_vif.end(),
                               decltype(resampled_vif)::value_type(0));
    sum_of_c = std::accumulate(resampled_c.front().begin(), resampled_c.front().end(),
                                decltype(resampled_c.front().begin())::value_type(0));
    FUNCINFO("sum of aif " << sum_of_aif << " sum of vif " << sum_of_vif << " sum of c " << sum_of_c);

    // Linear approximation at large t
    // samples_1D<float> linear_c_vals = new samples_1D();
    // std::vector<float> resampled_c_copy = resampled_c.front();

    // for (int i=0; i<10; i++) {
    //     float c = resampled_c_copy.pop_back();
    //     FUNCINFO("resampled_c_copy" << c);
    //     // linear_c_vals.push_back(resampled_c_copy.pop_back());
    // }

    // Construct AIF(t-dt), VIF(t-dt), C(t-dt)
    std::vector<float> shifted_aif = resampled_aif;
    std::vector<float> shifted_vif = resampled_vif;
    std::vector<float> shifted_c = resampled_c.front();

    // Create the shifted vectors by removing the first element
    // Remove the last element of the resampled vectors to ensure same size for inner products
    shifted_aif.erase(shifted_aif.begin());
    resampled_aif.pop_back();
    shifted_vif.erase(shifted_vif.begin());
    resampled_vif.pop_back();
    shifted_c.erase(shifted_c.begin());
    resampled_c.front().pop_back();

    // TODO: Move this constant to a better place for code cleanliness/clarity
    const float sample_rate = 0.1; // Seconds

    // Get the variables R and N (see readme)
    // TODO: Add description of variables + their meaning to Readme
    float R = 7.7;
    float N = (sum_of_c - R * sum_of_aif) / sum_of_vif;

    // std::cout << "shifted aif size: " << shifted_aif.size() << '\n';
    // std::cout << "resampled aif size: " << resampled_aif.size() << '\n';
    // std::cout << "shifted vif size: " << shifted_vif.size() << '\n';
    // std::cout << "resampled vif size: " << resampled_vif.size() << '\n';
    // std::cout << "shifted c size: " << shifted_c.size() << '\n';
    // std::cout << "resampled c size: " << resampled_c.front().size() << '\n';

    std::vector<float> vif_sum;
    std::vector<float> aif_sum;
    std::vector<float> c_diff;
    std::vector<float> D;
    std::vector<float> E;
    
    //adds the following vectors
    std::transform(resampled_vif.begin(), resampled_vif.end(), shifted_vif.begin(), vif_sum.begin( ), std::plus<float>( ));
    std::transform(resampled_aif.begin(), resampled_aif.end(), shifted_aif.begin(), aif_sum.begin( ), std::plus<float>( ));
    std::transform(resampled_c.front().begin(), resampled_c.front().end(), shifted_c.begin(), c_diff.begin(), std::minus<float>( ));

    // Define E(t) and D(t)
    // std::vector<float> D = 2 * (resampled_c.front() - shifted_c);
    MultiplyVectorByScalar(c_diff, 2.0); // D
    D = c_diff;
    //The following lines are the calculation of E. Below is the "simple code" that does not work for vectors
    ////std::vector<float> E = sample_rate * ( MultiplyVectorByScalar(vif_sum, N) + MultiplyVectorByScalar(aif_sum, R) +  MultiplyVectorByScalar(D, 0.5));

    MultiplyVectorByScalar(vif_sum, N); // E_vif
    MultiplyVectorByScalar(aif_sum, R); // E_aif
    MultiplyVectorByScalar(c_diff, 0.5); //E_c

    std::transform(vif_sum.begin(), vif_sum.end(), aif_sum.begin(), E.begin( ), std::plus<float>( ));
    std::transform(E.begin(), E.end(), c_diff.begin(), E.begin( ), std::plus<float>( ));
    MultiplyVectorByScalar(E, sample_rate);

    // Inner product calculation
    double DE_inner_product = std::inner_product(D.begin(), D.end(), E.begin(), 0);
    double EE_inner_product = std::inner_product(E.begin(), E.end(), E.begin(), 0);

    // Get the kinetic parameters from the calculated inner products
    double k2 = DE_inner_product / EE_inner_product;
    double k1_A = R * DE_inner_product / EE_inner_product;
    double k1_B = N * DE_inner_product / EE_inner_product;
    
    // TODO: Output parameters to file
    // TODO: Remove FUNCINFO if necessary?
    //FUNCINFO("k2 parameter: " + k2 + " | k1A: " + k1_A + " | k1B: " + k1_B);
    std::cout << "k2 parameter " << k2 << '\n';
    std::cout << "k1A parameter " << k1_A << '\n';
    std::cout << "k1B parameter " << k1_B << '\n';
    FUNCINFO("K2 " << k2 << " k1A " << k1_A << " k1B " << k1_B);

    // The following is an example of using Eigen for matrices.
    {
        Eigen::Matrix4d A;
        A <<  1.0,  0.0,  0.0,  0.0,
              0.0,  0.0,  1.0,  0.0,
             -3.0,  3.0, -2.0, -1.0,
              2.0, -2.0,  1.0,  1.0;
        auto AT = A.transpose();

        auto C = A * AT;

        double coeff_sum = 0.0;
        for(int i = 0; i < 4; ++i){
            for(int j = 0; j < 4; ++j){
                coeff_sum += C(i,j) * 1.23;
            }
        }

        FUNCINFO("The Eigen example coefficient sum is " << coeff_sum);
    }


    // The following is an example of calling the SYCL GPU-accelerated function above.
    std::vector<double> lhs = {  1.0, -2.0,  0.0, -2.5,  10.0 };
    std::vector<double> rhs = { -1.0,  2.0, -0.0,  2.5, -10.0 };

    cl::sycl::queue q;
    auto result = vec_add(q, lhs, rhs); // Performs vector summation using SYCL on CPU, GPU, FPGA, ...

    double sum = 0.0;
    for(const auto x : result) sum += x;

    if( (sum < -1E-6) || (1E-6 < sum) ){
        FUNCERR("Sum = " << sum << " (should be 0.0)");
    }else{
        FUNCINFO("SYCL function ran successfully.");
    }

    return;
}

void MultiplyVectorByScalar(std::vector<float> &v, float k){
    transform(v.begin(), v.end(), v.begin(), [k](float &c){ return c*k; });
}
// Taken from https://stackoverflow.com/questions/3376124/how-to-add-element-by-element-of-two-stl-vectors
// template <typename T>
// std::vector<T> operator+(const std::vector<T>& a, const std::vector<T>& b)
// {
//     assert(a.size() == b.size());

//     std::vector<T> result;
//     result.reserve(a.size());

//     std::transform(a.begin(), a.end(), b.begin(), 
//                    std::back_inserter(result), std::plus<T>());
//     return result;
// }

// std::vector<T> operator-(const std::vector<T>& a, const std::vector<T>& b)
// {
//     assert(a.size() == b.size());

//     std::vector<T> result;
//     result.reserve(a.size());

//     std::transform(a.begin(), a.end(), b.begin(), 
//                    std::back_inserter(result), std::minus<T>());
//     return result;
// }