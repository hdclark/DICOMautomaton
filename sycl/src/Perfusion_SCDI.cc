//Perfusion_Model.cc -- A part of DICOMautomaton 2020. Written by hal clark, ...
//
// This file is meant to contain an implementation of the SCDI blood perfusion kinetic model.

#include <exception>
#include <functional>
#include <iostream>
#include <list>
#include <limits>
#include <map>
#include <memory>
#include <string>    
#include <vector>

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
    std::vector<std::vector<float>> resampled_c;
    for(const auto &c : C) resampled_c.emplace_back( resample(c) );
    


    // Perfusion model implementation should be placed here using resampled time courses.

    // ...



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

