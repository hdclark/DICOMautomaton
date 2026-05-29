//GIS.h -- geographic information systems routines.

#pragma once

#include <utility>
#include <cstdint>


namespace dcma {
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


// Given latitude and longitude in degrees, and zoom level (exponent of two),
// compute the x and y tile coordinates that cover the given location.
std::pair<int64_t, int64_t>
project_web_mercator( double latitude_degrees,
                      double longitude_degrees,
                      int64_t zoom_exponent );


// Perform an inverse web Mercator projection. Note that this projection is not exactly invertible.
// The resulting (latitude, longitude) are in degrees.
std::pair<double,double>
project_inverse_web_mercator( int64_t tile_x,
                              int64_t tile_y,
                              int64_t zoom_exponent );

} // namespace gis
} // namespace dcma

