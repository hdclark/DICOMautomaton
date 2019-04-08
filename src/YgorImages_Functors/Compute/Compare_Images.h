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
    // The channel to consider. 
    //
    // Note: Channel numbers in the images that will be edited and reference images must match.
    long int channel = 0;


    // -----------------------------
    // The type of comparison method to use.
    enum class
    ComparisonMethod {
        DTA,             // Distance-to-agreement (i.e., search neighbourhood until agreement is found).
        Discrepancy,     // Discrepancy (i.e., value comparison from voxel to nearest reference voxel only).
        GammaIndex,      // Gamma index -- a blend of DTA and discrepancy comparisons. 
    } comparison_method = ComparisonMethod::GammaIndex;


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
    //       buffer beyond, otherwise the comparison can fail (possibly in hard-to-notice ways). These thresholds are
    //       meant to exclude obviously irrelevant voxels or invalid portions of the images.
    double ref_img_inc_lower_threshold = -(std::numeric_limits<double>::infinity());
    double ref_img_inc_upper_threshold = std::numeric_limits<double>::infinity();


    // -----------------------------
    // Parameters for all comparisons involving a distance-to-agreement (DTA) search.
    
    // The difference in voxel values considered to be sufficiently equal (absolute; in voxel intensity units).
    //
    // Note: This value CAN be zero.
    double DTA_vox_val_eq_abs = 1.0E-3;
    
    // The difference in voxel values considered to be sufficiently equal (~percent-difference; in %/100).
    //
    // Note: This value CAN be zero.
    double DTA_vox_val_eq_reldiff = 1.0 / 100.0;  // Note: 1/100 = 1%.

    // Maximally acceptable distance-to-agreement (in DICOM units: mm) above which to stop searching.
    //
    // Note: Some voxels further than the DTA_max may be evaluated. All voxels within the DTA_max will be evaluated.
    double DTA_max = 3.0;

    // Control how precisely and how often the space between voxel centres are interpolated to identify the exact
    // position of agreement. There are currently three options: no interpolation, nearest-neighbour, and
    // next-nearest-neighbour. 
    //
    // If no interpolation is selected, the agreement position will only be established to
    // within approximately the reference image voxels dimensions. To avoid interpolation, voxels that straddle the
    // target value are taken as the agreement distance. Conceptually, if you view a voxel as having a finite spatial
    // extent then this method may be sufficient for distance assessment. Though it is not precise, it is fast. 
    // This method will tend to over-estimate the actual distance, though it is possible that it slightly
    // under-estimates it. This method works best when the reference image grid size is small in comparison to the
    // desired spatial accuracy (e.g., if computing gamma, the tolerance should be much larger than the largest voxel
    // dimension) so supersampling is recommended.
    //
    // Nearest-neighbour interpolation considers the line connecting directly adjacent voxels. Using linear
    // interpolation along this line when adjacent voxels straddle the target value, the 3D point where the target value
    // appears can be predicted. This method can significantly improve distance estimation accuracy, though will
    // typically be much slower than no interpolation. On the other hand, this method lower amounts of supersampling,
    // though it is most reliable when the reference image grid size is small in comparison to the desired spatial
    // accuracy. Note that nearest-neighbour interpolation also makes use of the 'no interpolation' methods.
    //
    // Finally, next-nearest-neighbour considers the diagonally-adjacent neighbours separated by taxi-cab distance of 2
    // (so in-plane diagonals are considered, but 3D diagonals are not). Quadratic (i.e., bi-linear) interpolation is
    // analytically solved to determine where along the straddling diagonal the target value appears. This method is
    // more expensive than linear interpolation but will generally result in more accurate distance estimates. This
    // method may require lower amounts of supersampling than linear interpolation, but is most reliable when the
    // reference image grid size is small in comparison to the desired spatial accuracy. Use of this method may not be
    // appropriate in all cases considering that supersampling may be needed and a quadratic equation is solved for
    // every voxel diagonal. Note that next-nearest-neighbour interpolation also makes use of the nearest-neighbour and
    // 'no interpolation' methods.
    enum class
    InterpolationMethod {
        None,       // No voxel-to-voxel interpolation, only a simple straddle method.
        NN,         // Nearest-neighbour interpolation, along with the simple straddle method.
        NNN,        // Next-nearest-neighbour interpolation, along with NN and a simple straddle method.
    } interpolation_method = InterpolationMethod::NN;


    // -----------------------------
    // Parameters for all comparisons involving discrepancy.
    enum class
    DiscrepancyType {
        Difference,    // Absolute value of the difference between two voxels (i.e., subtraction; in voxel intensity units).
        Relative,      // Relative discrepancy between two voxels; the difference divided by the largest value (in %).
        PinnedToMax,   // Normalized relative discrepancy; the difference divided by the image's largest voxel value (in %).
    } discrepancy_type = DiscrepancyType::Relative;


    // -----------------------------
    // Parameters for all comparisons involving discrepancy.
    enum class
    DiscrepancyType {
        Difference,    // Absolute value of the difference between two voxels (i.e., subtraction; in voxel intensity units).
        Relative,      // Relative discrepancy between two voxels; the difference divided by the largest value (in %).
        PinnedToMax,   // Normalized relative discrepancy; the difference divided by the image's largest voxel value (in %).
    } discrepancy_type = DiscrepancyType::Relative;


    // -----------------------------
    // Parameters for Gamma comparisons.

    // Maximally acceptable distance-to-agreement (in DICOM units: mm).
    //
    // Note: This parameter can differ from the DTA_max search cut-off, but should be <= to it.
    double gamma_DTA_threshold = 5.0;

    // Direct, voxel-to-voxel value discrepancy threshold (~percent-difference in %/100, but depends on the DiscrepancyType);
    double gamma_Dis_threshold = 5.0 / 100.0;  // Note: 5/100 = 5%.

    // Halt spatial searching if the gamma index will necessarily indicate failure.
    //
    // Note: This can parameter can drastically reduce the computational effort required to compute the gamma index,
    //       but the reported gamma values will be invalid whenever they are >1. This is often tolerable since the
    //       magnitude only matters when it is <1.
    double gamma_terminate_when_max_exceeded = true;
    double gamma_terminated_early = std::nextafter(1.0, std::numeric_limits<double>::infinity());

    // Outgoing gamma passing counts.
    //
    // These can be read by the caller after performing a gamma analysis.
    long int passed = 0;  // The number of voxels that passed (i.e., gamma < 1).
    long int count = 0;   // The number of voxels that were considered (i.e., within the inclusivity thresholds).

};

bool ComputeCompareImages(planar_image_collection<float,double> &,
                          std::list<std::reference_wrapper<planar_image_collection<float,double>>>,
                          std::list<std::reference_wrapper<contour_collection<double>>>,
                          std::experimental::any ud );

