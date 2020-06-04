//Alignment_Rigid.h - A part of DICOMautomaton 2020. Written by hal clark.

#pragma once

#include <optional>
#include <limits>

#include "Structs.h"

#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorMath.h"         //Needed for vec3 class.


// This routine performs a simple centroid-based alignment.
//
// The resultant transformation is a rotation-less shift so the point cloud centres-of-mass overlap.
//
// Note that this routine only identifies a suitable transform, it does not implement it by altering the inputs.
//
std::optional<affine_transform<double>>
AlignViaCentroid(const point_set<double> & moving,
                 const point_set<double> & stationary );


#ifdef DCMA_USE_EIGEN
// This routine performs a PCA-based alignment.
//
// First, the moving point cloud is translated the moving point cloud so the centre of mass aligns to the reference
// point cloud, performs PCA separately on the reference and moving point clouds, compute distribution moments along
// each axis to determine the direction, and then rotates the moving point cloud so the principle axes coincide.
//
// Note that this routine only identifies a suitable transform, it does not implement it by altering the inputs.
//
std::optional<affine_transform<double>>
AlignViaPCA(const point_set<double> & moving,
            const point_set<double> & stationary );
#endif // DCMA_USE_EIGEN


#ifdef DCMA_USE_EIGEN
// This routine performs an exhaustive iterative closest point (ICP) alignment.
//
// Note that this routine only identifies a suitable transform, it does not implement it by altering the inputs.
//
std::optional<affine_transform<double>>
AlignViaExhaustiveICP( const point_set<double> & moving,
                       const point_set<double> & stationary,
                       long int max_icp_iters = 100,
                       double f_rel_tol = std::numeric_limits<double>::quiet_NaN() );
#endif // DCMA_USE_EIGEN


