//Run_Def_Reg.cc - A part of DICOMautomaton 2020. Written by hal clark, ...
//
// This program will load files, parse arguments, and run a deformable registration model.

#include <exception>
#include <functional>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <string>    
#include <vector>

#include <boost/filesystem.hpp>
#include <cstdlib>            //Needed for exit() calls.
#include <utility>            //Needed for std::pair.

#include "YgorArguments.h"    //Needed for ArgumentHandler class.
#include "YgorFilesDirs.h"    //Needed for Does_File_Exist_And_Can_Be_Read(...), etc..
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorMath.h"         //Needed for point_set.
#include "YgorMathIOXYZ.h"    //Needed for ReadPointSetFromXYZ.
#include "YgorString.h"       //Needed for GetFirstRegex(...)

// CPD_Shared.h file
#include "CPD_Shared.h"
#include "CPD_Rigid.h"
#include "CPD_Affine.h"
#include "CPD_Nonrigid.h"


int main(int argc, char* argv[]){
    //This is the main entry-point for an experimental implementation of the ABC deformable registration algorithm. This
    //function is called when the program is executed. The interface to this program should be kept relatively simple to
    //simplify later integration into the broader DICOMautomaton code base.


    // The 'moving' point set / point cloud. This is the point cloud that will be transformed to match the stationary
    // point set.
    point_set<double> moving;

    // The 'stationary' point set / point cloud. This point cloud will be considered the reference or target point
    // cloud. The deformable registration algorithm will attempt to create a transformation that maps the moving set to
    // the stationary set.
    point_set<double> stationary;

    // This structure is described in Alignment_ABC.h.
    CPDParams params;
    
    std::string type;
    std::string xyz_outfile;
    std::string temp_xyz_outfile;
    std::string tf_outfile;
    int iter_interval;
    std::string video;

    //================================================ Argument Parsing ==============================================

    class ArgumentHandler arger;
    const std::string progname(argv[0]);
    arger.examples = { { "--help", 
                         "Show the help screen and some info about the program." },
                       { "-m moving.txt -s stationary.txt",
                         "Load a moving point cloud, a stationary point cloud, and run the"
                         " deformable registration algorithm." }
                     };
    arger.description = "A program for running a deformable registration algorithm.";

    arger.default_callback = [](int, const std::string &optarg) -> void {
      FUNCERR("Unrecognized option with argument: '" << optarg << "'");
      return; 
    };
    arger.optionless_callback = [&](const std::string &optarg) -> void {
      FUNCERR("Unrecognized option with argument: '" << optarg << "'");
      return; 
    };

    arger.push_back( ygor_arg_handlr_t(1, 'm', "moving", true, "moving.txt",
      "Load a moving point cloud from the given file.",
      [&](const std::string &optarg) -> void {
        std::ifstream FI(optarg);
        if(!ReadPointSetFromXYZ(moving, FI) || moving.points.empty()){
          FUNCERR("Unable to parse moving point set file: '" << optarg << "'");
          exit(1);
        }
        return;
      })
    );
 
    arger.push_back( ygor_arg_handlr_t(1, 's', "stationary", true, "stationary.txt",
      "Load a stationary point cloud from the given file.",
      [&](const std::string &optarg) -> void {
        std::ifstream FI(optarg);
        if(!ReadPointSetFromXYZ(stationary, FI) || stationary.points.empty()){
          FUNCERR("Unable to parse stationary point set file: '" << optarg << "'");
          exit(1);
        }
        return;
      })
    );
    arger.push_back( ygor_arg_handlr_t(1, 't', "type", true, "nonrigid",
      "Use this coherent point drift algorithm. Options: rigid, affine, nonrigid",
      [&](const std::string &optarg) -> void {
        type = optarg;
        return;
      })
    );
    arger.push_back( ygor_arg_handlr_t(1, 'p', "ps", true, "transformed_point_set.txt",
      "Write transformed point set to given file.",
      [&](const std::string &optarg) -> void {
        xyz_outfile = optarg;
        return;
      })
    );
    arger.push_back( ygor_arg_handlr_t(1, 'o', "matrix", true, "transform.txt",
      "Write transform matrix to given file.",
      [&](const std::string &optarg) -> void {
        tf_outfile = optarg;
        return;
      })
    );
    arger.push_back( ygor_arg_handlr_t(1, 'd', "iteration_interval", true, "0",
      "Number of iterations between writing",
      [&](const std::string &optarg) -> void {
        iter_interval = std::stoi(optarg);
        return;
      })
    );
    arger.push_back( ygor_arg_handlr_t(1, 'v', "video", true, "False",
      "Boolean to represent whether to plot for a video",
      [&](const std::string &optarg) -> void {
        video = optarg;
        return;
      })
    );

        arger.push_back( ygor_arg_handlr_t(1, 'w', "wd", true, "0.2",
      "Weight of the uniform distribution. 0 <= w <= 1 (Optional, default w=0.2)",
      [&](const std::string &optarg) -> void {
        if (optarg.empty()) {
          std::string::size_type sz;
          params.distribution_weight = std::stof(optarg, &sz);
        }
        return;
      })
    );
    arger.push_back( ygor_arg_handlr_t(1, 'l', "lambda", true, "0.0",
      "Trade-off between the goodness of maximum likelihood "
      "fit and regularization. lambda > 0 (Optional, default l=2)",
      [&](const std::string &optarg) -> void {
        if (optarg.empty()) {
          std::string::size_type sz;
          params.lambda = std::stof(optarg, &sz);
        }
        return;
      })
    );
    arger.push_back( ygor_arg_handlr_t(1, 'b', "beta", true, "0.0",
      "Defines the model of the smoothness regularizer - width" 
      "of smoothing Gaussian filter. b>0 (Optional, default b=2)",
      [&](const std::string &optarg) -> void {
        if (optarg.empty()) {
          std::string::size_type sz;
          params.beta = std::stof(optarg, &sz);
        }
        return;
      })
    );
    arger.push_back( ygor_arg_handlr_t(1, 'i', "iterations", true, "100",
      "Maximum number of iterations for algorithm. (Optional, default i=100)",
      [&](const std::string &optarg) -> void {
        if (optarg.empty()) {
          params.iterations = std::stoi(optarg);
        }
        return;
      })
    );
    arger.push_back( ygor_arg_handlr_t(1, 'r', "threshold", true, "-15000",
      "Similarity threshold to terminate iteratiosn at.(Optional, default r=-15000)",
      [&](const std::string &optarg) -> void {
        if (optarg.empty()) {
          std::string::size_type sz;
          params.similarity_threshold = std::stof(optarg, &sz);
        }
        return;
      })
    );
    arger.Launch(argc, argv);

    //============================================= Input Validation ================================================
    if(moving.points.empty()){
        FUNCERR("Moving point set contains no points. Unable to continue.");
    }
    if(stationary.points.empty()){
        FUNCERR("Stationary point set contains no points. Unable to continue.");
    }

    //========================================== Launch Perfusion Model =============================================
    FUNCINFO(type)
    point_set<double> mutable_moving = moving;
    if(type == "rigid") {
        if(video == "True") {
          temp_xyz_outfile = xyz_outfile + "_iter0.xyz";
          std::ofstream PFO(temp_xyz_outfile);
          if(!WritePointSetToXYZ(mutable_moving, PFO))
            FUNCERR("Error writing point set to " << temp_xyz_outfile);
        }
        
        RigidCPDTransform transform = AlignViaRigidCPD(params, moving, stationary, iter_interval, video, xyz_outfile);
        transform.apply_to(mutable_moving);

        temp_xyz_outfile = xyz_outfile + "_last.xyz";
        std::ofstream PFO(temp_xyz_outfile);
        FUNCINFO("Writing transformed point set to " << (temp_xyz_outfile))
        if(!WritePointSetToXYZ(mutable_moving, PFO))
          FUNCERR("Error writing point set to " << temp_xyz_outfile);
        std::ofstream TFO(tf_outfile);
        FUNCINFO("Writing transform to " << tf_outfile)
        transform.write_to(TFO);
    } else if(type == "affine") {
        if(video == "True") {
          temp_xyz_outfile = xyz_outfile + "_iter0.xyz";
          std::ofstream PFO(temp_xyz_outfile);
          if(!WritePointSetToXYZ(mutable_moving, PFO))
            FUNCERR("Error writing point set to " << temp_xyz_outfile);
        }
        
        AffineCPDTransform transform = AlignViaAffineCPD(params, moving, stationary, iter_interval, video, xyz_outfile);
        transform.apply_to(mutable_moving);

        temp_xyz_outfile = xyz_outfile + "_last.xyz";
        std::ofstream PFO(temp_xyz_outfile);
        FUNCINFO("Writing transformed point set to " << (temp_xyz_outfile))
        if(!WritePointSetToXYZ(mutable_moving, PFO))
          FUNCERR("Error writing point set to " << temp_xyz_outfile);
        std::ofstream TFO(tf_outfile);
        FUNCINFO("Writing transform to " << tf_outfile)
        transform.write_to(TFO);
    } else if(type == "nonrigid") {
        if(video == "True") {
          temp_xyz_outfile = xyz_outfile + "_iter0.xyz";
          std::ofstream PFO(temp_xyz_outfile);
          if(!WritePointSetToXYZ(mutable_moving, PFO))
            FUNCERR("Error writing point set to " << temp_xyz_outfile);
        }
        
        NonRigidCPDTransform transform = AlignViaNonRigidCPD(params, moving, stationary, iter_interval, video, xyz_outfile);
        transform.apply_to(mutable_moving);
        
        temp_xyz_outfile = xyz_outfile + "_last.xyz";
        std::ofstream PFO(temp_xyz_outfile);
        FUNCINFO("Writing transformed point set to to " << (temp_xyz_outfile))
        if(!WritePointSetToXYZ(mutable_moving, PFO))
          FUNCERR("Error writing point set to " << temp_xyz_outfile);
        // TODO: Add in writing transform to file
        std::ofstream TFO(tf_outfile);
        FUNCINFO("Writing transform to " << tf_outfile)
        transform.write_to(TFO);
    } else {
        FUNCERR("The CPD algorithm specified was invalid. Options are rigid, affine, nonrigid");
        return 1;
    }

    return 0;
}
