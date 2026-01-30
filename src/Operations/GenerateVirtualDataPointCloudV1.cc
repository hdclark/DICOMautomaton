// GenerateVirtualDataPointCloudV1.cc - A part of DICOMautomaton 2026. Written by hal clark.

#include <cmath>
#include <list>
#include <map>
#include <memory>
#include <string>    
#include <cstdint>
#include <random>

#include "YgorMath.h"         //Needed for vec3 and point_set classes.
#include "YgorString.h"       //Needed for GetFirstRegex(...)

#include "../Structs.h"
#include "../Metadata.h"

#include "GenerateVirtualDataPointCloudV1.h"


OperationDoc OpArgDocGenerateVirtualDataPointCloudV1(){
    OperationDoc out;
    out.name = "GenerateVirtualDataPointCloudV1";

    out.tags.emplace_back("category: point cloud processing");
    out.tags.emplace_back("category: generator");
    out.tags.emplace_back("category: virtual phantom");

    out.desc = 
        "This operation generates a deterministic synthetic point cloud with 100 points randomly sampled"
        " from an axis-aligned cube centered at (0,0,0) with width 100.0."
        " It can be used for testing how point cloud data is transformed or processed.";

    return out;
}

bool GenerateVirtualDataPointCloudV1(Drover &DICOM_data,
                                        const OperationArgPkg&,
                                        std::map<std::string, std::string>& /*InvocationMetadata*/,
                                        const std::string& /*FilenameLex*/){

    // Parameters fixed for V1 virtual phantom.
    const int64_t num_points = 100;
    const double cube_width = 100.0;
    const double half_width = cube_width / 2.0;
    const uint32_t rng_seed = 12345;  // Deterministic seed for reproducibility.

    auto pc_ptr = std::make_shared<Point_Cloud>();

    // Assign baseline metadata.
    pc_ptr->pset.metadata["PatientID"] = "VirtualDataPointCloudVersion1";
    pc_ptr->pset.metadata["PointLabel"] = "SyntheticCubeSample";
    pc_ptr->pset.metadata["Description"] = "Synthetic point cloud sampled from cube";
    pc_ptr->pset.metadata["ContentDate"] = "20260128";
    pc_ptr->pset.metadata["ContentTime"] = "193430";
    pc_ptr->pset.metadata["OriginFilename"] = "/dev/null";

    pc_ptr->pset.metadata = coalesce_metadata_for_basic_pset(pc_ptr->pset.metadata);

    pc_ptr->pset.metadata["CubeCenterX"] = "0.0";
    pc_ptr->pset.metadata["CubeCenterY"] = "0.0";
    pc_ptr->pset.metadata["CubeCenterZ"] = "0.0";
    pc_ptr->pset.metadata["CubeWidth"] = std::to_string(cube_width);
    pc_ptr->pset.metadata["NumberOfPoints"] = std::to_string(num_points);
    //pc_ptr->pset.metadata["RandomSeed"] = std::to_string(rng_seed);

    // Generate random points within the cube.
    std::mt19937 rng(rng_seed);
    std::uniform_real_distribution<double> dist(-half_width, half_width);

    for(int64_t i = 0; i < num_points; ++i){
        const double x = dist(rng);
        const double y = dist(rng);
        const double z = dist(rng);
        
        pc_ptr->pset.points.emplace_back(x, y, z);
    }

    DICOM_data.point_data.emplace_back(pc_ptr);

    return true;
}
