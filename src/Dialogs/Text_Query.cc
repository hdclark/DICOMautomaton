// Text_Query.cc - A part of DICOMautomaton 2021. Written by hal clark.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cctype>
#include <cstdlib>            //Needed for exit() calls.
#include <cstdio>
#include <cstddef>
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
#include "YgorString.h"       //Needed for GetFirstRegex(...)

#include "../Thread_Pool.h"
#include "../String_Parsing.h"

#include "Text_Query.h"


std::vector<user_query_packet_t>
interactive_query(std::vector<user_query_packet_t> qv){

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

    for(auto &uq : qv){
        if(uq.answered) continue;

        long int tries = 0;
        while(tries++ < 3){
            try{

                // Prepare the query parameters.
                std::map<std::string, std::string> key_vals;
                key_vals["TITLE"] = escape_for_quotes("DICOMautomaton User Query");
                key_vals["QUERY"] = escape_for_quotes(uq.query);
                key_vals["DEFAULT"] = "";

                if(uq.val_type == user_input_t::string){
                    key_vals["DEFAULT"] = escape_for_quotes( std::any_cast<std::string>(uq.val) );
                    
                }else if(uq.val_type == user_input_t::real){
                    key_vals["DEFAULT"] = escape_for_quotes( std::to_string( std::any_cast<double>(uq.val) ) );

                }else if(uq.val_type == user_input_t::integer){
                    key_vals["DEFAULT"] = escape_for_quotes( std::to_string( std::any_cast<int64_t>(uq.val) ) );

                }

                // Windows Powershell (VisualBasic).
                if(qm.count(query_method::pshell) != 0){

                    // Build the invocation.
                    const std::string proto_cmd = R"***(powershell -Command " & {Add-Type -AssemblyName Microsoft.VisualBasic; [Microsoft.VisualBasic.Interaction]::InputBox('$QUERY', '$TITLE', '$DEFAULT')}")***";
                    std::string cmd = ExpandMacros(proto_cmd, key_vals, "$");

                    // Query the user.
                    FUNCINFO("About to perform pshell command: '" << cmd << "'");
                    auto res = Execute_Command_In_Pipe(cmd);
                    res = escape_for_quotes(res); // Trim newlines and unprintable characters.
                    FUNCINFO("Received user input: '" << res << "'");

                    // Ensure the input fits the required data type.
                    if(uq.val_type == user_input_t::string){
                        uq.val = res;
                    }else if(uq.val_type == user_input_t::real){
                        uq.val = std::stod(res);
                    }else if(uq.val_type == user_input_t::integer){
                        uq.val = static_cast<int64_t>(std::stoll(res));
                    }

                    // Break out of the while loop on success.
                    uq.answered = true;
                    break;
                }

                // Zenity.
                if(qm.count(query_method::zenity) != 0){

                    // Build the invocation.
                    const std::string proto_cmd = R"***(zenity --title='@TITLE' --entry --text='@QUERY' --entry-text='@DEFAULT')***";
                    std::string cmd = ExpandMacros(proto_cmd, key_vals, "@");

                    // Query the user.
                    FUNCINFO("About to perform zenity command: '" << cmd << "'");
                    auto res = Execute_Command_In_Pipe(cmd);
                    res = escape_for_quotes(res); // Trim newlines and unprintable characters.
                    FUNCINFO("Received user input: '" << res << "'");

                    // Ensure the input fits the required data type.
                    if(uq.val_type == user_input_t::string){
                        uq.val = res;
                    }else if(uq.val_type == user_input_t::real){
                        uq.val = std::stod(res);
                    }else if(uq.val_type == user_input_t::integer){
                        uq.val = static_cast<int64_t>(std::stoll(res));
                    }

                    // Break out of the while loop on success.
                    uq.answered = true;
                    break;
                }

            }catch(const std::exception &e){
                FUNCWARN("User input failed: '" << e.what() << "'");
            }
        }

        if(3 <= tries){
            throw std::runtime_error("Unable to query for user input");
        }
    }

    return qv;
}
