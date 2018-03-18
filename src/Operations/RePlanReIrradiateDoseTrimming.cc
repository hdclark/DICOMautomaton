//RePlanReIrradiateDoseTrimming.cc - A part of DICOMautomaton 2017. Written by hal clark.

#include <string>    
#include <map>
#include <experimental/optional>

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.

#include "../Structs.h"

#include "../Imebra_Shim.h"

#include "ConvertDoseToImage.h"
#include "HighlightROIs.h"
#include "DICOMExportImagesAsDose.h"


std::list<OperationArgDoc> OpArgDocRePlanReIrradiateDoseTrimming(void){
    std::list<OperationArgDoc> out;

    auto c1 = OpArgDocConvertDoseToImage();
    auto c2 = OpArgDocHighlightROIs();
    auto c3 = OpArgDocDICOMExportImagesAsDose();

    out.splice( out.end(), c1 );
    out.splice( out.end(), c2 );
    out.splice( out.end(), c3 );


    // Adjust the defaults to suit this particular workflow.
    for(auto &oparg : out){
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
