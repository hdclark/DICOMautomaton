//NoOp.cc - A part of DICOMautomaton 2021. Written by hal clark.

#include <list>
#include <map>
#include <memory>
#include <string>    

#include "../Structs.h"

#include "NoOp.h"


OperationDoc OpArgDocNoOp(){
    OperationDoc out;
    out.name = "NoOp";

    out.desc = 
        "This operation does nothing. It produces no side-effects.";

    return out;
}

Drover NoOp(const Drover &DICOM_data,
            const OperationArgPkg&,
            const std::map<std::string, std::string>&,
            const std::string& ){

    return DICOM_data;
}

