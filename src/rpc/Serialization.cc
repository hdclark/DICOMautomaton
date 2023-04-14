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

static void Serialize( const int32_t &in, int64_t &out ){
    // Warning: conversion from int32_t to int64_t which in needed when long int = int32_t.
    out = static_cast<int64_t>(in);
}
static void Deserialize( const int64_t &in, int32_t &out ){
    // Warning: conversion from int64_t to int32_t which in needed when long int = int32_t.
    out = static_cast<int32_t>(in);
}

static void SerializeLI( const long int &in, int64_t &out ){
    // Warning: conversion from long int (i.e., int32_t or int64_t) to int64_t.
    out = static_cast<int64_t>(in);
}
static void DeserializeLI( const int64_t &in, long int &out ){
    // Warning: conversion from int64_t to long int (i.e., int32_t or int64_t).
    out = static_cast<long int>(in);
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
    static_assert( (   sizeof(decltype(in.x))  
                     + sizeof(decltype(in.y)) 
                     + sizeof(decltype(in.z)) ) == sizeof(decltype(in)),
                   "Class layout is unexpected. Were members added?" );
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
    // Note: due to padding, the following may not be able to detect if the contour_of_points class has members added.
    static_assert( (   sizeof(decltype(in.points))  
                     + std::max(sizeof(decltype(in.closed)), alignof(decltype(in))) // Account for padding.
                     + sizeof(decltype(in.metadata)) ) == sizeof(decltype(in)),
                   "Class layout is unexpected. Were members added?" );
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
    static_assert( sizeof(decltype(in.contours)) == sizeof(decltype(in)),
                   "Class layout is unexpected. Were members added?" );
    SERIALIZE_CONTAINER(in.contours, out.contours);
}
void Deserialize( const dcma::rpc::contour_collection_double &in, contour_collection<double> &out ){
    DESERIALIZE_CONTAINER(in.contours, out.contours);
}

// point_set<double>
void Serialize( const point_set<double> &in, dcma::rpc::point_set_double &out ){
    static_assert( (   sizeof(decltype(in.points))  
                     + sizeof(decltype(in.normals)) 
                     + sizeof(decltype(in.colours)) 
                     + sizeof(decltype(in.metadata)) ) == sizeof(decltype(in)),
                   "Class layout is unexpected. Were members added?" );
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
    // Note: due to padding, the following may not be able to detect if the contour_of_points class has members added.
    static_assert( (   sizeof(decltype(in.samples))  
                     + std::max(sizeof(decltype(in.uncertainties_known_to_be_independent_and_random)), alignof(decltype(in))) // Account for padding.
                     + sizeof(decltype(in.metadata)) ) == sizeof(decltype(in)),
                   "Class layout is unexpected. Were members added?" );
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
    static_assert( (   sizeof(decltype(in.vertices))  
                     + sizeof(decltype(in.vertex_normals))
                     + sizeof(decltype(in.vertex_colours))
                     + sizeof(decltype(in.faces))
                     + sizeof(decltype(in.involved_faces))
                     + sizeof(decltype(in.metadata)) ) == sizeof(decltype(in)),
                   "Class layout is unexpected. Were members added?" );

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
    static_assert( (   sizeof(decltype(in.data))  
                     // Handle platform differences with long int = int64_t or int32_t.
                     // Members rows, columns, and channels will either be 8 bytes each and packed sequentially into
                     // 24 bytes, or will be 4 bytes each and packed into 16 bytes with 4 bytes of padding.
                     + ((sizeof(long int) == 8) ?   sizeof(decltype(in.rows))
                                                  + sizeof(decltype(in.columns))
                                                  + sizeof(decltype(in.channels))
                                                :   sizeof(decltype(in.rows))
                                                  + sizeof(decltype(in.columns))
                                                  + sizeof(decltype(in.channels)) * 2)
                     + sizeof(decltype(in.pxl_dx))
                     + sizeof(decltype(in.pxl_dy))
                     + sizeof(decltype(in.pxl_dz))
                     + sizeof(decltype(in.anchor))
                     + sizeof(decltype(in.offset))
                     + sizeof(decltype(in.row_unit))
                     + sizeof(decltype(in.col_unit))
                     + sizeof(decltype(in.metadata)) ) == sizeof(decltype(in)),
                   "Class layout is unexpected. Were members added?" );
    SERIALIZE_CONTAINER(in.data, out.data);
    SerializeLI(in.rows, out.rows);
    SerializeLI(in.columns, out.columns);
    SerializeLI(in.channels, out.channels);
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
    DeserializeLI(in.rows, out.rows);
    DeserializeLI(in.columns, out.columns);
    DeserializeLI(in.channels, out.channels);
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
    static_assert( sizeof(decltype(in.images)) == sizeof(decltype(in)),
                   "Class layout is unexpected. Were members added?" );
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
    // tables::cell<std::string>
    static_assert( (   sizeof(decltype(tables::cell<std::string>().get_row()))
                     + sizeof(decltype(tables::cell<std::string>().get_col()))
                     + sizeof(decltype(tables::cell<std::string>().val)) ) == sizeof(tables::cell<std::string>),
                   "Class layout is unexpected. Were members added?" );
    // tables::table2
    static_assert( (   sizeof(decltype(in.data))  
                     + sizeof(decltype(in.metadata)) ) == sizeof(decltype(in)),
                   "Class layout is unexpected. Were members added?" );

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
    static_assert( sizeof(decltype(in.ccs)) == sizeof(decltype(in)),
                   "Class layout is unexpected. Were members added?" );
    SERIALIZE_CONTAINER(in.ccs, out.ccs);
}
void Deserialize( const dcma::rpc::Contour_Data &in, Contour_Data &out ){
    DESERIALIZE_CONTAINER(in.ccs, out.ccs);
}

// Image_Array
// Point_Cloud
// Surface_Mesh
// Static_Machine_State
// Dynamic_Machine_State
// RTPlan
// Transform3
// Sparse_Table

//void Serialize( const Image_Array &in, dcma::rpc::Image_Array &out );
//void Deserialize( const dcma::rpc::Image_Array &in, Image_Array &out );
//
//void Serialize( const Point_Cloud &in, dcma::rpc::Point_Cloud &out );
//void Deserialize( const dcma::rpc::Point_Cloud &in, Point_Cloud &out );
//
//void Serialize( const Surface_Mesh &in, dcma::rpc::Surface_Mesh &out );
//void Deserialize( const dcma::rpc::Surface_Mesh &in, Surface_Mesh &out );
//
//void Serialize( const Static_Machine_State &in, dcma::rpc::Static_Machine_State &out );
//void Deserialize( const dcma::rpc::Static_Machine_State &in, Static_Machine_State &out );
//
//void Serialize( const Dynamic_Machine_State &in, dcma::rpc::Dynamic_Machine_State &out );
//void Deserialize( const dcma::rpc::Dynamic_Machine_State &in, Dynamic_Machine_State &out );
//
//void Serialize( const RTPlan &in, dcma::rpc::RTPlan &out );
//void Deserialize( const dcma::rpc::RTPlan &in, RTPlan &out );
//
//void Serialize( const Line_Sample &in, dcma::rpc::Line_Sample &out );
//void Deserialize( const dcma::rpc::Line_Sample &in, Line_Sample &out );
//
//void Serialize( const Transform3 &in, dcma::rpc::Transform3 &out );
//void Deserialize( const dcma::rpc::Transform3 &in, Transform3 &out );
//
//void Serialize( const Sparse_Table &in, dcma::rpc::Sparse_Table &out );
//void Deserialize( const dcma::rpc::Sparse_Table &in, Sparse_Table &out );

// Drover
void Serialize( const Drover &in, dcma::rpc::Drover &out ){
    static_assert( (   sizeof(decltype(in.contour_data))  
                     + sizeof(decltype(in.image_data)) 
                     + sizeof(decltype(in.point_data)) 
                     + sizeof(decltype(in.smesh_data)) 
                     + sizeof(decltype(in.rtplan_data)) 
                     + sizeof(decltype(in.lsamp_data)) 
                     + sizeof(decltype(in.trans_data)) 
                     + sizeof(decltype(in.table_data)) ) == sizeof(decltype(in)),
                   "Class layout is unexpected. Were members added?" );
    // For a pointer contour_data member.
    if(in.Has_Contour_Data() && !in.contour_data->ccs.empty()){
        out.__set_contour_data( {} ); // Field is optional, so ensure it is marked as set.
        out.contour_data.emplace_back();
        Serialize(*(in.contour_data), out.contour_data.back());
    }
    
    // For a list-of-pointers contour_data member.
    //if(in.Has_Contour_Data() && !in.contour_data->ccs.empty()){
    //    std::vector<dcma::rpc::Contour_Data> rpc_cd;
    //    for(const auto& cd_ptr : in.contour_data){
    //        rpc_cd.emplace_back();
    //        Serialize(*cd_ptr, rpc_cd.back());
    //    }
    //    out.__set_contour_data( rpc_cd ); // Field is optional, so ensure it is marked as set.
    //}

//struct Drover {
//    1: optional list<Contour_Data> contour_data;
//    2: optional list<Image_Array>  image_data;
//    3: optional list<Point_Cloud>  point_data;
//    4: optional list<Surface_Mesh> smesh_data;
//    5: optional list<RTPlan>       rtplan_data;
//    6: optional list<Line_Sample>  lsamp_data;
//    7: optional list<Transform3>   trans_data;
//    8: optional list<Sparse_Table> table_data;
//}

//    Note: for Drover, or any other optional fields.
//    
//        Set via:
//        out.__set_x(in.x);
//        out.__set_y(in.y);
//        out.__set_z(in.z);
//    
//        Check if set via:
//        if(in.__isset.x) out.x = in.x;
}
void Deserialize( const dcma::rpc::Drover &in, Drover &out ){
    // NOTE: the Drover class contains many optional fields. Each needs to be confirmed to be available before
    // deserializing.
    //
    // For a pointer contour_data member.
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
    //if(in.Has_Contour_Data() && !in.contour_data->ccs.empty()){
    //    out.__set_contour_data( {} ); // Field is optional, so ensure it is marked as set.
    //    out.contour_data.emplace_back();
    //    Serialize(*(in.contour_data), out.contour_data.back());
    //}
}


#undef SERIALIZE_CONTAINER

#undef DESERIALIZE_CONTAINER

