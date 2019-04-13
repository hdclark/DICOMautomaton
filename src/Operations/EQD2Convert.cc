//EQD2Convert.cc - A part of DICOMautomaton 2017. Written by hal clark.

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
#include "../YgorImages_Functors/Processing/EQD2Conversion.h"
#include "EQD2Convert.h"
#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.



OperationDoc OpArgDocEQD2Convert(void){
    OperationDoc out;
    out.name = "EQD2Convert";

    out.desc = 
        "This operation performs a BED-based conversion to a dose-equivalent that would have 2Gy fractions.";
        
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
    out.args.back().name = "PrescriptionDose";
    out.args.back().desc = "The prescription dose that was (or will be) delivered to the PTV."
                      " Note that this is a theoretical dose since the PTV or CTV will only nominally"
                      " receive this dose. Also note that the specified dose need not exist somewhere"
                      " in the image. It can be purely theoretical to accommodate previous BED"
                      " conversions.";
    out.args.back().default_val = "70";
    out.args.back().expected = true;
    out.args.back().examples = { "15", "22.5", "45.0", "66", "70.001" };


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



Drover EQD2Convert(Drover DICOM_data, 
                           OperationArgPkg OptArgs, 
                           std::map<std::string,std::string> /*InvocationMetadata*/, 
                           std::string /*FilenameLex*/ ){

    EQD2ConversionUserData ud;

    //---------------------------------------------- User Parameters --------------------------------------------------
    ud.AlphaBetaRatioNormal = std::stod(OptArgs.getValueStr("AlphaBetaRatioNormal").value());
    ud.AlphaBetaRatioTumour = std::stod(OptArgs.getValueStr("AlphaBetaRatioTumour").value());

    ud.NumberOfFractions = std::stod(OptArgs.getValueStr("NumberOfFractions").value());
    ud.PrescriptionDose = std::stod(OptArgs.getValueStr("PrescriptionDose").value());

    const auto NormalizedROILabelRegex = OptArgs.getValueStr("NormalizedROILabelRegex").value();
    const auto ROILabelRegex = OptArgs.getValueStr("ROILabelRegex").value();

    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();

    //-----------------------------------------------------------------------------------------------------------------

    const auto regex_none = std::regex("no?n?e?$", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto regex_last = std::regex("la?s?t?$", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto regex_all  = std::regex("al?l?$",   std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);

    if( ud.PrescriptionDose <= 0.0 ){
        throw std::invalid_argument("PrescriptionDose must be specified (>0.0)");
    }
    if( ud.NumberOfFractions <= 0.0 ){
        throw std::invalid_argument("NumberOfFractions must be specified (>0.0)");
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
                                                          EQD2Conversion,
                                                          {}, cc_ROIs, &ud )){
            throw std::runtime_error("Unable to convert image_array voxels to EQD2 using the specified ROI(s).");
        }
    }

    return DICOM_data;
}
