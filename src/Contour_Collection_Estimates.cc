//Contour_Collection_Estimates.cc - A part of DICOMautomaton 2017. Written by hal clark.

#include <initializer_list>
#include <limits>
#include <list>

#include "YgorMisc.h"
#include "YgorMath.h"

#include "Contour_Collection_Estimates.h"


double Estimate_Contour_Separation_Multi(
           std::list<std::reference_wrapper<contour_collection<double>>> ccl ){

    //Zero contours have no meaningful separation.
    if(ccl.empty()) return std::numeric_limits<double>::quiet_NaN();

    // This routine is used to estimate the minimum separation of a collection of contours above some threshold value
    // an 'epsilon' value. It is not always possible to estimate contour separation, but there are many methods which
    // could yield an acceptable solution. They are attempted here.
    //
    // Note: If no method is successful, a default separation based on typical CT slice thickness is returned.
    const double fallback_sep = 2.5; // in DICOM units (usually mm).

    //------------------------------------------------------------------
    // Estimation technique A: extract from common metadata.
    //
    // This assumes that the initial estimation was legitimate (it may not be) and that the contours have not been
    // altered since loading (or that the metadata was updated correctly). OTOH this routine can make use of information
    // communicated inside channels, such as DICOM headers.
    try{
        auto common_metadata = contour_collection<double>().get_common_metadata(ccl, {});
        const auto md_cont_sep_str = common_metadata["MinimumSeparation"];
        const auto md_cont_sep = std::stod(md_cont_sep_str);
        return md_cont_sep;
    }catch(const std::exception &){}

    //------------------------------------------------------------------
    // Estimation technique B: extract from contours directly.
    //
    // This method will be costly if there are many contours. It provides the most up-to-date estimate, but also
    // requires an estimation of the contour normal. It also assumes the contour normal is identical for all contours,
    // which may not be true in some cases. This method will also fail for single contours.
    try{
        const vec3<double> contour_normal = Average_Contour_Normals(ccl);
        double est_spacing = Estimate_Contour_Separation(ccl, contour_normal);
        if(std::isfinite(est_spacing)) return est_spacing;
    }catch(const std::exception &){}

    //------------------------------------------------------------------
    //Otherwise, use the default;
    return fallback_sep;
}


