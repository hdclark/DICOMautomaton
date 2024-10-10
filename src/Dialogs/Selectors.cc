// Selectors.cc - A part of DICOMautomaton 2021. Written by hal clark.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cctype>
#include <cstdlib>            //Needed for exit() calls.
#include <cstdio>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <any>
#include <optional>
#include <fstream>
#include <functional>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <limits>
#include <list>
#include <map>
#include <set>
#include <memory>
#include <numeric>
#include <regex>
#include <stdexcept>
#include <string>    
#include <tuple>
#include <type_traits>
#include <utility>            //Needed for std::pair.
#include <vector>
#include <chrono>
#include <future>
#include <mutex>
#include <shared_mutex>
#include <initializer_list>
#include <thread>

#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorLog.h"
#include "YgorString.h"       //Needed for GetFirstRegex(...)

#include "../Thread_Pool.h"
#include "../String_Parsing.h"

#include "Selectors.h"


std::string
select_directory(std::string query_text){

    enum class query_method {
        zenity,
        pshell1,
        pshell2,
    };
    std::set<query_method> qm;
#if defined(_WIN32) || defined(_WIN64)
    qm.insert( query_method::pshell1 );
    qm.insert( query_method::pshell2 );
#endif
#if defined(__linux__)
    qm.insert( query_method::zenity );
#endif
#if defined(__APPLE__) && defined(__MACH__)
    qm.insert( query_method::zenity );
#endif

    std::string out;

    int64_t tries = 0;
    while(tries++ < 3){
        try{

            // Prepare the query parameters.
            std::map<std::string, std::string> key_vals;
            key_vals["TITLE"] = escape_for_quotes("DICOMautomaton Directory Selection");
            key_vals["QUERY"] = escape_for_quotes(query_text);

            // Windows Powershell (option 1 - older-style folder selection, but more portable).
            if(qm.count(query_method::pshell1) != 0){

                // Build the invocation.
                //
                // NOTE: 'options' are listed at
                // https://learn.microsoft.com/en-us/windows/win32/api/shlobj_core/ns-shlobj_core-browseinfoa
                // and should be bitwise-or'd together.
                const std::string proto_cmd = R"***(powershell -Command " & { )***"
                                              R"***( $shell = new-object -comobject Shell.Application ; )***"
                                              R"***( $options = 0x0 ; )***"
                                              R"***( $options += 0x4 ; )***" // Status Text
                                              R"***( $options += 0x10 ; )***" // Edit Box, where the user can type a path.
                                              R"***( $options += 0x20 ; )***" // Validate the path.
                                              R"***( $options += 0x40 ; )***" // New Dialog Style; resizeable window, drag-and-drop, etc.
                                              R"***( $options += 0x80 ; )***" // Browse Include URLs.
                                              //R"***( $options += 0x200 ; )***" // No New Folder Button.
                                              R"***( $options += 0x8000 ; )***" // Shareable; include remote shares.
                                              //R"***( $options += 0x10000 ; )***" // Browse File Junctions; e.g., look inside zip files.
                                              R"***( $rootdir = 0x0 ; )***" // Desktop, the 'root of the namespace'.
                                              R"***( $dir = $shell.BrowseForFolder(0, '%QUERY', $options, $rootdir) ; )***"
                                              R"***( if($dir){ write-host $dir.Self.Path() }else{ write-host 'dcmausercancelled' } ; )***"
                                              R"***( }" )***";
                std::string cmd = ExpandMacros(proto_cmd, key_vals, "%");

                // Query the user.
                YLOGINFO("About to perform pshell command: '" << cmd << "'");
                auto res = Execute_Command_In_Pipe(cmd);
                res = escape_for_quotes(res); // Trim newlines and unprintable characters.
                YLOGINFO("Received user input: '" << res << "'");

                if(res == "dcmausercancelled"){
                    throw std::runtime_error("User cancelled directory selection");
                }

                // Break out of the while loop on success.
                if(!res.empty()){
                    out = res;
                    break;
                }
            }

            // Windows Powershell (option 2 - slightly less user-friendly).
            if(qm.count(query_method::pshell2) != 0){

                // Build the invocation.
                const std::string proto_cmd = R"***(powershell -Command " & { )***"
                                              R"***( Add-Type -AssemblyName System.Windows.Forms ; )***"
                                              R"***( $dialog = New-Object System.Windows.Forms.FolderBrowserDialog ; )***"
                                              R"***( $dialog.Description = '%QUERY' ; )***"
                                              R"***( if($dialog.ShowDialog() -eq 'OK'){ write-host $dialog.SelectedPath }else{ write-host 'dcmausercancelled' } ; )***"
                                              R"***( }" )***";
                std::string cmd = ExpandMacros(proto_cmd, key_vals, "%");

                // Query the user.
                YLOGINFO("About to perform pshell command: '" << cmd << "'");
                auto res = Execute_Command_In_Pipe(cmd);
                res = escape_for_quotes(res); // Trim newlines and unprintable characters.
                YLOGINFO("Received user input: '" << res << "'");

                if(res == "dcmausercancelled"){
                    throw std::runtime_error("User cancelled directory selection");
                }

                // Break out of the while loop on success.
                if(!res.empty()){
                    out = res;
                    break;
                }
            }

            // Zenity.
            if(qm.count(query_method::zenity) != 0){

                // Build the invocation.
                const std::string proto_cmd = R"***(: | zenity --title='@QUERY' --file-selection --directory)***";
                std::string cmd = ExpandMacros(proto_cmd, key_vals, "@");

                // Query the user.
                YLOGINFO("About to perform zenity command: '" << cmd << "'");
                auto res = Execute_Command_In_Pipe(cmd);
                res = escape_for_quotes(res); // Trim newlines and unprintable characters.
                YLOGINFO("Received user input: '" << res << "'");

                // Break out of the while loop on success.
                if(!res.empty()){
                    out = res;
                    break;
                }
            }

        }catch(const std::exception &e){
            YLOGWARN("User input (directory selection) failed: '" << e.what() << "'");
        }
    }

    if(3 <= tries){
        throw std::runtime_error("Unable to query user for directory selection");
    }

    return out;
}
