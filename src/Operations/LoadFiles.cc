//LoadFiles.cc - A part of DICOMautomaton 2019, 2021. Written by hal clark.

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

#include "Explicator.h"       //Needed for Explicator class.

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../Thread_Pool.h"
#include "../Write_File.h"
#include "../File_Loader.h"
#include "../Operation_Dispatcher.h"

#include "LoadFiles.h"


OperationDoc OpArgDocLoadFiles(){
    OperationDoc out;
    out.name = "LoadFiles";

    out.desc = 
        "This operation loads files on-the-fly.";
        
    out.notes.emplace_back(
        "This operation requires all files provided to it to exist and be accessible."
        " Inaccessible files are not silently ignored and will cause this operation to fail."
    );
        
    out.args.emplace_back();
    out.args.back().name = "FileName";
    out.args.back().desc = "This file will be parsed and loaded."
                           " All file types supported by the DICOMautomaton system can be loaded in this way."
                           " Currently this includes serialized Drover class files, DICOM files,"
                           " FITS image files, and XYZ point cloud files.";
    out.args.back().default_val = "";
    out.args.back().expected = true;
    out.args.back().examples = { "/tmp/image.dcm", "rois.dcm", "dose.dcm", "image.fits", "point_cloud.xyz" };

    return out;
}

bool LoadFiles(Drover &DICOM_data,
                 const OperationArgPkg& OptArgs,
                 std::map<std::string, std::string>& InvocationMetadata,
                 const std::string& FilenameLex){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto FileNameStr = OptArgs.getValueStr("FileName").value();

    //-----------------------------------------------------------------------------------------------------------------

    std::list<std::string> FileNames = { { FileNameStr } };
    std::list<std::filesystem::path> Paths;

    // Prepare file paths.
    std::filesystem::path PathShuttle;
    for(const auto &auri : FileNames){
        bool wasOK = false;
        try{
            PathShuttle = std::filesystem::canonical(auri);
            wasOK = std::filesystem::exists(PathShuttle);
        }catch(const std::filesystem::filesystem_error &){ }

        if(wasOK){
            Paths.emplace_back(auri);
        }else{
            std::stringstream ss;
            ss << "Unable to resolve file or directory '" << auri << "'. Refusing to continue." << std::endl;
            throw std::invalid_argument(ss.str().c_str());
        }
    }
    
    // Load the files to a placeholder Drover class.
    Drover DD_work;
    std::map<std::string, std::string> dummy;
    std::list<OperationArgPkg> Operations;
    const auto res = Load_Files(DD_work, dummy, FilenameLex, Operations, Paths);
    if(!res){
        throw std::runtime_error("Unable to load one or more files. Refusing to continue.");
    }

    // Merge the loaded files into the current Drover class.
    DICOM_data.Consume(DD_work);

    // Because we can't inject the loaded operations directly into the global operation list, we instead treat them as
    // children and execute them locally.
    if(!Operations.empty()
    && !Operation_Dispatcher(DICOM_data, InvocationMetadata, FilenameLex, Operations)){
        throw std::runtime_error("Loaded operation failed");
    }

    return true;
}

