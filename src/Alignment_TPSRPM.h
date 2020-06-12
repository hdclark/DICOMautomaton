//Alignment_TPSRPM.h - A part of DICOMautomaton 2020. Written by hal clark.

#pragma once

#include <optional>
#include <iosfwd>

#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
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
        thin_plate_spline(const point_set<double> &ps,     // Requires moving points, not stationary points.
                          long int kernel_dimension = 2L); // 2 seems to work best, even in 3D.

        // Member functions.
        double eval_kernel(const double &dist) const;

        vec3<double> transform(const vec3<double> &v) const;
        void apply_to(point_set<double> &ps) const;

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

    // Should forced correspondences go here too?? TODO.

    // ... TODO ...

    // Should we report final correspondence in some way?
    // Yes! -- it probably makes sense to provide them back to the caller, since they will also have the original points
    // sets handy. TODO.

    // ... TODO ...
};

std::optional<thin_plate_spline>
AlignViaTPS(AlignViaTPSParams &params,
            const point_set<double> & moving,
            const point_set<double> & stationary );
#endif // DCMA_USE_EIGEN


#ifdef DCMA_USE_EIGEN
// This routine finds a non-rigid alignment using the 'robust point matching: thin plate spline' algorithm.
//
// Note that this routine only identifies a transform, it does not implement it by altering the inputs.
//
std::optional<thin_plate_spline>
AlignViaTPSRPM(const point_set<double> & moving,
               const point_set<double> & stationary );
#endif // DCMA_USE_EIGEN

