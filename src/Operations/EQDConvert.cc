//EQDConvert.cc - A part of DICOMautomaton 2017, 2019. Written by hal clark.

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
#include "../YgorImages_Functors/Processing/EQDConversion.h"
#include "EQDConvert.h"
#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.



OperationDoc OpArgDocEQDConvert(){
    OperationDoc out;
    out.name = "EQDConvert";

    out.desc = 
        "This operation performs a BED-based conversion to a dose-equivalent that would have 'd' dose per fraction"
        " (e.g., for 'EQD2' the dose per fraction would be 2 Gy).";
        
    out.notes.emplace_back(
        "This operation treats all tissue as either tumourous or not, and allows specification of a single"
        " alpha/beta for each type (i.e., one for tumourous tissues, one for normal tissues)."
        " Owing to this limitation, use of this operation is generally limited to single-OAR or PTV-only"
        " EQD conversions."
    );
    out.notes.emplace_back(
        "This operation requires NumberOfFractions and cannot use DosePerFraction."
        " The reasoning is that the DosePerFraction would need to be specified for each individual voxel;"
        " the prescription DosePerFraction is NOT the same as voxels outside the PTV."
    );

    out.args.emplace_back();
    out.args.back() = IAWhitelistOpArgDoc();
    out.args.back().name = "ImageSelection";
    out.args.back().default_val = "last";
    out.args.back().visibility = OpArgVisibility::Hide;


    out.args.emplace_back();
    out.args.back().name = "AlphaBetaRatioNormal";
    out.args.back().desc = "The value to use for alpha/beta in normal (non-cancerous) tissues."
                           " Generally a value of 3.0 Gy is used. Tissues that are sensitive to fractionation"
                           " may warrant smaller ratios, such as 1.5-3 Gy for cervical central nervous tissues"
                           " and 2.3-4.9 for lumbar central nervous tissues (consult table 8.1, page 107 in: "
                           " Joiner et al., 'Fractionation: the linear-quadratic approach', 4th Ed., 2009,"
                           " in the book 'Basic Clinical Radiobiology', ISBN: 0340929669)."
                           " Note that the selected ROIs denote which tissues are diseased. The remaining tissues are "
                           " considered to be normal.";
    out.args.back().default_val = "3.0";
    out.args.back().expected = true;
    out.args.back().examples = { "2.0", "3.0" };


    out.args.emplace_back();
    out.args.back().name = "AlphaBetaRatioTumour";
    out.args.back().desc = "The value to use for alpha/beta in diseased (tumourous) tissues."
                           " Generally a value of 10.0 is used. Note that the selected ROIs"
                           " denote which tissues are diseased. The remaining tissues are "
                           " considered to be normal.";
    out.args.back().default_val = "10.0";
    out.args.back().expected = true;
    out.args.back().examples = { "10.0" };


    out.args.emplace_back();
    out.args.back().name = "NumberOfFractions";
    out.args.back().desc = "The number of fractions in which a plan was (or will be) delivered."
                           " Decimal fractions are supported to accommodate previous BED conversions.";
    out.args.back().default_val = "35";
    out.args.back().expected = true;
    out.args.back().examples = { "10", "20.5", "35", "40.123" };


    out.args.emplace_back();
    out.args.back().name = "TargetDosePerFraction";
    out.args.back().desc = "The desired dose per fraction. For 'EQD2' this value must be 2 Gy."
                           " Note that the specific interpretation of this parameter depends on the model.";
    out.args.back().default_val = "2.0";
    out.args.back().expected = true;
    out.args.back().examples = { "1.8", "2.0", "5.0", "8.0" };


    out.args.emplace_back();
    out.args.back().name = "PrescriptionDose";
    out.args.back().desc = "The prescription dose that was (or will be) delivered to the PTV."
                           " This parameter is only used for the 'pinned-lq-simple' model."
                           " Note that this is a theoretical dose since the PTV or CTV will only nominally"
                           " receive this dose. Also note that the specified dose need not exist somewhere"
                           " in the image. It can be purely theoretical to accommodate previous BED"
                           " conversions.";
    out.args.back().default_val = "70";
    out.args.back().expected = true;
    out.args.back().examples = { "15", "22.5", "45.0", "66", "70.001" };


    out.args.emplace_back();
    out.args.back().name = "Model";
    out.args.back().desc = "The EQD model to use."
                           " Current options are 'lq-simple' and 'lq-simple-pinned'."
                           " The 'lq-simple' model uses a simplistic linear-quadratic model."
                           " This model disregards time delays, including repopulation."
                           " The 'lq-simple-pinned' model is an **experimental** alternative to the 'lq-simple' model."
                           " The 'lq-simple-pinned' model implements the 'lq-simple' model, but avoids having to"
                           " specify d dose per fraction. First the prescription dose is transformed to EQD with d"
                           " dose per fraction and the effective number of fractions is extracted."
                           " Then, each voxel is transformed assuming this effective number of fractions"
                           " rather than a specific dose per fraction."
                           " This model conveniently avoids having to awkwardly specify d dose per fraction"
                           " for voxels that receive less than d dose. It is also idempotent."
                           " Note, however, that the 'lq-simple-pinned' model produces EQD estimates that are"
                           " **incompatbile** with 'lq-simple' EQD estimates.";
    out.args.back().default_val = "lq-simple";
    out.args.back().expected = true;
    out.args.back().examples = { "lq-simple", "lq-simple-pinned" };


    out.args.emplace_back();
    out.args.back().name = "NormalizedROILabelRegex";
    out.args.back().desc = "A regex matching ROI labels/names to consider as bounding tumourous tissues."
                           " The default will match"
                           " all available ROIs. Be aware that input spaces are trimmed to a single space."
                           " If your ROI name has more than two sequential spaces, use regex to avoid them."
                           " All ROIs have to match the single regex, so use the 'or' token if needed."
                           " Regex is case insensitive and uses extended POSIX syntax.";
    out.args.back().default_val = ".*";
    out.args.back().expected = true;
    out.args.back().examples = { ".*", ".*GTV.*", "PTV66", R"***(.*PTV.*|.*GTV.**)***" };


    out.args.emplace_back();
    out.args.back().name = "ROILabelRegex";
    out.args.back().desc = "A regex matching ROI labels/names to consider as bounding tumourous tissues."
                           " The default will match"
                           " all available ROIs. Be aware that input spaces are trimmed to a single space."
                           " If your ROI name has more than two sequential spaces, use regex to avoid them."
                           " All ROIs have to match the single regex, so use the 'or' token if needed."
                           " Regex is case insensitive and uses extended POSIX syntax.";
    out.args.back().default_val = ".*";
    out.args.back().expected = true;
    out.args.back().examples = { ".*", ".*GTV.*", "PTV66", R"***(.*PTV.*|.*GTV.**)***" };

    return out;
}



Drover EQDConvert(Drover DICOM_data,
                  const OperationArgPkg& OptArgs,
                  const std::map<std::string, std::string>& /*InvocationMetadata*/,
                  const std::string& /*FilenameLex*/){

    EQDConversionUserData ud;

    //---------------------------------------------- User Parameters --------------------------------------------------
    ud.AlphaBetaRatioNormal = std::stod(OptArgs.getValueStr("AlphaBetaRatioNormal").value());
    ud.AlphaBetaRatioTumour = std::stod(OptArgs.getValueStr("AlphaBetaRatioTumour").value());

    ud.NumberOfFractions = std::stod(OptArgs.getValueStr("NumberOfFractions").value());
    ud.PrescriptionDose = std::stod(OptArgs.getValueStr("PrescriptionDose").value());
    ud.TargetDosePerFraction = std::stod(OptArgs.getValueStr("TargetDosePerFraction").value());

    const auto ModelStr = OptArgs.getValueStr("Model").value();

    const auto NormalizedROILabelRegex = OptArgs.getValueStr("NormalizedROILabelRegex").value();
    const auto ROILabelRegex = OptArgs.getValueStr("ROILabelRegex").value();

    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();

    //-----------------------------------------------------------------------------------------------------------------
    const auto regex_model_lqs  = Compile_Regex("^linear-quadratic-simple$");
    const auto regex_model_lqsp = Compile_Regex("^linear-quadratic-simple$");

    //-----------------------------------------------------------------------------------------------------------------

    if( ud.PrescriptionDose <= 0.0 ){
        throw std::invalid_argument("PrescriptionDose must be specified (>0.0)");
    }
    if( ud.NumberOfFractions <= 0.0 ){
        throw std::invalid_argument("NumberOfFractions must be specified (>0.0)");
    }

    if( std::regex_match(ModelStr, regex_model_lqs) ){
        ud.model = EQDConversionUserData::Model::SimpleLinearQuadratic;
    }else if( std::regex_match(ModelStr, regex_model_lqsp) ){
        ud.model = EQDConversionUserData::Model::PinnedLinearQuadratic;
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
                                                          EQDConversion,
                                                          {}, cc_ROIs, &ud )){
            throw std::runtime_error("Unable to convert image_array voxels to EQD using the specified ROI(s).");
        }
    }

    return DICOM_data;
}
