//CopyWarps.cc - A part of DICOMautomaton 2025. Written by hal clark.

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
#include "CopyWarps.h"

OperationDoc OpArgDocCopyWarps(){
    OperationDoc out;
    out.name = "CopyWarps";
    out.aliases.emplace_back("CopyTransforms");

    out.tags.emplace_back("category: spatial transform processing");

    out.desc = 
        "This operation deep-copies the selected spatial transformations (i.e., warps).";

    out.args.emplace_back();
    out.args.back() = T3WhitelistOpArgDoc();
    out.args.back().name = "TransformSelection";
    out.args.back().default_val = "last";
    
    return out;
}

bool CopyWarps(Drover &DICOM_data,
               const OperationArgPkg& OptArgs,
               std::map<std::string, std::string>& /*InvocationMetadata*/,
               const std::string&){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto TransformSelectionStr = OptArgs.getValueStr("TransformSelection").value();

    //-----------------------------------------------------------------------------------------------------------------

    //Gather a list of transforms to copy.
    std::deque<std::shared_ptr<Transform3>> transforms_to_copy;

    auto T3s_all = All_T3s( DICOM_data );
    auto T3s = Whitelist( T3s_all, TransformSelectionStr );
    for(auto & t3_it : T3s){
        transforms_to_copy.push_back(*t3_it);
    }

    //Copy the transforms.
    for(auto & t3p : transforms_to_copy){
        DICOM_data.trans_data.emplace_back( std::make_shared<Transform3>( *t3p ) );
    }

    return true;
}
