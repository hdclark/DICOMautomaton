//MapTableToParameters.cc - A part of DICOMautomaton 2024. Written by hal clark.

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
#include "../Metadata.h"
#include "../Thread_Pool.h"
#include "../Write_File.h"
#include "../String_Parsing.h"
#include "../Operation_Dispatcher.h"

#include "MapTableToParameters.h"


OperationDoc OpArgDocMapTableToParameters(){
    OperationDoc out;
    out.name = "MapTableToParameters";

    out.desc = 
        "Process the rows of a table, one-at-a-time, by mapping them to the global parameter table"
        " and invoking children operations.";

    out.args.emplace_back();
    out.args.back().name = "KeyPrefix";
    out.args.back().desc = "Used to map columns to the global parameter table. This string will prefix"
                           " the parameter table keys; an identifier will be appended for each column."
                           "\n\n"
                           "Note that any metadata keys that inadvertently match the mapping will be"
                           " stowed while children operations are being invoked, and reset afterward."
                           " All other metadata, including metadata added by children, are unaffected."
                           "";
    out.args.back().default_val = "_column_";
    out.args.back().expected = true;
    out.args.back().examples = { "key_", "mapped", "xyz" };


    out.args.emplace_back();
    out.args.back() = STWhitelistOpArgDoc();
    out.args.back().name = "TableSelection";
    out.args.back().default_val = "last";


//    out.args.emplace_back();
//    out.args.back().name = "IgnoreHeader";
//    out.args.back().desc = "If 'true', the first non-empty row is assumed to contain a header and is skipped."
//                           "";
//    out.args.back().default_val = "false";
//    out.args.back().expected = true;
//    out.args.back().examples = { "true", "false" };
//    out.args.back().samples = OpArgSamples::Exhaustive;
//
//
//    out.args.emplace_back();
//    out.args.back().name = "AccessMode";
//    out.args.back().desc = "Controls whether the table can be modified."
//                           "\n\n"
//                           "If 'read-only', then the table will ignore any modifications made to the mapped"
//                           " cell contents held in the global parameter table."
//                           "\n\n"
//                           "If 'read-write', modifications to the global parameter table will be reflected"
//                           " in the table."
//                           "";
//    out.args.back().default_val = "false";
//    out.args.back().expected = true;
//    out.args.back().examples = { "true", "false" };
//    out.args.back().samples = OpArgSamples::Exhaustive;


    return out;
}

bool MapTableToParameters(Drover& DICOM_data,
                          const OperationArgPkg& OptArgs,
                          std::map<std::string, std::string>& InvocationMetadata,
                          const std::string& FilenameLex){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto KeyPrefixStr = OptArgs.getValueStr("KeyPrefix").value();
    const auto TableSelectionStr = OptArgs.getValueStr("TableSelection").value();
//    const auto IgnoreHeaderStr = OptArgs.getValueStr("IgnoreHeader").value();
//    const auto AccessModeStr = OptArgs.getValueStr("AccessMode").value();

    //-----------------------------------------------------------------------------------------------------------------
    const auto full_key_prefix = KeyPrefixStr + "_"_s;
    const auto N_full_key_prefix = full_key_prefix.size();

//    const auto regex_true = Compile_Regex("^tr?u?e?$");
//    const auto regex_false = Compile_Regex("^fa?l?s?e?$");

    // Select or create a table.
    auto STs_all = All_STs( DICOM_data );
    auto STs = Whitelist( STs_all, TableSelectionStr );


    // Encoding and decoding of keys in the parameter table.
    const auto encode_cell_key = [&](int64_t i) -> std::string {
        return full_key_prefix + std::to_string(i);
    };
    const auto decode_cell_key = [&](const std::string &s) -> std::optional<int64_t> {
        std::optional<int64_t> out;

        const auto pos = s.find( full_key_prefix );
        if( (pos == static_cast<std::string::size_type>(0))
        &&  (N_full_key_prefix < s.size()) ){
            try{
                out = std::stoll(s.substr(pos + N_full_key_prefix));
            }catch(const std::exception &){};
        }
        return out;
    };

    // ====== Metadata stowing =======

    // Data and closure for deferred key-value garbage removal.
    std::set<std::string> keys_to_erase;
    const auto erase_marked_keys = [&](){
        for(const auto& k : keys_to_erase){
            InvocationMetadata.erase(k);
        }
        keys_to_erase.clear();
        return;
    };

    // Stow a copy of all matching keys currently in the parameter table, including keys that
    // will match later. Remove them from the parameter table and only restore at the end.
    //
    // TODO: Move this to a class accepting a user-provided unary functor controlling whether to stow?
    std::map<std::string, std::optional<std::string>> stowed_kvps;
    for(const auto& kvp : InvocationMetadata){
        const auto key = kvp.first;
        const auto col_opt = decode_cell_key(key);
        if(col_opt){ // If we can decode a column number, this key interferes.
            stowed_kvps[key] = kvp.second;
            keys_to_erase.insert(key);
        }
    }
    erase_marked_keys();

    // Restore stowed metadata.
    //
    // TODO: Move this to a class for automatic restoration via RAII?
    const auto restore_stowed_metadata = [&](){
        for(const auto &kvp : stowed_kvps){
            const auto& key = kvp.first;
            const auto& val_opt = kvp.second;

            InvocationMetadata.erase(key);
            if(val_opt){
                InvocationMetadata[key] = val_opt.value();
            }
        }
        return;
    };
    // ===========================


    bool ret = true;
    for(auto & stp_it : STs){
        tables::table2& t = (*stp_it)->table;
        try{
            const auto mmr = t.min_max_row();

            // Process rows one-at-a-time.
            for(auto r = mmr.first; r <= mmr.second; ++r){
                // Recompute the column number bounding box each row in case additional columns were added.
                const auto mmc = t.min_max_col();

                // Insert the cell contents into the parameter table.
                for(auto c = mmc.first; c <= mmc.second; ++c){
                    const auto key = encode_cell_key(c);
                    const auto cell_val = t.value(r,c);

                    InvocationMetadata.erase(key);
                    if(cell_val){
                        InvocationMetadata[key] = cell_val.value();
                    }
                }

                // Invoke children.
                auto children = OptArgs.getChildren();
                if(!children.empty()){
                    ret = Operation_Dispatcher(DICOM_data, InvocationMetadata, FilenameLex, children);
                }

                // Extract cell contents from the parameter table.
                //
                // First, detect modifications, insertions, and deletions of the cells that were encoded.
                for(auto c = mmc.first; c <= mmc.second; ++c){
                    const auto key = encode_cell_key(c);
                    const auto val_opt = get_as<std::string>(InvocationMetadata, key);
                    t.remove(r,c);
                    if(val_opt){
                        t.inject(r, c, val_opt.value());
                        keys_to_erase.insert(key);
                    }
                }
                erase_marked_keys();

                // Next, detect insertions of cells that were not encoded,
                // i.e., new column entries that were added out of the prior column bounds.
                for(const auto &kvp : InvocationMetadata){
                    const auto &key = kvp.first;
                    const auto col_opt = decode_cell_key(key);
                    if(col_opt){
                        t.inject(r, col_opt.value(), kvp.second);
                        keys_to_erase.insert(key);
                    }
                }
                erase_marked_keys();

                if(!ret) break;
            }

        }catch(const std::exception &e){
            YLOGWARN("Unable to map parameters: " << e.what());
            ret = false;
            break;
        }
    }

    restore_stowed_metadata();

    return ret;
}

