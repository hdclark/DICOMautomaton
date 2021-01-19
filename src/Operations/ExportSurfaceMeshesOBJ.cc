//ExportSurfaceMeshesOBJ.cc - A part of DICOMautomaton 2020. Written by hal clark.

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
#include <filesystem>

#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorStats.h"        //Needed for Stats:: namespace.
#include "YgorString.h"       //Needed for GetFirstRegex(...)
#include "YgorMathIOOBJ.h"
#include "YgorFilesDirs.h"

#include "Explicator.h"       //Needed for Explicator class.

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../Thread_Pool.h"

#include "ExportSurfaceMeshesOBJ.h"

OperationDoc OpArgDocExportSurfaceMeshesOBJ(){
    OperationDoc out;
    out.name = "ExportSurfaceMeshesOBJ";

    out.desc = 
        "This operation writes one or more surface meshes to file in Wavefront Object ('OBJ') format.";

    out.notes.emplace_back(
        "Support for metadata in OBJ files is currently limited. Metadata will generally be lost."
    );
    out.notes.emplace_back(
        "OBJ files can refer to MTL 'sidecar' files for information about materials and various properties."
        " MTL files are not supported at this time."
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
                           " Files will be formatted in ASCII Wavefront Object ('OBJ') format.";
    out.args.back().default_val = "";
    out.args.back().expected = true;
    out.args.back().examples = { "surface_mesh.obj", 
                                 "../somedir/mesh.obj", 
                                 "/path/to/some/surface_mesh.obj" };
    //out.args.back().mimetype = "application/object-file-format";
    out.args.back().mimetype = "model/obj";

    return out;
}



Drover ExportSurfaceMeshesOBJ(Drover DICOM_data,
                              const OperationArgPkg& OptArgs,
                              const std::map<std::string, std::string>& /*InvocationMetadata*/,
                              const std::string& /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto MeshSelectionStr = OptArgs.getValueStr("MeshSelection").value();
    auto FilenameStr = OptArgs.getValueStr("Filename").value();

    //-----------------------------------------------------------------------------------------------------------------
    const std::string required_file_extension = ".obj";
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
        if(!WriteFVSMeshToOBJ( (*smp_it)->meshes, FO )){
            throw std::runtime_error("Unable to write surface mesh in OBJ format. Cannot continue.");
        }
        FUNCINFO("Surface mesh written to '" << FN << "'");
    }

    return DICOM_data;
}
