//BCCAExtractRadiomicFeatures.cc - A part of DICOMautomaton 2018. Written by hal clark.

#include <algorithm>
#include <list>
#include <map>
#include <string>    

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "BCCAExtractRadiomicFeatures.h"
#include "SimplifyContours.h"
#include "ExtractRadiomicFeatures.h"

#ifdef DCMA_USE_SFML    
#include "PresentationImage.h"
#endif 


OperationDoc OpArgDocBCCAExtractRadiomicFeatures(){
    OperationDoc out;
    out.name = "BCCAExtractRadiomicFeatures";
    out.desc = "This operation extracts radiomic features from an image and one or more ROIs.";

    out.notes.emplace_back(
        "This is a 'simplified' version of the full radiomics extract routine that uses defaults"
        " that are expected to be reasonable across a wide range of scenarios." 
    );

#ifdef DCMA_USE_SFML    
#else
    out.notes.emplace_back(
        "This version of DICOMautomaton has been compiled without SFML support."
        " The post-extraction PresentationImage operation will be omitted."
    );
#endif

    out.args.splice( out.args.end(), OpArgDocSimplifyContours().args );
    out.args.splice( out.args.end(), OpArgDocExtractRadiomicFeatures().args );
#ifdef DCMA_USE_SFML    
    out.args.splice( out.args.end(), OpArgDocPresentationImage().args );
#endif

    // Adjust the defaults to suit this particular workflow.
    for(auto &oparg : out.args){

        // SimplifyContours
        if(false){
        }else if(oparg.name == "FractionalAreaTolerance"){
            oparg.default_val = "0.05";

        }else if(oparg.name == "SimplificationMethod"){
            oparg.default_val = "vert-rem";
            oparg.visibility  = OpArgVisibility::Hide;

        // ExtractRadiomicFeatures
        }else if(oparg.name == "ImageSelection"){
            oparg.default_val = "last";
            oparg.visibility  = OpArgVisibility::Hide;

#ifdef DCMA_USE_SFML
        // PresentationImage
        }else if(oparg.name == "ScaleFactor"){
            oparg.default_val = "1.5";
            oparg.visibility  = OpArgVisibility::Hide;
#endif

        }

    }

    return out;
}


Drover
BCCAExtractRadiomicFeatures(Drover DICOM_data, 
              const OperationArgPkg& OptArgs,
              const std::map<std::string, std::string>& InvocationMetadata,
              const std::string& FilenameLex){

    DICOM_data = SimplifyContours(
                     DICOM_data, OptArgs, InvocationMetadata, FilenameLex);
    DICOM_data = ExtractRadiomicFeatures(
                     DICOM_data, OptArgs, InvocationMetadata, FilenameLex);
#ifdef DCMA_USE_SFML
    DICOM_data = PresentationImage(
                     DICOM_data, OptArgs, InvocationMetadata, FilenameLex);
#endif

    return DICOM_data;
}
