//CopyMeshes.cc - A part of DICOMautomaton 2019. Written by hal clark.

#include <deque>
#include <experimental/optional>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>    

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "CopyMeshes.h"

OperationDoc OpArgDocCopyMeshes(void){
    OperationDoc out;
    out.name = "CopyMeshes";

    out.desc = 
        "This operation deep-copies the selected surface meshes.";

    out.args.emplace_back();
    out.args.back() = SMWhitelistOpArgDoc();
    out.args.back().name = "MeshSelection";
    out.args.back().default_val = "last";
    
    return out;
}

Drover CopyMeshes(Drover DICOM_data, OperationArgPkg OptArgs, std::map<std::string,std::string> , std::string){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto MeshSelectionStr = OptArgs.getValueStr("MeshSelection").value();

    //-----------------------------------------------------------------------------------------------------------------

    //Gather a list of meshes to copy.
    std::deque<std::shared_ptr<Surface_Mesh>> smeshes_to_copy;

    auto SMs_all = All_SMs( DICOM_data );
    auto SMs = Whitelist( SMs_all, MeshSelectionStr );
    for(auto & smp_it : SMs){
        smeshes_to_copy.push_back(*smp_it);
    }

    //Copy the meshes.
    for(auto & smp : smeshes_to_copy){
        DICOM_data.smesh_data.emplace_back( std::make_shared<Surface_Mesh>( *smp ) );
    }

    return DICOM_data;
}
