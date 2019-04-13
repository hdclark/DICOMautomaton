//Average.cc - A part of DICOMautomaton 2017. Written by hal clark.

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
#include "../YgorImages_Functors/Grouping/Misc_Functors.h"
#include "../YgorImages_Functors/Processing/Average_Pixel_Value.h"
#include "Average.h"
#include "YgorImages.h"



OperationDoc OpArgDocAverage(void){
    OperationDoc out;
    out.name = "Average";

    out.desc = 
        "This operation averages image arrays/volumes. It can average over spatial or temporal dimensions. However, rather than"
        " relying specifically on time for temporal averaging, any images that have overlapping voxels can be averaged.";

    out.notes.emplace_back(
        "This operation is typically used to create an aggregate view of a large volume of data. It may also increase SNR"
        " and can be used for contouring purposes."
    );
        
    out.args.emplace_back();
    out.args.back() = IAWhitelistOpArgDoc();
    out.args.back().name = "ImageSelection";
    out.args.back().default_val = "last";
   
    out.args.emplace_back();
    out.args.back().name = "AveragingMethod";
    out.args.back().desc = "The averaging method to use. Valid methods are 'overlapping-spatially' and 'overlapping-temporally'.";
    out.args.back().default_val = "";
    out.args.back().expected = true;
    out.args.back().examples = { "overlapping-spatially", "overlapping-temporally" };

    return out;
}


Drover Average(Drover DICOM_data, OperationArgPkg OptArgs, std::map<std::string,std::string> /*InvocationMetadata*/, std::string /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();
    const auto AveragingMethodStr = OptArgs.getValueStr("AveragingMethod").value();
    //-----------------------------------------------------------------------------------------------------------------
    const auto regex_none = std::regex("no?n?e?$", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto regex_last = std::regex("la?s?t?$", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto regex_all  = std::regex("al?l?$",   std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);

    const auto overlap_spat = std::regex("overlapping-spatially", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto overlap_temp = std::regex("overlapping-temporally", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);


    if(false){
    }else if(std::regex_match(AveragingMethodStr, overlap_spat)){
        auto IAs_all = All_IAs( DICOM_data );
        auto IAs = Whitelist( IAs_all, ImageSelectionStr );
        for(auto & iap_it : IAs){
            if(!(*iap_it)->imagecoll.Process_Images_Parallel( GroupSpatiallyOverlappingImages,
                                                              CondenseAveragePixel,
                                                              {}, {} )){
                throw std::runtime_error("Unable to average (image_array, overlapping-spatially).");
            }
        }

    }else if(std::regex_match(AveragingMethodStr, overlap_temp)){
        auto IAs_all = All_IAs( DICOM_data );
        auto IAs = Whitelist( IAs_all, ImageSelectionStr );
        for(auto & iap_it : IAs){
            if(!(*iap_it)->imagecoll.Process_Images_Parallel( GroupTemporallyOverlappingImages,
                                                              CondenseAveragePixel,
                                                              {}, {} )){
                throw std::runtime_error("Unable to average (image_array, overlapping-temporally).");
            }
        }

    }else{
        throw std::invalid_argument("Invalid averaging method specified. Cannot continue");
    }

    return DICOM_data;
}
