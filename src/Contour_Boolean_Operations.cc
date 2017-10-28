//Contour_Boolean_Operations.cc - A part of DICOMautomaton 2017. Written by hal clark.

// These functions are used to perform first-order Boolean operations on (2D) polygon contours.

#include <list>
#include <functional>
#include <limits>
#include <map>
#include <cmath>
#include <experimental/any>

//#include <CGAL/Exact_predicates_exact_constructions_kernel.h>
#include <CGAL/Cartesian.h>
#include <CGAL/Polygon_2.h>
#include <CGAL/Polygon_with_holes_2.h>
#include <CGAL/Polygon_set_2.h>
#include <CGAL/connect_holes.h>

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

    // Closure for expressing vectors in the plane's basis.
    const auto R3_v_to_R2_P_basis = [=](vec3<double> R) -> vec3<double> {
        // 'R' is a vector from the R^3 origin to a contour vertex.
        // We want to express 'R' explicitly in terms of the R^2 plane's basis.
        // (Our convention in this representation will be to use a vec3 but enforce z=0 everywhere.)

        //Project onto the plane.
        const auto proj = p.Project_Onto_Plane_Orthogonally(R);

        //Now express the projected point in terms of the plane's basis.
        const auto dR = (proj - p.R_0);  // in-plane vector from plane's pinning vector.

        return vec3<double>(dR.Dot(U_x), dR.Dot(U_y), 0.0);
    };

    // Closure for converting from the plane's basis back to R^3 representation.
    const auto R2_P_basis_to_R3_v = [=](vec3<double> R) -> vec3<double> {
        // 'R' is a vector from the plane's origin (and R_0) to a contour vertex.
        // We want to express 'R' explicitly in terms of the R^3 coordinate system the plane is described in.
        // Note that we cannot un-project the vertices off the plane, so we assume they were already exactly coincident
        // with the plane.

        vec3<double> actual = p.R_0 + (U_x * R.x) + (U_y * R.y);
        return actual;
    };

    // Extract the common metadata from all contours in both A and B sets. Store it for later.
    std::list<std::reference_wrapper<contour_of_points<double>>> all;
    all.insert(all.end(), A.begin(), A.end());
    all.insert(all.end(), B.begin(), B.end());
    auto common_metadata = contour_collection<double>().get_common_metadata( { }, { std::ref(all) } );


    typedef CGAL::Simple_cartesian<double>     Kernel;
    typedef Kernel::Point_2                    Point_2;
    typedef CGAL::Polygon_2<Kernel>            Polygon_2;
    typedef CGAL::Polygon_with_holes_2<Kernel> Polygon_with_holes_2;
    typedef CGAL::Polygon_set_2<Kernel>        Polygon_set_2;

    // Convert the A set of contours into CGAL style by projecting onto the plane and expressing in the new basis.
    Polygon_set_2 A_set;
    for(auto &c_ref : A){
        //Express in the planar basis (with z'=0 everywhere).
        contour_of_points<double> projected;
        projected.closed = true;
        for(auto &v : c_ref.get().points){
            projected.points.emplace_back(R3_v_to_R2_P_basis(v));
        }

        //Ensure that the contour is counter-clockwise (as per the CGAL requirement for outer-boundary polygons).
        if(!projected.Is_Counter_Clockwise()) projected.Reorient_Counter_Clockwise();

        //Insert in the CGAL polygon set.
        Polygon_2 A_cgal;
        for(auto &v : projected.points){
            A_cgal.push_back(Point_2(v.x,v.y));
        }
        A_set.join(A_cgal);
    }

    // Convert the B set of contours into CGAL style by projecting onto the plane and expressing in the new basis.
    Polygon_set_2 B_set;
    for(auto &c_ref : B){
        //Express in the planar basis (with z'=0 everywhere).
        contour_of_points<double> projected;
        projected.closed = true;
        for(auto &v : c_ref.get().points){
            projected.points.emplace_back(R3_v_to_R2_P_basis(v));
        }

        //Ensure that the contour is counter-clockwise (as per the CGAL requirement for outer-boundary polygons).
        if(!projected.Is_Counter_Clockwise()) projected.Reorient_Counter_Clockwise();

        //Insert in the CGAL polygon set.
        Polygon_2 B_cgal;
        for(auto &v : projected.points){
            B_cgal.push_back(Point_2(v.x,v.y));
        }
        B_set.join(B_cgal);
    }


    // Perform the selected Boolean operation.
    //
    // Note: For some reason the sets must be specified in reverse order (so A-B requires B.difference(A)).
    //       It only matters for the difference operation. All others are symmetric.
    Polygon_set_2 C_set;
    C_set.join(A_set);
    if(false){
    }else if(op == ContourBooleanMethod::join){
        C_set.join(B_set);
    }else if(op == ContourBooleanMethod::intersection){
        C_set.intersection(B_set);
    }else if(op == ContourBooleanMethod::difference){
        C_set.difference(B_set);
    }else if(op == ContourBooleanMethod::symmetric_difference){
        C_set.symmetric_difference(B_set);
    }else{
        throw std::logic_error("Requested Boolean operation is not supported.");
    }

    // Convert each contour back to the DICOMautomaton coordinate system using the orthonormal basis.
    contour_collection<double> out;

    if(C_set.number_of_polygons_with_holes() != 0){
        std::list<Polygon_with_holes_2> pwhl;
        C_set.polygons_with_holes(std::back_inserter(pwhl));

        for(auto &pwh : pwhl){
            //If necessary, remove polygon holes by 'seaming' the contours.
            // Otherwise there are no holes to seam.
            //
            // Note: The following connect_holes routine fails with CGAL 4.10-1 (Arch Linux)
            //       when using CGAL::Exact_predicates_inexact_constructions_kernel. Beware if you switch kernels.
            std::list<Point_2> p2l;
            connect_holes(pwh,std::back_inserter(p2l));

            if(p2l.empty()) continue;
            out.contours.emplace_back();
            for(auto &p2 : p2l){
                const vec3<double> proj(p2.x(), p2.y(), 0.0);
                const auto v = R2_P_basis_to_R3_v(proj);
                out.contours.back().points.emplace_back(v);
            }
            //The outer boundary of all CGAL contours with holes are oriented clockwise.
            // Flip them around as per normal positive orientation in DICOMautomaton.
            out.contours.back().points.reverse();

            //Attach the common metadata.
            out.contours.back().closed = true;
            out.contours.back().metadata = common_metadata;
        }
    }

    return out;
}               


