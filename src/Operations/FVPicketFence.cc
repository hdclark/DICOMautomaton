//FVPicketFence.cc - A part of DICOMautomaton 2018. Written by hal clark.

#include <algorithm>
#include <list>
#include <map>
#include <string>    

#include "../Structs.h"
#include "../Regex_Selectors.h"

#include "ContourWholeImages.h"
#include "IsolatedVoxelFilter.h"
#include "CropImages.h"
#include "AutoCropImages.h"
#include "AnalyzePicketFence.h"

#ifdef DCMA_USE_SFML
#include "PresentationImage.h"
#endif

#include "FVPicketFence.h"


OperationDoc OpArgDocFVPicketFence(void){
    OperationDoc out;
    out.name = "FVPicketFence";
    out.desc = "This operation performs a picket fence QA test using an RTIMAGE file.";

    out.notes.emplace_back(
        "This is a 'simplified' version of the full picket fence analysis program that uses defaults"
        " that are expected to be reasonable across a wide range of scenarios." 
    );

#ifdef DCMA_USE_SFML    
#else
    out.notes.emplace_back(
        "This version of DICOMautomaton has been compiled without SFML support."
        " The post-analysis PresentationImage operation will be omitted."
    );
#endif

    out.args.splice( out.args.end(), OpArgDocContourWholeImages().args );
    out.args.splice( out.args.end(), OpArgDocIsolatedVoxelFilter().args );

    out.args.splice( out.args.end(), OpArgDocAutoCropImages().args );
    out.args.splice( out.args.end(), OpArgDocCropImages().args );

    out.args.splice( out.args.end(), OpArgDocAnalyzePicketFence().args );

#ifdef DCMA_USE_SFML    
    out.args.splice( out.args.end(), OpArgDocPresentationImage().args );
#endif

    // Adjust the defaults to suit this particular workflow.
    for(auto &oparg : out.args){

        // Hide all options by default. This is important to maintain a streamlined facade in case the constituent
        // operations are altered (which is likely).
        oparg.visibility  = OpArgVisibility::Hide;

        // ContourWholeImages
        if(false){
        }else if(oparg.name == "ImageSelection"){
            oparg.default_val = "last";

        }else if(oparg.name == "ROILabel"){
            oparg.default_val = "entire_image";

        // IsolatedVoxelFilter 
        //}else if(oparg.name == "ImageSelection"){
        //    oparg.default_val = "last";
        //
        }else if(oparg.name == "Replacement"){
            oparg.default_val = "conservative";

        }else if(oparg.name == "Replace"){
            oparg.default_val = "isolated";

        }else if(oparg.name == "ROILabelRegex"){
            oparg.default_val = "entire_image";

        // CropImages
        //}else if(oparg.name == "ImageSelection"){
        //    oparg.default_val = "last";
        //
        }else if(oparg.name == "RowsL"){
            oparg.default_val = "5px";
        }else if(oparg.name == "RowsH"){
            oparg.default_val = "5px";
        }else if(oparg.name == "ColumnsL"){
            oparg.default_val = "5px";
        }else if(oparg.name == "ColumnsH"){
            oparg.default_val = "5px";

        // AutoCropImages
        //}else if(oparg.name == "ImageSelection"){
        //    oparg.default_val = "last";
        //
        }else if(oparg.name == "RTIMAGE"){
            oparg.default_val = "true";

        // AnalyzePicketFence
        //}else if(oparg.name == "ImageSelection"){
        //    oparg.default_val = "last";
        //
        }else if(oparg.name == "ThresholdDistance"){
            oparg.default_val = "0.5";
            oparg.visibility  = OpArgVisibility::Show;

        }else if(oparg.name == "InteractivePlots"){
            oparg.default_val = "false";

        }else if(oparg.name == "MLCModel"){
            oparg.visibility  = OpArgVisibility::Show;

        }else if(oparg.name == "MinimumJunctionSeparation"){
            oparg.visibility  = OpArgVisibility::Show;

#ifdef DCMA_USE_SFML    
        // PresentationImage
        //}else if(oparg.name == "ImageSelection"){
        //    oparg.default_val = "last";
        //
        }else if(oparg.name == "ScaleFactor"){
            oparg.default_val = "1.5";
            oparg.visibility  = OpArgVisibility::Show;
#endif
        }
    }

    return out;
}


Drover
FVPicketFence(Drover DICOM_data, 
              OperationArgPkg OptArgs,
              std::map<std::string, std::string> InvocationMetadata,
              std::string FilenameLex){

    DICOM_data = ContourWholeImages(std::move(DICOM_data), OptArgs, InvocationMetadata, FilenameLex);
    DICOM_data = IsolatedVoxelFilter(std::move(DICOM_data), OptArgs, InvocationMetadata, FilenameLex);

    DICOM_data = AutoCropImages(std::move(DICOM_data), OptArgs, InvocationMetadata, FilenameLex);
    DICOM_data = CropImages(std::move(DICOM_data), OptArgs, InvocationMetadata, FilenameLex);

    DICOM_data = AnalyzePicketFence(std::move(DICOM_data), OptArgs, InvocationMetadata, FilenameLex);

#ifdef DCMA_USE_SFML    
    DICOM_data = PresentationImage(std::move(DICOM_data), OptArgs, InvocationMetadata, FilenameLex);
#endif

    return DICOM_data;
}
