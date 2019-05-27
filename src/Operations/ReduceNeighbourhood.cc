//ReduceNeighbourhood.cc - A part of DICOMautomaton 2019. Written by hal clark.

#include <experimental/any>
#include <experimental/optional>
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
#include "ReduceNeighbourhood.h"
#include "YgorImages.h"
#include "YgorString.h"       //Needed for GetFirstRegex(...)
#include "YgorStats.h"       //Needed for Stats:: namespace.


OperationDoc OpArgDocReduceNeighbourhood(void){
    OperationDoc out;
    out.name = "ReduceNeighbourhood";

    out.desc = 
        "This routine walks the voxels of a 3D rectilinear image collection, "
        " reducing the distribution of voxels within the local volumetric neighbourhood to a scalar value,"
        " and updating the voxel value with this scalar. This routine can be used to implement mean and"
        " median filters (amongst others) that operate over a variety of 3D neighbourhoods."
        " Besides purely statistical reductions, logical reductions can be applied.";
    
    out.notes.emplace_back(
        "The provided image collection must be rectilinear."
    );
    
    out.args.emplace_back();
    out.args.back() = IAWhitelistOpArgDoc();
    out.args.back().name = "ImageSelection";
    out.args.back().default_val = "last";
    

    out.args.emplace_back();
    out.args.back().name = "NormalizedROILabelRegex";
    out.args.back().desc = "A regex matching ROI labels/names to consider. The default will match"
                      " all available ROIs. Be aware that input spaces are trimmed to a single space."
                      " If your ROI name has more than two sequential spaces, use regex to avoid them."
                      " All ROIs have to match the single regex, so use the 'or' token if needed."
                      " Regex is case insensitive and uses extended POSIX syntax.";
    out.args.back().default_val = ".*";
    out.args.back().expected = true;
    out.args.back().examples = { ".*", ".*Body.*", "Body", "Gross_Liver",
                            R"***(.*Left.*Parotid.*|.*Right.*Parotid.*|.*Eye.*)***",
                            R"***(Left Parotid|Right Parotid)***" };


    out.args.emplace_back();
    out.args.back().name = "ROILabelRegex";
    out.args.back().desc = "A regex matching ROI labels/names to consider. The default will match"
                      " all available ROIs. Be aware that input spaces are trimmed to a single space."
                      " If your ROI name has more than two sequential spaces, use regex to avoid them."
                      " All ROIs have to match the single regex, so use the 'or' token if needed."
                      " Regex is case insensitive and uses extended POSIX syntax.";
    out.args.back().default_val = ".*";
    out.args.back().expected = true;
    out.args.back().examples = { ".*", ".*body.*", "body", "Gross_Liver",
                            R"***(.*left.*parotid.*|.*right.*parotid.*|.*eyes.*)***",
                            R"***(left_parotid|right_parotid)***" };


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
    out.args.back().name = "Neighbourhood";
    out.args.back().desc = "Controls how the neighbourhood surrounding a voxel is defined."
                           " Currently, 'spherical' and 'cubic' are defined.";
    out.args.back().default_val = "spherical";
    out.args.back().expected = true;
    out.args.back().examples = { "spherical",
                                 "cubic"};


    out.args.emplace_back();
    out.args.back().name = "Reduction";
    out.args.back().desc = "Controls how the distribution of voxel values from neighbouring voxels is reduced."
                           " Statistical distribution reducers 'min', 'mean', 'median', and 'max' are defined."
                           " Logical reducers 'is_min' and 'is_max' are also available -- is_min (is_max)"
                           " replace the voxel value with 1.0 if it was the min (max) in the neighbourhood and"
                           " 0.0 otherwise. Logical reducers 'is_min_nan' and 'is_max_nan' are variants that"
                           " replace the voxel with a NaN instead of 1.0 and otherwise do not overwrite the"
                           " original voxel value. 'Conservative' refers to the so-called conservative filter"
                           " that suppresses isolated peaks; for every voxel considered, the voxel intensity"
                           " is clamped to the local neighbourhood's extrema. This filter works best for"
                           " removing spurious peak and trough voxels and performs no averaging.";
    out.args.back().default_val = "median";
    out.args.back().expected = true;
    out.args.back().examples = { "min",
                                 "mean", 
                                 "median",
                                 "max",
                                 "is_min",
                                 "is_max",
                                 "is_min_nan",
                                 "is_max_nan",
                                 "conservative" };


    out.args.emplace_back();
    out.args.back().name = "MaxDistance";
    out.args.back().desc = "The maximum distance (inclusive, in DICOM units: mm) within which neighbouring"
                           " voxels will be evaluated. For spherical neighbourhoods, this distance refers to the"
                           " radius. For cubic neighbourhoods, this distance refers to 'box radius' or the distance"
                           " from the cube centre to the nearest point on each bounding face."
                           " Voxels separated by more than this distance will not be evaluated together.";
    out.args.back().default_val = "2.0";
    out.args.back().expected = true;
    out.args.back().examples = { "0.5",
                                 "2.0",
                                 "15.0" };

    return out;
}

Drover ReduceNeighbourhood(Drover DICOM_data, OperationArgPkg OptArgs, std::map<std::string,std::string> /*InvocationMetadata*/, std::string /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();

    const auto NormalizedROILabelRegex = OptArgs.getValueStr("NormalizedROILabelRegex").value();
    const auto ROILabelRegex = OptArgs.getValueStr("ROILabelRegex").value();

    const auto Channel = std::stol( OptArgs.getValueStr("Channel").value() );

    const auto NeighbourhoodStr = OptArgs.getValueStr("Neighbourhood").value();
    const auto ReductionStr = OptArgs.getValueStr("Reduction").value();

    const auto MaxDistance = std::stod( OptArgs.getValueStr("MaxDistance").value() );


    //-----------------------------------------------------------------------------------------------------------------
    const auto regex_sph = Compile_Regex("^sp?h?e?r?i?c?a?l?$");
    const auto regex_cub = Compile_Regex("^cu?b?i?c?$");

    const auto regex_min    = Compile_Regex("^mini?m?u?m?$");
    const auto regex_median = Compile_Regex("^medi?a?n?$");
    const auto regex_mean   = Compile_Regex("^mean?$");
    const auto regex_max    = Compile_Regex("^maxi?m?u?m?$");

    const auto regex_is_min = Compile_Regex("^is?_?m?ini?m?u?m?$");
    const auto regex_is_max = Compile_Regex("^is?_?m?axi?m?u?m?$");
    const auto regex_is_min_nan = Compile_Regex("^is?_?m?ini?m?u?m?_?nan$");
    const auto regex_is_max_nan = Compile_Regex("^is?_?m?axi?m?u?m?_?nan$");

    const auto regex_conserv = Compile_Regex("^co?n?s?e?r?v?a?t?i?v?e?$");

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

        ComputeVolumetricNeighbourhoodSamplerUserData ud;
        ud.channel = Channel;
        ud.maximum_distance = MaxDistance;
        {
            std::stringstream oss;
            oss << "Neighbourhood-reduced (max-dist=" << ud.maximum_distance << ")";
            ud.description = oss.str();
        }

        if(false){
        }else if( std::regex_match(NeighbourhoodStr, regex_sph) ){
            ud.neighbourhood = ComputeVolumetricNeighbourhoodSamplerUserData::Neighbourhood::Spherical;
        }else if( std::regex_match(NeighbourhoodStr, regex_cub) ){
            ud.neighbourhood = ComputeVolumetricNeighbourhoodSamplerUserData::Neighbourhood::Cubic;
        }else{
            throw std::invalid_argument("Neighbourhood argument '"_s + NeighbourhoodStr + "' is not valid");
        }

        if(false){
        }else if( std::regex_match(ReductionStr, regex_min) ){
            ud.f_reduce = [](float, std::vector<float> &shtl, vec3<double>) -> float {
                              return Stats::Min(shtl);
                          };
        }else if( std::regex_match(ReductionStr, regex_median) ){
            ud.f_reduce = [](float, std::vector<float> &shtl, vec3<double>) -> float {
                              return Stats::Median(shtl);
                          };
        }else if( std::regex_match(ReductionStr, regex_mean) ){
            ud.f_reduce = [](float, std::vector<float> &shtl, vec3<double>) -> float {
                              return Stats::Mean(shtl);
                          };
        }else if( std::regex_match(ReductionStr, regex_max) ){
            ud.f_reduce = [](float, std::vector<float> &shtl, vec3<double>) -> float {
                              return Stats::Max(shtl);
                          };

        }else if( std::regex_match(ReductionStr, regex_is_min_nan) ){
            const auto machine_eps = std::sqrt( std::numeric_limits<float>::epsilon() );
            ud.f_reduce = [=](float v, std::vector<float> &shtl, vec3<double>) -> float {
                              const auto min = Stats::Min(shtl);
                              const auto diff = std::abs(v - min);
                              return (diff < machine_eps) ? std::numeric_limits<float>::quiet_NaN() : v;
                          };
        }else if( std::regex_match(ReductionStr, regex_is_max_nan) ){
            const auto machine_eps = std::sqrt( std::numeric_limits<float>::epsilon() );
            ud.f_reduce = [=](float v, std::vector<float> &shtl, vec3<double>) -> float {
                              const auto max = Stats::Max(shtl);
                              const auto diff = std::abs(v - max);
                              return (diff < machine_eps) ? std::numeric_limits<float>::quiet_NaN() : v;
                          };

        }else if( std::regex_match(ReductionStr, regex_is_min) ){
            const auto machine_eps = std::sqrt( std::numeric_limits<float>::epsilon() );
            ud.f_reduce = [=](float v, std::vector<float> &shtl, vec3<double>) -> float {
                              const auto min = Stats::Min(shtl);
                              const auto diff = std::abs(v - min);
                              return (diff < machine_eps) ? 1.0f : 0.0f;
                          };
        }else if( std::regex_match(ReductionStr, regex_is_max) ){
            const auto machine_eps = std::sqrt( std::numeric_limits<float>::epsilon() );
            ud.f_reduce = [=](float v, std::vector<float> &shtl, vec3<double>) -> float {
                              const auto max = Stats::Max(shtl);
                              const auto diff = std::abs(v - max);
                              return (diff < machine_eps) ? 1.0f : 0.0f;
                          };

        }else if( std::regex_match(ReductionStr, regex_conserv) ){
            ud.f_reduce = [](float v, std::vector<float> &shtl, vec3<double>) -> float {
                              if(shtl.size() < 3){
                                  throw std::runtime_error("Voxel neighbourhood comprises insufficient voxels for this filter. Cannot continue.");
                              }

                              //First, remove the voxel corresponding to this voxel, which will confound this filter.
                              std::sort(std::begin(shtl), std::end(shtl), [&](float A, float B) -> bool {
                                  return std::abs(A - v) > std::abs(B - v);
                              });
                              shtl.pop_back();

                              //Then clamp the voxel value.
                              std::sort(std::begin(shtl), std::end(shtl));
                              const auto clamped = std::clamp(v, shtl.front(), shtl.back());
                              return clamped;
                          };

        }else{
            throw std::invalid_argument("Reduction argument '"_s + ReductionStr + "' is not valid");
        }

        if(!(*iap_it)->imagecoll.Compute_Images( ComputeVolumetricNeighbourhoodSampler, 
                                                 {}, cc_ROIs, &ud )){
            throw std::runtime_error("Unable to reduce voxel neighbourhood.");
        }
    }

    return DICOM_data;
}
