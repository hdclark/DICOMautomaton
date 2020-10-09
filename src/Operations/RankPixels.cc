//RankPixels.cc - A part of DICOMautomaton 2019. Written by hal clark.

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
#include "../YgorImages_Functors/Compute/Rank_Pixels.h"
#include "RankPixels.h"
#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.



OperationDoc OpArgDocRankPixels(){
    OperationDoc out;
    out.name = "RankPixels";
    out.desc = "This operation ranks pixels throughout an image array.";

    out.notes.emplace_back(
        "This routine operates on all images in an image array, so pixel value ranks are valid throughout the array."
        " However, the window and level of each window is separately determined."
        " You will need to set a uniform window and level manually if desired."
    );
    out.notes.emplace_back(
        "This routine operates on all images in an image array."
        " If images need to be processed individually, arrays will have to be exploded prior to calling this routine."
        " Note that if this functionality is required, it can be implemented as an operation option easily."
        " Likewise, if multiple image arrays must be considered simultaneously, they will need to be combined before"
        " invoking this operation."
        // For individual image processing, you just need to apply the Compute_Images() separately to each image in the array.
    );
                        
    out.args.emplace_back();
    out.args.back() = IAWhitelistOpArgDoc();
    out.args.back().name = "ImageSelection";
    out.args.back().default_val = "last";
    

    out.args.emplace_back();
    out.args.back().name = "Method";
    out.args.back().desc = "Pixels participating in the ranking process will have their pixel values replaced."
                           " They can be replaced with either a rank or the corresponding percentile."
                           " Ranks start at zero and percentiles are centre-weighted (i.e., rank-averaged).";
    out.args.back().default_val = "Percentile";
    out.args.back().expected = true;
    out.args.back().examples = { "Rank",
                                 "Percentile" };
    out.args.back().samples = OpArgSamples::Exhaustive;

    out.args.emplace_back();
    out.args.back().name = "LowerThreshold";
    out.args.back().desc = "The (inclusive) threshold above which pixel values must be in order to participate"
                           " in the rank.";
    out.args.back().default_val = "-inf";
    out.args.back().expected = true;
    out.args.back().examples = { "-inf", "0.0", "-900" };
    

    out.args.emplace_back();
    out.args.back().name = "UpperThreshold";
    out.args.back().desc = "The (inclusive) threshold below which pixel values must be in order to participate"
                           " in the rank.";
    out.args.back().default_val = "inf";
    out.args.back().expected = true;
    out.args.back().examples = { "inf", "0.0", "1500" };


    return out;
}



Drover RankPixels(Drover DICOM_data,
                  const OperationArgPkg& OptArgs,
                  const std::map<std::string, std::string>&
                  /*InvocationMetadata*/,
                  const std::string& /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();
    const auto MethodStr = OptArgs.getValueStr("Method").value();
    const auto LowerThreshold = std::stod( OptArgs.getValueStr("LowerThreshold").value() );
    const auto UpperThreshold = std::stod( OptArgs.getValueStr("UpperThreshold").value() );

    //-----------------------------------------------------------------------------------------------------------------
    const auto method_rank = Compile_Regex("^ra?n?k?$");
    const auto method_tile = Compile_Regex("^pe?r?c?e?n?t?i?l?e?$");

    //-----------------------------------------------------------------------------------------------------------------
    {
        auto IAs_all = All_IAs( DICOM_data );
        auto IAs = Whitelist( IAs_all, ImageSelectionStr );
        for(auto & iap_it : IAs){
            RankPixelsUserData ud;
            ud.inc_lower_threshold = LowerThreshold;
            ud.inc_upper_threshold = UpperThreshold;

            if( std::regex_match(MethodStr, method_rank) ){
                ud.replacement_method = RankPixelsUserData::ReplacementMethod::Rank;
            }else if( std::regex_match(MethodStr, method_tile) ){
                ud.replacement_method = RankPixelsUserData::ReplacementMethod::Percentile;
            }else{
                throw std::invalid_argument("Method not understood. Cannot continue.");
            }

            if(!(*iap_it)->imagecoll.Compute_Images( ComputeRankPixels, { }, { }, &ud )){
                throw std::runtime_error("Unable to rank pixels.");
            }
        }
    }

    return DICOM_data;
}
