//SupersampleImageGrid.cc - A part of DICOMautomaton 2016. Written by hal clark.

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
#include "../YgorImages_Functors/Processing/In_Image_Plane_Bicubic_Supersample.h"
#include "../YgorImages_Functors/Processing/In_Image_Plane_Bilinear_Supersample.h"
#include "SupersampleImageGrid.h"
#include "YgorImages.h"



OperationDoc OpArgDocSupersampleImageGrid(void){
    OperationDoc out;
    out.name = "SupersampleImageGrid";

    out.desc = 
        " This operation scales supersamples images so they have more rows and/or columns, but the whole image keeps its"
        " shape and spatial extent."
        " This operation is typically used for zooming into images or trying to ensure a sufficient number of voxels are"
        " within small contours.";
        
    out.notes.emplace_back(
        "Be aware that specifying large multipliers (or even small multipliers on large images) will consume much"
        " memory. It is best to pre-crop images to a region of interest if possible."
    );

    out.args.emplace_back();
    out.args.back().name = "ColumnScaleFactor";
    out.args.back().desc = "A positive integer specifying how many columns will be in the new images."
                      " The number is relative to the incoming image column count. Specifying '1' will"
                      " result in nothing happening. Specifying '8' will result in 8x as many columns.";
    out.args.back().default_val = "2";
    out.args.back().expected = true;
    out.args.back().examples = { "1", "2", "3", "8" };

    out.args.emplace_back();
    out.args.back().name = "DoseImageSelection";
    out.args.back().desc = "Dose images to operate on. Either 'none', 'last', or 'all'.";
    out.args.back().default_val = "none";
    out.args.back().expected = true;
    out.args.back().examples = { "none", "last", "all" };
    
    out.args.emplace_back();
    out.args.back() = IAWhitelistOpArgDoc();
    out.args.back().name = "ImageSelection";
    out.args.back().default_val = "last";
    
    out.args.emplace_back();
    out.args.back().name = "RowScaleFactor";
    out.args.back().desc = "A positive integer specifying how many rows will be in the new images."
                      " The number is relative to the incoming image row count. Specifying '1' will"
                      " result in nothing happening. Specifying '8' will result in 8x as many rows.";
    out.args.back().default_val = "2";
    out.args.back().expected = true;
    out.args.back().examples = { "1", "2", "3", "8" };

    out.args.emplace_back();
    out.args.back().name = "SamplingMethod";
    out.args.back().desc = "The supersampling method to use. Note: 'inplane-' methods only consider neighbours"
                      " in the plane of a single image -- neighbours in adjacent images are not considered.";
    out.args.back().default_val = "inplane-bicubic";
    out.args.back().expected = true;
    out.args.back().examples = { "inplane-bicubic", "inplane-bilinear" };

    return out;
}


Drover SupersampleImageGrid(Drover DICOM_data, OperationArgPkg OptArgs, std::map<std::string,std::string> /*InvocationMetadata*/, std::string /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ColumnScaleFactor = std::stol(OptArgs.getValueStr("ColumnScaleFactor").value());
    const auto DoseImageSelectionStr = OptArgs.getValueStr("DoseImageSelection").value();
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();
    const auto RowScaleFactor = std::stol(OptArgs.getValueStr("RowScaleFactor").value());
    const auto SamplingMethodStr = OptArgs.getValueStr("SamplingMethod").value();
    //-----------------------------------------------------------------------------------------------------------------
    const auto regex_none = std::regex("no?n?e?$", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto regex_last = std::regex("la?s?t?$", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto regex_all  = std::regex("al?l?$",   std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);

    const auto inplane_bilin = std::regex("inp?l?a?n?e?-?b?i?line?a?r?", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto inplane_bicub = std::regex("inp?l?a?n?e?-?b?i?cubi?c?", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);

    if( !std::regex_match(DoseImageSelectionStr, regex_none)
    &&  !std::regex_match(DoseImageSelectionStr, regex_last)
    &&  !std::regex_match(DoseImageSelectionStr, regex_all) ){
        throw std::invalid_argument("Dose Image selection is not valid. Cannot continue.");
    }
    
    if(false){
    }else if(std::regex_match(SamplingMethodStr, inplane_bilin)){
        //Image data.
        auto IAs_all = All_IAs( DICOM_data );
        auto IAs = Whitelist( IAs_all, ImageSelectionStr );
        for(auto & iap_it : IAs){
            InImagePlaneBilinearSupersampleUserData bilin_ud;
            bilin_ud.RowScaleFactor = RowScaleFactor; 
            bilin_ud.ColumnScaleFactor = ColumnScaleFactor; 
            if(!(*iap_it)->imagecoll.Process_Images_Parallel( GroupIndividualImages,
                                                              InImagePlaneBilinearSupersample,
                                                              {}, {}, &bilin_ud )){
                throw std::runtime_error("Unable to bilinearly supersample images. Cannot continue.");
            }
        }

        //Dose data.
        auto dap_it = DICOM_data.dose_data.begin();
        if(false){
        }else if(std::regex_match(DoseImageSelectionStr, regex_none)){ dap_it = DICOM_data.dose_data.end();
        }else if(std::regex_match(DoseImageSelectionStr, regex_last)){
            if(!DICOM_data.dose_data.empty()) dap_it = std::prev(DICOM_data.dose_data.end());
        }
        while(dap_it != DICOM_data.dose_data.end()){
            InImagePlaneBilinearSupersampleUserData bilin_ud;
            bilin_ud.RowScaleFactor = RowScaleFactor; 
            bilin_ud.ColumnScaleFactor = ColumnScaleFactor; 
            if(!(*dap_it)->imagecoll.Process_Images_Parallel( GroupIndividualImages,
                                                              InImagePlaneBilinearSupersample,
                                                              {}, {}, &bilin_ud )){
                throw std::runtime_error("Unable to bilinearly supersample dose images. Cannot continue.");
            }
            ++dap_it;
        }

    }else if(std::regex_match(SamplingMethodStr, inplane_bicub)){
        //Image data.
        auto IAs_all = All_IAs( DICOM_data );
        auto IAs = Whitelist( IAs_all, ImageSelectionStr );
        for(auto & iap_it : IAs){
            InImagePlaneBicubicSupersampleUserData bicub_ud;
            bicub_ud.RowScaleFactor = RowScaleFactor; 
            bicub_ud.ColumnScaleFactor = ColumnScaleFactor; 
            if(!(*iap_it)->imagecoll.Process_Images_Parallel( GroupIndividualImages,
                                                              InImagePlaneBicubicSupersample,
                                                              {}, {}, &bicub_ud )){
                throw std::runtime_error("Unable to bicubically supersample images. Cannot continue.");
            }
        }

        //Dose data.
        auto dap_it = DICOM_data.dose_data.begin();
        if(false){
        }else if(std::regex_match(DoseImageSelectionStr, regex_none)){ dap_it = DICOM_data.dose_data.end();
        }else if(std::regex_match(DoseImageSelectionStr, regex_last)){
            if(!DICOM_data.dose_data.empty()) dap_it = std::prev(DICOM_data.dose_data.end());
        }
        while(dap_it != DICOM_data.dose_data.end()){
            InImagePlaneBicubicSupersampleUserData bicub_ud;
            bicub_ud.RowScaleFactor = RowScaleFactor; 
            bicub_ud.ColumnScaleFactor = ColumnScaleFactor; 
            if(!(*dap_it)->imagecoll.Process_Images_Parallel( GroupIndividualImages,
                                                              InImagePlaneBicubicSupersample,
                                                              {}, {}, &bicub_ud )){
                throw std::runtime_error("Unable to bicubically supersample dose images. Cannot continue.");
            }
            ++dap_it;
        }

    }else{
        throw std::invalid_argument("Invalid sampling method specified. Cannot continue");
    }

    return DICOM_data;
}
