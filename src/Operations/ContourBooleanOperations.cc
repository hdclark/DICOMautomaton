//ContourBooleanOperations.cc - A part of DICOMautomaton 2017. Written by hal clark.

#include <algorithm>
#include <cmath>
#include <cstdlib>            //Needed for exit() calls.
#include <optional>
#include <fstream>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>    

#include "../Contour_Boolean_Operations.h"
#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "ContourBooleanOperations.h"
#include "Explicator.h"       //Needed for Explicator class.
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.

#ifdef DCMA_USE_CGAL
#else
    #error "Attempted to compile without CGAL support, which is required."
#endif


OperationDoc OpArgDocContourBooleanOperations(){
    OperationDoc out;
    out.name = "ContourBooleanOperations";

    out.desc = 
        "This routine performs 2D Boolean operations on user-provided sets of ROIs. The ROIs themselves are planar"
        " contours embedded in R^3, but the Boolean operation is performed once for each 2D plane where the selected ROIs"
        " reside. This routine can only perform Boolean operations on co-planar contours. This routine can operate on"
        " single contours (rather than ROIs composed of several contours) by simply presenting this routine with a single"
        " contour to select.";
        
    out.notes.emplace_back(
        "Contour ROI regex matches comprise the sets 'A' and 'B',"
        " as in f(A,B) where f is the Boolean operation."
    );
    out.notes.emplace_back(
        "This routine DOES support disconnected ROIs, such as left- and right-parotid contours that have been"
        " joined into a single 'parotids' ROI."
    );
    out.notes.emplace_back(
        "Many Boolean operations can produce contours with holes. This operation currently connects the interior"
        " and exterior with a seam so that holes can be represented by a single polygon (rather than a separate hole"
        " polygon). It *is* possible to export holes as contours with a negative orientation, but this was not needed when"
        " writing."
    );
    out.notes.emplace_back(
        "Only the common metadata between contours is propagated to the product contours."
    );
        

    out.args.emplace_back();
    out.args.back() = NCWhitelistOpArgDoc();
    out.args.back().name = "NormalizedROILabelRegexA";
    out.args.back().default_val = ".*";

    out.args.emplace_back();
    out.args.back() = RCWhitelistOpArgDoc();
    out.args.back().name = "ROILabelRegexA";
    out.args.back().default_val = ".*";

    out.args.emplace_back();
    out.args.back() = NCWhitelistOpArgDoc();
    out.args.back().name = "NormalizedROILabelRegexB";
    out.args.back().default_val = ".*";

    out.args.emplace_back();
    out.args.back() = RCWhitelistOpArgDoc();
    out.args.back().name = "ROILabelRegexB";
    out.args.back().default_val = ".*";

    out.args.emplace_back();
    out.args.back().name = "Operation";
    out.args.back().desc = "The Boolean operation (e.g., the function 'f') to perform on the sets of"
                      " contour polygons 'A' and 'B'. 'Symmetric difference' is also known as 'XOR'.";
    out.args.back().default_val = "join";
    out.args.back().expected = true;
    out.args.back().examples = { "intersection", "join", "difference", "symmetric_difference" };
    out.args.back().samples = OpArgSamples::Exhaustive;

    out.args.emplace_back();
    out.args.back().name = "OutputROILabel";
    out.args.back().desc = "The label to attach to the ROI contour product of f(A,B).";
    out.args.back().default_val = "Boolean_result";
    out.args.back().expected = true;
    out.args.back().examples = { "A+B", "A-B", "AuB", "AnB", "AxB", "A^B", "union", "xor", "combined", "body_without_spinal_cord" };

    return out;
}



bool ContourBooleanOperations(Drover &DICOM_data,
                                const OperationArgPkg& OptArgs,
                                std::map<std::string, std::string>& /*InvocationMetadata*/,
                                const std::string& FilenameLex){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ROILabelRegexA = OptArgs.getValueStr("ROILabelRegexA").value();
    const auto ROILabelRegexB = OptArgs.getValueStr("ROILabelRegexB").value();
    const auto NormalizedROILabelRegexA = OptArgs.getValueStr("NormalizedROILabelRegexA").value();
    const auto NormalizedROILabelRegexB = OptArgs.getValueStr("NormalizedROILabelRegexB").value();

    const auto Operation_str = OptArgs.getValueStr("Operation").value();
    const auto OutputROILabel = OptArgs.getValueStr("OutputROILabel").value();

    //-----------------------------------------------------------------------------------------------------------------
    const auto roiregexA = Compile_Regex(ROILabelRegexA);
    const auto roiregexB = Compile_Regex(ROILabelRegexB);

    const auto roinormalizedregexA = Compile_Regex(NormalizedROILabelRegexA);
    const auto roinormalizedregexB = Compile_Regex(NormalizedROILabelRegexB);

    const auto regex_join = Compile_Regex("^jo?i?n?$");
    const auto regex_intersection = Compile_Regex("^inte?r?s?e?c?t?i?o?n?$");
    const auto regex_difference = Compile_Regex("^diffe?r?e?n?c?e?$");
    const auto regex_symmdiff = Compile_Regex("^symme?t?r?i?c?_?d?i?f?f?e?r?e?n?c?e?$");

    //Figure out which operation is desired.
    ContourBooleanMethod op = ContourBooleanMethod::join;
    if(std::regex_match(Operation_str,regex_join)){
        op = ContourBooleanMethod::join;
    }else if(std::regex_match(Operation_str,regex_intersection)){
        op = ContourBooleanMethod::intersection;
    }else if(std::regex_match(Operation_str,regex_difference)){
        op = ContourBooleanMethod::difference;
    }else if(std::regex_match(Operation_str,regex_symmdiff)){
        op = ContourBooleanMethod::symmetric_difference;
    }else{
        throw std::logic_error("Unanticipated Boolean operation request.");
    }

    Explicator X(FilenameLex);

    //Stuff references to all contours into a list. Remember that you can still address specific contours through
    // the original holding containers (which are not modified here).
    auto cc_all = All_CCs( DICOM_data );
    auto cc_A = Whitelist( cc_all, { { "ROIName", ROILabelRegexA },
                                     { "NormalizedROIName", NormalizedROILabelRegexA } } );
    auto cc_B = Whitelist( cc_all, { { "ROIName", ROILabelRegexB },
                                     { "NormalizedROIName", NormalizedROILabelRegexB } } );


    //Make a copy of all contours for assessing some information later.
    std::list<std::reference_wrapper<contour_collection<double>>> cc_A_B;
    cc_A_B.insert(cc_A_B.end(), cc_A.begin(), cc_A.end());
    cc_A_B.insert(cc_A_B.end(), cc_B.begin(), cc_B.end());
    if(cc_A_B.empty()){
        throw std::invalid_argument("No contours were selected. Cannot continue.");
        // Note that while zero contours may technically be valid input for some operations (e.g., joins), it will most
        // likely indicate an error in ROI selection. If truly necessary, this routine can be modified to accept zero
        // contours OR dummy contour collections (i.e., containing no contours, or extremely small-area contours, or
        // distant contours, etc.) can be added by the user.
    }

    // Identify the unique planes represented in sets A and B.
    const auto est_cont_normal = cc_A_B.front().get().contours.front().Estimate_Planar_Normal();
    const auto ucp = Unique_Contour_Planes(cc_A_B, est_cont_normal, /*distance_eps=*/ 0.005);

    const double fallback_spacing = 0.25; // in DICOM units (usually mm).
    const auto cont_sep_range = std::abs(ucp.front().Get_Signed_Distance_To_Point( ucp.back().R_0 ));
    const double est_cont_spacing = (ucp.size() <= 1) ? fallback_spacing : cont_sep_range / static_cast<double>(ucp.size() - 1);
    const double est_cont_thickness = 0.5005 * est_cont_spacing; // Made slightly thicker to avoid gaps.

    //For identifying duplicate vertices later.
    const auto verts_equal_F = [](const vec3<double> &vA, const vec3<double> &vB) -> bool {
        return ( vA.sq_dist(vB) < std::pow(0.01,2.0) );
    };

    // For each plane, pack the shuttles with (only) the relevant contours.
    contour_collection<double> cc_new;
    for(const auto &aplane : ucp){
        std::list<std::reference_wrapper<contour_of_points<double>>> A;
        std::list<std::reference_wrapper<contour_of_points<double>>> B;

        for(auto &cc : cc_A){
            for(auto &cop : cc.get().contours){
                //Ignore contours that are not 'on' the specified plane.
                // We give planes a thickness to help determine coincidence.
                cop.Remove_Sequential_Duplicate_Points(verts_equal_F);
                cop.Remove_Needles(verts_equal_F);
                if(cop.points.empty()) continue;
                const auto dist_to_plane = std::abs(aplane.Get_Signed_Distance_To_Point(cop.points.front()));
                if(dist_to_plane > est_cont_thickness) continue;

                //Pack the contour into the shuttle.
                A.emplace_back(std::ref(cop));
            }
        }
        for(auto &cc : cc_B){
            for(auto &cop : cc.get().contours){
                cop.Remove_Sequential_Duplicate_Points(verts_equal_F);
                cop.Remove_Needles(verts_equal_F);
                if(cop.points.empty()) continue;
                const auto dist_to_plane = std::abs(aplane.Get_Signed_Distance_To_Point(cop.points.front()));
                if(dist_to_plane > est_cont_thickness) continue;

                B.emplace_back(std::ref(cop));
            }
        }

        //Perform the operation.
        //FUNCINFO("About to perform boolean operation. A and B have " << A.size() << " and " << B.size() << " elements, respectively");
        auto cc = ContourBoolean(aplane, A, B, op);

        //Insert any contours created into a holding contour_collection.
        cc_new.contours.splice(cc_new.contours.end(), std::move(cc.contours));
    }

    //Attach the requested metadata.
    cc_new.Insert_Metadata("ROIName", OutputROILabel);
    cc_new.Insert_Metadata("NormalizedROIName", X(OutputROILabel));
    cc_new.Insert_Metadata("ROINumber", "999");
    cc_new.Insert_Metadata("MinimumSeparation", std::to_string(est_cont_spacing));

    // Insert it into the Contour_Data.
    FUNCINFO("Boolean operation created " << cc_new.contours.size() << " contours");
    if(cc_new.contours.empty()){
        FUNCWARN("ROI was not added because it is empty");
        // Note: While it is valid to have no resulting contours (e.g., the difference operation), having zero contours
        // in the collection is not well-defined in many situations and will potentially cause issues in other operations.
        // So the result is not propagated out at this time.
    }else{
        DICOM_data.contour_data->ccs.emplace_back(cc_new);
    }

    return true;
}
