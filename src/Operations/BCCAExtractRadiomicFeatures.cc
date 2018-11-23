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
#include "PresentationImage.h"


OperationDoc OpArgDocBCCAExtractRadiomicFeatures(void){
    OperationDoc out;
    out.name = "BCCAExtractRadiomicFeatures";
    out.desc = "This operation extracts radiomic features from an image and one or more ROIs.";

    out.notes.emplace_back(
        "This is a 'simplified' version of the full radiomics extract routine that uses defaults"
        " that are expected to be reasonable across a wide range of scenarios." 
    );


    out.args.splice( out.args.end(), OpArgDocSimplifyContours().args );
    out.args.splice( out.args.end(), OpArgDocExtractRadiomicFeatures().args );
    out.args.splice( out.args.end(), OpArgDocPresentationImage().args );

    // Adjust the defaults to suit this particular workflow.
    for(auto &oparg : out.args){

        // SimplifyContours
        if(false){
        }else if(oparg.name == "FractionalAreaTolerance"){
            oparg.default_val = "0.10";

        // ExtractRadiomicFeatures
        }else if(oparg.name == "ImageSelection"){
            oparg.default_val = "last";
            oparg.visibility  = OpArgVisibility::Hide;

        // PresentationImage
        }else if(oparg.name == "ScaleFactor"){
            oparg.default_val = "1.5";
            oparg.visibility  = OpArgVisibility::Hide;
        }
    }

    return out;
}


Drover
BCCAExtractRadiomicFeatures(Drover DICOM_data, 
              OperationArgPkg OptArgs,
              std::map<std::string, std::string> InvocationMetadata,
              std::string FilenameLex){

    DICOM_data = SimplifyContours(
                     std::move(DICOM_data), OptArgs, InvocationMetadata, FilenameLex);
    DICOM_data = ExtractRadiomicFeatures(
                     std::move(DICOM_data), OptArgs, InvocationMetadata, FilenameLex);
    DICOM_data = PresentationImage(
                     std::move(DICOM_data), OptArgs, InvocationMetadata, FilenameLex);

    return DICOM_data;
}
