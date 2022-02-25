//NotifyUser.cc - A part of DICOMautomaton 2022. Written by hal clark.

#include <asio.hpp>
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
#include "YgorStats.h"        //Needed for Stats:: namespace.
#include "YgorString.h"       //Needed for GetFirstRegex(...)

#include "Explicator.h"       //Needed for Explicator class.

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../Thread_Pool.h"
#include "../Write_File.h"
#include "../File_Loader.h"
#include "../String_Parsing.h"
#include "../Operation_Dispatcher.h"
#include "../Dialogs/Tray_Notification.h"

#include "NotifyUser.h"


OperationDoc OpArgDocNotifyUser(){
    OperationDoc out;
    out.name = "NotifyUser";

    out.desc = 
        "This operation attempts to notify the user using a tray notification.";

    out.args.emplace_back();
    out.args.back().name = "Notifications";
    out.args.back().desc = "A list of notifications to send to the user, where each function represents a single notification."
                           "\n"
                           "Currently only tray notifications are supported."
                           " Accepted syntax is 'tray(urgency, message, duration)' where urgency is 'low', 'medium', or"
                           " 'high' and duration is in milliseconds. Duration is optional."
                           " All notifications will be displayed concurrently."
                           "\n"
                           "For example, 'tray(\"low\", \"Calculation finished.\", 5000)' will send a low-urgency"
                           " notification that a calculation finished. It will be displayed for 5 seconds."
                           "";
    out.args.back().default_val = "";
    out.args.back().expected = true;
    out.args.back().examples = { "tray('low', 'Calculation finished')",
                                 "tray('medium', 'Minor issue detected', 5000); tray(high, 'Severe error encountered', 10000)" };

    return out;
}

bool NotifyUser(Drover &DICOM_data,
                const OperationArgPkg& OptArgs,
                std::map<std::string, std::string>& InvocationMetadata,
                const std::string& FilenameLex){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto NotificationsStr = OptArgs.getValueStr("Notifications").value();

    //-----------------------------------------------------------------------------------------------------------------
    const auto regex_tray   = Compile_Regex("^tr?a?y?$");
    const auto regex_low    = Compile_Regex("^lo?w?$");
    const auto regex_medium = Compile_Regex("^me?d?i?u?m?$");
    const auto regex_high   = Compile_Regex("^hi?g?h?$");

    // Extract the notifications.
    const auto pfs = parse_functions(NotificationsStr);
    if(pfs.empty()){
        throw std::invalid_argument("No notifications specified");
    }

    std::vector<notification_t> nv;
    for(const auto &pf : pfs){
        if(!pf.children.empty()){
            throw std::invalid_argument("Children functions are not accepted");
        }
        if( (pf.parameters.size() != 2)
        &&  (pf.parameters.size() != 3) ){
            throw std::invalid_argument("Incorrect number of arguments were provided");
        }
        if(!std::regex_match(pf.name, regex_tray)){
            throw std::invalid_argument("Only 'tray' notifications are currently supported.");
        }

        nv.emplace_back();

        const auto UrgencyStr = pf.parameters.at(0).raw;
        if(std::regex_match(UrgencyStr, regex_low)){
            nv.back().urgency = notification_urgency_t::low;
        }else if(std::regex_match(UrgencyStr, regex_medium)){
            nv.back().urgency = notification_urgency_t::medium;
        }else if(std::regex_match(UrgencyStr, regex_high)){
            nv.back().urgency = notification_urgency_t::high;
        }else{
            throw std::invalid_argument("Unrecognized urgency level");
        }

        nv.back().message = pf.parameters.at(1).raw;

        if(pf.parameters.size() == 3){
            nv.back().duration = static_cast<int32_t>(std::stoll(pf.parameters.at(2).raw));
        }
    }

    // Issue the notifications.
    FUNCINFO("Notifying user "_s + std::to_string(nv.size()) + " times");
    for(auto &n : nv){
        if(!tray_notification(n)){
            throw std::runtime_error("Notification failed");
        }
    }

    return true;
}

