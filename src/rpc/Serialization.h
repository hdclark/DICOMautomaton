#pragma once

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
#include "../Metadata.h"

#include "../Alignment_Rigid.h"
#include "../Alignment_TPSRPM.h"
#include "../Alignment_Field.h"

#include "gen-cpp/DCMA_constants.h"
#include "gen-cpp/DCMA_types.h"
#include "gen-cpp/Receiver.h"

#include "Serialization.h"

// Helper functions.
void Serialize( const bool &in, bool &out );
void Deserialize( const bool &in, bool &out );

void Serialize( const std::string &in, std::string &out );
void Deserialize( const std::string &in, std::string &out );

void Serialize( const uint32_t &in, int64_t &out );
void Deserialize( const int64_t &in, uint32_t &out );

void Serialize( const uint64_t &in, int64_t &out );
void Deserialize( const int64_t &in, uint64_t &out );

void Serialize( const int64_t &in, int64_t &out );
void Deserialize( const int64_t &in, int64_t &out );

void Serialize( const double &in, double &out );
void Deserialize( const double &in, double &out );

void Serialize( const float &in, double &out );
void Deserialize( const double &in, float &out );

// --------------------------------------------------------------------
// Ygor classes -- YgorMath.h.
// --------------------------------------------------------------------
void Serialize( const metadata_map_t &in, dcma::rpc::metadata_t &out );
void Deserialize( const dcma::rpc::metadata_t &in, metadata_map_t &out );

void Serialize( const vec3<double> &in, dcma::rpc::vec3_double &out ); 
void Deserialize( const dcma::rpc::vec3_double &in, vec3<double> &out ); 

void Serialize( const contour_of_points<double> &in, dcma::rpc::contour_of_points_double &out );
void Deserialize( const dcma::rpc::contour_of_points_double &in, contour_of_points<double> &out );

void Serialize( const contour_of_points<double> &in, dcma::rpc::contour_of_points_double &out );
void Deserialize( const dcma::rpc::contour_of_points_double &in, contour_of_points<double> &out );

void Serialize( const contour_collection<double> &in, dcma::rpc::contour_collection_double &out );
void Deserialize( const dcma::rpc::contour_collection_double &in, contour_collection<double> &out );

void Serialize( const point_set<double> &in, dcma::rpc::point_set_double &out );
void Deserialize( const dcma::rpc::point_set_double &in, point_set<double> &out );

void Serialize( const std::array<double,4> &in, dcma::rpc::sample4_double &out );
void Deserialize( const dcma::rpc::sample4_double &in, std::array<double,4> &out );

void Serialize( const samples_1D<double> &in, dcma::rpc::samples_1D_double &out );
void Deserialize( const dcma::rpc::samples_1D_double &in, samples_1D<double> &out );

void Serialize( const fv_surface_mesh<double,uint64_t> &in, dcma::rpc::fv_surface_mesh_double_int64 &out );
void Deserialize( const dcma::rpc::fv_surface_mesh_double_int64 &in, fv_surface_mesh<double,uint64_t> &out );

// --------------------------------------------------------------------
// Ygor classes -- YgorImages.h.
// --------------------------------------------------------------------
void Serialize( const planar_image<float,double> &in, dcma::rpc::planar_image_double_double &out );
void Deserialize( const dcma::rpc::planar_image_double_double &in, planar_image<float,double> &out );

void Serialize( const planar_image_collection<float,double> &in, dcma::rpc::planar_image_collection_double_double &out );
void Deserialize( const dcma::rpc::planar_image_collection_double_double &in, planar_image_collection<float,double> &out );

// --------------------------------------------------------------------
// DICOMautomaton classes -- Tables.h.
// --------------------------------------------------------------------
void Serialize( const tables::table2 &in, dcma::rpc::table2 &out );
void Deserialize( const dcma::rpc::table2 &in, tables::table2 &out );

// --------------------------------------------------------------------
// DICOMautomaton classes -- Structs.h.
// --------------------------------------------------------------------
void Serialize( const Contour_Data &in, dcma::rpc::Contour_Data &out );
void Deserialize( const dcma::rpc::Contour_Data &in, Contour_Data &out );

void Serialize( const Image_Array &in, dcma::rpc::Image_Array &out );
void Deserialize( const dcma::rpc::Image_Array &in, Image_Array &out );

void Serialize( const Point_Cloud &in, dcma::rpc::Point_Cloud &out );
void Deserialize( const dcma::rpc::Point_Cloud &in, Point_Cloud &out );

void Serialize( const Surface_Mesh &in, dcma::rpc::Surface_Mesh &out );
void Deserialize( const dcma::rpc::Surface_Mesh &in, Surface_Mesh &out );

void Serialize( const Static_Machine_State &in, dcma::rpc::Static_Machine_State &out );
void Deserialize( const dcma::rpc::Static_Machine_State &in, Static_Machine_State &out );

void Serialize( const Dynamic_Machine_State &in, dcma::rpc::Dynamic_Machine_State &out );
void Deserialize( const dcma::rpc::Dynamic_Machine_State &in, Dynamic_Machine_State &out );

void Serialize( const RTPlan &in, dcma::rpc::RTPlan &out );
void Deserialize( const dcma::rpc::RTPlan &in, RTPlan &out );

void Serialize( const Line_Sample &in, dcma::rpc::Line_Sample &out );
void Deserialize( const dcma::rpc::Line_Sample &in, Line_Sample &out );

void Serialize( const Transform3 &in, dcma::rpc::Transform3 &out );
void Deserialize( const dcma::rpc::Transform3 &in, Transform3 &out );

void Serialize( const Sparse_Table &in, dcma::rpc::Sparse_Table &out );
void Deserialize( const dcma::rpc::Sparse_Table &in, Sparse_Table &out );

void Serialize( const Drover &in, dcma::rpc::Drover &out ); 
void Deserialize( const dcma::rpc::Drover &in, Drover &out ); 

// --------------------------------------------------------------------
// DICOMautomaton RPC methods.
// --------------------------------------------------------------------
//void Serialize( const OperationsQuery &in, dcma::rpc::OperationsQuery &out );
//void Deserialize( const dcma::rpc::OperationsQuery &in, OperationsQuery &out );
//
//void Serialize( const KnownOperation &in, dcma::rpc::KnownOperation &out );
//void Deserialize( const dcma::rpc::KnownOperation &in, KnownOperation &out );
//
//
//void Serialize( const LoadFilesQuery &in, dcma::rpc::LoadFilesQuery &out );
//void Deserialize( const dcma::rpc::LoadFilesQuery &in, LoadFilesQuery &out );
//
//void Serialize( const LoadFilesResponse &in, dcma::rpc::LoadFilesResponse &out );
//void Deserialize( const dcma::rpc::LoadFilesResponse &in, LoadFilesResponse &out );
//
//void Serialize( const ExecuteScriptQuery &in, dcma::rpc::ExecuteScriptQuery &out );
//void Deserialize( const dcma::rpc::ExecuteScriptQuery &in, ExecuteScriptQuery &out );
//
//void Serialize( const ExecuteScriptResponse &in, dcma::rpc::ExecuteScriptResponse &out );
//void Deserialize( const dcma::rpc::ExecuteScriptResponse &in, ExecuteScriptResponse &out );

