//Insert_Contours.h.

#pragma once

#include <limits>
#include <list>
#include <map>
#include <string>

#include "YgorMath.h"
#include "YgorMisc.h"

template <class T, class R> class planar_image;
template <class T> class contour_collection;
template <class T> class line;


//Injects contours that mimic the provided line.
void Inject_Thin_Line_Contour( const planar_image<float,double> &animg,
                               line<double> aline, // The line to insert.
                               contour_collection<double> &dest, // Where to put the contours.
                               std::map<std::string, std::string> metadata );

//Injects contours that mimic the provided point.
//
// This routine approximates a circle centred on the point. The number of vertices can be
// specified, so triangles, squares, heptagons, pentagons, hexagons, etc. can be created.
// 
// Note: If radius is not finite, a default relative to the image features is used instead.
// 
// Note: >=3 vertices must be used in this routine.
//
void Inject_Point_Contour( const planar_image<float,double> &animg,
                           vec3<double> apoint,
                           contour_collection<double> &dest, // Where to put the contours.
                           std::map<std::string, std::string> metadata,
                           double radius = std::numeric_limits<double>::quiet_NaN(),
                           long int num_verts = 5);
