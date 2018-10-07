//SimplifyContours.cc - A part of DICOMautomaton 2018. Written by hal clark.

#include <limits>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>            //Needed for exit() calls.
#include <experimental/optional>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>    
#include <vector>
#include <utility>

#include "../Insert_Contours.h"
#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../Thread_Pool.h"

#include "SimplifyContours.h"
#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMathPlottingGnuplot.h" //Needed for YgorMathPlottingGnuplot::*.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorStats.h"        //Needed for Stats:: namespace.


OperationDoc OpArgDocSimplifyContours(void){
    OperationDoc out;
    out.name = "SimplifyContours";

    out.desc = 
      "This operation performs simplification on contours by removing or moving vertices."
      " This operation is mostly used to reduce the computational complexity of other operations.";

    out.notes.emplace_back(
        "Contours are currently processed individually, not as a volume."
    );


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
    out.args.back().name = "FractionalAreaTolerance";
    out.args.back().desc = "The fraction of area each contour will tolerate during simplified."
                           " This is a measure of how much the contour area can change due to simplification.";
    out.args.back().default_val = "0.01";
    out.args.back().expected = true;
    out.args.back().examples = { "0.001", "0.01", "0.02", "0.05", "0.10" };


    return out;
}

Drover SimplifyContours(Drover DICOM_data, OperationArgPkg OptArgs, std::map<std::string,std::string> /*InvocationMetadata*/, std::string FilenameLex){

    Explicator X(FilenameLex);

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto NormalizedROILabelRegex = OptArgs.getValueStr("NormalizedROILabelRegex").value();
    const auto ROILabelRegex = OptArgs.getValueStr("ROILabelRegex").value();
    const auto FractionalAreaTolerance = std::stod( OptArgs.getValueStr("FractionalAreaTolerance").value() );

    //-----------------------------------------------------------------------------------------------------------------

    //Stuff references to all contours into a list. Remember that you can still address specific contours through
    // the original holding containers (which are not modified here).
    auto cc_all = All_CCs( DICOM_data );
    auto cc_ROIs = Whitelist( cc_all, { { "ROIName", ROILabelRegex },
                                        { "NormalizedROIName", NormalizedROILabelRegex } } );


    const double tolerance_sq_dist = 1E-4;
    const auto verts_equal = [=](const vec3<double> &A, const vec3<double> &B) -> bool {
        return A.sq_dist(B) < tolerance_sq_dist;
    };


    const bool AssumePlanar = true;
    for(auto &cc_refw : cc_ROIs){
        for(auto &c : cc_refw.get().contours){
            const auto A_orig = std::abs( c.Get_Signed_Area(AssumePlanar) );

            // Vertex removal. No vertices are added.
            c = c.Remove_Vertices( FractionalAreaTolerance * A_orig );

            // Vertex collapse. Adjacent vertices are merged together. TODO
            // ...
        }
    }

    return DICOM_data;
}
