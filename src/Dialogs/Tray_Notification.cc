//Tray_Notification.cc - A part of DICOMautomaton 2022. Written by hal clark.

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
#include "YgorLog.h"
#include "YgorString.h"       //Needed for GetFirstRegex(...)

#include "../Thread_Pool.h"
#include "../String_Parsing.h"

#include "Tray_Notification.h"

static
bool win_cmd_is_available(const std::string &name){
    const auto l_name = escape_for_quotes(name);
    std::stringstream ss;
    ss << R"***((help ")***" << l_name << R"***(" 1> nul 2> nul || exit 0 ))***"
       << R"***( && where ")***" << l_name << R"***(" 1> nul 2> nul)***"
       << R"***( && echo cmd_is_available )***";
    const auto cmd = ss.str();
    auto res = Execute_Command_In_Pipe(cmd);
    res = escape_for_quotes(res); // Trim newlines and unprintable characters.
    return (res == "cmd_is_available");
}

static
bool sh_cmd_is_available(const std::string &name){
    std::stringstream ss;
    ss << ": | if command -v '" << escape_for_quotes(name) << "' 1>/dev/null 2>/dev/null ; then "
       << "  echo cmd_is_available ; "
       << "fi";
    const auto cmd = ss.str();
    auto res = Execute_Command_In_Pipe(cmd);
    res = escape_for_quotes(res); // Trim newlines and unprintable characters.
    return (res == "cmd_is_available");
}


bool
tray_notification(const notification_t &n){

    enum class query_method {
        notifysend,
        zenity,
        pshell,
        osascript,
    };
    std::set<query_method> qm;
#if defined(_WIN32) || defined(_WIN64)
    YLOGINFO("Assuming powershell is available");
    qm.insert( query_method::pshell );

    if( win_cmd_is_available("zenity")
    ||  win_cmd_is_available("zenity.exe")){
        YLOGINFO("zenity is available");
        qm.insert( query_method::zenity );
    }
#endif
#if defined(__linux__)
    if(sh_cmd_is_available("notify-send")){
        YLOGINFO("notify-send is available");
        qm.insert( query_method::notifysend );
    }
    if(sh_cmd_is_available("zenity")){
        YLOGINFO("zenity is available");
        qm.insert( query_method::zenity );
    }
#endif
#if defined(__APPLE__) && defined(__MACH__)
    if(sh_cmd_is_available("notify-send")){
        YLOGINFO("notify-send is available");
        qm.insert( query_method::notifysend );
    }
    if(sh_cmd_is_available("zenity")){
        YLOGINFO("zenity is available");
        qm.insert( query_method::zenity );
    }
    if(sh_cmd_is_available("osascript")){
        YLOGINFO("zenity is available");
        qm.insert( query_method::osascript );
    }
#endif


    int64_t tries = 0;
    while(tries++ < 3){
        try{

            // Prepare the query parameters.
            std::map<std::string, std::string> key_vals;
            key_vals["TITLE"]   = escape_for_quotes("DICOMautomaton");
            key_vals["MESSAGE"] = escape_for_quotes(n.message);

            key_vals["DURATION_MS"] = escape_for_quotes(std::to_string(n.duration));
            key_vals["DURATION_S"]  = escape_for_quotes(std::to_string(n.duration/1000));

            if(n.urgency == notification_urgency_t::low){
                key_vals["PS_URGENCY"] = escape_for_quotes("Information");
                key_vals["OA_URGENCY"] = escape_for_quotes("Information");
                key_vals["NS_URGENCY"] = escape_for_quotes("low");
                key_vals["Z_URGENCY"]  = escape_for_quotes("info");
            }else if(n.urgency == notification_urgency_t::medium){
                key_vals["PS_URGENCY"] = escape_for_quotes("Warning");
                key_vals["OA_URGENCY"] = escape_for_quotes("Warning");
                key_vals["NS_URGENCY"] = escape_for_quotes("normal");
                key_vals["Z_URGENCY"]  = escape_for_quotes("warning");
            }else if(n.urgency == notification_urgency_t::high){
                key_vals["PS_URGENCY"] = escape_for_quotes("Error");
                key_vals["OA_URGENCY"] = escape_for_quotes("Error");
                key_vals["NS_URGENCY"] = escape_for_quotes("critical");
                key_vals["Z_URGENCY"]  = escape_for_quotes("error");
            }else{
                throw std::logic_error("Unrecognized urgency category");
            }


            // Windows Powershell (VisualBasic).
            if(qm.count(query_method::pshell) != 0){
                // Build the invocation.
                std::stringstream ss;
                ss << R"***(powershell)***"
                   << R"***( -WindowStyle hidden)***"
                   << R"***( -ExecutionPolicy bypass)***"
                   << R"***( -NonInteractive)***"
                   << R"***( -Command "& {)***"
                   << R"***(  [void] [System.Reflection.Assembly]::LoadWithPartialName('System.Windows.Forms');)***"
                   << R"***(  $objNotifyIcon=New-Object System.Windows.Forms.NotifyIcon;)***"
                   << R"***(  $objNotifyIcon.Icon=[system.drawing.systemicons]::'@PS_URGENCY';)***"
                   << R"***(  $objNotifyIcon.BalloonTipIcon='None';)***"
                   << R"***(  $objNotifyIcon.BalloonTipTitle='@TITLE';)***"
                   << R"***(  $objNotifyIcon.BalloonTipText='@MESSAGE';)***"
                   << R"***(  $objNotifyIcon.Visible=$True;)***"
                   << R"***(  $objNotifyIcon.ShowBalloonTip(@DURATION_MS);)***"
                   << R"***( }")***";
                const std::string proto_cmd = ss.str();
                std::string cmd = ExpandMacros(proto_cmd, key_vals, "@");

                // Notify the user.
                const auto exec_cmd = [](std::string cmd){
                    Execute_Command_In_Pipe(cmd);
                    return;
                };
                YLOGINFO("About to perform pshell command: '" << cmd << "'");
                std::thread t(exec_cmd, cmd);
                t.detach();
                break;
            }

            // osascript.
            if(qm.count(query_method::osascript) != 0){

                // Build the invocation.
                std::stringstream ss;
                ss << R"***(: | osascript -e ')***"
                   << R"***( display notification "@MESSAGE" )***"
                   << R"***( with title "@TITLE" )***"
                   << R"***( subtitle "@OA_URGENCY" ' )***"
                   << R"***( 1>/dev/null 2>/dev/null && echo successful )***";
                const std::string proto_cmd = ss.str();
                std::string cmd = ExpandMacros(proto_cmd, key_vals, "@");

                // Notify the user.
                YLOGINFO("About to perform osascript command: '" << cmd << "'");
                auto res = Execute_Command_In_Pipe(cmd);
                res = escape_for_quotes(res); // Trim newlines and unprintable characters.

                if(res == "successful") break;
            }

            // Notify-send.
            if(qm.count(query_method::notifysend) != 0){

                // Build the invocation.
                std::stringstream ss;
                ss << ": | notify-send "
                   << "  --app-name='DICOMautomaton' "
                   << "  --urgency='@NS_URGENCY' "
                   << "  --expire-time='@DURATION_MS' "
                   << "  '@TITLE' "
                   << "  '@MESSAGE' 1>/dev/null 2>/dev/null && echo successful";
                const std::string proto_cmd = ss.str();
                std::string cmd = ExpandMacros(proto_cmd, key_vals, "@");

                // Notify the user.
                YLOGINFO("About to perform notify-send command: '" << cmd << "'");
                auto res = Execute_Command_In_Pipe(cmd);
                res = escape_for_quotes(res); // Trim newlines and unprintable characters.

                if(res == "successful") break;
            }

            // Zenity.
            if(qm.count(query_method::zenity) != 0){

                // Build the invocation.
                //
                // Note that Zenity blocks for the duration, and also doesn't return a useful return value.
                // At this point we pragmatically always assume the notification reached the user.
                std::stringstream ss;
                ss << " : | zenity "
                   << "   --title='@TITLE' "
                   << "   --notification "
                   << "   --timeout='@DURATION_S' "
                   << "   --window-icon='@Z_URGENCY' "
                   << "   --text='@MESSAGE' 1>/dev/null 2>/dev/null";
                const std::string proto_cmd = ss.str();
                std::string cmd = ExpandMacros(proto_cmd, key_vals, "@");

                const auto exec_cmd = [](std::string cmd){
                    Execute_Command_In_Pipe(cmd);
                    return;
                };
                YLOGINFO("About to perform zenity command: '" << cmd << "'");
                std::thread t(exec_cmd, cmd);
                t.detach();
                break;
            }

        }catch(const std::exception &e){
            YLOGWARN("User notification failed: '" << e.what() << "'");
        }
    }

    return (tries < 3);
}

