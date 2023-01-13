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

#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorString.h"       //Needed for GetFirstRegex(...)
#include "YgorStats.h"

#include "../Structs.h"
#include "../Regex_Selectors.h"

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
        "Only file names and sizes are used to evaluate when a file was last altered. Filesystem modification times"
        " are not used, and file contents being altered will not be detected."
    );
    out.notes.emplace_back(
        "To reduce dependencies, only rudimentary directory polling methods are used."
        " Polling may therefore be slow and/or inefficient, depending on filesystem/OS caching."
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
    out.args.back().name = "HonourDirectories";
    out.args.back().desc = "Controls whether files in different (sub-)directories should be considered part of a single"
                           " logical group. If 'true', then each subdirectory will be loaded and processed separately."
                           " If 'false', then all files present in all subdirectories will be considered part of the"
                           " same logical group.";
    out.args.back().default_val = "false";
    out.args.back().expected = true;
    out.args.back().examples = { "true", "false" };
    out.args.back().samples = OpArgSamples::Exhaustive;

    return out;
}



bool PollDirectories(Drover &DICOM_data,
                     const OperationArgPkg& OptArgs,
                     std::map<std::string, std::string>& /*InvocationMetadata*/,
                     const std::string& /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto DirectoriesStr = OptArgs.getValueStr("Directories").value();
    const auto PollInterval = std::stod( OptArgs.getValueStr("PollInterval").value() );
    const auto SettleDelay = std::stod( OptArgs.getValueStr("SettleDelay").value() );
    const auto HonourDirectoriesStr = OptArgs.getValueStr("HonourDirectories").value();

    long int filesystem_error_count = 0;
    const long int max_filesystem_error_count = 20;
    //-----------------------------------------------------------------------------------------------------------------
    const auto regex_true = Compile_Regex("^tr?u?e?$");

    const auto ShouldHonourDirectories = std::regex_match(HonourDirectoriesStr, regex_true);

    if(PollInterval < 0){
        throw std::invalid_argument("Polling interval is invalid. Cannot continue.");
    }
    if(SettleDelay < 0){
        throw std::invalid_argument("Settle delay is invalid. Cannot continue.");
    }
    if(SettleDelay < PollInterval){
        FUNCWARN("Settle delay is shorter than polling interval. Files will be considered settled when first detected");
    }
    const auto PollInterval_ms = static_cast<int64_t>(1000.0 * PollInterval);

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
    while(true){
        // Sleep for the polling interval time.
        //FUNCINFO("Waiting for poll interval");
        std::this_thread::sleep_for(std::chrono::milliseconds(PollInterval_ms));

        // Reset the cache visibility for each entry.
        for(auto& block : cache){
            for(auto& p : block.second){
                p.second.present = false;
            }
        }

        // Enumerate the contents of the input directories.
        try{
            for(const auto& d : watch_dirs){
                for(const auto& e : std::filesystem::recursive_directory_iterator(d)){
                    if(!e.exists() || e.is_directory()) continue;

                    const auto f = e.path();
                    const auto p = f.parent_path();
                    //const auto l = e.last_write_time();
                    //const auto l = std::chrono::steady_clock::now();
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
                        entry_ptr->processed = false;
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
        }catch(const std::exception &e){
            ++filesystem_error_count;
            FUNCWARN("Encountered error enumerating directory: '" << e.what() << "'");
            if(filesystem_error_count < max_filesystem_error_count){
                FUNCINFO("Filesystem error count: " << filesystem_error_count);
            }else{
                throw std::runtime_error("Exceeded maximum permissable filesystem error count. Cannot continue.");
            }
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

        // Report on cache contents for monitoring.
        uint64_t total_count = 0U;
        uint64_t processed_count = 0U;
        uint64_t ready_count = 0U;
        uint64_t pending_count = 0U;
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
            FUNCINFO("Poll results: "
                 << "(" << std::put_time(std::localtime(&t_now), "%F %T") << ") "
                 << "cache contains " << total_count << " entries -- "
                 << pending_count << " pending, "
                 << ready_count << " ready, and "
                 << processed_count << " processed");
        }

        // If all files are ready to process, then process them.
        if( ShouldHonourDirectories ){
            for(auto& block : cache){
                const bool unprocessed_present = std::any_of( std::begin(block.second), std::end(block.second),
                                                    [](const inner_cache_t::value_type& p){
                                                        return !(p.second.processed);
                                                    });
                if(!unprocessed_present) continue; // Nothing to do.


                const bool all_ready = std::all_of( std::begin(block.second), std::end(block.second),
                                                    [](const inner_cache_t::value_type& p){
                                                        return (p.second.processed) || p.second.ready;
                                                    });
                if(all_ready){
                    FUNCINFO("Processing the following files");
FUNCWARN("This operation is WIP! No processing will be performed");
                    for(auto& p : block.second){
                        if(!p.second.processed && p.second.ready) FUNCINFO("\t" << p.first.string());
                    }

                    /*
                    duplicate existing Drover();
                    process_Drover();

                    for( all){
                        ... processed = true;
                        // Optionally remove???
                    }

                    merge Drover back();  // Easy to work around this if not needed, hard to emulate.
                    */

                    for(auto& p : block.second) p.second.processed = true;

                    // Optionally remove all processed files from input directories.
                    // ...
                }
            }
        }else{
            throw std::runtime_error("This functionality it not yet supported");
        }
            
    }

    return true;
}
