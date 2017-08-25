//RePlanReIrradiateDoseTrimming.cc - A part of DICOMautomaton 2017. Written by hal clark.

#include <string>    
#include <map>
#include <experimental/optional>

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.

#include "../Structs.h"

#include "../Imebra_Shim.h"


std::list<OperationArgDoc> OpArgDocRePlanReIrradiateDoseTrimming(void){
    std::list<OperationArgDoc> out;


    // Composite args here via splicing.

    return out;
}

Drover
RePlanReIrradiateDoseTrimming(Drover DICOM_data, 
                              OperationArgPkg OptArgs,
                              std::map<std::string, std::string> /*InvocationMetadata*/,
                              std::string /*FilenameLex*/){

    // Composite operation here via serializing individual ops.

    // Convert to images.

    // Highlight.

    // Export to DICOM.


    return std::move(DICOM_data);
}
