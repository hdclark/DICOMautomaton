//ExportSurfaceMeshesOFF.cc - A part of DICOMautomaton 2020. Written by hal clark.

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
#include <filesystem>

#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorLog.h"
#include "YgorStats.h"        //Needed for Stats:: namespace.
#include "YgorString.h"       //Needed for GetFirstRegex(...)
#include "YgorMathIOOFF.h"
#include "YgorFilesDirs.h"

#include "Explicator.h"       //Needed for Explicator class.

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../Thread_Pool.h"

#include "ExportSurfaceMeshesOFF.h"

OperationDoc OpArgDocExportSurfaceMeshesOFF(){
    OperationDoc out;
    out.name = "ExportSurfaceMeshesOFF";

    out.desc = 
        "This operation writes one or more surface meshes to file in Object File Format ('OFF').";

    out.notes.emplace_back(
        "Support for metadata in OFF files is currently limited. Metadata will generally be lost."
    );
    out.notes.emplace_back(
        "OFF files can contain many different types of geometry, and some software may not support"
        " the specific subset used by DICOMautomaton. For example, vertex normals may not be supported,"
        " and their presence can cause some OFF file loaders to reject valid OFF files."
        " For the best portability, consider more common formats like PLY or OBJ."
    );

    out.args.emplace_back();
    out.args.back() = SMWhitelistOpArgDoc();
    out.args.back().name = "MeshSelection";
    out.args.back().default_val = "last";
   

    out.args.emplace_back();
    out.args.back().name = "Filename";
    out.args.back().desc = "The filename (or full path name) to which the surface mesh data should be written."
                           " Existing files will not be overwritten."
                           " If an invalid or missing file extension is provided, one will automatically be added."
                           " If an empty filename is given, a unique name will be chosen automatically."
                           " If multiple meshes are selected, each will be written to a separate file;"
                           " the name of each will be derived from the user-provided filename (or default)"
                           " by appending a sequentially increasing counter between the file's stem name and extension."
                           " Files will be formatted in Object File Format ('OFF').";
    out.args.back().default_val = "";
    out.args.back().expected = true;
    out.args.back().examples = { "surface_mesh.off", 
                                 "../somedir/mesh.off", 
                                 "/path/to/some/surface_mesh.off" };
    out.args.back().mimetype = "text/plain";

    return out;
}



bool ExportSurfaceMeshesOFF(Drover &DICOM_data,
                              const OperationArgPkg& OptArgs,
                              std::map<std::string, std::string>& /*InvocationMetadata*/,
                              const std::string& /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto MeshSelectionStr = OptArgs.getValueStr("MeshSelection").value();
    auto FilenameStr = OptArgs.getValueStr("Filename").value();

    //-----------------------------------------------------------------------------------------------------------------
    const std::string required_file_extension = ".off";
    const long int n_of_digit_pads = 6;

    // Prepare filename and prototype in case multiple files need to be written.
    if(FilenameStr.empty()){
        FilenameStr = (std::filesystem::temp_directory_path() / "dicomautomaton_surfacemesh").string();
    }
    const auto suffixless_fullpath = std::filesystem::path(FilenameStr).replace_extension().string();
    FilenameStr = suffixless_fullpath + required_file_extension;


    auto SMs_all = All_SMs( DICOM_data );
    auto SMs = Whitelist( SMs_all, MeshSelectionStr );

    for(auto & smp_it : SMs){
        auto FN = FilenameStr;
        if( (1 < SMs.size()) 
        ||  std::filesystem::exists(FN) ){
            FN = Get_Unique_Sequential_Filename(suffixless_fullpath + "_", n_of_digit_pads, required_file_extension);
        }

        std::fstream FO(FN, std::fstream::out | std::ios::binary);
        if(!WriteFVSMeshToOFF( (*smp_it)->meshes, FO )){
            throw std::runtime_error("Unable to write surface mesh in OFF format. Cannot continue.");
        }
        YLOGINFO("Surface mesh written to '" << FN << "'");
    }

    return true;
}
