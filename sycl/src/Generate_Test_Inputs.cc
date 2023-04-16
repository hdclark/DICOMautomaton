//Generate_Test_Inputs.cc - A part of DICOMautomaton 2020. Written by hal clark, ...
//
// This program is used to generate synthetic inputs for testing a perfusion model.

#include <exception>
#include <functional>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <random>
#include <cstdint>

#include <cstdlib> //Needed for exit() calls.
#include <utility> //Needed for std::pair.

#include "YgorArguments.h" //Needed for ArgumentHandler class.
#include "YgorFilesDirs.h" //Needed for Does_File_Exist_And_Can_Be_Read(...), etc..
#include "YgorMisc.h"      //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorLog.h"
#include "YgorMath.h"      //Needed for samples_1D.
#include "YgorString.h"    //Needed for GetFirstRegex(...)


samples_1D<double>
make_test_SCDI_C(const samples_1D<double>& AIF, const samples_1D<double>& VIF, double k1A, double k1V, double k2) {
    // This function uses an AIF, a VIF, and the Single-Compartment Dual-Input (SCDI) blood perfusion model parameters
    // to generate a synthetic contrast-enhancement time course 'C'.
    //
    // This function uses a backward finite-difference approximation to solve for C(t) over time.
    //
    // Note: each of the AIF, VIF, and C are required to be zero at t=0. This is for convenience for a
    // physically-sensible model (i.e., there should be no contrast enhancement before contrast agent is injected).

    if(AIF.samples.size() != VIF.samples.size()) {
        throw std::invalid_argument("This routine requires AIF and VIF to be sampled at the same times.");
    }
    const auto N_samples = static_cast<int64_t>(AIF.samples.size());

    if(N_samples < 2) {
        throw std::invalid_argument("The AIF and VIF do not contain enough data.");
    }

    samples_1D<double> C;
    const bool inhibit_sort = true;
    const auto eps          = std::sqrt(std::numeric_limits<double>::epsilon());
    for(int64_t i = 0; i < N_samples; ++i) {
        if(i == 0) {
            C.push_back(0.0, 0.0, inhibit_sort);
        } else {
            const double t_prev = AIF.samples.at(i - 1).at(0);
            const double t_now  = AIF.samples.at(i).at(0);
            const double dt     = (t_now - t_prev);
            if(dt < eps) {
                throw std::invalid_argument("Temporal sampling too frequent -- is there a duplicate sample?");
            }

            const double A_now  = AIF.samples.at(i).at(2);
            const double V_now  = VIF.samples.at(i).at(2);
            const double C_prev = C.samples.at(i - 1).at(2);

            const double C_now = (C_prev + dt * (k1A * A_now + k1V * V_now)) / (1.0 + k2 * dt);

            C.push_back(t_now, C_now, inhibit_sort);
        }
    }

    return C;
}

int
main(int, char**) {
    // SCDI model parameters.
    const double k1A = 0.75; // Mostly arterial supply.
    const double k1V = 0.25;
    const double k2  = 0.25; // Slow outflow.

    std::ofstream kParamsFile("inputkParams.txt");

    if (kParamsFile.is_open()) {
        kParamsFile << k1A << " " << k1V << " " << k2;
        kParamsFile.close();
    }

    // Sampling parameters.
    const double dt          = 1.2; // seconds.
    const double t_start     = 0.0; // seconds.
    const int64_t N_samples = 100;  // number of CT/MR images taken serially.

    std::random_device rdev;
    std::mt19937 re(rdev());
    std::uniform_real_distribution<> ud(0.0, 1.0);
    double mean    = 0.0;
    double std_dev = 0.1;
    std::normal_distribution<> nd(mean, std_dev);

    // Make an Arterial input function (AIF) and Venous input function (VIF).
    samples_1D<double> AIF;
    samples_1D<double> AIF_noise;
    samples_1D<double> VIF;
    samples_1D<double> VIF_noise;

    const bool inhibit_sort = true;
    for(int64_t i = 0; i < N_samples; ++i) {
        const auto t = t_start + dt * static_cast<double>(i);
        if((i == 0) || (i == 1)) {
            AIF.push_back(t, 0.0, inhibit_sort);
            VIF.push_back(t, 0.0, inhibit_sort);
            AIF_noise.push_back(t, 0.0, inhibit_sort);
            VIF_noise.push_back(t, 0.0, inhibit_sort);
        } else {
            // These are purely synthetic, created specifically to mimic the overall shape of real AIF and VIF.
            const double A = std::exp(-std::pow(t - 15.0, 2.0) / 15.0) + 0.2 * std::exp(-std::pow(t - 30.0, 2.0) / 15.0)
                             + 0.25 * (10.0 + std::tanh(t - 15.0) / 0.1) * std::exp(-std::sqrt(t) / 1.75);
            AIF.push_back(t, A, inhibit_sort);
            AIF_noise.push_back(t, A + nd(re), inhibit_sort);
            const double V = t * (10.0 + std::tanh(t - 13.0) / 0.1) * std::exp(-std::sqrt(t) / 1.75) / 55.0;
            VIF.push_back(t, V, inhibit_sort);
            VIF_noise.push_back(t, V + nd(re), inhibit_sort);
        }
    }

    // Using the AIF, VIF, and model parameters, create the C(t) we would observe (if the model were exactly correct).
    const samples_1D<double> C       = make_test_SCDI_C(AIF, VIF, k1A, k1V, k2);
    const samples_1D<double> C_noise = make_test_SCDI_C(AIF_noise, VIF_noise, k1A, k1V, k2);


    // Write AIF, VIF, and C to file. These files can be used to fit the model and try recover k1A, k1V, and k2.
    AIF.Write_To_File("aif.txt");
    AIF_noise.Write_To_File("aif_noise.txt");
    VIF.Write_To_File("vif.txt");
    VIF_noise.Write_To_File("vif_noise.txt");
    C.Write_To_File("c.txt");
    C_noise.Write_To_File("c_noise.txt");

    return 0;
}
