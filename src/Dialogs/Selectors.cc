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
        pshell,
    };
    std::set<query_method> qm;
#if defined(_WIN32) || defined(_WIN64)
    qm.insert( query_method::pshell );
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

            // Windows Powershell (VisualBasic).
            if(qm.count(query_method::pshell) != 0){

                // Build the invocation.
                const std::string proto_cmd = R"***(powershell -Command " & { )***"
                                              R"***( Add-Type -AssemblyName System.Windows.Forms ; )***"
                                              R"***( $dialog = New-Object System.Windows.Forms.FolderBrowserDialog ; )***"
                                              R"***( $dialog.Description = '%QUERY' ; )***"
                                              R"***( if($dialog.ShowDialog() -eq 'OK'){ $dialog.SelectedPath }; )***";
                std::string cmd = ExpandMacros(proto_cmd, key_vals, "%");

                // Query the user.
                YLOGINFO("About to perform pshell command: '" << cmd << "'");
                auto res = Execute_Command_In_Pipe(cmd);
                res = escape_for_quotes(res); // Trim newlines and unprintable characters.
                YLOGINFO("Received user input: '" << res << "'");

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
