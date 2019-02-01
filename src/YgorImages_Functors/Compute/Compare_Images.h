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

    // Pixel thresholds for the images that will be edited. Only pixels with values between these thresholds (inclusive)
    // will be compared.
    double inc_lower_threshold = -(std::numeric_limits<double>::infinity());
    double inc_upper_threshold = std::numeric_limits<double>::infinity();

    // Pixel thresholds for the reference images. Only pixels with values between these thresholds (inclusive)
    // will be made available for comparison.
    //
    // Note that these thresholds should accommodate at least the acceptable discrepancy range, and ideally a reasonable
    // buffer beyond, otherwise the comparison can fail (possibly in hard-to-notice ways). These thresholds are meant to
    // exclude obviously irrelevant voxels or invalid portions of the images.
    double ref_img_inc_lower_threshold = -(std::numeric_limits<double>::infinity());
    double ref_img_inc_upper_threshold = std::numeric_limits<double>::infinity();

    // The channel to consider. 
    //
    // Note that the channel numbers in the images that will be edited and reference images must match.
    long int channel = 0;

    // Comparisons involving a distance-to-agreement (DTA) search.
    double DTA_vox_val_eq_abs = 1.0E-3;  // The difference in voxel values considered to be sufficiently equal (absolute; in voxel intensity units).
    double DTA_vox_val_eq_pdiff = 1.0;   // The difference in voxel values considered to be sufficiently equal (percent-difference; in %).
    double DTA_sampling_dx = 0.1;        // Sampling resolution (in DICOM units).


    // Gamma comparisons.
    double gamma_DTA_max = 3.0; // Maximally acceptable distance-to-agreement (in DICOM units).
    double gamma_DTA_stop_when_max_exceeded = true; // Halt spatial searching if the gamma index will necessarily indicate failure.
                                                    // This can considerably reduce the computational effort required,
                                                    // but the reported gamma indicies will be invalid when they are >1.

    double gamma_Dis_max_abs = 1.0;    // Voxel value discrepancy (absolute; in voxel intensity units).
    double gamma_Dis_max_pdiff = 1.0;  // Voxel value discrepancy (percent-difference; in %);

};

bool ComputeCompareImages(planar_image_collection<float,double> &,
                          std::list<std::reference_wrapper<planar_image_collection<float,double>>>,
                          std::list<std::reference_wrapper<contour_collection<double>>>,
                          std::experimental::any ud );

