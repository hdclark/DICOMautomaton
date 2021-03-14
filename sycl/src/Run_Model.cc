//Run_Model.cc - A part of DICOMautomaton 2020. Written by hal clark, ...
//
// This program is a starter for a perfusion model. It is meant to be compiled with a SYCL-enabled toolchain.

#include <exception>
#include <functional>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <boost/filesystem.hpp>
#include <cstdlib> //Needed for exit() calls.
#include <utility> //Needed for std::pair.

#include "YgorArguments.h" //Needed for ArgumentHandler class.
#include "YgorFilesDirs.h" //Needed for Does_File_Exist_And_Can_Be_Read(...), etc..
#include "YgorMisc.h"      //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorMath.h"      //Needed for samples_1D.
#include "YgorString.h"    //Needed for GetFirstRegex(...)

#include "Perfusion_SCDI.h"
#include "Perfusion_SCSI.h"


int
main(int argc, char *argv[]) {
    //This is the main entry-point for a Single-Compartment Dual-Input (SCDI) blood perfusion model. This function is
    // called when the program is executed. The interface to this program should be kept relatively simple to simplify
    // later integration into the broader DICOMautomaton code base.
    //

    // "Arterial input function," which represents the flow of contrast agent in blood through a major artery. This
    // function is sampled at irregular intervals.
    samples_1D<double> AIF;

    // "Venous input function," which represents the flow of contrast agent in blood through a nearby vein (usually
    // portal vein for liver). This function is sampled at irregular intervals.
    samples_1D<double> VIF;

    // Contrast enhancement curves. These each represent the flow of contrast agent in blood through a small cube of
    // tissue over time. These functions are all sampled at irregular intervals. These functions are combined with the
    // AIF and VIF, modeled according to the SCDI model, and model parameters are extracted from the fitted models. The
    // fitted model parameters are of clinical value.
    //
    // For a clinical implementation, several thousands or even millions of these will need to be handled.
    std::vector<samples_1D<double>> C;

    // Placeholder parameters, in case you need to add any of these.
    bool SCSIRunBool       = false;
    double PlaceholderDouble      = 1.0;
    float PlaceholderFloat        = -1.0;
    std::string PlaceholderString = "placeholder";

    //================================================ Argument Parsing ==============================================

    class ArgumentHandler arger;
    const std::string progname(argv[0]);
    arger.examples    = { { "--help", "Show the help screen and some info about the program." },
                       { "-a aif.txt -v vif.txt -c c.txt",
                         "Load AIF, VIF, and a contrast enhancement curve 'c' from files. The file 'c.txt' will be"
                         " modeled according to the AIF and VIF." } };
    arger.description = "A program for running a blood perfusion model.";

    arger.default_callback = [](int, const std::string &optarg) -> void {
        FUNCERR("Unrecognized option with argument: '" << optarg << "'");
        return;
    };
    arger.optionless_callback = [&](const std::string &optarg) -> void {
        C.emplace_back();
        if(!C.back().Read_From_File(optarg) || C.back().samples.empty()) {
            FUNCERR("Unable to parse C file: '" << optarg << "'");
            exit(1);
        }
        return;
    };

    arger.push_back(ygor_arg_handlr_t(1, 'a', "aif", true, "aif.txt",
                                      "Load an AIF contrast enhancement time course from the given file.",
                                      [&](const std::string &optarg) -> void {
                                          if(!AIF.Read_From_File(optarg) || AIF.samples.empty()) {
                                              FUNCERR("Unable to parse AIF file: '" << optarg << "'");
                                              exit(1);
                                          }
                                          return;
                                      }));

    arger.push_back(ygor_arg_handlr_t(1, 'v', "vif", true, "vif.txt",
                                      "Load a VIF contrast enhancement time course from the given file.",
                                      [&](const std::string &optarg) -> void {
                                          if(!VIF.Read_From_File(optarg) || VIF.samples.empty()) {
                                              FUNCERR("Unable to parse VIF file: '" << optarg << "'");
                                              exit(1);
                                          }
                                          return;
                                      }));

    arger.push_back(ygor_arg_handlr_t(1, 'c', "course", true, "c.txt",
                                      "Load a tissue contrast enhancement time course from the given file.",
                                      [&](const std::string &optarg) -> void {
                                          C.emplace_back();
                                          if(!C.back().Read_From_File(optarg) || C.back().samples.empty()) {
                                              FUNCERR("Unable to parse C file: '" << optarg << "'");
                                              exit(1);
                                          }
                                          return;
                                      }));

    arger.push_back(ygor_arg_handlr_t(3, 'b', "SCSIRunBool", false, "", "Boolean to run single input model.",
                                      [&](const std::string &) -> void {
                                          SCSIRunBool = true;
                                          return;
                                      }));

    arger.push_back(ygor_arg_handlr_t(3, 'f', "placeholder-float", true, "1.23", "Placeholder for a float option.",
                                      [&](const std::string &optarg) -> void {
                                          PlaceholderFloat = std::stof(optarg);
                                          return;
                                      }));

    arger.Launch(argc, argv);

    //============================================= Input Validation ================================================
    if(AIF.samples.empty()) {
        FUNCERR("AIF contains no samples. Unable to continue.");
    }
    if(VIF.samples.empty()) {
        FUNCERR("VIF contains no samples. Unable to continue.");
    }
    if(C.empty()) {
        FUNCERR("No tissue contrast curves to model. Unable to continue.");
    }
    for(const auto &c : C) {
        if(c.samples.empty()) {
            FUNCERR("Tissue contrast curve contains no samples. Unable to continue.");
        }
    }

    //========================================== Launch Perfusion Model =============================================
    timestamp_t t0 = get_timestamp();
    if(SCSIRunBool) {
        Launch_SCSI(AIF, C);
    } else {
        Launch_SCDI(AIF, VIF, C);   
    }
    timestamp_t t1 = get_timestamp();
    double secs    = (t1 - t0) / 1000000.0L;
    FUNCINFO("Runtime:" << secs);
    return 0;
}

timestamp_t
get_timestamp() {
    struct timeval now;
    gettimeofday(&now, NULL);
    return now.tv_usec + (timestamp_t)now.tv_sec * 1000000;
}
