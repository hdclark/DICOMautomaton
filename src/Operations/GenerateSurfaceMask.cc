//GenerateSurfaceMask.cc - A part of DICOMautomaton 2016. Written by hal clark.

#include <experimental/any>
#include <experimental/optional>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>    

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../YgorImages_Functors/Compute/GenerateSurfaceMask.h"
#include "GenerateSurfaceMask.h"
#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.



std::list<OperationArgDoc> OpArgDocGenerateSurfaceMask(void){
    std::list<OperationArgDoc> out;

    out.emplace_back();
    out.back().name = "BackgroundVal";
    out.back().desc = "The value to give to voxels neither inside nor on the surface of the ROI(s).";
    out.back().default_val = "0.0";
    out.back().expected = true;
    out.back().examples = { "0.0", "-1.0", "1.23", "2.34E26" };

    out.emplace_back();
    out.back().name = "InteriorVal";
    out.back().desc = "The value to give to voxels within the volume of the ROI(s) but not on the surface.";
    out.back().default_val = "1.0";
    out.back().expected = true;
    out.back().examples = { "0.0", "-1.0", "1.23", "2.34E26" };

    out.emplace_back();
    out.back().name = "SurfaceVal";
    out.back().desc = "The value to give to voxels on the surface/boundary of ROI(s).";
    out.back().default_val = "2.0";
    out.back().expected = true;
    out.back().examples = { "0.0", "-1.0", "1.23", "2.34E26" };



    out.emplace_back();
    out.back().name = "NormalizedROILabelRegex";
    out.back().desc = "A regex matching ROI labels/names to consider. The default will match"
                      " all available ROIs. Be aware that input spaces are trimmed to a single space."
                      " If your ROI name has more than two sequential spaces, use regex to avoid them."
                      " All ROIs have to match the single regex, so use the 'or' token if needed."
                      " Regex is case insensitive and uses extended POSIX syntax.";
    out.back().default_val = ".*";
    out.back().expected = true;
    out.back().examples = { ".*", ".*Body.*", "Body", "Gross_Liver",
                            R"***(.*Left.*Parotid.*|.*Right.*Parotid.*|.*Eye.*)***",
                            R"***(Left Parotid|Right Parotid)***" };

    out.emplace_back();
    out.back().name = "ROILabelRegex";
    out.back().desc = "A regex matching ROI labels/names to consider. The default will match"
                      " all available ROIs. Be aware that input spaces are trimmed to a single space."
                      " If your ROI name has more than two sequential spaces, use regex to avoid them."
                      " All ROIs have to match the single regex, so use the 'or' token if needed."
                      " Regex is case insensitive and uses extended POSIX syntax.";
    out.back().default_val = ".*";
    out.back().expected = true;
    out.back().examples = { ".*", ".*body.*", "body", "Gross_Liver",
                            R"***(.*left.*parotid.*|.*right.*parotid.*|.*eyes.*)***",
                            R"***(left_parotid|right_parotid)***" };

    return out;
}



Drover GenerateSurfaceMask(Drover DICOM_data, 
                           OperationArgPkg OptArgs, 
                           std::map<std::string,std::string> /*InvocationMetadata*/, 
                           std::string /*FilenameLex*/ ){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto BackgroundVal  = std::stod(OptArgs.getValueStr("BackgroundVal").value());
    const auto InteriorVal    = std::stod(OptArgs.getValueStr("InteriorVal").value());
    const auto SurfaceVal     = std::stod(OptArgs.getValueStr("SurfaceVal").value());
    const auto ROILabelRegex  = OptArgs.getValueStr("ROILabelRegex").value();
    const auto NormalizedROILabelRegex = OptArgs.getValueStr("NormalizedROILabelRegex").value();
    //-----------------------------------------------------------------------------------------------------------------

    auto img_arr_ptr = DICOM_data.image_data.back();
    if(img_arr_ptr == nullptr){
        throw std::runtime_error("Encountered a nullptr when expecting a valid Image_Array.");
    }else if(img_arr_ptr->imagecoll.images.empty()){
        throw std::runtime_error("Encountered a Image_Array with no valid images.");
    }


    //Stuff references to all contours into a list. Remember that you can still address specific contours through
    // the original holding containers (which are not modified here).
    auto cc_all = All_CCs( DICOM_data );
    auto cc_ROIs = Whitelist( cc_all, { { "ROIName", ROILabelRegex },
                                        { "NormalizedROIName", NormalizedROILabelRegex } } );
    if(cc_ROIs.empty()){
        throw std::invalid_argument("No contours selected. Cannot continue.");
    }

    //Perform the computation.
    GenerateSurfaceMaskUserData ud;
    ud.background_val = BackgroundVal;
    ud.surface_val    = SurfaceVal;
    ud.interior_val   = InteriorVal;

    if(!img_arr_ptr->imagecoll.Compute_Images( ComputeGenerateSurfaceMask, { },
                                               cc_ROIs, &ud )){
        throw std::runtime_error("Unable to generate a surface mask.");
    }

    return DICOM_data;
}
