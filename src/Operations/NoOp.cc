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
    out.tags.emplace_back("category: meta");
    out.tags.emplace_back("category: control flow");

    out.desc = 
        "This operation does nothing. It produces no side-effects.";

    return out;
}

bool NoOp(Drover&,
          const OperationArgPkg&,
          std::map<std::string, std::string>&,
          const std::string& ){

    return true;
}

