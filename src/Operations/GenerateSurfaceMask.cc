//GenerateSurfaceMask.cc - A part of DICOMautomaton 2016. Written by hal clark.

#include <any>
#include <optional>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>    

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../YgorImages_Functors/Compute/GenerateSurfaceMask.h"
#include "GenerateSurfaceMask.h"
#include "YgorImages.h"



OperationDoc OpArgDocGenerateSurfaceMask(){
    OperationDoc out;
    out.name = "GenerateSurfaceMask";
    out.desc = 
        "This operation generates a surface image mask, which contains information about whether each voxel is"
        " within, on, or outside the selected ROI(s).";


    out.args.emplace_back();
    out.args.back().name = "BackgroundVal";
    out.args.back().desc = "The value to give to voxels neither inside nor on the surface of the ROI(s).";
    out.args.back().default_val = "0.0";
    out.args.back().expected = true;
    out.args.back().examples = { "0.0", "-1.0", "1.23", "2.34E26" };

    out.args.emplace_back();
    out.args.back().name = "InteriorVal";
    out.args.back().desc = "The value to give to voxels within the volume of the ROI(s) but not on the surface.";
    out.args.back().default_val = "1.0";
    out.args.back().expected = true;
    out.args.back().examples = { "0.0", "-1.0", "1.23", "2.34E26" };

    out.args.emplace_back();
    out.args.back().name = "SurfaceVal";
    out.args.back().desc = "The value to give to voxels on the surface/boundary of ROI(s).";
    out.args.back().default_val = "2.0";
    out.args.back().expected = true;
    out.args.back().examples = { "0.0", "-1.0", "1.23", "2.34E26" };



    out.args.emplace_back();
    out.args.back() = NCWhitelistOpArgDoc();
    out.args.back().name = "NormalizedROILabelRegex";
    out.args.back().default_val = ".*";

    out.args.emplace_back();
    out.args.back() = RCWhitelistOpArgDoc();
    out.args.back().name = "ROILabelRegex";
    out.args.back().default_val = ".*";

    return out;
}



bool GenerateSurfaceMask(Drover &DICOM_data,
                           const OperationArgPkg& OptArgs,
                           std::map<std::string, std::string>& /*InvocationMetadata*/,
                           const std::string& /*FilenameLex*/){

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

    return true;
}
