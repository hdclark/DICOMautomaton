//Serialization_Tests.cc - A part of DICOMautomaton 2026. Written by hal clark.
//
// This file contains unit tests for the RPC serialization and deserialization routines
// defined in rpc/Serialization.cc. These tests are separated into their own file because
// Thrift_objs is linked into shared libraries which don't include doctest implementation.

#include <cmath>
#include <cstdint>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "doctest20251212/doctest.h"

#include "YgorMath.h"
#include "YgorImages.h"

#include "Structs.h"
#include "Tables.h"

#include "rpc/Serialization.h"


// ============================================================================
// Primitive type round-trip tests
// ============================================================================

TEST_CASE( "Serialize/Deserialize bool" ){
    for(bool val : {true, false}){
        bool rpc_val = !val;
        Serialize(val, rpc_val);
        bool out_val = !val;
        Deserialize(rpc_val, out_val);
        REQUIRE( out_val == val );
    }
}

TEST_CASE( "Serialize/Deserialize string" ){
    for(const std::string &val : {"", "hello", "key=value", "with spaces", "\ttab\nnewline"}){
        std::string rpc_val;
        Serialize(val, rpc_val);
        std::string out_val;
        Deserialize(rpc_val, out_val);
        REQUIRE( out_val == val );
    }
}

TEST_CASE( "Serialize/Deserialize int64_t" ){
    for(int64_t val : {int64_t(0), int64_t(1), int64_t(-1),
                       std::numeric_limits<int64_t>::min(),
                       std::numeric_limits<int64_t>::max(),
                       std::numeric_limits<int64_t>::lowest()}){
        int64_t rpc_val = 0;
        Serialize(val, rpc_val);
        int64_t out_val = 0;
        Deserialize(rpc_val, out_val);
        REQUIRE( out_val == val );
    }
}

TEST_CASE( "Serialize/Deserialize uint32_t" ){
    for(uint32_t val : {uint32_t(0), uint32_t(1), uint32_t(255),
                        std::numeric_limits<uint32_t>::max()}){
        int64_t rpc_val = 0;
        Serialize(val, rpc_val);
        uint32_t out_val = 0;
        Deserialize(rpc_val, out_val);
        REQUIRE( out_val == val );
    }
}

TEST_CASE( "Serialize/Deserialize uint64_t" ){
    for(uint64_t val : {uint64_t(0), uint64_t(1), uint64_t(12345)}){
        int64_t rpc_val = 0;
        Serialize(val, rpc_val);
        uint64_t out_val = 0;
        Deserialize(rpc_val, out_val);
        REQUIRE( out_val == val );
    }
}

TEST_CASE( "Serialize/Deserialize double" ){
    for(double val : {0.0, 1.0, -1.0, 3.14159, 1.0e-300, 1.0e+300,
                      std::numeric_limits<double>::infinity(),
                      -std::numeric_limits<double>::infinity()}){
        double rpc_val = 0.0;
        Serialize(val, rpc_val);
        double out_val = 0.0;
        Deserialize(rpc_val, out_val);
        REQUIRE( out_val == val );
    }
    // NaN must be tested separately since NaN != NaN.
    {
        double val = std::numeric_limits<double>::quiet_NaN();
        double rpc_val = 0.0;
        Serialize(val, rpc_val);
        double out_val = 0.0;
        Deserialize(rpc_val, out_val);
        REQUIRE( std::isnan(out_val) );
    }
}

TEST_CASE( "Serialize/Deserialize float via double" ){
    for(float val : {0.0f, 1.0f, -1.0f, 3.14f}){
        double rpc_val = 0.0;
        Serialize(val, rpc_val);
        float out_val = 0.0f;
        Deserialize(rpc_val, out_val);
        REQUIRE( out_val == val );
    }
}


// ============================================================================
// Ygor class round-trip tests -- YgorMath.h
// ============================================================================

TEST_CASE( "Serialize/Deserialize metadata_map_t" ){
    SUBCASE( "empty map" ){
        metadata_map_t in;
        dcma::rpc::metadata_t rpc;
        Serialize(in, rpc);
        metadata_map_t out;
        Deserialize(rpc, out);
        REQUIRE( out.empty() );
    }
    SUBCASE( "populated map" ){
        metadata_map_t in;
        in["key1"] = "value1";
        in["key2"] = "value2";
        in["empty_val"] = "";
        dcma::rpc::metadata_t rpc;
        Serialize(in, rpc);
        metadata_map_t out;
        Deserialize(rpc, out);
        REQUIRE( out.size() == 3 );
        REQUIRE( out["key1"] == "value1" );
        REQUIRE( out["key2"] == "value2" );
        REQUIRE( out["empty_val"] == "" );
    }
}

TEST_CASE( "Serialize/Deserialize vec3<double>" ){
    SUBCASE( "typical values" ){
        vec3<double> in(1.0, 2.0, 3.0);
        dcma::rpc::vec3_double rpc;
        Serialize(in, rpc);
        vec3<double> out;
        Deserialize(rpc, out);
        REQUIRE( out.x == in.x );
        REQUIRE( out.y == in.y );
        REQUIRE( out.z == in.z );
    }
    SUBCASE( "NaN values" ){
        vec3<double> in(std::numeric_limits<double>::quiet_NaN(),
                        std::numeric_limits<double>::quiet_NaN(),
                        std::numeric_limits<double>::quiet_NaN());
        dcma::rpc::vec3_double rpc;
        Serialize(in, rpc);
        vec3<double> out;
        Deserialize(rpc, out);
        REQUIRE( std::isnan(out.x) );
        REQUIRE( std::isnan(out.y) );
        REQUIRE( std::isnan(out.z) );
    }
    SUBCASE( "zero values" ){
        vec3<double> in(0.0, 0.0, 0.0);
        dcma::rpc::vec3_double rpc;
        Serialize(in, rpc);
        vec3<double> out;
        Deserialize(rpc, out);
        REQUIRE( out.x == 0.0 );
        REQUIRE( out.y == 0.0 );
        REQUIRE( out.z == 0.0 );
    }
}

TEST_CASE( "Serialize/Deserialize contour_of_points<double>" ){
    SUBCASE( "empty contour" ){
        contour_of_points<double> in;
        in.closed = false;
        dcma::rpc::contour_of_points_double rpc;
        Serialize(in, rpc);
        contour_of_points<double> out;
        Deserialize(rpc, out);
        REQUIRE( out.points.empty() );
        REQUIRE( out.closed == false );
        REQUIRE( out.metadata.empty() );
    }
    SUBCASE( "triangle contour" ){
        contour_of_points<double> in;
        in.closed = true;
        in.points.emplace_back(vec3<double>(0.0, 0.0, 0.0));
        in.points.emplace_back(vec3<double>(1.0, 0.0, 0.0));
        in.points.emplace_back(vec3<double>(0.0, 1.0, 0.0));
        in.metadata["ROIName"] = "TestROI";
        dcma::rpc::contour_of_points_double rpc;
        Serialize(in, rpc);
        contour_of_points<double> out;
        Deserialize(rpc, out);
        REQUIRE( out.points.size() == 3 );
        REQUIRE( out.closed == true );
        REQUIRE( out.metadata.size() == 1 );
        REQUIRE( out.metadata["ROIName"] == "TestROI" );
        auto it = out.points.begin();
        REQUIRE( it->x == 0.0 );
        ++it;
        REQUIRE( it->x == 1.0 );
        ++it;
        REQUIRE( it->y == 1.0 );
    }
}

TEST_CASE( "Serialize/Deserialize contour_collection<double>" ){
    SUBCASE( "empty collection" ){
        contour_collection<double> in;
        dcma::rpc::contour_collection_double rpc;
        Serialize(in, rpc);
        contour_collection<double> out;
        Deserialize(rpc, out);
        REQUIRE( out.contours.empty() );
    }
    SUBCASE( "collection with one contour" ){
        contour_collection<double> in;
        in.contours.emplace_back();
        in.contours.back().closed = true;
        in.contours.back().points.emplace_back(vec3<double>(1.0, 2.0, 3.0));
        in.contours.back().points.emplace_back(vec3<double>(4.0, 5.0, 6.0));
        dcma::rpc::contour_collection_double rpc;
        Serialize(in, rpc);
        contour_collection<double> out;
        Deserialize(rpc, out);
        REQUIRE( out.contours.size() == 1 );
        REQUIRE( out.contours.front().points.size() == 2 );
        REQUIRE( out.contours.front().closed == true );
    }
}

TEST_CASE( "Serialize/Deserialize point_set<double>" ){
    SUBCASE( "empty point set" ){
        point_set<double> in;
        dcma::rpc::point_set_double rpc;
        Serialize(in, rpc);
        point_set<double> out;
        Deserialize(rpc, out);
        REQUIRE( out.points.empty() );
        REQUIRE( out.normals.empty() );
        REQUIRE( out.colours.empty() );
        REQUIRE( out.metadata.empty() );
    }
    SUBCASE( "populated point set" ){
        point_set<double> in;
        in.points.emplace_back(vec3<double>(1.0, 2.0, 3.0));
        in.points.emplace_back(vec3<double>(4.0, 5.0, 6.0));
        in.normals.emplace_back(vec3<double>(0.0, 0.0, 1.0));
        in.normals.emplace_back(vec3<double>(0.0, 1.0, 0.0));
        in.colours.push_back(0xFF0000FFu);
        in.colours.push_back(0x00FF00FFu);
        in.metadata["label"] = "test_cloud";
        dcma::rpc::point_set_double rpc;
        Serialize(in, rpc);
        point_set<double> out;
        Deserialize(rpc, out);
        REQUIRE( out.points.size() == 2 );
        REQUIRE( out.normals.size() == 2 );
        REQUIRE( out.colours.size() == 2 );
        REQUIRE( out.colours[0] == 0xFF0000FFu );
        REQUIRE( out.colours[1] == 0x00FF00FFu );
        REQUIRE( out.metadata["label"] == "test_cloud" );
        REQUIRE( out.points[0].x == 1.0 );
        REQUIRE( out.points[1].z == 6.0 );
    }
}

TEST_CASE( "Serialize/Deserialize std::array<double,4> (sample4)" ){
    std::array<double,4> in = {{1.0, 0.1, 2.0, 0.2}};
    dcma::rpc::sample4_double rpc;
    Serialize(in, rpc);
    std::array<double,4> out = {{0.0, 0.0, 0.0, 0.0}};
    Deserialize(rpc, out);
    REQUIRE( out[0] == 1.0 );
    REQUIRE( out[1] == 0.1 );
    REQUIRE( out[2] == 2.0 );
    REQUIRE( out[3] == 0.2 );
}

TEST_CASE( "Serialize/Deserialize samples_1D<double>" ){
    SUBCASE( "empty samples" ){
        samples_1D<double> in;
        dcma::rpc::samples_1D_double rpc;
        Serialize(in, rpc);
        samples_1D<double> out;
        Deserialize(rpc, out);
        REQUIRE( out.samples.empty() );
        REQUIRE( out.uncertainties_known_to_be_independent_and_random == false );
        REQUIRE( out.metadata.empty() );
    }
    SUBCASE( "populated samples" ){
        samples_1D<double> in;
        in.push_back(1.0, 0.1, 10.0, 0.5);
        in.push_back(2.0, 0.2, 20.0, 1.0);
        in.uncertainties_known_to_be_independent_and_random = true;
        in.metadata["unit"] = "Gy";
        dcma::rpc::samples_1D_double rpc;
        Serialize(in, rpc);
        samples_1D<double> out;
        Deserialize(rpc, out);
        REQUIRE( out.samples.size() == 2 );
        REQUIRE( out.uncertainties_known_to_be_independent_and_random == true );
        REQUIRE( out.metadata["unit"] == "Gy" );
        REQUIRE( out.samples[0][0] == 1.0 );
        REQUIRE( out.samples[0][2] == 10.0 );
        REQUIRE( out.samples[1][0] == 2.0 );
        REQUIRE( out.samples[1][2] == 20.0 );
    }
}

TEST_CASE( "Serialize/Deserialize fv_surface_mesh<double,uint64_t>" ){
    SUBCASE( "empty mesh" ){
        fv_surface_mesh<double,uint64_t> in;
        dcma::rpc::fv_surface_mesh_double_int64 rpc;
        Serialize(in, rpc);
        fv_surface_mesh<double,uint64_t> out;
        Deserialize(rpc, out);
        REQUIRE( out.vertices.empty() );
        REQUIRE( out.vertex_normals.empty() );
        REQUIRE( out.vertex_colours.empty() );
        REQUIRE( out.faces.empty() );
        REQUIRE( out.involved_faces.empty() );
        REQUIRE( out.metadata.empty() );
    }
    SUBCASE( "single triangle" ){
        fv_surface_mesh<double,uint64_t> in;
        in.vertices.emplace_back(vec3<double>(0.0, 0.0, 0.0));
        in.vertices.emplace_back(vec3<double>(1.0, 0.0, 0.0));
        in.vertices.emplace_back(vec3<double>(0.0, 1.0, 0.0));
        in.vertex_normals.emplace_back(vec3<double>(0.0, 0.0, 1.0));
        in.vertex_normals.emplace_back(vec3<double>(0.0, 0.0, 1.0));
        in.vertex_normals.emplace_back(vec3<double>(0.0, 0.0, 1.0));
        in.vertex_colours.push_back(0xFF0000FFu);
        in.vertex_colours.push_back(0x00FF00FFu);
        in.vertex_colours.push_back(0x0000FFFFu);
        in.faces.push_back({0, 1, 2});
        in.involved_faces.push_back({0});
        in.involved_faces.push_back({0});
        in.involved_faces.push_back({0});
        in.metadata["mesh_type"] = "triangle";
        dcma::rpc::fv_surface_mesh_double_int64 rpc;
        Serialize(in, rpc);
        fv_surface_mesh<double,uint64_t> out;
        Deserialize(rpc, out);
        REQUIRE( out.vertices.size() == 3 );
        REQUIRE( out.vertex_normals.size() == 3 );
        REQUIRE( out.vertex_colours.size() == 3 );
        REQUIRE( out.faces.size() == 1 );
        REQUIRE( out.faces[0].size() == 3 );
        REQUIRE( out.faces[0][0] == 0 );
        REQUIRE( out.faces[0][1] == 1 );
        REQUIRE( out.faces[0][2] == 2 );
        REQUIRE( out.involved_faces.size() == 3 );
        REQUIRE( out.metadata["mesh_type"] == "triangle" );
    }
}


// ============================================================================
// Ygor class round-trip tests -- YgorImages.h
// ============================================================================

TEST_CASE( "Serialize/Deserialize planar_image<float,double>" ){
    SUBCASE( "small image" ){
        planar_image<float,double> in;
        in.init_buffer(2, 3, 1);
        in.init_spatial(1.0, 1.0, 1.0,
                        vec3<double>(0.0, 0.0, 0.0),
                        vec3<double>(0.0, 0.0, 0.0));
        in.init_orientation(vec3<double>(1.0, 0.0, 0.0),
                            vec3<double>(0.0, 1.0, 0.0));
        in.fill_pixels(0, 42.0f);
        in.metadata["Modality"] = "CT";
        dcma::rpc::planar_image_double_double rpc;
        Serialize(in, rpc);
        planar_image<float,double> out;
        Deserialize(rpc, out);
        REQUIRE( out.rows == 2 );
        REQUIRE( out.columns == 3 );
        REQUIRE( out.channels == 1 );
        REQUIRE( out.pxl_dx == 1.0 );
        REQUIRE( out.pxl_dy == 1.0 );
        REQUIRE( out.pxl_dz == 1.0 );
        REQUIRE( out.data.size() == in.data.size() );
        REQUIRE( out.data[0] == 42.0f );
        REQUIRE( out.metadata["Modality"] == "CT" );
        REQUIRE( out.row_unit.x == 1.0 );
        REQUIRE( out.col_unit.y == 1.0 );
    }
}

TEST_CASE( "Serialize/Deserialize planar_image_collection<float,double>" ){
    SUBCASE( "empty collection" ){
        planar_image_collection<float,double> in;
        dcma::rpc::planar_image_collection_double_double rpc;
        Serialize(in, rpc);
        planar_image_collection<float,double> out;
        Deserialize(rpc, out);
        REQUIRE( out.images.empty() );
    }
    SUBCASE( "collection with one image" ){
        planar_image_collection<float,double> in;
        in.images.emplace_back();
        in.images.back().init_buffer(2, 2, 1);
        in.images.back().init_spatial(1.0, 1.0, 1.0,
                                      vec3<double>(0.0, 0.0, 0.0),
                                      vec3<double>(0.0, 0.0, 0.0));
        in.images.back().init_orientation(vec3<double>(1.0, 0.0, 0.0),
                                          vec3<double>(0.0, 1.0, 0.0));
        in.images.back().fill_pixels(0, 5.0f);
        dcma::rpc::planar_image_collection_double_double rpc;
        Serialize(in, rpc);
        planar_image_collection<float,double> out;
        Deserialize(rpc, out);
        REQUIRE( out.images.size() == 1 );
        REQUIRE( out.images.front().rows == 2 );
        REQUIRE( out.images.front().columns == 2 );
        REQUIRE( out.images.front().data[0] == 5.0f );
    }
}


// ============================================================================
// DICOMautomaton class round-trip tests -- Tables.h
// ============================================================================

TEST_CASE( "Serialize/Deserialize tables::table2" ){
    SUBCASE( "empty table" ){
        tables::table2 in;
        dcma::rpc::table2 rpc;
        Serialize(in, rpc);
        tables::table2 out;
        Deserialize(rpc, out);
        REQUIRE( out.data.empty() );
        REQUIRE( out.metadata.empty() );
    }
    SUBCASE( "table with cells" ){
        tables::table2 in;
        in.inject(0, 0, "A");
        in.inject(0, 1, "B");
        in.inject(1, 0, "C");
        in.metadata["title"] = "test_table";
        dcma::rpc::table2 rpc;
        Serialize(in, rpc);
        tables::table2 out;
        Deserialize(rpc, out);
        REQUIRE( out.data.size() == 3 );
        REQUIRE( out.value(0, 0).value() == "A" );
        REQUIRE( out.value(0, 1).value() == "B" );
        REQUIRE( out.value(1, 0).value() == "C" );
        REQUIRE( out.metadata["title"] == "test_table" );
    }
}


// ============================================================================
// DICOMautomaton class round-trip tests -- Structs.h
// ============================================================================

TEST_CASE( "Serialize/Deserialize Contour_Data" ){
    SUBCASE( "empty" ){
        Contour_Data in;
        dcma::rpc::Contour_Data rpc;
        Serialize(in, rpc);
        Contour_Data out;
        Deserialize(rpc, out);
        REQUIRE( out.ccs.empty() );
    }
    SUBCASE( "with contour collections" ){
        Contour_Data in;
        in.ccs.emplace_back();
        in.ccs.back().contours.emplace_back();
        in.ccs.back().contours.back().closed = true;
        in.ccs.back().contours.back().points.emplace_back(vec3<double>(1.0, 2.0, 3.0));
        in.ccs.back().contours.back().metadata["ROIName"] = "Body";
        dcma::rpc::Contour_Data rpc;
        Serialize(in, rpc);
        Contour_Data out;
        Deserialize(rpc, out);
        REQUIRE( out.ccs.size() == 1 );
        REQUIRE( out.ccs.front().contours.size() == 1 );
        REQUIRE( out.ccs.front().contours.front().closed == true );
        REQUIRE( out.ccs.front().contours.front().points.size() == 1 );
        REQUIRE( out.ccs.front().contours.front().metadata["ROIName"] == "Body" );
    }
}

TEST_CASE( "Serialize/Deserialize Image_Array" ){
    Image_Array in;
    in.filename = "test_image.dcm";
    in.imagecoll.images.emplace_back();
    in.imagecoll.images.back().init_buffer(2, 2, 1);
    in.imagecoll.images.back().init_spatial(1.0, 1.0, 1.0,
                                            vec3<double>(0.0, 0.0, 0.0),
                                            vec3<double>(0.0, 0.0, 0.0));
    in.imagecoll.images.back().init_orientation(vec3<double>(1.0, 0.0, 0.0),
                                                vec3<double>(0.0, 1.0, 0.0));
    in.imagecoll.images.back().fill_pixels(0, 100.0f);

    dcma::rpc::Image_Array rpc;
    Serialize(in, rpc);
    Image_Array out;
    Deserialize(rpc, out);
    REQUIRE( out.filename == "test_image.dcm" );
    REQUIRE( out.imagecoll.images.size() == 1 );
    REQUIRE( out.imagecoll.images.front().rows == 2 );
    REQUIRE( out.imagecoll.images.front().data[0] == 100.0f );
}

TEST_CASE( "Serialize/Deserialize Point_Cloud" ){
    Point_Cloud in;
    in.pset.points.emplace_back(vec3<double>(1.0, 2.0, 3.0));
    in.pset.metadata["label"] = "marker";

    dcma::rpc::Point_Cloud rpc;
    Serialize(in, rpc);
    Point_Cloud out;
    Deserialize(rpc, out);
    REQUIRE( out.pset.points.size() == 1 );
    REQUIRE( out.pset.points[0].x == 1.0 );
    REQUIRE( out.pset.metadata["label"] == "marker" );
}

TEST_CASE( "Serialize/Deserialize Surface_Mesh" ){
    Surface_Mesh in;
    in.meshes.vertices.emplace_back(vec3<double>(0.0, 0.0, 0.0));
    in.meshes.vertices.emplace_back(vec3<double>(1.0, 0.0, 0.0));
    in.meshes.vertices.emplace_back(vec3<double>(0.0, 1.0, 0.0));
    in.meshes.faces.push_back({0, 1, 2});
    in.meshes.metadata["type"] = "triangle";

    dcma::rpc::Surface_Mesh rpc;
    Serialize(in, rpc);
    Surface_Mesh out;
    Deserialize(rpc, out);
    REQUIRE( out.meshes.vertices.size() == 3 );
    REQUIRE( out.meshes.faces.size() == 1 );
    REQUIRE( out.meshes.faces[0].size() == 3 );
    REQUIRE( out.meshes.metadata["type"] == "triangle" );
}

TEST_CASE( "Serialize/Deserialize Static_Machine_State" ){
    Static_Machine_State in;
    in.CumulativeMetersetWeight = 0.5;
    in.ControlPointIndex = 3;
    in.GantryAngle = 90.0;
    in.GantryRotationDirection = 1.0;
    in.BeamLimitingDeviceAngle = 0.0;
    in.BeamLimitingDeviceRotationDirection = 0.0;
    in.PatientSupportAngle = 0.0;
    in.PatientSupportRotationDirection = 0.0;
    in.TableTopEccentricAngle = 0.0;
    in.TableTopEccentricRotationDirection = 0.0;
    in.TableTopVerticalPosition = 10.0;
    in.TableTopLongitudinalPosition = 20.0;
    in.TableTopLateralPosition = 5.0;
    in.TableTopPitchAngle = 0.0;
    in.TableTopPitchRotationDirection = 0.0;
    in.TableTopRollAngle = 0.0;
    in.TableTopRollRotationDirection = 0.0;
    in.IsocentrePosition = vec3<double>(0.0, 0.0, 100.0);
    in.JawPositionsX = {-5.0, 5.0};
    in.JawPositionsY = {-5.0, 5.0};
    in.MLCPositionsX = {-3.0, -2.0, -1.0, 1.0, 2.0, 3.0};
    in.metadata["PatientID"] = "TEST001";

    dcma::rpc::Static_Machine_State rpc;
    Serialize(in, rpc);
    Static_Machine_State out;
    Deserialize(rpc, out);
    REQUIRE( out.CumulativeMetersetWeight == 0.5 );
    REQUIRE( out.ControlPointIndex == 3 );
    REQUIRE( out.GantryAngle == 90.0 );
    REQUIRE( out.GantryRotationDirection == 1.0 );
    REQUIRE( out.IsocentrePosition.z == 100.0 );
    REQUIRE( out.JawPositionsX.size() == 2 );
    REQUIRE( out.JawPositionsY.size() == 2 );
    REQUIRE( out.MLCPositionsX.size() == 6 );
    REQUIRE( out.MLCPositionsX[0] == -3.0 );
    REQUIRE( out.metadata["PatientID"] == "TEST001" );
}

TEST_CASE( "Serialize/Deserialize Static_Machine_State with NaN defaults" ){
    // Test round-tripping a default-constructed state (mostly NaN values).
    Static_Machine_State in;
    dcma::rpc::Static_Machine_State rpc;
    Serialize(in, rpc);
    Static_Machine_State out;
    Deserialize(rpc, out);
    REQUIRE( std::isnan(out.CumulativeMetersetWeight) );
    REQUIRE( out.ControlPointIndex == std::numeric_limits<int64_t>::lowest() );
    REQUIRE( std::isnan(out.GantryAngle) );
    REQUIRE( std::isnan(out.IsocentrePosition.x) );
    REQUIRE( out.JawPositionsX.empty() );
    REQUIRE( out.JawPositionsY.empty() );
    REQUIRE( out.MLCPositionsX.empty() );
}

TEST_CASE( "Serialize/Deserialize Dynamic_Machine_State" ){
    Dynamic_Machine_State in;
    in.BeamNumber = 1;
    in.FinalCumulativeMetersetWeight = 100.0;
    in.metadata["BeamName"] = "TestBeam";

    Static_Machine_State sms;
    sms.CumulativeMetersetWeight = 0.0;
    sms.ControlPointIndex = 0;
    sms.GantryAngle = 0.0;
    in.static_states.push_back(sms);

    sms.CumulativeMetersetWeight = 100.0;
    sms.ControlPointIndex = 1;
    sms.GantryAngle = 360.0;
    in.static_states.push_back(sms);

    dcma::rpc::Dynamic_Machine_State rpc;
    Serialize(in, rpc);
    Dynamic_Machine_State out;
    Deserialize(rpc, out);
    REQUIRE( out.BeamNumber == 1 );
    REQUIRE( out.FinalCumulativeMetersetWeight == 100.0 );
    REQUIRE( out.static_states.size() == 2 );
    REQUIRE( out.static_states[0].GantryAngle == 0.0 );
    REQUIRE( out.static_states[1].GantryAngle == 360.0 );
    REQUIRE( out.metadata["BeamName"] == "TestBeam" );
}

TEST_CASE( "Serialize/Deserialize RTPlan" ){
    RTPlan in;
    in.metadata["PlanName"] = "TestPlan";

    Dynamic_Machine_State dms;
    dms.BeamNumber = 1;
    dms.FinalCumulativeMetersetWeight = 50.0;
    in.dynamic_states.push_back(dms);

    dcma::rpc::RTPlan rpc;
    Serialize(in, rpc);
    RTPlan out;
    Deserialize(rpc, out);
    REQUIRE( out.dynamic_states.size() == 1 );
    REQUIRE( out.dynamic_states[0].BeamNumber == 1 );
    REQUIRE( out.metadata["PlanName"] == "TestPlan" );
}

TEST_CASE( "Serialize/Deserialize Line_Sample" ){
    Line_Sample in;
    in.line.push_back(1.0, 0.1, 10.0, 0.5);
    in.line.push_back(2.0, 0.2, 20.0, 1.0);
    in.line.metadata["type"] = "DVH";

    dcma::rpc::Line_Sample rpc;
    Serialize(in, rpc);
    Line_Sample out;
    Deserialize(rpc, out);
    REQUIRE( out.line.samples.size() == 2 );
    REQUIRE( out.line.samples[0][0] == 1.0 );
    REQUIRE( out.line.samples[1][2] == 20.0 );
    REQUIRE( out.line.metadata["type"] == "DVH" );
}

TEST_CASE( "Serialize/Deserialize Sparse_Table" ){
    Sparse_Table in;
    in.table.inject(0, 0, "header1");
    in.table.inject(0, 1, "header2");
    in.table.inject(1, 0, "value1");
    in.table.inject(1, 1, "value2");
    in.table.metadata["source"] = "test";

    dcma::rpc::Sparse_Table rpc;
    Serialize(in, rpc);
    Sparse_Table out;
    Deserialize(rpc, out);
    REQUIRE( out.table.data.size() == 4 );
    REQUIRE( out.table.value(0, 0).value() == "header1" );
    REQUIRE( out.table.value(1, 1).value() == "value2" );
    REQUIRE( out.table.metadata["source"] == "test" );
}

TEST_CASE( "Serialize/Deserialize Transform3 throws" ){
    // Transform3 serialization is intentionally not yet supported.
    Transform3 in;
    dcma::rpc::Transform3 rpc;
    REQUIRE_THROWS_AS( Serialize(in, rpc), std::runtime_error );
    REQUIRE_THROWS_AS( Deserialize(rpc, in), std::runtime_error );
}


// ============================================================================
// Drover round-trip tests
// ============================================================================

TEST_CASE( "Serialize/Deserialize empty Drover" ){
    Drover in;
    dcma::rpc::Drover rpc;
    Serialize(in, rpc);
    Drover out;
    Deserialize(rpc, out);
    REQUIRE( !out.Has_Contour_Data() );
    REQUIRE( !out.Has_Image_Data() );
    REQUIRE( !out.Has_Point_Data() );
    REQUIRE( !out.Has_Mesh_Data() );
    REQUIRE( !out.Has_RTPlan_Data() );
    REQUIRE( !out.Has_LSamp_Data() );
    REQUIRE( !out.Has_Tran3_Data() );
    REQUIRE( !out.Has_Table_Data() );
}

TEST_CASE( "Serialize/Deserialize Drover with contour data" ){
    Drover in;
    in.Ensure_Contour_Data_Allocated();
    in.contour_data->ccs.emplace_back();
    in.contour_data->ccs.back().contours.emplace_back();
    in.contour_data->ccs.back().contours.back().closed = true;
    in.contour_data->ccs.back().contours.back().points.emplace_back(vec3<double>(1.0, 2.0, 3.0));
    in.contour_data->ccs.back().contours.back().points.emplace_back(vec3<double>(4.0, 5.0, 6.0));
    in.contour_data->ccs.back().contours.back().metadata["ROIName"] = "TestROI";

    dcma::rpc::Drover rpc;
    Serialize(in, rpc);
    Drover out;
    Deserialize(rpc, out);
    REQUIRE( out.Has_Contour_Data() );
    REQUIRE( out.contour_data->ccs.size() == 1 );
    REQUIRE( out.contour_data->ccs.front().contours.size() == 1 );
    REQUIRE( out.contour_data->ccs.front().contours.front().closed == true );
    REQUIRE( out.contour_data->ccs.front().contours.front().points.size() == 2 );
    REQUIRE( out.contour_data->ccs.front().contours.front().metadata["ROIName"] == "TestROI" );
}

TEST_CASE( "Serialize/Deserialize Drover with image data" ){
    Drover in;
    auto img_arr = std::make_shared<Image_Array>();
    img_arr->filename = "test.dcm";
    img_arr->imagecoll.images.emplace_back();
    img_arr->imagecoll.images.back().init_buffer(3, 3, 1);
    img_arr->imagecoll.images.back().init_spatial(1.0, 1.0, 2.0,
                                                   vec3<double>(0.0, 0.0, 0.0),
                                                   vec3<double>(10.0, 20.0, 30.0));
    img_arr->imagecoll.images.back().init_orientation(vec3<double>(1.0, 0.0, 0.0),
                                                      vec3<double>(0.0, 1.0, 0.0));
    img_arr->imagecoll.images.back().fill_pixels(0, 42.0f);
    in.image_data.push_back(img_arr);

    dcma::rpc::Drover rpc;
    Serialize(in, rpc);
    Drover out;
    Deserialize(rpc, out);
    REQUIRE( out.Has_Image_Data() );
    REQUIRE( out.image_data.size() == 1 );
    const auto &out_img = out.image_data.front()->imagecoll.images.front();
    REQUIRE( out_img.rows == 3 );
    REQUIRE( out_img.columns == 3 );
    REQUIRE( out_img.pxl_dz == 2.0 );
    REQUIRE( out_img.offset.x == 10.0 );
    REQUIRE( out_img.data[0] == 42.0f );
    REQUIRE( out.image_data.front()->filename == "test.dcm" );
}

TEST_CASE( "Serialize/Deserialize Drover with point data" ){
    Drover in;
    auto pc = std::make_shared<Point_Cloud>();
    pc->pset.points.emplace_back(vec3<double>(1.0, 2.0, 3.0));
    pc->pset.metadata["label"] = "point";
    in.point_data.push_back(pc);

    dcma::rpc::Drover rpc;
    Serialize(in, rpc);
    Drover out;
    Deserialize(rpc, out);
    REQUIRE( out.Has_Point_Data() );
    REQUIRE( out.point_data.size() == 1 );
    REQUIRE( out.point_data.front()->pset.points.size() == 1 );
    REQUIRE( out.point_data.front()->pset.points[0].x == 1.0 );
}

TEST_CASE( "Serialize/Deserialize Drover with mesh data" ){
    Drover in;
    auto sm = std::make_shared<Surface_Mesh>();
    sm->meshes.vertices.emplace_back(vec3<double>(0.0, 0.0, 0.0));
    sm->meshes.vertices.emplace_back(vec3<double>(1.0, 0.0, 0.0));
    sm->meshes.vertices.emplace_back(vec3<double>(0.0, 1.0, 0.0));
    sm->meshes.faces.push_back({0, 1, 2});
    in.smesh_data.push_back(sm);

    dcma::rpc::Drover rpc;
    Serialize(in, rpc);
    Drover out;
    Deserialize(rpc, out);
    REQUIRE( out.Has_Mesh_Data() );
    REQUIRE( out.smesh_data.size() == 1 );
    REQUIRE( out.smesh_data.front()->meshes.vertices.size() == 3 );
    REQUIRE( out.smesh_data.front()->meshes.faces.size() == 1 );
}

TEST_CASE( "Serialize/Deserialize Drover with RT plan data" ){
    Drover in;
    auto plan = std::make_shared<RTPlan>();
    plan->metadata["PlanName"] = "TestPlan";
    Dynamic_Machine_State dms;
    dms.BeamNumber = 1;
    dms.FinalCumulativeMetersetWeight = 100.0;
    plan->dynamic_states.push_back(dms);
    in.rtplan_data.push_back(plan);

    dcma::rpc::Drover rpc;
    Serialize(in, rpc);
    Drover out;
    Deserialize(rpc, out);
    REQUIRE( out.Has_RTPlan_Data() );
    REQUIRE( out.rtplan_data.size() == 1 );
    REQUIRE( out.rtplan_data.front()->dynamic_states.size() == 1 );
    REQUIRE( out.rtplan_data.front()->dynamic_states[0].BeamNumber == 1 );
    REQUIRE( out.rtplan_data.front()->metadata["PlanName"] == "TestPlan" );
}

TEST_CASE( "Serialize/Deserialize Drover with line sample data" ){
    Drover in;
    auto ls = std::make_shared<Line_Sample>();
    ls->line.push_back(1.0, 0.0, 10.0, 0.0);
    ls->line.push_back(2.0, 0.0, 20.0, 0.0);
    ls->line.metadata["type"] = "DVH";
    in.lsamp_data.push_back(ls);

    dcma::rpc::Drover rpc;
    Serialize(in, rpc);
    Drover out;
    Deserialize(rpc, out);
    REQUIRE( out.Has_LSamp_Data() );
    REQUIRE( out.lsamp_data.size() == 1 );
    REQUIRE( out.lsamp_data.front()->line.samples.size() == 2 );
    REQUIRE( out.lsamp_data.front()->line.metadata["type"] == "DVH" );
}

TEST_CASE( "Serialize/Deserialize Drover with table data" ){
    Drover in;
    auto tbl = std::make_shared<Sparse_Table>();
    tbl->table.inject(0, 0, "A");
    tbl->table.inject(1, 0, "B");
    tbl->table.metadata["source"] = "test";
    in.table_data.push_back(tbl);

    dcma::rpc::Drover rpc;
    Serialize(in, rpc);
    Drover out;
    Deserialize(rpc, out);
    REQUIRE( out.Has_Table_Data() );
    REQUIRE( out.table_data.size() == 1 );
    REQUIRE( out.table_data.front()->table.value(0, 0).value() == "A" );
    REQUIRE( out.table_data.front()->table.value(1, 0).value() == "B" );
    REQUIRE( out.table_data.front()->table.metadata["source"] == "test" );
}

TEST_CASE( "Serialize/Deserialize Drover with multiple data types" ){
    // Create a Drover with multiple types of data to test the full round-trip.
    Drover in;

    // Contour data.
    in.Ensure_Contour_Data_Allocated();
    in.contour_data->ccs.emplace_back();
    in.contour_data->ccs.back().contours.emplace_back();
    in.contour_data->ccs.back().contours.back().closed = true;
    in.contour_data->ccs.back().contours.back().points.emplace_back(vec3<double>(0.0, 0.0, 0.0));

    // Image data.
    auto img_arr = std::make_shared<Image_Array>();
    img_arr->imagecoll.images.emplace_back();
    img_arr->imagecoll.images.back().init_buffer(2, 2, 1);
    img_arr->imagecoll.images.back().init_spatial(1.0, 1.0, 1.0,
                                                   vec3<double>(0.0, 0.0, 0.0),
                                                   vec3<double>(0.0, 0.0, 0.0));
    img_arr->imagecoll.images.back().init_orientation(vec3<double>(1.0, 0.0, 0.0),
                                                      vec3<double>(0.0, 1.0, 0.0));
    img_arr->imagecoll.images.back().fill_pixels(0, 1.0f);
    in.image_data.push_back(img_arr);

    // Point cloud.
    auto pc = std::make_shared<Point_Cloud>();
    pc->pset.points.emplace_back(vec3<double>(5.0, 5.0, 5.0));
    in.point_data.push_back(pc);

    // Line sample.
    auto ls = std::make_shared<Line_Sample>();
    ls->line.push_back(0.0, 0.0, 0.0, 0.0);
    in.lsamp_data.push_back(ls);

    // Table.
    auto tbl = std::make_shared<Sparse_Table>();
    tbl->table.inject(0, 0, "x");
    in.table_data.push_back(tbl);

    dcma::rpc::Drover rpc;
    Serialize(in, rpc);
    Drover out;
    Deserialize(rpc, out);

    REQUIRE( out.Has_Contour_Data() );
    REQUIRE( out.Has_Image_Data() );
    REQUIRE( out.Has_Point_Data() );
    REQUIRE( out.Has_LSamp_Data() );
    REQUIRE( out.Has_Table_Data() );
    REQUIRE( !out.Has_Mesh_Data() );
    REQUIRE( !out.Has_RTPlan_Data() );
    REQUIRE( !out.Has_Tran3_Data() );

    REQUIRE( out.contour_data->ccs.size() == 1 );
    REQUIRE( out.image_data.size() == 1 );
    REQUIRE( out.point_data.size() == 1 );
    REQUIRE( out.lsamp_data.size() == 1 );
    REQUIRE( out.table_data.size() == 1 );
}
