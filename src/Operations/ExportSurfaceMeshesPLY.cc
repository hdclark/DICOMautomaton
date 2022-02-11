//ExportSurfaceMeshesPLY.cc - A part of DICOMautomaton 2020. Written by hal clark.

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
#include "YgorMathIOPLY.h"
#include "YgorFilesDirs.h"

#include "Explicator.h"       //Needed for Explicator class.

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../Thread_Pool.h"

#include "ExportSurfaceMeshesPLY.h"

OperationDoc OpArgDocExportSurfaceMeshesPLY(){
    OperationDoc out;
    out.name = "ExportSurfaceMeshesPLY";

    out.desc = 
        "This operation writes one or more surface meshs to file in the 'Stanford' Polygon File format.";

    out.notes.emplace_back(
        "Support for metadata in OBJ files is fully supported. Surface mesh metadata will be encoded in"
        " specially-marked comments and base64 encoded if non-printable characters are present."
        " Metadata will be recovered when PLY files are loaded in DICOMautomaton."
        " Note that other software may disregard these comments."
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
                           " Files will be formatted in Stanford Polygon File ('PLY') format.";
    out.args.back().default_val = "";
    out.args.back().expected = true;
    out.args.back().examples = { "surface_mesh.ply", 
                                 "../somedir/mesh.ply", 
                                 "/path/to/some/surface_mesh.ply" };
    out.args.back().mimetype = "text/plain";


    out.args.emplace_back();
    out.args.back().name = "Variant";
    out.args.back().desc = "Controls whether files are written in the binary or ASCII PLY file format variants."
                           " Binary files will generally be smaller, and therefore faster to write,"
                           " but may be less portable."
                           " ASCII format is better suited for archival purposes, and may be more widely supported."
                           " ASCII is generally recommended unless performance or storage will be problematic.";
    out.args.back().default_val = "ascii";
    out.args.back().expected = true;
    out.args.back().examples = { "ascii", 
                                 "binary" };
    out.args.back().samples = OpArgSamples::Exhaustive;

    return out;
}



bool ExportSurfaceMeshesPLY(Drover &DICOM_data,
                              const OperationArgPkg& OptArgs,
                              std::map<std::string, std::string>& /*InvocationMetadata*/,
                              const std::string& /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto MeshSelectionStr = OptArgs.getValueStr("MeshSelection").value();
    auto FilenameStr = OptArgs.getValueStr("Filename").value();
    const auto VariantStr = OptArgs.getValueStr("Variant").value();

    //-----------------------------------------------------------------------------------------------------------------
    const auto regex_ascii = Compile_Regex("^as?c?i?i?$");
    const auto regex_binary = Compile_Regex("^bi?n?a?r?y?$");
    bool as_binary = false;
    if(false){
    }else if(std::regex_match(VariantStr, regex_ascii)){
        as_binary = false;
    }else if(std::regex_match(VariantStr, regex_binary)){
        as_binary = true;
    }else{
        throw std::invalid_argument("Variant not understood. Refusing to continue.");
    }

    const std::string required_file_extension = ".ply";
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

        std::fstream FO(FN, std::fstream::out | std::ios::binary );
        if(!WriteFVSMeshToPLY( (*smp_it)->meshes, FO, as_binary )){
            throw std::runtime_error("Unable to write surface mesh in PLY format. Cannot continue.");
        }
        FUNCINFO("Surface mesh written to '" << FN << "'");
    }

    return true;
}
