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
#include "../Alignment_TPSRPM.h"
#include "../Regex_Selectors.h"
#include "../Thread_Pool.h"

#include "ExportWarps.h"

OperationDoc OpArgDocExportWarps(){
    OperationDoc out;
    out.name = "ExportWarps";

    out.desc = 
        "This operation exports a vector-valued transformation (e.g., a deformation) to a file.";
        
    out.args.emplace_back();
    out.args.back() = T3WhitelistOpArgDoc();
    out.args.back().name = "TransformSelection";
    out.args.back().default_val = "last";
    out.args.back().desc = "The transformation that will be applied. "_s
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
                                 "trans.txt",
                                 "/path/to/some/trans.txt" };
    out.args.back().mimetype = "text/plain";

    return out;
}



Drover ExportWarps(Drover DICOM_data, OperationArgPkg OptArgs, std::map<std::string,std::string> /*InvocationMetadata*/, std::string /*FilenameLex*/){

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

        std::visit([&](auto && t){
            using V = std::decay_t<decltype(t)>;
            if constexpr (std::is_same_v<V, std::monostate>){
                throw std::invalid_argument("Transformation is invalid. Unable to continue.");

            // Affine transformations.
            }else if constexpr (std::is_same_v<V, affine_transform<double>>){
                FUNCINFO("Exporting affine transformation now");
                if(!(t.write_to(FO))){
                    std::runtime_error("Unable to write to file. Cannot continue.");
                }

            // Thin-plate spline transformations.
            }else if constexpr (std::is_same_v<V, thin_plate_spline>){
                FUNCINFO("Exporting thin-plate spline transformation now");
                if(!(t.write_to(FO))){
                    std::runtime_error("Unable to write to file. Cannot continue.");
                }

            }else{
                static_assert(std::is_same_v<V,void>, "Transformation not understood.");
            }
            return;
        }, (*t3p_it)->transform);
    }
 
    return DICOM_data;
}
