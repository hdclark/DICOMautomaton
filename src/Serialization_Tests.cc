//Serialization_Tests.cc - A part of DICOMautomaton 2026. Written by hal clark.

#include <filesystem>
#include <fstream>
#include <list>
#include <map>
#include <memory>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include "doctest20251212/doctest.h"

#include "Common_Serialization.h"
#include "Operations/SerializeDrover.h"
#include "Serialization_File_Loader.h"
#include "Structs.h"

#ifdef DCMA_USE_GNU_GSL
#include "KineticModel_1Compartment2Input_5Param_Chebyshev_Common.h"
#include "KineticModel_1Compartment2Input_5Param_LinearInterp_Common.h"
#include "KineticModel_1Compartment2Input_Reduced3Param_Chebyshev_Common.h"
#endif


namespace {

#ifdef DCMA_USE_GNU_GSL
bool
equal_special(const double lhs, const double rhs){
    if(std::isnan(lhs) && std::isnan(rhs)) return true;
    return lhs == rhs;
}

void
check_special_equal(const double lhs, const double rhs){
    CHECK( equal_special(lhs, rhs) );
}

std::shared_ptr<samples_1D<double>>
make_test_samples(const std::string &description){
    auto out = std::make_shared<samples_1D<double>>();
    out->push_back(0.0, 0.01, 1.0, 0.1, true);
    out->push_back(1.0, 0.02, 2.0, 0.2, true);
    out->uncertainties_known_to_be_independent_and_random = true;
    out->metadata["Description"] = description;
    return out;
}

std::shared_ptr<cheby_approx<double>>
make_test_cheby(const double offset){
    auto out = std::make_shared<cheby_approx<double>>();
    out->Prepare(std::vector<double>{ offset, 1.0, -0.25 }, 0.0, 2.0);
    return out;
}

void
check_samples_equal(const std::shared_ptr<samples_1D<double>> &lhs,
                    const std::shared_ptr<samples_1D<double>> &rhs){
    REQUIRE( static_cast<bool>(lhs) == static_cast<bool>(rhs) );
    if(!lhs) return;
    CHECK( lhs->samples == rhs->samples );
    CHECK( lhs->uncertainties_known_to_be_independent_and_random
           == rhs->uncertainties_known_to_be_independent_and_random );
    CHECK( lhs->metadata == rhs->metadata );
}

void
check_cheby_equal(const std::shared_ptr<cheby_approx<double>> &lhs,
                  const std::shared_ptr<cheby_approx<double>> &rhs){
    REQUIRE( static_cast<bool>(lhs) == static_cast<bool>(rhs) );
    if(!lhs) return;
    CHECK( lhs->Sample(0.0) == rhs->Sample(0.0) );
    CHECK( lhs->Sample(1.0) == rhs->Sample(1.0) );
    CHECK( lhs->Sample(2.0) == rhs->Sample(2.0) );
}
#endif

} // namespace.


TEST_CASE("Common serialization round-trips an empty Drover through gzip XML"){
    const auto out_path = std::filesystem::temp_directory_path()
                        / "dicomautomaton_empty_drover_roundtrip.xml.gz";

    Drover original;
    Drover reloaded;

    const auto cleanup = [&](){
        std::error_code ec;
        std::filesystem::remove(out_path, ec);
    };
    cleanup();

    REQUIRE( Common_Serialize_Drover(original, out_path) );
    REQUIRE( std::filesystem::exists(out_path) );
    REQUIRE( std::filesystem::file_size(out_path) > 0ULL );

    REQUIRE( Common_Deserialize_Drover(reloaded, out_path) );

    CHECK_FALSE( reloaded.Has_Contour_Data() );
    CHECK_FALSE( reloaded.Has_Image_Data() );
    CHECK_FALSE( reloaded.Has_Point_Data() );
    CHECK_FALSE( reloaded.Has_Mesh_Data() );
    CHECK_FALSE( reloaded.Has_RTPlan_Data() );
    CHECK_FALSE( reloaded.Has_LSamp_Data() );
    CHECK_FALSE( reloaded.Has_Tran3_Data() );
    CHECK_FALSE( reloaded.Has_Table_Data() );

    cleanup();
}


TEST_CASE("Common serialization round-trips Drover contour data through gzip XML"){
    const auto out_path = std::filesystem::temp_directory_path()
                        / "dicomautomaton_contour_drover_roundtrip.xml.gz";

    contour_of_points<double> contour;
    contour.closed = true;
    contour.metadata["ROIName"] = "serialization-test-roi";
    contour.metadata["NormalizedROIName"] = "serialization_test_roi";
    contour.points.emplace_back(0.0, 0.0, 2.5);
    contour.points.emplace_back(1.0, 0.0, 2.5);
    contour.points.emplace_back(1.0, 1.0, 2.5);
    contour.points.emplace_back(0.0, 1.0, 2.5);

    contour_collection<double> contour_collection;
    contour_collection.contours.emplace_back(contour);

    Drover original;
    original.contour_data = std::make_shared<Contour_Data>();
    original.contour_data->ccs.emplace_back(contour_collection);

    Drover reloaded;

    const auto cleanup = [&](){
        std::error_code ec;
        std::filesystem::remove(out_path, ec);
    };
    cleanup();

    REQUIRE( Common_Serialize_Drover(original, out_path) );
    REQUIRE( Common_Deserialize_Drover(reloaded, out_path) );

    REQUIRE( reloaded.Has_Contour_Data() );
    REQUIRE( reloaded.contour_data != nullptr );
    REQUIRE( reloaded.contour_data->ccs.size() == 1UL );

    const auto &reloaded_collection = reloaded.contour_data->ccs.front();
    REQUIRE( reloaded_collection.contours.size() == 1UL );

    const auto &reloaded_contour = reloaded_collection.contours.front();
    CHECK( reloaded_contour.closed == contour.closed );
    CHECK( reloaded_contour.metadata == contour.metadata );
    CHECK( reloaded_contour.points == contour.points );

    cleanup();
}


TEST_CASE("Common serialization round-trips Drover image array data through gzip XML"){
    const auto out_path = std::filesystem::temp_directory_path()
                        / "dicomautomaton_image_array_drover_roundtrip.xml.gz";

    planar_image<float,double> image;
    image.init_buffer(2, 3, 2);
    image.init_spatial(1.25, 2.5, 3.75,
                       vec3<double>(10.0, 20.0, 30.0),
                       vec3<double>(0.5, 1.5, 2.5));
    image.init_orientation(vec3<double>(1.0, 0.0, 0.0),
                           vec3<double>(0.0, 1.0, 0.0));
    image.metadata["Modality"] = "serialization-test";
    image.metadata["SeriesDescription"] = "image-array-roundtrip";

    for(int64_t row = 0; row < image.rows; ++row){
        for(int64_t col = 0; col < image.columns; ++col){
            for(int64_t ch = 0; ch < image.channels; ++ch){
                image.reference(row, col, ch) = static_cast<float>(100 * row + 10 * col + ch);
            }
        }
    }

    auto image_array = std::make_shared<Image_Array>();
    image_array->imagecoll.images.emplace_back(image);

    Drover original;
    original.image_data.emplace_back(image_array);

    Drover reloaded;

    const auto cleanup = [&](){
        std::error_code ec;
        std::filesystem::remove(out_path, ec);
    };
    cleanup();

    REQUIRE( Common_Serialize_Drover(original, out_path) );
    REQUIRE( Common_Deserialize_Drover(reloaded, out_path) );

    REQUIRE( reloaded.Has_Image_Data() );
    REQUIRE( reloaded.image_data.size() == 1UL );
    REQUIRE( reloaded.image_data.front() != nullptr );

    const auto &reloaded_image_array = *reloaded.image_data.front();
    REQUIRE( reloaded_image_array.imagecoll.images.size() == 1UL );

    const auto &reloaded_image = reloaded_image_array.imagecoll.images.front();
    CHECK( reloaded_image == image );

    cleanup();
}


TEST_CASE("Common serialization round-trips Drover point cloud data through gzip XML"){
    const auto out_path = std::filesystem::temp_directory_path()
                        / "dicomautomaton_point_cloud_drover_roundtrip.xml.gz";

    auto point_cloud = std::make_shared<Point_Cloud>();
    point_cloud->pset.points.emplace_back(1.0, 2.0, 3.0);
    point_cloud->pset.points.emplace_back(-4.5, 5.5, -6.5);
    point_cloud->pset.normals.emplace_back(0.0, 0.0, 1.0);
    point_cloud->pset.normals.emplace_back(0.0, 1.0, 0.0);
    point_cloud->pset.colours.emplace_back(point_cloud->pset.pack_RGBA32_colour({ 255, 0, 0, 255 }));
    point_cloud->pset.colours.emplace_back(point_cloud->pset.pack_RGBA32_colour({ 0, 255, 0, 128 }));
    point_cloud->pset.metadata["Description"] = "point-cloud-roundtrip";

    Drover original;
    original.point_data.emplace_back(point_cloud);

    Drover reloaded;

    const auto cleanup = [&](){
        std::error_code ec;
        std::filesystem::remove(out_path, ec);
    };
    cleanup();

    REQUIRE( Common_Serialize_Drover(original, out_path) );
    REQUIRE( Common_Deserialize_Drover(reloaded, out_path) );

    REQUIRE( reloaded.Has_Point_Data() );
    REQUIRE( reloaded.point_data.size() == 1UL );
    REQUIRE( reloaded.point_data.front() != nullptr );

    const auto &reloaded_point_cloud = *reloaded.point_data.front();
    CHECK( reloaded_point_cloud.pset == point_cloud->pset );

    cleanup();
}


TEST_CASE("Common serialization round-trips Drover surface mesh data through gzip XML"){
    const auto out_path = std::filesystem::temp_directory_path()
                        / "dicomautomaton_surface_mesh_drover_roundtrip.xml.gz";

    auto surface_mesh = std::make_shared<Surface_Mesh>();
    surface_mesh->meshes.vertices.emplace_back(0.0, 0.0, 0.0);
    surface_mesh->meshes.vertices.emplace_back(1.0, 0.0, 0.0);
    surface_mesh->meshes.vertices.emplace_back(0.0, 1.0, 0.0);
    surface_mesh->meshes.vertex_normals.emplace_back(0.0, 0.0, 1.0);
    surface_mesh->meshes.vertex_normals.emplace_back(0.0, 0.0, 1.0);
    surface_mesh->meshes.vertex_normals.emplace_back(0.0, 0.0, 1.0);
    surface_mesh->meshes.vertex_colours.emplace_back(surface_mesh->meshes.pack_RGBA32_colour({ 255, 0, 0, 255 }));
    surface_mesh->meshes.vertex_colours.emplace_back(surface_mesh->meshes.pack_RGBA32_colour({ 0, 255, 0, 255 }));
    surface_mesh->meshes.vertex_colours.emplace_back(surface_mesh->meshes.pack_RGBA32_colour({ 0, 0, 255, 255 }));
    surface_mesh->meshes.faces.emplace_back(std::vector<uint64_t>{ 0, 1, 2 });
    surface_mesh->meshes.involved_faces.emplace_back(std::vector<uint64_t>{ 0 });
    surface_mesh->meshes.involved_faces.emplace_back(std::vector<uint64_t>{ 0 });
    surface_mesh->meshes.involved_faces.emplace_back(std::vector<uint64_t>{ 0 });
    surface_mesh->meshes.metadata["Description"] = "surface-mesh-roundtrip";

    Drover original;
    original.smesh_data.emplace_back(surface_mesh);

    Drover reloaded;

    const auto cleanup = [&](){
        std::error_code ec;
        std::filesystem::remove(out_path, ec);
    };
    cleanup();

    REQUIRE( Common_Serialize_Drover(original, out_path) );
    REQUIRE( Common_Deserialize_Drover(reloaded, out_path) );

    REQUIRE( reloaded.Has_Mesh_Data() );
    REQUIRE( reloaded.smesh_data.size() == 1UL );
    REQUIRE( reloaded.smesh_data.front() != nullptr );

    const auto &reloaded_surface_mesh = *reloaded.smesh_data.front();
    CHECK( reloaded_surface_mesh.meshes == surface_mesh->meshes );

    cleanup();
}


TEST_CASE("Common serialization round-trips Drover RT plan data through gzip XML"){
    const auto out_path = std::filesystem::temp_directory_path()
                        / "dicomautomaton_rtplan_drover_roundtrip.xml.gz";

    Static_Machine_State static_state;
    static_state.CumulativeMetersetWeight = 0.5;
    static_state.ControlPointIndex = 7;
    static_state.GantryAngle = 181.25;
    static_state.BeamLimitingDeviceAngle = 12.5;
    static_state.PatientSupportAngle = 3.25;
    static_state.TableTopVerticalPosition = 10.0;
    static_state.TableTopLongitudinalPosition = 20.0;
    static_state.TableTopLateralPosition = 30.0;
    static_state.IsocentrePosition = vec3<double>(1.0, 2.0, 3.0);
    static_state.JawPositionsX = { -4.0, 4.0 };
    static_state.JawPositionsY = { -5.0, 5.0 };
    static_state.MLCPositionsX = { -1.0, -0.5, 0.5, 1.0 };
    static_state.metadata["ControlPointLabel"] = "serialization-test-control-point";

    Dynamic_Machine_State dynamic_state;
    dynamic_state.BeamNumber = 3;
    dynamic_state.FinalCumulativeMetersetWeight = 1.0;
    dynamic_state.static_states.emplace_back(static_state);
    dynamic_state.metadata["BeamName"] = "serialization-test-beam";

    auto rtplan = std::make_shared<RTPlan>();
    rtplan->dynamic_states.emplace_back(dynamic_state);
    rtplan->metadata["PlanLabel"] = "serialization-test-plan";

    Drover original;
    original.rtplan_data.emplace_back(rtplan);

    Drover reloaded;

    const auto cleanup = [&](){
        std::error_code ec;
        std::filesystem::remove(out_path, ec);
    };
    cleanup();

    REQUIRE( Common_Serialize_Drover(original, out_path) );
    REQUIRE( Common_Deserialize_Drover(reloaded, out_path) );

    REQUIRE( reloaded.Has_RTPlan_Data() );
    REQUIRE( reloaded.rtplan_data.size() == 1UL );
    REQUIRE( reloaded.rtplan_data.front() != nullptr );

    const auto &reloaded_rtplan = *reloaded.rtplan_data.front();
    CHECK( reloaded_rtplan.metadata == rtplan->metadata );
    REQUIRE( reloaded_rtplan.dynamic_states.size() == 1UL );

    const auto &reloaded_dynamic_state = reloaded_rtplan.dynamic_states.front();
    CHECK( reloaded_dynamic_state.BeamNumber == dynamic_state.BeamNumber );
    CHECK( reloaded_dynamic_state.FinalCumulativeMetersetWeight == dynamic_state.FinalCumulativeMetersetWeight );
    CHECK( reloaded_dynamic_state.metadata == dynamic_state.metadata );
    REQUIRE( reloaded_dynamic_state.static_states.size() == 1UL );

    const auto &reloaded_static_state = reloaded_dynamic_state.static_states.front();
    CHECK( reloaded_static_state.CumulativeMetersetWeight == static_state.CumulativeMetersetWeight );
    CHECK( reloaded_static_state.ControlPointIndex == static_state.ControlPointIndex );
    CHECK( reloaded_static_state.GantryAngle == static_state.GantryAngle );
    CHECK( reloaded_static_state.BeamLimitingDeviceAngle == static_state.BeamLimitingDeviceAngle );
    CHECK( reloaded_static_state.PatientSupportAngle == static_state.PatientSupportAngle );
    CHECK( reloaded_static_state.TableTopVerticalPosition == static_state.TableTopVerticalPosition );
    CHECK( reloaded_static_state.TableTopLongitudinalPosition == static_state.TableTopLongitudinalPosition );
    CHECK( reloaded_static_state.TableTopLateralPosition == static_state.TableTopLateralPosition );
    CHECK( reloaded_static_state.IsocentrePosition == static_state.IsocentrePosition );
    CHECK( reloaded_static_state.JawPositionsX == static_state.JawPositionsX );
    CHECK( reloaded_static_state.JawPositionsY == static_state.JawPositionsY );
    CHECK( reloaded_static_state.MLCPositionsX == static_state.MLCPositionsX );
    CHECK( reloaded_static_state.metadata == static_state.metadata );

    cleanup();
}


TEST_CASE("Common serialization round-trips Drover line sample data through gzip XML"){
    const auto out_path = std::filesystem::temp_directory_path()
                        / "dicomautomaton_line_sample_drover_roundtrip.xml.gz";

    auto line_sample = std::make_shared<Line_Sample>();
    line_sample->line.push_back(0.0, 0.01, 10.0, 0.1, true);
    line_sample->line.push_back(1.0, 0.02, 12.5, 0.2, true);
    line_sample->line.push_back(2.0, 0.03, 15.0, 0.3, true);
    line_sample->line.uncertainties_known_to_be_independent_and_random = true;
    line_sample->line.metadata["LineName"] = "serialization-test-line";
    line_sample->line.metadata["Modality"] = "serialization-test";
    line_sample->line.metadata["Abscissa"] = "time";
    line_sample->line.metadata["Ordinate"] = "signal";

    Drover original;
    original.lsamp_data.emplace_back(line_sample);

    Drover reloaded;

    const auto cleanup = [&](){
        std::error_code ec;
        std::filesystem::remove(out_path, ec);
    };
    cleanup();

    REQUIRE( Common_Serialize_Drover(original, out_path) );
    REQUIRE( Common_Deserialize_Drover(reloaded, out_path) );

    REQUIRE( reloaded.Has_LSamp_Data() );
    REQUIRE( reloaded.lsamp_data.size() == 1UL );
    REQUIRE( reloaded.lsamp_data.front() != nullptr );

    const auto &reloaded_line = reloaded.lsamp_data.front()->line;
    CHECK( reloaded_line.samples == line_sample->line.samples );
    CHECK( reloaded_line.uncertainties_known_to_be_independent_and_random
           == line_sample->line.uncertainties_known_to_be_independent_and_random );
    CHECK( reloaded_line.metadata == line_sample->line.metadata );

    cleanup();
}


TEST_CASE("Serialization file loader consumes a Ygor serialized Drover file"){
    const auto out_path = std::filesystem::temp_directory_path()
                        / "dicomautomaton_loader_serialized_drover.xml.gz";

    auto point_cloud = std::make_shared<Point_Cloud>();
    point_cloud->pset.points.emplace_back(1.0, 2.0, 3.0);
    point_cloud->pset.metadata["Description"] = "loader-serialization-test";

    Drover original;
    original.point_data.emplace_back(point_cloud);

    const auto cleanup = [&](){
        std::error_code ec;
        std::filesystem::remove(out_path, ec);
    };
    cleanup();

    REQUIRE( Common_Serialize_Drover(original, out_path) );

    Drover loaded;
    std::map<std::string, std::string> invocation_metadata;
    std::list<std::filesystem::path> filenames{ out_path };

    REQUIRE( Load_From_Serialization_Files(loaded, invocation_metadata, "", filenames) );

    CHECK( filenames.empty() );
    REQUIRE( loaded.Has_Point_Data() );
    REQUIRE( loaded.point_data.size() == 1UL );
    REQUIRE( loaded.point_data.front() != nullptr );
    CHECK( loaded.point_data.front()->pset == point_cloud->pset );

    cleanup();
}


TEST_CASE("Serialization file loader leaves a non-serialized file unconsumed"){
    const auto out_path = std::filesystem::temp_directory_path()
                        / "dicomautomaton_loader_nonserialized_file.xml.gz";

    const auto cleanup = [&](){
        std::error_code ec;
        std::filesystem::remove(out_path, ec);
    };
    cleanup();

    {
        std::ofstream os(out_path, std::ios::binary);
        REQUIRE( os.good() );
        os << "This is not a serialized Drover archive.\n";
    }

    Drover loaded;
    std::map<std::string, std::string> invocation_metadata;
    std::list<std::filesystem::path> filenames{ out_path };

    REQUIRE( Load_From_Serialization_Files(loaded, invocation_metadata, "", filenames) );

    REQUIRE( filenames.size() == 1UL );
    CHECK( filenames.front() == out_path );
    CHECK_FALSE( loaded.Has_Contour_Data() );
    CHECK_FALSE( loaded.Has_Image_Data() );
    CHECK_FALSE( loaded.Has_Point_Data() );
    CHECK_FALSE( loaded.Has_Mesh_Data() );
    CHECK_FALSE( loaded.Has_RTPlan_Data() );
    CHECK_FALSE( loaded.Has_LSamp_Data() );

    cleanup();
}


#ifdef DCMA_USE_GNU_GSL
TEST_CASE("Kinetic model serialization round-trips 5-param linear interpolation state"){
    KineticModel_1Compartment2Input_5Param_LinearInterp_Parameters original;
    original.cAIF = make_test_samples("linear-aif");
    original.cVIF = make_test_samples("linear-vif");
    original.cROI = make_test_samples("linear-roi");
    original.FittingPerformed = true;
    original.FittingSuccess = false;
    original.RSS = std::numeric_limits<double>::quiet_NaN();
    original.k1A = 1.25;
    original.tauA = std::numeric_limits<double>::infinity();
    original.k1V = -2.5;
    original.tauV = -std::numeric_limits<double>::infinity();
    original.k2 = 0.125;

    const auto serialized = Serialize(original);
    REQUIRE_FALSE( serialized.empty() );

    KineticModel_1Compartment2Input_5Param_LinearInterp_Parameters reloaded;
    REQUIRE( Deserialize(serialized, reloaded) );

    check_samples_equal(reloaded.cAIF, original.cAIF);
    check_samples_equal(reloaded.cVIF, original.cVIF);
    check_samples_equal(reloaded.cROI, original.cROI);
    CHECK( reloaded.FittingPerformed == original.FittingPerformed );
    CHECK( reloaded.FittingSuccess == original.FittingSuccess );
    check_special_equal(reloaded.RSS, original.RSS);
    check_special_equal(reloaded.k1A, original.k1A);
    check_special_equal(reloaded.tauA, original.tauA);
    check_special_equal(reloaded.k1V, original.k1V);
    check_special_equal(reloaded.tauV, original.tauV);
    check_special_equal(reloaded.k2, original.k2);
}


TEST_CASE("Kinetic model serialization round-trips 5-param Chebyshev state"){
    KineticModel_1Compartment2Input_5Param_Chebyshev_Parameters original;
    original.cAIF = make_test_cheby(1.0);
    original.dcAIF = make_test_cheby(2.0);
    original.cVIF = make_test_cheby(3.0);
    original.dcVIF = make_test_cheby(4.0);
    original.cROI = make_test_samples("cheby-roi");
    original.FittingPerformed = true;
    original.FittingSuccess = true;
    original.RSS = 5.5;
    original.k1A = std::numeric_limits<double>::quiet_NaN();
    original.tauA = 6.5;
    original.k1V = std::numeric_limits<double>::infinity();
    original.tauV = 7.5;
    original.k2 = -std::numeric_limits<double>::infinity();
    original.ExpApproxTrunc = 12;
    original.MultiplicationCoeffTrunc = 8.0;

    const auto serialized = Serialize(original);
    REQUIRE_FALSE( serialized.empty() );

    KineticModel_1Compartment2Input_5Param_Chebyshev_Parameters reloaded;
    REQUIRE( Deserialize(serialized, reloaded) );

    check_cheby_equal(reloaded.cAIF, original.cAIF);
    check_cheby_equal(reloaded.dcAIF, original.dcAIF);
    check_cheby_equal(reloaded.cVIF, original.cVIF);
    check_cheby_equal(reloaded.dcVIF, original.dcVIF);
    check_samples_equal(reloaded.cROI, original.cROI);
    CHECK( reloaded.FittingPerformed == original.FittingPerformed );
    CHECK( reloaded.FittingSuccess == original.FittingSuccess );
    check_special_equal(reloaded.RSS, original.RSS);
    check_special_equal(reloaded.k1A, original.k1A);
    check_special_equal(reloaded.tauA, original.tauA);
    check_special_equal(reloaded.k1V, original.k1V);
    check_special_equal(reloaded.tauV, original.tauV);
    check_special_equal(reloaded.k2, original.k2);
    CHECK( reloaded.ExpApproxTrunc == original.ExpApproxTrunc );
    check_special_equal(reloaded.MultiplicationCoeffTrunc, original.MultiplicationCoeffTrunc);
}


TEST_CASE("Kinetic model serialization round-trips reduced 3-param Chebyshev state"){
    KineticModel_1Compartment2Input_Reduced3Param_Chebyshev_Parameters original;
    original.cAIF = make_test_cheby(5.0);
    original.dcAIF = make_test_cheby(6.0);
    original.cVIF = make_test_cheby(7.0);
    original.dcVIF = make_test_cheby(8.0);
    original.cROI = make_test_samples("reduced-cheby-roi");
    original.FittingPerformed = true;
    original.FittingSuccess = true;
    original.RSS = std::numeric_limits<double>::quiet_NaN();
    original.k1A = 1.0;
    original.tauA = 2.0;
    original.k1V = 3.0;
    original.tauV = std::numeric_limits<double>::infinity();
    original.k2 = -std::numeric_limits<double>::infinity();
    original.dF_dtauA = 4.0;
    original.dF_dtauV = 5.0;
    original.dF_dk2 = 6.0;
    original.S_IA_IV = 7.0;
    original.S_IA_R = 8.0;
    original.S_IV_R = 9.0;
    original.S_IA_IA = 10.0;
    original.S_IV_IV = 11.0;
    original.S_R_R = 12.0;
    original.S_R_dtauA_IA = 13.0;
    original.S_IA_dtauA_IA = 14.0;
    original.S_IV_dtauA_IA = 15.0;
    original.S_R_dtauV_IV = 16.0;
    original.S_IV_dtauV_IV = 17.0;
    original.S_IA_dtauV_IV = 18.0;
    original.S_R_dk2_IA = 19.0;
    original.S_R_dk2_IV = 20.0;
    original.S_IA_dk2_IA = 21.0;
    original.S_IV_dk2_IV = 22.0;
    original.S_IA_dk2_IV = 23.0;
    original.S_IV_dk2_IA = 24.0;
    original.ExpApproxTrunc = 14;
    original.MultiplicationCoeffTrunc = 9.0;

    const auto serialized = Serialize(original);
    REQUIRE_FALSE( serialized.empty() );

    KineticModel_1Compartment2Input_Reduced3Param_Chebyshev_Parameters reloaded;
    REQUIRE( Deserialize(serialized, reloaded) );

    check_cheby_equal(reloaded.cAIF, original.cAIF);
    check_cheby_equal(reloaded.dcAIF, original.dcAIF);
    check_cheby_equal(reloaded.cVIF, original.cVIF);
    check_cheby_equal(reloaded.dcVIF, original.dcVIF);
    check_samples_equal(reloaded.cROI, original.cROI);
    CHECK( reloaded.FittingPerformed == original.FittingPerformed );
    CHECK( reloaded.FittingSuccess == original.FittingSuccess );
    check_special_equal(reloaded.RSS, original.RSS);
    check_special_equal(reloaded.k1A, original.k1A);
    check_special_equal(reloaded.tauA, original.tauA);
    check_special_equal(reloaded.k1V, original.k1V);
    check_special_equal(reloaded.tauV, original.tauV);
    check_special_equal(reloaded.k2, original.k2);
    check_special_equal(reloaded.dF_dtauA, original.dF_dtauA);
    check_special_equal(reloaded.dF_dtauV, original.dF_dtauV);
    check_special_equal(reloaded.dF_dk2, original.dF_dk2);
    check_special_equal(reloaded.S_IA_IV, original.S_IA_IV);
    check_special_equal(reloaded.S_IA_R, original.S_IA_R);
    check_special_equal(reloaded.S_IV_R, original.S_IV_R);
    check_special_equal(reloaded.S_IA_IA, original.S_IA_IA);
    check_special_equal(reloaded.S_IV_IV, original.S_IV_IV);
    check_special_equal(reloaded.S_R_R, original.S_R_R);
    check_special_equal(reloaded.S_R_dtauA_IA, original.S_R_dtauA_IA);
    check_special_equal(reloaded.S_IA_dtauA_IA, original.S_IA_dtauA_IA);
    check_special_equal(reloaded.S_IV_dtauA_IA, original.S_IV_dtauA_IA);
    check_special_equal(reloaded.S_R_dtauV_IV, original.S_R_dtauV_IV);
    check_special_equal(reloaded.S_IV_dtauV_IV, original.S_IV_dtauV_IV);
    check_special_equal(reloaded.S_IA_dtauV_IV, original.S_IA_dtauV_IV);
    check_special_equal(reloaded.S_R_dk2_IA, original.S_R_dk2_IA);
    check_special_equal(reloaded.S_R_dk2_IV, original.S_R_dk2_IV);
    check_special_equal(reloaded.S_IA_dk2_IA, original.S_IA_dk2_IA);
    check_special_equal(reloaded.S_IV_dk2_IV, original.S_IV_dk2_IV);
    check_special_equal(reloaded.S_IA_dk2_IV, original.S_IA_dk2_IV);
    check_special_equal(reloaded.S_IV_dk2_IA, original.S_IV_dk2_IA);
    CHECK( reloaded.ExpApproxTrunc == original.ExpApproxTrunc );
    check_special_equal(reloaded.MultiplicationCoeffTrunc, original.MultiplicationCoeffTrunc);
}
#endif


TEST_CASE("SerializeDrover operation preserves selected components only"){
    const auto out_path = std::filesystem::temp_directory_path()
                        / "dicomautomaton_serializedrover_component_selection.xml.gz";

    auto point_cloud = std::make_shared<Point_Cloud>();
    point_cloud->pset.points.emplace_back(1.0, 2.0, 3.0);
    point_cloud->pset.metadata["Description"] = "operation-component-selection-point-cloud";

    contour_of_points<double> contour;
    contour.closed = true;
    contour.points.emplace_back(0.0, 0.0, 0.0);
    contour.points.emplace_back(1.0, 0.0, 0.0);

    contour_collection<double> contour_collection;
    contour_collection.contours.emplace_back(contour);

    Drover original;
    original.point_data.emplace_back(point_cloud);
    original.contour_data = std::make_shared<Contour_Data>();
    original.contour_data->ccs.emplace_back(contour_collection);

    const auto cleanup = [&](){
        std::error_code ec;
        std::filesystem::remove(out_path, ec);
    };
    cleanup();

    OperationArgPkg opt_args("SerializeDrover");
    REQUIRE( opt_args.insert("Filename", out_path.string()) );
    REQUIRE( opt_args.insert("Components", "pointclouds") );
    std::map<std::string, std::string> invocation_metadata;

    REQUIRE( SerializeDrover(original, opt_args, invocation_metadata, "") );

    Drover reloaded;
    REQUIRE( Common_Deserialize_Drover(reloaded, out_path) );

    REQUIRE( reloaded.Has_Point_Data() );
    REQUIRE( reloaded.point_data.size() == 1UL );
    REQUIRE( reloaded.point_data.front() != nullptr );
    CHECK( reloaded.point_data.front()->pset == point_cloud->pset );
    CHECK_FALSE( reloaded.Has_Contour_Data() );
    CHECK_FALSE( reloaded.Has_Image_Data() );
    CHECK_FALSE( reloaded.Has_Mesh_Data() );
    CHECK_FALSE( reloaded.Has_RTPlan_Data() );

    cleanup();
}
