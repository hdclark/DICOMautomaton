//YgorMathQuaternions.cc - A part of DICOMautomaton 2026. Written by hal clark.

#include <algorithm>
#include <cmath>
#include <limits>

#include "YgorMath.h"

#include "YgorMathQuaternions.h"

quaternion::quaternion(double w_, double x_, double y_, double z_)
    : w(w_)
    , x(x_)
    , y(y_)
    , z(z_){
}

quaternion
quaternion::identity(){
    return quaternion(1.0, 0.0, 0.0, 0.0);
}

quaternion
quaternion::from_axis_angle(const vec3<double> &axis, double angle_rad){
    const auto axis_n = axis.unit();
    const auto half_angle = 0.5 * angle_rad;
    const auto s = std::sin(half_angle);
    return quaternion(std::cos(half_angle),
                      axis_n.x * s,
                      axis_n.y * s,
                      axis_n.z * s).normalized();
}

quaternion
quaternion::from_two_unit_vectors(const vec3<double> &from, const vec3<double> &to){
    const auto pi = std::acos(-1.0);
    auto from_u = from.unit();
    auto to_u = to.unit();

    const auto cos_theta = std::clamp(from_u.Dot(to_u), -1.0, 1.0);
    if(cos_theta < (-1.0 + 1.0E-9)){
        auto ortho = vec3<double>(1.0, 0.0, 0.0).Cross(from_u);
        if(ortho.Dot(ortho) < 1.0E-18){
            ortho = vec3<double>(0.0, 1.0, 0.0).Cross(from_u);
        }
        return from_axis_angle(ortho.unit(), pi);
    }

    const auto axis = from_u.Cross(to_u);
    return quaternion(1.0 + cos_theta, axis.x, axis.y, axis.z).normalized();
}

quaternion
quaternion::from_euler_ypr(double yaw_rad, double pitch_rad, double roll_rad){
    const auto q_pitch = from_axis_angle(vec3<double>(1.0, 0.0, 0.0), pitch_rad);
    // Roll uses a negated sign so quaternion<->Euler conversion matches existing SDL_Viewer roll direction.
    const auto q_roll = from_axis_angle(vec3<double>(0.0, 0.0, 1.0), -roll_rad);
    const auto q_yaw = from_axis_angle(vec3<double>(0.0, 1.0, 0.0), yaw_rad);
    return (q_yaw * q_roll * q_pitch).normalized();
}

quaternion
quaternion::conjugate() const {
    return quaternion(w, -x, -y, -z);
}

double
quaternion::norm() const {
    return std::sqrt(w*w + x*x + y*y + z*z);
}

quaternion
quaternion::normalized() const {
    quaternion out(*this);
    out.normalize();
    return out;
}

void
quaternion::normalize(){
    const auto n = norm();
    if(n <= std::numeric_limits<double>::epsilon()){
        w = 1.0;
        x = y = z = 0.0;
        return;
    }
    w /= n;
    x /= n;
    y /= n;
    z /= n;
}

quaternion
quaternion::operator*(const quaternion &rhs) const {
    return quaternion( w*rhs.w - x*rhs.x - y*rhs.y - z*rhs.z,
                       w*rhs.x + x*rhs.w + y*rhs.z - z*rhs.y,
                       w*rhs.y - x*rhs.z + y*rhs.w + z*rhs.x,
                       w*rhs.z + x*rhs.y - y*rhs.x + z*rhs.w );
}

vec3<double>
quaternion::rotate(const vec3<double> &v) const {
    const auto qn = normalized();
    const quaternion p(0.0, v.x, v.y, v.z);
    const auto qr = qn * p * qn.conjugate();
    return vec3<double>(qr.x, qr.y, qr.z);
}

void
quaternion::to_euler_ypr(double &yaw_rad, double &pitch_rad, double &roll_rad) const {
    const auto qn = normalized();

    // Pitch around X.
    const auto sin_pitch = std::clamp(2.0 * (qn.w*qn.x - qn.y*qn.z), -1.0, 1.0);
    pitch_rad = std::asin(sin_pitch);

    // Yaw around Y.
    const auto sin_yaw = 2.0 * (qn.w*qn.y + qn.x*qn.z);
    const auto cos_yaw = 1.0 - 2.0 * (qn.x*qn.x + qn.y*qn.y);
    yaw_rad = std::atan2(sin_yaw, cos_yaw);

    // Roll around Z (stored with opposite sign to keep viewer conventions).
    const auto sin_roll = 2.0 * (qn.w*qn.z + qn.x*qn.y);
    const auto cos_roll = 1.0 - 2.0 * (qn.x*qn.x + qn.z*qn.z);
    roll_rad = -std::atan2(sin_roll, cos_roll);
}
