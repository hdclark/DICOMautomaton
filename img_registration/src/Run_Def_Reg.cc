//Run_Def_Reg.cc - A part of DICOMautomaton 2021. Written by hal clark, ...
//
// This program will load files, parse arguments, and run a registration model.

#include <exception>
#include <functional>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <string>    
#include <vector>
#include <filesystem>

#include <cstdlib>            //Needed for exit() calls.
#include <utility>            //Needed for std::pair.
#include <chrono>

#include "YgorArguments.h"    //Needed for ArgumentHandler class.
#include "YgorFilesDirs.h"    //Needed for Does_File_Exist_And_Can_Be_Read(...), etc..
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorMath.h"         //Needed for various geometry classes (e.g., vec3).
#include "YgorImages.h"       //Needed for planar_image_collection and planar_image classes.
#include "YgorImagesIO.h"     //Needed for reading and writing images in FITS format, which preserves embedded metadata and supports 32bit-per-channel intensity.
#include "YgorString.h"       //Needed for GetFirstRegex(...)

#include "Alignment_ABC.h"    // Put header file for implementation 'ABC' here.

using namespace std::chrono;

int main(int argc, char* argv[]){
    //This is the main entry-point for an experimental implementation of the ABC deformable registration algorithm. This
    //function is called when the program is executed. The interface to this program should be kept relatively simple to
    //simplify later integration into the broader DICOMautomaton code base.


    // The 'moving' image array. This is the image array that will be transformed to match the stationary
    // image array.
    planar_image_collection<float, double> moving;

    // The 'stationary' image array. This set of images will be considered the reference or target image array.
    // The deformable registration algorithm will attempt to create a transformation that maps the moving set to
    // the stationary set.
    planar_image_collection<float, double> stationary;

    // This structure is described in Alignment_ABC.h.
    AlignViaABCParams params;

    // See below for description of these parameters. You can also put them in the AlignViaABCParams structure to more
    // easily pass them into your algorithm.
    std::string type = "ABC";
    long int iters = 1;
    double tune = 0.0;

    
    //================================================ Argument Parsing ==============================================

    class ArgumentHandler arger;
    const std::string progname(argv[0]);
    arger.examples = { { "--help", 
                         "Show the help screen and some info about the program." },
                       { "-m moving.fits -s stationary.fits",
                         "Load a moving image array, a stationary image array, and run the"
                         " deformable registration algorithm." }
                     };
    arger.description = "A program for running a deformable registration algorithm.";

    // save the moving filenames
    std::vector<std::string> movingFN;

    arger.default_callback = [](int, const std::string &optarg) -> void {
      FUNCERR("Unrecognized option with argument: '" << optarg << "'");
      return; 
    };
    arger.optionless_callback = [&](const std::string &optarg) -> void {
      FUNCERR("Unrecognized option with argument: '" << optarg << "'");
      return; 
    };
    arger.push_back( ygor_arg_handlr_t(1, 'm', "moving", true, "moving.fits",
      "Load a moving image array from the given file, or directory of images.",
      [&](const std::string &optarg) -> void {
          const auto p = std::filesystem::path(optarg);
          if(std::filesystem::is_directory(p)){
              for(const auto &f_it : std::filesystem::directory_iterator{p}){
                  moving.images.emplace_back(ReadFromFITS<float,double>(f_it.path().string()));
              }
          }else{
              // Assume it's a file.
              moving.images.emplace_back(ReadFromFITS<float,double>(optarg));
          }
          return;
      })
    );
    arger.push_back( ygor_arg_handlr_t(1, 's', "stationary", true, "stationary.fits",
      "Load a stationary image array from the given file, or directory of images.",
      [&](const std::string &optarg) -> void {
          const auto p = std::filesystem::path(optarg);
          if(std::filesystem::is_directory(p)){
              for(const auto &f_it : std::filesystem::directory_iterator{p}){
                  stationary.images.emplace_back(ReadFromFITS<float,double>(f_it.path().string()));
              }
          }else{
              // Assume it's a file.
              stationary.images.emplace_back(ReadFromFITS<float,double>(optarg));
          }
          return;
      })
    );

    arger.push_back( ygor_arg_handlr_t(1, 't', "type", true, "ABC",
      "Which algorithm to use. Options: ABC, ...",
      [&](const std::string &optarg) -> void {
        type = optarg;
        return;
      })
    );
    arger.push_back( ygor_arg_handlr_t(1, 'd', "iterations", true, "1",
      "Number of iterations to perform.",
      [&](const std::string &optarg) -> void {
        iters = std::stol(optarg);
        return;
      })
    );
    arger.push_back( ygor_arg_handlr_t(1, 't', "tune", true, "1.23",
      "Numerical factor that can tune the algorithm.",
      [&](const std::string &optarg) -> void {
        tune = std::stod(optarg);
        return;
      })
    );
    arger.Launch(argc, argv);

    //============================================= Input Validation ================================================
    if(moving.images.empty()){
        FUNCERR("Moving image array contains no images. Unable to continue.");
    }
    if(stationary.images.empty()){
        FUNCERR("Stationary image array contains no images. Unable to continue.");
    }

    //============================================ Perform Registration  =============================================

    high_resolution_clock::time_point start = high_resolution_clock::now();
    if(type == "ABC") {
        
        // Perform your registration algorithm here.
        // The result is a transform that can be saved, applied to the moving images, or applied to other kinds of
        // objects (e.g., surface meshes).
        std::optional<AlignViaABCTransform> transform_opt = AlignViaABC(params, moving, stationary );

        if(transform_opt){
            transform_opt.value().apply_to(moving);

            // save the transformed images
            int i = 1;
            for (auto& img : moving.images) {
                bool save = WriteToFITS<float,double>(img, "images/transformed/" + std::to_string(i) + ".fits");
                if (!save) {
                    FUNCERR("Could not save transformed images.");
                    return 1;
                }
            }
        }

        // If needed (for testing, debugging, ...) try to apply the transform.
        //transform_opt.value().apply_to(moving);

        // If needed, try save the transform by writing it to file.
        //transform_opt.value().write_to("transform.txt");

    } else {
        FUNCERR("Specified algorithm specified was invalid. Options are ABC, ...");
        return 1;
    }

    high_resolution_clock::time_point stop = high_resolution_clock::now();
    duration<double> time_span = duration_cast<duration<double>>(stop - start);
    FUNCINFO("Excecution took time: " << time_span.count())
    return 0;
}
