//IsolatedVoxelFilter.cc - A part of DICOMautomaton 2019. Written by hal clark.

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
#include "IsolatedVoxelFilter.h"
#include "YgorImages.h"
#include "YgorString.h"       //Needed for GetFirstRegex(...)
#include "YgorStats.h"       //Needed for Stats:: namespace.


OperationDoc OpArgDocIsolatedVoxelFilter(void){
    OperationDoc out;
    out.name = "IsolatedVoxelFilter";

    out.desc = 
        "This routine applies a filter that discriminates between well-connected and isolated vertices."
        " Isolated vertices can either be filtered out or retained."
        " This operation considers the 3D neighbourhood surrounding a voxel.";
    
    out.notes.emplace_back(
        "The provided image collection must be rectilinear."
    );
    out.notes.emplace_back(
        "If the neighbourhood involves voxels that do not exist, they are treated as NaNs in the same"
        " way that voxels with the NaN value are treated."
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
    out.args.back().name = "Replacement";
    out.args.back().desc = "Controls how replacements are generated."
                           " Currently, 'mean' and 'median' are defined."
                           " These replacement strategies replace the voxel value with the mean and median,"
                           " respectively, from the surrounding neighbourhood."
                           " A numeric value can also be supplied, which will replace all voxels.";
    out.args.back().default_val = "mean";
    out.args.back().expected = true;
    out.args.back().examples = { "mean",
                                 "median", 
                                 "0.0",
                                 "-1.23",
                                 "1E6",
                                 "nan" };


    out.args.emplace_back();
    out.args.back().name = "Replace";
    out.args.back().desc = "Controls whether isolated or well-connected voxels are retained.";
    out.args.back().default_val = "isolated";
    out.args.back().expected = true;
    out.args.back().examples = { "isolated",
                                 "well-connected" };


    out.args.emplace_back();
    out.args.back().name = "NeighbourCount";
    out.args.back().desc = "Controls the number of neighbours being considered."
                           " For purposes of speed, this option is limited to specific levels of neighbour adjacency.";
    out.args.back().default_val = "isolated";
    out.args.back().expected = true;
    out.args.back().examples = { "1",
                                 "2",
                                 "3"};
                                 // TODO: Need a more general neighbour specification method...


    out.args.emplace_back();
    out.args.back().name = "AgreementCount";
    out.args.back().desc = "Controls the number of neighbours that must be in agreement for a voxel to be considered"
                           " 'well-connected.'";
    out.args.back().default_val = "6";
    out.args.back().expected = true;
    out.args.back().examples = { "1",
                                 "2",
                                 "25"};


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

Drover IsolatedVoxelFilter(Drover DICOM_data, OperationArgPkg OptArgs, std::map<std::string,std::string> /*InvocationMetadata*/, std::string /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();

    const auto NormalizedROILabelRegex = OptArgs.getValueStr("NormalizedROILabelRegex").value();
    const auto ROILabelRegex = OptArgs.getValueStr("ROILabelRegex").value();

    const auto Channel = std::stol( OptArgs.getValueStr("Channel").value() );

    const auto ReplacementStr = OptArgs.getValueStr("Replacement").value();
    const auto ReplaceStr = OptArgs.getValueStr("Replace").value();

    const auto MaxDistance = std::stod( OptArgs.getValueStr("MaxDistance").value() );

    //-----------------------------------------------------------------------------------------------------------------
    const auto regex_mean = Compile_Regex("^mea?n?$");
    const auto regex_median = Compile_Regex("^medi?a?n?$");

    bool replacement_is_mean   = false;
    bool replacement_is_median = false;
    bool replacement_is_value  = false;
    double replacement_value = std::numeric_limits<double>::quiet_NaN();

    try{
        replacement_value = std::stod(ReplacementStr);
        replacement_is_value = true;
    }catch(const std::exception &){ }
    if(replacement_is_value){
    }else if( std::regex_match(ReplacementStr, regex_mean) ){
        replacement_is_mean = true;
    }else if( std::regex_match(ReplacementStr, regex_median) ){
        replacement_is_median = true;
    }else{
        throw std::invalid_argument("'Replacement' parameter is invalid. Cannot continue.");
    }

    const auto regex_iso = Compile_Regex("^is?o?l?a?t?e?d?$");
    const auto regex_well = Compile_Regex("^we?l?l?-?c?o?n?n?e?c?t?e?d?$");

    bool replace_is_iso = false;
    bool replace_is_well = false;

    if(false){
    }else if( std::regex_match(ReplaceStr, regex_iso) ){
        replace_is_iso = true;
    }else if( std::regex_match(ReplaceStr, regex_median) ){
        replace_is_well = true;
    }else{
        throw std::invalid_argument("'Replace' parameter is invalid. Cannot continue.");
    }

    const auto machine_eps = std::sqrt( std::numeric_limits<float>::epsilon() );


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
            oss << "Isolated voxel filtered"; 
            ud.description = oss.str();
        }

        ud.neighbourhood = ComputeVolumetricNeighbourhoodSamplerUserData::Neighbourhood::Selection;

        ud.voxel_triplets.empty();
        //using udvt = decltype(ud.voxel_triplets);

        if(false){
            ud.voxel_triplets = {{ 
                  {  0,  0,  0 }
            }};
        }

        if(false){
            ud.voxel_triplets = {{ 
                  {  0,  0, -1 }, 
                  { -1,  0,  0 }, 
                  {  0, -1,  0 }, 
                  {  0,  1,  0 }, 
                  {  1,  0,  0 }, 
                                  
                  {  0,  0,  1 }
            }};
        }

        if(false){
            ud.voxel_triplets = {{ 
                  { -1,  0, -1 },
                  {  0, -1, -1 },
                  {  0,  1, -1 },
                  {  1,  0, -1 },
                                 
                  { -1, -1,  0 },
                  { -1,  1,  0 },
                  {  1, -1,  0 },
                  {  1,  1,  0 },
                                 
                  { -1,  0,  1 },
                  {  0, -1,  1 },
                  {  0,  1,  1 },
                  {  1,  0,  1 }
            }};
        }

        if(false){
            ud.voxel_triplets = {{ 
                  { -1, -1, -1 },
                  { -1,  1, -1 },
                  {  1, -1, -1 },
                  {  1,  1, -1 },
                  { -1, -1,  1 },
                  { -1,  1,  1 },
                  {  1, -1,  1 },
                  {  1,  1,  1 } 
            }};
        }

        // All of the above combined into one...
        if(true){
            ud.voxel_triplets = {{ 
                  { -1, -1, -1 },
                  { -1,  0, -1 },
                  { -1,  1, -1 },
                  {  0, -1, -1 },
                  {  0,  0, -1 },
                  {  0,  1, -1 },
                  {  1, -1, -1 },
                  {  1,  0, -1 },
                  {  1,  1, -1 },
                                 
                  { -1, -1,  0 },
                  { -1,  0,  0 },
                  { -1,  1,  0 },
                  {  0, -1,  0 },
                  //{  0,  0,  0 },
                  {  0,  1,  0 },
                  {  1, -1,  0 },
                  {  1,  0,  0 },
                  {  1,  1,  0 },
                                 
                  { -1, -1,  1 },
                  { -1,  0,  1 },
                  { -1,  1,  1 },
                  {  0, -1,  1 },
                  {  0,  0,  1 },
                  {  0,  1,  1 },
                  {  1, -1,  1 },
                  {  1,  0,  1 },
                  {  1,  1,  1 } 
            }};
        }
        const auto neighbour_count = ud.voxel_triplets.size();


        ud.f_reduce = [=](float v, std::vector<float> &shtl) -> float {
                long int agree = 0; // The number of neighbouring voxels that are in agreement.
                for(const auto &n : shtl){
                    if(std::isfinite(v)){
                        // Agreement based on numerical value.
                        if( std::abs(v - n) < machine_eps ) ++agree;
                    }else{
                        // Agreement based on non-finiteness (e.g., to ensure infs and NaNs alike match).
                        if(std::isfinite(v) == std::isfinite(n)) ++agree;
                    }
                }

                const bool connected = (agree > 6); // 0.5 < (1.0 * agree / neighbour_count);
                const bool isolated = !connected;

                float new_val = v;


                if(false){
                }else if( connected  && replace_is_iso  && replacement_is_mean    ){
                    new_val = v; // Existing value, a no-op.

                }else if( connected  && replace_is_iso  && replacement_is_median  ){
                    new_val = v; // Existing value, a no-op.

                }else if( connected  && replace_is_iso  && replacement_is_value   ){
                    new_val = v; // Existing value, a no-op.


                }else if( connected  && replace_is_well && replacement_is_median  ){
                    throw std::logic_error("The requested parameter combination is not yet supported.");
                    // Not sure how useful this will be. The aggregate will probably be tainted by well-connected voxels.

                }else if( connected  && replace_is_well && replacement_is_mean    ){
                    throw std::logic_error("The requested parameter combination is not yet supported.");

                }else if( connected  && replace_is_well && replacement_is_value   ){
                    new_val = replacement_value;



                }else if( isolated  && replace_is_iso  && replacement_is_mean    ){
                    new_val = Stats::Mean(shtl);

                }else if( isolated  && replace_is_iso  && replacement_is_median  ){
                    new_val = Stats::Median(shtl);

                }else if( isolated  && replace_is_iso  && replacement_is_value   ){
                    new_val = replacement_value;



                }else if( isolated  && replace_is_well && replacement_is_mean    ){
                    new_val = v; // Existing value, a no-op.

                }else if( isolated  && replace_is_well && replacement_is_median  ){
                    new_val = v; // Existing value, a no-op.

                }else if( isolated  && replace_is_well && replacement_is_value   ){
                    new_val = v; // Existing value, a no-op.

                }else{
                    throw std::logic_error("Parameter combination was not anticipated. Cannot continue.");
                }

                return new_val;
        };

        if(!(*iap_it)->imagecoll.Compute_Images( ComputeVolumetricNeighbourhoodSampler, 
                                                 {}, cc_ROIs, &ud )){
            throw std::runtime_error("Unable to filter isolated voxels.");
        }
    }

    return DICOM_data;
}
