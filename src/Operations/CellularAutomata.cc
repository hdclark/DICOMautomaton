//CellularAutomata.cc - A part of DICOMautomaton 2021. Written by hal clark.

#include <any>
#include <optional>
#include <functional>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>    

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../YgorImages_Functors/ConvenienceRoutines.h"
#include "../YgorImages_Functors/Grouping/Misc_Functors.h"
#include "../YgorImages_Functors/Compute/Volumetric_Neighbourhood_Sampler.h"
#include "CellularAutomata.h"
#include "YgorImages.h"
#include "YgorString.h"       //Needed for GetFirstRegex(...)
#include "YgorStats.h"       //Needed for Stats:: namespace.


OperationDoc OpArgDocCellularAutomata(){
    OperationDoc out;
    out.name = "CellularAutomata";

    out.desc = 
        "This operation implements 2D cellular automata (Conway's Game of Life) with periodic boundary"
        " conditions.";
    
    out.notes.emplace_back(
        "The provided image collection must be rectilinear. All images will be modeled independently"
        " of one another."
    );
    
    out.args.emplace_back();
    out.args.back() = IAWhitelistOpArgDoc();
    out.args.back().name = "ImageSelection";
    out.args.back().default_val = "last";
    

    out.args.emplace_back();
    out.args.back() = NCWhitelistOpArgDoc();
    out.args.back().name = "NormalizedROILabelRegex";
    out.args.back().default_val = ".*";


    out.args.emplace_back();
    out.args.back() = RCWhitelistOpArgDoc();
    out.args.back().name = "ROILabelRegex";
    out.args.back().default_val = ".*";


    out.args.emplace_back();
    out.args.back().name = "Channel";
    out.args.back().desc = "The channel to operated on (zero-based)."
                           " Negative values will cause all channels to be operated on.";
    out.args.back().default_val = "0";
    out.args.back().expected = true;
    out.args.back().examples = { "-1",
                                 "0",
                                 "1" };

    out.args.emplace_back();
    out.args.back().name = "Iterations";
    out.args.back().desc = "The number of iterations to simulate."
                           " Note that intermediary iterations are not retained.";
    out.args.back().default_val = "1";
    out.args.back().expected = true;
    out.args.back().examples = { "1",
                                 "10",
                                 "1000" };

    out.args.emplace_back();
    out.args.back().name = "Low";
    out.args.back().desc = "The voxel value that represents 'dead' cells. Since cells are either 'alive' or 'dead', the"
                           " value halfway between the low and high values is used as the threshold.";
    out.args.back().default_val = "0.0";
    out.args.back().expected = true;
    out.args.back().examples = { "0.0",
                                 "-1.23",
                                 "10.0" };

    out.args.emplace_back();
    out.args.back().name = "High";
    out.args.back().desc = "The voxel value that represents 'alive' cells. Since cells are either 'alive' or 'dead', the"
                           " value halfway between the low and high values is used as the threshold.";
    out.args.back().default_val = "1.0";
    out.args.back().expected = true;
    out.args.back().examples = { "1.5",
                                 "-0.23",
                                 "255.0" };

    return out;
}

Drover CellularAutomata(Drover DICOM_data,
                        const OperationArgPkg& OptArgs,
                        const std::map<std::string, std::string>& /*InvocationMetadata*/,
                        const std::string& /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();

    const auto NormalizedROILabelRegex = OptArgs.getValueStr("NormalizedROILabelRegex").value();
    const auto ROILabelRegex = OptArgs.getValueStr("ROILabelRegex").value();

    const auto Channel = std::stol( OptArgs.getValueStr("Channel").value() );

    const auto Iterations = std::stol( OptArgs.getValueStr("Iterations").value() );

    const auto Low = std::stod( OptArgs.getValueStr("Low").value() );
    const auto High = std::stod( OptArgs.getValueStr("High").value() );

    //-----------------------------------------------------------------------------------------------------------------
    if(!isininc(0,Iterations,10'000'000)){
        throw std::invalid_argument("Invalid iteration count. Refusing to continue");
    }

    const auto Threshold = (High * 0.5) + (Low * 0.5);

    //-----------------------------------------------------------------------------------------------------------------

    //Stuff references to all contours into a list. Remember that you can still address specific contours through
    // the original holding containers (which are not modified here).
    auto cc_all = All_CCs( DICOM_data );
    auto cc_ROIs = Whitelist( cc_all, { { "ROIName", ROILabelRegex },
                                        { "NormalizedROIName", NormalizedROILabelRegex } } );
    if(cc_ROIs.empty()){
        throw std::invalid_argument("No contours selected. Cannot continue.");
    }


    auto IAs_all = All_IAs( DICOM_data );
    auto IAs = Whitelist( IAs_all, ImageSelectionStr );
    for(auto & iap_it : IAs){
        if( (*iap_it)->imagecoll.images.empty() ) continue;

        ComputeVolumetricNeighbourhoodSamplerUserData ud;
        ud.channel = Channel;
        ud.neighbourhood = ComputeVolumetricNeighbourhoodSamplerUserData::Neighbourhood::SelectionPeriodic;
        ud.maximum_distance = std::numeric_limits<double>::quiet_NaN();
        ud.description = "2D Conway's Game of Life";
        ud.voxel_triplets = { { 
            // The 2D "Moore" neighbourhood, i.e., includes both nearest and next-nearest neighbours (diagonals).
            std::array<long int, 3>{ -1, -1,  0 },
            std::array<long int, 3>{ -1,  0,  0 },
            std::array<long int, 3>{ -1,  1,  0 },

            std::array<long int, 3>{  0, -1,  0 },
            std::array<long int, 3>{  0,  1,  0 },

            std::array<long int, 3>{  1, -1,  0 },
            std::array<long int, 3>{  1,  0,  0 },
            std::array<long int, 3>{  1,  1,  0 },
        } };

        ud.f_reduce = [Low, High, Threshold](float v, std::vector<float> &shtl, vec3<double>) -> float {
            long int alive = 0;
            long int dead = 0;
            for(const auto& nv : shtl){
                if(!std::isfinite(nv)){
                    throw std::runtime_error("Encountered non-finite cell. Refusing to continue");
                }
                if(nv < Threshold){
                    ++dead;
                }else{
                    ++alive;
                }
            }

            const bool v_alive = !(v < Threshold);
            if(v_alive){
                if(isininc(2,alive,3)){
                    v = High;
                }else{
                    v = Low;
                }

            }else{
                v = Low;
                if(alive == 3) v = High;
            }

            return v;
        };

        for(long int i = 0; i < Iterations; ++i){
            if(!(*iap_it)->imagecoll.Compute_Images( ComputeVolumetricNeighbourhoodSampler, 
                                                     {}, cc_ROIs, &ud )){
                throw std::runtime_error("Unable to perform Game of Life iteration.");
            }
        }
    }

    return DICOM_data;
}
