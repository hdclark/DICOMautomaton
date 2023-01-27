//Generate_Expected_Output.cc - A part of DICOMautomaton 2021. Written by Tianna Hudak, Krysten Zissos
//
// This program is used to generate the expected measured (C) contrast enhancement time course associated with given aif time course, vif time course, and K parameters.

#include <exception>
#include <functional>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <random>

#include <boost/filesystem.hpp>
#include <cstdlib> //Needed for exit() calls.
#include <utility> //Needed for std::pair.

#include "YgorArguments.h" //Needed for ArgumentHandler class.
#include "YgorFilesDirs.h" //Needed for Does_File_Exist_And_Can_Be_Read(...), etc..
#include "YgorMisc.h"      //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorLog.h"
#include "YgorMath.h"      //Needed for samples_1D.
#include "YgorString.h"    //Needed for GetFirstRegex(...)

#include "Perfusion_SCDI.h"


samples_1D<double>
make_output_c(const samples_1D<double>& AIF, const samples_1D<double>& VIF, double k1A, double k1V, double k2) {
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
    const auto N_samples = static_cast<long int>(AIF.samples.size());

    if(N_samples < 2) {
        throw std::invalid_argument("The AIF and VIF do not contain enough data.");
    }

    samples_1D<double> C;
    const bool inhibit_sort = true;
    const auto eps          = std::sqrt(std::numeric_limits<double>::epsilon());
    for(long int i = 0; i < N_samples; ++i) {
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
main(int argc, char *argv[]) {
    // SCDI model parameters.
    const double k1A = 0.00117036; // Mostly arterial supply.
    const double k1V = 0.0179035;
    const double k2  = 0.0718663; // Slow outflow.

    // Make an Arterial input function (AIF) and Venous input function (VIF).
    samples_1D<double> AIF;
    samples_1D<double> VIF;

    //Parse AIF and VIF files 
    class ArgumentHandler arger;
    const std::string progname(argv[0]);
    arger.description = "A program to generate the expected measured (C) contrast enhancement time course associated with given aif time course, vif time course, and K parameters.";
    arger.default_callback = [](int, const std::string &optarg) -> void {
        YLOGERR("Unrecognized option with argument: '" << optarg << "'");
        return;
    };

    arger.push_back(ygor_arg_handlr_t(1, 'a', "aif", true, "aif.txt",
                                      "Load an AIF contrast enhancement time course from the given file.",
                                      [&](const std::string &optarg) -> void {
                                          if(!AIF.Read_From_File(optarg) || AIF.samples.empty()) {
                                              YLOGERR("Unable to parse AIF file: '" << optarg << "'");
                                              exit(1);
                                          }
                                          return;
                                      }));

    arger.push_back(ygor_arg_handlr_t(1, 'v', "vif", true, "vif.txt",
                                      "Load a VIF contrast enhancement time course from the given file.",
                                      [&](const std::string &optarg) -> void {
                                          if(!VIF.Read_From_File(optarg) || VIF.samples.empty()) {
                                              YLOGERR("Unable to parse VIF file: '" << optarg << "'");
                                              exit(1);
                                          }
                                          return;
                                      }));

    arger.Launch(argc, argv);

    // Validate Inputs
    if(AIF.samples.empty()) {
        YLOGERR("AIF contains no samples. Unable to continue.");
    }
    if(VIF.samples.empty()) {
        YLOGERR("VIF contains no samples. Unable to continue.");
    }

    // Function to resample AIF and VIF as constant spaced 
    const auto resample = [](const samples_1D<double> &s) -> samples_1D<double> {
        const double dt      = 0.1; // units of seconds.
        const auto cropped   = s.Select_Those_Within_Inc(0.0, std::numeric_limits<double>::infinity());
        const auto extrema_x = cropped.Get_Extreme_Datum_x();

        samples_1D<double> resampled;
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

            resampled.push_back(t, cropped.Interpolate_Linearly(t)[2]);

            ++N;
        }
        return resampled;
    };

    //Resample AIF and VIF to have constant sample rates -- may need to include entire function
    samples_1D<double> resampled_aif = resample(AIF);
    samples_1D<double> resampled_vif = resample(VIF);

    // Using the AIF, VIF, and model parameters, create the C(t) we would observe (if the model were exactly correct).
    const samples_1D<double> C = make_output_c(resampled_aif, resampled_vif, k1A, k1V, k2);

    // Write C to file
    C.Write_To_File("c.txt");

    return 0;
}
