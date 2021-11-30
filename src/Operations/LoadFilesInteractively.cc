//LoadFilesInteractively.cc - A part of DICOMautomaton 2021. Written by hal clark.

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

#include "Explicator.h"       //Needed for Explicator class.

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../Thread_Pool.h"
#include "../Write_File.h"
#include "../File_Loader.h"
#include "../Operation_Dispatcher.h"
#include "../Dialogs.h"

#include "LoadFilesInteractively.h"


OperationDoc OpArgDocLoadFilesInteractively(){
    OperationDoc out;
    out.name = "LoadFilesInteractively";

    out.desc = 
        "This operation lets the user interactively select one or more files and then attempts to load them.";
        
    out.notes.emplace_back(
        "This operation requires all files provided to it to exist and be accessible."
        " Inaccessible files are not silently ignored and will cause this operation to fail."
    );
        
    out.args.emplace_back();
    out.args.back().name = "Instruction";
    out.args.back().desc = "An instruction provided to the user, if possible. In most cases this will be the"
                           " title of a GUI dialog box.";
    out.args.back().default_val = "Please select one or more files to load";
    out.args.back().expected = true;
    out.args.back().examples = { "Select files", "Select XYZ files to load" };

    return out;
}

bool LoadFilesInteractively(Drover &DICOM_data,
                            const OperationArgPkg& OptArgs,
                            std::map<std::string, std::string>& InvocationMetadata,
                            const std::string& FilenameLex){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto InstructionStr = OptArgs.getValueStr("Instruction").value();

    //-----------------------------------------------------------------------------------------------------------------

    // Query the user and block execution until input is received.
    select_files sf(InstructionStr);
    while( !sf.is_ready() ){ }

    std::list<std::filesystem::path> Paths;
    for(const auto &f : sf.get_selection()){
        FUNCINFO("Attempting to load file '" << f << "'");
        std::filesystem::path PathShuttle;
        bool wasOK = false;
        try{
            PathShuttle = std::filesystem::canonical(f);
            wasOK = std::filesystem::exists(PathShuttle);
        }catch(const std::filesystem::filesystem_error &){ }

        if(wasOK){
            Paths.emplace_back(PathShuttle);
        }else{
            std::stringstream ss;
            ss << "Unable to resolve file or directory '" << f << "'. Refusing to continue." << std::endl;
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

