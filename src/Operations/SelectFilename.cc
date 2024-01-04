//SelectFilename.cc - A part of DICOMautomaton 2024. Written by hal clark.

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
#include "../Dialogs.h"

#include "SelectFilename.h"


OperationDoc OpArgDocSelectFilename(){
    OperationDoc out;
    out.name = "SelectFilename";
    out.aliases.push_back("SpecifyFilename");

    out.notes.emplace_back(
        "Invocation of this operation will first purge any existing filename with the provided key."
        " This is done to avoid risk of any existing filenames being propagated through if the file"
        " selection fails or the user cancels the dialog."
    );
    out.notes.emplace_back(
        "As with any non-atomic filename selection operation where the file is not reserved, there"
        " is a possible race condition between filename selection and file use. This is broadly known"
        " as the `TOCTOU` or time-of-check, time-of-use race condition. Beware!"
    );

    out.desc = 
        "Allow the user to interactively select/specify a filename, and then insert it into the global parameter table.";

    out.args.emplace_back();
    out.args.back().name = "Key";
    out.args.back().desc = "The key used to store the selected/specified filename in the global parameter table."
                           "\n"
                           "A file with this name does not need to exist, and no file is created by this operation."
                           " If a file does exist with the specified filename, it will not be modified by this"
                           " operation. However, subsequent operations may use the filename to create, overwrite,"
                           " or append such a file."
                           "";
    out.args.back().default_val = "selected_filename";
    out.args.back().expected = true;
    out.args.back().examples = { "selected_filename",
                                 "out_filename",
                                 "value" };

    out.args.emplace_back();
    out.args.back().name = "Extension";
    out.args.back().desc = "An extension to impose on the filename. Note that this option will add the extension"
                           " or override an extension provided by the user."
                           "\n"
                           "To permit any extension and disable overriding the extension, leave this option empty."
                           "";
    out.args.back().default_val = "";
    out.args.back().expected = true;
    out.args.back().examples = { "",
                                 ".dcm",
                                 ".txt",
                                 ".tar.gz",
                                 ".CSV" };

    return out;
}

bool SelectFilename(Drover& /*DICOM_data*/,
                    const OperationArgPkg& OptArgs,
                    std::map<std::string, std::string>& InvocationMetadata,
                    const std::string& /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto KeyStr = OptArgs.getValueStr("Key").value();
    const auto ExtensionStr = OptArgs.getValueStr("Extension").value();

    //-----------------------------------------------------------------------------------------------------------------
    // Do this *prior* to checking the provided key's syntax.
    //
    // If invalid, there should be no key anyways, and throwing prior to clearing the key from the table could
    // conceivably result in files being overwritten / data loss.
    InvocationMetadata.erase(KeyStr);

    if(KeyStr.empty()){
        throw std::invalid_argument("No key provided for filename storage");
    }

    // Create a dialog box.
    //std::filesystem::path open_file_root = std::filesystem::current_path();
    std::optional<select_filename> selector_opt;
    if(!selector_opt){
       selector_opt.emplace("Select file path and name"_s);
    }

    // Wait for the user to provide input.
    //
    // Note: the following blocks by continuous polling.
    std::string selection = selector_opt.value().get_selection();
    selector_opt.reset();

    if(selection.empty()){
        YLOGINFO("No selection provided, not inserting key '" << KeyStr << "' into parameter table");
    }else{
        if(!ExtensionStr.empty()){
            selection = std::filesystem::path(selection).replace_extension(ExtensionStr).string();
        }
        InvocationMetadata[KeyStr] = selection;
        YLOGINFO("Adding entry '" << KeyStr << "' = '" << selection << "' to global parameter table");
    }

    return true;
}

