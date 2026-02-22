//FindFiles.cc - A part of DICOMautomaton 2024. Written by hal clark.

#include <algorithm>
#include <chrono>
#include <ctime>
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

#include "FindFiles.h"


OperationDoc OpArgDocFindFiles(){
    OperationDoc out;
    out.name = "FindFiles";

    out.tags.emplace_back("category: meta");
    out.tags.emplace_back("category: filesystem");

    out.desc = 
        "Search a directory for files and/or subdirectories without loading them."
        " Each located item is recorded as a row in a table."
        " If children operations are provided, they are invoked once per located item,"
        " similar to the GNU find '-exec' option;"
        " the search stops if a child operation returns false.";

    out.notes.emplace_back(
        "The search is halted when a child operation returns false."
    );
    out.notes.emplace_back(
        "The return value is false if a child operation returns false, otherwise true."
    );
    out.notes.emplace_back(
        "Each table row contains the full path, last modification time (UTC, ISO 8601 format), and file size in bytes."
        " File size is reported as zero for directories and non-regular files."
    );
    out.notes.emplace_back(
        "Name and path filters use case-sensitive regular expressions."
        " The corresponding case-insensitive variants (IName, IWholePath) use the same syntax but ignore case."
    );

    out.args.emplace_back();
    out.args.back().name = "RootDir";
    out.args.back().desc = "The root directory to search.";
    out.args.back().default_val = "";
    out.args.back().expected = true;
    out.args.back().examples = { "/tmp/", "/path/to/root/dir" };

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
                           " Ignored when MaxDepth is non-negative.";
    out.args.back().default_val = "true";
    out.args.back().expected = true;
    out.args.back().examples = { "true", "false" };
    out.args.back().samples = OpArgSamples::Exhaustive;

    out.args.emplace_back();
    out.args.back().name = "Target";
    out.args.back().desc = "Controls whether files, directories, or both should be located."
                           " Corresponds to the GNU find '-type' option (f = files, d = directories).";
    out.args.back().default_val = "files";
    out.args.back().expected = true;
    out.args.back().examples = { "files", "directories", "files+directories" };
    out.args.back().samples = OpArgSamples::Exhaustive;

    out.args.emplace_back();
    out.args.back().name = "Name";
    out.args.back().desc = "If non-empty, only items whose base filename matches this case-sensitive regular"
                           " expression are selected. Corresponds to the GNU find '-name' option.";
    out.args.back().default_val = "";
    out.args.back().expected = true;
    out.args.back().examples = { "", ".*\\.dcm", ".*\\.txt", ".*plan.*" };

    out.args.emplace_back();
    out.args.back().name = "IName";
    out.args.back().desc = "If non-empty, only items whose base filename matches this case-insensitive regular"
                           " expression are selected. Corresponds to the GNU find '-iname' option.";
    out.args.back().default_val = "";
    out.args.back().expected = true;
    out.args.back().examples = { "", ".*\\.dcm", ".*\\.txt", ".*plan.*" };

    out.args.emplace_back();
    out.args.back().name = "WholePath";
    out.args.back().desc = "If non-empty, only items whose full path matches this case-sensitive regular"
                           " expression are selected. Corresponds to the GNU find '-wholename' option.";
    out.args.back().default_val = "";
    out.args.back().expected = true;
    out.args.back().examples = { "", ".*/ct/.*", ".*/plan.*" };

    out.args.emplace_back();
    out.args.back().name = "IWholePath";
    out.args.back().desc = "If non-empty, only items whose full path matches this case-insensitive regular"
                           " expression are selected. Corresponds to the GNU find '-iwholename' option.";
    out.args.back().default_val = "";
    out.args.back().expected = true;
    out.args.back().examples = { "", ".*/ct/.*", ".*/plan.*" };

    out.args.emplace_back();
    out.args.back().name = "NewerMT";
    out.args.back().desc = "If non-empty, only items with a modification time strictly newer than the given"
                           " date-time are selected."
                           " The expected format is 'YYYY-MM-DD HH:MM:SS' (interpreted as UTC)."
                           " Corresponds to the GNU find '-newermt' option.";
    out.args.back().default_val = "";
    out.args.back().expected = true;
    out.args.back().examples = { "", "2024-01-01 00:00:00", "2023-06-15 12:30:00" };

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
    const auto RootDir           = OptArgs.getValueStr("RootDir").value();
    const auto MinDepthStr       = OptArgs.getValueStr("MinDepth").value();
    const auto MaxDepthStr       = OptArgs.getValueStr("MaxDepth").value();
    const auto RecurseStr        = OptArgs.getValueStr("Recurse").value();
    const auto TargetStr         = OptArgs.getValueStr("Target").value();
    const auto NameStr           = OptArgs.getValueStr("Name").value();
    const auto INameStr          = OptArgs.getValueStr("IName").value();
    const auto WholePathStr      = OptArgs.getValueStr("WholePath").value();
    const auto IWholePathStr     = OptArgs.getValueStr("IWholePath").value();
    const auto NewerMTStr        = OptArgs.getValueStr("NewerMT").value();
    const auto QuitStr           = OptArgs.getValueStr("Quit").value();
    const auto Key               = OptArgs.getValueStr("Key").value();
    const auto TableSelectionStr = OptArgs.getValueStr("TableSelection").value();
    const auto TableLabelStr     = OptArgs.getValueStr("TableLabel").value();

    //-----------------------------------------------------------------------------------------------------------------
    const auto NormalizedTableLabelStr = X(TableLabelStr);

    const auto regex_true  = Compile_Regex("^tr?u?e?$");
    const auto regex_files = Compile_Regex(".*fi?l?e?s?.*");
    const auto regex_dirs  = Compile_Regex(".*di?r?e?c?t?o?r?i?e?s?.*");

    const bool should_recurse = std::regex_match(RecurseStr, regex_true);
    const bool should_quit    = std::regex_match(QuitStr, regex_true);
    const bool get_files      = std::regex_match(TargetStr, regex_files);
    const bool get_dirs       = std::regex_match(TargetStr, regex_dirs);

    // Parse depth limits.
    int64_t min_depth = 0;
    int64_t max_depth = -1; // negative means unlimited
    {
        char *endp = nullptr;
        min_depth = std::strtol(MinDepthStr.c_str(), &endp, 10);
        if(endp == MinDepthStr.c_str() || *endp != '\0'){
            throw std::invalid_argument("Unable to parse MinDepth parameter");
        }
        max_depth = std::strtol(MaxDepthStr.c_str(), &endp, 10);
        if(endp == MaxDepthStr.c_str() || *endp != '\0'){
            throw std::invalid_argument("Unable to parse MaxDepth parameter");
        }
    }

    // Effective max depth: when max_depth is negative, use Recurse to decide.
    const int64_t eff_max = (max_depth >= 0) ? max_depth : (should_recurse ? -1LL : 1LL);

    // Compile optional name/path regex filters.
    // Name/WholePath are case-sensitive; IName/IWholePath are case-insensitive.
    const auto re_flags_sensitive   = std::regex::nosubs | std::regex::optimize | std::regex::ECMAScript;
    const auto re_flags_insensitive = std::regex::icase | re_flags_sensitive;

    std::optional<std::regex> name_re;
    std::optional<std::regex> iname_re;
    std::optional<std::regex> wholepath_re;
    std::optional<std::regex> iwholepath_re;
    if(!NameStr.empty())       name_re       = std::regex(NameStr,       re_flags_sensitive);
    if(!INameStr.empty())      iname_re      = std::regex(INameStr,      re_flags_insensitive);
    if(!WholePathStr.empty())  wholepath_re  = std::regex(WholePathStr,  re_flags_sensitive);
    if(!IWholePathStr.empty()) iwholepath_re = std::regex(IWholePathStr, re_flags_insensitive);

    // Parse NewerMT modification-time threshold (UTC).
    std::optional<std::time_t> newer_mt_threshold;
    if(!NewerMTStr.empty()){
        struct tm tm_val{};
        if(strptime(NewerMTStr.c_str(), "%Y-%m-%d %H:%M:%S", &tm_val) == nullptr){
            throw std::invalid_argument("Unable to parse NewerMT: expected 'YYYY-MM-DD HH:MM:SS'");
        }
        tm_val.tm_isdst = 0;
        newer_mt_threshold = timegm(&tm_val); // interpret as UTC
    }

    // Helpers for file metadata.
    //
    // Note: On POSIX (Linux), std::filesystem::file_time_type::clock and std::chrono::system_clock
    //       share the same epoch (1970-01-01T00:00:00Z), allowing direct duration arithmetic.
    const auto get_mod_time_t = [](const std::filesystem::path& p) -> std::time_t {
        auto ft  = std::filesystem::last_write_time(p);
        auto dur = ft.time_since_epoch();
        auto sec = std::chrono::duration_cast<std::chrono::seconds>(dur);
        return static_cast<std::time_t>(sec.count());
    };

    const auto format_time_t = [](std::time_t t) -> std::string {
        std::tm tmi{};
        if(gmtime_r(&t, &tmi) == nullptr) return "unknown";
        char buf[32];
        std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tmi);
        return std::string(buf);
    };

    // Validate root directory.
    const auto root = std::filesystem::path(RootDir);
    if(!std::filesystem::is_directory(root)){
        throw std::invalid_argument("RootDir is not a directory or is not accessible.");
    }

    // Test whether a path passes all filters (type, depth, name, path, time).
    const auto passes_filters = [&](const std::filesystem::path& p, int64_t depth) -> bool {
        if(depth < min_depth) return false;
        if(eff_max >= 0 && depth > eff_max) return false;

        const bool is_dir  = std::filesystem::is_directory(p);
        const bool is_file = !is_dir && std::filesystem::exists(p);
        if( is_dir  && !get_dirs ) return false;
        if( is_file && !get_files) return false;
        if(!is_dir  && !is_file  ) return false;

        const auto fname = p.filename().string();
        if(name_re       && !std::regex_match(fname, *name_re      )) return false;
        if(iname_re      && !std::regex_match(fname, *iname_re     )) return false;

        const auto fpath = p.string();
        if(wholepath_re  && !std::regex_match(fpath, *wholepath_re )) return false;
        if(iwholepath_re && !std::regex_match(fpath, *iwholepath_re)) return false;

        if(newer_mt_threshold){
            const auto mod_t = get_mod_time_t(p);
            if(mod_t <= *newer_mt_threshold) return false;
        }
        return true;
    };

    // Collect matching paths.
    std::list<std::filesystem::path> paths;

    // The root itself is at depth 0.
    if(passes_filters(root, 0)){
        paths.emplace_back(root);
    }

    if(eff_max != 0){
        if(eff_max == 1){
            // Non-recursive: iterate direct children only (depth 1).
            for(const auto& entry : std::filesystem::directory_iterator(root)){
                if(passes_filters(entry.path(), 1)){
                    paths.emplace_back(entry.path());
                }
            }
        }else{
            // Recursive: use recursive_directory_iterator with optional depth pruning.
            auto rit = std::filesystem::recursive_directory_iterator(root);
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

    // Locate or create a table for the results.
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
    auto* t = created_new_table
            ? &(DICOM_data.table_data.back()->table)
            : &((*(STs.back()))->table);

    // When creating a new table, insert a header row.
    if(created_new_table){
        const auto r = t->next_empty_row();
        t->inject(r, 0, "FilePath");
        t->inject(r, 1, "LastModTime");
        t->inject(r, 2, "FileSizeBytes");
    }

    // Stow the original value of Key so it is restored when this scope exits.
    auto metadata_stow = stow_metadata(InvocationMetadata, {}, Key);
    metadata_stow_guard msg(InvocationMetadata, metadata_stow);

    auto children = OptArgs.getChildren();

    bool ret = true;
    for(const auto& p : paths){
        // Record this item in the table.
        const auto mod_t     = get_mod_time_t(p);
        const auto mod_t_str = format_time_t(mod_t);
        const auto fsize_str = std::filesystem::is_regular_file(p)
                             ? std::to_string(std::filesystem::file_size(p))
                             : std::string("0");
        const auto row = t->next_empty_row();
        t->inject(row, 0, p.string());
        t->inject(row, 1, mod_t_str);
        t->inject(row, 2, fsize_str);

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

