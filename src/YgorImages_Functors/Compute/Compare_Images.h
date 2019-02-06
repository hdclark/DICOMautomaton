//Compare_Images.h.
#pragma once

#include <cmath>
#include <experimental/any>
#include <functional>
#include <limits>
#include <list>
#include <map>
#include <string>
#include <vector>

#include "YgorImages.h"
#include "YgorMath.h"
#include "YgorMisc.h"

template <class T, class R> class planar_image_collection;
template <class T> class contour_collection;

struct ComputeCompareImagesUserData {

    // -----------------------------
    // The type of comparison method to use.
    enum class
    ComparisonMethod {
        DTA,             // Distance-to-agreement (i.e., search neighbourhood until agreement is found).
        Discrepancy,     // Discrepancy (i.e., value comparison from voxel to nearest reference voxel only).
        GammaIndex,      // Gamma index -- a blend of DTA and discrepancy comparisons. 
    } comparison_method = ComparisonMethod::GammaIndex;


    // -----------------------------
    // The channel to consider. 
    //
    // Note: Channel numbers in the images that will be edited and reference images must match.
    long int channel = 0;


    // -----------------------------
    // Parameters for pixel thresholds.

    // Pixel thresholds for the images that will be edited. Only pixels with values between these thresholds (inclusive)
    // will be compared.
    double inc_lower_threshold = -(std::numeric_limits<double>::infinity());
    double inc_upper_threshold = std::numeric_limits<double>::infinity();

    // Pixel thresholds for the reference images. Only pixels with values between these thresholds (inclusive)
    // will be made available for comparison.
    //
    // Note: These thresholds should accommodate at least the acceptable discrepancy range, and ideally a reasonable
    //      buffer beyond, otherwise the comparison can fail (possibly in hard-to-notice ways). These thresholds are
    //      meant to exclude obviously irrelevant voxels or invalid portions of the images.
    double ref_img_inc_lower_threshold = -(std::numeric_limits<double>::infinity());
    double ref_img_inc_upper_threshold = std::numeric_limits<double>::infinity();


    // -----------------------------
    // Parameters for all comparisons involving a distance-to-agreement (DTA) search.
    
    // The difference in voxel values considered to be sufficiently equal (absolute; in voxel intensity units).
    //
    // Note: This value CAN be zero.
    double DTA_vox_val_eq_abs = 1.0E-3;
    
    // The difference in voxel values considered to be sufficiently equal (~percent-difference; in %).
    //
    // Note: This value CAN be zero.
    double DTA_vox_val_eq_reldiff = 1.0;

    // Maximally acceptable distance-to-agreement (in DICOM units: mm) above which to stop searching.
    //
    // Note: Some voxels further than the DTA_max may be evaluated. All voxels within the DTA_max will be evaluated.
    double DTA_max = 3.0;


    // -----------------------------
    // Parameters for Gamma comparisons.

    // Maximally acceptable distance-to-agreement (in DICOM units: mm).
    //
    // Note: This parameter can differ from the DTA_max search cut-off, but should be <= to it.
    double gamma_DTA_threshold = 5.0;

    // Voxel value relative discrepancy (~percent-difference; in %);
    double gamma_Dis_reldiff_threshold = 5.0;


    // Halt spatial searching if the gamma index will necessarily indicate failure.
    //
    // Note: This can parameter can drastically reduce the computational effort required to compute the gamma index,
    //       but the reported gamma values will be invalid whenever they are >1. This is often tolerable since the
    //       magnitude only matters when it is <1.
    double gamma_terminate_when_max_exceeded = true;
    double gamma_terminated_early = std::nextafter(1.0, std::numeric_limits<double>::infinity());

};

bool ComputeCompareImages(planar_image_collection<float,double> &,
                          std::list<std::reference_wrapper<planar_image_collection<float,double>>>,
                          std::list<std::reference_wrapper<contour_collection<double>>>,
                          std::experimental::any ud );

