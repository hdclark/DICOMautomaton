//DeletePoints.cc - A part of DICOMautomaton 2019. Written by hal clark.

#include <cmath>
#include <cstdlib>            //Needed for exit() calls.
#include <optional>
#include <list>
#include <map>
#include <memory>
#include <regex>
#include <string>    

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "DeletePoints.h"
#include "YgorMath.h"         //Needed for vec3 class.



OperationDoc OpArgDocDeletePoints(){
    OperationDoc out;
    out.name = "DeletePoints";
    out.tags.emplace_back("category: point cloud processing");

    out.desc = 
        "This routine deletes point clouds from memory."
        " It is most useful when working with positional operations in stages.";

    out.args.emplace_back();
    out.args.back() = PCWhitelistOpArgDoc();
    out.args.back().name = "PointSelection";
    out.args.back().default_val = "last";

    return out;
}



bool DeletePoints(Drover &DICOM_data,
                    const OperationArgPkg& OptArgs,
                    std::map<std::string, std::string>& /*InvocationMetadata*/,
                    const std::string&){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto PointSelectionStr = OptArgs.getValueStr("PointSelection").value();

    //-----------------------------------------------------------------------------------------------------------------
    auto PCs_all = All_PCs( DICOM_data );
    auto PCs = Whitelist( PCs_all, PointSelectionStr );
    for(auto & pcp_it : PCs){
        DICOM_data.point_data.erase( pcp_it );
    }

    return true;
}
