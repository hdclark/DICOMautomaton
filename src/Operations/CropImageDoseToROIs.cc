//CropImageDoseToROIs.cc - A part of DICOMautomaton 2016. Written by hal clark.

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
#include "../YgorImages_Functors/Compute/CropToROIs.h"
#include "CropImageDoseToROIs.h"
#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.



OperationDoc OpArgDocCropImageDoseToROIs(void){
    OperationDoc out;
    out.name = "CropImageDoseToROIs";

    out.desc = 
        " This operation crops image and/or dose array slices to the specified ROI(s), with an additional margin.";

    out.args.emplace_back();
    out.args.back().name = "DICOMMargin";
    out.args.back().desc = "The amount of margin (in the DICOM coordinate system) to surround the ROI(s).";
    out.args.back().default_val = "0.5";
    out.args.back().expected = true;
    out.args.back().examples = { "0.1", "2.0", "-0.5", "20.0" };

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

    return out;
}



Drover CropImageDoseToROIs(Drover DICOM_data, OperationArgPkg OptArgs, std::map<std::string,std::string> /*InvocationMetadata*/, std::string /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto DICOMMargin = std::stod(OptArgs.getValueStr("DICOMMargin").value());
    const auto DoseImageSelectionStr = OptArgs.getValueStr("DoseImageSelection").value();
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();
    const auto ROILabelRegex = OptArgs.getValueStr("ROILabelRegex").value();
    const auto NormalizedROILabelRegex = OptArgs.getValueStr("NormalizedROILabelRegex").value();
    //-----------------------------------------------------------------------------------------------------------------

    const auto regex_none = std::regex("no?n?e?$", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto regex_last = std::regex("la?s?t?$", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto regex_all  = std::regex("al?l?$",   std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);

    if( !std::regex_match(DoseImageSelectionStr, regex_none)
    &&  !std::regex_match(DoseImageSelectionStr, regex_last)
    &&  !std::regex_match(DoseImageSelectionStr, regex_all) ){
        throw std::invalid_argument("Dose Image selection is not valid. Cannot continue.");
    }

    //Stuff references to all contours into a list. Remember that you can still address specific contours through
    // the original holding containers (which are not modified here).
    auto cc_all = All_CCs( DICOM_data );
    auto cc_ROIs = Whitelist( cc_all, { { "ROIName", ROILabelRegex },
                                        { "NormalizedROIName", NormalizedROILabelRegex } } );

    // --- Cycle over all images and dose arrays, performing the crop ---

    //Image data.
    auto IAs_all = All_IAs( DICOM_data );
    auto IAs = Whitelist( IAs_all, ImageSelectionStr );
    for(auto & iap_it : IAs){
        CropToROIsUserData ud;
        ud.row_margin = DICOMMargin;
        ud.col_margin = DICOMMargin;
        ud.ort_margin = DICOMMargin;

        if(!(*iap_it)->imagecoll.Compute_Images( ComputeCropToROIs, { },
                                                 cc_ROIs, &ud )){
            throw std::runtime_error("Unable to perform crop.");
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
        CropToROIsUserData ud;
        ud.row_margin = DICOMMargin;
        ud.col_margin = DICOMMargin;
        ud.ort_margin = DICOMMargin;

        if(!(*dap_it)->imagecoll.Compute_Images( ComputeCropToROIs, { },
                                                 cc_ROIs, &ud )){
            throw std::runtime_error("Unable to perform crop.");
        }
        ++dap_it;
    }

    return DICOM_data;
}
