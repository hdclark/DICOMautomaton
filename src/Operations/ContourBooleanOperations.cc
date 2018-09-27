//ContourBooleanOperations.cc - A part of DICOMautomaton 2017. Written by hal clark.

#include <algorithm>
#include <cmath>
#include <cstdlib>            //Needed for exit() calls.
#include <experimental/optional>
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



OperationDoc OpArgDocContourBooleanOperations(void){
    OperationDoc out;
    out.name = "ContourBooleanOperations";

    out.desc = 
        "This routine performs 2D Boolean operations on user-provided sets of ROIs. The ROIs themselves are planar"
        " contours embedded in R^3, but the Boolean operation is performed once for each 2D plane where the selected ROIs"
        " reside. This routine can only perform Boolean operations on co-planar contours. This routine can operate on"
        " single contours (rather than ROIs composed of several contours) by simply presenting this routine with a single"
        " contour to select.";
        
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
    out.args.back().name = "ROILabelRegexA";
    out.args.back().desc = "A regex matching ROI labels/names that comprise the set of contour polygons 'A'"
                      " as in f(A,B) where f is some Boolean operation." 
                      " The default with match all available ROIs, which is probably not what you want.";
    out.args.back().default_val = ".*";
    out.args.back().expected = true;
    out.args.back().examples = { ".*", ".*[pP]rostate.*", "body", "Gross_Liver",
                            R"***(.*left.*parotid.*|.*right.*parotid.*|.*eyes.*)***",
                            R"***(left_parotid|right_parotid)***" };

    out.args.emplace_back();
    out.args.back().name = "ROILabelRegexB";
    out.args.back().desc = "A regex matching ROI labels/names that comprise the set of contour polygons 'B'"
                      " as in f(A,B) where f is some Boolean operation." 
                      " The default with match all available ROIs, which is probably not what you want.";
    out.args.back().default_val = ".*";
    out.args.back().expected = true;
    out.args.back().examples = { ".*", ".*body.*", "body", "Gross_Liver",
                            R"***(.*left.*parotid.*|.*right.*parotid.*|.*eyes.*)***",
                            R"***(left_parotid|right_parotid)***" };

    out.args.emplace_back();
    out.args.back().name = "NormalizedROILabelRegexA";
    out.args.back().desc = "A regex matching ROI labels/names that comprise the set of contour polygons 'A'"
                      " as in f(A,B) where f is some Boolean operation. "
                      " The regex is applied to normalized ROI labels/names, which are translated using"
                      " a user-provided lexicon (i.e., a dictionary that supports fuzzy matching)."
                      " The default with match all available ROIs, which is probably not what you want.";
    out.args.back().default_val = ".*";
    out.args.back().expected = true;
    out.args.back().examples = { ".*", ".*Body.*", "Body", "Gross_Liver",
                            R"***(.*Left.*Parotid.*|.*Right.*Parotid.*|.*Eye.*)***",
                            R"***(Left Parotid|Right Parotid)***" };

    out.args.emplace_back();
    out.args.back().name = "NormalizedROILabelRegexB";
    out.args.back().desc = "A regex matching ROI labels/names that comprise the set of contour polygons 'B'"
                      " as in f(A,B) where f is some Boolean operation. "
                      " The regex is applied to normalized ROI labels/names, which are translated using"
                      " a user-provided lexicon (i.e., a dictionary that supports fuzzy matching)."
                      " The default with match all available ROIs, which is probably not what you want.";
    out.args.back().default_val = ".*";
    out.args.back().expected = true;
    out.args.back().examples = { ".*", ".*Body.*", "Body", "Gross_Liver",
                            R"***(.*Left.*Parotid.*|.*Right.*Parotid.*|.*Eye.*)***",
                            R"***(Left Parotid|Right Parotid)***" };

    out.args.emplace_back();
    out.args.back().name = "Operation";
    out.args.back().desc = "The Boolean operation (e.g., the function 'f') to perform on the sets of"
                      " contour polygons 'A' and 'B'. 'Symmetric difference' is also known as 'XOR'.";
    out.args.back().default_val = "join";
    out.args.back().expected = true;
    out.args.back().examples = { "intersection", "join", "difference", "symmetric_difference" };

    out.args.emplace_back();
    out.args.back().name = "OutputROILabel";
    out.args.back().desc = "The label to attach to the ROI contour product of f(A,B).";
    out.args.back().default_val = "Boolean_result";
    out.args.back().expected = true;
    out.args.back().examples = { "A+B", "A-B", "AuB", "AnB", "AxB", "A^B", "union", "xor", "combined", "body_without_spinal_cord" };

    return out;
}



Drover ContourBooleanOperations(Drover DICOM_data, OperationArgPkg OptArgs, std::map<std::string,std::string> /*InvocationMetadata*/, std::string FilenameLex){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ROILabelRegexA = OptArgs.getValueStr("ROILabelRegexA").value();
    const auto ROILabelRegexB = OptArgs.getValueStr("ROILabelRegexB").value();
    const auto NormalizedROILabelRegexA = OptArgs.getValueStr("NormalizedROILabelRegexA").value();
    const auto NormalizedROILabelRegexB = OptArgs.getValueStr("NormalizedROILabelRegexB").value();

    const auto Operation_str = OptArgs.getValueStr("Operation").value();
    const auto OutputROILabel = OptArgs.getValueStr("OutputROILabel").value();

    //-----------------------------------------------------------------------------------------------------------------
    const auto roiregexA = std::regex(ROILabelRegexA, std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto roiregexB = std::regex(ROILabelRegexB, std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);

    const auto roinormalizedregexA = std::regex(NormalizedROILabelRegexA, std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto roinormalizedregexB = std::regex(NormalizedROILabelRegexB, std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);

    const auto regex_join = Compile_Regex("^jo?i?n?$");
    const auto regex_intersection = Compile_Regex("^inte?r?s?e?c?t?i?o?n?$");
    const auto regex_difference = Compile_Regex("^diffe?r?e?n?c?e?$");
    const auto regex_symmdiff = Compile_Regex("^symme?t?r?i?c?_?d?i?f?f?e?r?e?n?c?e?$");

    //Figure out which operation is desired.
    ContourBooleanMethod op = ContourBooleanMethod::join;
    if(false){
    }else if(std::regex_match(Operation_str,regex_join)){
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
    std::list<std::reference_wrapper<contour_collection<double>>> cc_all;
    for(auto & cc : DICOM_data.contour_data->ccs){
        auto base_ptr = reinterpret_cast<contour_collection<double> *>(&cc);
        cc_all.push_back( std::ref(*base_ptr) );
    }

    //Whitelist contours using the provided regex.
    auto cc_A = cc_all;
    cc_A.remove_if([=](std::reference_wrapper<contour_collection<double>> cc) -> bool {
                   const auto ROINameOpt = cc.get().contours.front().GetMetadataValueAs<std::string>("ROIName");
                   const auto ROIName = ROINameOpt.value_or("");
                   return !(std::regex_match(ROIName,roiregexA));
    });
    cc_A.remove_if([=](std::reference_wrapper<contour_collection<double>> cc) -> bool {
                   const auto ROINameOpt = cc.get().contours.front().GetMetadataValueAs<std::string>("NormalizedROIName");
                   const auto ROIName = ROINameOpt.value_or("");
                   return !(std::regex_match(ROIName,roinormalizedregexA));
    });

    auto cc_B = cc_all;
    cc_B.remove_if([=](std::reference_wrapper<contour_collection<double>> cc) -> bool {
                   const auto ROINameOpt = cc.get().contours.front().GetMetadataValueAs<std::string>("ROIName");
                   const auto ROIName = ROINameOpt.value_or("");
                   return !(std::regex_match(ROIName,roiregexB));
    });
    cc_B.remove_if([=](std::reference_wrapper<contour_collection<double>> cc) -> bool {
                   const auto ROINameOpt = cc.get().contours.front().GetMetadataValueAs<std::string>("NormalizedROIName");
                   const auto ROIName = ROINameOpt.value_or("");
                   return !(std::regex_match(ROIName,roinormalizedregexB));
    });

    //Make a copy of all contours for assessing some information later.
    std::list<std::reference_wrapper<contour_collection<double>>> cc_A_B;
    cc_A_B.insert(cc_A_B.end(), cc_A.begin(), cc_A.end());
    cc_A_B.insert(cc_A_B.end(), cc_B.begin(), cc_B.end());

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
    }else{
        DICOM_data.contour_data->ccs.emplace_back(cc_new);
        DICOM_data.contour_data->ccs.back().ROI_number = 999;
        DICOM_data.contour_data->ccs.back().Minimum_Separation = est_cont_spacing;
        DICOM_data.contour_data->ccs.back().Raw_ROI_name = OutputROILabel;
    }

    return DICOM_data;
}
