//Alignment_TPSRPM.h - A part of DICOMautomaton 2020. Written by hal clark.

#pragma once

#include <optional>

#include "Structs.h"

#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorMath.h"         //Needed for vec3 class.


#ifdef DCMA_USE_EIGEN
// This routine finds a non-rigid alignment using thin plate splines.
//
// Note that the point sets must be ordered and have the same number of points, and each pair (i.e., the nth moving
// point and the nth stationary point) correspond.
//
// Note that this routine only identifies a transform, it does not implement it by altering the inputs.
//
std::optional<affine_transform<double>>
AlignViaTPS(const point_set<double> & moving,
            const point_set<double> & stationary );
#endif // DCMA_USE_EIGEN


#ifdef DCMA_USE_EIGEN
// This routine finds a non-rigid alignment using the 'robust point matching: thin plate spline' algorithm.
//
// Note that this routine only identifies a transform, it does not implement it by altering the inputs.
//
std::optional<affine_transform<double>>
AlignViaTPSRPM(const point_set<double> & moving,
               const point_set<double> & stationary );
#endif // DCMA_USE_EIGEN

