//CropROIDose.cc - A part of DICOMautomaton 2018. Written by hal clark.

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


OperationDoc OpArgDocCropROIDose(void){
    OperationDoc out;
    out.name = "CropROIDose";
    out.desc = 
      "This operation provides a simplified interface for overriding the dose outside a ROI."
      " For example, this operation can be used to modify a base plan by eliminating dose "
      " outside an OAR.";

    out.notes.emplace_back(
      "This operation performs the opposite of the 'Trim' operation, which trims the dose"
      " inside a ROI."
    );
    out.notes.emplace_back(
      "The inclusivity of a dose voxel that straddles the ROI boundary can be specified in"
      " various ways. Refer to the Inclusivity parameter documentation."
    );
    out.notes.emplace_back(
      "By default this operation only overrides dose within a ROI. The opposite, overriding"
      " dose outside of a ROI, can be accomplished using the expert interface."
    );

    out.args.splice( out.args.end(), OpArgDocConvertDoseToImage().args );
    out.args.splice( out.args.end(), OpArgDocHighlightROIs().args );
    out.args.splice( out.args.end(), OpArgDocDICOMExportImagesAsDose().args );


    // Adjust the defaults to suit this particular workflow.
    for(auto &oparg : out.args){
        if(false){
        // ConvertDoseToImage options.
        // ... currently no options...

        // HighlightROIs options.
        }else if(oparg.name == "Channel"){
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

        }else if(oparg.name == "InteriorVal"){
            oparg.default_val = "0.0";
            oparg.visibility  = OpArgVisibility::Hide;

        }else if(oparg.name == "ExteriorOverwrite"){
            oparg.default_val = "true";
            oparg.visibility  = OpArgVisibility::Hide;

        }else if(oparg.name == "InteriorOverwrite"){
            oparg.default_val = "false";
            oparg.visibility  = OpArgVisibility::Hide;

        // DICOMExportImagesAsDose options.
        }else if(oparg.name == "ParanoiaLevel"){
            oparg.default_val = "medium";
            oparg.visibility  = OpArgVisibility::Hide;
        }
    }

    return out;
}


Drover
CropROIDose(Drover DICOM_data, 
            OperationArgPkg OptArgs,
            std::map<std::string, std::string> InvocationMetadata,
            std::string FilenameLex){

    DICOM_data.dose_data = Meld_Dose_Data(DICOM_data.dose_data);

    DICOM_data = ConvertDoseToImage(std::move(DICOM_data), OptArgs, InvocationMetadata, FilenameLex);

    DICOM_data = HighlightROIs(std::move(DICOM_data), OptArgs, InvocationMetadata, FilenameLex);

    DICOM_data = DICOMExportImagesAsDose(std::move(DICOM_data), OptArgs, InvocationMetadata, FilenameLex);

    return DICOM_data;
}
