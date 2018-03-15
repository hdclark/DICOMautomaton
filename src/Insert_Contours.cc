//Insert_Contours.cc - A part of DICOMautomaton 2017. Written by hal clark.

#include <initializer_list>
#include <limits>
#include <list>

#include "YgorMisc.h"
#include "YgorMath.h"
#include "YgorImages.h"

#include "Insert_Contours.h"


//Injects contours that mimic the provided line.
void Inject_Thin_Line_Contour( const planar_image<float,double> &animg,
                               line<double> aline, // The line to plot.
                               contour_collection<double> &dest, // Where to put the contours.
                               std::map<std::string, std::string> metadata ){

    // Enclose the image with a sphere (i.e., a convenient geometrical shape for computing intersections).
    const auto img_C = animg.center();
    const auto img_r = 0.5 * std::hypot( animg.pxl_dx * animg.rows, animg.pxl_dy * animg.columns );
    sphere<double> S(img_C, img_r);

    // Find the intersection points of the sphere and line (if any).
    const auto Is = S.Line_Intersections(aline);
    if(Is.size() != 2) throw std::domain_error("Cannot approximate line with contour: line and image not coincident.");
    const auto I0 = Is.front();
    const auto I1 = Is.back();

    // Ensure they're within the image bounds. (If both are, assume the whole line is; if neither forgo the line?).
    // Note: Contours on purely 2D images are permitted.
    if( (animg.pxl_dz > std::numeric_limits<double>::min())
    &&  (   !animg.sandwiches_point_within_top_bottom_planes(I0)
         || !animg.sandwiches_point_within_top_bottom_planes(I1) ) ){
        throw std::domain_error("Cannot approximate line with contour: line-image intersections out-of-plane.");
    }

    // Project the intersection points onto the (central) plane of the image.
    const auto img_plane = animg.image_plane();
    const auto IO0 = img_plane.Project_Onto_Plane_Orthogonally(I0);
    const auto IO1 = img_plane.Project_Onto_Plane_Orthogonally(I1);

    // Find the in-plane direction orthogonal to the line direction.
    const auto img_ortho = animg.row_unit.Cross(animg.col_unit).unit();
    const auto perp = img_ortho.Cross( aline.U_0 ).unit();

    // Split the intersection points by a small amount, to give the line a small width.
    const auto perp_p = perp * (1E-4) * std::min(animg.pxl_dx, animg.pxl_dy);
    const auto IO0p = IO0 + perp_p;
    const auto IO0m = IO0 - perp_p;
    const auto IO1p = IO1 + perp_p;
    const auto IO1m = IO1 - perp_p;

    // Add the vertices and metadata to a new contour.
    dest.contours.emplace_back();
    dest.contours.back().closed = true;
    dest.contours.back().points.emplace_back( IO0p );
    dest.contours.back().points.emplace_back( IO0m );
    dest.contours.back().points.emplace_back( IO1p );
    dest.contours.back().points.emplace_back( IO1m );
    dest.contours.back().metadata = metadata;

    return;
};

