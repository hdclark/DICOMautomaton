//SamplePoints.cc - A part of DICOMautomaton 2026. Written by hal clark.

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

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "SamplePoints.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorLog.h"


OperationDoc OpArgDocSamplePoints(){
    OperationDoc out;
    out.name = "SamplePoints";

    out.tags.emplace_back("category: point cloud processing");

    out.desc = 
        "This operation pseudorandomly selects a subset of points from the selected point clouds."
        " The selection is deterministic when a seed is provided.";

    out.notes.emplace_back(
        "This operation modifies point clouds in-place by removing points that are not selected."
    );

    out.args.emplace_back();
    out.args.back() = PCWhitelistOpArgDoc();
    out.args.back().name = "PointSelection";
    out.args.back().default_val = "last";

    out.args.emplace_back();
    out.args.back().name = "Fraction";
    out.args.back().desc = "The fraction of points to retain, in the range [0, 1]."
                           " A value of 0.5 will retain approximately half of the points."
                           " A value of 1.0 will retain all points (no sampling)."
                           " A value of 0.0 will remove all points.";
    out.args.back().default_val = "0.5";
    out.args.back().expected = true;
    out.args.back().examples = { "0.1", "0.25", "0.5", "0.75", "1.0" };

    out.args.emplace_back();
    out.args.back().name = "Seed";
    out.args.back().desc = "The random seed used for deterministic sampling."
                           " Different seeds will produce different (but reproducible) selections.";
    out.args.back().default_val = "12345";
    out.args.back().expected = true;
    out.args.back().examples = { "0", "12345", "54321", "99999" };

    return out;
}


bool SamplePoints(Drover &DICOM_data,
                  const OperationArgPkg& OptArgs,
                  std::map<std::string, std::string>& /*InvocationMetadata*/,
                  const std::string&){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto PointSelectionStr = OptArgs.getValueStr("PointSelection").value();
    const auto FractionStr = OptArgs.getValueStr("Fraction").value();
    const auto SeedStr = OptArgs.getValueStr("Seed").value();

    //-----------------------------------------------------------------------------------------------------------------
    const auto Fraction = std::stod(FractionStr);
    if(!std::isfinite(Fraction) || (Fraction < 0.0) || (1.0 < Fraction)){
        throw std::invalid_argument("Fraction must be in the range [0, 1]. Cannot continue.");
    }

    const auto Seed = std::stoul(SeedStr);

    auto PCs_all = All_PCs( DICOM_data );
    auto PCs = Whitelist( PCs_all, PointSelectionStr );
    YLOGINFO("Selected " << PCs.size() << " point clouds");

    for(auto & pcp_it : PCs){
        const auto N_original = (*pcp_it)->pset.points.size();
        YLOGINFO("Processing a point cloud with " << N_original << " points");

        // Early exit if no sampling is needed.
        if(Fraction >= 1.0){
            YLOGINFO("Fraction is >= 1.0, retaining all points");
            continue;
        }
        if(Fraction <= 0.0){
            YLOGINFO("Fraction is <= 0.0, removing all points");
            (*pcp_it)->pset.points.clear();
            continue;
        }

        // Randomly sample points.
        std::mt19937 rng(Seed);
        std::uniform_real_distribution<double> dist(0.0, 1.0);

        // Filter points: keep point if random value < Fraction.
        auto new_end = std::remove_if((*pcp_it)->pset.points.begin(),
                                      (*pcp_it)->pset.points.end(),
                                      [&](const vec3<double>&) -> bool {
                                          return (dist(rng) >= Fraction);
                                      });
        (*pcp_it)->pset.points.erase(new_end, (*pcp_it)->pset.points.end());

        const auto N_sampled = (*pcp_it)->pset.points.size();
        YLOGINFO("Retained " << N_sampled << " of " << N_original << " points"
                 << " (expected ~" << static_cast<int64_t>(N_original * Fraction) << ")");

        // Update metadata.
        (*pcp_it)->pset.metadata["Description"] = "Sampled point cloud";
        (*pcp_it)->pset.metadata["SamplingFraction"] = FractionStr;
        (*pcp_it)->pset.metadata["SamplingSeed"] = SeedStr;
        (*pcp_it)->pset.metadata["OriginalPointCount"] = std::to_string(N_original);
        (*pcp_it)->pset.metadata["SampledPointCount"] = std::to_string(N_sampled);
    }

    return true;
}
