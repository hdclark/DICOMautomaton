//NormalizeLineSamples.cc - A part of DICOMautomaton 2020. Written by hal clark.

#include <boost/interprocess/creation_tags.hpp>
#include <boost/interprocess/sync/named_mutex.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>            //Needed for exit() calls.
#include <exception>
#include <optional>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>    
#include <utility>            //Needed for std::pair.
#include <vector>

#include <sstream>
#include <thread>
#include <chrono>

#include "../Insert_Contours.h"
#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "NormalizeLineSamples.h"
#include "YgorFilesDirs.h"    //Needed for Does_File_Exist_And_Can_Be_Read(...), etc..
#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMathBSpline.h" //Needed for basis_spline class.
#include "YgorMathPlottingGnuplot.h" //Needed for YgorMathPlottingGnuplot::*.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorLog.h"
#include "YgorString.h"       //Needed for GetFirstRegex(...)


OperationDoc OpArgDocNormalizeLineSamples(){
    OperationDoc out;
    out.name = "NormalizeLineSamples";

    out.tags.emplace_back("category: line sample processing");

    out.desc = 
        "This operation scales line samples according to a user-provided normalization criteria.";


    out.notes.emplace_back(
        "Each line sample is independently normalized."
    );

    out.args.emplace_back();
    out.args.back() = LSWhitelistOpArgDoc();
    out.args.back().name = "LineSelection";
    out.args.back().default_val = "last";

    out.args.emplace_back();
    out.args.back().name = "Method";
    out.args.back().desc = "The type of normalization to apply."
                           " The currently supported options are 'area' and 'peak'."
                           ""
                           " 'Area' ensures that the total integrated area is equal to one by scaling the ordinate."
                           " 'Peak' ensures that the maximum ordinate is one and the minimum ordinate is zero.";
    out.args.back().default_val = "area";
    out.args.back().expected = true;
    out.args.back().examples = { "area", "peak" };
    out.args.back().samples = OpArgSamples::Exhaustive;

    return out;
}

bool NormalizeLineSamples(Drover &DICOM_data,
                            const OperationArgPkg& OptArgs,
                            std::map<std::string, std::string>& /*InvocationMetadata*/,
                            const std::string& /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto LineSelectionStr = OptArgs.getValueStr("LineSelection").value();
    const auto MethodStr        = OptArgs.getValueStr("Method").value();

    //-----------------------------------------------------------------------------------------------------------------
    const auto regex_area = Compile_Regex("^ar?e?a?$");
    const auto regex_peak = Compile_Regex("^pe?a?k?$");



    auto LSs_all = All_LSs( DICOM_data );
    const auto LSs = Whitelist( LSs_all, LineSelectionStr );

    for(auto & lsp_it : LSs){
        auto &ls = (*lsp_it)->line;

        if( std::regex_match(MethodStr, regex_area)){
            const auto I_max = ls.Integrate_Over_Kernel_unit()[0];
            const auto s = 1.0 / I_max;
            if(!std::isfinite(s)){
                throw std::runtime_error("Unable to normalize: required scaling factor is not finite. Refusing to continue.");
            }
            ls = ls.Multiply_With(s);

        }else if( std::regex_match(MethodStr, regex_peak)){
            const auto f = ls.Get_Extreme_Datum_y();
            const auto f_min = f.first[2];
            const auto f_max = f.second[2];
            const auto s = 1.0 / (f_max - f_min);
            if(!std::isfinite(s)){
                throw std::runtime_error("Unable to normalize: required scaling factor is not finite. Refusing to continue.");
            }
            ls = ls.Sum_With(-f_min).Multiply_With(s);

        }else{
            throw std::invalid_argument("Method not understood. Cannot continue.");
        }

        // Adjust metadata.
        ls.metadata["OrdinateScaling"] = "Normalized";
    }

    return true;
}
