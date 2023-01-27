//Insert_Contours.h.

#pragma once

#include <limits>
#include <list>
#include <map>
#include <string>

#include "YgorMath.h"
#include "YgorMisc.h"
#include "YgorLog.h"

template <class T, class R> class planar_image;
template <class T> class contour_collection;
template <class T> class plane;
template <class T> class line;


//Injects contours that mimic the provided plane (only) where it intersects the image plane.
// Contour thickness can be controlled.
//
// Note: If the contour thickness is not finite, a default relative to the image features is used instead.
//       The units are DICOM coordinate system units.
// 
// Note: This routine can handle large contour thicknesses, which generate rectangular contours.
//
void Inject_Thin_Plane_Contour( const planar_image<float,double> &animg,
                                const plane<double>& aplane, // The plane to mimic.
                                contour_collection<double> &dest, // Where to put the contours.
                                std::map<std::string, std::string> metadata,
                                double c_thickness = std::numeric_limits<double>::quiet_NaN() );

//Injects contours that mimic the provided line. Contour thickness can be controlled.
//
// Note: If the contour thickness is not finite, a default relative to the image features is used instead.
//       The units are DICOM coordinate system units.
// 
// Note: This routine can handle large contour thicknesses, which generate rectangular contours.
//
void Inject_Thin_Line_Contour( const planar_image<float,double> &animg,
                               line<double> aline, // The line to insert.
                               contour_collection<double> &dest, // Where to put the contours.
                               const std::map<std::string, std::string>& metadata,
                               double c_thickness = std::numeric_limits<double>::quiet_NaN() );

//Injects contours that mimic the provided point.
//
// This routine approximates a circle centred on the point. The number of vertices can be
// specified, so triangles, squares, heptagons, pentagons, hexagons, etc. can be created.
// 
// Note: If radius is not finite, a default relative to the image features is used instead.
//       The units are DICOM coordinate system units.
// 
// Note: >=3 vertices must be used in this routine.
//
void Inject_Point_Contour( const planar_image<float,double> &animg,
                           const vec3<double>& apoint,
                           contour_collection<double> &dest, // Where to put the contours.
                           const std::map<std::string, std::string>& metadata,
                           double radius = std::numeric_limits<double>::quiet_NaN(),
                           long int num_verts = 5);
