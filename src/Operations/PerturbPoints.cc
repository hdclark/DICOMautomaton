// PerturbPoints.cc - A part of DICOMautomaton 2026. Written by hal clark.

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <optional>
#include <list>
#include <map>
#include <memory>
#include <regex>
#include <string>    
#include <random>
#include <cstdint>

#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorLog.h"

#include "../Structs.h"
#include "../Regex_Selectors.h"

#include "PerturbPoints.h"

OperationDoc OpArgDocPerturbPoints(){
    OperationDoc out;
    out.name = "PerturbPoints";

    out.tags.emplace_back("category: point cloud processing");

    out.desc = 
        "This operation pseudorandomly applies shifts to individual points in a point cloud."
        " The amount of random jitter applied to each point is confined within an axis-aligned cube"
        " centered on the point. The selection is deterministic when a seed is provided.";

    out.notes.emplace_back(
        "This operation modifies point clouds in-place by shifting points pseudorandomly."
    );

    out.args.emplace_back();
    out.args.back() = PCWhitelistOpArgDoc();
    out.args.back().name = "PointSelection";
    out.args.back().default_val = "last";

    out.args.emplace_back();
    out.args.back().name = "Width";
    out.args.back().desc = "The width of the axis-aligned cube within which"
                           " points can be randomly shifted. The cube is centred on each point."
                           " A value of 0.0 will not perturb any points."
                           " Larger values will cause points to be shifted further from their"
                           " original positions.";
    out.args.back().default_val = "1.0";
    out.args.back().expected = true;
    out.args.back().examples = { "0.0", "0.5", "1.0", "2.0", "5.0", "10.0" };

    out.args.emplace_back();
    out.args.back().name = "Seed";
    out.args.back().desc = "The random seed used for deterministic perturbation."
                           " Different seeds will produce different (but reproducible) perturbations."
                           " Negative values will generate a random seed, but note that the same seed will"
                           " be used for each selected point cloud.";
    out.args.back().default_val = "-1";
    out.args.back().expected = true;
    out.args.back().examples = { "-1", "0", "12345", "54321", "99999" };

    return out;
}


bool PerturbPoints(Drover &DICOM_data,
                   const OperationArgPkg& OptArgs,
                   std::map<std::string, std::string>& /*InvocationMetadata*/,
                   const std::string&){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto PointSelectionStr = OptArgs.getValueStr("PointSelection").value();
    const auto WidthStr = OptArgs.getValueStr("Width").value();
    auto SeedStr = OptArgs.getValueStr("Seed").value();

    //-----------------------------------------------------------------------------------------------------------------
    const auto Width = std::stod(WidthStr);
    if( !std::isfinite(Width) || (Width <= 0.0) ){
        throw std::invalid_argument("Width must be finite and greater than zero. Cannot continue.");
    }

    int64_t Seed = std::stol(SeedStr);
    if(Seed < 0){
        std::random_device rd;
        std::mt19937_64 gen(rd());
        std::uniform_int_distribution<int64_t> dist;
        Seed = dist(gen);
        SeedStr = std::to_string(Seed);
    }

    auto PCs_all = All_PCs( DICOM_data );
    auto PCs = Whitelist( PCs_all, PointSelectionStr );
    YLOGINFO("Selected " << PCs.size() << " point clouds");

    for(auto & pcp_it : PCs){
        const auto N_points = (*pcp_it)->pset.points.size();
        YLOGINFO("Processing a point cloud with " << N_points << " points");

        // Randomly perturb points.
        //
        // Note that we intentionally reset the pseudorandom generator afresh for each point cloud.
        // If each point cloud needs a different perturbation pattern, this routine can be called multiple times
        // (individually) with different seeds.
        std::mt19937 rng(Seed);
        const double half_width = Width / 2.0;
        std::uniform_real_distribution<double> dist(-half_width, half_width);

        // Apply random offset to each point within the axis-aligned cube.
        for(auto &point : (*pcp_it)->pset.points){
            const double dx = dist(rng);
            const double dy = dist(rng);
            const double dz = dist(rng);
            
            point.x += dx;
            point.y += dy;
            point.z += dz;
        }

        YLOGINFO("Perturbed " << N_points << " points with width " << Width);

        // Update metadata.
        (*pcp_it)->pset.metadata["Description"] = "Perturbed point cloud";
        (*pcp_it)->pset.metadata["PerturbationWidth"] = WidthStr;
        //(*pcp_it)->pset.metadata["PerturbationSeed"] = SeedStr;
        //(*pcp_it)->pset.metadata["PointCount"] = std::to_string(N_points);
    }

    return true;
}
