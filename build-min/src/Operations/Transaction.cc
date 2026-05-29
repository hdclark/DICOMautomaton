//Transaction.cc - A part of DICOMautomaton 2023. Written by hal clark.

#include <any>
#include <optional>
#include <functional>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>    

#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorString.h"       //Needed for GetFirstRegex(...)
#include "YgorStats.h"

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../Operation_Dispatcher.h"

#include "Transaction.h"



OperationDoc OpArgDocTransaction(){
    OperationDoc out;
    out.name = "Transaction";

    out.tags.emplace_back("category: meta");
    out.tags.emplace_back("category: control flow");

    out.desc = 
        "This operation will make a snapshot of the internal data state and execute children operations."
        " If any child operation fails or returns false, the state will be reset to the snapshot and the remaining"
        " children operations will not be executed."
        " If all children operations succeed, any modifications to the modified state will be committed"
        " automatically when the final operation succeeds, and the snapshot will be discarded.";

    out.notes.emplace_back(
        "This operation only transacts the internal state of the Drover object and the parameter table."
        " Any side-effects caused by the children operations, such as modifying files, appending to logs, interaction"
        " with terminals/consoles, or interacting over networks, will not be transacted."
        " Side-effects will therefore be committed immediately, regardless of whether the transaction succeeds."
    );
    out.notes.emplace_back(
        "This operation duplicates the full internal data state, so can be memory-intensive."
    );

    return out;
}



bool Transaction(Drover &DICOM_data,
                 const OperationArgPkg& OptArgs,
                 std::map<std::string, std::string>& InvocationMetadata,
                 const std::string& FilenameLex){

    //---------------------------------------------- User Parameters --------------------------------------------------
    // ... nothing to load ...

    //-----------------------------------------------------------------------------------------------------------------

    auto children = OptArgs.getChildren();
    if(children.empty()){
        YLOGWARN("No children operations specified, forgoing transaction");
    }else{

        // Copy Drover and other relevant internal state.
        const auto orig_DICOM_data = DICOM_data.Deep_Copy();
        const auto orig_InvocationMetadata = InvocationMetadata;

        // Perform children operations.
        const bool res = Operation_Dispatcher(DICOM_data, InvocationMetadata, FilenameLex, children);
        if(res){
            YLOGINFO("Transaction succeeding. Committing state");
            
        }else{
            YLOGWARN("Transaction failed. Reverting state");
            DICOM_data = orig_DICOM_data;
            InvocationMetadata = orig_InvocationMetadata;
            return false;
        }
    }

    return true;
}
