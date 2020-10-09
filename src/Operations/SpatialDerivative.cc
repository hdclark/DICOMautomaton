//SpatialDerivative.cc - A part of DICOMautomaton 2018. Written by hal clark.

#include <any>
#include <optional>
#include <functional>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>    

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../YgorImages_Functors/Grouping/Misc_Functors.h"
#include "../YgorImages_Functors/Processing/ImagePartialDerivative.h"
#include "../YgorImages_Functors/Compute/Volumetric_Neighbourhood_Sampler.h"
#include "SpatialDerivative.h"
#include "YgorImages.h"
#include "YgorString.h"       //Needed for GetFirstRegex(...)


OperationDoc OpArgDocSpatialDerivative(){
    OperationDoc out;
    out.name = "SpatialDerivative";

    out.desc = 
        "This operation estimates various partial derivatives (of pixel values) within 2D images.";

    out.args.emplace_back();
    out.args.back() = IAWhitelistOpArgDoc();
    out.args.back().name = "ImageSelection";
    out.args.back().default_val = "last";
    
    out.args.emplace_back();
    out.args.back().name = "Estimator";
    out.args.back().desc = "Controls the finite-difference partial derivative order or estimator used. All estimators are"
                           " centred and use mirror boundary conditions. First-order estimators include the basic"
                           " nearest-neighbour first derivative, and Roberts' cross, Prewitt, Sobel, Scharr estimators."
                           " 'XxY' denotes the size of the convolution kernel (i.e., the number of adjacent pixels"
                           " considered)."
                           " The only second-order estimator is the basic nearest-neighbour second derivative.";
    out.args.back().default_val = "Scharr-3x3";
    out.args.back().expected = true;
    out.args.back().examples = { "first",
                                 "Roberts-cross-3x3",
                                 "Prewitt-3x3",
                                 "Sobel-3x3",
                                 "Sobel-5x5",
                                 "Scharr-3x3",
                                 "Scharr-5x5",
                                 "second" };
    out.args.back().samples = OpArgSamples::Exhaustive;

    out.args.emplace_back();
    out.args.back().name = "Method";
    out.args.back().desc = "Controls partial derivative method. First-order derivatives can be row- or column-aligned,"
                           " Roberts' cross can be (+row,+col)-aligned or (-row,+col)-aligned."
                           " Second-order derivatives can be row-aligned, column-aligned, or 'cross' --meaning the"
                           " compound partial derivative."
                           " All methods support non-maximum-suppression for edge thinning, but currently only"
                           " the magnitude is output."
                           " All methods support magnitude (addition of orthogonal components in quadrature) and"
                           " orientation (in radians; [0,2pi) ).";
    out.args.back().default_val = "magnitude";
    out.args.back().expected = true;
    out.args.back().examples = { "row-aligned",
                                 "column-aligned",
                                 "prow-pcol-aligned",
                                 "nrow-pcol-aligned",
                                 "magnitude",
                                 "orientation",
                                 "non-maximum-suppression",
                                 "cross" };
    out.args.back().samples = OpArgSamples::Exhaustive;

    return out;
}

Drover SpatialDerivative(Drover DICOM_data,
                         const OperationArgPkg& OptArgs,
                         const std::map<std::string, std::string>&
                         /*InvocationMetadata*/,
                         const std::string& /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();
    const auto EstimatorStr = OptArgs.getValueStr("Estimator").value();
    const auto MethodStr = OptArgs.getValueStr("Method").value();

    //-----------------------------------------------------------------------------------------------------------------
    const auto regex_1st = Compile_Regex("^fi?r?s?t?$");
    const auto regex_2nd = Compile_Regex("^se?c?o?n?d?$");
    const auto regex_rcr3x3 = Compile_Regex("^ro?b?e?r?t?s?-?c?r?o?s?s?-?3x?3?$");
    const auto regex_pre3x3 = Compile_Regex("^pr?e?w?i?t?t?-?3x?3?$");
    const auto regex_sob3x3 = Compile_Regex("^so?b?e?l?-?3x?3?$");
    const auto regex_sch3x3 = Compile_Regex("^sc?h?a?r?r?-?3x?3?$");
    const auto regex_sob5x5 = Compile_Regex("^so?b?e?l?-?5x?5?$");
    const auto regex_sch5x5 = Compile_Regex("^sc?h?a?r?r?-?5x?5?$");

    const auto regex_row  = Compile_Regex("^ro?w?-?a?l?i?g?n?e?d?$");
    const auto regex_col  = Compile_Regex("^col?u?m?n?-?a?l?i?g?n?e?d?$");
    const auto regex_prpc = Compile_Regex("^pr?o?w?-?p?c?o?l?-?a?l?i?g?n?e?d?$");
    const auto regex_nrpc = Compile_Regex("^nr?o?w?-?p?c?o?l?u?m?n?-?a?l?i?g?n?e?d?$");
    const auto regex_mag  = Compile_Regex("^ma?g?n?i?t?u?d?e?$");
    const auto regex_orn  = Compile_Regex("^or?i?e?n?t?a?t?i?o?n?$");
    const auto regex_nms  = Compile_Regex("^no?n?-?m?a?x?i?m?u?m?-?s?u?p?p?r?e?s?s?i?o?n?$");
    const auto regex_crs  = Compile_Regex("^cro?s?s?$");

    auto IAs_all = All_IAs( DICOM_data );
    auto IAs = Whitelist( IAs_all, ImageSelectionStr );
    for(auto & iap_it : IAs){

        // Planar derivatives.
        ImagePartialDerivativeUserData ud;
        ud.order = PartialDerivativeEstimator::first;
        ud.method = PartialDerivativeMethod::row_aligned;

        if( std::regex_match(EstimatorStr, regex_1st) ){
            ud.order = PartialDerivativeEstimator::first;
        }else if( std::regex_match(EstimatorStr, regex_rcr3x3) ){
            ud.order = PartialDerivativeEstimator::Roberts_cross_3x3;
        }else if( std::regex_match(EstimatorStr, regex_pre3x3) ){
            ud.order = PartialDerivativeEstimator::Prewitt_3x3;
        }else if( std::regex_match(EstimatorStr, regex_sob3x3) ){
            ud.order = PartialDerivativeEstimator::Sobel_3x3;
        }else if( std::regex_match(EstimatorStr, regex_sch3x3) ){
            ud.order = PartialDerivativeEstimator::Scharr_3x3;
        }else if( std::regex_match(EstimatorStr, regex_sob5x5) ){
            ud.order = PartialDerivativeEstimator::Sobel_5x5;
        }else if( std::regex_match(EstimatorStr, regex_sch5x5) ){
            ud.order = PartialDerivativeEstimator::Scharr_5x5;
        }else if( std::regex_match(EstimatorStr, regex_2nd) ){
            ud.order = PartialDerivativeEstimator::second;
        }else{
            throw std::invalid_argument("Estimator argument '"_s + EstimatorStr + "' is not valid");
        }
        if( std::regex_match(MethodStr, regex_row) ){
            ud.method = PartialDerivativeMethod::row_aligned;
        }else if( std::regex_match(MethodStr, regex_col) ){
            ud.method = PartialDerivativeMethod::column_aligned;
        }else if( std::regex_match(MethodStr, regex_prpc) ){
            ud.method = PartialDerivativeMethod::prow_pcol_aligned;
        }else if( std::regex_match(MethodStr, regex_nrpc) ){
            ud.method = PartialDerivativeMethod::nrow_pcol_aligned;
        }else if( std::regex_match(MethodStr, regex_mag) ){
            ud.method = PartialDerivativeMethod::magnitude;
        }else if( std::regex_match(MethodStr, regex_orn) ){
            ud.method = PartialDerivativeMethod::orientation;
        }else if( std::regex_match(MethodStr, regex_nms) ){
            ud.method = PartialDerivativeMethod::non_maximum_suppression;
        }else if( std::regex_match(MethodStr, regex_crs) ){
            ud.method = PartialDerivativeMethod::cross;
        }else{
            throw std::invalid_argument("Method argument '"_s + MethodStr + "' is not valid");
        }

        if(!(*iap_it)->imagecoll.Process_Images_Parallel( GroupIndividualImages,
                                                          ImagePartialDerivative,
                                                          {}, {}, &ud )){
            throw std::runtime_error("Unable to compute in-plane partial derivative.");
        }
    }

    return DICOM_data;
}
