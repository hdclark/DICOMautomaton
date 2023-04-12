// RPC.thrift - A part of DICOMautomaton 2023. Written by hal clark.

namespace cpp dcma.rpc
namespace py dcma.rpc


// --------------------------------------------------------------------
// Ygor classes -- YgorMath.h.
// --------------------------------------------------------------------
typedef map<string, string> metadata_t;

struct vec3_double {
    1: required double x;
    2: required double y;
    3: required double z;
}
struct contour_of_points_double {
    1: required list<vec3_double> points;
    2: required bool closed;
    3: required metadata_t metadata;
}
struct contour_collection_double {
    1: required list<contour_of_points_double> contours;
}
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

// --------------------------------------------------------------------
// Ygor classes -- YgorImages.h.
// --------------------------------------------------------------------
struct planar_image_double_double {
    1: required list<double> data;  // NOTE: float not supported, so using double here.
    2: required i64 rows;
    3: required i64 columns;
    4: required i64 channels;
    5: required double pxl_dx;
    6: required double pxl_dy;
    7: required double pxl_dz;
    8: required vec3_double anchor;
    9: required vec3_double offset;
    10: required vec3_double row_unit;
    11: required vec3_double col_unit;
    12: required metadata_t metadata;
}
struct planar_image_collection_double_double {
    1: required list<planar_image_double_double> images; // NOTE: for <float,double>, but float not available.
}

// --------------------------------------------------------------------
// DICOMautomaton classes -- Tables.h.
// --------------------------------------------------------------------
struct cell_string {
    // TODO:
    //class cell<T> {
    //    private:
    //        int64_t row;
    //        int64_t col;
    //
    //    public:
    //        T val;
}
struct table2 {
    // TODO:
    //std::set< cell<std::string> > data;
    //std::map<std::string, std::string> metadata;
}

// --------------------------------------------------------------------
// DICOMautomaton classes -- Structs.h.
// --------------------------------------------------------------------
struct Contour_Data {
    1: required list<contour_collection_double> ccs;
}
struct Image_Array {
    1: required planar_image_collection_double_double imagecoll; // NOTE: for <float,double>, but float not available.
    2: required string filename;
}
struct Point_Cloud {
    1: required point_set_double pset;
}
struct Surface_Mesh {
    1: required fv_surface_mesh_double_int64 meshes;

    // TODO:
    //2: optional map<string, list<byte>> vertex_attributes; // bytes should be std::any
    //3: optional map<string, list<byte>> face_attributes;   // bytes should be std::any
}
struct Static_Machine_State {
    1: required double CumulativeMetersetWeight;
    2: required i64 ControlPointIndex;

    3: required double GantryAngle;
    4: required double GantryRotationDirection;

    5: required double BeamLimitingDeviceAngle;
    6: required double BeamLimitingDeviceRotationDirection;

    7: required double PatientSupportAngle;
    8: required double PatientSupportRotationDirection;

    9: required double TableTopEccentricAngle;
    10: required double TableTopEccentricRotationDirection;

    11: required double TableTopVerticalPosition;
    12: required double TableTopLongitudinalPosition;
    13: required double TableTopLateralPosition;

    14: required double TableTopPitchAngle;
    15: required double TableTopPitchRotationDirection;

    16: required double TableTopRollAngle;
    17: required double TableTopRollRotationDirection;

    18: required vec3_double IsocentrePosition;

    19: required list<double> JawPositionsX;
    20: required list<double> JawPositionsY;

    21: required list<double> MLCPositionsX;

    22: required metadata_t metadata;
}
struct Dynamic_Machine_State {
    1: required i64 BeamNumber;
    2: required double FinalCumulativeMetersetWeight;
    3: required list<Static_Machine_State> static_states;
    4: required metadata_t metadata;
}
struct RTPlan {
    1: required list<Dynamic_Machine_State> dynamic_states;
    2: required metadata_t metadata;
}
struct Line_Sample {
    1: required samples_1D_double line;
}
struct Transform3 {
    // TODO:
    //std::variant< std::monostate,
    //              affine_transform<double>,
    //              thin_plate_spline,
    //              deformation_field > transform;
    //std::map< std::string, std::string > metadata; //User-defined metadata.
}
struct Sparse_Table {
    1: required table2 table;
}
struct Drover {
    1: optional list<Contour_Data> contour_data; // NOTE: harmonized in anticipation of future change.
    2: optional list<Image_Array>  image_data;
    3: optional list<Point_Cloud>  point_data;
    4: optional list<Surface_Mesh> smesh_data;
    5: optional list<RTPlan>       rtplan_data;
    6: optional list<Line_Sample>  lsamp_data;
    7: optional list<Transform3>   trans_data;
    8: optional list<Sparse_Table> table_data;
}



// --------------------------------------------------------------------
// DICOMautomaton RPC methods.
// --------------------------------------------------------------------
struct OperationsQuery {
    // Intentionally empty.
}
struct KnownOperation {
    1: required string name;
}


struct LoadFilesQuery {
    1: required string server_filename;
}
struct LoadFilesResponse {
    1: required bool success;
    2: optional Drover drover;
}


struct ExecuteScriptQuery{
    1: required Drover drover;
    2: required metadata_t invocation_metadata;
    3: required string filename_lex;
}
struct ExecuteScriptResponse {
    1: required bool success;
    2: optional Drover drover;
    3: optional metadata_t invocation_metadata;
    4: optional string filename_lex;
}


service Receiver {
    // Level 1 interface: high-level 'config-query' interface.
    //
    // Report available DCMA operations provided this server.
    list<KnownOperation>
    GetSupportedOperations(
        1: OperationsQuery query
    );

    // Level 2 interface: Drover interchange.
    //
    // Load files into a Drover RPC-proxy object.
    LoadFilesResponse
    LoadFiles(1: list<LoadFilesQuery> server_filenames);

    // Transfer a Drover object, execute a script, and receive the Drover back.
    ExecuteScriptResponse
    ExecuteScript(1: ExecuteScriptQuery query,
                  2: string script);

    // Level 3 interface: Drover operations.
    //
    // ...
}

