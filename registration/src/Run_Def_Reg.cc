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
    std::string tf_outfile;

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
    arger.push_back( ygor_arg_handlr_t(1, 'p', "ps", true, "transformed_pont_set.txt",
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
        RigidCPDTransform transform = AlignViaRigidCPD(params, moving, stationary);
        transform.apply_to(mutable_moving);
        std::ofstream PFO(xyz_outfile);
        FUNCINFO("Writing to " << xyz_outfile)
        if(!WritePointSetToXYZ(mutable_moving, PFO))
          FUNCERR("Error writing point set to " << xyz_outfile)
        std::ofstream TFO(tf_outfile);
        FUNCINFO("Writing to " << tf_outfile)
        if(!transform.write_to(TFO))
          FUNCERR("Error writing transform to " << tf_outfile)
    } else if(type == "affine") {
        AffineCPDTransform transform = AlignViaAffineCPD(params, moving, stationary);
        transform.apply_to(mutable_moving);
        std::ofstream PFO(xyz_outfile);
        FUNCINFO("Writing to " << xyz_outfile)
        if(!WritePointSetToXYZ(mutable_moving, PFO))
          FUNCERR("Error writing point set to " << xyz_outfile)
        std::ofstream TFO(tf_outfile);
        FUNCINFO("Writing to " << tf_outfile)
        if(!transform.write_to(TFO))
          FUNCERR("Error writing transform to " << tf_outfile)
    } else if(type == "nonrigid") {
        FUNCERR("This option has not been implemented yet.");
    } else {
        FUNCERR("The CPD algorithm specified was invalid. Options are rigid, affine, nonrigid");
        return 1;
    }

    return 0;
}
