//SelectDirectory.cc - A part of DICOMautomaton 2024. Written by hal clark.

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
#include "../Dialogs/Selectors.h"

#include "SelectDirectory.h"


OperationDoc OpArgDocSelectDirectory(){
    OperationDoc out;
    out.name = "SelectDirectory";
    out.tags.emplace_back("category: meta");
    out.tags.emplace_back("category: interactive");
    out.tags.emplace_back("category: parameter table");

    out.aliases.push_back("SpecifyDirectory");
    out.aliases.push_back("SelectFolder");
    out.aliases.push_back("SpecifyFolder");

    out.notes.emplace_back(
        "Invocation of this operation will first purge any existing directory names with the provided key."
        " This is done to avoid risk of any existing directory names being propagated through if the"
        " selection dialog fails or the user cancels the dialog."
    );
    out.notes.emplace_back(
        "The specified directory name is not validated. However, providing an empty name will cause a"
        " false to be returned."
    );

    out.desc = 
        "Allow the user to interactively select/specify a directory name, and then insert it into the"
        " global parameter table.";

    out.args.emplace_back();
    out.args.back().name = "Key";
    out.args.back().desc = "The key used to store the selected/specified directory name in the global parameter table."
                           "";
    out.args.back().default_val = "selected_dirname";
    out.args.back().expected = true;
    out.args.back().examples = { "selected_dirname",
                                 "out_dirname",
                                 "value" };

    return out;
}

bool SelectDirectory(Drover& /*DICOM_data*/,
                     const OperationArgPkg& OptArgs,
                     std::map<std::string, std::string>& InvocationMetadata,
                     const std::string& /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto KeyStr = OptArgs.getValueStr("Key").value();

    //-----------------------------------------------------------------------------------------------------------------
    // Do this *prior* to checking the provided key's syntax.
    //
    // If invalid, there should be no key anyways, and throwing prior to clearing the key from the table could
    // conceivably result in files being overwritten / data loss.
    InvocationMetadata.erase(KeyStr);

    if(KeyStr.empty()){
        throw std::invalid_argument("No key provided for directory name storage");
    }

    const std::string query_text = "Select directory...";

    // Wait for the user to provide input.
    const std::string selection = select_directory(query_text);

    bool ret = true;
    if(selection.empty()){
        YLOGINFO("No selection provided, not inserting key '" << KeyStr << "' into parameter table");
        ret = false;

    }else{
        YLOGINFO("Adding entry '" << KeyStr << "' = '" << selection << "' to global parameter table");
        InvocationMetadata[KeyStr] = selection;

        ret = true;
    }

    return ret;
}

