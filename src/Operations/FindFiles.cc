//FindFiles.cc - A part of DICOMautomaton 2024-2026. Written by hal clark.

#include <algorithm>
#include <chrono>
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
#include "YgorTime.h"         //Needed for time_mark class.

#include "Explicator.h"       //Needed for Explicator class.

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../Metadata.h"
#include "../Thread_Pool.h"
#include "../Write_File.h"
#include "../String_Parsing.h"
#include "../Operation_Dispatcher.h"

#include "FindFiles.h"


OperationDoc OpArgDocFindFiles(){
    OperationDoc out;
    out.name = "FindFiles";

    out.tags.emplace_back("category: meta");
    out.tags.emplace_back("category: filesystem");

    out.desc = 
        "Search a directory for files and/or subdirectories without loading them."
        " If children operations are provided, they are invoked once per located item,"
        " similar to the GNU find '-exec' option;"
        " search stops if a child operation returns false."
        " Optionally, each located item can be recorded as a row in a table (see TableOutputMethod).";

    out.notes.emplace_back(
        "The search is halted when a child operation returns false."
    );
    out.notes.emplace_back(
        "The return value is false if a child operation returns false, otherwise true."
    );
    out.notes.emplace_back(
        "When table output is enabled, each row contains the full path,"
        " last modification time, and file size in bytes."
        " File size is reported as zero for directories and non-regular files."
    );
    out.notes.emplace_back(
        "Name and path filters use case-insensitive regular expressions."
    );

    out.args.emplace_back();
    out.args.back().name = "RootDir";
    out.args.back().desc = "The root directory to search.";
    out.args.back().default_val = ".";
    out.args.back().expected = true;
    out.args.back().examples = { ".", "/tmp/", "/path/to/root/dir" };

    out.args.emplace_back();
    out.args.back().name = "MinDepth";
    out.args.back().desc = "Minimum directory depth relative to the root. Depth zero is the root itself."
                           " Setting MinDepth to 1 skips the root directory itself.";
    out.args.back().default_val = "0";
    out.args.back().expected = true;
    out.args.back().examples = { "0", "1", "2" };

    out.args.emplace_back();
    out.args.back().name = "MaxDepth";
    out.args.back().desc = "Maximum directory depth relative to the root. Depth zero means only the root."
                           " A negative value means unlimited depth."
                           " When non-negative, recursive search is limited to this depth"
                           " and the Recurse parameter is ignored.";
    out.args.back().default_val = "-1";
    out.args.back().expected = true;
    out.args.back().examples = { "-1", "0", "1", "5" };

    out.args.emplace_back();
    out.args.back().name = "Recurse";
    out.args.back().desc = "Controls whether the search recurses into subdirectories."
                           " This parameter is ignored when MaxDepth is non-negative.";
    out.args.back().default_val = "true";
    out.args.back().expected = true;
    out.args.back().examples = { "true", "false" };
    out.args.back().samples = OpArgSamples::Exhaustive;

    out.args.emplace_back();
    out.args.back().name = "Target";
    out.args.back().desc = "Controls whether files, directories, or both should be located."
                           " Corresponds to the GNU find '-type' option (f = files, d = directories)."
                           " Note that 'files' matches only regular files.";
    out.args.back().default_val = "files";
    out.args.back().expected = true;
    out.args.back().examples = { "files", "directories", "files+directories" };
    out.args.back().samples = OpArgSamples::Exhaustive;

    out.args.emplace_back();
    out.args.back().name = "Name";
    out.args.back().desc = "If non-empty, only items whose base filename matches this case-insensitive regular"
                           " expression are selected. Corresponds to the GNU find '-iname' option.";
    out.args.back().default_val = "";
    out.args.back().expected = true;
    out.args.back().examples = { "", ".*[.]dcm", ".*[.]txt", ".*plan.*" };

    out.args.emplace_back();
    out.args.back().name = "WholeName";
    out.args.back().desc = "If non-empty, only items whose full path matches this case-insensitive regular"
                           " expression are selected. Corresponds to the GNU find '-iwholename' option.";
    out.args.back().default_val = "";
    out.args.back().expected = true;
    out.args.back().examples = { "", ".*/ct/.*", ".*/plan.*" };

    out.args.emplace_back();
    out.args.back().name = "NewerMT";
    out.args.back().desc = "If non-empty, only items with a modification time strictly newer than the given"
                           " date-time are selected."
                           "\n\n"
                           "The format is flexible and accepts 'YYYYMMDD-HHMMSS' (preferred),"
                           " 'YYYY-MM-DD HH:MM:SS', 'YYYY-MM-DDTHH:MM:SSZ', and similar ISO 8601 variants."
                           " Corresponds to the GNU find '-newermt' option.";
    out.args.back().default_val = "";
    out.args.back().expected = true;
    out.args.back().examples = { "", "20240101-000000", "20230615-123000" };

    out.args.emplace_back();
    out.args.back().name = "Quit";
    out.args.back().desc = "If true, stop after the first matching item is found and processed."
                           " Corresponds to the GNU find '-quit' option.";
    out.args.back().default_val = "false";
    out.args.back().expected = true;
    out.args.back().examples = { "true", "false" };
    out.args.back().samples = OpArgSamples::Exhaustive;

    out.args.emplace_back();
    out.args.back().name = "Key";
    out.args.back().desc = "When child operations are invoked, the path of the current item is stored in the"
                           " global parameter table using this key."
                           "\n\n"
                           "Note that any existing value for this key will be restored after this operation completes.";
    out.args.back().default_val = "path";
    out.args.back().expected = true;
    out.args.back().examples = { "path", "file", "dir", "x" };

    out.args.emplace_back();
    out.args.back().name = "OutputAbsolute";
    out.args.back().desc = "If true, output an absolute path for each file, rather than the path relative to the"
                           " RootDir.";
    out.args.back().default_val = "false";
    out.args.back().expected = true;
    out.args.back().examples = { "true", "false" };
    out.args.back().samples = OpArgSamples::Exhaustive;

    out.args.emplace_back();
    out.args.back().name = "TableOutputMethod";
    out.args.back().desc = "Controls whether located items are written to a table."
                           "\n\n"
                           "'Always' unconditionally writes each matched item to a table."
                           "\n\n"
                           "'Never' never writes to a table; no table is created or selected."
                           "\n\n"
                           "'Auto' (default) automatically decides: when child operations are"
                           " absent, items are written to a table (the user most likely wants to"
                           " cache the file list); when child operations are present, table output"
                           " is disabled (the user most likely does not need a cached list and can"
                           " re-invoke the operation without children if needed).";
    out.args.back().default_val = "auto";
    out.args.back().expected = true;
    out.args.back().examples = { "auto", "always", "never" };
    out.args.back().samples = OpArgSamples::Exhaustive;

    out.args.emplace_back();
    out.args.back() = STWhitelistOpArgDoc();
    out.args.back().name = "TableSelection";
    out.args.back().default_val = "last";

    out.args.emplace_back();
    out.args.back().name = "TableLabel";
    out.args.back().desc = "A label to attach to the table when a new table is created.";
    out.args.back().default_val = "unspecified";
    out.args.back().expected = true;
    out.args.back().examples = { "unspecified", "search-results", "file-list" };

    return out;
}

bool FindFiles(Drover& DICOM_data,
               const OperationArgPkg& OptArgs,
               std::map<std::string, std::string>& InvocationMetadata,
               const std::string& FilenameLex){

    Explicator X(FilenameLex);

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto RootDir              = OptArgs.getValueStr("RootDir").value();
    const auto MinDepthStr          = OptArgs.getValueStr("MinDepth").value();
    const auto MaxDepthStr          = OptArgs.getValueStr("MaxDepth").value();
    const auto RecurseStr           = OptArgs.getValueStr("Recurse").value();
    const auto TargetStr            = OptArgs.getValueStr("Target").value();
    const auto NameStr              = OptArgs.getValueStr("Name").value();
    const auto WholeNameStr         = OptArgs.getValueStr("WholeName").value();
    const auto NewerMTStr           = OptArgs.getValueStr("NewerMT").value();
    const auto QuitStr              = OptArgs.getValueStr("Quit").value();
    const auto Key                  = OptArgs.getValueStr("Key").value();
    const auto OutputAbsoluteStr    = OptArgs.getValueStr("OutputAbsolute").value();
    const auto TableOutputMethodStr = OptArgs.getValueStr("TableOutputMethod").value();
    const auto TableSelectionStr    = OptArgs.getValueStr("TableSelection").value();
    const auto TableLabelStr        = OptArgs.getValueStr("TableLabel").value();

    //-----------------------------------------------------------------------------------------------------------------
    const auto NormalizedTableLabelStr = X(TableLabelStr);

    const auto regex_true   = Compile_Regex("^tr?u?e?$");
    const auto regex_files  = Compile_Regex(".*fi?l?e?s?.*");
    const auto regex_dirs   = Compile_Regex(".*di?r?e?c?t?o?r?i?e?s?.*");

    const auto regex_always = Compile_Regex("^al?w?a?y?s?$");
    const auto regex_never  = Compile_Regex("^ne?v?e?r?$");
    const auto regex_auto   = Compile_Regex("^au?t?o?$");

    const bool should_recurse = std::regex_match(RecurseStr, regex_true);
    const bool should_quit    = std::regex_match(QuitStr, regex_true);
    const bool get_files      = std::regex_match(TargetStr, regex_files);
    const bool get_dirs       = std::regex_match(TargetStr, regex_dirs);

    const bool tbl_always = std::regex_match(TableOutputMethodStr, regex_always);
    const bool tbl_never  = std::regex_match(TableOutputMethodStr, regex_never);
    const bool tbl_auto   = std::regex_match(TableOutputMethodStr, regex_auto);

    if(!tbl_always && !tbl_never && !tbl_auto){
        throw std::invalid_argument("Unrecognized TableOutputMethod '"_s + TableOutputMethodStr + "'; expected 'always', 'never', or 'auto'");
    }

    const bool should_output_absolute = std::regex_match(OutputAbsoluteStr, regex_true);

    int64_t min_depth = std::stoll(MinDepthStr);
    int64_t max_depth = std::stoll(MaxDepthStr);

    // Effective max depth: when max_depth is negative, use Recurse to decide.
    const int64_t eff_max = (max_depth >= 0) ? max_depth : (should_recurse ? -1LL : 1LL);

    // Compile optional name and full-path regex filters (case-insensitive via Compile_Regex).
    std::optional<std::regex> name_re;
    std::optional<std::regex> wholename_re;
    if(!NameStr.empty())      name_re      = Compile_Regex(NameStr);
    if(!WholeNameStr.empty()) wholename_re = Compile_Regex(WholeNameStr);

    // Parse NewerMT modification-time threshold using the portable time_mark class.
    std::optional<time_mark> newer_mt_threshold;
    if(!NewerMTStr.empty()){
        time_mark t;
        if(!t.Read_from_string(NewerMTStr)){
            throw std::invalid_argument("Unable to parse NewerMT '"_s + NewerMTStr + "': expected 'YYYYMMDD-HHMMSS' or ISO 8601 variant");
        }
        newer_mt_threshold = t;
    }

    // Convert last_write_time to time_t portably.
    //
    // Note: In C++17 the epoch of std::filesystem::file_time_type::clock is unspecified
    //       and may not match std::chrono::system_clock, so we translate between the
    //       clocks rather than assuming a shared epoch.
    const auto get_mod_time_t = [](const std::filesystem::path& p) -> std::time_t {
        const auto ft = std::filesystem::last_write_time(p);
        using file_clock = std::filesystem::file_time_type::clock;
        const auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            ft - file_clock::now() + std::chrono::system_clock::now());
        return std::chrono::system_clock::to_time_t(sctp);
    };

    // Validate root directory.
    const auto root = std::filesystem::path(RootDir);
    if(!std::filesystem::is_directory(root)){
        throw std::invalid_argument("RootDir is not a directory or is not accessible.");
    }

    // Test whether a path passes all filters (type, depth, name, path, time).
    const auto passes_filters = [&](const std::filesystem::path& p, int64_t depth) -> bool {
        if( (depth < min_depth)
        ||  (eff_max >= 0 && depth > eff_max) ){
            return false;
        }

        const bool is_dir      = std::filesystem::is_directory(p);
        const bool is_reg_file = std::filesystem::is_regular_file(p);
        if( ( is_dir      && !get_dirs   )
        ||  ( is_reg_file && !get_files  )
        ||  (!is_dir      && !is_reg_file) ){
            return false;
        }

        const auto fname = p.filename().string();
        if( name_re
        &&  !std::regex_match(fname, name_re.value()) ){
            return false;
        }

        const auto fpath = std::filesystem::absolute(p).string();
        if( wholename_re
        &&  !std::regex_match(fpath, wholename_re.value()) ){
            return false;
        }

        if(newer_mt_threshold){
            const time_mark mod_tm( get_mod_time_t(p) );
            if(mod_tm <= newer_mt_threshold.value()) return false;
        }
        return true;
    };

    // Collect matching paths, using skip_permission_denied to continue past inaccessible directories.
    std::list<std::filesystem::path> paths;
    const auto iter_opts = std::filesystem::directory_options::skip_permission_denied;

    // The root itself is at depth 0.
    if(passes_filters(root, 0)){
        paths.emplace_back(root);
    }

    if(eff_max != 0){
        if(eff_max == 1){
            // Non-recursive: iterate direct children only (depth 1).
            for(const auto& entry : std::filesystem::directory_iterator(root, iter_opts)){
                if(passes_filters(entry.path(), 1)){
                    paths.emplace_back(entry.path());
                }
            }
        }else{
            // Recursive: use recursive_directory_iterator with optional depth pruning.
            auto rit = std::filesystem::recursive_directory_iterator(root, iter_opts);
            for(; rit != std::filesystem::recursive_directory_iterator(); ++rit){
                // rit.depth() is 0 for direct children of root, 1 for grandchildren, etc.
                // Adding 1 gives the depth in GNU find terms (root = 0, direct children = 1).
                const int64_t depth = static_cast<int64_t>(rit.depth()) + 1;
                // Prevent recursing into directories that would exceed eff_max.
                if( std::filesystem::is_directory(rit->path())
                &&  eff_max >= 0
                &&  depth >= eff_max ){
                    rit.disable_recursion_pending();
                }
                if(passes_filters(rit->path(), depth)){
                    paths.emplace_back(rit->path());
                }
            }
        }
    }

    // Sort paths for deterministic, consistent behaviour.
    paths.sort();

    YLOGINFO("FindFiles: located " << paths.size() << " item(s) under '" << RootDir << "'");

    // Determine whether table output is enabled.
    auto children = OptArgs.getChildren();
    const bool write_table = tbl_always
                          || (tbl_auto && children.empty());

    // Locate or create a table for the results when table output is enabled.
    tables::table2* t = nullptr;
    if(write_table){
        auto STs_all = All_STs(DICOM_data);
        auto STs = Whitelist(STs_all, TableSelectionStr);
        bool created_new_table = false;
        if(STs.empty()){
            auto meta = coalesce_metadata_for_basic_table({}, meta_evolve::iterate);
            DICOM_data.table_data.emplace_back(std::make_unique<Sparse_Table>());
            DICOM_data.table_data.back()->table.metadata = meta;
            DICOM_data.table_data.back()->table.metadata["TableLabel"] = TableLabelStr;
            DICOM_data.table_data.back()->table.metadata["NormalizedTableLabel"] = NormalizedTableLabelStr;
            DICOM_data.table_data.back()->table.metadata["Description"] = "Generated via FindFiles";
            created_new_table = true;
        }
        t = created_new_table
          ? &(DICOM_data.table_data.back()->table)
          : &((*(STs.back()))->table);

        // When creating a new table, insert a header row.
        if(created_new_table){
            const auto row = t->next_empty_row();
            t->inject(row, 0, "FilePath");
            t->inject(row, 1, "LastModTime");
            t->inject(row, 2, "FileSizeBytes");
        }
    }

    // Stow the original value of Key so it is restored when this scope exits.
    auto metadata_stow = stow_metadata(InvocationMetadata, {}, Key);
    metadata_stow_guard msg(InvocationMetadata, metadata_stow);

    bool ret = true;
    for(const auto& p : paths){
        // Record this item in the table when table output is enabled.
        if(t != nullptr){
            const auto mod_t_str = time_mark( get_mod_time_t(p) ).Dump_as_string();
            const auto fsize_str = std::filesystem::is_regular_file(p)
                                 ? std::to_string(std::filesystem::file_size(p))
                                 : std::string("0");
            const auto row = t->next_empty_row();
            const std::string out_p = should_output_absolute ? std::filesystem::weakly_canonical(std::filesystem::absolute(p)).string()
                                                             : p.string();
            t->inject(row, 0, out_p);
            t->inject(row, 1, mod_t_str);
            t->inject(row, 2, fsize_str);
        }

        // Invoke child operations.
        if(!children.empty()){
            InvocationMetadata[Key] = p.string();
            ret = Operation_Dispatcher(DICOM_data, InvocationMetadata, FilenameLex, children);
            if(!ret) break;
        }

        if(should_quit) break;
    }

    return ret;
}

