//Contour_Boolean_Operations.cc - A part of DICOMautomaton 2017. Written by hal clark.

// These functions are used to perform first-order Boolean operations on (2D) polygon contours.

#include <list>
#include <functional>
#include <limits>
#include <map>
#include <cmath>
#include <experimental/any>

#include "YgorMisc.h"
#include "YgorMath.h"

#include "Contour_Boolean_Operations.h"


//typedef enum { 
//    join,                 // A ∪ B (or A + B; aka, the "union").
//    intersection,         // A ∩ B (aka, the "overlap").
//    difference,           // A - B (aka, all of A except the part shared by B).
//    symmetric_difference  // A x B = (A-B) ∪ (B-A) (aka, all of A except the part shared by B, and also
//                          //                        all of B except the part shared by A.)
//} ContourBooleanMethod;


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
contour_collection<double>
ContourBoolean(ContourBooleanMethod op,
               plane<double> p,
               std::list<std::reference_wrapper<contour_of_points<double>>> A,
               std::list<std::reference_wrapper<contour_of_points<double>>> B){

    // Identify an orthonormal set that spans the 2D plane. Store them for later projection.
    const auto U_z = p.N_0.unit();
    vec3<double> U_y = vec3<double>(1.0, 0.0, 0.0); //Candidate vector.
    if(U_y.Dot(U_z) > 0.25){
        U_y = U_z.rotate_around_x(M_PI * 0.5);
    }
    vec3<double> U_x = U_z.Cross(U_y);
    if(!U_z.GramSchmidt_orthogonalize(U_y, U_x)){
        throw std::runtime_error("Unable to find planar basis vectors.");
    }
    U_x = U_x.unit();  // U_x and U_y now form an in-plane basis.
    U_y = U_y.unit();


    // Extract the common metadata from all contours in both A and B sets. Store it for later.
    std::list<std::reference_wrapper<contour_of_points<double>>> all;
    all.insert(all.end(), A.begin(), A.end());
    all.insert(all.end(), B.begin(), B.end());
    auto common_metadata = contour_collection().get_common_metadata( { }, { std::ref(all) } );


    // Convert the A set of contours into CGAL style by projecting onto the plane and expressing in the new basis.
    // Remember that [0,inf] contours may be selected. Purge the original contours.
    // Remember to ensure clockwise orientation!


    // Convert the B set of contours into CGAL style by projecting onto the plane and expressing in the new basis.
    // Remember that [0,inf] contours may be selected. Purge the original contours.
    // Remember to ensure clockwise orientation!


    // Perform the selected Boolean operation.


    // If necessary, remove polygon holes by 'seaming' the contours.


    // Convert each contour back to the DICOMautomaton coordinate system using the orthonormal basis.
    // Attach the metadata. Insert it into the Contour_Data.



}               


