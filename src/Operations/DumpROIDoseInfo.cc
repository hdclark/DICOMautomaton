//DumpROIDoseInfo.cc - A part of DICOMautomaton 2016. Written by hal clark.

#include <optional>
#include <fstream>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>    
#include <utility>            //Needed for std::pair.

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "DumpROIDoseInfo.h"
#include "Explicator.h"       //Needed for Explicator class.


OperationDoc OpArgDocDumpROIDoseInfo(void){
    OperationDoc out;
    out.name = "DumpROIDoseInfo";

    out.desc = 
        " This operation computes mean voxel doses with the given ROIs.";

    out.args.emplace_back();
    out.args.back().name = "ROILabelRegex";
    out.args.back().desc = "A regex matching ROI labels/names to consider. The default will match"
                      " all available ROIs. Be aware that input spaces are trimmed to a single space."
                      " If your ROI name has more than two sequential spaces, use regex to avoid them."
                      " All ROIs have to match the single regex, so use the 'or' token if needed."
                      " Regex is case insensitive and uses grep syntax.";
    out.args.back().default_val = ".*";
    out.args.back().expected = true;
    out.args.back().examples = { ".*", ".*body.*", "body", "Gross_Liver", 
                            R"***(.*parotid.*|.*sub.*mand.*)***", 
                            R"***(left_parotid|right_parotid|eyes)***" };


    return out;
}

Drover DumpROIDoseInfo(Drover DICOM_data, OperationArgPkg OptArgs, std::map<std::string,std::string> /*InvocationMetadata*/, std::string FilenameLex){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ROILabelRegex = OptArgs.getValueStr("ROILabelRegex").value();
    //-----------------------------------------------------------------------------------------------------------------
    const auto theregex = Compile_Regex(ROILabelRegex);

    Explicator X(FilenameLex);

    FUNCERR("This operation has been removed");

    return DICOM_data;
}
