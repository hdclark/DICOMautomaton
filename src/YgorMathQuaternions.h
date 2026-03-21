//YgorMathQuaternions.h.

#pragma once

#include <cmath>

#include "YgorMath.h"

class quaternion {
public:
    double w = 1.0;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    quaternion() = default;
    quaternion(double w_, double x_, double y_, double z_);

    static quaternion identity();
    static quaternion from_axis_angle(const vec3<double> &axis, double angle_rad);
    static quaternion from_two_unit_vectors(const vec3<double> &from, const vec3<double> &to);
    static quaternion from_euler_ypr(double yaw_rad, double pitch_rad, double roll_rad);

    quaternion conjugate() const;
    double norm() const;
    quaternion normalized() const;
    void normalize();

    quaternion operator*(const quaternion &rhs) const;
    vec3<double> rotate(const vec3<double> &v) const;

    void to_euler_ypr(double &yaw_rad, double &pitch_rad, double &roll_rad) const;
};

