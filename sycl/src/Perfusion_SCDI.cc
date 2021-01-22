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

#include <cstdlib> //Needed for exit() calls.
#include <utility> //Needed for std::pair.

#include <boost/filesystem.hpp> // Needed for Boost filesystem access.

#include <CL/sycl.hpp> //Needed for SYCL routines.

#include <eigen3/Eigen/Dense> //Needed for Eigen library dense matrices.

#include "YgorFilesDirs.h" //Needed for Does_File_Exist_And_Can_Be_Read(...), etc..
#include "YgorMisc.h"      //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorMath.h"      //Needed for samples_1D.
#include "YgorString.h"    //Needed for GetFirstRegex(...)

#include "Perfusion_SCDI.h"

#define TIME_INTERVAL 0.1 //In units of seconds

// This SYCL-powered function performs vector summation. It can run on CPU, GPU, FPGA, etc.
// It is only here to demonstrate roughly how SYCL is used.
static std::vector<double>
vec_add(cl::sycl::queue &q, const std::vector<double> &lhs, const std::vector<double> &rhs) {
    const auto N = lhs.size();
    std::vector<double> dst(N); // Destination; buffer for the result.

    if(N != rhs.size()) {
        throw std::runtime_error("This routine only supports same-size buffer addition. Cannot continue.");
    }
    if(N != dst.size()) {
        throw std::runtime_error("This routine only supports same-size buffer outputs. Cannot continue.");
    }

    cl::sycl::range<1> work_items { N };

    {
        cl::sycl::buffer<double> buff_lhs(lhs.data(), N);
        cl::sycl::buffer<double> buff_rhs(rhs.data(), N);
        cl::sycl::buffer<double> buff_dst(dst.data(), N);

        q.submit([&](cl::sycl::handler &cgh) {
            auto access_lhs = buff_lhs.get_access<cl::sycl::access::mode::read>(cgh);
            auto access_rhs = buff_rhs.get_access<cl::sycl::access::mode::read>(cgh);
            auto access_dst = buff_dst.get_access<cl::sycl::access::mode::write>(cgh);

            cgh.parallel_for<class vector_add>(work_items, [=](cl::sycl::id<1> tid) {
                access_dst[tid] = access_lhs[tid] + access_rhs[tid];
            });
        });
    }
    return dst;
}

// This function should be where the blood perfusion model is implemented.
void
Launch_SCDI(samples_1D<double> &AIF, samples_1D<double> &VIF, std::vector<samples_1D<double>> &C) {
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
        const double dt      = TIME_INTERVAL; // units of seconds.
        const auto cropped   = s.Select_Those_Within_Inc(0.0, std::numeric_limits<double>::infinity());
        const auto extrema_x = cropped.Get_Extreme_Datum_x();

        std::vector<float> resampled;
        long int N = 0;

        while(true) {
            const double t = 0.0 + static_cast<double>(N) * dt;
            if(t < extrema_x.first[0]) {
                throw std::logic_error("Time courses should start at 0. Please adjust the time course.");
            }
            if(extrema_x.second[0] < t)
                break;
            if(1'000'000 < N) {
                throw std::runtime_error("Excessive number of samples detected. Is this intended?");
            }

            resampled.emplace_back(cropped.Interpolate_Linearly(t)[2]);
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
    for(const auto &c : C) resampled_c.emplace_back(resample(c));


    // Perfusion model implementation should be placed here using resampled time courses.

    // Calculate the DC gain, first equation
    const float sum_of_aif = std::accumulate(resampled_aif.begin(), resampled_aif.end(), 0.0f);
    const float sum_of_vif = std::accumulate(resampled_vif.begin(), resampled_vif.end(), 0.0f);
    const float sum_of_c   = std::accumulate(resampled_c.front().begin(), resampled_c.front().end(), 0.0f);
    FUNCINFO("sum of aif " << sum_of_aif << " sum of vif " << sum_of_vif << " sum of c " << sum_of_c);

    // Linear approximation at large t
    samples_1D<float> linear_c_vals;
    samples_1D<float> linear_aif_vals;
    samples_1D<float> linear_vif_vals;


    const auto c_size       = static_cast<long int>(resampled_c.front().size());
    const auto slope_window = 100L;
    for(auto i = (c_size - slope_window); i < c_size; i++) {
        float t = TIME_INTERVAL * i;
        linear_c_vals.push_back(t, resampled_c.front().at(i));
        linear_aif_vals.push_back(t, resampled_aif.at(i));
        linear_vif_vals.push_back(t, resampled_vif.at(i));
    }
    FUNCINFO("Length of linear_c_vals: " << linear_c_vals.size());

    // Approximate region by a line
    const auto c_res   = linear_c_vals.Linear_Least_Squares_Regression();
    const auto aif_res = linear_aif_vals.Linear_Least_Squares_Regression();
    const auto vif_res = linear_vif_vals.Linear_Least_Squares_Regression();

    const auto c_slope       = static_cast<float>(c_res.slope);
    const auto c_intercept   = static_cast<float>(c_res.intercept);
    const auto aif_slope     = static_cast<float>(aif_res.slope);
    const auto aif_intercept = static_cast<float>(aif_res.intercept);
    const auto vif_slope     = static_cast<float>(vif_res.slope);
    const auto vif_intercept = static_cast<float>(vif_res.intercept);

    FUNCINFO("The slope is " << c_slope);
    FUNCINFO("The amount of data points in C is " << c_size);

    // Find eqn 2
    const float time_midpoint = static_cast<float>(c_size -  (slope_window) * 0.5 )* TIME_INTERVAL;
    const float C_pt          = time_midpoint * c_slope + c_intercept;
    const float VIF_pt        = time_midpoint * vif_slope + vif_intercept;
    const float AIF_pt        = time_midpoint * aif_slope + aif_intercept;

    FUNCINFO("C point is " << C_pt << " VIF point is " << VIF_pt << " AIF point is " << AIF_pt);

    // Find R
    const float R = (C_pt - (sum_of_c / sum_of_vif) * VIF_pt) / (AIF_pt - (sum_of_aif / sum_of_vif) * VIF_pt);
    FUNCINFO("R is " << R);
    const float Q = c_slope / (AIF_pt - (sum_of_aif / sum_of_vif) * VIF_pt);
    FUNCINFO("Q is " << Q);
    const float N = (sum_of_c - R * sum_of_aif) / sum_of_vif;
    FUNCINFO("N is " << N);

    // Construct AIF(t-dt), VIF(t-dt), C(t-dt)
    std::vector<float> shifted_aif = resampled_aif;
    std::vector<float> shifted_vif = resampled_vif;
    std::vector<float> shifted_c   = resampled_c.front();

    // Create the shifted vectors by removing the first element
    // Remove the last element of the resampled vectors to ensure same size for inner products
    shifted_aif.erase(shifted_aif.begin());
    resampled_aif.pop_back();
    shifted_vif.erase(shifted_vif.begin());
    resampled_vif.pop_back();
    shifted_c.erase(shifted_c.begin());
    resampled_c.front().pop_back();

    std::vector<float> D;       //see math for definition
    std::vector<float> E;       //see math for definition
    std::vector<float> F;       //see math for definition
    std::vector<float> G;       //see math for definition
    std::vector<float> vif_sum; //this is defined as vif(t)+vif(t-T)
    std::vector<float> aif_sum; //this is defined as aif(t)+aif(t-T)
    std::vector<float> c_sum;   //this is defined as c(t)+c(t-T)
    std::vector<float> c_diff;  //this is defined as c(t)-c(t-T)

    std::transform(resampled_vif.begin(), resampled_vif.end(), shifted_vif.begin(), back_inserter(vif_sum),
                   std::plus<float>()); //vif_sum = resampled_vif + shifted_vif
    std::transform(resampled_aif.begin(), resampled_aif.end(), shifted_aif.begin(), back_inserter(aif_sum),
                   std::plus<float>()); //aif_sum = resampled_aif + shifted_aif
    std::transform(resampled_c.front().begin(), resampled_c.front().end(), shifted_c.begin(), back_inserter(c_diff),
                   std::minus<float>()); //c_diff = resampled_c - shifted_c
    std::transform(resampled_c.front().begin(), resampled_c.front().end(), shifted_c.begin(), back_inserter(c_sum),
                   std::plus<float>()); //c_sum = resampled_c + shifted_c

    //Computation of D
    D = c_diff;
    MultiplyVectorByScalar(D, 2.0f); // gives us D(t)=2(c(t)-c(t-T))

    //Use these to be able to multiply by scalars/add them without changing the orginal values
    std::vector<float> vif_sum_temp = vif_sum;
    std::vector<float> aif_sum_temp = aif_sum;

    //Computation of F
    MultiplyVectorByScalar(vif_sum_temp, (-Q * sum_of_aif / sum_of_vif));
    MultiplyVectorByScalar(aif_sum_temp, Q);

    std::transform(vif_sum_temp.begin(), vif_sum_temp.end(), aif_sum_temp.begin(), back_inserter(F),
                   std::plus<float>()); //adds the two above and saves them to F

    MultiplyVectorByScalar(F, TIME_INTERVAL); //Gives us the final version of F as in the mathematical Model

    //Computation of E
    vif_sum_temp = vif_sum;
    aif_sum_temp = aif_sum;

    MultiplyVectorByScalar(vif_sum_temp, N);
    MultiplyVectorByScalar(aif_sum_temp, R);

    std::transform(vif_sum_temp.begin(), vif_sum_temp.end(), aif_sum_temp.begin(), back_inserter(E),
                   std::plus<float>());
    std::transform(E.begin(), E.end(), c_sum.begin(), E.begin(), std::minus<float>());
    MultiplyVectorByScalar(E, TIME_INTERVAL);

    //Computation of G
    std::transform(D.begin(), D.end(), F.begin(), back_inserter(G), std::minus<float>());

    // Inner product calculation
    const float GE_inner_product = std::inner_product(G.begin(), G.end(), E.begin(), 0.0f);
    const float EE_inner_product = std::inner_product(E.begin(), E.end(), E.begin(), 0.0f);

    // Intentionally perform undefined behaviour with an out-of-range read.
    // (This is just to test whether the sanitizers were correctly compiled in and working.)
    //G.shrink_to_fit();
    //const auto test = G[ G.size() + 1 ];
    //FUNCINFO("Avoiding optimizing away by printing " << test);

    FUNCINFO("G.E = " << GE_inner_product);
    FUNCINFO("E.E = " << EE_inner_product);

    // Get the kinetic parameters from the calculated inner products
    const float k2   = GE_inner_product / EE_inner_product;
    const float k1_A = R * k2 + Q;
    const float k1_B = N * k2 - Q * sum_of_aif / sum_of_vif;
    FUNCINFO("K2: " << k2 << " k1A: " << k1_A << " k1B: " << k1_B);

    // The following is an example of using Eigen for matrices.
    {
        Eigen::Matrix4d A;
        A << 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, -3.0, 3.0, -2.0, -1.0, 2.0, -2.0, 1.0, 1.0;
        auto AT = A.transpose();

        auto C = A * AT;

        double coeff_sum = 0.0;
        for(int i = 0; i < 4; ++i) {
            for(int j = 0; j < 4; ++j) { coeff_sum += C(i, j) * 1.23; }
        }

        FUNCINFO("The Eigen example coefficient sum is " << coeff_sum);
    }


    // The following is an example of calling the SYCL GPU-accelerated function above.
    std::vector<double> lhs = { 1.0, -2.0, 0.0, -2.5, 10.0 };
    std::vector<double> rhs = { -1.0, 2.0, -0.0, 2.5, -10.0 };

    cl::sycl::queue q;
    auto result = vec_add(q, lhs, rhs); // Performs vector summation using SYCL on CPU, GPU, FPGA, ...

    double sum = 0.0;
    for(const auto x : result) sum += x;

    if((sum < -1E-6) || (1E-6 < sum)) {
        FUNCERR("Sum = " << sum << " (should be 0.0)");
    } else {
        FUNCINFO("SYCL function ran successfully.");
    }

    return;
}

void
MultiplyVectorByScalar(std::vector<float> &v, float k) {
    transform(v.begin(), v.end(), v.begin(), [k](float &c) {
        return c * k;
    });
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