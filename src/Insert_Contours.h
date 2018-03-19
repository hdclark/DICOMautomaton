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
                               line<double> aline, // The line to plot.
                               contour_collection<double> &dest, // Where to put the contours.
                               std::map<std::string, std::string> metadata );
