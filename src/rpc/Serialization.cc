// Serialization.cc -- A part of DICOMautomaton 2022. Written by hal clark.

#include <optional>
#include <list>
#include <functional>
#include <iosfwd>
#include <array>
#include <optional>
#include <initializer_list>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>
#include <variant>
#include <any>
#include <cstdint>

#include "YgorMisc.h"
#include "YgorLog.h"
#include "YgorMath.h"
#include "YgorImages.h"

#include "../Structs.h"
#include "../Tables.h"

#include "../Alignment_Rigid.h"
#include "../Alignment_TPSRPM.h"
#include "../Alignment_Field.h"

#include "gen-cpp/DCMA_constants.h"
#include "gen-cpp/DCMA_types.h"
#include "gen-cpp/Receiver.h"

#include "Serialization.h"


// Misc helpers.
#define SERIALIZE_CONTAINER(x_in, x_out) {      \
            for(const auto& y : x_in){          \
                x_out.emplace_back();           \
                Serialize(y, x_out.back());     \
            }                                   \
        }

#define DESERIALIZE_CONTAINER(x_in, x_out) {    \
            for(const auto& y : x_in){          \
                x_out.emplace_back();           \
                Deserialize(y, x_out.back());   \
            }                                   \
        }

#if defined(__x86_64__) || defined(__i386__) || defined(__aarch64__)
    // I can't figure out a reasonable way to account for padding except for on these systems.
    // Confirming the classes have not been modified on at least *some* systems is probably enough.
    #define PERFORM_CLASS_LAYOUT_CHECKS 1
#endif

static void Serialize( const bool &in, bool &out ){
    out = in;
}
static void Deserialize( const bool &in, bool &out ){
    out = in;
}

static void Serialize( const std::string &in, std::string &out ){
    out = in;
}
static void Deserialize( const std::string &in, std::string &out ){
    out = in;
}

static void Serialize( const uint32_t &in, int64_t &out ){
    // Warning: conversion from uint32_t to int64_t. (Thrift does not have uint32_t.)
    out = static_cast<int64_t>(in);
}
static void Deserialize( const int64_t &in, uint32_t &out ){
    // Warning: conversion from int64_t to uint32_t. (Thrift does not have uint32_t.)
    out = static_cast<uint32_t>(in);
}

static void Serialize( const uint64_t &in, int64_t &out ){
    // Warning: conversion from uint64_t to int64_t. (Thrift does not have uint64_t.)
    out = static_cast<int64_t>(in);
}
static void Deserialize( const int64_t &in, uint64_t &out ){
    // Warning: conversion from int64_t to uint64_t. (Thrift does not have uint64_t.)
    out = static_cast<uint64_t>(in);
}

static void Serialize( const int64_t &in, int64_t &out ){
    out = in;
}
static void Deserialize( const int64_t &in, int64_t &out ){
    out = in;
}

static void Serialize( const double &in, double &out ){
    out = in;
}
static void Deserialize( const double &in, double &out ){
    out = in;
}

static void Serialize( const float &in, double &out ){
    // Warning: conversion from float to double. (Thrift does not have float.)
    out = static_cast<double>(in);
}
static void Deserialize( const double &in, float &out ){
    // Warning: conversion from double to float. (Thrift does not have float.)
    out = static_cast<float>(in);
}

// --------------------------------------------------------------------
// Ygor classes -- YgorMath.h.
// --------------------------------------------------------------------
// metadata_map_t
void Serialize( const metadata_map_t &in, dcma::rpc::metadata_t &out ){
    out = in;
}
void Deserialize( const dcma::rpc::metadata_t &in, metadata_map_t &out ){
    out = in;
}

// vec3<double>
void Serialize( const vec3<double> &in, dcma::rpc::vec3_double &out ){
#if defined(PERFORM_CLASS_LAYOUT_CHECKS)
    static_assert( (   sizeof(decltype(in.x))
                     + sizeof(decltype(in.y))
                     + sizeof(decltype(in.z)) ) == sizeof(decltype(in)),
                   "Class layout is unexpected. Were members added?" );
#endif // defined(PERFORM_CLASS_LAYOUT_CHECKS)

    Serialize(in.x, out.x);
    Serialize(in.y, out.y);
    Serialize(in.z, out.z);
}
void Deserialize( const dcma::rpc::vec3_double &in, vec3<double> &out ){
    Deserialize(in.x, out.x);
    Deserialize(in.y, out.y);
    Deserialize(in.z, out.z);
}

// contour_of_points<double>
void Serialize( const contour_of_points<double> &in, dcma::rpc::contour_of_points_double &out ){
#if defined(PERFORM_CLASS_LAYOUT_CHECKS)
    // Note: due to padding, the following may not be able to detect if the contour_of_points class has members added.
    static_assert( (   sizeof(decltype(in.points))
                     + std::max<size_t>(sizeof(decltype(in.closed)), alignof(decltype(in))) // Account for padding.
                     + sizeof(decltype(in.metadata)) ) == sizeof(decltype(in)),
                   "Class layout is unexpected. Were members added?" );
#endif // defined(PERFORM_CLASS_LAYOUT_CHECKS)

    SERIALIZE_CONTAINER(in.points, out.points);
    Serialize(in.closed, out.closed);
    Serialize(in.metadata, out.metadata);
}
void Deserialize( const dcma::rpc::contour_of_points_double &in, contour_of_points<double> &out ){
    DESERIALIZE_CONTAINER(in.points, out.points);
    Deserialize(in.closed, out.closed);
    Deserialize(in.metadata, out.metadata);
}

// contour_collection<double>
void Serialize( const contour_collection<double> &in, dcma::rpc::contour_collection_double &out ){
#if defined(PERFORM_CLASS_LAYOUT_CHECKS)
    static_assert( sizeof(decltype(in.contours)) == sizeof(decltype(in)),
                   "Class layout is unexpected. Were members added?" );
#endif // defined(PERFORM_CLASS_LAYOUT_CHECKS)

    SERIALIZE_CONTAINER(in.contours, out.contours);
}
void Deserialize( const dcma::rpc::contour_collection_double &in, contour_collection<double> &out ){
    DESERIALIZE_CONTAINER(in.contours, out.contours);
}

// point_set<double>
void Serialize( const point_set<double> &in, dcma::rpc::point_set_double &out ){
#if defined(PERFORM_CLASS_LAYOUT_CHECKS)
    static_assert( (   sizeof(decltype(in.points))
                     + sizeof(decltype(in.normals))
                     + sizeof(decltype(in.colours))
                     + sizeof(decltype(in.metadata)) ) == sizeof(decltype(in)),
                   "Class layout is unexpected. Were members added?" );
#endif // defined(PERFORM_CLASS_LAYOUT_CHECKS)

    SERIALIZE_CONTAINER(in.points, out.points);
    SERIALIZE_CONTAINER(in.normals, out.normals);
    SERIALIZE_CONTAINER(in.colours, out.colours);
    Serialize(in.metadata, out.metadata);
}
void Deserialize( const dcma::rpc::point_set_double &in, point_set<double> &out ){
    DESERIALIZE_CONTAINER(in.points, out.points);
    DESERIALIZE_CONTAINER(in.normals, out.normals);
    DESERIALIZE_CONTAINER(in.colours, out.colours);
    Deserialize(in.metadata, out.metadata);
}

// std::array<double,4>
void Serialize( const std::array<double,4> &in, dcma::rpc::sample4_double &out ){
    Serialize(in[0], out.x);
    Serialize(in[1], out.sigma_x);
    Serialize(in[2], out.f);
    Serialize(in[3], out.sigma_f);
}
void Deserialize( const dcma::rpc::sample4_double &in, std::array<double,4> &out ){
    Deserialize(in.x,       out[0]);
    Deserialize(in.sigma_x, out[1]);
    Deserialize(in.f,       out[2]);
    Deserialize(in.sigma_f, out[3]);
}

// samples_1D<double>
void Serialize( const samples_1D<double> &in, dcma::rpc::samples_1D_double &out ){
#if defined(PERFORM_CLASS_LAYOUT_CHECKS)
    // Note: due to padding, the following may not be able to detect if the contour_of_points class has members added.
    static_assert( (   sizeof(decltype(in.samples))
                     + std::max<size_t>(sizeof(decltype(in.uncertainties_known_to_be_independent_and_random)), alignof(decltype(in))) // Account for padding.
                     + sizeof(decltype(in.metadata)) ) == sizeof(decltype(in)),
                   "Class layout is unexpected. Were members added?" );
#endif // defined(PERFORM_CLASS_LAYOUT_CHECKS)

    SERIALIZE_CONTAINER(in.samples, out.samples);
    Serialize(in.uncertainties_known_to_be_independent_and_random, out.uncertainties_known_to_be_independent_and_random);
    Serialize(in.metadata, out.metadata);
}
void Deserialize( const dcma::rpc::samples_1D_double &in, samples_1D<double> &out ){
    DESERIALIZE_CONTAINER(in.samples, out.samples);
    Deserialize(in.uncertainties_known_to_be_independent_and_random, out.uncertainties_known_to_be_independent_and_random);
    Deserialize(in.metadata, out.metadata);
}

// fv_surface_mesh<double,uint64_t>
void Serialize( const fv_surface_mesh<double,uint64_t> &in, dcma::rpc::fv_surface_mesh_double_int64 &out ){
#if defined(PERFORM_CLASS_LAYOUT_CHECKS)
    static_assert( (   sizeof(decltype(in.vertices))
                     + sizeof(decltype(in.vertex_normals))
                     + sizeof(decltype(in.vertex_colours))
                     + sizeof(decltype(in.faces))
                     + sizeof(decltype(in.involved_faces))
                     + sizeof(decltype(in.metadata)) ) == sizeof(decltype(in)),
                   "Class layout is unexpected. Were members added?" );
#endif // defined(PERFORM_CLASS_LAYOUT_CHECKS)

    SERIALIZE_CONTAINER(in.vertices, out.vertices);
    SERIALIZE_CONTAINER(in.vertex_normals, out.vertex_normals);
    SERIALIZE_CONTAINER(in.vertex_colours, out.vertex_colours);
    for(const auto& f : in.faces){
        out.faces.emplace_back();
        SERIALIZE_CONTAINER(f, out.faces.back());
    }
    for(const auto& f : in.involved_faces){
        out.involved_faces.emplace_back();
        SERIALIZE_CONTAINER(f, out.involved_faces.back());
    }
    Serialize(in.metadata, out.metadata);
}
void Deserialize( const dcma::rpc::fv_surface_mesh_double_int64 &in, fv_surface_mesh<double,uint64_t> &out ){
    DESERIALIZE_CONTAINER(in.vertices, out.vertices);
    DESERIALIZE_CONTAINER(in.vertex_normals, out.vertex_normals);
    DESERIALIZE_CONTAINER(in.vertex_colours, out.vertex_colours);
    for(const auto& f : in.faces){
        out.faces.emplace_back();
        DESERIALIZE_CONTAINER(f, out.faces.back());
    }
    for(const auto& f : in.involved_faces){
        out.involved_faces.emplace_back();
        DESERIALIZE_CONTAINER(f, out.involved_faces.back());
    }
    Deserialize(in.metadata, out.metadata);
}

// --------------------------------------------------------------------
// Ygor classes -- YgorImages.h.
// --------------------------------------------------------------------
// planar_image<float,double>
void Serialize( const planar_image<float,double> &in, dcma::rpc::planar_image_double_double &out ){
#if defined(PERFORM_CLASS_LAYOUT_CHECKS)
    static_assert( (   sizeof(decltype(in.data))
                     + sizeof(decltype(in.rows))
                     + sizeof(decltype(in.columns))
                     + sizeof(decltype(in.channels))
                     + sizeof(decltype(in.pxl_dx))
                     + sizeof(decltype(in.pxl_dy))
                     + sizeof(decltype(in.pxl_dz))
                     + sizeof(decltype(in.anchor))
                     + sizeof(decltype(in.offset))
                     + sizeof(decltype(in.row_unit))
                     + sizeof(decltype(in.col_unit))
                     + sizeof(decltype(in.metadata)) ) == sizeof(decltype(in)),
                   "Class layout is unexpected. Were members added?" );
#endif // defined(PERFORM_CLASS_LAYOUT_CHECKS)

    SERIALIZE_CONTAINER(in.data, out.data);
    Serialize(in.rows, out.rows);
    Serialize(in.columns, out.columns);
    Serialize(in.channels, out.channels);
    Serialize(in.pxl_dx, out.pxl_dx);
    Serialize(in.pxl_dy, out.pxl_dy);
    Serialize(in.pxl_dz, out.pxl_dz);
    Serialize(in.anchor, out.anchor);
    Serialize(in.offset, out.offset);
    Serialize(in.row_unit, out.row_unit);
    Serialize(in.col_unit, out.col_unit);
    Serialize(in.metadata, out.metadata);
}
void Deserialize( const dcma::rpc::planar_image_double_double &in, planar_image<float,double> &out ){
    DESERIALIZE_CONTAINER(in.data, out.data);
    Deserialize(in.rows, out.rows);
    Deserialize(in.columns, out.columns);
    Deserialize(in.channels, out.channels);
    Deserialize(in.pxl_dx, out.pxl_dx);
    Deserialize(in.pxl_dy, out.pxl_dy);
    Deserialize(in.pxl_dz, out.pxl_dz);
    Deserialize(in.anchor, out.anchor);
    Deserialize(in.offset, out.offset);
    Deserialize(in.row_unit, out.row_unit);
    Deserialize(in.col_unit, out.col_unit);
    Deserialize(in.metadata, out.metadata);
}

// planar_image_collection<float,double>
void Serialize( const planar_image_collection<float,double> &in, dcma::rpc::planar_image_collection_double_double &out ){
#if defined(PERFORM_CLASS_LAYOUT_CHECKS)
    static_assert( sizeof(decltype(in.images)) == sizeof(decltype(in)),
                   "Class layout is unexpected. Were members added?" );
#endif // defined(PERFORM_CLASS_LAYOUT_CHECKS)

    SERIALIZE_CONTAINER(in.images, out.images);
}
void Deserialize( const dcma::rpc::planar_image_collection_double_double &in, planar_image_collection<float,double> &out ){
    DESERIALIZE_CONTAINER(in.images, out.images);
}

// --------------------------------------------------------------------
// DICOMautomaton classes -- Tables.h.
// --------------------------------------------------------------------
// tables::cell<std::string>
// and
// tables::table2
void Serialize( const tables::table2 &in, dcma::rpc::table2 &out ){
    // Note: cells have private rows and columns that need to be provided at the time of construction. To simplify
    // access, we serialize and deserialize them inline here.
    //
#if defined(PERFORM_CLASS_LAYOUT_CHECKS)
    // tables::cell<std::string>
    static_assert( (   sizeof(decltype(tables::cell<std::string>().get_row()))
                     + sizeof(decltype(tables::cell<std::string>().get_col()))
                     + sizeof(decltype(tables::cell<std::string>().val)) ) == sizeof(tables::cell<std::string>),
                   "Class layout is unexpected. Were members added?" );
    // tables::table2
    static_assert( (   sizeof(decltype(in.data))
                     + sizeof(decltype(in.metadata)) ) == sizeof(decltype(in)),
                   "Class layout is unexpected. Were members added?" );
#endif // defined(PERFORM_CLASS_LAYOUT_CHECKS)

    for(const auto &c : in.data){
        out.data.emplace_back();
        Serialize(c.get_row(), out.data.back().row);
        Serialize(c.get_col(), out.data.back().col);
        Serialize(c.val,       out.data.back().val);
    }
    Serialize(in.metadata, out.metadata);
}
void Deserialize( const dcma::rpc::table2 &in, tables::table2 &out ){
    for(const auto &c : in.data){
        int64_t l_row;
        int64_t l_col;
        std::string l_val;

        Deserialize( c.row, l_row );
        Deserialize( c.col, l_col );
        Deserialize( c.val, l_val );

        out.inject( l_row, l_col, l_val );
    }
    Deserialize(in.metadata, out.metadata);
}

// --------------------------------------------------------------------
// DICOMautomaton classes -- Structs.h.
// --------------------------------------------------------------------
// Contour_Data
void Serialize( const Contour_Data &in, dcma::rpc::Contour_Data &out ){
#if defined(PERFORM_CLASS_LAYOUT_CHECKS)
    static_assert( sizeof(decltype(in.ccs)) == sizeof(decltype(in)),
                   "Class layout is unexpected. Were members added?" );
#endif // defined(PERFORM_CLASS_LAYOUT_CHECKS)

    SERIALIZE_CONTAINER(in.ccs, out.ccs);
}
void Deserialize( const dcma::rpc::Contour_Data &in, Contour_Data &out ){
    DESERIALIZE_CONTAINER(in.ccs, out.ccs);
}

// Image_Array
void Serialize( const Image_Array &in, dcma::rpc::Image_Array &out ){
#if defined(PERFORM_CLASS_LAYOUT_CHECKS)
    static_assert( (   sizeof(decltype(in.imagecoll))
                     + sizeof(decltype(in.filename)) ) == sizeof(decltype(in)),
                   "Class layout is unexpected. Were members added?" );
#endif // defined(PERFORM_CLASS_LAYOUT_CHECKS)

    Serialize(in.imagecoll, out.imagecoll);
    Serialize(in.filename, out.filename);
}
void Deserialize( const dcma::rpc::Image_Array &in, Image_Array &out ){
    Deserialize(in.imagecoll, out.imagecoll);
    Deserialize(in.filename, out.filename);
}

// Point_Cloud
void Serialize( const Point_Cloud &in, dcma::rpc::Point_Cloud &out ){
#if defined(PERFORM_CLASS_LAYOUT_CHECKS)
    static_assert( sizeof(decltype(in.pset)) == sizeof(decltype(in)),
                   "Class layout is unexpected. Were members added?" );
#endif // defined(PERFORM_CLASS_LAYOUT_CHECKS)

    Serialize(in.pset, out.pset);
}
void Deserialize( const dcma::rpc::Point_Cloud &in, Point_Cloud &out ){
    Deserialize(in.pset, out.pset);
}

// Surface_Mesh
void Serialize( const Surface_Mesh &in, dcma::rpc::Surface_Mesh &out ){
#if defined(PERFORM_CLASS_LAYOUT_CHECKS)
    static_assert( (   sizeof(decltype(in.meshes))
                     + sizeof(decltype(in.vertex_attributes))
                     + sizeof(decltype(in.face_attributes)) ) == sizeof(decltype(in)),
                   "Class layout is unexpected. Were members added?" );
#endif // defined(PERFORM_CLASS_LAYOUT_CHECKS)

    Serialize(in.meshes, out.meshes);

    // TODO.
    if(!in.vertex_attributes.empty()){
        YLOGWARN("Attempting to serialize mesh with vertex attributes; attributes will be omitted");
    }
    if(!in.face_attributes.empty()){
        YLOGWARN("Attempting to serialize mesh with face attributes; attributes will be omitted");
    }
}
void Deserialize( const dcma::rpc::Surface_Mesh &in, Surface_Mesh &out ){
    Deserialize(in.meshes, out.meshes);
}

// Static_Machine_State
void Serialize( const Static_Machine_State &in, dcma::rpc::Static_Machine_State &out ){
#if defined(PERFORM_CLASS_LAYOUT_CHECKS)
    static_assert( (   sizeof(decltype(in.CumulativeMetersetWeight))
                     + sizeof(decltype(in.ControlPointIndex))
                     + sizeof(decltype(in.GantryAngle))
                     + sizeof(decltype(in.GantryRotationDirection))
                     + sizeof(decltype(in.BeamLimitingDeviceAngle))
                     + sizeof(decltype(in.BeamLimitingDeviceRotationDirection))
                     + sizeof(decltype(in.PatientSupportAngle))
                     + sizeof(decltype(in.PatientSupportRotationDirection))
                     + sizeof(decltype(in.TableTopEccentricAngle))
                     + sizeof(decltype(in.TableTopEccentricRotationDirection))
                     + sizeof(decltype(in.TableTopVerticalPosition))
                     + sizeof(decltype(in.TableTopLongitudinalPosition))
                     + sizeof(decltype(in.TableTopLateralPosition))
                     + sizeof(decltype(in.TableTopPitchAngle))
                     + sizeof(decltype(in.TableTopPitchRotationDirection))
                     + sizeof(decltype(in.TableTopRollAngle))
                     + sizeof(decltype(in.TableTopRollRotationDirection))
                     + sizeof(decltype(in.IsocentrePosition))
                     + sizeof(decltype(in.JawPositionsX))
                     + sizeof(decltype(in.JawPositionsY))
                     + sizeof(decltype(in.MLCPositionsX))
                     + sizeof(decltype(in.metadata)) ) == sizeof(decltype(in)),
                   "Class layout is unexpected. Were members added?" );
#endif // defined(PERFORM_CLASS_LAYOUT_CHECKS)

    Serialize(in.CumulativeMetersetWeight, out.CumulativeMetersetWeight);
    Serialize(in.ControlPointIndex, out.ControlPointIndex);
    Serialize(in.GantryAngle, out.GantryAngle);
    Serialize(in.GantryRotationDirection, out.GantryRotationDirection);
    Serialize(in.BeamLimitingDeviceAngle, out.BeamLimitingDeviceAngle);
    Serialize(in.BeamLimitingDeviceRotationDirection, out.BeamLimitingDeviceRotationDirection);
    Serialize(in.PatientSupportAngle, out.PatientSupportAngle);
    Serialize(in.PatientSupportRotationDirection, out.PatientSupportRotationDirection);
    Serialize(in.TableTopEccentricAngle, out.TableTopEccentricAngle);
    Serialize(in.TableTopEccentricRotationDirection, out.TableTopEccentricRotationDirection);
    Serialize(in.TableTopVerticalPosition, out.TableTopVerticalPosition);
    Serialize(in.TableTopLongitudinalPosition, out.TableTopLongitudinalPosition);
    Serialize(in.TableTopLateralPosition, out.TableTopLateralPosition);
    Serialize(in.TableTopPitchAngle, out.TableTopPitchAngle);
    Serialize(in.TableTopPitchRotationDirection, out.TableTopPitchRotationDirection);
    Serialize(in.TableTopRollAngle, out.TableTopRollAngle);
    Serialize(in.TableTopRollRotationDirection, out.TableTopRollRotationDirection);
    Serialize(in.IsocentrePosition, out.IsocentrePosition);
    SERIALIZE_CONTAINER(in.JawPositionsX, out.JawPositionsX);
    SERIALIZE_CONTAINER(in.JawPositionsY, out.JawPositionsY);
    SERIALIZE_CONTAINER(in.MLCPositionsX, out.MLCPositionsX);
    Serialize(in.metadata, out.metadata);
}
void Deserialize( const dcma::rpc::Static_Machine_State &in, Static_Machine_State &out ){
    Deserialize(in.CumulativeMetersetWeight, out.CumulativeMetersetWeight);
    Deserialize(in.ControlPointIndex, out.ControlPointIndex);
    Deserialize(in.GantryAngle, out.GantryAngle);
    Deserialize(in.GantryRotationDirection, out.GantryRotationDirection);
    Deserialize(in.BeamLimitingDeviceAngle, out.BeamLimitingDeviceAngle);
    Deserialize(in.BeamLimitingDeviceRotationDirection, out.BeamLimitingDeviceRotationDirection);
    Deserialize(in.PatientSupportAngle, out.PatientSupportAngle);
    Deserialize(in.PatientSupportRotationDirection, out.PatientSupportRotationDirection);
    Deserialize(in.TableTopEccentricAngle, out.TableTopEccentricAngle);
    Deserialize(in.TableTopEccentricRotationDirection, out.TableTopEccentricRotationDirection);
    Deserialize(in.TableTopVerticalPosition, out.TableTopVerticalPosition);
    Deserialize(in.TableTopLongitudinalPosition, out.TableTopLongitudinalPosition);
    Deserialize(in.TableTopLateralPosition, out.TableTopLateralPosition);
    Deserialize(in.TableTopPitchAngle, out.TableTopPitchAngle);
    Deserialize(in.TableTopPitchRotationDirection, out.TableTopPitchRotationDirection);
    Deserialize(in.TableTopRollAngle, out.TableTopRollAngle);
    Deserialize(in.TableTopRollRotationDirection, out.TableTopRollRotationDirection);
    Deserialize(in.IsocentrePosition, out.IsocentrePosition);
    DESERIALIZE_CONTAINER(in.JawPositionsX, out.JawPositionsX);
    DESERIALIZE_CONTAINER(in.JawPositionsY, out.JawPositionsY);
    DESERIALIZE_CONTAINER(in.MLCPositionsX, out.MLCPositionsX);
    Deserialize(in.metadata, out.metadata);
}

// Dynamic_Machine_State
void Serialize( const Dynamic_Machine_State &in, dcma::rpc::Dynamic_Machine_State &out ){
#if defined(PERFORM_CLASS_LAYOUT_CHECKS)
    static_assert( (   sizeof(decltype(in.BeamNumber))
                     + sizeof(decltype(in.FinalCumulativeMetersetWeight))
                     + sizeof(decltype(in.static_states))
                     + sizeof(decltype(in.metadata)) ) == sizeof(decltype(in)),
                   "Class layout is unexpected. Were members added?" );
#endif // defined(PERFORM_CLASS_LAYOUT_CHECKS)

    Serialize(in.BeamNumber, out.BeamNumber);
    Serialize(in.FinalCumulativeMetersetWeight, out.FinalCumulativeMetersetWeight);
    SERIALIZE_CONTAINER(in.static_states, out.static_states);
    Serialize(in.metadata, out.metadata);
}
void Deserialize( const dcma::rpc::Dynamic_Machine_State &in, Dynamic_Machine_State &out ){
    Deserialize(in.BeamNumber, out.BeamNumber);
    Deserialize(in.FinalCumulativeMetersetWeight, out.FinalCumulativeMetersetWeight);
    DESERIALIZE_CONTAINER(in.static_states, out.static_states);
    Deserialize(in.metadata, out.metadata);
}

// RTPlan
void Serialize( const RTPlan &in, dcma::rpc::RTPlan &out ){
#if defined(PERFORM_CLASS_LAYOUT_CHECKS)
    static_assert( (   sizeof(decltype(in.dynamic_states))
                     + sizeof(decltype(in.metadata)) ) == sizeof(decltype(in)),
                   "Class layout is unexpected. Were members added?" );
#endif // defined(PERFORM_CLASS_LAYOUT_CHECKS)

    SERIALIZE_CONTAINER(in.dynamic_states, out.dynamic_states);
    Serialize(in.metadata, out.metadata);
}
void Deserialize( const dcma::rpc::RTPlan &in, RTPlan &out ){
    DESERIALIZE_CONTAINER(in.dynamic_states, out.dynamic_states);
    Deserialize(in.metadata, out.metadata);
}

// Line_Sample
void Serialize( const Line_Sample &in, dcma::rpc::Line_Sample &out ){
#if defined(PERFORM_CLASS_LAYOUT_CHECKS)
    static_assert( sizeof(decltype(in.line)) == sizeof(decltype(in)),
                   "Class layout is unexpected. Were members added?" );
#endif // defined(PERFORM_CLASS_LAYOUT_CHECKS)

    Serialize(in.line, out.line);
}
void Deserialize( const dcma::rpc::Line_Sample &in, Line_Sample &out ){
    Deserialize(in.line, out.line);
}

// Transform3
void Serialize( const Transform3 &in, dcma::rpc::Transform3 &out ){
    // TODO.
    throw std::runtime_error("Transform data is not yet supported. Refusing to continue");
}
void Deserialize( const dcma::rpc::Transform3 &in, Transform3 &out ){
    // TODO.
    throw std::runtime_error("Transform data is not yet supported. Refusing to continue");
}

// Sparse_Table
void Serialize( const Sparse_Table &in, dcma::rpc::Sparse_Table &out ){
#if defined(PERFORM_CLASS_LAYOUT_CHECKS)
    static_assert( sizeof(decltype(in.table)) == sizeof(decltype(in)),
                   "Class layout is unexpected. Were members added?" );
#endif // defined(PERFORM_CLASS_LAYOUT_CHECKS)

    Serialize(in.table, out.table);
}
void Deserialize( const dcma::rpc::Sparse_Table &in, Sparse_Table &out ){
    Deserialize(in.table, out.table);
}

// Drover
void Serialize( const Drover &in, dcma::rpc::Drover &out ){
#if defined(PERFORM_CLASS_LAYOUT_CHECKS)
    static_assert( (   sizeof(decltype(in.contour_data))
                     + sizeof(decltype(in.image_data))
                     + sizeof(decltype(in.point_data))
                     + sizeof(decltype(in.smesh_data))
                     + sizeof(decltype(in.rtplan_data))
                     + sizeof(decltype(in.lsamp_data))
                     + sizeof(decltype(in.trans_data))
                     + sizeof(decltype(in.table_data)) ) == sizeof(decltype(in)),
                   "Class layout is unexpected. Were members added?" );
#endif // defined(PERFORM_CLASS_LAYOUT_CHECKS)

    // For a pointer contour_data member.
    if(in.Has_Contour_Data() && !in.contour_data->ccs.empty()){
        out.__set_contour_data( {} ); // Field is optional, so ensure it is marked as set.
        out.contour_data.emplace_back();
        Serialize(*(in.contour_data), out.contour_data.back());
    }
    //
    // For a list-of-pointers contour_data member.
    //if(in.Has_Contour_Data() && !in.contour_data->ccs.empty()){
    //    std::vector<dcma::rpc::Contour_Data> rpc_cd;
    //    for(const auto& cd_ptr : in.contour_data){
    //        rpc_cd.emplace_back();
    //        Serialize(*cd_ptr, rpc_cd.back());
    //    }
    //    out.__set_contour_data( rpc_cd ); // Field is optional, so ensure it is marked as set.
    //}

    if(in.Has_Image_Data()){
        std::vector<dcma::rpc::Image_Array> shtl;
        for(const auto& ptr : in.image_data){
            shtl.emplace_back();
            Serialize(*ptr, shtl.back());
        }
        out.__set_image_data( shtl ); // Field is optional, so ensure it is marked as set.
    }

    if(in.Has_Point_Data()){
        std::vector<dcma::rpc::Point_Cloud> shtl;
        for(const auto& ptr : in.point_data){
            shtl.emplace_back();
            Serialize(*ptr, shtl.back());
        }
        out.__set_point_data( shtl ); // Field is optional, so ensure it is marked as set.
    }

    if(in.Has_Mesh_Data()){
        std::vector<dcma::rpc::Surface_Mesh> shtl;
        for(const auto& ptr : in.smesh_data){
            shtl.emplace_back();
            Serialize(*ptr, shtl.back());
        }
        out.__set_smesh_data( shtl ); // Field is optional, so ensure it is marked as set.
    }

    if(in.Has_RTPlan_Data()){
        std::vector<dcma::rpc::RTPlan> shtl;
        for(const auto& ptr : in.rtplan_data){
            shtl.emplace_back();
            Serialize(*ptr, shtl.back());
        }
        out.__set_rtplan_data( shtl ); // Field is optional, so ensure it is marked as set.
    }

    if(in.Has_LSamp_Data()){
        std::vector<dcma::rpc::Line_Sample> shtl;
        for(const auto& ptr : in.lsamp_data){
            shtl.emplace_back();
            Serialize(*ptr, shtl.back());
        }
        out.__set_lsamp_data( shtl ); // Field is optional, so ensure it is marked as set.
    }

    if(in.Has_Tran3_Data()){
        throw std::runtime_error("Transform data is not yet supported. Refusing to continue");

        std::vector<dcma::rpc::Transform3> shtl;
        for(const auto& ptr : in.trans_data){
            shtl.emplace_back();
            Serialize(*ptr, shtl.back());
        }
        out.__set_trans_data( shtl ); // Field is optional, so ensure it is marked as set.
    }

    if(in.Has_Table_Data()){
        std::vector<dcma::rpc::Sparse_Table> shtl;
        for(const auto& ptr : in.table_data){
            shtl.emplace_back();
            Serialize(*ptr, shtl.back());
        }
        out.__set_table_data( shtl ); // Field is optional, so ensure it is marked as set.
    }
}
void Deserialize( const dcma::rpc::Drover &in, Drover &out ){
    if(in.__isset.contour_data){
        // Currently, the Drover class allows a single pointer Contour_Data item, so we have to pack all inner contours
        // into the same object.
        out.Ensure_Contour_Data_Allocated();
        for(const auto &rpc_cd : in.contour_data){
            Contour_Data shtl;
            Deserialize(rpc_cd, shtl);
            out.contour_data->ccs.splice( std::end(out.contour_data->ccs), shtl.ccs);
        }
    }

    if(in.__isset.image_data){
        for(const auto& x : in.image_data){
            auto ptr = std::make_shared<Image_Array>();
            Deserialize(x, *ptr);
            out.image_data.emplace_back(ptr);
        }
    }

    if(in.__isset.point_data){
        for(const auto& x : in.point_data){
            auto ptr = std::make_shared<Point_Cloud>();
            Deserialize(x, *ptr);
            out.point_data.emplace_back(ptr);
        }
    }

    if(in.__isset.smesh_data){
        for(const auto& x : in.smesh_data){
            auto ptr = std::make_shared<Surface_Mesh>();
            Deserialize(x, *ptr);
            out.smesh_data.emplace_back(ptr);
        }
    }

    if(in.__isset.rtplan_data){
        for(const auto& x : in.rtplan_data){
            auto ptr = std::make_shared<RTPlan>();
            Deserialize(x, *ptr);
            out.rtplan_data.emplace_back(ptr);
        }
    }

    if(in.__isset.lsamp_data){
        for(const auto& x : in.lsamp_data){
            auto ptr = std::make_shared<Line_Sample>();
            Deserialize(x, *ptr);
            out.lsamp_data.emplace_back(ptr);
        }
    }

    if(in.__isset.trans_data){
        throw std::runtime_error("Transform data is not yet supported. Refusing to continue");

        for(const auto& x : in.trans_data){
            auto ptr = std::make_shared<Transform3>();
            Deserialize(x, *ptr);
            out.trans_data.emplace_back(ptr);
        }
    }

    if(in.__isset.table_data){
        for(const auto& x : in.table_data){
            auto ptr = std::make_shared<Sparse_Table>();
            Deserialize(x, *ptr);
            out.table_data.emplace_back(ptr);
        }
    }
}

#if defined(PERFORM_CLASS_LAYOUT_CHECKS)
    #undef PERFORM_CLASS_LAYOUT_CHECKS
#endif

#undef SERIALIZE_CONTAINER

#undef DESERIALIZE_CONTAINER

