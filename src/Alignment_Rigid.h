//Alignment_Rigid.h - A part of DICOMautomaton 2020. Written by hal clark.

#pragma once

#include <optional>
#include <limits>

#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorMath.h"         //Needed for vec3 class.


// This routine performs a simple centroid-based alignment.
//
// The resultant transformation is a rotation-less shift so the point cloud centres-of-mass overlap.
//
// Note that this routine only identifies a suitable transform, it does not implement it by altering the inputs.
//
// Any non-zero number of points are supported. Moving and stationary sets may differ in number of points.
// This algorithm is not strongly impacted by low-dimensional degeneracies, but mirroring can occur.
//
std::optional<affine_transform<double>>
AlignViaCentroid(const point_set<double> & moving,
                 const point_set<double> & stationary );


#ifdef DCMA_USE_EIGEN
// This routine performs a PCA-based alignment.
//
// The moving point cloud is translated the moving point cloud so the centre of mass aligns to the reference
// point cloud, PCA is performed separately on the reference and moving point clouds, distribution moments along
// each axis are computed to determine the direction, and then rotates the moving point cloud so the principle axes
// coincide.
//
// This algorithm will eagerly produce mirror transformations, so is best suited for dense, non-symmetric point sets.
//
// Note that this routine only identifies a suitable transform, it does not implement it by altering the inputs.
//
// Moving and stationary sets may differ in number of points. Each set should have at least two points.
// Special logic is provided to handle low-dimensional degeneracies, but it is recommended to not rely on it.
//
std::optional<affine_transform<double>>
AlignViaPCA(const point_set<double> & moving,
            const point_set<double> & stationary );
#endif // DCMA_USE_EIGEN


#ifdef DCMA_USE_EIGEN
// This routine performs an alignment using the "Orthogonal Procrustes" algorithm.
//
// This method is similar to PCA-based alignment, but singular-value decomposition (SVD) is used to estimate the best
// rotation. For more information, see the "Wahba problem" or the "Kabsch algorithm."
//
// In contrast to PCA this method disallows mirroring, making it suitable for low-density and/or symmetric point sets.
//
// Note that this routine only identifies a suitable transform, it does not implement it by altering the inputs.
//
// Moving and stationary sets must be paired and corresponding. Low-dimensional degeneracies are somewhat protected
// against, but the resulting transformation is not robust and may involve mirroring, so it is recommended to
// avoid cases with low-dimensional degeneracies.
//
std::optional<affine_transform<double>>
AlignViaOrthogonalProcrustes(const point_set<double> & moving,
                             const point_set<double> & stationary );
#endif // DCMA_USE_EIGEN


#ifdef DCMA_USE_EIGEN
// This routine performs an exhaustive iterative closest point (ICP) alignment.
//
// It alternates between phases of correspondence assessment (assuming nearest points correspond) and Orthogonal
// Procrustes transformation solving. This algorithm generally works well, but it is possible (even likely) to find a
// local optimum rather than a global optimum transformation.
//
// This algorithm works best when the point sets are initially aligned. This algorithm scales poorly due to
// correspondence estimation.
//
// Note that this routine only identifies a suitable transform, it does not implement it by altering the inputs.
//
// Any non-zero number of points are supported. Moving and stationary sets may differ in number of points.
// This algorithm is strongly impacted by low-dimensional degeneracies, but may produce mirror transforms.
//
std::optional<affine_transform<double>>
AlignViaExhaustiveICP( const point_set<double> & moving,
                       const point_set<double> & stationary,
                       long int max_icp_iters = 100,
                       double f_rel_tol = std::numeric_limits<double>::quiet_NaN() );
#endif // DCMA_USE_EIGEN


