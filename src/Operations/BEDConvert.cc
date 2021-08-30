//BEDConvert.cc - A part of DICOMautomaton 2017, 2019, 2020. Written by hal clark.

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
#include "../YgorImages_Functors/Processing/BEDConversion.h"
#include "BEDConvert.h"
#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.



OperationDoc OpArgDocBEDConvert(){
    OperationDoc out;
    out.name = "BEDConvert";

    out.desc = 
        "This operation performs Biologically Effective Dose (BED) and Equivalent Dose with 'x'-dose per fraction"
        " (EQDx) conversions. Currently, only photon external beam therapy conversions are supported.";
        
    out.notes.emplace_back(
        "For an 'EQD2' transformation, select an EQDx conversion model with 2 Gy per fraction (i.e., $x=2$)."
    );
    out.notes.emplace_back(
        "This operation treats all tissue as either early-responding (e.g., tumour) or late-responding"
        " (e.g., some normal tissues)."
        " A single alpha/beta estimate for each type (early or late) can be provided."
        " Currently, only two tissue types can be specified."
    );
    out.notes.emplace_back(
        "This operation requires specification of the initial number of fractions and cannot use dose per fraction."
        " The rationale is that for some models, the dose per fraction would need to be specified for *each"
        " individual voxel* since the prescription dose per fraction is **not** the same for voxels outside the PTV."
    );
    out.notes.emplace_back(
        "Be careful in handling the output of a BED calculation. In particular, BED doses with a given"
        " $\\alpha/\\beta$ should **only** be summed with BED doses that have the same $\\alpha/\\beta$."
    );

    out.args.emplace_back();
    out.args.back() = IAWhitelistOpArgDoc();
    out.args.back().name = "ImageSelection";
    out.args.back().default_val = "last";
    out.args.back().visibility = OpArgVisibility::Hide;


    out.args.emplace_back();
    out.args.back().name = "AlphaBetaRatioLate";
    out.args.back().desc = "The value to use for alpha/beta in late-responding (i.e., 'normal', non-cancerous) tissues."
                           " Generally a value of 3.0 Gy is used. Tissues that are sensitive to fractionation"
                           " may warrant smaller ratios, such as 1.5-3 Gy for cervical central nervous tissues"
                           " and 2.3-4.9 for lumbar central nervous tissues (consult table 8.1, page 107 in: "
                           " Joiner et al., 'Fractionation: the linear-quadratic approach', 4th Ed., 2009,"
                           " in the book 'Basic Clinical Radiobiology', ISBN: 0340929669)."
                           " Note that the selected ROIs denote early-responding tissues;"
                           " all remaining tissues are considered late-responding.";
    out.args.back().default_val = "3.0";
    out.args.back().expected = true;
    out.args.back().examples = { "2.0", "3.0" };


    out.args.emplace_back();
    out.args.back().name = "AlphaBetaRatioEarly";
    out.args.back().desc = "The value to use for alpha/beta in early-responding tissues (i.e., tumourous and some normal tissues)."
                           " Generally a value of 10.0 Gy is used."
                           " Note that the selected ROIs denote early-responding tissues;"
                           " all remaining tissues are considered late-responding.";
    out.args.back().default_val = "10.0";
    out.args.back().expected = true;
    out.args.back().examples = { "10.0" };


    out.args.emplace_back();
    out.args.back().name = "PriorNumberOfFractions";
    out.args.back().desc = "The number of fractions over which the dose distribution was (or will be) delivered."
                           " This parameter is required for both BED and EQDx conversions."
                           " Decimal fractions are supported to accommodate multi-pass BED conversions.";
    out.args.back().default_val = "35";
    out.args.back().expected = true;
    out.args.back().examples = { "10", "20.5", "35", "40.123" };


    out.args.emplace_back();
    out.args.back().name = "PriorPrescriptionDose";
    out.args.back().desc = "The prescription dose that was (or will be) delivered to the PTV."
                           " This parameter is only used for the 'eqdx-lq-simple-pinned' model."
                           " Note that this is a theoretical dose since the PTV or CTV will only nominally"
                           " receive this dose. Also note that the specified dose need not exist somewhere"
                           " in the image. It can be purely theoretical to accommodate previous BED"
                           " conversions.";
    out.args.back().default_val = "70";
    out.args.back().expected = true;
    out.args.back().examples = { "15", "22.5", "45.0", "66", "70.001" };


    out.args.emplace_back();
    out.args.back().name = "TargetDosePerFraction";
    out.args.back().desc = "The desired dose per fraction 'x' for an EQDx conversion."
                           " For an 'EQD2' conversion, this value *must* be 2 Gy."
                           " For an 'EQD3.5' conversion, this value should be 3.5 Gy."
                           " Note that the specific interpretation of this parameter depends on the model.";
    out.args.back().default_val = "2.0";
    out.args.back().expected = true;
    out.args.back().examples = { "1.8", "2.0", "5.0", "8.0" };


    out.args.emplace_back();
    out.args.back().name = "Model";
    out.args.back().desc = "The BED or EQDx model to use. All assume e was delivered using photon external beam therapy."
                           " Current options are 'bed-lq-simple', 'eqdx-lq-simple', and 'eqdx-lq-simple-pinned'."
                           " The 'bed-lq-simple' model uses a standard linear-quadratic model that disregards"
                           " time delays, including repopulation ($BED = (1 + \\alpha/\\beta)nd$)."
                           " The 'eqdx-lq-simple' model uses the widely-known, standard formula"
                           " $EQD_{x} = nd(d + \\alpha/\\beta)/(x + \\alpha/\\beta)$"
                           " which is dervied from the"
                           " linear-quadratic radiobiological model and is also known as the 'Withers' formula."
                           " This model disregards time delays, including repopulation."
                           " The 'eqdx-lq-simple-pinned' model is an **experimental** alternative to the 'eqdx-lq-simple' model."
                           " The 'eqdx-lq-simple-pinned' model implements the 'eqdx-lq-simple' model, but avoids having to"
                           " specify *x* dose per fraction. First the prescription dose is transformed to EQDx with *x*"
                           " dose per fraction and the effective number of fractions is extracted."
                           " Then, each voxel is transformed assuming this effective number of fractions"
                           " rather than a specific dose per fraction."
                           " This model conveniently avoids having to awkwardly specify *x* dose per fraction"
                           " for voxels that receive less than *x* dose. It is also idempotent."
                           " Note, however, that the 'eqdx-lq-simple-pinned' model produces EQDx estimates that are"
                           " **incompatbile** with 'eqdx-lq-simple' EQDx estimates.";
    out.args.back().default_val = "eqdx-lq-simple";
    out.args.back().expected = true;
    out.args.back().examples = { "bed-lq-simple", "eqdx-lq-simple", "eqdx-lq-simple-pinned" };
    out.args.back().samples = OpArgSamples::Exhaustive;


    out.args.emplace_back();
    out.args.back() = RCWhitelistOpArgDoc();
    out.args.back().name = "EarlyROILabelRegex";
    out.args.back().desc = "This parameter selects ROI labels/names to consider as bounding early-responding tissues. "_s
                         + out.args.back().desc;
    out.args.back().examples = { ".*", ".*GTV.*", "PTV66", R"***(.*PTV.*|.*GTV.**)***" };
    out.args.back().default_val = ".*";


    out.args.emplace_back();
    out.args.back() = NCWhitelistOpArgDoc();
    out.args.back().name = "EarlyNormalizedROILabelRegex";
    out.args.back().desc = "This parameter selects ROI labels/names to consider as bounding early-responding tissues. "_s
                         + out.args.back().desc;
    out.args.back().examples = { ".*", ".*GTV.*", "PTV66", R"***(.*PTV.*|.*GTV.**)***" };
    out.args.back().default_val = ".*";

    return out;
}



bool BEDConvert(Drover &DICOM_data,
                  const OperationArgPkg& OptArgs,
                  const std::map<std::string, std::string>& /*InvocationMetadata*/,
                  const std::string& /*FilenameLex*/){

    BEDConversionUserData ud;

    //---------------------------------------------- User Parameters --------------------------------------------------

    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();

    ud.AlphaBetaRatioLate = std::stod(OptArgs.getValueStr("AlphaBetaRatioLate").value());
    ud.AlphaBetaRatioEarly = std::stod(OptArgs.getValueStr("AlphaBetaRatioEarly").value());

    ud.NumberOfFractions = std::stod(OptArgs.getValueStr("PriorNumberOfFractions").value());
    ud.PrescriptionDose = std::stod(OptArgs.getValueStr("PriorPrescriptionDose").value());
    ud.TargetDosePerFraction = std::stod(OptArgs.getValueStr("TargetDosePerFraction").value());

    const auto ModelStr = OptArgs.getValueStr("Model").value();

    const auto NormalizedROILabelRegex = OptArgs.getValueStr("EarlyNormalizedROILabelRegex").value();
    const auto ROILabelRegex = OptArgs.getValueStr("EarlyROILabelRegex").value();

    //-----------------------------------------------------------------------------------------------------------------
    const auto regex_model_bed_lqs   = Compile_Regex("^be?d?-?li?n?e?a?r?-?qu?a?d?r?a?t?i?c?-?s?i?m?p?l?e?$");
    const auto regex_model_eqdx_lqs  = Compile_Regex("^eq?d?x?-?li?n?e?a?r?-?qu?a?d?r?a?t?i?c?-?s?i?m?p?l?e?$");
    const auto regex_model_eqdx_lqsp = Compile_Regex("^eq?d?x?-?li?n?e?a?r?-?qu?a?d?r?a?t?i?c?-?s?i?m?p?l?e?-?pi?n?n?e?d?$");

    //-----------------------------------------------------------------------------------------------------------------

    if( ud.PrescriptionDose <= 0.0 ){
        throw std::invalid_argument("PrescriptionDose must be specified (>0.0)");
    }
    if( ud.NumberOfFractions <= 0.0 ){
        throw std::invalid_argument("NumberOfFractions must be specified (>0.0)");
    }

    if( std::regex_match(ModelStr, regex_model_bed_lqs) ){
        ud.model = BEDConversionUserData::Model::BEDSimpleLinearQuadratic;
    }else if( std::regex_match(ModelStr, regex_model_eqdx_lqsp) ){
        ud.model = BEDConversionUserData::Model::EQDXPinnedLinearQuadratic;
    }else if( std::regex_match(ModelStr, regex_model_eqdx_lqs) ){
        ud.model = BEDConversionUserData::Model::EQDXSimpleLinearQuadratic;
    }else{
        throw std::invalid_argument("Model not understood. Cannot continue.");
    }

    //Stuff references to all contours into a list. Remember that you can still address specific contours through
    // the original holding containers (which are not modified here).
    auto cc_all = All_CCs( DICOM_data );
    auto cc_ROIs = Whitelist( cc_all, { { "ROIName", ROILabelRegex },
                                        { "NormalizedROIName", NormalizedROILabelRegex } } );
    if(cc_ROIs.empty()){
        throw std::invalid_argument("No contours selected. Cannot continue.");
    }

    auto IAs_all = All_IAs( DICOM_data );
    auto IAs = Whitelist( IAs_all, ImageSelectionStr );
    IAs = Whitelist(IAs, "Modality", "RTDOSE");
    for(auto & iap_it : IAs){
        if(!(*iap_it)->imagecoll.Process_Images_Parallel( GroupIndividualImages,
                                                          BEDConversion,
                                                          {}, cc_ROIs, &ud )){
            throw std::runtime_error("Unable to convert image_array voxels to BED or EQDx using the specified ROI(s).");
        }
    }

    return true;
}
