//VolumetricSpatialDerivative.cc - A part of DICOMautomaton 2019. Written by hal clark.

#include <experimental/any>
#include <experimental/optional>
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
#include "VolumetricSpatialDerivative.h"
#include "YgorImages.h"
#include "YgorString.h"       //Needed for GetFirstRegex(...)


OperationDoc OpArgDocVolumetricSpatialDerivative(void){
    OperationDoc out;
    out.name = "VolumetricSpatialDerivative";

    out.desc = 
        "This operation estimates various partial derivatives (of pixel values) within 3D rectilinear image arrays.";

    out.notes.emplace_back(
        "The provided image collection must be rectilinear."
    );

    out.args.emplace_back();
    out.args.back() = IAWhitelistOpArgDoc();
    out.args.back().name = "ImageSelection";
    out.args.back().default_val = "last";
    

    out.args.emplace_back();
    out.args.back().name = "NormalizedROILabelRegex";
    out.args.back().desc = "A regex matching ROI labels/names to consider. The default will match"
                      " all available ROIs. Be aware that input spaces are trimmed to a single space."
                      " If your ROI name has more than two sequential spaces, use regex to avoid them."
                      " All ROIs have to match the single regex, so use the 'or' token if needed."
                      " Regex is case insensitive and uses extended POSIX syntax.";
    out.args.back().default_val = ".*";
    out.args.back().expected = true;
    out.args.back().examples = { ".*", ".*Body.*", "Body", "Gross_Liver",
                            R"***(.*Left.*Parotid.*|.*Right.*Parotid.*|.*Eye.*)***",
                            R"***(Left Parotid|Right Parotid)***" };


    out.args.emplace_back();
    out.args.back().name = "ROILabelRegex";
    out.args.back().desc = "A regex matching ROI labels/names to consider. The default will match"
                      " all available ROIs. Be aware that input spaces are trimmed to a single space."
                      " If your ROI name has more than two sequential spaces, use regex to avoid them."
                      " All ROIs have to match the single regex, so use the 'or' token if needed."
                      " Regex is case insensitive and uses extended POSIX syntax.";
    out.args.back().default_val = ".*";
    out.args.back().expected = true;
    out.args.back().examples = { ".*", ".*body.*", "body", "Gross_Liver",
                            R"***(.*left.*parotid.*|.*right.*parotid.*|.*eyes.*)***",
                            R"***(left_parotid|right_parotid)***" };


    out.args.emplace_back();
    out.args.back().name = "Channel";
    out.args.back().desc = "The channel to operated on (zero-based)."
                           " Negative values will cause all channels to be operated on.";
    out.args.back().default_val = "0";
    out.args.back().expected = true;
    out.args.back().examples = { "-1",
                                 "0",
                                 "1" };


    out.args.emplace_back();
    out.args.back().name = "Estimator";
    out.args.back().desc = "Controls the finite-difference partial derivative order or estimator used. All estimators are"
                           " centred and use mirror boundary conditions. First-order estimators include the basic"
                           " nearest-neighbour first derivative and Sobel estimators."
                           " 'XxYxZ' denotes the size of the convolution kernel (i.e., the number of adjacent pixels"
                           " considered).";
    out.args.back().default_val = "Sobel-3x3x3";
    out.args.back().expected = true;
    out.args.back().examples = { "first",
                                 //"Roberts-cross-3x3x3",
                                 //"Prewitt-3x3x3",
                                 "Sobel-3x3x3" };
                                 //"Sobel-5x5x5",
                                 //"Scharr-3x3x3",
                                 //"Scharr-5x5x5",
                                 //"second" };

    out.args.emplace_back();
    out.args.back().name = "Method";
    out.args.back().desc = "Controls partial derivative method. First-order derivatives can be row-, column-, or image-aligned,"
                           " All methods also support magnitude (addition of orthogonal components in quadrature).";
    out.args.back().default_val = "magnitude";
    out.args.back().expected = true;
    out.args.back().examples = { "row-aligned",
                                 "column-aligned",
                                 "image-aligned",
                                 //"prow-pcol-aligned",
                                 //"nrow-pcol-aligned",
                                 "magnitude" };
                                 //"orientation",
                                 //"non-maximum-suppression",
                                 //"cross" };

    return out;
}

Drover VolumetricSpatialDerivative(Drover DICOM_data, OperationArgPkg OptArgs, std::map<std::string,std::string> /*InvocationMetadata*/, std::string /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();

    const auto NormalizedROILabelRegex = OptArgs.getValueStr("NormalizedROILabelRegex").value();
    const auto ROILabelRegex = OptArgs.getValueStr("ROILabelRegex").value();

    const auto Channel = std::stol( OptArgs.getValueStr("Channel").value() );

    const auto EstimatorStr = OptArgs.getValueStr("Estimator").value();
    const auto MethodStr = OptArgs.getValueStr("Method").value();

    //-----------------------------------------------------------------------------------------------------------------
    const auto regex_1st = Compile_Regex("^fi?r?s?t?$");
    //const auto regex_2nd = Compile_Regex("^se?c?o?n?d?$");
    //const auto regex_rcr3x3x3 = Compile_Regex("^ro?b?e?r?t?s?-?c?r?o?s?s?-?3x?3?x?3?$");
    //const auto regex_pre3x3x3 = Compile_Regex("^pr?e?w?i?t?t?-?3x?3?x?3?$");
    const auto regex_sob3x3x3 = Compile_Regex("^so?b?e?l?-?3x?3?x?3?$");
    //const auto regex_sch3x3x3 = Compile_Regex("^sc?h?a?r?r?-?3x?3?x?3?$");
    //const auto regex_sob5x5x5 = Compile_Regex("^so?b?e?l?-?5x?5?x?5?$");
    //const auto regex_sch5x5x5 = Compile_Regex("^sc?h?a?r?r?-?5x?5?x?5?$");

    const auto regex_row  = Compile_Regex("^ro?w?-?a?l?i?g?n?e?d?$");
    const auto regex_col  = Compile_Regex("^col?u?m?n?-?a?l?i?g?n?e?d?$");
    const auto regex_img  = Compile_Regex("^im?a?g?e?-?a?l?i?g?n?e?d?$");
    //const auto regex_prpc = Compile_Regex("^pr?o?w?-?p?c?o?l?-?a?l?i?g?n?e?d?$");
    //const auto regex_nrpc = Compile_Regex("^nr?o?w?-?p?c?o?l?u?m?n?-?a?l?i?g?n?e?d?$");
    const auto regex_mag  = Compile_Regex("^ma?g?n?i?t?u?d?e?$");
    //const auto regex_orn  = Compile_Regex("^or?i?e?n?t?a?t?i?o?n?$");
    //const auto regex_nms  = Compile_Regex("^no?n?-?m?a?x?i?m?u?m?-?s?u?p?p?r?e?s?s?i?o?n?$");
    //const auto regex_crs  = Compile_Regex("^cro?s?s?$");


    auto cc_all = All_CCs( DICOM_data );
    auto cc_ROIs = Whitelist( cc_all, { { "ROIName", ROILabelRegex },
                                        { "NormalizedROIName", NormalizedROILabelRegex } } );
    if(cc_ROIs.empty()){
        throw std::invalid_argument("No contours selected. Cannot continue.");
    }

    auto IAs_all = All_IAs( DICOM_data );
    auto IAs = Whitelist( IAs_all, ImageSelectionStr );
    for(auto & iap_it : IAs){

        ComputeVolumetricNeighbourhoodSamplerUserData ud;
        ud.channel = Channel;
        ud.neighbourhood = ComputeVolumetricNeighbourhoodSamplerUserData::Neighbourhood::Selection;


        if(false){
        }else if( std::regex_match(EstimatorStr, regex_1st) ){
            ud.voxel_triplets = {{ {  0,  0,  0 },    // 0
                                   { -1,  0,  0 },    // 1
                                   {  1,  0,  0 },    // 2
                                   {  0, -1,  0 },    // 3
                                   {  0,  1,  0 },    // 4
                                   {  0,  0, -1 },    // 5
                                   {  0,  0,  1 } }}; // 6
            if(false){
            }else if( std::regex_match(MethodStr, regex_row) ){
                ud.f_reduce = [](std::vector<double> &shtl) -> double {
                                  const auto col_m = std::isfinite(shtl[3]) ? shtl[3] : shtl[0];
                                  const auto col_p = std::isfinite(shtl[4]) ? shtl[4] : shtl[0];
                                  return (col_p - col_m) * 0.5;
                              };
            }else if( std::regex_match(MethodStr, regex_col) ){
                ud.f_reduce = [](std::vector<double> &shtl) -> double {
                                  const auto row_m = std::isfinite(shtl[1]) ? shtl[1] : shtl[0];
                                  const auto row_p = std::isfinite(shtl[2]) ? shtl[2] : shtl[0];
                                  return (row_p - row_m) * 0.5;
                              };

            }else if( std::regex_match(MethodStr, regex_img) ){
                ud.f_reduce = [](std::vector<double> &shtl) -> double {
                                  const auto img_m = std::isfinite(shtl[5]) ? shtl[5] : shtl[0];
                                  const auto img_p = std::isfinite(shtl[6]) ? shtl[6] : shtl[0];
                                  return (img_p - img_m) * 0.5;
                              };

            //}else if( std::regex_match(MethodStr, regex_prpc) ){
            //    throw std::invalid_argument("Method '"_s + MethodStr + "' and estimator '"_s + EstimatorStr + "' currently cannot be combined.");
            //}else if( std::regex_match(MethodStr, regex_nrpc) ){
            //    throw std::invalid_argument("Method '"_s + MethodStr + "' and estimator '"_s + EstimatorStr + "' currently cannot be combined.");
            }else if( std::regex_match(MethodStr, regex_mag) ){
                ud.f_reduce = [](std::vector<double> &shtl) -> double {
                                  const auto col_m = std::isfinite(shtl[3]) ? shtl[3] : shtl[0];
                                  const auto col_p = std::isfinite(shtl[4]) ? shtl[4] : shtl[0];
                                  const auto row_m = std::isfinite(shtl[1]) ? shtl[1] : shtl[0];
                                  const auto row_p = std::isfinite(shtl[2]) ? shtl[2] : shtl[0];
                                  const auto img_m = std::isfinite(shtl[5]) ? shtl[5] : shtl[0];
                                  const auto img_p = std::isfinite(shtl[6]) ? shtl[6] : shtl[0];
                                  return std::hypot( (col_p - col_m) * 0.5,
                                                     (row_p - row_m) * 0.5,
                                                     (img_p - img_m) * 0.5 );
                              };

            //}else if( std::regex_match(MethodStr, regex_orn) ){
            //    throw std::invalid_argument("Method '"_s + MethodStr + "' and estimator '"_s + EstimatorStr + "' currently cannot be combined.");
            //}else if( std::regex_match(MethodStr, regex_nms) ){
            //    throw std::invalid_argument("Method '"_s + MethodStr + "' and estimator '"_s + EstimatorStr + "' currently cannot be combined.");
            //}else if( std::regex_match(MethodStr, regex_crs) ){
            //    throw std::invalid_argument("Method '"_s + MethodStr + "' and estimator '"_s + EstimatorStr + "' currently cannot be combined.");
            }else{
                throw std::invalid_argument("Method argument '"_s + MethodStr + "' is not valid");
            }

        }else if( std::regex_match(EstimatorStr, regex_sob3x3x3) ){
            ud.voxel_triplets = {{ { -1, -1, -1 },    //  0
                                   { -1,  0, -1 },    //  1
                                   { -1,  1, -1 },    //  2
                                   {  0, -1, -1 },    //  3
                                   {  0,  0, -1 },    //  4
                                   {  0,  1, -1 },    //  5
                                   {  1, -1, -1 },    //  6
                                   {  1,  0, -1 },    //  7
                                   {  1,  1, -1 },    //  8

                                   { -1, -1,  0 },    //  9
                                   { -1,  0,  0 },    // 10
                                   { -1,  1,  0 },    // 11
                                   {  0, -1,  0 },    // 12
                                   {  0,  0,  0 },    // 13
                                   {  0,  1,  0 },    // 14
                                   {  1, -1,  0 },    // 15
                                   {  1,  0,  0 },    // 16
                                   {  1,  1,  0 },    // 17

                                   { -1, -1,  1 },    // 18
                                   { -1,  0,  1 },    // 19
                                   { -1,  1,  1 },    // 20
                                   {  0, -1,  1 },    // 21
                                   {  0,  0,  1 },    // 22
                                   {  0,  1,  1 },    // 23
                                   {  1, -1,  1 },    // 24
                                   {  1,  0,  1 },    // 25
                                   {  1,  1,  1 } }}; // 26

            // Note: The convolution kernel used here was adapted from
            // https://en.wikipedia.org/wiki/Sobel_operator#Extension_to_other_dimensions (accessed 20190226).
            const auto row_aligned = [](std::vector<double> &shtl) -> double {
                                    const auto r_m_c_m_i_m = std::isfinite(shtl[ 0]) ? shtl[ 0] : shtl[13];
                                    //const auto r_m_c_0_i_m = std::isfinite(shtl[ 1]) ? shtl[ 1] : shtl[13];
                                    const auto r_m_c_p_i_m = std::isfinite(shtl[ 2]) ? shtl[ 2] : shtl[13];

                                    const auto r_0_c_m_i_m = std::isfinite(shtl[ 3]) ? shtl[ 3] : shtl[13];
                                    //const auto r_0_c_0_i_m = std::isfinite(shtl[ 4]) ? shtl[ 4] : shtl[13];
                                    const auto r_0_c_p_i_m = std::isfinite(shtl[ 5]) ? shtl[ 5] : shtl[13];

                                    const auto r_p_c_m_i_m = std::isfinite(shtl[ 6]) ? shtl[ 6] : shtl[13];
                                    //const auto r_p_c_0_i_m = std::isfinite(shtl[ 7]) ? shtl[ 7] : shtl[13];
                                    const auto r_p_c_p_i_m = std::isfinite(shtl[ 8]) ? shtl[ 8] : shtl[13];

                                    const auto r_m_c_m_i_0 = std::isfinite(shtl[ 9]) ? shtl[ 9] : shtl[13];
                                    //const auto r_m_c_0_i_0 = std::isfinite(shtl[10]) ? shtl[10] : shtl[13];
                                    const auto r_m_c_p_i_0 = std::isfinite(shtl[11]) ? shtl[11] : shtl[13];

                                    const auto r_0_c_m_i_0 = std::isfinite(shtl[12]) ? shtl[12] : shtl[13];
                                    //const auto r_0_c_0_i_0 = shtl[13];
                                    const auto r_0_c_p_i_0 = std::isfinite(shtl[14]) ? shtl[14] : shtl[13];

                                    const auto r_p_c_m_i_0 = std::isfinite(shtl[15]) ? shtl[15] : shtl[13];
                                    //const auto r_p_c_0_i_0 = std::isfinite(shtl[16]) ? shtl[16] : shtl[13];
                                    const auto r_p_c_p_i_0 = std::isfinite(shtl[17]) ? shtl[17] : shtl[13];

                                    const auto r_m_c_m_i_p = std::isfinite(shtl[18]) ? shtl[18] : shtl[13];
                                    //const auto r_m_c_0_i_p = std::isfinite(shtl[19]) ? shtl[19] : shtl[13];
                                    const auto r_m_c_p_i_p = std::isfinite(shtl[20]) ? shtl[20] : shtl[13];

                                    const auto r_0_c_m_i_p = std::isfinite(shtl[21]) ? shtl[21] : shtl[13];
                                    //const auto r_0_c_0_i_p = std::isfinite(shtl[22]) ? shtl[22] : shtl[13];
                                    const auto r_0_c_p_i_p = std::isfinite(shtl[23]) ? shtl[23] : shtl[13];

                                    const auto r_p_c_m_i_p = std::isfinite(shtl[24]) ? shtl[24] : shtl[13];
                                    //const auto r_p_c_0_i_p = std::isfinite(shtl[25]) ? shtl[25] : shtl[13];
                                    const auto r_p_c_p_i_p = std::isfinite(shtl[26]) ? shtl[26] : shtl[13];

                                    return (  1.0 * r_p_c_p_i_p
                                            + 1.0 * r_m_c_p_i_p
                                            + 1.0 * r_p_c_p_i_m
                                            + 1.0 * r_m_c_p_i_m

                                            + 2.0 * r_0_c_p_i_m
                                            + 2.0 * r_0_c_p_i_p
                                            + 2.0 * r_m_c_p_i_0
                                            + 2.0 * r_p_c_p_i_0

                                            + 4.0 * r_0_c_p_i_0

                                            - 1.0 * r_p_c_m_i_p
                                            - 1.0 * r_m_c_m_i_p
                                            - 1.0 * r_p_c_m_i_m
                                            - 1.0 * r_m_c_m_i_m

                                            - 2.0 * r_0_c_m_i_m
                                            - 2.0 * r_0_c_m_i_p
                                            - 2.0 * r_m_c_m_i_0
                                            - 2.0 * r_p_c_m_i_0

                                            - 4.0 * r_0_c_m_i_0)
                                           / 32.0;
            };

            const auto col_aligned = [](std::vector<double> &shtl) -> double {
                                    const auto r_m_c_m_i_m = std::isfinite(shtl[ 0]) ? shtl[ 0] : shtl[13];
                                    const auto r_m_c_0_i_m = std::isfinite(shtl[ 1]) ? shtl[ 1] : shtl[13];
                                    const auto r_m_c_p_i_m = std::isfinite(shtl[ 2]) ? shtl[ 2] : shtl[13];

                                    //const auto r_0_c_m_i_m = std::isfinite(shtl[ 3]) ? shtl[ 3] : shtl[13];
                                    //const auto r_0_c_0_i_m = std::isfinite(shtl[ 4]) ? shtl[ 4] : shtl[13];
                                    //const auto r_0_c_p_i_m = std::isfinite(shtl[ 5]) ? shtl[ 5] : shtl[13];

                                    const auto r_p_c_m_i_m = std::isfinite(shtl[ 6]) ? shtl[ 6] : shtl[13];
                                    const auto r_p_c_0_i_m = std::isfinite(shtl[ 7]) ? shtl[ 7] : shtl[13];
                                    const auto r_p_c_p_i_m = std::isfinite(shtl[ 8]) ? shtl[ 8] : shtl[13];

                                    const auto r_m_c_m_i_0 = std::isfinite(shtl[ 9]) ? shtl[ 9] : shtl[13];
                                    const auto r_m_c_0_i_0 = std::isfinite(shtl[10]) ? shtl[10] : shtl[13];
                                    const auto r_m_c_p_i_0 = std::isfinite(shtl[11]) ? shtl[11] : shtl[13];

                                    //const auto r_0_c_m_i_0 = std::isfinite(shtl[12]) ? shtl[12] : shtl[13];
                                    //const auto r_0_c_0_i_0 = shtl[13];
                                    //const auto r_0_c_p_i_0 = std::isfinite(shtl[14]) ? shtl[14] : shtl[13];

                                    const auto r_p_c_m_i_0 = std::isfinite(shtl[15]) ? shtl[15] : shtl[13];
                                    const auto r_p_c_0_i_0 = std::isfinite(shtl[16]) ? shtl[16] : shtl[13];
                                    const auto r_p_c_p_i_0 = std::isfinite(shtl[17]) ? shtl[17] : shtl[13];

                                    const auto r_m_c_m_i_p = std::isfinite(shtl[18]) ? shtl[18] : shtl[13];
                                    const auto r_m_c_0_i_p = std::isfinite(shtl[19]) ? shtl[19] : shtl[13];
                                    const auto r_m_c_p_i_p = std::isfinite(shtl[20]) ? shtl[20] : shtl[13];

                                    //const auto r_0_c_m_i_p = std::isfinite(shtl[21]) ? shtl[21] : shtl[13];
                                    //const auto r_0_c_0_i_p = std::isfinite(shtl[22]) ? shtl[22] : shtl[13];
                                    //const auto r_0_c_p_i_p = std::isfinite(shtl[23]) ? shtl[23] : shtl[13];

                                    const auto r_p_c_m_i_p = std::isfinite(shtl[24]) ? shtl[24] : shtl[13];
                                    const auto r_p_c_0_i_p = std::isfinite(shtl[25]) ? shtl[25] : shtl[13];
                                    const auto r_p_c_p_i_p = std::isfinite(shtl[26]) ? shtl[26] : shtl[13];

                                    return (  1.0 * r_p_c_p_i_p
                                            + 1.0 * r_p_c_m_i_p
                                            + 1.0 * r_p_c_p_i_m
                                            + 1.0 * r_p_c_m_i_m

                                            + 2.0 * r_p_c_0_i_m
                                            + 2.0 * r_p_c_0_i_p
                                            + 2.0 * r_p_c_m_i_0
                                            + 2.0 * r_p_c_p_i_0

                                            + 4.0 * r_p_c_0_i_0

                                            - 1.0 * r_m_c_p_i_p
                                            - 1.0 * r_m_c_m_i_p
                                            - 1.0 * r_m_c_p_i_m
                                            - 1.0 * r_m_c_m_i_m

                                            - 2.0 * r_m_c_0_i_m
                                            - 2.0 * r_m_c_0_i_p
                                            - 2.0 * r_m_c_m_i_0
                                            - 2.0 * r_m_c_p_i_0

                                            - 4.0 * r_m_c_0_i_0)
                                           / 32.0;
            };

            const auto img_aligned = [](std::vector<double> &shtl) -> double {
                                    const auto r_m_c_m_i_m = std::isfinite(shtl[ 0]) ? shtl[ 0] : shtl[13];
                                    const auto r_m_c_0_i_m = std::isfinite(shtl[ 1]) ? shtl[ 1] : shtl[13];
                                    const auto r_m_c_p_i_m = std::isfinite(shtl[ 2]) ? shtl[ 2] : shtl[13];

                                    const auto r_0_c_m_i_m = std::isfinite(shtl[ 3]) ? shtl[ 3] : shtl[13];
                                    const auto r_0_c_0_i_m = std::isfinite(shtl[ 4]) ? shtl[ 4] : shtl[13];
                                    const auto r_0_c_p_i_m = std::isfinite(shtl[ 5]) ? shtl[ 5] : shtl[13];

                                    const auto r_p_c_m_i_m = std::isfinite(shtl[ 6]) ? shtl[ 6] : shtl[13];
                                    const auto r_p_c_0_i_m = std::isfinite(shtl[ 7]) ? shtl[ 7] : shtl[13];
                                    const auto r_p_c_p_i_m = std::isfinite(shtl[ 8]) ? shtl[ 8] : shtl[13];

                                    //const auto r_m_c_m_i_0 = std::isfinite(shtl[ 9]) ? shtl[ 9] : shtl[13];
                                    //const auto r_m_c_0_i_0 = std::isfinite(shtl[10]) ? shtl[10] : shtl[13];
                                    //const auto r_m_c_p_i_0 = std::isfinite(shtl[11]) ? shtl[11] : shtl[13];

                                    //const auto r_0_c_m_i_0 = std::isfinite(shtl[12]) ? shtl[12] : shtl[13];
                                    //const auto r_0_c_0_i_0 = shtl[13];
                                    //const auto r_0_c_p_i_0 = std::isfinite(shtl[14]) ? shtl[14] : shtl[13];

                                    //const auto r_p_c_m_i_0 = std::isfinite(shtl[15]) ? shtl[15] : shtl[13];
                                    //const auto r_p_c_0_i_0 = std::isfinite(shtl[16]) ? shtl[16] : shtl[13];
                                    //const auto r_p_c_p_i_0 = std::isfinite(shtl[17]) ? shtl[17] : shtl[13];

                                    const auto r_m_c_m_i_p = std::isfinite(shtl[18]) ? shtl[18] : shtl[13];
                                    const auto r_m_c_0_i_p = std::isfinite(shtl[19]) ? shtl[19] : shtl[13];
                                    const auto r_m_c_p_i_p = std::isfinite(shtl[20]) ? shtl[20] : shtl[13];

                                    const auto r_0_c_m_i_p = std::isfinite(shtl[21]) ? shtl[21] : shtl[13];
                                    const auto r_0_c_0_i_p = std::isfinite(shtl[22]) ? shtl[22] : shtl[13];
                                    const auto r_0_c_p_i_p = std::isfinite(shtl[23]) ? shtl[23] : shtl[13];

                                    const auto r_p_c_m_i_p = std::isfinite(shtl[24]) ? shtl[24] : shtl[13];
                                    const auto r_p_c_0_i_p = std::isfinite(shtl[25]) ? shtl[25] : shtl[13];
                                    const auto r_p_c_p_i_p = std::isfinite(shtl[26]) ? shtl[26] : shtl[13];

                                    return (  1.0 * r_p_c_p_i_p
                                            + 1.0 * r_m_c_p_i_p
                                            + 1.0 * r_p_c_m_i_p
                                            + 1.0 * r_m_c_m_i_p

                                            + 2.0 * r_0_c_m_i_p
                                            + 2.0 * r_0_c_p_i_p
                                            + 2.0 * r_m_c_0_i_p
                                            + 2.0 * r_p_c_0_i_p

                                            + 4.0 * r_0_c_0_i_p

                                            - 1.0 * r_p_c_p_i_m
                                            - 1.0 * r_m_c_p_i_m
                                            - 1.0 * r_p_c_m_i_m
                                            - 1.0 * r_m_c_m_i_m

                                            - 2.0 * r_0_c_m_i_m
                                            - 2.0 * r_0_c_p_i_m
                                            - 2.0 * r_m_c_0_i_m
                                            - 2.0 * r_p_c_0_i_m

                                            - 4.0 * r_0_c_0_i_m)
                                           / 32.0;
            };

            if(false){
            }else if( std::regex_match(MethodStr, regex_row) ){
                ud.f_reduce = row_aligned;

            }else if( std::regex_match(MethodStr, regex_col) ){
                ud.f_reduce = col_aligned;

            }else if( std::regex_match(MethodStr, regex_img) ){
                ud.f_reduce = img_aligned;

            //}else if( std::regex_match(MethodStr, regex_prpc) ){
            //    throw std::invalid_argument("Method '"_s + MethodStr + "' and estimator '"_s + EstimatorStr + "' currently cannot be combined.");
            //}else if( std::regex_match(MethodStr, regex_nrpc) ){
            //    throw std::invalid_argument("Method '"_s + MethodStr + "' and estimator '"_s + EstimatorStr + "' currently cannot be combined.");
            }else if( std::regex_match(MethodStr, regex_mag) ){
                ud.f_reduce = [&](std::vector<double> &shtl) -> double {
                                    return std::hypot( row_aligned(shtl),
                                                       col_aligned(shtl),
                                                       img_aligned(shtl) );
                };

            //}else if( std::regex_match(MethodStr, regex_orn) ){
            //    throw std::invalid_argument("Method '"_s + MethodStr + "' and estimator '"_s + EstimatorStr + "' currently cannot be combined.");
            //}else if( std::regex_match(MethodStr, regex_nms) ){
            //    throw std::invalid_argument("Method '"_s + MethodStr + "' and estimator '"_s + EstimatorStr + "' currently cannot be combined.");
            //}else if( std::regex_match(MethodStr, regex_crs) ){
            //    throw std::invalid_argument("Method '"_s + MethodStr + "' and estimator '"_s + EstimatorStr + "' currently cannot be combined.");
            }else{
                throw std::invalid_argument("Method argument '"_s + MethodStr + "' is not valid");
            }

        }else{
            throw std::invalid_argument("Estimator argument '"_s + EstimatorStr + "' is not valid");
        }

        if(!(*iap_it)->imagecoll.Compute_Images( ComputeVolumetricNeighbourhoodSampler, 
                                                 {}, cc_ROIs, &ud )){
            throw std::runtime_error("Unable to compute volumetric partial derivative.");
        }

    }

    return DICOM_data;
}
