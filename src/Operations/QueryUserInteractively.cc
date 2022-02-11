//QueryUserInteractively.cc - A part of DICOMautomaton 2021. Written by hal clark.

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
#include "../Dialogs/Text_Query.h"

#include "QueryUserInteractively.h"


OperationDoc OpArgDocQueryUserInteractively(){
    OperationDoc out;
    out.name = "QueryUserInteractively";

    out.desc = 
        "This operation queries the user interactively, and then injects parameters into the global"
        " parameter table.";

    out.args.emplace_back();
    out.args.back().name = "Queries";
    out.args.back().desc = "A list of queries to pose to the user, where each function represents a single query."
                           "\n"
                           "There are currently three query types: 'integer', 'real', and 'string'. The only"
                           " difference being how the user input is validated."
                           "\n"
                           "All three functions have the same signature: the variable name (which is used to store"
                           " the user input), a query/instruction string that is provided to the user, and a"
                           " default/example value."
                           "\n"
                           "For example, 'integer(x, \"Input the day of the month.\", 0)' will query the user for"
                           " an integer with the instructions 'Input the day of the month.' and the result will be"
                           " stored in variable named 'x'."
                           "\n"
                           "Note that multiple queries can be separated by a semicolon, characters can be escaped"
                           " inside quotations using a backslash, and outer quotation marks are stripped away."
                           " Note that the query interface may also remove or transform problematic characters."
                           "";
    out.args.back().default_val = "";
    out.args.back().expected = true;
    out.args.back().examples = { "integer(var1, 'Please provide an integer', 123); real(var2, 'Please provide a float', -1.23); string(var3, 'Please provide a string', 'default text')" };

    return out;
}

bool QueryUserInteractively(Drover &DICOM_data,
                            const OperationArgPkg& OptArgs,
                            std::map<std::string, std::string>& InvocationMetadata,
                            const std::string& FilenameLex){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto QueriesStr = OptArgs.getValueStr("Queries").value();

    //-----------------------------------------------------------------------------------------------------------------
    const auto regex_integer = Compile_Regex("^in?t?e?g?e?r?$");
    const auto regex_real    = Compile_Regex("^re?a?l?$|^do?u?b?l?e?$|^fl?o?a?t?$");
    const auto regex_string  = Compile_Regex("^st?r?i?n?g?$");

    // Extract the queries.
    const auto pfs = parse_functions(QueriesStr);
    if(pfs.empty()){
        throw std::invalid_argument("No queries specified");
    }

    std::vector<user_query_packet_t> qv;
    for(const auto &pf : pfs){
        if(!pf.children.empty()){
            throw std::invalid_argument("Children functions are not accepted");
        }
        if(pf.parameters.size() != 3){
            throw std::invalid_argument("Incorrect number of arguments were provided");
        }

        qv.emplace_back();
        qv.back().answered = false;
        qv.back().key = pf.parameters.at(0).raw;
        qv.back().query = pf.parameters.at(1).raw;

        if(std::regex_match(pf.name, regex_real)){
            qv.back().val_type = user_input_t::real;
            qv.back().val = std::stod(pf.parameters.at(2).raw);

        }else if(std::regex_match(pf.name, regex_integer)){
            qv.back().val_type = user_input_t::integer;
            qv.back().val = static_cast<int64_t>(std::stoll(pf.parameters.at(2).raw));

        }else if(std::regex_match(pf.name, regex_string)){
            qv.back().val_type = user_input_t::string;
            qv.back().val = pf.parameters.at(2).raw;

        }else{
            throw std::invalid_argument("Unrecognized query type");
        }
    }

    FUNCINFO("Querying user "_s + std::to_string(qv.size()) + " times");
    
    // ... TODO ...

/*
                    qv.emplace_back();
                    qv.back().answered = false;
                    qv.back().key = "test key 1";
                    qv.back().query = "This is test query 1. What is your double input?";
                    qv.back().val_type = user_input_t::real;
                    qv.back().val = 1.23;

                    qv.emplace_back();
                    qv.back().answered = false;
                    qv.back().key = "test key 2";
                    qv.back().query = "This is test query 2. What is your integer input?";
                    qv.back().val_type = user_input_t::integer;
                    qv.back().val = static_cast<int64_t>(123);

                    qv.emplace_back();
                    qv.back().answered = false;
                    qv.back().key = "test key 3";
                    qv.back().query = "This is test query 3. What is your text input?";
                    qv.back().val_type = user_input_t::string;
                    qv.back().val = std::string("Some test value");
*/


    // Query the user.
    qv = interactive_query(qv);

    // Propagate the responses.
    for(auto &q : qv){
        if(!q.answered) continue;

        const auto key = q.key;
        std::string val;

        if(auto* v = std::any_cast<std::string>(&q.val);  v != nullptr){
            val = *v;
        }else if(auto* v = std::any_cast<int64_t>(&q.val);  v != nullptr){
            val = std::to_string(*v);
        }else if(auto* v = std::any_cast<double>(&q.val);  v != nullptr){
            val = std::to_string(*v);
        }

        InvocationMetadata[key] = val;
    }

    return true;
}

