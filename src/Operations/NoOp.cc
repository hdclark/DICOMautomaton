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

bool NoOp(Drover &DICOM_data,
            const OperationArgPkg&,
            std::map<std::string, std::string>& /*InvocationMetadata*/,
            const std::string& ){

    return true;
}

