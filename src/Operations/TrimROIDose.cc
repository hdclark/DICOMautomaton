//TrimROIDose.cc - A part of DICOMautomaton 2017. Written by hal clark.

#include <algorithm>
#include <list>
#include <map>
#include <string>    

#include "../Dose_Meld.h"
#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "ConvertDoseToImage.h"
#include "DICOMExportImagesAsDose.h"
#include "HighlightROIs.h"


OperationDoc OpArgDocTrimROIDose(){
    OperationDoc out;
    out.name = "TrimROIDose";

    out.tags.emplace_back("category: image processing");
    out.tags.emplace_back("category: contour processing");
    out.tags.emplace_back("category: radiation dose");

    out.desc = 
      "This operation provides a simplified interface for overriding the dose within a ROI."
      " For example, this operation can be used to modify a base plan by eliminating dose"
      " that coincides with a PTV/CTV/GTV/ROI etc.";

    out.notes.emplace_back(
      "This operation performs the opposite of the 'Crop' operation, which trims the dose"
      " outside a ROI."
    );
    out.notes.emplace_back(
      "The inclusivity of a dose voxel that straddles the ROI boundary can be specified in"
      " various ways. Refer to the Inclusivity parameter documentation."
    );
    out.notes.emplace_back(
      "By default this operation only overrides dose within a ROI. The opposite, overriding"
      " dose outside of a ROI, can be accomplished using the expert interface."
    );

    out.args.splice( out.args.end(), OpArgDocHighlightROIs().args );
    out.args.splice( out.args.end(), OpArgDocDICOMExportImagesAsDose().args );


    // Adjust the defaults to suit this particular workflow.
    for(auto &oparg : out.args){

        // HighlightROIs options.
        if(oparg.name == "Channel"){
            oparg.default_val = "-1";
            oparg.visibility  = OpArgVisibility::Hide;

        }else if(oparg.name == "ImageSelection"){
            oparg.default_val = "all";
            oparg.visibility  = OpArgVisibility::Hide;

        }else if(oparg.name == "ContourOverlap"){
            oparg.default_val = "ignore";
            oparg.visibility  = OpArgVisibility::Hide;

        }else if(oparg.name == "Inclusivity"){
            oparg.default_val = "planar_inc";

        }else if(oparg.name == "ExteriorVal"){
            oparg.default_val = "0.0";
            oparg.visibility  = OpArgVisibility::Hide;

        }else if(oparg.name == "InteriorVal"){
            oparg.default_val = "0.0";

        }else if(oparg.name == "ExteriorOverwrite"){
            oparg.default_val = "false";
            oparg.visibility  = OpArgVisibility::Hide;

        }else if(oparg.name == "InteriorOverwrite"){
            oparg.default_val = "true";
            oparg.visibility  = OpArgVisibility::Hide;

        // DICOMExportImagesAsDose options.
        }else if(oparg.name == "ParanoiaLevel"){
            oparg.default_val = "medium";
            oparg.visibility  = OpArgVisibility::Hide;
        }
    }

    return out;
}


bool TrimROIDose(Drover &DICOM_data,
                   const OperationArgPkg& OptArgs,
                   std::map<std::string, std::string>& InvocationMetadata,
                   const std::string& FilenameLex){

    DICOM_data = Meld_Only_Dose_Data(DICOM_data);

    return HighlightROIs(DICOM_data, OptArgs, InvocationMetadata, FilenameLex)
        && DICOMExportImagesAsDose(DICOM_data, OptArgs, InvocationMetadata, FilenameLex);

    return true;
}
