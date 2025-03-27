//SimplifyContours.cc - A part of DICOMautomaton 2018. Written by hal clark.

#include <cstdlib>            //Needed for exit() calls.
#include <optional>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>    
#include <vector>

#include "../Insert_Contours.h"
#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../Thread_Pool.h"

#include "YgorMath.h"         //Needed for vec3 class.

#include "Explicator.h"

#include "SimplifyContours.h"

OperationDoc OpArgDocSimplifyContours(){
    OperationDoc out;
    out.name = "SimplifyContours";
    out.tags.emplace_back("category: contour processing");

    out.desc = 
      "This operation performs simplification on contours by removing or moving vertices."
      " This operation is mostly used to reduce the computational complexity of other operations.";

    out.notes.emplace_back(
        "Contours are currently processed individually, not as a volume."
    );
    out.notes.emplace_back(
        "Simplification is generally performed most eagerly on regions with relatively low curvature."
        " Regions of high curvature are generally simplified only as necessary."
    );


    out.args.emplace_back();
    out.args.back() = NCWhitelistOpArgDoc();
    out.args.back().name = "NormalizedROILabelRegex";
    out.args.back().default_val = ".*";


    out.args.emplace_back();
    out.args.back() = RCWhitelistOpArgDoc();
    out.args.back().name = "ROILabelRegex";
    out.args.back().default_val = ".*";

    out.args.emplace_back();
    out.args.back() = CCWhitelistOpArgDoc();
    out.args.back().name = "ROISelection";
    out.args.back().default_val = "all";


    out.args.emplace_back();
    out.args.back().name = "FractionalAreaTolerance";
    out.args.back().desc = "The fraction of area each contour will tolerate during simplified."
                           " This is a measure of how much the contour area can change due to simplification.";
    out.args.back().default_val = "0.01";
    out.args.back().expected = true;
    out.args.back().examples = { "0.001", "0.01", "0.02", "0.05", "0.10" };


    out.args.emplace_back();
    out.args.back().name = "SimplificationMethod";
    out.args.back().desc = "The specific algorithm used to perform contour simplification."
                           " 'Vertex removal' is a simple algorithm that removes vertices one-by-one without"
                           " replacement."
                           " It iteratively ranks vertices and removes the single vertex that"
                           " has the least impact on contour area."
                           " It is best suited to removing redundant vertices or whenever new vertices"
                           " should not be added."
                           " 'Vertex collapse' combines two adjacent vertices into a single vertex"
                           " at their midpoint."
                           " It iteratively ranks vertex pairs and removes the single vertex that"
                           " has the least total impact on contour area."
                           " Note that small sharp features that alternate inward and outward will"
                           " have a small total area cost, so will be pruned early."
                           " Thus this technique acts as a low-pass filter and will defer simplification"
                           " of high-curvature regions until necessary."
                           " It is more economical compared to vertex removal in that it will usually"
                           " simplify contours more for a given tolerance (or, equivalently, can retain"
                           " contour fidelity better than vertex removal for the same number of vertices)."
                           " However, vertex collapse performs an averaging that may"
                           " result in numerical imprecision.";
    out.args.back().default_val = "vert-collapse";
    out.args.back().expected = true;
    out.args.back().examples = { "vertex-collapse", "vertex-removal" };
    out.args.back().samples = OpArgSamples::Exhaustive;


    return out;
}

bool SimplifyContours(Drover &DICOM_data,
                        const OperationArgPkg& OptArgs,
                        std::map<std::string, std::string>& /*InvocationMetadata*/,
                        const std::string& FilenameLex){

    Explicator X(FilenameLex);

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto NormalizedROILabelRegex = OptArgs.getValueStr("NormalizedROILabelRegex").value();
    const auto ROILabelRegex = OptArgs.getValueStr("ROILabelRegex").value();
    const auto ROISelection = OptArgs.getValueStr("ROISelection").value();
    const auto FractionalAreaTolerance = std::stod( OptArgs.getValueStr("FractionalAreaTolerance").value() );
    const auto SimplificationMethod = OptArgs.getValueStr("SimplificationMethod").value();

    //-----------------------------------------------------------------------------------------------------------------
    const auto regex_vert_col = Compile_Regex("ve?r?t?e?x?-?co?l?l?a?p?s?e?");
    const auto regex_vert_rem = Compile_Regex("ve?r?t?e?x?-?re?m?o?v?a?l?");

    if( !std::regex_match(SimplificationMethod, regex_vert_col)
    &&  !std::regex_match(SimplificationMethod, regex_vert_rem) ){
        throw std::invalid_argument("SimplificationMethod selection is not valid. Cannot continue.");
    }


    //Stuff references to all contours into a list. Remember that you can still address specific contours through
    // the original holding containers (which are not modified here).
    auto cc_all = All_CCs( DICOM_data );
    auto cc_ROIs = Whitelist( cc_all, ROILabelRegex, NormalizedROILabelRegex, ROISelection );

    const bool AssumePlanar = true;
    for(auto &cc_refw : cc_ROIs){
        for(auto &c : cc_refw.get().contours){
            const auto A_orig = std::abs( c.Get_Signed_Area(AssumePlanar) );
            const auto A_tol = FractionalAreaTolerance * A_orig;

            if( std::regex_match(SimplificationMethod, regex_vert_col) ){
                // Vertex collapse. Adjacent vertices are merged together.
                c = c.Collapse_Vertices(A_tol);

            }else if( std::regex_match(SimplificationMethod, regex_vert_rem) ){
                // Vertex removal. No vertices are added.
                c = c.Remove_Vertices(A_tol);

            }else{
                throw std::logic_error("SimplificationMethod options have been updated incompletely. Cannot continue.");
            }

        }
    }

    return true;
}
