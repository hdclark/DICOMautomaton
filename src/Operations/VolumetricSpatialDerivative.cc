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

#include "YgorImages.h"
#include "YgorString.h"       //Needed for GetFirstRegex(...)

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../YgorImages_Functors/Grouping/Misc_Functors.h"
#include "../YgorImages_Functors/Compute/Volumetric_Spatial_Derivative.h"

#include "VolumetricSpatialDerivative.h"


OperationDoc OpArgDocVolumetricSpatialDerivative(void){
    OperationDoc out;
    out.name = "VolumetricSpatialDerivative";

    out.desc = 
        "This operation estimates various spatial partial derivatives (of pixel values) within 3D rectilinear image arrays.";

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
    out.args.back().default_val = "-1";
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
                                 "Sobel-3x3x3" };


    out.args.emplace_back();
    out.args.back().name = "Method";
    out.args.back().desc = "Controls partial derivative method. First-order derivatives can be row-, column-, or image-aligned,"
                           " All methods also support magnitude (addition of orthogonal components in quadrature).";
    out.args.back().default_val = "magnitude";
    out.args.back().expected = true;
    out.args.back().examples = { "row-aligned",
                                 "column-aligned",
                                 "image-aligned",
                                 "magnitude",
                                 "non-maximum-suppression" };

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
    const auto regex_sob3x3x3 = Compile_Regex("^so?b?e?l?-?3x?3?x?3?$");

    const auto regex_row  = Compile_Regex("^ro?w?-?a?l?i?g?n?e?d?$");
    const auto regex_col  = Compile_Regex("^col?u?m?n?-?a?l?i?g?n?e?d?$");
    const auto regex_img  = Compile_Regex("^im?a?g?e?-?a?l?i?g?n?e?d?$");
    const auto regex_mag  = Compile_Regex("^ma?g?n?i?t?u?d?e?$");
    const auto regex_nms  = Compile_Regex("^no?n?-?m?a?x?i?m?u?m?-?s?u?p?p?r?e?s?s?i?o?n?$");


    auto cc_all = All_CCs( DICOM_data );
    auto cc_ROIs = Whitelist( cc_all, { { "ROIName", ROILabelRegex },
                                        { "NormalizedROIName", NormalizedROILabelRegex } } );
    if(cc_ROIs.empty()){
        throw std::invalid_argument("No contours selected. Cannot continue.");
    }


    auto IAs_all = All_IAs( DICOM_data );
    auto IAs = Whitelist( IAs_all, ImageSelectionStr );
    for(auto & iap_it : IAs){

/*
        ////////////////////////
        // If non-maximum suppression is enabled, the magnitude will need to be pre-computed on a copy of the images.
        // In this case we make a local copy to feed to the magnitude routine.
        Drover DD_magn;
        if( std::regex_match(MethodStr, regex_nms) ){
            DD_magn.image_data.emplace_back( std::make_shared<Image_Array>( *(*iap_it) ) );

            OperationArgPkg Op("VolumetricSpatialDerivative:ImageSelectionStr=last:";

            auto OpDocs = OpArgDocVolumetricSpatialDerivative();
            for(const auto &r : OpDocs.args){
                if(r.expected) optargs.insert( r.name, r.default_val );
            }
            DD_magn = VolumetricSpatialDerivative(DD_magn, OptArgs, {}, "" );
        }
        ////////////////////////
*/

        // Planar derivatives.
        ComputeVolumetricSpatialDerivativeUserData ud;
        ud.channel = Channel;
        ud.order = VolumetricSpatialDerivativeEstimator::first;
        ud.method = VolumetricSpatialDerivativeMethod::row_aligned;

        if(false){
        }else if( std::regex_match(EstimatorStr, regex_1st) ){
            ud.order = VolumetricSpatialDerivativeEstimator::first;
        }else if( std::regex_match(EstimatorStr, regex_sob3x3x3) ){
            ud.order = VolumetricSpatialDerivativeEstimator::Sobel_3x3x3;
        }else{
            throw std::invalid_argument("Estimator argument '"_s + EstimatorStr + "' is not valid");
        }
        if(false){
        }else if( std::regex_match(MethodStr, regex_row) ){
            ud.method = VolumetricSpatialDerivativeMethod::row_aligned;
        }else if( std::regex_match(MethodStr, regex_col) ){
            ud.method = VolumetricSpatialDerivativeMethod::column_aligned;
        }else if( std::regex_match(MethodStr, regex_img) ){
            ud.method = VolumetricSpatialDerivativeMethod::image_aligned;
        }else if( std::regex_match(MethodStr, regex_mag) ){
            ud.method = VolumetricSpatialDerivativeMethod::magnitude;
        }else if( std::regex_match(MethodStr, regex_nms) ){
            ud.method = VolumetricSpatialDerivativeMethod::non_maximum_suppression;
FUNCERR("This routine is not yet implemented");    // Do I need to take any extra steps here ???

        }else{
            throw std::invalid_argument("Method argument '"_s + MethodStr + "' is not valid");
        }

        if(!(*iap_it)->imagecoll.Compute_Images( ComputeVolumetricSpatialDerivative,
                                                 {}, cc_ROIs, &ud )){
            throw std::runtime_error("Unable to compute volumetric partial derivative.");
        }
    }

    return DICOM_data;
}

