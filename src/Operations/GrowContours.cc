//GrowContours.cc - A part of DICOMautomaton 2017. Written by hal clark.

#include <optional>
#include <list>
#include <map>
#include <memory>
#include <regex>
#include <string>    

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "GrowContours.h"
#include "YgorMath.h"         //Needed for vec3 class.



OperationDoc OpArgDocGrowContours(){
    OperationDoc out;
    out.name = "GrowContours";

    out.desc = 
        "This routine will grow (or shrink) 2D contours in their plane by the specified amount. "
        " Growth is accomplish by translating vertices away from the interior by the specified amount."
        " The direction is chosen to be the direction opposite of the in-plane normal produced by averaging the line"
        " segments connecting the contours.";


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
    out.args.back().name = "Distance";
    out.args.back().desc = "The distance to translate contour vertices. (The direction is outward.)";
    out.args.back().default_val = "0.00354165798657632";
    out.args.back().expected = true;
    out.args.back().examples = { "1E-5", "0.321", "1.1", "15.3" };

    return out;
}



Drover GrowContours(const Drover &DICOM_data, const OperationArgPkg& OptArgs, const std::map<std::string,std::string>&, const std::string& ){
    if(DICOM_data.contour_data == nullptr) return DICOM_data;

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ROILabelRegex = OptArgs.getValueStr("ROILabelRegex").value();
    const auto NormalizedROILabelRegex = OptArgs.getValueStr("NormalizedROILabelRegex").value();

    const auto dR = std::stod( OptArgs.getValueStr("Distance").value() );

    //-----------------------------------------------------------------------------------------------------------------

    const auto theregex = Compile_Regex(ROILabelRegex);
    const auto thenormalizedregex = Compile_Regex(NormalizedROILabelRegex);

    for(auto &cc : DICOM_data.contour_data->ccs){
        for(auto &cop : cc.contours){
            if(cop.points.size() < 3) continue;

            const auto ROINameOpt = cop.GetMetadataValueAs<std::string>("ROIName");
            const auto ROIName = ROINameOpt.value_or("");
            const auto NROINameOpt = cop.GetMetadataValueAs<std::string>("NormalizedROIName");
            const auto NROIName = NROINameOpt.value_or("");
//            if(!( std::regex_match(ROIName,theregex) || std::regex_match(NROIName,thenormalizedregex))) continue;
            if(!std::regex_match(ROIName,theregex)) continue;

            const auto N = cop.Estimate_Planar_Normal();
            const auto aplane = cop.Least_Squares_Best_Fit_Plane(N);

            auto cop_orig = cop;
            auto cop_ref = cop;
            //Simplify the implementation by duplicating the first and last vertices.

            cop_ref.points.push_front(cop.points.back());
            cop_ref.points.push_back(cop.points.front());

            auto itA = cop_ref.points.begin();
            auto itB = cop.points.begin();

            const auto R_cent = cop.Centroid();

            for( ; (itB != cop.points.end()) ; ++itA, ++itB ){
                auto itC = itA;

                const auto R_prev = *(itC++);
                const auto R_this = *(itC++);
                const auto R_next = *itC;

                const auto R_fwd = (R_this - R_next);
                const auto R_bak = (R_this - R_prev);
                const auto R_avg = (R_fwd + R_bak)*0.5;
                auto R_dir = R_avg.unit();

                R_dir = (R_this - R_cent).unit();
/*                

                //if(R_dir.length() < 0.5){ // Edge case: straight lines...
                { // Edge case: straight lines...
                    //Figure out which side is inside the contour and which side is outside. Move away from the inside.

                    const auto R_l = R_fwd.unit().rotate_around_z( M_PI)*0.05;
                    const auto R_r = R_fwd.unit().rotate_around_z(-M_PI)*0.05;
                    const auto PA = *itB + R_l;
                    const auto PB = *itB + R_r;
                    if(cop_orig.Is_Point_In_Polygon_Projected_Orthogonally(aplane, PA)){
                        R_dir = PA.unit();
                    }else{
                        R_dir = PB.unit();
                    }
                }
*/

                *itB += R_dir * dR;
            }
        }
    }

    return DICOM_data;
}
