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
                           " One option is currently available: 'vertices'."
                           ""
                           " The 'vertices' option extracts all vertices from all selected meshes and directly"
                           " inserts them into the new point cloud."
                           " Point clouds created this way will contain as many points as there are mesh vertices.";
    out.args.back().default_val = "vertices";
    out.args.back().expected = true;
    out.args.back().examples = { "vertices" };
    out.args.back().samples = OpArgSamples::Exhaustive;

    return out;
}



Drover ConvertMeshesToPoints(Drover DICOM_data,
                               const OperationArgPkg& OptArgs,
                               const std::map<std::string, std::string>& /*InvocationMetadata*/,
                               const std::string& FilenameLex){

    Explicator X(FilenameLex);

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto MeshSelectionStr = OptArgs.getValueStr("MeshSelection").value();

    const auto LabelStr = OptArgs.getValueStr("Label").value();

    const auto MethodStr = OptArgs.getValueStr("Method").value();
    //-----------------------------------------------------------------------------------------------------------------
    const auto regex_vertices = Compile_Regex("^ve?r?t?i?c?e?s?$");

    auto SMs_all = All_SMs( DICOM_data );
    auto SMs = Whitelist( SMs_all, MeshSelectionStr );
    const auto sm_count = SMs.size();
    FUNCINFO("Selected " << sm_count << " meshes");

    //Construct a destination for the point clouds.
    DICOM_data.point_data.emplace_back( std::make_unique<Point_Cloud>() );

    long int completed = 0;
    for(auto & smp_it : SMs){

        if( std::regex_match(MethodStr, regex_vertices) ){
            // Insert all vertices into the point cloud.
            for(auto &v : (*smp_it)->meshes.vertices){
                DICOM_data.point_data.back()->pset.points.emplace_back( v );
            }

        }else{
            throw std::invalid_argument("Method not understood. Cannot continue.");
        }

        ++completed;
        FUNCINFO("Completed " << completed << " of " << sm_count
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

    return DICOM_data;
}

