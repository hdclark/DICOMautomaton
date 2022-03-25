//ExportWarps.cc - A part of DICOMautomaton 2020. Written by hal clark.

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
#include <variant>

#include "Explicator.h"       //Needed for Explicator class.

#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorStats.h"        //Needed for Stats:: namespace.
#include "YgorString.h"       //Needed for GetFirstRegex(...)
#include "YgorFilesDirs.h"

#include "../Structs.h"
#include "../Alignment_Rigid.h"
#include "../Alignment_TPSRPM.h"
#include "../Alignment_Field.h"
#include "../Regex_Selectors.h"
#include "../Thread_Pool.h"
#include "../Transformation_File_Loader.h"

#include "ExportWarps.h"

OperationDoc OpArgDocExportWarps(){
    OperationDoc out;
    out.name = "ExportWarps";

    out.desc = 
        "This operation exports a transform object (e.g., affine matrix, vector deformation field) to file.";
        
    out.args.emplace_back();
    out.args.back() = T3WhitelistOpArgDoc();
    out.args.back().name = "TransformSelection";
    out.args.back().default_val = "last";
    out.args.back().desc = "The transformation that will be exported. "_s
                         + out.args.back().desc;


    out.args.emplace_back();
    out.args.back().name = "Filename";
    out.args.back().desc = "The filename (or full path name) to which the transformation should be written."
                           " Existing files will be overwritten."
                           " The file format is a 4x4 Affine matrix."
                           " If no name is given, a unique name will be chosen automatically.";
    out.args.back().default_val = "";
    out.args.back().expected = true;
    out.args.back().examples = { "transformation.trans",
                                 "affine.trans",
                                 "/path/to/some/mapping.trans" };
    out.args.back().mimetype = "text/plain";

    return out;
}



bool ExportWarps(Drover &DICOM_data,
                   const OperationArgPkg& OptArgs,
                   std::map<std::string, std::string>& /*InvocationMetadata*/,
                   const std::string& /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto TFormSelectionStr = OptArgs.getValueStr("TransformSelection").value();

    const auto FilenameStr = OptArgs.getValueStr("Filename").value();
    //-----------------------------------------------------------------------------------------------------------------

    auto T3s_all = All_T3s( DICOM_data );
    auto T3s = Whitelist( T3s_all, TFormSelectionStr );
    FUNCINFO(T3s.size() << " transformations selected");

    // I can't think of a better way to handle multiple outputs right now. Maybe a TAR archive?? Disallowing for now...
    if(T3s.size() != 1){
        throw std::invalid_argument("Selection of only a single transformation is currently supported. Refusing to continue.");
    }

    for(auto & t3p_it : T3s){
        // Determine which filename to use.
        auto FN = FilenameStr;
        if(FN.empty()){
            FN = Get_Unique_Sequential_Filename("/tmp/dcma_export_warps_", 6, ".trans");
        }
        std::fstream FO(FN, std::fstream::out);
        if(!WriteTransform3(*(*t3p_it), FO)){
             std::runtime_error("Unable to write to file. Cannot continue.");
        }
    }
 
    return true;
}
