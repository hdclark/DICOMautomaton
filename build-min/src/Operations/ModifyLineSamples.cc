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
                           "\n\n"
                           "Option 'histogram' generates a histogram with N equal-width bins. Each bin's height is"
                           " the sum of the samples that appear within the bin's domain. This method can also optionally"
                           " add an outline surrounding the histogram bins for visualization purposes by supplying a"
                           " second numerical argument that evaluates to 'true' (e.g., 1)."
                           "\n\n"
                           "Option 'moving-average-two-sided-15-sample' computes the \"Spencer's\" 15-sample moving average,"
                           " averaging the ordinates. This is a convolution that effectively acts like a low-pass"
                           " filter, letting polynomials of order 3 or less through approximately as-is."
                           "\n\n"
                           "Option 'moving-average-two-sided-23-sample' computes the \"Henderson's\" 23-sample moving average,"
                           " averaging the ordinates. This is a convolution that effectively acts like a low-pass"
                           " filter, letting polynomials of order 3 or less through approximately as-is."
                           "\n\n"
                           "Option 'moving-average-two-sided-equal-weighting' computes a $(2N+1)$-sample moving average,"
                           " averaging the ordinates with equal weighting. The discrete window size $N$ must be supplied."
                           "\n\n"
                           "Option 'moving-average-two-sided-gaussian-weighting' computes a moving average,"
                           " averaging the ordinates using gaussian weighting. The width of the gaussian ($\\sigma$)"
                           " must be supplied. Applying consecutively emulates applying once with a larger width."
                           "\n\n"
                           "Option 'moving-median-filter-two-sided-equal-weighting' computes a $(2N+1)$-sample moving"
                           " median filter of the ordinate values. All ordinates are weighted identically."
                           " The discrete window size $N$ must be supplied."
                           "\n\n"
                           "Option 'moving-median-filter-two-sided-gaussian-weighting' computes a $(2N+1)$-sample moving"
                           " median filter of the ordinate values, using gaussian weighting to scale ordinates based on"
                           " their distance. The width of the gaussian ($\\sigma$) must be supplied."
                           "\n\n"
                           "Option 'moving-median-filter-two-sided-triangular-weighting' computes a $(2N+1)$-sample moving"
                           " median filter of the ordinate values. All ordinates are weighted linearly based on their"
                           " rank position relative to the averaging point."
                           " The discrete window size $N$ must be supplied."
                           "\n\n"
                           "Option 'moving-variance-two-sided' calculates an unbiased estimate of a population's"
                           " variance over a window of ($2N+1$) samples. Endpoint variance estimation uses fewer"
                           " samples (min = N) and have higher variance. Setting N to be 5 or higher is recommended."
                           " The discrete window size $N$ must be supplied."
                           "\n\n"
                           "Option 'derivative-forward-finite-differences' calculates the discrete derivative using"
                           " forward finite differences. The right-side endpoint uses backward finite differences to"
                           " handle the boundary. Data should be reasonably smooth -- no interpolation is used."
                           "\n\n"
                           "Option 'derivative-backward-finite-differences' calculates the discrete derivative using"
                           " backward finite differences. The left-side endpoint uses forward finite differences to"
                           " handle the boundary. Data should be reasonably smooth -- no interpolation is used."
                           "\n\n"
                           "Option 'derivative-backward-centered-differences' calculates the discrete derivative using"
                           " centered finite differences. The endpoints use either forward or backward finite"
                           " differences to handle the boundaries. Data should be reasonably smooth -- no"
                           " interpolation is used."
                           "\n\n"
                           "Option 'local-signed-curvature-three-sample' calculates the local signed curvature at"
                           " each sample using the two nearest-neighbour samples."
                           " Endpoints are discarded. Curvature here is the tangent circle's inverse radius, and the"
                           " sign indicates the direction of concavity."
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
                                 "purge-nonfinite()",
                                 "histogram(100)",
                                 "histogram(100, 1)",
                                 "moving-average-two-sided-15-sample()",
                                 "moving-average-two-sided-23-sample()",
                                 "moving-average-two-sided-equal-weighting(5)",
                                 "moving-average-two-sided-gaussian-weighting(1.23)",
                                 "moving-median-filter-two-sided-equal-weighting(5)",
                                 "moving-median-filter-two-sided-gaussian-weighting(1.23)",
                                 "moving-median-filter-two-sided-triangular-weighting(5)",
                                 "moving-variance-two-sided(5)",
                                 "derivative-forward-finite-differences()",
                                 "derivative-backward-finite-differences()",
                                 "derivative-centered-finite-differences()",
                                 "local-signed-curvature-three-sample()",
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
    const auto method_hist     = rg.insert("histogram");
    const auto method_ma15     = rg.insert("moving-average-two-sided-15-sample");
    const auto method_ma23     = rg.insert("moving-average-two-sided-23-sample");
    const auto method_maew     = rg.insert("moving-average-two-sided-equal-weighting");
    const auto method_magw     = rg.insert("moving-average-two-sided-gaussian-weighting");
    const auto method_mmfew    = rg.insert("moving-median-filter-two-sided-equal-weighting");
    const auto method_mmfgw    = rg.insert("moving-median-filter-two-sided-gaussian-weighting");
    const auto method_mmftw    = rg.insert("moving-median-filter-two-sided-triangular-weighting");
    const auto method_mv       = rg.insert("moving-variance-two-sided");
    const auto method_dffd     = rg.insert("derivative-forward-finite-differences");
    const auto method_dbfd     = rg.insert("derivative-backward-finite-differences");
    const auto method_dcfd     = rg.insert("derivative-centered-finite-differences");
    const auto method_lsc3s    = rg.insert("local-signed-curvature-three-sample");

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
        YLOGINFO("Attempting method '" << pf.name << "' now");
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

        // histogram
        }else if(rg.matches(pf.name, method_hist)){
            if( (pf.parameters.size() != 1UL)
            &&  (pf.parameters.size() != 2UL) ){
                throw std::invalid_argument("Incorrect number of arguments");
            }
            const auto n = pf.parameters.at(0).number.value();
            const auto N = static_cast<int64_t>(n);

            bool explicit_bins = false;
            if(pf.parameters.size() == 2UL){
                const auto b = pf.parameters.at(1).number.value();
                explicit_bins = static_cast<bool>(b);
            }

            for(auto & lsp_it : LSs){
                (*lsp_it)->line = (*lsp_it)->line.Histogram_Equal_Sized_Bins(N, explicit_bins);
            }

        // moving-average-two-sided-15-sample
        }else if(rg.matches(pf.name, method_ma15)){
            if(pf.parameters.size() != 0UL){
                throw std::invalid_argument("Incorrect number of arguments");
            }
            for(auto & lsp_it : LSs){
                (*lsp_it)->line = (*lsp_it)->line.Moving_Average_Two_Sided_Spencers_15_point();
            }

        // moving-average-two-sided-23-sample
        }else if(rg.matches(pf.name, method_ma23)){
            if(pf.parameters.size() != 0UL){
                throw std::invalid_argument("Incorrect number of arguments");
            }
            for(auto & lsp_it : LSs){
                (*lsp_it)->line = (*lsp_it)->line.Moving_Average_Two_Sided_Hendersons_23_point();
            }

        // moving-average-two-sided-equal-weighting
        }else if(rg.matches(pf.name, method_maew)){
            if(pf.parameters.size() != 1UL){
                throw std::invalid_argument("Incorrect number of arguments");
            }
            const auto n = pf.parameters.at(0).number.value();
            const auto N = static_cast<int64_t>(n);

            for(auto & lsp_it : LSs){
                (*lsp_it)->line = (*lsp_it)->line.Moving_Average_Two_Sided_Equal_Weighting(N);
            }

        // moving-average-two-sided-gaussian-weighting
        }else if(rg.matches(pf.name, method_magw)){
            if(pf.parameters.size() != 1UL){
                throw std::invalid_argument("Incorrect number of arguments");
            }
            const auto sigma = pf.parameters.at(0).number.value();

            for(auto & lsp_it : LSs){
                (*lsp_it)->line = (*lsp_it)->line.Moving_Average_Two_Sided_Gaussian_Weighting(sigma);
            }

        // moving-median-filter-two-sided-equal-weighting
        }else if(rg.matches(pf.name, method_mmfew)){
            if(pf.parameters.size() != 1UL){
                throw std::invalid_argument("Incorrect number of arguments");
            }
            const auto n = pf.parameters.at(0).number.value();
            const auto N = static_cast<int64_t>(n);

            for(auto & lsp_it : LSs){
                (*lsp_it)->line = (*lsp_it)->line.Moving_Median_Filter_Two_Sided_Equal_Weighting(N);
            }

        // moving-median-filter-two-sided-gaussian-weighting
        }else if(rg.matches(pf.name, method_mmfgw)){
            if(pf.parameters.size() != 1UL){
                throw std::invalid_argument("Incorrect number of arguments");
            }
            const auto sigma = pf.parameters.at(0).number.value();

            for(auto & lsp_it : LSs){
                (*lsp_it)->line = (*lsp_it)->line.Moving_Median_Filter_Two_Sided_Gaussian_Weighting(sigma);
            }

        // moving-median-filter-two-sided-triangular-weighting
        }else if(rg.matches(pf.name, method_mmftw)){
            if(pf.parameters.size() != 1UL){
                throw std::invalid_argument("Incorrect number of arguments");
            }
            const auto n = pf.parameters.at(0).number.value();
            const auto N = static_cast<int64_t>(n);

            for(auto & lsp_it : LSs){
                (*lsp_it)->line = (*lsp_it)->line.Moving_Median_Filter_Two_Sided_Triangular_Weighting(N);
            }

        // moving-variance-two-sided
        }else if(rg.matches(pf.name, method_mv)){
            if(pf.parameters.size() != 1UL){
                throw std::invalid_argument("Incorrect number of arguments");
            }
            const auto n = pf.parameters.at(0).number.value();
            const auto N = static_cast<int64_t>(n);

            for(auto & lsp_it : LSs){
                (*lsp_it)->line = (*lsp_it)->line.Moving_Variance_Two_Sided(N);
            }

        // derivative-forward-finite-differences
        }else if(rg.matches(pf.name, method_dffd)){
            if(pf.parameters.size() != 0UL){
                throw std::invalid_argument("Incorrect number of arguments");
            }
            for(auto & lsp_it : LSs){
                (*lsp_it)->line = (*lsp_it)->line.Derivative_Forward_Finite_Differences();
            }

        // derivative-backward-finite-differences
        }else if(rg.matches(pf.name, method_dbfd)){
            if(pf.parameters.size() != 0UL){
                throw std::invalid_argument("Incorrect number of arguments");
            }
            for(auto & lsp_it : LSs){
                (*lsp_it)->line = (*lsp_it)->line.Derivative_Backward_Finite_Differences();
            }

        // derivative-centered-finite-differences
        }else if(rg.matches(pf.name, method_dcfd)){
            if(pf.parameters.size() != 0UL){
                throw std::invalid_argument("Incorrect number of arguments");
            }
            for(auto & lsp_it : LSs){
                (*lsp_it)->line = (*lsp_it)->line.Derivative_Centered_Finite_Differences();
            }

        // local-signed-curvature-three-sample
        }else if(rg.matches(pf.name, method_lsc3s)){
            if(pf.parameters.size() != 0UL){
                throw std::invalid_argument("Incorrect number of arguments");
            }
            for(auto & lsp_it : LSs){
                (*lsp_it)->line = (*lsp_it)->line.Local_Signed_Curvature_Three_Datum();
            }


        }else{
            throw std::invalid_argument("Method '"_s + pf.name + "' not understood");
        }
    }

    return true;
}
