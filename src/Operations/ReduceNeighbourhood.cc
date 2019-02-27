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
        "This routine walks the voxels of a 3D rectilinear image collection, invoking a user-provided"
        " functor to reduce the distribution of voxels within the local neighbourhood to a scalar value,"
        " and updating the voxel value with this scalar. This routine can be used to implement mean and"
        " median filters (amongst others) that operate over a variety of 3D neighbourhoods.";
    
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
    out.args.back().default_val = "Spherical";
    out.args.back().expected = true;
    out.args.back().examples = { "spherical",
                                 "cubic"};


    out.args.emplace_back();
    out.args.back().name = "Reduction";
    out.args.back().desc = "Controls how the distribution of voxel values from neighbouring voxels is reduced."
                           " Currently, 'min', 'mean', 'median', and 'max' are defined.";
    out.args.back().default_val = "Median";
    out.args.back().expected = true;
    out.args.back().examples = { "min",
                                 "mean", 
                                 "median",
                                 "max" };


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
            ud.f_reduce = [](std::vector<double> &shtl) -> double {
                              return Stats::Min(shtl);
                          };
        }else if( std::regex_match(ReductionStr, regex_median) ){
            ud.f_reduce = [](std::vector<double> &shtl) -> double {
                              return Stats::Median(shtl);
                          };
        }else if( std::regex_match(ReductionStr, regex_mean) ){
            ud.f_reduce = [](std::vector<double> &shtl) -> double {
                              return Stats::Mean(shtl);
                          };
        }else if( std::regex_match(ReductionStr, regex_max) ){
            ud.f_reduce = [](std::vector<double> &shtl) -> double {
                              return Stats::Max(shtl);
                          };
        }else{
            throw std::invalid_argument("Reduction argument '"_s + ReductionStr + "' is not valid");
        }

        if(!(*iap_it)->imagecoll.Compute_Images( ComputeVolumetricNeighbourhoodSampler, 
                                                 {}, cc_ROIs, &ud )){
            throw std::runtime_error("Unable to reduce voxel neighbourhood.");
        }

        for(auto &img : (*iap_it)->imagecoll.images){
            std::stringstream oss;
            oss << "Neighbourhood-reduced (max-dist=" << ud.maximum_distance << ")";
            UpdateImageDescription( std::ref(img), oss.str() );
            UpdateImageWindowCentreWidth( std::ref(img) );
        }

    }

    return DICOM_data;
}
