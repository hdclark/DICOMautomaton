//GIS.h -- geographic information systems routines.

#pragma once

#include <utility>


namespace gis {

// Perform a Mercator projection, from latitude and longitude in degrees to 2D coordinates in metres.
//
// - latitude ranges from (-90, 90) where 0 defines the equator.
// - longitude ranges from (-180, 180) where 0 intersects the prime meridian, close to Greenwich UK.
// - the resulting coordinates are in units of metres, and have a coordinate origin where the equator and
//   prime meridian intersect.
// - the resulting coordinate 'x' is a 'horizontal' axis.
//
std::pair<double,double>
project_mercator( double latitude_degrees,
                  double longitude_degrees );

// Perform an inverse Mercator projection. The resulting (latitude, longitude) are in degrees.
std::pair<double,double>
project_inverse_mercator( double x,
                          double y );

} // namespace gis

