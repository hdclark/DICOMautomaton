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
#include "Common.h"

#define TIME_INTERVAL 0.1 //In units of seconds

// This function should be where the blood perfusion model is implemented.
void
Launch_SCSI(samples_1D<double> &AIF, std::vector<samples_1D<double>> &C) {
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

    // TODO: Re-evaluate vector of a vector
    std::vector<std::vector<float>> resampled_c;
    for(const auto &c : C) resampled_c.emplace_back(resample(c));

    // Calculate the DC gain, first equation
    const float sum_of_aif = std::accumulate(resampled_aif.begin(), resampled_aif.end(), 0.0f);
    std::vector<float> sum_of_c;
    for(auto c : resampled_c) sum_of_c.push_back(std::accumulate(c.begin(), c.end(), 0.0f));

    // Construct AIF(t-dt), VIF(t-dt), C(t-dt)
    std::vector<float> shifted_aif = resampled_aif;

    // Create the shifted vectors by removing the first element
    // Remove the last element of the resampled vectors to ensure same size for inner products
    shifted_aif.erase(shifted_aif.begin());
    resampled_aif.pop_back();

    std::vector<float> aif_sum; //this is defined as aif(t)+aif(t-T)

    std::transform(resampled_aif.begin(), resampled_aif.end(), shifted_aif.begin(), back_inserter(aif_sum),
                   std::plus<float>()); //aif_sum = resampled_aif + shifted_aif

    for(unsigned long i = 0; i < resampled_c.size(); i++) {
        // DC gain
        const float dc_gain = sum_of_c.at(i) / sum_of_aif;
        FUNCINFO("DC gain:" << dc_gain);
        // Construct C(t-dt)
        std::vector<float> shifted_c = resampled_c.at(i);

        // Create shifted C vectors
        shifted_c.erase(shifted_c.begin());
        resampled_c.at(i).pop_back();

        std::vector<float> c_sum;  //this is defined as c(t)+c(t-T)
        std::vector<float> c_diff; //this is defined as c(t)-c(t-T)

        std::transform(resampled_c.at(i).begin(), resampled_c.at(i).end(), shifted_c.begin(), back_inserter(c_diff),
                       std::minus<float>()); //c_diff = resampled_c - shifted_c
        std::transform(resampled_c.at(i).begin(), resampled_c.at(i).end(), shifted_c.begin(), back_inserter(c_sum),
                       std::plus<float>()); //c_sum = resampled_c + shifted_c
       
        //Computation of D
        std::vector<float> D;      //see math for definition
        D = c_diff;
        MultiplyVectorByScalar(D, 2.0f); // gives us D(t)=2(c(t)-c(t-T))

        //Computation of E
        std::vector<float> E;      //see math for definition
        MultiplyVectorByScalar(aif_sum, dc_gain); 
        std::transform(aif_sum.begin(), aif_sum.end(), c_sum.begin(), back_inserter(E), std::minus<float>());
        MultiplyVectorByScalar(E, TIME_INTERVAL);

        // Inner product calculation
        const float DE_inner_product = std::inner_product(D.begin(), D.end(), E.begin(), 0.0f);
        const float EE_inner_product = std::inner_product(E.begin(), E.end(), E.begin(), 0.0f);

        FUNCINFO("D.E = " << DE_inner_product);
        FUNCINFO("E.E = " << EE_inner_product);

        // Get the kinetic parameters from the calculated inner products
        const float k2   = DE_inner_product / EE_inner_product;
        const float k1_A = dc_gain * k2;

        FUNCINFO("K2: " << k2 << " k1A: " << k1_A);

        std::ofstream kParamsFile("kParams.txt");
    //kParamsFile.open("kParams.txt");
        if (kParamsFile.is_open()) {
            kParamsFile << k1_A << " " << k2 << "\n";
            kParamsFile.close();
        }
    }
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