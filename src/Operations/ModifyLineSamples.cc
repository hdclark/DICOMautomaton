//ModifyLineSamples.cc - A part of DICOMautomaton 2025. Written by hal clark.

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

#include "Explicator.h"       //Needed for Explicator class.

#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorLog.h"
#include "YgorStats.h"        //Needed for Stats:: namespace.
#include "YgorString.h"       //Needed for GetFirstRegex(...)

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../Thread_Pool.h"
#include "../String_Parsing.h"

#include "ModifyLineSamples.h"

OperationDoc OpArgDocModifyLineSamples(){
    OperationDoc out;
    out.name = "ModifyLineSamples";

    out.tags.emplace_back("category: line sample processing");

    out.desc = 
        "This operation can apply a variety of processing algorithms to line samples, providing functionality"
        " that supports smoothing, normalization, arithmetical operations, and analysis of line samples.";


    out.args.emplace_back();
    out.args.back() = LSWhitelistOpArgDoc();
    out.args.back().name = "LineSelection";
    out.args.back().default_val = "last";
   

    out.args.emplace_back();
    out.args.back().name = "Methods";
    out.args.back().desc = "A list of methods to apply to the selected line samples."
                           " Multiple methods can be specified, and are applied sequentially in the order supplied."
                           " Note that some methods accept parameters."
                           "\n\n"
                           "Option 'abscissa-offset' finds the left-most abscissa value from all selected line"
                           " samples, and subtracts it from each individual line sample abscissa."
                           "\n\n"
                           "Option 'ordinate-offset' finds the bottom-most ordinate value from all selected line"
                           " samples, and subtracts it from each individual line sample ordinate."
                           "\n\n"
                           "Option 'average-coincident-values' ensures there is a single datum with the given abscissa"
                           " range, across the entire line sample, averaging adjcaent data if necessary."
                           "";
    out.args.back().default_val = "";
    out.args.back().expected = true;
    out.args.back().examples = { "abscissa-offset()", 
                                 "ordinate-offset()",
                                 "average-coincident-values()", };

    return out;
}

bool ModifyLineSamples(Drover &DICOM_data,
                         const OperationArgPkg& OptArgs,
                         std::map<std::string, std::string>& /*InvocationMetadata*/,
                         const std::string& /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto LineSelectionStr = OptArgs.getValueStr("LineSelection").value();
    const auto MethodsStr = OptArgs.getValueStr("Methods").value();

    //-----------------------------------------------------------------------------------------------------------------
    regex_group rg;
    const auto method_abs_off  = rg.insert("abscissa-offset");
    const auto method_ord_off  = rg.insert("ordinate-offset");
    const auto method_acv      = rg.insert("average-coincident-values");

/*
struct function_parameter {
    std::string raw;
    std::optional<double> number;
    bool is_fractional = false;
    bool is_percentage = false;
};

struct parsed_function {
    std::string name;
    std::vector<function_parameter> parameters;

    std::vector<parsed_function> children;
};
            const auto key = pf.parameters.at(0).raw;
            const auto val = pf.parameters.at(1).raw;
*/


    const auto pfs = parse_functions(MethodsStr);
    if(pfs.empty()){
        throw std::invalid_argument("No methods specified");
    }
    YLOGINFO("Proceeding with " << pfs.size() << " methods");

    auto LSs_all = All_LSs( DICOM_data );
    const auto LSs = Whitelist( LSs_all, LineSelectionStr );
    YLOGINFO("Selected " << LSs.size() << " line samples");

    for(const auto &pf : pfs){
        YLOGDEBUG("Attempting method '" << pf.name << "' now");
        if(!pf.children.empty()){
            throw std::invalid_argument("Children functions are not accepted");

        // abscissa-offset
        }else if(rg.matches(pf.name, method_abs_off)){
            if(pf.parameters.size() != 0UL){
                throw std::invalid_argument("Incorrect number of arguments");
            }

            // Gather information about all selected line samples.
            std::optional<double> min_x;
            for(auto & lsp_it : LSs){
                const auto x = (*lsp_it)->line.Get_Extreme_Datum_x().first.at(0);
                if(!min_x || (x < min_x.value())){
                    min_x = x;
                }
            }
            // Apply the offset.
            if(min_x){
                for(auto & lsp_it : LSs){
                    for(auto & s : (*lsp_it)->line.samples){
                        s.at(0) -= min_x.value();
                    }
                }
            }

        // ordinate-offset
        }else if(rg.matches(pf.name, method_ord_off)){
            if(pf.parameters.size() != 0UL){
                throw std::invalid_argument("Incorrect number of arguments");
            }

            std::optional<double> min_y;
            for(auto & lsp_it : LSs){
                const auto y = (*lsp_it)->line.Get_Extreme_Datum_y().first.at(2);
                if(!min_y || (y < min_y.value())){
                    min_y = y;
                }
            }
            if(min_y){
                for(auto & lsp_it : LSs){
                    for(auto & s : (*lsp_it)->line.samples){
                        s.at(2) -= min_y.value();
                    }
                }
            }

        // average-coincident-values 
        }else if(rg.matches(pf.name, method_acv)){
            if(pf.parameters.size() != 1UL){
                throw std::invalid_argument("Incorrect number of arguments");
            }
            const auto eps = pf.parameters.at(0).number.value();
            for(auto & lsp_it : LSs){
                (*lsp_it)->line.Average_Coincident_Data(eps);
            }

        }else{
            throw std::invalid_argument("Method '"_s + pf.name + "' not understood");
        }
    }

    return true;
}
