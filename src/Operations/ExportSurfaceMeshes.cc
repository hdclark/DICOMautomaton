//ExportSurfaceMeshes.cc - A part of DICOMautomaton 2019. Written by hal clark.

#include <asio.hpp>
#include <algorithm>
#include <optional>
#include <fstream>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <regex>
#include <set> 
#include <stdexcept>
#include <string>    
#include <utility>            //Needed for std::pair.
#include <vector>

#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorStats.h"        //Needed for Stats:: namespace.
#include "YgorString.h"       //Needed for GetFirstRegex(...)
#include "YgorMathIOOFF.h"
#include "YgorFilesDirs.h"

#include "Explicator.h"       //Needed for Explicator class.

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../Thread_Pool.h"

#include "ExportSurfaceMeshes.h"

OperationDoc OpArgDocExportSurfaceMeshes(){
    OperationDoc out;
    out.name = "ExportSurfaceMeshes";

    out.desc = 
        "This operation writes a surface mesh to a file.";

    out.args.emplace_back();
    out.args.back() = SMWhitelistOpArgDoc();
    out.args.back().name = "MeshSelection";
    out.args.back().default_val = "last";
   

    out.args.emplace_back();
    out.args.back().name = "Filename";
    out.args.back().desc = "The filename (or full path name) to which the surface mesh data should be written."
                           " The file format is an ASCII OFF model."
                           " If no name is given, unique names will be chosen automatically.";
    out.args.back().default_val = "";
    out.args.back().expected = true;
    out.args.back().examples = { "smesh.off", 
                                 "../somedir/mesh.off", 
                                 "/path/to/some/surface_mesh.off" };
    out.args.back().mimetype = "application/object-file-format"; // TODO: find correct MIME type.

    return out;
}



Drover ExportSurfaceMeshes(Drover DICOM_data, const OperationArgPkg& OptArgs, const std::map<std::string,std::string>&
/*InvocationMetadata*/, const std::string&  /*FilenameLex*/, const std::list<OperationArgPkg>& /*Children*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto MeshSelectionStr = OptArgs.getValueStr("MeshSelection").value();
    auto FilenameStr = OptArgs.getValueStr("Filename").value();

    //-----------------------------------------------------------------------------------------------------------------

    auto SMs_all = All_SMs( DICOM_data );
    auto SMs = Whitelist( SMs_all, MeshSelectionStr );
    for(auto & smp_it : SMs){
        auto FN = FilenameStr;
        if(FilenameStr.empty()){
            FN = Get_Unique_Sequential_Filename("/tmp/dicomautomaton_exportsurfacemeshes_", 6, ".off");
        }
        std::fstream FO(FN, std::fstream::out);

        if(!WriteFVSMeshToOFF( (*smp_it)->meshes, FO )){
            throw std::runtime_error("Unable to write mesh in OFF format. Cannot continue.");
        }
        FUNCINFO("Surface mesh written to '" << FN << "'");
    }

    return DICOM_data;
}
