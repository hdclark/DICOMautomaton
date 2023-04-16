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
#include <ctime>
#include <cstdint>

#include <cstdlib> //Needed for exit() calls.
#include <utility> //Needed for std::pair.

#include <boost/filesystem.hpp> // Needed for Boost filesystem access.

#include <CL/sycl.hpp> //Needed for SYCL routines.

#include <eigen3/Eigen/Dense> //Needed for Eigen library dense matrices.

#include "YgorFilesDirs.h" //Needed for Does_File_Exist_And_Can_Be_Read(...), etc..
#include "YgorMisc.h"      //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorLog.h"
#include "YgorMath.h"      //Needed for samples_1D.
#include "YgorString.h"    //Needed for GetFirstRegex(...)

#include "Perfusion_SCDI.h"
#include "Common.h"

#define TIME_INTERVAL 0.1 //In units of seconds

// Closed form single comparment dual input model, inpsired by https://escholarship.org/uc/item/8145r963 single comparment single input model
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
        int64_t N = 0;

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

    std::vector<std::vector<float>> resampled_c;
    for(const auto &c : C) resampled_c.emplace_back(resample(c));

    // Calculate the DC gain, first equation
    const float sum_of_aif = std::accumulate(resampled_aif.begin(), resampled_aif.end(), 0.0f);
    const float sum_of_vif = std::accumulate(resampled_vif.begin(), resampled_vif.end(), 0.0f);
    std::vector<float> sum_of_c;
    for(auto c : resampled_c) sum_of_c.push_back(std::accumulate(c.begin(), c.end(), 0.0f));
    // YLOGINFO("sum of aif " << sum_of_aif << " sum of vif " << sum_of_vif << " sum of c " << sum_of_c);

    // Linear approximation at large t
    std::vector<samples_1D<float>> linear_c_vals;
    samples_1D<float> linear_aif_vals;
    samples_1D<float> linear_vif_vals;

    for(unsigned long i = 0; i < resampled_c.size(); i++) {
        samples_1D<float> c;
        linear_c_vals.push_back(c);
    }

    const auto c_size = static_cast<int64_t>(resampled_c.front().size()); // all c vectors are the same size
    const auto slope_window = 100L;

    for(auto i = (c_size - slope_window); i < c_size; i++) {
        float t = TIME_INTERVAL * i;
        for(unsigned long j = 0; j < resampled_c.size(); j++) {
            linear_c_vals.at(j).push_back(t, resampled_c.at(j).at(i)); // update every vector in the resampled_c vector
        }
        linear_aif_vals.push_back(t, resampled_aif.at(i));
        linear_vif_vals.push_back(t, resampled_vif.at(i));
    }

    // Approximate region by a line, all C related calculations are in the for loop
    const auto aif_res = linear_aif_vals.Linear_Least_Squares_Regression();
    const auto vif_res = linear_vif_vals.Linear_Least_Squares_Regression();

    const auto aif_slope     = static_cast<float>(aif_res.slope);
    const auto aif_intercept = static_cast<float>(aif_res.intercept);
    const auto vif_slope     = static_cast<float>(vif_res.slope);
    const auto vif_intercept = static_cast<float>(vif_res.intercept);

    // Find eqn 2
    const float time_midpoint = static_cast<float>(c_size - (slope_window)*0.5) * TIME_INTERVAL;
    const float VIF_pt        = time_midpoint * vif_slope + vif_intercept;
    const float AIF_pt        = time_midpoint * aif_slope + aif_intercept;

    // Construct AIF(t-dt), VIF(t-dt), C(t-dt)
    std::vector<float> shifted_aif = resampled_aif;
    std::vector<float> shifted_vif = resampled_vif;

    // Create the shifted vectors by removing the first element
    // Remove the last element of the resampled vectors to ensure same size for inner products
    shifted_aif.erase(shifted_aif.begin());
    resampled_aif.pop_back();
    shifted_vif.erase(shifted_vif.begin());
    resampled_vif.pop_back();

    std::vector<float> vif_sum; //this is defined as vif(t)+vif(t-T)
    std::vector<float> aif_sum; //this is defined as aif(t)+aif(t-T)

    std::transform(resampled_vif.begin(), resampled_vif.end(), shifted_vif.begin(), back_inserter(vif_sum),
                   std::plus<float>()); //vif_sum = resampled_vif + shifted_vif
    std::transform(resampled_aif.begin(), resampled_aif.end(), shifted_aif.begin(), back_inserter(aif_sum),
                   std::plus<float>()); //aif_sum = resampled_aif + shifted_aif

    // Generate filename in the format kParams_SCDI_timestamp.txt
    auto timestamp = std::time(nullptr);
    std::stringstream strm;
    strm << timestamp;
    auto kParamsFileName = "kParams_SCDI_" + strm.str() + ".txt";

    // All calculations that are needed per C file are included in this loop
    for(unsigned long i = 0; i < linear_c_vals.size(); i++) {
        const auto c_res       = linear_c_vals.at(i).Linear_Least_Squares_Regression();
        const auto c_slope     = static_cast<float>(c_res.slope);
        const auto c_intercept = static_cast<float>(c_res.intercept);

        const float C_pt = time_midpoint * c_slope + c_intercept;

        const float R = (C_pt - (sum_of_c.at(i) / sum_of_vif) * VIF_pt) / (AIF_pt - (sum_of_aif / sum_of_vif) * VIF_pt);
        const float Q = c_slope / (AIF_pt - (sum_of_aif / sum_of_vif) * VIF_pt);
        const float N = (sum_of_c.at(i) - R * sum_of_aif) / sum_of_vif;

        // Construct C(t-dt)
        std::vector<float> shifted_c = resampled_c.at(i);

        // Create shifted C vectors
        shifted_c.erase(shifted_c.begin());
        resampled_c.at(i).pop_back();

        std::vector<float> D;      //see math for definition
        std::vector<float> E;      //see math for definition
        std::vector<float> F;      //see math for definition
        std::vector<float> G;      //see math for definition
        std::vector<float> c_sum;  //this is defined as c(t)+c(t-T)
        std::vector<float> c_diff; //this is defined as c(t)-c(t-T)

        std::transform(resampled_c.at(i).begin(), resampled_c.at(i).end(), shifted_c.begin(), back_inserter(c_diff),
                       std::minus<float>()); //c_diff = resampled_c - shifted_c
        std::transform(resampled_c.at(i).begin(), resampled_c.at(i).end(), shifted_c.begin(), back_inserter(c_sum),
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

        // Get the kinetic parameters from the calculated inner products
        const float k2   = GE_inner_product / EE_inner_product;
        const float k1_A = R * k2 + Q;
        const float k1_V = N * k2 - Q * sum_of_aif / sum_of_vif;

        std::ofstream kParamsFile;
        kParamsFile.open(kParamsFileName , std::ofstream::out | std::ofstream::app);
        if (kParamsFile.is_open()) {
            kParamsFile << k1_A << " " << k1_V << " " << k2 << "\n";
            kParamsFile.close();
        }
    }

    return;
}
