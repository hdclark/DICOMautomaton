//RePlanReIrradiateDoseTrimming.cc - A part of DICOMautomaton 2017. Written by hal clark.

#include <algorithm>
#include <list>
#include <map>
#include <string>    

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "ConvertDoseToImage.h"
#include "DICOMExportImagesAsDose.h"
#include "HighlightROIs.h"


OperationDoc OpArgDocRePlanReIrradiateDoseTrimming(void){
    OperationDoc out;
    out.name = "RePlanReIrradiateDoseTrimming";
    out.desc = "This operation provides a simplified interface for replanning dose trimming.";

    out.args.splice( out.args.end(), OpArgDocConvertDoseToImage().args );
    out.args.splice( out.args.end(), OpArgDocHighlightROIs().args );
    out.args.splice( out.args.end(), OpArgDocDICOMExportImagesAsDose().args );


    // Adjust the defaults to suit this particular workflow.
    for(auto &oparg : out.args){
        if(false){
        }else if(oparg.name == "Inclusivity"){
            oparg.default_val = "planar_inc";
        }else if(oparg.name == "InteriorOverwrite"){
            oparg.default_val = "false";
        }
    }

    return out;
}


Drover
RePlanReIrradiateDoseTrimming(Drover DICOM_data, 
                              OperationArgPkg OptArgs,
                              std::map<std::string, std::string> InvocationMetadata,
                              std::string FilenameLex){

    DICOM_data = ConvertDoseToImage(std::move(DICOM_data), OptArgs, InvocationMetadata, FilenameLex);

    DICOM_data = HighlightROIs(std::move(DICOM_data), OptArgs, InvocationMetadata, FilenameLex);

    DICOM_data = DICOMExportImagesAsDose(std::move(DICOM_data), OptArgs, InvocationMetadata, FilenameLex);

    return DICOM_data;
}
