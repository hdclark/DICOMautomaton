//FVPicketFence.cc - A part of DICOMautomaton 2018. Written by hal clark.

#include <algorithm>
#include <list>
#include <map>
#include <string>    

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "FVPicketFence.h"
#include "CropImages.h"
#include "AutoCropImages.h"
#include "AnalyzePicketFence.h"
#include "PresentationImage.h"


std::list<OperationArgDoc> OpArgDocFVPicketFence(void){
    std::list<OperationArgDoc> out;

    auto c1 = OpArgDocCropImages();
    auto c2 = OpArgDocAutoCropImages();
    auto c5 = OpArgDocAnalyzePicketFence();
    auto c7 = OpArgDocPresentationImage();

    out.splice( out.end(), c1 );
    out.splice( out.end(), c2 );
    out.splice( out.end(), c5 );
    out.splice( out.end(), c7 );

    // Adjust the defaults to suit this particular workflow.
    for(auto &oparg : out){
        if(false){
        }else if(oparg.name == "ImageSelection"){
            oparg.default_val = "last";
        }else if(oparg.name == "RowsL"){
            oparg.default_val = "30px";
        }else if(oparg.name == "RowsH"){
            oparg.default_val = "30px";
        }else if(oparg.name == "ColumnsL"){
            oparg.default_val = "30px";
        }else if(oparg.name == "ColumnsH"){
            oparg.default_val = "30px";

        }else if(oparg.name == "RTIMAGE"){
            oparg.default_val = "true";

        }else if(oparg.name == "ThresholdDistance"){
            oparg.default_val = "1.0";
        }else if(oparg.name == "InteractivePlots"){
            oparg.default_val = "false";

        }else if(oparg.name == "ScaleFactor"){
            oparg.default_val = "1.0";
        }
    }

    return out;
}


Drover
FVPicketFence(Drover DICOM_data, 
              OperationArgPkg OptArgs,
              std::map<std::string, std::string> InvocationMetadata,
              std::string FilenameLex){

    DICOM_data = CropImages(std::move(DICOM_data), OptArgs, InvocationMetadata, FilenameLex);
    DICOM_data = AutoCropImages(std::move(DICOM_data), OptArgs, InvocationMetadata, FilenameLex);
    DICOM_data = AnalyzePicketFence(std::move(DICOM_data), OptArgs, InvocationMetadata, FilenameLex);
    DICOM_data = PresentationImage(std::move(DICOM_data), OptArgs, InvocationMetadata, FilenameLex);

    return DICOM_data;
}
