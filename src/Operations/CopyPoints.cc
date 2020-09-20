//CopyPoints.cc - A part of DICOMautomaton 2019. Written by hal clark.

#include <deque>
#include <optional>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>    

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "CopyPoints.h"

OperationDoc OpArgDocCopyPoints(){
    OperationDoc out;
    out.name = "CopyPoints";

    out.desc = 
        "This operation deep-copies the selected point clouds.";

    out.args.emplace_back();
    out.args.back() = PCWhitelistOpArgDoc();
    out.args.back().name = "PointSelection";
    out.args.back().default_val = "last";
    
    return out;
}

Drover CopyPoints(Drover DICOM_data,
                  const OperationArgPkg& OptArgs,
                  const std::map<std::string, std::string>&,
                  const std::string&){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto PointSelectionStr = OptArgs.getValueStr("PointSelection").value();

    //-----------------------------------------------------------------------------------------------------------------

    //Gather a list of pclouds to copy.
    std::deque<std::shared_ptr<Point_Cloud>> pclouds_to_copy;

    auto PCs_all = All_PCs( DICOM_data );
    auto PCs = Whitelist( PCs_all, PointSelectionStr );
    for(auto & pcp_it : PCs){
        pclouds_to_copy.push_back(*pcp_it);
    }

    //Copy the pclouds.
    for(auto & pcp : pclouds_to_copy){
        DICOM_data.point_data.emplace_back( std::make_shared<Point_Cloud>( *pcp ) );
    }

    return DICOM_data;
}
