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
                           " range, across the entire line sample, averaging adjacent data if necessary."
                           "\n\n"
                           "Option 'purge-redundant-samples' ensures there is a single datum with the given abscissa"
                           " and ordinate range across the entire line sample, purging adjacent data if necessary."
                           "\n\n"
                           "Option 'rank-abscissa' replaces the abscissa values with their ordered rank number."
                           "\n\n"
                           "Option 'rank-ordinate' replaces the ordinate values with their ordered rank number."
                           "\n\n"
                           "Option 'swap-abscissa-and-ordinate' swaps the abscissa and ordinate for each individual"
                           " datum."
                           "\n\n"
                           "Option 'select-abscissa-range' trims all datum that fall outside of the provided abscissa"
                           " range. The selection is inclusive, so datum coinciding with one or both endpoints will be"
                           " retained."
                           "\n\n"
                           "Option 'crossings' locates the places where each line sample crosses the provided ordinate"
                           " value (using linear interpolation) and returns a new line sample containing only the"
                           " crossings."
                           "\n\n"
                           "Option 'peaks' locates the local peaks for each line sample (using linear interpolation)"
                           " and returns a new line sample containing only the peaks."
                           "\n\n"
                           "Option 'resample-equal-spacing' resamples each line sample into approximately"
                           " equally-spaced samples using linear interpolation. The number of outgoing samples needs"
                           " to be provided, e.g., 100."
                           "\n\n"
                           "Option 'multiply-scalar' multiplies all ordinates by the provided scalar factor."
                           "\n\n"
                           "Option 'sum-scalar' adds to all ordinates the provided scalar factor."
                           "\n\n"
                           "Option 'absolute-ordinate' replaces the ordinate of each line sample with its absolute"
                           " value."
                           "\n\n"
                           "Option 'purge-nonfinite' censors all datum with infinite or NaN coordinates."
//                           "\n\n"
//                           "Option '' "
//                           ""
//                           "\n\n"
//                           "Option '' "
//                           ""
//                           "\n\n"
//                           "Option '' "
                           ""
                           "";
    out.args.back().default_val = "";
    out.args.back().expected = true;
    out.args.back().examples = { "abscissa-offset()", 
                                 "ordinate-offset()",
                                 "average-coincident-values(0.5)",
                                 "purge-redundant-samples(0.5, 1.23)",
                                 "rank-abscissa()",
                                 "rank-ordinate()",
                                 "swap-abscissa-and-ordinate()",
                                 "select-abscissa-range(-1.23, 2.34)",
                                 "crossings(0.0)",
                                 "peaks()",
                                 "resample-equal-spacing(100)",
                                 "multiply-scalar(1.25)",
                                 "sum-scalar(-1.23)",
                                 "absolute-ordinate()",
                                 "purge-nonfinite",
//                                 "",
//                                 "",
                                 };

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
    const auto method_prs      = rg.insert("purge-redundant-samples");
    const auto method_ra       = rg.insert("rank-abscissa");
    const auto method_ro       = rg.insert("rank-ordinate");
    const auto method_sao      = rg.insert("swap-abscissa-and-ordinate");
    const auto method_sar      = rg.insert("select-abscissa-range");
    const auto method_cross    = rg.insert("crossings");
    const auto method_peaks    = rg.insert("peaks");
    const auto method_res      = rg.insert("resample-equal-spacing");
    const auto method_mults    = rg.insert("multiply-scalar");
    const auto method_sums     = rg.insert("sum-scalar");
    const auto method_abs      = rg.insert("absolute-ordinate");
    const auto method_pnf      = rg.insert("purge-nonfinite");
//    const auto method_         = rg.insert("");
//    const auto method_         = rg.insert("");
//    const auto method_         = rg.insert("");
//    const auto method_         = rg.insert("");
//    const auto method_         = rg.insert("");
//    const auto method_         = rg.insert("");
//    const auto method_         = rg.insert("");

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

        // purge-redundant-samples 
        }else if(rg.matches(pf.name, method_prs)){
            if(pf.parameters.size() != 2UL){
                throw std::invalid_argument("Incorrect number of arguments");
            }
            const auto x_eps = pf.parameters.at(0).number.value();
            const auto f_eps = pf.parameters.at(1).number.value();
            for(auto & lsp_it : LSs){
                (*lsp_it)->line = (*lsp_it)->line.Purge_Redundant_Samples(x_eps, f_eps);
            }

        // rank-abscissa 
        }else if(rg.matches(pf.name, method_ra)){
            if(pf.parameters.size() != 0UL){
                throw std::invalid_argument("Incorrect number of arguments");
            }
            for(auto & lsp_it : LSs){
                (*lsp_it)->line = (*lsp_it)->line.Rank_x();
            }

        // rank-ordinate
        }else if(rg.matches(pf.name, method_ro)){
            if(pf.parameters.size() != 0UL){
                throw std::invalid_argument("Incorrect number of arguments");
            }
            for(auto & lsp_it : LSs){
                (*lsp_it)->line = (*lsp_it)->line.Rank_y();
            }

        // swap-abscissa-and-ordinate
        }else if(rg.matches(pf.name, method_sao)){
            if(pf.parameters.size() != 0UL){
                throw std::invalid_argument("Incorrect number of arguments");
            }
            for(auto & lsp_it : LSs){
                (*lsp_it)->line = (*lsp_it)->line.Swap_x_and_y();
            }

        // select-abscissa-range
        }else if(rg.matches(pf.name, method_sar)){
            if(pf.parameters.size() != 2UL){
                throw std::invalid_argument("Incorrect number of arguments");
            }
            const auto l_low = pf.parameters.at(0).number.value();
            const auto l_high = pf.parameters.at(1).number.value();
            for(auto & lsp_it : LSs){
                (*lsp_it)->line = (*lsp_it)->line.Select_Those_Within_Inc(l_low, l_high);
            }

        // crossings
        }else if(rg.matches(pf.name, method_cross)){
            if(pf.parameters.size() != 1UL){
                throw std::invalid_argument("Incorrect number of arguments");
            }
            const auto t = pf.parameters.at(0).number.value();
            for(auto & lsp_it : LSs){
                (*lsp_it)->line = (*lsp_it)->line.Crossings(t);
            }

        // peaks
        }else if(rg.matches(pf.name, method_peaks)){
            if(pf.parameters.size() != 0UL){
                throw std::invalid_argument("Incorrect number of arguments");
            }
            for(auto & lsp_it : LSs){
                (*lsp_it)->line = (*lsp_it)->line.Peaks();
            }

        // resample-equal-spacing
        }else if(rg.matches(pf.name, method_res)){
            if(pf.parameters.size() != 1UL){
                throw std::invalid_argument("Incorrect number of arguments");
            }
            const auto l_n = static_cast<size_t>(pf.parameters.at(0).number.value());
            for(auto & lsp_it : LSs){
                (*lsp_it)->line = (*lsp_it)->line.Resample_Equal_Spacing(l_n);
            }

        // multiply-scalar
        }else if(rg.matches(pf.name, method_mults)){
            if(pf.parameters.size() != 1UL){
                throw std::invalid_argument("Incorrect number of arguments");
            }
            const auto x = pf.parameters.at(0).number.value();
            for(auto & lsp_it : LSs){
                (*lsp_it)->line = (*lsp_it)->line.Multiply_With(x);
            }

        // sum-scalar
        }else if(rg.matches(pf.name, method_sums)){
            if(pf.parameters.size() != 1UL){
                throw std::invalid_argument("Incorrect number of arguments");
            }
            const auto x = pf.parameters.at(0).number.value();
            for(auto & lsp_it : LSs){
                (*lsp_it)->line = (*lsp_it)->line.Sum_With(x);
            }

        // absolute-ordinate
        }else if(rg.matches(pf.name, method_abs)){
            if(pf.parameters.size() != 0UL){
                throw std::invalid_argument("Incorrect number of arguments");
            }
            for(auto & lsp_it : LSs){
                (*lsp_it)->line = (*lsp_it)->line.Apply_Abs();
            }

        // purge-nonfinite
        }else if(rg.matches(pf.name, method_pnf)){
            if(pf.parameters.size() != 0UL){
                throw std::invalid_argument("Incorrect number of arguments");
            }
            for(auto & lsp_it : LSs){
                (*lsp_it)->line = (*lsp_it)->line.Purge_Nonfinite_Samples();
            }


        }else{
            throw std::invalid_argument("Method '"_s + pf.name + "' not understood");
        }
    }

    return true;
}
