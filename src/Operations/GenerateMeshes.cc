//GenerateMeshes.cc - A part of DICOMautomaton 2021. Written by hal clark.

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

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../Thread_Pool.h"
#include "../String_Parsing.h"
#include "../Metadata.h"
#include "../Surface_Meshes.h"
#include "../CSG_SDF.h"
#include "GenerateMeshes.h"


OperationDoc OpArgDocGenerateMeshes(){
    OperationDoc out;
    out.name = "GenerateMeshes";

    out.desc = 
        "This operation contructs surface meshes using constructive solid geometry (CSG) with"
        " signed distance functions (SDFs).";

    out.args.emplace_back();
    out.args.back().name = "Objects";
    out.args.back().desc = "This parameter is used to specify a hierarchial tree of CSG-SDF objects."
                           " It can include shape primitives and operations over these shapes.";
    out.args.back().default_val = "";
    out.args.back().expected = true;
    out.args.back().examples = { "sphere(2.34);",
                                 "aa_box(1.0, 2.0, 3.0);",
                                 "join(){ sphere(1.5); aa_box(1.0, 2.0, 3.0); }" };

    out.args.emplace_back();
    out.args.back().name = "MeshLabel";
    out.args.back().desc = "A label to attach to the surface mesh.";
    out.args.back().default_val = "unspecified";
    out.args.back().expected = true;
    out.args.back().examples = { "unspecified", "shape", "sphere and box" };

    out.args.emplace_back();
    out.args.back().name = "Resolution";
    out.args.back().desc = "The (minimal) spatial resolution to apply along x, y, and z axes."
                           " Can be specified as a list of three numbers.";
    out.args.back().default_val = "1.0, 1.0, 1.0";
    out.args.back().expected = true;
    out.args.back().examples = { "1.0, 1.0, 1.0", "0.1, 0.1, 1.0", "0.12, 3.45, 6.78" };

    return out;
}

bool GenerateMeshes(Drover &DICOM_data,
                    const OperationArgPkg& OptArgs,
                    std::map<std::string, std::string>& /*InvocationMetadata*/,
                    const std::string& FilenameLex){

    Explicator X(FilenameLex);

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ObjectsStr = OptArgs.getValueStr("Objects").value();
    const auto MeshLabel = OptArgs.getValueStr("MeshLabel").value();
    const auto ResolutionStr = OptArgs.getValueStr("Resolution").value();

    //-----------------------------------------------------------------------------------------------------------------
    const auto NormalizedMeshLabel = X(MeshLabel);
    const auto pfs = parse_functions(ObjectsStr);
    if(pfs.size() != 1){
        throw std::invalid_argument("Exactly one root object is required (e.g., 'join')");
    }
    std::shared_ptr<csg::sdf::node> root = csg::sdf::build_node(pfs.front());
    if(root == nullptr){
        throw std::logic_error("Failed to build CSG-SDF tree");
    }

    const auto min_res_vec = parse_numbers(",()", ResolutionStr);
    if(min_res_vec.size() != 3){
        throw std::invalid_argument("Minimum resolutions are required for x, y, and z axes");
    }
    const vec3<double> min_res( min_res_vec.at(0), min_res_vec.at(1), min_res_vec.at(2) );

    // Ensure the BB and SDF can be evaluated without throwing.
    auto bb = root->evaluate_aa_bbox();
    YLOGINFO("axis-aligned bounding box min: " << bb.min);
    YLOGINFO("axis-aligned bounding box max: " << bb.max);
    const auto sdf_at_origin = root->evaluate_sdf( vec3<double>(0,0,0) );
    YLOGINFO("sdf at origin: " << sdf_at_origin );

    dcma_surface_meshes::Parameters meshing_params;
    const double inclusion_threshold = 0.0;
    const bool below_is_interior = true;
    auto fv_mesh = dcma_surface_meshes::Estimate_Surface_Mesh_Marching_Cubes( root,
                                                                              min_res,
                                                                              inclusion_threshold,
                                                                              below_is_interior,
                                                                              meshing_params);

    // Inject the result into the Drover.
    auto mesh_meta = coalesce_metadata_for_basic_mesh({}, meta_evolve::iterate);
    DICOM_data.smesh_data.emplace_back( std::make_unique<Surface_Mesh>() );
    DICOM_data.smesh_data.back()->meshes = fv_mesh;
    DICOM_data.smesh_data.back()->meshes.metadata = mesh_meta;
    DICOM_data.smesh_data.back()->meshes.metadata["MeshLabel"] = MeshLabel;
    DICOM_data.smesh_data.back()->meshes.metadata["NormalizedMeshLabel"] = NormalizedMeshLabel;
    DICOM_data.smesh_data.back()->meshes.metadata["Description"] = "Generated surface mesh";

    //// Insert user-specified metadata.
    ////
    //// Note: This must occur last so it overwrites incumbent metadata entries.
    //for(const auto &kvp : Metadata){
    //    DICOM_data.smesh_data.back()->metadata[kvp.first] = kvp.second;
    //}
    //
    // ... TODO ...

    return true;
}
