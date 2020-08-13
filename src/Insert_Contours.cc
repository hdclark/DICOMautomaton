//Insert_Contours.cc - A part of DICOMautomaton 2017. Written by hal clark.

#include <algorithm>
#include <cmath>
#include <limits>
#include <list>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

#include "Insert_Contours.h"
#include "YgorImages.h"
#include "YgorMath.h"



//Injects contours that mimic the image plane intersection with the provided plane.
void Inject_Thin_Plane_Contour( const planar_image<float,double> &animg,
                                const plane<double>& aplane, // The line to insert.
                                contour_collection<double> &dest, // Where to put the contours.
                                std::map<std::string, std::string> metadata,
                                double c_thickness ){ // The contour thickness.
    // Determine the plane-plane intersection, if there is one, and insert contours IFF there is a line intersection.
    const auto img_plane = animg.image_plane();
    line<double> int_line; // The intersection line, if one exists.
    if(img_plane.Intersects_With_Plane_Along_Line(aplane, int_line)){
        Inject_Thin_Line_Contour( animg, int_line, dest, std::move(metadata), c_thickness );
    }
    return;
}


//Injects contours that mimic the provided line projected onto the image plane.
void Inject_Thin_Line_Contour( const planar_image<float,double> &animg,
                               line<double> aline, // The line to insert.
                               contour_collection<double> &dest, // Where to put the contours.
                               const std::map<std::string, std::string>& metadata,
                               double c_thickness ){ // The contour thickness.

    if(!std::isfinite(c_thickness)){
        c_thickness = (1E-4) * std::min(animg.pxl_dx, animg.pxl_dy); // Small relative to the image features.
    }

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
    const auto perp_p = perp * c_thickness * 0.5;
    const auto IO0p = IO0 + perp_p;
    const auto IO0m = IO0 - perp_p;
    const auto IO1p = IO1 + perp_p;
    const auto IO1m = IO1 - perp_p;

    // Add the vertices and metadata to a new contour.
    contour_of_points<double> c;
    c.closed = true;
    c.points.emplace_back( IO0m );
    c.points.emplace_back( IO0p );
    c.points.emplace_back( IO1p );
    c.points.emplace_back( IO1m );

    // Trim the contour to the image bounding volume.
    contour_collection<double> cc({c});
    auto clipped = animg.clip_to_volume(cc);

    for(auto &c : clipped.contours){
        if(c.points.size() >= 3){
            dest.contours.emplace_back(c);
            dest.contours.back().Reorient_Counter_Clockwise();
            dest.contours.back().closed = true;
            dest.contours.back().metadata = metadata;
        }
    }

    return;
}

//Injects contours that mimic the provided point projected onto the image plane.
void Inject_Point_Contour( const planar_image<float,double> &animg,
                           const vec3<double>& apoint, // The point to insert.
                           contour_collection<double> &dest, // Where to put the contours.
                           const std::map<std::string, std::string>& metadata,
                           double radius,
                           long int num_verts){

    // Project the points onto the (central) plane of the image.
    const auto img_plane = animg.image_plane();
    const auto proj = img_plane.Project_Onto_Plane_Orthogonally(apoint);

    const auto row_unit = animg.row_unit;
    const auto col_unit = animg.col_unit;

    // Split the intersection points by a small amount, to give the line a small width.
    if(!std::isfinite(radius)){
        radius = std::max(animg.pxl_dx, animg.pxl_dy); // Something reasonable relative to the image features.
    }
    if(num_verts < 3) throw std::logic_error("This routine requires >=3 vertices for approximations."); 

    // Add the vertices and metadata to a new contour.
    const auto pi = std::acos(-1.0);
    contour_of_points<double> c;
    c.closed = true;
    for(long int n = 0; n < num_verts; ++n){
        const auto angle = 2.0 * pi * static_cast<double>(n) / static_cast<double>(num_verts);
        c.points.emplace_back( proj + row_unit * std::cos(angle) * radius
                                    + col_unit * std::sin(angle) * radius );
    }

    // Trim the contours to the (rectangular) bounding box.
    contour_collection<double> cc({c});
    auto clipped = animg.clip_to_volume(cc);

    for(auto &c : clipped.contours){
        if(c.points.size() >= 3){
            dest.contours.emplace_back(c);
            dest.contours.back().Reorient_Counter_Clockwise();
            dest.contours.back().closed = true;
            dest.contours.back().metadata = metadata;
        }
    }

    return;
}
