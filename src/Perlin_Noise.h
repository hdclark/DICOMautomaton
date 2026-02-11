//Perlin_Noise.h.

#pragma once

#include <vector>
#include <numeric>
#include <algorithm>
#include <random>
#include <cmath>
#include <cstdint>

#include "YgorMath.h"


class PerlinNoise {
  private:
    std::vector<int> p;

    // Internal helpers.
    double fade(double t) const;
    double lerp(double t, double a, double b) const;
    double grad(int hash, double x, double y, double z) const;

  public:
    // Initialize with a seed to generate a unique permutation table
    explicit PerlinNoise(int64_t seed = std::random_device{}());

    // Main noise function: returns values in range [-1.0, 1.0]
    double sample(vec3<double> point,
                  double scale = 1.0,
                  vec3<double> offset = vec3<double>(0,0,0) ) const;

};

