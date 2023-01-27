//PollDirectories.cc - A part of DICOMautomaton 2023. Written by hal clark.

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
#include <filesystem>
#include <chrono>
#include <unordered_set>
#include <iomanip>            //Needed for std::put_time(...)

#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorString.h"       //Needed for GetFirstRegex(...)
#include "YgorStats.h"

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../File_Loader.h"
#include "../Operation_Dispatcher.h"

#include "PollDirectories.h"


OperationDoc OpArgDocPollDirectories(){
    OperationDoc out;
    out.name = "PollDirectories";

    out.desc = 
        "This operation continuously polls ('watches') a directory, waiting for new files."
        " When files are received, they are loaded and child operations are performed.";

    out.notes.emplace_back(
        "This operation can be used to automatically perform an action when data appears in the specified directories."
        " Consider this operation a 'trigger' that can initiate further processing."
    );
    out.notes.emplace_back(
        "Only file names and sizes are used to evaluate when a file was last altered."
        " Filesystem modification times are not used, and file contents being altered will not be detected."
    );
    out.notes.emplace_back(
        "To reduce external dependencies, only rudimentary directory polling methods are used."
        " Polling may therefore be slow and/or inefficient, depending on filesystem/OS caching."
    );
    out.notes.emplace_back(
        "Files will be loaded and processed in batches sequentially, i.e., in 'blocking' mode."
    );
    out.notes.emplace_back(
        "Before files are processed, they are loaded into the existing Drover object."
        " Similarly after processing, the Drover object containing loaded files and processing results"
        " are retained. The Drover object can be explicitly cleared after processing if needed."
    );
    out.notes.emplace_back(
        "This operation will stop polling and return false when the first child operation returns false."
        " If files cannot be loaded, this operation will also stop polling and return false."
        " Otherwise, this operation will continue polling forever. It will never return true."
    );

    out.args.emplace_back();
    out.args.back().name = "Directories";
    out.args.back().desc = "The directories to poll, separated by semicolons. Files and directories within these"
                           " directories will be loaded and processed.";
    out.args.back().default_val = "./";
    out.args.back().expected = true;
    out.args.back().examples = { "/tmp/", "/home/user/incoming/;/path/to/another/directory/" };

    out.args.emplace_back();
    out.args.back().name = "PollInterval";
    out.args.back().desc = "The amount of time, in seconds, to wait between polling. Note that the time spent"
                           " polling (i.e., enumerating directory contents and metadata) is not included in this"
                           " time, so the total polling cycle time will be larger than this interval.";
    out.args.back().default_val = "5.0";
    out.args.back().expected = true;
    out.args.back().examples = { "1.0", "5", "600" };

    out.args.emplace_back();
    out.args.back().name = "SettleDelay";
    out.args.back().desc = "The amount of time, in seconds, that directories need to remain unaltered before processing."
                           " When files are copied to one of the input directories, this amount of time needs to pass"
                           " before the file will be loaded. If any file is altered within this time period, the delay"
                           " timer will reset."
                           "\n\n"
                           "Adding this delay ensures that files still in transit are not loaded early.";
    out.args.back().default_val = "60.0";
    out.args.back().expected = true;
    out.args.back().examples = { "30.0", "60", "200" };

    out.args.emplace_back();
    out.args.back().name = "IgnoreExisting";
    out.args.back().desc = "Controls whether files present during the first poll should be considered already"
                           " processed. This option can increase robustness if irrelevant files are found, but can"
                           " also result in files being missed if inputs are provided prior to the first poll.";
    out.args.back().default_val = "false";
    out.args.back().expected = true;
    out.args.back().examples = { "true", "false" };
    out.args.back().samples = OpArgSamples::Exhaustive;

    out.args.emplace_back();
    out.args.back().name = "GroupBy";
    out.args.back().desc = "Controls how files are grouped together for processing."
                           " Currently supported options are 'separate', 'subdirs', and 'altogether'."
                           "\n\n"
                           "Use 'separate' to process files individually, one-at-a-time. This option is most useful"
                           " for performing checks or validation of individual files where the logical relations to"
                           " other files are not important."
                           "\n\n"
                           "Use 'subdirs' to group all files that share a common parent sub-directory or folder."
                           " This option will cause all files in a directory (non-recursively) to be processed"
                           " together. This option is useful when multiple logically-distinct inputs are received"
                           " at the same time, but use a single top-level directory to keep separated."
                           "\n\n"
                           "Use 'altogether' to process all files together as one logical unit, disregarding the"
                           " directory structure. This option works best when the directory is expected to receive"
                           " one set of files at a time, and is robust to the directory structure (e.g., a set of"
                           " DICOM files which have been nested in a DICOM tree for optimal filesystem lookup, but"
                           " not necessarily grouped logically).";
    out.args.back().default_val = "separate";
    out.args.back().expected = true;
    out.args.back().examples = { "separate", "subdirs", "altogether" };
    out.args.back().samples = OpArgSamples::Exhaustive;

    return out;
}



bool PollDirectories(Drover &DICOM_data,
                     const OperationArgPkg& OptArgs,
                     std::map<std::string, std::string>& InvocationMetadata,
                     const std::string& FilenameLex){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto DirectoriesStr = OptArgs.getValueStr("Directories").value();
    const auto PollInterval = std::stod( OptArgs.getValueStr("PollInterval").value() );
    const auto SettleDelay = std::stod( OptArgs.getValueStr("SettleDelay").value() );
    const auto GroupByStr = OptArgs.getValueStr("GroupBy").value();
    const auto IgnoreExistingStr = OptArgs.getValueStr("IgnoreExisting").value();

    long int filesystem_error_count = 0;
    const long int max_filesystem_error_count = 20;
    //-----------------------------------------------------------------------------------------------------------------
    const auto regex_true       = Compile_Regex("^tr?u?e?$");
    const auto regex_separate   = Compile_Regex("^se?p?a?r?a?t?e?$");
    const auto regex_subdirs    = Compile_Regex("^su?b[_-]?d?i?r?e?c?t?o?r?[iy]?e?s?$");
    const auto regex_altogether = Compile_Regex("^al?t?o?g?e?t?h?e?r?$");

    const auto IgnoreExisting  = std::regex_match(IgnoreExistingStr, regex_true);
    const auto GroupBySeparate = std::regex_match(GroupByStr, regex_separate);
    const auto GroupBySubdirs  = std::regex_match(GroupByStr, regex_subdirs);
    const auto GroupAltogether = std::regex_match(GroupByStr, regex_altogether);

    if(OptArgs.getChildren().empty()){
        YLOGWARN("No children operations specified; files will be loaded but not processed");
    }
    if(PollInterval < 0){
        throw std::invalid_argument("Polling interval is invalid. Cannot continue.");
    }
    if(SettleDelay < 0){
        throw std::invalid_argument("Settle delay is invalid. Cannot continue.");
    }
    if(SettleDelay < PollInterval){
        YLOGWARN("Settle delay is shorter than polling interval. Files will be considered settled when first detected");
    }
    const auto PollInterval_ms = static_cast<int64_t>(1000.0 * PollInterval);
    const auto wait = [PollInterval_ms](){
        std::this_thread::sleep_for(std::chrono::milliseconds(PollInterval_ms));
        return;
    };

    const auto raw_dirs = SplitStringToVector(DirectoriesStr, ';', 'd');
    std::vector<std::filesystem::path> watch_dirs;
    for(const auto& s : raw_dirs){
        watch_dirs.emplace_back(s);
        if(!std::filesystem::is_directory(watch_dirs.back())){
            throw std::invalid_argument("Cannot access directory '"_s + s + "'. Cannot continue.");
        }
    }
    if(watch_dirs.empty()){
        throw std::invalid_argument("No directories to poll. Cannot continue.");
    }

    struct file_metadata {
        //std::filesystem::file_time_type last_time;
        std::chrono::time_point<std::chrono::steady_clock> last_time;
        std::uintmax_t file_size = 0U;

        bool present   = false; // File appeared in the most recent directory enumeration.
        bool processed = false; // File has already been processed and should be ignored.
        bool ready     = false; // File is ready to be processed, but could be waiting for sibling files to transit.
    };

    // Cache uses parent directory and file path as keys, file metadata as values.
    // Parent directory key is separate to facilitate easier access to all the files sharing a common parent directory.
    using inner_cache_t = std::map<std::filesystem::path, file_metadata>;
    using cache_t = std::map<std::filesystem::path, inner_cache_t>;
    cache_t cache;
    bool first_pass = true;
    while(true){
        if(!first_pass) wait();

        // Reset the cache visibility for each entry.
        for(auto& block : cache){
            for(auto& p : block.second){
                p.second.present = false;
            }
        }

        // Enumerate the contents of the input directories.
        try{
            const auto t_start = std::chrono::system_clock::now();
            for(const auto& d : watch_dirs){
                for(const auto& e : std::filesystem::recursive_directory_iterator(d)){
                    if(!e.exists() || e.is_directory()) continue;

                    const auto f = e.path();
                    const auto p = f.parent_path();
                    const auto s = e.file_size();
                    const auto now = std::chrono::steady_clock::now();

                    // Ensure the subdirectory block exists.
                    auto& subdir = cache[p];

                    // Search for an existing entry in the subdirectory block.
                    file_metadata* entry_ptr = nullptr;
                    auto it = subdir.find(f);


                    // If not yet seen, create an entry including relevant metadata.
                    if(it == subdir.end()){ 
                        subdir[f];
                        it = subdir.find(f);
                        entry_ptr = &(it->second);

                        entry_ptr->file_size = s;
                        entry_ptr->last_time = now;

                        entry_ptr->present   = true;
                        entry_ptr->processed = (IgnoreExisting && first_pass);
                        entry_ptr->ready     = false;
                    }

                    // Mark the file as present.
                    entry_ptr = &(it->second);
                    entry_ptr->present = true;

                    // If already processed, ignore.
                    if( entry_ptr->processed == true ) continue;

                    // Otherwise if size is still being modified, then reset the entry metadata.
                    if( s != entry_ptr->file_size ){
                        entry_ptr->file_size = s;
                        entry_ptr->last_time = now;

                    // Otherwise check if the file is ready to be processed.
                    }else{
                        //const std::chrono::duration<double> dt = (now - entry_ptr->last_time);
                        const auto dt = std::chrono::duration<double>(now - entry_ptr->last_time).count();
                        entry_ptr->ready = (s == entry_ptr->file_size) && (SettleDelay < dt);
                    }
                }
            }

            const auto t_stop = std::chrono::system_clock::now();
            const auto elapsed = std::chrono::duration<double>(t_stop - t_start).count();
            if( (5.0 < elapsed) 
            ||  ((0.5 * PollInterval) < elapsed)
            ||  ((0.5 * SettleDelay) < elapsed) ){
                YLOGWARN("Directory enumeration took " << elapsed << " s");
            }

            first_pass = false;

        }catch(const std::exception &e){
            ++filesystem_error_count;
            YLOGWARN("Encountered error enumerating directory: '" << e.what() << "'");
            if(filesystem_error_count < max_filesystem_error_count){
                YLOGINFO("Filesystem error count: " << filesystem_error_count);
            }else{
                throw std::runtime_error("Exceeded maximum permissable filesystem error count. Cannot continue.");
            }

            // Sleep for the polling interval time.
            wait();
            continue;
        }

        // Purge any entries that are no longer visible.
        for(auto& block : cache){
            auto it = block.second.begin();
            const auto end = block.second.end();
            while(it != end){
                if(!it->second.present){
                    it = block.second.erase(it);
                }else{
                    ++it;
                }
            }
        }

        // Remove any parent directories that are empty.
        {
            auto it = cache.begin();
            const auto end = cache.end();
            while(it != end){
                if(it->second.empty()){
                    it = cache.erase(it);
                }else{
                    ++it;
                }
            }
        }

        // Report on cache contents for monitoring / debugging.
        {
            uint64_t total_count     = 0U;
            uint64_t processed_count = 0U;
            uint64_t ready_count     = 0U;
            uint64_t pending_count   = 0U;
            for(auto& block : cache){
                for(auto& p : block.second){
                    ++total_count;
                    if(p.second.processed){
                        ++processed_count;
                    }else if(p.second.ready){
                        ++ready_count;
                    }else{
                        ++pending_count;
                    }
                }
            }
            {
                const auto now = std::chrono::system_clock::now();
                const std::time_t t_now = std::chrono::system_clock::to_time_t(now);
                YLOGINFO("Poll results: "
                     << "(" << std::put_time(std::localtime(&t_now), "%Y%m%d-%H%M%S") << ") "
                     << "cache contains " << total_count << " entries -- "
                     << pending_count << " pending, "
                     << ready_count << " ready, and "
                     << processed_count << " processed");
            }
        }

        // If all files are ready to process, assemble batches for later processing.
        // Prospectively mark the file as processed to avoid a second-pass later.
        const auto is_unprocessed = [](const inner_cache_t::value_type& p){
                                        return !(p.second.processed);
                                    };
        const auto is_ready = [](const inner_cache_t::value_type& p){
                                  return !p.second.processed && p.second.ready;
                              };
        const auto is_processed_or_ready = [](const inner_cache_t::value_type& p){
                                               return (p.second.processed) || p.second.ready;
                                           };

        const auto block_has_unprocessed = [is_unprocessed](const cache_t::value_type& block){
                                        return std::any_of( std::begin(block.second),
                                                            std::end(block.second),
                                                            is_unprocessed );
                                     };
        const auto block_ready = [&](const cache_t::value_type& block){
                                   return std::all_of( std::begin(block.second),
                                                       std::end(block.second),
                                                       is_processed_or_ready );
                               };

        std::list< std::list<std::filesystem::path> > to_process;

        // Treat each subdirectory as a distinct logical group.
        if( GroupBySubdirs ){
            for(auto& block : cache){
                if(!block_has_unprocessed(block)) continue; // Nothing to do.

                if(block_ready(block)){
                    to_process.emplace_back();
                    for(auto& p : block.second){
                        if(is_ready(p)){
                            to_process.back().emplace_back(p.first);
                            p.second.processed = true;
                        }
                    }
                }
            }

        // Treat all files as a single logical group.
        }else if( GroupAltogether ){
            do{
                const bool has_unprocessed = std::any_of( std::begin(cache),
                                                          std::end(cache),
                                                          [&](const cache_t::value_type& c){
                                                              return block_has_unprocessed(c);
                                                          });
                if(!has_unprocessed) break; // Nothing to do.

                const bool all_eligible_ready = std::all_of( std::begin(cache),
                                                             std::end(cache),
                                                             [&](const cache_t::value_type& c){
                                                                 return block_ready(c);
                                                             });
                if(all_eligible_ready){
                    to_process.emplace_back();
                    for(auto& block : cache){
                        for(auto& p : block.second){
                            if(is_ready(p)){
                                to_process.back().emplace_back(p.first);
                                p.second.processed = true;
                            }
                        }
                    }
                }
            }while(false);

        // Consider all files spearate from one another.
        }else if( GroupBySeparate ){
            for(auto& block : cache){
                for(auto& p : block.second){
                    if(is_ready(p)){
                        to_process.emplace_back();
                        to_process.back().emplace_back(p.first);
                        p.second.processed = true;
                    }
                }
            }

        }else{
            throw std::invalid_argument("Grouping argument not understood. Cannot continue.");
        }

        // Process files in batches, one batch at a time (sequentially).
        for(const auto& batch : to_process){
            YLOGINFO("Processing a batch with " << batch.size() << " files");

            // Load the files to a placeholder Drover class.
            Drover DD_work;
            std::map<std::string, std::string> placeholder_1;
            std::list<OperationArgPkg> Operations;
            {
                std::list<std::filesystem::path> l_batch(batch);
                const auto res = Load_Files(DD_work, placeholder_1, FilenameLex, Operations, l_batch);
                if(!res){
                    throw std::runtime_error("Unable to load one or more files. Refusing to continue.");
                }
            }
            if( !Operations.empty() ){
                YLOGWARN("Loaded one or more operations. Note that loaded operations will be ignored");
            }

            // Merge the loaded files into the current Drover class.
            DICOM_data.Consume(DD_work);

            auto children = OptArgs.getChildren();
            if(!children.empty()){
                const auto res = Operation_Dispatcher(DICOM_data, InvocationMetadata, FilenameLex, children);
                if(!res){
                    return false;
                }
            }
        }

        // Optionally remove all processed files from input directories.
        // ... could be quite dangerous / easy to cause data loss.
    }

    return true;
}
