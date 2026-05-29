//For.cc - A part of DICOMautomaton 2024. Written by hal clark.

#include <algorithm>
#include <optional>
#include <fstream>
#include <iterator>
#include <list>
#include <set>
#include <map>
#include <memory>
#include <mutex>
#include <regex>
#include <set> 
#include <stdexcept>
#include <string>    
#include <utility>            //Needed for std::pair.
#include <list>
#include <memory>
#include <type_traits>

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
#include "../Operation_Dispatcher.h"
#include "../Metadata.h"
#include "../String_Parsing.h"

#include "For.h"


OperationDoc OpArgDocFor() {
    OperationDoc out;
    out.name = "For";

    out.tags.emplace_back("category: meta");
    out.tags.emplace_back("category: control flow");

    out.desc = "This operation is a control flow meta-operation that invokes children operations"
               " multiple times.";

    out.notes.emplace_back(
        "If this operation has no children, this operation will evaluate to a no-op."
    );

    out.args.emplace_back();
    out.args.back().name = "Key";
    out.args.back().desc = "If a non-empty value is provided, the value or number associated with each loop is stored"
                           " in the global parameter table using this key. If the key already exists in the global"
                           " parameter table, it is temporarily stored during the loop and restored afterward."
                           "\n\n"
                           "Note: altering the value of the key stored in global parameter table in one iteration"
                           " will not impact other iterations of the loop.";
    out.args.back().default_val = "i";
    out.args.back().expected = true;
    out.args.back().examples = { "i", "j", "k", "x", "val", "abc", "123" };


    out.args.emplace_back();
    out.args.back().name = "EachOf";
    out.args.back().desc = "Loop over the provided comma-separated list, invoking children operations once for every"
                           " item in the order provided."
                           " The item in each loop is optionally inserted into the global parameter table."
                           "\n\n"
                           "Note that this option is used for 'discrete' loop mode and cannot be combined when"
                           " any 'counter' loop mode parameters are provided."
                           "";
    out.args.back().default_val = "";
    out.args.back().expected = false;
    out.args.back().examples = { "a,b,c,d,e,f",
                                 "1,2,3,4,5",
                                 "InstanceCreationDate,StudyData,SeriesDate,AcquisitionDate,ContentDate", 
                                 "x,123,Modality" };

    out.args.emplace_back();
    out.args.back().name = "Begin";
    out.args.back().desc = "'Counter' loop mode parameter."
                           ""
                           " This is the value which the counter will first start with."
                           " The counter is incremented until the end value is reached."
                           " Children operations are invoked once per counter value."
                           " The counter value in each loop is optionally inserted into the global parameter table."
                           "\n\n"
                           "Note that this option is used for 'counter' loop mode and cannot be combined when"
                           " any 'discrete' loop mode parameters are provided."
                           "";
    out.args.back().default_val = "";
    out.args.back().expected = false;
    out.args.back().examples = { "0", "1", "-10", "100.23" }; 


    out.args.emplace_back();
    out.args.back().name = "End";
    out.args.back().desc = "'Counter' loop mode parameter."
                           " This value controls when the loop terminates."
                           " Note that whether this parameter is treated inclusively (i.e., '<=') or exclusively"
                           " (i.e., '<'); is controlled by the Inclusivity parameter;"
                           " the default is to be inclusive."
                           "\n\n"
                           "Note that this option is used for 'counter' loop mode and cannot be combined when"
                           " any 'discrete' loop mode parameters are provided."
                           "";
    out.args.back().default_val = "";
    out.args.back().expected = false;
    out.args.back().examples = { "100", "3", "-5", "200.23" }; 


    out.args.emplace_back();
    out.args.back().name = "Increment";
    out.args.back().desc = "'Counter' loop mode parameter."
                           " Controls the step size."
                           " The counter value in each loop is optionally inserted into the global parameter table."
                           "\n\n"
                           "Note that this option is used for 'counter' loop mode and cannot be combined when"
                           " any 'discrete' loop mode parameters are provided."
                           "";
    out.args.back().default_val = "";
    out.args.back().expected = false;
    out.args.back().examples = { "1", "2", "-10", "1.23" }; 


    out.args.emplace_back();
    out.args.back().name = "Inclusivity";
    out.args.back().desc = "'Counter' loop mode parameter."
                           " Controls whether the end value is treated inclusively (i.e., '<=') or exclusively"
                           " (i.e., '<'). The default is to be inclusive."
                           "\n\n"
                           "Note that this option is only used for 'counter' loop mode."
                           "";
    out.args.back().default_val = "inclusive";
    out.args.back().expected = true;
    out.args.back().examples = { "inclusive", "exclusive" }; 
    out.args.back().samples = OpArgSamples::Exhaustive;

    return out;
}

bool For(Drover &DICOM_data,
         const OperationArgPkg& OptArgs,
         std::map<std::string, std::string>& InvocationMetadata,
         const std::string& FilenameLex){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto KeyOpt = OptArgs.getValueStr("Key");

    const auto EachOfOpt = OptArgs.getValueStr("EachOf");

    const auto BegOpt = OptArgs.getValueStr("Begin");
    const auto EndOpt = OptArgs.getValueStr("End");
    const auto IncOpt = OptArgs.getValueStr("Increment");
    const auto InclusivityStr = OptArgs.getValueStr("Inclusivity").value();

    //-----------------------------------------------------------------------------------------------------------------
    const auto regex_inclusive = Compile_Regex("^in?c?l?u?s?i?v?e?$");
    const auto regex_exclusive = Compile_Regex("^ex?c?l?u?s?i?v?e?$");

    const bool end_inclusive = std::regex_match(InclusivityStr, regex_inclusive);
    const bool end_exclusive = std::regex_match(InclusivityStr, regex_exclusive);

    if( end_inclusive == end_exclusive ){
        throw std::invalid_argument("Inclusivity not understood");
    }

    const bool use_discrete = !!EachOfOpt;

    std::optional<double> beg_opt;
    std::optional<double> end_opt;
    std::optional<double> inc_opt;
    if(BegOpt) beg_opt = std::stod( BegOpt.value() );
    if(EndOpt) end_opt = std::stod( EndOpt.value() );
    if(IncOpt) inc_opt = std::stod( IncOpt.value() );

    const bool use_counter = !!beg_opt || !!end_opt || !!inc_opt;
    const bool all_counter = !!beg_opt && !!end_opt && !!inc_opt;
    if( use_counter && !all_counter ){
        throw std::invalid_argument("Invalid or insufficient parameters provided for counter mode.");
    }
    if( use_counter ){
        const auto l = end_opt.value() - beg_opt.value();
        const auto i = inc_opt.value();
        if( !std::isfinite(l) || !std::isfinite(i) ){
            throw std::invalid_argument("One or more counter parameters are not finite.");
        }
        if( (l != 0.0)
        && (std::signbit(l) != std::signbit(i)) ){
            throw std::invalid_argument("Increment direction will result in infinite loop.");
        }
        const auto n = l / i;
        if( 1'000'000 < n ){
            throw std::invalid_argument("Loop requires too many iterations.");
        }
    }

    // Store the original state of the counter key in the global parameter table.
    std::optional<std::string> orig_val;
    if(KeyOpt){
        orig_val = get_as<std::string>(InvocationMetadata, KeyOpt.value());
    }

    bool ret = true;
    if(false){
    }else if(use_discrete){
        YLOGDEBUG("Proceeding with discrete loop mode");

        // Tokenize the item list.
        std::list<std::string> tokens;
        if(EachOfOpt){
            for(auto t : SplitStringToVector(EachOfOpt.value(), ',', 'd')){
                tokens.emplace_back(t);
            }
        }

        // Invoke the children.
        for(const auto &t : tokens){
            YLOGDEBUG("Looping with counter = '" << t << "'");
            if(KeyOpt) InvocationMetadata[KeyOpt.value()] = t;

            ret = Operation_Dispatcher(DICOM_data, InvocationMetadata, FilenameLex, OptArgs.getChildren());
            if(!ret) break;
        }

    }else if( use_counter ){
        YLOGDEBUG("Proceeding with counter loop mode");
            
        const auto beg = beg_opt.value();
        const auto end = end_opt.value();
        const auto inc = inc_opt.value();
        const bool is_increasing = !std::signbit(inc);
        const auto is_done = [&](double x){
            return (end_inclusive) ? (is_increasing ? (x <= end) : (x >= end))
                                   : (is_increasing ? (x <  end) : (x >  end));
        };
        for(double i = beg; is_done(i); i += inc){
            YLOGDEBUG("Looping with counter = " << i);
            if(KeyOpt) InvocationMetadata[KeyOpt.value()] = to_string_max_precision(i);

            ret = Operation_Dispatcher(DICOM_data, InvocationMetadata, FilenameLex, OptArgs.getChildren());
            if(!ret) break;
        }

    }else{
        throw std::invalid_argument("Neither counter mode nor discrete mode parameters were provided.");
    }

    // Restore the counter key to its original state in the global parameter table.
    if( KeyOpt ){
        InvocationMetadata.erase(KeyOpt.value());
    }
    if( orig_val
    &&  KeyOpt ){
        InvocationMetadata[KeyOpt.value()] = orig_val.value();
    }

    return ret;
}

