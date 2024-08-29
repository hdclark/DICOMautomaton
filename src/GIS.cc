
#include <limits>
#include <utility>
#include <cstdint>
#include <cmath>
#include <stdexcept>

#include "GIS.h"

//#ifndef DCMA_GIS_DISABLE_ALL_SPECIALIZATIONS
//    #define DCMA_GIS_DISABLE_ALL_SPECIALIZATIONS
//#endif
static const double earth_R = 6'378'137.0; // WGS 184 semimajor axis, represents radius of Earth in metres.

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
                  double longitude_degrees ){

    // Wrap domain.
    longitude_degrees = std::fmod(longitude_degrees + 180.0, 360.0) - 180.0;
    latitude_degrees = std::fmod(latitude_degrees + 90.0, 180.0) - 90.0;

//    // Check domain.
//    if( (latitude_degrees <= -90.0)
//    ||  (90.0 <= latitude_degrees) ){
//        throw std::invalid_argument("Latitude is not valid should fall within (-90,90) degrees");
//    }
//    if( (longitude_degrees <= -180.0)
//    ||  (180.0 <= longitude_degrees) ){
//        throw std::invalid_argument("Longitude is not valid should fall within (-180,180) degrees");
//    }

    // Convert from degrees to radians.
    const double pi = std::acos(-1.0);
    const double t = latitude_degrees  * (pi / 180.0);
    const double l = longitude_degrees * (pi / 180.0);

    // Get coordinates in metres.
    const double x = earth_R * l;
    const double y = -earth_R * std::log( std::tan( (pi * 0.25) + (t * 0.5) ) );

    return { x, y };
}

// Perform an inverse Mercator projection. The resulting (latitude, longitude) are in degrees.
std::pair<double,double>
project_inverse_mercator( double x,
                          double y ){

    // Get coordinates in radians.
    const double pi = std::acos(-1.0);
    const double l = x / earth_R;
    const double t = 2.0*(std::atan(std::exp(-y/earth_R)) - 0.25*pi);

    // Convert from radians to degrees.
    const double latitude_degrees  = t * (180.0 / pi);
    const double longitude_degrees = l * (180.0 / pi);

    return { latitude_degrees, longitude_degrees };
}


// Given latitude and longitude in degrees, and zoom level (exponent of two),
// compute the x and y tile coordinates that cover the given location.
std::pair<int64_t, int64_t>
project_web_mercator( double latitude_degrees,
                      double longitude_degrees,
                      int64_t zoom_exponent ){

    const auto longitude_ratio = (longitude_degrees + 180.0) / 360.0;
    const auto zoom = std::pow(2.0, zoom_exponent);

    const auto tile_x = static_cast<int64_t>(std::floor(longitude_ratio * zoom));

    const double pi = std::acos(-1.0);
    const double lat_rad = latitude_degrees * (pi / 180.0);
    const auto tile_y = static_cast<int64_t>(std::floor(
        0.5 * std::pow(2.0, zoom_exponent) * (1.0 - (std::asinh(std::tan(lat_rad)) / pi))
    ));

    return { tile_x, tile_y };
}

// Perform an inverse web Mercator projection. Note that this projection is not exactly invertible.
// The resulting (latitude, longitude) are in degrees.
std::pair<double,double>
project_inverse_web_mercator( int64_t tile_x,
                              int64_t tile_y,
                              int64_t zoom_exponent ){

    const double pi = std::acos(-1.0);
    const auto zoom = std::pow(2.0, zoom_exponent);

    const auto lon = tile_x * (360.0 / zoom) - 180.0;
    const auto lat = std::atan(std::sinh(pi - (tile_y * 2.0 * pi / zoom))) * (180.0 / pi);

    return { lat, lon };
}

} // namespace gis
} // namespace dcma

