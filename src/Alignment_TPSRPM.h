//Alignment_TPSRPM.h - A part of DICOMautomaton 2020. Written by hal clark.

#pragma once

#include <optional>
#include <iosfwd>

#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorLog.h"
#include "YgorMath.h"         //Needed for vec3 class.


// This class encapsulates enough data to *evaluate* a thin-plate spline transformation.
// It does not itself *solve* for such a transform though. See the corresponding free functions that can.
class thin_plate_spline {
    public:
        point_set<double> control_points;
        long int kernel_dimension;
        num_array<double> W_A;

        // Constructor.
        //
        thin_plate_spline() = delete;
        thin_plate_spline(std::istream &is); // Defers to read_from(), but throws on errors.
        thin_plate_spline(const point_set<double> &ps,     // Requires moving points, not stationary points.
                          long int kernel_dimension = 2L); // 2 seems to work best, even in 3D.

        // Member functions.
        double eval_kernel(const double &dist) const;

        vec3<double> transform(const vec3<double> &v) const;
        void apply_to(point_set<double> &ps) const; // Included for parity with affine_transform class.
        void apply_to(vec3<double> &v) const;       // Included for parity with affine_transform class.

        // Serialize and deserialize to a human- and machine-readable format.
        bool write_to( std::ostream &os ) const;
        bool read_from( std::istream &is );
};


#ifdef DCMA_USE_EIGEN
// This routine finds a non-rigid alignment using thin plate splines.
//
// Note that the point sets must be ordered and have the same number of points, and each pair (i.e., the nth moving
// point and the nth stationary point) correspond.
//
// Note that this routine only identifies a transform, it does not implement it by altering the inputs.
//
struct AlignViaTPSParams {
    // Dimensionality of the kernel. It *should* match the dimensionality of the points (i.e., 3), but doesn't need to.
    // 2 seems to work best.
    long int kernel_dimension = 2;

    // Regularization parameter. Controls the smoothness of the fitted thin plate spline function.
    // Setting to zero will ensure that all points are interpolated exactly (up to numerical imprecision). Setting
    // higher will allow the spline to 'relax' and smooth out.
    double lambda = 0.0;

    // The method used to solve the system of linear equtions that defines the thin plate spline solution.
    // The pseudoinverse will likely be able to provide a solution when the system is degenerate, but it might not be
    // reasonable.
    enum class SolutionMethod {
        PseudoInverse,
        LDLT
    };
    SolutionMethod solution_method = SolutionMethod::LDLT;
};

std::optional<thin_plate_spline>
AlignViaTPS(AlignViaTPSParams & params,
            const point_set<double> & moving,
            const point_set<double> & stationary );
#endif // DCMA_USE_EIGEN


#ifdef DCMA_USE_EIGEN
// This routine finds a non-rigid alignment using the 'robust point matching: thin plate spline' algorithm.
//
// Both the alignment and correspondence are determined using an iterative approach. This routine may require tweaking
// in order to suit the problem domain.
//
// Note that this routine only identifies a transform, it does not implement it by altering the inputs.
//
struct AlignViaTPSRPMParams {
    // TPS parameters.
    //
    // Dimensionality of the kernel. It *should* match the dimensionality of the points (i.e., 3), but doesn't need to.
    // 2 seems to work best.
    long int kernel_dimension = 2;

    // Annealing parameters.
    //
    // Temperature parameters. Starting and end parameters are relative to the max square-distance between points and
    // the mean nearest-neighbour square-distance, respectively. The temperature step is a multiplicative factor, so
    // setting to 0.95 would reduce the temperature to 95% of the current temperature each iteration.
    double T_start_scale = 1.05; // Slightly larger than all possible to allow any pairing.
    double T_end_scale   = 0.01; // Default to precise registration. Higher numbers = coarser registration.
    double T_step        = 0.93; // Should be [0.9:0.99] or so. Larger number = slower annealing.

    // The number of iterations (i.e., update correspondence, update TPS function cycles) to perform at each T.
    long int N_iters_at_fixed_T = 5; // Lower = faster, but possibly less accurate.

    // The number of iterations to perform in the softassign correspondence update step. May need to be higher if any
    // forced correspondences are used. Note that this number is worst-case; if tolerance is reached earlier then the
    // Sinkhorn procedure completes early.
    long int N_Sinkhorn_iters = 5000; // Lower = faster, but possibly less accurate.

    // The tolerable worst deviation from row- and column-sum normalization conditions. If this tolerance is reached,
    // the Sinkhorn procedure is completed early. Conversely, if this tolerance is NOT reached after the allowed
    // iterations, then the algorithm terminates due to failure.
    double Sinkhorn_tolerance = 0.01;

    // Regularization parameters.
    //
    // Controls the smoothness of the fitted thin plate spline function.
    // Setting to zero will ensure that all points are interpolated exactly (up to numerical imprecision). Setting
    // higher will allow the spline to 'relax' and smooth out.
    double lambda_start = 0.0;

    // Controls the balance of how points are considered to be outliers.
    // Setting to zero will disable this bias. Setting higher will cause fewer points to be considered outliers.
    double zeta_start = 0.0;

    // Whether to use a modified version of the 'double-sided outlier handling' approach described by Yang et al.
    // (2011; doi:10.1016/j.patrec.2011.01.015). Enabling this parameter adjusts the interpretation of the lambda
    // regularization parameter, and can also result in reduced numerical stability.
    //
    // Note: The double-sided error handling algorithm also seems to be more sensitive to kernel dimension.
    bool double_sided_outliers = false;

    // Whether to permit outlier detection. A major strength of the TPS-RPM algorithm is semi-automatic outlier
    // detection and handling. Disabling outlier detection keeps the correspondence determination aspect of the TPS-RPM
    // algorithm. These parameters control whether correspondence matrix elements used to indicate a point is an outlier
    // (i.e., the 'gutter' coefficients) are overridden. Disabling outlier detection may cause the Sinkhorn procedure to
    // fail to converge.
    //
    // Note: Outlier detection should not be disabled when forced correspondence is used to force points to be outliers.
    //
    // Note: The Sinkhorn normalization method will likely fail when outliers in the larger point cloud are disallowed.
    bool permit_move_outliers = true;
    bool permit_stat_outliers = true;

    // Solver parameters.
    //
    // The method used to solve the system of linear equtions that defines the thin plate spline solution.
    // The pseudoinverse will likely be able to provide a solution when the system is degenerate, but it might not be
    // reasonable.
    enum class SolutionMethod {
        PseudoInverse,
        LDLT
    };
    SolutionMethod solution_method = SolutionMethod::LDLT;

    // Algorithm-altering parameters.
    //
    // Seed the initial transformation with the result of a rigid centroid-to-centroid shift transformation. The default
    // initial transformation is an identity transform; if the point sets have a deliberate relative position, then a
    // centroid shift may be detrimental. Alternatively, if the point clouds have a similar shape then seeding the
    // transformation might facilitate fewer annealing steps and/or a cooler starting temperature, speeding up the
    // computation.
    bool seed_with_centroid_shift = false;

    // Correspondence parameters.
    //
    // Point-pairs that are forced to correspond. Indices are zero-based. The first index refers to the moving set, and
    // the second refers to the stationary set. Note that forced correspondences cause the two named points to
    // *exclusively* correspond. So a single point cannot be named twice.
    //
    // Note that use of forced correspondence may cause the softassign procedure to fail to converge, or converge
    // slowly. Adjusting the number of iterations used in the Sinkhorn procedure may be required.
    std::vector< std::pair<long int, long int> > forced_correspondence;

    // The final correspondence, interpretted as a binary correspondence by (effectively) taking T -> 0. These
    // correspondences refer to the mapping from a moving set point index (the first index) to a stationary set point
    // index (the second index) for **both** outputs. Indices are zero-based and legitmate correspondences
    // span [0, num_points_in_other_set - 1]. Outliers will have the index *just after* the maximum (i.e., equal to the
    // total number of points in the corresponding set). Note that the correspondence will not always be symmetric! For
    // most use-cases, the moving set correspondence is probably all that the user will want. However, the full set of
    // information can be useful for double-sided outlier detection, evaluating the registration, evaluating whether the
    // softassign procedure has appropriately coalesced, predicting how the inverse transformation will behave, etc.
    bool report_final_correspondence = false;
    std::vector< std::pair<long int, long int> > final_move_correspondence; // moving point index -> stat point index.
    std::vector< std::pair<long int, long int> > final_stat_correspondence; // ALSO moving point index -> stat point index.

};

std::optional<thin_plate_spline>
AlignViaTPSRPM(AlignViaTPSRPMParams & params,
               const point_set<double> & moving,
               const point_set<double> & stationary );
#endif // DCMA_USE_EIGEN

