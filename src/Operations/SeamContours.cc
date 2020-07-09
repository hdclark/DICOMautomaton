//SeamContours.cc - A part of DICOMautomaton 2017. Written by hal clark.

#ifdef DCMA_USE_CGAL
#else
    #error "Attempted to compile without CGAL support, which is required."
#endif

#include <algorithm>
#include <cmath>
#include <cstdlib>            //Needed for exit() calls.
#include <fstream>
#include <functional>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <set> 
#include <stdexcept>
#include <string>    

#include "../Contour_Boolean_Operations.h"
#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "SeamContours.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.



OperationDoc OpArgDocSeamContours(){
    OperationDoc out;
    out.name = "SeamContours";

    out.desc = 
        "This routine converts contours that represent 'outer' and 'inner' via contour orientation into contours that are"
        " uniformly outer but have a zero-area seam connecting the inner and outer portions.";
        
    out.notes.emplace_back(
        "This routine currently operates on all available ROIs."
    );
        
    out.notes.emplace_back(
        "This routine operates on one contour_collection at a time. It will combine contours that are in the same"
        " contour_collection and overlap, even if they have different ROINames. Consider making a complementary routine"
        " that partitions contours into ROIs based on ROIName (or other metadata) if more rigorous enforcement is needed."
    );
        
    out.notes.emplace_back(
        "This routine actually computes the XOR Boolean of contours that overlap. So if contours partially overlap,"
        " this routine will treat the overlapping parts as if they are holes, and the non-overlapping parts as if they"
        " represent the ROI. This behaviour may be surprising in some cases."
    );
        
    out.notes.emplace_back(
        "This routine will also treat overlapping contours with like orientation as if the smaller contour were a"
        " hole of the larger contour."
    );
        
    out.notes.emplace_back(
        "This routine will ignore contour orientation if there is only a single contour. More specifically, for a"
        " given ROI label, planes with a single contour will be unaltered."
    );
        
    out.notes.emplace_back(
        "Only the common metadata between outer and inner contours is propagated to the seamed contours."
    );
        
    out.notes.emplace_back(
        "This routine will NOT combine disconnected contours with a seam. Disconnected contours will remain"
        " disconnected."
    );

    return out;
}



Drover SeamContours(Drover DICOM_data, OperationArgPkg, std::map<std::string,std::string>, std::string ){

    if(DICOM_data.contour_data == nullptr) return DICOM_data;

    //For identifying duplicate vertices later.
    const auto verts_equal_F = [](const vec3<double> &vA, const vec3<double> &vB) -> bool {
        return ( vA.sq_dist(vB) < std::pow(0.01,2.0) );
    };
    
    auto cd = DICOM_data.contour_data->Duplicate();
    if(cd == nullptr) throw std::logic_error("Unable to duplicate contours. Cannot continue.");

    for(auto &cc : cd->ccs){
        if(cc.contours.empty()) continue;
        if(cc.contours.front().points.empty()){
            throw std::logic_error("Planar normal estimation technique failed. Consider searching more contours.");
        }
 
        // Identify the unique planes.
        std::list<std::reference_wrapper<contour_collection<double>>> cc_ref;
        {
            auto base_ptr = reinterpret_cast<contour_collection<double> *>(&cc);
            cc_ref.push_back( std::ref(*base_ptr) );
        }
        const auto est_cont_normal = cc.contours.front().Estimate_Planar_Normal();
        const auto ucp = Unique_Contour_Planes(cc_ref, est_cont_normal, /*distance_eps=*/ 0.005);

        const double fallback_spacing = 0.005; // in DICOM units (usually mm).
        const auto cont_sep_range = std::abs(ucp.front().Get_Signed_Distance_To_Point( ucp.back().R_0 ));
        const double est_cont_spacing = (ucp.size() <= 1) ? fallback_spacing : cont_sep_range / static_cast<double>(ucp.size() - 1);
        const double est_cont_thickness = 0.5005 * est_cont_spacing; // Made slightly thicker to avoid gaps.

        
        // For each plane, pack the shuttles with (only) the relevant contours.
        contour_collection<double> cc_new;
        for(const auto &aplane : ucp){
            std::list<std::reference_wrapper<contour_of_points<double>>> copl;
            std::set<std::string> ROINames;
            for(auto &cop : cc.contours){
                //Ignore contours that are not 'on' the specified plane.
                // We give planes a thickness to help determine coincidence.
                cop.Remove_Sequential_Duplicate_Points(verts_equal_F);
                cop.Remove_Needles(verts_equal_F);
                if(cop.points.size() < 3) continue;
                const auto dist_to_plane = std::abs(aplane.Get_Signed_Distance_To_Point(cop.points.front()));
                if(dist_to_plane > est_cont_thickness) continue;

                //Pack the contour into the shuttle.
                copl.emplace_back(std::ref(cop));
                ROINames.insert( cop.metadata["ROIName"] );
            }

            if(false){
            }else if(copl.empty()){
                throw std::logic_error("Found no contours incident on plane previously found to house contours.");
            }else if(copl.size() == 1){ // No Boolean operation needed. Copy as-is.
                cc_new.contours.emplace_back( copl.front().get() );
            }else{ // Possible overlap. Let CGAL work it out...
                const auto construct_op = ContourBooleanMethod::symmetric_difference;
                const auto op = ContourBooleanMethod::noop;
                auto cc_out = ContourBoolean(aplane, copl, {}, op, construct_op);

                if(ROINames.size() != 1){ //Warn if ROINames vary.
                    std::stringstream ss;
                    ss << "Seamed contours that had different ROI names (";
                    ss << *ROINames.begin();
                    for(auto it = std::next(ROINames.begin()); it != ROINames.end(); ++it){
                        ss << ", " << *it;
                    }
                    ss << "). Was this intentional?";
                    FUNCWARN(ss.str());
                    // Implementation Note:
                    // This will happen if a contour collection has contours from more than one ROI.
                    // When I implemented this, I found that there was always 1 ROI per contour_collection.
                    // If this needs to be more rigourous enforced, consider both making this an error and 
                    // providing a separate operation for partitioning contours using ROINames.
                }
                cc_new.contours.splice(cc_new.contours.end(), std::move(cc_out.contours));
            }
        }

        // Replace the existing cc.
        cc.contours.clear();
        cc.contours.splice( cc.contours.end(), std::move(cc_new.contours) );
    }

    DICOM_data.contour_data = std::move(cd);
    return DICOM_data;
}
