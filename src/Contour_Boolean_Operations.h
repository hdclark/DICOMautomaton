//Contour_Boolean_Operations.h.

#pragma once

// These functions are used to perform first-order Boolean operations on (2D) polygon contours.

#include <list>
#include <functional>
#include <limits>
#include <map>
#include <cmath>
#include <experimental/any>

#include "YgorMisc.h"
#include "YgorMath.h"


typedef enum { 
    noop,                 // Perform no operation.
    join,                 // A ∪ B (or A + B; aka, the "union").
    intersection,         // A ∩ B (aka, the "overlap").
    difference,           // A - B (aka, all of A except the part shared by B).
    symmetric_difference  // A x B = (A-B) ∪ (B-A) (aka, all of A except the part shared by B, and also
                          //                        all of B except the part shared by A.)
} ContourBooleanMethod;


// Because ROI contours are 2D planar contours embedded in R^3, an explicit projection plane must be provided. Contours
// are projected on the plane, an orthonormal basis is created, the projected contours are expressed in the basis, and
// the Boolean operations are performed. Note that the outgoing contours remain projected onto the provided plane.
//
// Note: This routine is only designed to handle simple, non-self-intersecting polygons. Operations on other contours
//       are potentially undefined. (Refer to the documentation provided by the supporting library.)
//
// Note: This routine will project all contours onto the provided plane. Irrelevant contours should be filtered out
//       beforehand.
//
// Note: This routine will only produce contours of a uniform direction. (The direction depends on the provided plane.)
//       The orientation of the provided contours will be ignored.
//
// Note: Outgoing contours with holes are converted to single contours with seams. The seam location cannot (easily) be
//       specified.
//
// Note: This routine is able to treat the inputs as sets of disconnected polygons.
//
// Note: The number of contours this routine can potentially return are [0,inf] depending on the operation and inputs --
//       even when holes are converted to seams.
//
// Note: This routine performs an operation on sets of contours which are built from the provided contours. How these
//       sets are constructed can be controlled too. (By default you'll probably want to join, but through this setting
//       you can switch from a contour_collection primitive to a single contour primitive, which is especially useful
//       for XOR/symmetric_difference of the individual contours.)
//
contour_collection<double>
ContourBoolean(plane<double> p,
               std::list<std::reference_wrapper<contour_of_points<double>>> A,
               std::list<std::reference_wrapper<contour_of_points<double>>> B,
               ContourBooleanMethod op,
               ContourBooleanMethod construction_op = ContourBooleanMethod::join);


