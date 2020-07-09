//DeleteMeshes.cc - A part of DICOMautomaton 2019. Written by hal clark.

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
#include "DeleteMeshes.h"
#include "YgorMath.h"         //Needed for vec3 class.



OperationDoc OpArgDocDeleteMeshes(){
    OperationDoc out;
    out.name = "DeleteMeshes";
    out.desc = 
        "This routine deletes surface meshes from memory."
        " It is most useful when working with positional operations in stages.";

    out.args.emplace_back();
    out.args.back() = SMWhitelistOpArgDoc();
    out.args.back().name = "MeshSelection";
    out.args.back().default_val = "last";

    return out;
}



Drover DeleteMeshes(Drover DICOM_data, OperationArgPkg OptArgs, std::map<std::string,std::string>, std::string ){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto MeshSelectionStr = OptArgs.getValueStr("MeshSelection").value();

    //-----------------------------------------------------------------------------------------------------------------
    auto SMs_all = All_SMs( DICOM_data );
    auto SMs = Whitelist( SMs_all, MeshSelectionStr );
    for(auto & smp_it : SMs){
        DICOM_data.smesh_data.erase( smp_it );
    }

    return DICOM_data;
}
