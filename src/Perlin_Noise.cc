//Perlin_Noise.cc - A part of DICOMautomaton 2026. Written by hal clark.
//
// This file contains the implementation for a Perlin noise generator using Ken Perlin's "improved noise" algorithm from
// 2002:
// - instead of using 3*t*t - 2*t*t*t as the fade function, it uses a higher-order polynomial for the fade:
//   6*t*t*t*t*t - 15*t*t*t*t + 10*t*t*t.
// - it uses a fixed set of 12 gradient vectors to avoid directional bias.

#include <vector>
#include <numeric>
#include <algorithm>
#include <random>
#include <cmath>

#include "YgorMath.h"

#include "Perlin_Noise.h"

// Helpers.
double PerlinNoise::fade(double t) const {
    return t * t * t * (t * (t * 6 - 15) + 10);
}

double PerlinNoise::lerp(double t, double a, double b) const {
    return a + t * (b - a);
}

double PerlinNoise::grad(int hash, double x, double y, double z) const {
    // Convert LO 4 bits of hash code into 12 gradient directions
    int h = hash & 15;
    double u = h < 8 ? x : y;
    double v = h < 4 ? y : h == 12 || h == 14 ? x : z;
    return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
}


// Constructor.
PerlinNoise::PerlinNoise(int64_t seed){
    p.resize(256);
    std::iota(p.begin(), p.end(), 0);
    
    std::default_random_engine engine(seed);
    std::shuffle(p.begin(), p.end(), engine);

    // Duplicate the permutation table to avoid modulo operations
    p.insert(p.end(), p.begin(), p.end());
}

// Main noise function: returns values in range [-1.0, 1.0]
double PerlinNoise::sample( vec3<double> pos,
                            double scale,
                            vec3<double> offset ) const {
    double x = pos.x * scale + offset.x;
    double y = pos.y * scale + offset.y;
    double z = pos.z * scale + offset.z;

    // Find unit cube that contains the position
    int X = static_cast<int>(std::floor(x)) & 255;
    int Y = static_cast<int>(std::floor(y)) & 255;
    int Z = static_cast<int>(std::floor(z)) & 255;

    // Find relative x, y, z of position in cube
    x -= std::floor(x);
    y -= std::floor(y);
    z -= std::floor(z);

    // Compute fade curves for each of x, y, z
    double u = fade(x);
    double v = fade(y);
    double w = fade(z);

    // Hash coordinates of the 8 cube corners
    int A = p[X] + Y;
    int AA = p[A] + Z;
    int AB = p[A + 1] + Z;
    int B = p[X + 1] + Y;
    int BA = p[B] + Z;
    int BB = p[B + 1] + Z;

    // Add blended results from 8 corners of cube
    return lerp(w, lerp(v, lerp(u, grad(p[AA], x, y, z),
                                   grad(p[BA], x - 1, y, z)),
                           lerp(u, grad(p[AB], x, y - 1, z),
                                   grad(p[BB], x - 1, y - 1, z))),
                   lerp(v, lerp(u, grad(p[AA + 1], x, y, z - 1),
                                   grad(p[BA + 1], x - 1, y, z - 1)),
                           lerp(u, grad(p[AB + 1], x, y - 1, z - 1),
                                   grad(p[BB + 1], x - 1, y - 1, z - 1))));
}


