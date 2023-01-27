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

/*
struct point_set_double {
    1: required list<vec3_double> points;
    2: required list<vec3_double> normals;
    3: required list<i64> colours; // NOTE: should be uint32 with 8-bit packed RGBA.
    4: required metadata_t metadata;
}
struct sample4_double { // NOTE: wrapper for std::array<double,4>.
    1: required double x;
    2: required double sigma_x;
    3: required double f;
    4: required double sigma_f;
}
struct samples_1D_double {
    1: required list<sample4_double> samples;
    2: required bool uncertainties_known_to_be_independent_and_random;
    3: required metadata_t metadata;
}
struct fv_surface_mesh_double_int64 { // NOTE: uint64_t not supported, so using int64_t.
    1: required list<vec3_double> vertices;
    2: required list<vec3_double> vertex_normals;
    3: required list<i64> vertex_colours; // NOTE: should be uint32 with 8-bit packed RGBA.
    4: required list<list<i64>> faces; // NOTE: should be uint64_t rather than int64_t.
    5: required list<list<i64>> involved_faces; // NOTE: should be uint64_t rather than int64_t.
    6: required metadata_t metadata;
}
*/

// --------------------------------------------------------------------
// Ygor classes -- YgorMath.h.
// --------------------------------------------------------------------
void Serialize( const metadata_map_t &in, dcma::rpc::metadata_t &out ){
    out = in;
}
void Deserialize( const dcma::rpc::metadata_t &in, metadata_map_t &out ){
    out = in;
}

void Serialize( const vec3<double> &in, dcma::rpc::vec3_double &out ){
    out.x = in.x;
    out.y = in.y;
    out.z = in.z;
}
void Deserialize( const dcma::rpc::vec3_double &in, vec3<double> &out ){
    out.x = in.x;
    out.y = in.y;
    out.z = in.z;
}

void Serialize( const contour_of_points<double> &in, dcma::rpc::contour_of_points_double &out ){
    for(const auto& p : in.points){
        out.points.emplace_back();
        Serialize(p, out.points.back());
    }
    out.closed = in.closed;
    Serialize(in.metadata, out.metadata);
}
void Deserialize( const dcma::rpc::contour_of_points_double &in, contour_of_points<double> &out ){
    for(const auto& p : in.points){
        out.points.emplace_back();
        Deserialize(p, out.points.back());
    }
    out.closed = in.closed;
    Deserialize(in.metadata, out.metadata);
}

void Serialize( const contour_collection<double> &in, dcma::rpc::contour_collection_double &out ){
    for(const auto& c : in.contours){
        out.contours.emplace_back();
        Serialize(c, out.contours.back());
    }
}
void Deserialize( const dcma::rpc::contour_collection_double &in, contour_collection<double> &out ){
    for(const auto& c : in.contours){
        out.contours.emplace_back();
        Deserialize(c, out.contours.back());
    }
}

//void Serialize( const point_set<double> &in, dcma::rpc::point_set_double &out );
//void Deserialize( const dcma::rpc::point_set_double &in, point_set<double> &out );
//
//void Serialize( const std::array<double,4> &in, dcma::rpc::sample4_double &out );
//void Deserialize( const dcma::rpc::sample4_double &in, std::array<double,4> &out );
//
//void Serialize( const samples_1D<double> &in, dcma::rpc::samples_1D_double &out );
//void Deserialize( const dcma::rpc::samples_1D_double &in, samples_1D<double> &out );
//
//void Serialize( const fv_surface_mesh<double,uint64_t> &in, dcma::rpc::fv_surface_mesh_double_int64 &out );
//void Deserialize( const dcma::rpc::fv_surface_mesh_double_int64 &in, fv_surface_mesh<double,uint64_t> &out );
//
//// --------------------------------------------------------------------
//// Ygor classes -- YgorImages.h.
//// --------------------------------------------------------------------
//void Serialize( const planar_image<float,double> &in, dcma::rpc::planar_image_double_double &out );
//void Deserialize( const dcma::rpc::planar_image_double_double &in, planar_image<float,double> &out );
//
//void Serialize( const planar_image_collection<float,double> &in, dcma::rpc::planar_image_collection_double_double &out );
//void Deserialize( const dcma::rpc::planar_image_collection_double_double &in, planar_image_collection<float,double> &out );
//
//// --------------------------------------------------------------------
//// DICOMautomaton classes -- Tables.h.
//// --------------------------------------------------------------------
//void Serialize( const tables::cell<std::string> &in, dcma::rpc::cell_string &out );
//void Deserialize( const dcma::rpc::cell_string &in, tables::cell<std::string> &out );
//
//void Serialize( const tables::table2 &in, dcma::rpc::table2 &out );
//void Deserialize( const dcma::rpc::table2 &in, tables::table2 &out );

// --------------------------------------------------------------------
// DICOMautomaton classes -- Structs.h.
// --------------------------------------------------------------------
void Serialize( const Contour_Data &in, dcma::rpc::Contour_Data &out ){
    for(const auto& cc : in.ccs){
        out.ccs.emplace_back();
        Serialize(cc, out.ccs.back());
    }
}
void Deserialize( const dcma::rpc::Contour_Data &in, Contour_Data &out ){
    for(const auto& cc : in.ccs){
        out.ccs.emplace_back();
        Deserialize(cc, out.ccs.back());
    }
}

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

void Serialize( const Drover &in, dcma::rpc::Drover &out ){
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


