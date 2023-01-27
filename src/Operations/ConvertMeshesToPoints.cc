//ConvertMeshesToPoints.cc - A part of DICOMautomaton 2020. Written by hal clark.

#include <asio.hpp>
#include <algorithm>
#include <optional>
#include <fstream>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <regex>
#include <set> 
#include <stdexcept>
#include <string>    
#include <utility>            //Needed for std::pair.
#include <vector>

#include "Explicator.h"       //Needed for Explicator class.

#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorLog.h"
#include "YgorStats.h"        //Needed for Stats:: namespace.
#include "YgorString.h"       //Needed for GetFirstRegex(...)
#include "YgorMathIOOFF.h"

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../Thread_Pool.h"

#include "ConvertMeshesToPoints.h"


OperationDoc OpArgDocConvertMeshesToPoints(){
    OperationDoc out;
    out.name = "ConvertMeshesToPoints";

    out.desc = 
        "This operation converts meshes to point clouds.";
        
    out.notes.emplace_back(
        "Meshes are unaltered. Existing point clouds are ignored and unaltered."
    );


    out.args.emplace_back();
    out.args.back() = SMWhitelistOpArgDoc();
    out.args.back().name = "MeshSelection";
    out.args.back().default_val = "last";


    out.args.emplace_back();
    out.args.back().name = "Label";
    out.args.back().desc = "A label to attach to the point cloud.";
    out.args.back().default_val = "unspecified";
    out.args.back().expected = true;
    out.args.back().examples = { "unspecified", "POIs", "peaks", "above_zero", "below_5.3" };


    out.args.emplace_back();
    out.args.back().name = "Method";
    out.args.back().desc = "The conversion method to use."
                           " Two options are currently available: 'vertices' and 'random'."
                           ""
                           " The 'vertices' option extracts all vertices from all selected meshes and directly"
                           " inserts them into the new point cloud."
                           " Point clouds created this way will contain as many points as there are mesh vertices."
                           ""
                           " The 'random' option samples the surface mesh uniformly."
                           " The likelihood of specific a face being sampled is proportional to its area."
                           " This method requires a target sample density, which determines the number of samples"
                           " taken; this density is an average over the entire mesh surface area, and individual"
                           " samples may have less or more separation from neighbouring samples."
                           " Note that the 'random' method will tend to result in clusters of samples and pockets"
                           " without samples. This is unavoidable when sampling randomly."
                           " The 'random' method accepts two parameters: a pseudo-random number generator seed and"
                           " the desired sample density."
                           "";
    out.args.back().default_val = "vertices";
    out.args.back().expected = true;
    out.args.back().examples = { "vertices", "random" };
    out.args.back().samples = OpArgSamples::Exhaustive;


    out.args.emplace_back();
    out.args.back().name = "RandomSeed";
    out.args.back().desc = "A parameter for the 'random' method:"
                           " the seed used for the random surface sampling method.";
    out.args.back().default_val = "1595813";
    out.args.back().expected = true;
    out.args.back().examples = { "25633", "20771", "271", "1006003", "11", "3511" };


    out.args.emplace_back();
    out.args.back().name = "RandomSampleDensity";
    out.args.back().desc = "A parameter for the 'random' method:"
                           " the target sample density (as samples/area where area is in DICOM units,"
                           " nominally $mm^{-2}$))."
                           " This parameter effectively controls the total number of samples."
                           " Note that the sample density is averaged over the entire surface, so individual"
                           " samples may cluster or spread out and develop pockets.";
    out.args.back().default_val = "1.0";
    out.args.back().expected = true;
    out.args.back().examples = { "0.1", "0.5", "1.0", "5.0", "10.0" };

    return out;
}



bool ConvertMeshesToPoints(Drover &DICOM_data,
                               const OperationArgPkg& OptArgs,
                               std::map<std::string, std::string>& /*InvocationMetadata*/,
                               const std::string& FilenameLex){

    Explicator X(FilenameLex);

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto MeshSelectionStr = OptArgs.getValueStr("MeshSelection").value();

    const auto LabelStr = OptArgs.getValueStr("Label").value();

    const auto MethodStr = OptArgs.getValueStr("Method").value();
    const auto RandomSeed = std::stol( OptArgs.getValueStr("RandomSeed").value() );
    const auto RandomSampleDensity = std::stod( OptArgs.getValueStr("RandomSampleDensity").value() );
    //-----------------------------------------------------------------------------------------------------------------
    const auto regex_vertices = Compile_Regex("^ve?r?t?i?c?e?s?$");
    const auto regex_random = Compile_Regex("^ra?n?d?o?m?$");

    auto SMs_all = All_SMs( DICOM_data );
    auto SMs = Whitelist( SMs_all, MeshSelectionStr );
    const auto sm_count = SMs.size();
    YLOGINFO("Selected " << sm_count << " meshes");

    //Construct a destination for the point clouds.
    DICOM_data.point_data.emplace_back( std::make_unique<Point_Cloud>() );

    long int completed = 0;
    for(auto & smp_it : SMs){

        if( std::regex_match(MethodStr, regex_vertices) ){
            // Insert all vertices into the point cloud.
            for(auto &v : (*smp_it)->meshes.vertices){
                DICOM_data.point_data.back()->pset.points.emplace_back( v );
            }

        }else if( std::regex_match(MethodStr, regex_random) ){
            // Sample the surface and combine into the point cloud.
            auto ps = (*smp_it)->meshes.sample_surface_randomly(RandomSampleDensity, RandomSeed);
            DICOM_data.point_data.back()->pset.points.insert(
                std::end(DICOM_data.point_data.back()->pset.points),
                std::begin(ps.points),
                std::end(ps.points) );

        }else{
            throw std::invalid_argument("Method not understood. Cannot continue.");
        }

        ++completed;
        YLOGINFO("Completed " << completed << " of " << sm_count
              << " --> " << static_cast<int>(1000.0*(completed)/sm_count)/10.0 << "% done");
    }

    //// Determine the common set of mesh metadata and assign it to the point data.
    //{
    //    // ... TODO ...
    //    DICOM_data.point_data.back()->pset.metadata = cm;
    //}

    DICOM_data.point_data.back()->pset.metadata["Label"] = LabelStr;
    DICOM_data.point_data.back()->pset.metadata["NormalizedLabel"] = X(LabelStr);
    DICOM_data.point_data.back()->pset.metadata["Description"] = "Point cloud derived from surface meshes.";

    return true;
}

