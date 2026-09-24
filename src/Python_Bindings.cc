//Python_Bindings.cc.

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iterator>
#include <limits>
#include <list>
#include <locale>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "Python_Bindings.h"
#include "Process_Fork.h"
#include "Session.h"

namespace py = pybind11;

namespace dcma {
namespace python {

Drover & DataView::data() const {
    if(session){
        return session->data();
    }
    if(!embedded || !embedded->active || !embedded->data){
        throw std::runtime_error("embedded Python operation session is no longer active");
    }
    return *embedded->data;
}

void EmbeddedSession::require_active() const {
    if(!state || !state->active || !state->data || !state->metadata || !state->lexicon_filename){
        throw std::runtime_error("embedded Python operation session is no longer active");
    }
}

} // namespace python
} // namespace dcma

namespace {

using dcma::python::DataView;
using dcma::python::EmbeddedSession;

std::string python_scalar_to_string(const py::handle &value){
    if(py::isinstance<py::str>(value)){
        return py::cast<std::string>(value);
    }
    if(py::isinstance<py::bool_>(value)){
        return py::cast<bool>(value) ? "true" : "false";
    }
    if(py::isinstance<py::int_>(value)){
        return py::str(value).cast<std::string>();
    }
    if(py::isinstance<py::float_>(value)){
        std::ostringstream os;
        os.imbue(std::locale::classic());
        os << std::setprecision(std::numeric_limits<double>::max_digits10)
           << py::cast<double>(value);
        return os.str();
    }
    throw py::type_error("operation arguments must be str, bool, int, or float");
}

OperationArgPkg make_operation(const std::string &name, const py::dict &arguments){
    OperationArgPkg operation(name);
    for(const auto &item : arguments){
        if(!py::isinstance<py::str>(item.first)){
            throw py::type_error("operation argument names must be strings");
        }
        const auto key = py::cast<std::string>(item.first);
        if(!operation.insert(key, python_scalar_to_string(item.second))){
            throw py::value_error("duplicate operation argument '" + key + "'");
        }
    }
    return operation;
}

std::string script_feedback_message(const dcma::Session::feedback_list_t &feedback){
    std::ostringstream os;
    bool first = true;
    for(const auto &entry : feedback){
        if(entry.severity != script_feedback_severity_t::err){
            continue;
        }
        if(!first){
            os << "; ";
        }
        if(0 <= entry.line){
            os << "line " << entry.line << ": ";
        }
        os << entry.message;
        first = false;
    }
    return first ? "script parsing or execution failed" : os.str();
}

struct ImageSnapshot {
    planar_image<float, double> image;
    std::string filename;
};

struct ContourSnapshot {
    contour_of_points<double> contour;
};

struct PointCloudSnapshot {
    point_set<double> points;
};

struct SurfaceMeshSnapshot {
    Surface_Mesh mesh;
};

struct StaticMachineStateSnapshot {
    Static_Machine_State state;
};

struct DynamicMachineStateSnapshot {
    Dynamic_Machine_State state;
};

struct RTPlanSnapshot {
    RTPlan plan;
};

struct LineSampleSnapshot {
    samples_1D<double> line;
};

struct SparseTableSnapshot {
    tables::table2 table;
};

struct TransformSnapshot {
    Transform3 transform;
};

Image_Array & get_image_array(DataView &view, const py::ssize_t array_index){
    auto &image_data = view.data().image_data;
    if((array_index < 0) || (image_data.size() <= static_cast<std::size_t>(array_index))){
        throw py::index_error("image array index out of range");
    }
    auto iter = image_data.begin();
    std::advance(iter, array_index);
    if(!(*iter)){
        throw std::runtime_error("native image array is null");
    }
    return *(*iter);
}

planar_image<float, double> & get_image(DataView &view,
                                         const py::ssize_t array_index,
                                         const py::ssize_t image_index){
    auto &images = get_image_array(view, array_index).imagecoll.images;
    if((image_index < 0) || (images.size() <= static_cast<std::size_t>(image_index))){
        throw py::index_error("image index out of range");
    }
    auto iter = images.begin();
    std::advance(iter, image_index);
    return *iter;
}

contour_collection<double> & get_contour_collection(DataView &view,
                                                      const py::ssize_t collection_index){
    auto &contour_data = view.data().contour_data;
    if(!contour_data){
        throw py::index_error("contour collection index out of range");
    }
    auto &collections = contour_data->ccs;
    if((collection_index < 0) || (collections.size() <= static_cast<std::size_t>(collection_index))){
        throw py::index_error("contour collection index out of range");
    }
    auto iter = collections.begin();
    std::advance(iter, collection_index);
    return *iter;
}

contour_of_points<double> & get_contour(DataView &view,
                                         const py::ssize_t collection_index,
                                         const py::ssize_t contour_index){
    auto &contours = get_contour_collection(view, collection_index).contours;
    if((contour_index < 0) || (contours.size() <= static_cast<std::size_t>(contour_index))){
        throw py::index_error("contour index out of range");
    }
    auto iter = contours.begin();
    std::advance(iter, contour_index);
    return *iter;
}

Point_Cloud & get_point_cloud(DataView &view, const py::ssize_t point_cloud_index){
    auto &point_data = view.data().point_data;
    if((point_cloud_index < 0) || (point_data.size() <= static_cast<std::size_t>(point_cloud_index))){
        throw py::index_error("point cloud index out of range");
    }
    auto iter = point_data.begin();
    std::advance(iter, point_cloud_index);
    if(!(*iter)){
        throw std::runtime_error("native point cloud is null");
    }
    return *(*iter);
}

Surface_Mesh & get_surface_mesh(DataView &view, const py::ssize_t mesh_index){
    auto &meshes = view.data().smesh_data;
    if((mesh_index < 0) || (meshes.size() <= static_cast<std::size_t>(mesh_index))){
        throw py::index_error("surface mesh index out of range");
    }
    auto iter = meshes.begin();
    std::advance(iter, mesh_index);
    if(!(*iter)){
        throw std::runtime_error("native surface mesh is null");
    }
    return *(*iter);
}

RTPlan & get_rtplan(DataView &view, const py::ssize_t rtplan_index){
    auto &rtplans = view.data().rtplan_data;
    if((rtplan_index < 0) || (rtplans.size() <= static_cast<std::size_t>(rtplan_index))){
        throw py::index_error("RT plan index out of range");
    }
    auto iter = rtplans.begin();
    std::advance(iter, rtplan_index);
    if(!(*iter)){
        throw std::runtime_error("native RT plan is null");
    }
    return *(*iter);
}

Line_Sample & get_line_sample(DataView &view, const py::ssize_t line_sample_index){
    auto &line_samples = view.data().lsamp_data;
    if((line_sample_index < 0) || (line_samples.size() <= static_cast<std::size_t>(line_sample_index))){
        throw py::index_error("line sample index out of range");
    }
    auto iter = line_samples.begin();
    std::advance(iter, line_sample_index);
    if(!(*iter)){
        throw std::runtime_error("native line sample is null");
    }
    return *(*iter);
}

Sparse_Table & get_sparse_table(DataView &view, const py::ssize_t table_index){
    auto &tables = view.data().table_data;
    if((table_index < 0) || (tables.size() <= static_cast<std::size_t>(table_index))){
        throw py::index_error("table index out of range");
    }
    auto iter = tables.begin();
    std::advance(iter, table_index);
    if(!(*iter)){
        throw std::runtime_error("native sparse table is null");
    }
    return *(*iter);
}

Transform3 & get_transform(DataView &view, const py::ssize_t transform_index){
    auto &transforms = view.data().trans_data;
    if((transform_index < 0) || (transforms.size() <= static_cast<std::size_t>(transform_index))){
        throw py::index_error("transform index out of range");
    }
    auto iter = transforms.begin();
    std::advance(iter, transform_index);
    if(!(*iter)){
        throw std::runtime_error("native transform is null");
    }
    return *(*iter);
}

py::array_t<double> vec3s_to_numpy(const std::vector<vec3<double>> &values){
    if(static_cast<std::size_t>(std::numeric_limits<py::ssize_t>::max()) < values.size()){
        throw std::runtime_error("native vector size exceeds NumPy index limits");
    }
    std::vector<py::ssize_t> shape{static_cast<py::ssize_t>(values.size()), 3};
    py::array_t<double> result(shape);
    auto *destination = result.mutable_data();
    for(const auto &value : values){
        *(destination++) = value.x;
        *(destination++) = value.y;
        *(destination++) = value.z;
    }
    return result;
}

std::vector<vec3<double>> numpy_to_vec3s(const py::array &values, const std::string &name){
    auto converted = py::array_t<double, py::array::c_style | py::array::forcecast>::ensure(values);
    if(!converted){
        throw py::type_error(name + " must be convertible to a C-contiguous float64 NumPy array");
    }
    if((converted.ndim() != 2) || (converted.shape(1) != 3)){
        throw py::value_error(name + " must have shape (item, xyz)");
    }
    std::vector<vec3<double>> replacement;
    replacement.reserve(static_cast<std::size_t>(converted.shape(0)));
    const auto *source = converted.data();
    for(py::ssize_t i = 0; i < converted.shape(0); ++i){
        replacement.emplace_back(source[0], source[1], source[2]);
        source += 3;
    }
    return replacement;
}

std::size_t validated_pixel_count(const planar_image<float, double> &image){
    if((image.rows <= 0) || (image.columns <= 0) || (image.channels <= 0)){
        throw std::runtime_error("native image has invalid dimensions");
    }
    const auto rows = static_cast<std::size_t>(image.rows);
    const auto columns = static_cast<std::size_t>(image.columns);
    const auto channels = static_cast<std::size_t>(image.channels);
    if((std::numeric_limits<py::ssize_t>::max() < image.rows)
    || (std::numeric_limits<py::ssize_t>::max() < image.columns)
    || (std::numeric_limits<py::ssize_t>::max() < image.channels)){
        throw std::runtime_error("native image dimensions exceed NumPy index limits");
    }
    if((std::numeric_limits<std::size_t>::max() / rows < columns)
    || (std::numeric_limits<std::size_t>::max() / (rows * columns) < channels)){
        throw std::runtime_error("native image dimensions overflow addressable storage");
    }
    const auto count = rows * columns * channels;
    if(image.data.size() != count){
        throw std::runtime_error("native image dimensions do not match its pixel buffer");
    }
    return count;
}

std::tuple<double, double, double> as_tuple(const vec3<double> &value){
    return std::make_tuple(value.x, value.y, value.z);
}

py::array_t<double> doubles_to_numpy(const std::vector<double> &values){
    if(static_cast<std::size_t>(std::numeric_limits<py::ssize_t>::max()) < values.size()){
        throw std::runtime_error("native vector size exceeds NumPy index limits");
    }
    py::array_t<double> result(static_cast<py::ssize_t>(values.size()));
    std::copy(values.cbegin(), values.cend(), result.mutable_data());
    return result;
}

py::handle required_dict_value(const py::dict &values,
                               const char *key,
                               const std::string &context){
    auto *item = PyDict_GetItemString(values.ptr(), key);
    if(item == nullptr){
        throw py::key_error(context + " is missing required key '" + key + "'");
    }
    return py::handle(item);
}

double dict_double(const py::dict &values, const char *key, const std::string &context){
    const auto value = required_dict_value(values, key, context);
    if(py::isinstance<py::bool_>(value)
    || (!py::isinstance<py::int_>(value) && !py::isinstance<py::float_>(value))){
        throw py::type_error(context + " key '" + key + "' must be a float");
    }
    return py::cast<double>(value);
}

std::int64_t dict_int64(const py::dict &values, const char *key, const std::string &context){
    const auto value = required_dict_value(values, key, context);
    if(!py::isinstance<py::int_>(value) || py::isinstance<py::bool_>(value)){
        throw py::type_error(context + " key '" + key + "' must be an integer");
    }
    try{
        return py::cast<std::int64_t>(value);
    }catch(const py::cast_error &){
        throw py::value_error(context + " key '" + key + "' is outside the int64 range");
    }
}

std::map<std::string, std::string> dict_metadata(const py::dict &values,
                                                  const char *key,
                                                  const std::string &context){
    try{
        return py::cast<std::map<std::string, std::string>>(required_dict_value(values, key, context));
    }catch(const py::cast_error &){
        throw py::type_error(context + " key '" + key + "' must be a dict[str, str]");
    }
}

std::vector<std::uint32_t> dict_colours(const py::dict &values,
                                        const char *key,
                                        const std::string &context){
    auto converted = py::array_t<std::uint32_t, py::array::c_style | py::array::forcecast>::ensure(
        required_dict_value(values, key, context));
    if(!converted){
        throw py::type_error(context + " key '" + key + "' must be convertible to a uint32 NumPy array");
    }
    if(converted.ndim() != 1){
        throw py::value_error(context + " key '" + key + "' must have shape (point,)");
    }
    return std::vector<std::uint32_t>(converted.data(), converted.data() + converted.size());
}

std::vector<double> dict_double_vector(const py::dict &values,
                                       const char *key,
                                       const std::string &context){
    auto converted = py::array_t<double, py::array::c_style | py::array::forcecast>::ensure(
        required_dict_value(values, key, context));
    if(!converted){
        throw py::type_error(context + " key '" + key + "' must be convertible to a float64 NumPy array");
    }
    if(converted.ndim() != 1){
        throw py::value_error(context + " key '" + key + "' must have shape (value,)");
    }
    return std::vector<double>(converted.data(), converted.data() + converted.size());
}

vec3<double> dict_vec3(const py::dict &values, const char *key, const std::string &context){
    auto converted = py::array_t<double, py::array::c_style | py::array::forcecast>::ensure(
        required_dict_value(values, key, context));
    if(!converted){
        throw py::type_error(context + " key '" + key + "' must be convertible to a float64 NumPy array");
    }
    if((converted.ndim() != 1) || (converted.shape(0) != 3)){
        throw py::value_error(context + " key '" + key + "' must have shape (xyz,)");
    }
    return vec3<double>(converted.data()[0], converted.data()[1], converted.data()[2]);
}

Static_Machine_State parse_static_machine_state(const py::handle &value,
                                                 const std::string &context){
    if(!py::isinstance<py::dict>(value)){
        throw py::type_error(context + " must be a dict");
    }
    const auto values = py::reinterpret_borrow<py::dict>(value);
    Static_Machine_State state;
    state.CumulativeMetersetWeight = dict_double(values, "cumulative_meterset_weight", context);
    state.ControlPointIndex = dict_int64(values, "control_point_index", context);
    state.GantryAngle = dict_double(values, "gantry_angle", context);
    state.GantryRotationDirection = dict_double(values, "gantry_rotation_direction", context);
    state.BeamLimitingDeviceAngle = dict_double(values, "beam_limiting_device_angle", context);
    state.BeamLimitingDeviceRotationDirection = dict_double(values, "beam_limiting_device_rotation_direction", context);
    state.PatientSupportAngle = dict_double(values, "patient_support_angle", context);
    state.PatientSupportRotationDirection = dict_double(values, "patient_support_rotation_direction", context);
    state.TableTopEccentricAngle = dict_double(values, "table_top_eccentric_angle", context);
    state.TableTopEccentricRotationDirection = dict_double(values, "table_top_eccentric_rotation_direction", context);
    state.TableTopVerticalPosition = dict_double(values, "table_top_vertical_position", context);
    state.TableTopLongitudinalPosition = dict_double(values, "table_top_longitudinal_position", context);
    state.TableTopLateralPosition = dict_double(values, "table_top_lateral_position", context);
    state.TableTopPitchAngle = dict_double(values, "table_top_pitch_angle", context);
    state.TableTopPitchRotationDirection = dict_double(values, "table_top_pitch_rotation_direction", context);
    state.TableTopRollAngle = dict_double(values, "table_top_roll_angle", context);
    state.TableTopRollRotationDirection = dict_double(values, "table_top_roll_rotation_direction", context);
    state.IsocentrePosition = dict_vec3(values, "isocentre_position", context);
    state.JawPositionsX = dict_double_vector(values, "jaw_positions_x", context);
    state.JawPositionsY = dict_double_vector(values, "jaw_positions_y", context);
    state.MLCPositionsX = dict_double_vector(values, "mlc_positions_x", context);
    state.metadata = dict_metadata(values, "metadata", context);
    return state;
}

Dynamic_Machine_State parse_dynamic_machine_state(const py::handle &value,
                                                   const std::size_t beam_index){
    const auto context = "beam " + std::to_string(beam_index);
    if(!py::isinstance<py::dict>(value)){
        throw py::type_error(context + " must be a dict");
    }
    const auto values = py::reinterpret_borrow<py::dict>(value);
    Dynamic_Machine_State state;
    state.BeamNumber = dict_int64(values, "beam_number", context);
    state.FinalCumulativeMetersetWeight = dict_double(values, "final_cumulative_meterset_weight", context);
    state.metadata = dict_metadata(values, "metadata", context);

    const auto static_values = required_dict_value(values, "static_states", context);
    if(!py::isinstance<py::list>(static_values) && !py::isinstance<py::tuple>(static_values)){
        throw py::type_error(context + " key 'static_states' must be a list or tuple");
    }
    std::size_t state_index = 0;
    for(const auto &static_value : py::reinterpret_borrow<py::sequence>(static_values)){
        state.static_states.emplace_back(parse_static_machine_state(
            static_value, context + " control point " + std::to_string(state_index)));
        ++state_index;
    }
    return state;
}

py::array_t<double> affine_to_numpy(const affine_transform<double> &affine){
    py::array_t<double> result(std::vector<py::ssize_t>{4, 4});
    auto *destination = result.mutable_data();
    for(std::int64_t row = 0; row < 4; ++row){
        for(std::int64_t column = 0; column < 4; ++column){
            *(destination++) = affine.read_coeff(row, column);
        }
    }
    return result;
}

affine_transform<double> parse_affine(const py::dict &payload){
    auto matrix = py::array_t<double, py::array::c_style | py::array::forcecast>::ensure(
        required_dict_value(payload, "matrix", "affine payload"));
    if(!matrix){
        throw py::type_error("affine payload key 'matrix' must be convertible to a float64 NumPy array");
    }
    if((matrix.ndim() != 2) || (matrix.shape(0) != 4) || (matrix.shape(1) != 4)){
        throw py::value_error("affine matrix must have shape (4, 4)");
    }
    const auto *source = matrix.data();
    if((source[12] != 0.0) || (source[13] != 0.0) || (source[14] != 0.0) || (source[15] != 1.0)){
        throw py::value_error("affine matrix bottom row must be (0, 0, 0, 1)");
    }
    affine_transform<double> affine;
    for(std::int64_t row = 0; row < 3; ++row){
        for(std::int64_t column = 0; column < 4; ++column){
            affine.coeff(row, column) = source[(row * 4) + column];
        }
    }
    return affine;
}

py::array_t<double> num_array_to_numpy(const num_array<double> &values){
    const auto rows = values.num_rows();
    const auto columns = values.num_cols();
    if((rows < 0) || (columns < 0)
    || (std::numeric_limits<py::ssize_t>::max() < rows)
    || (std::numeric_limits<py::ssize_t>::max() < columns)){
        throw std::runtime_error("native matrix dimensions exceed NumPy index limits");
    }
    py::array_t<double> result(std::vector<py::ssize_t>{
        static_cast<py::ssize_t>(rows), static_cast<py::ssize_t>(columns)
    });
    auto *destination = result.mutable_data();
    for(std::int64_t row = 0; row < rows; ++row){
        for(std::int64_t column = 0; column < columns; ++column){
            *(destination++) = values.read_coeff(row, column);
        }
    }
    return result;
}

py::array_t<std::uint32_t> colours_to_numpy(const std::vector<std::uint32_t> &colours){
    if(static_cast<std::size_t>(std::numeric_limits<py::ssize_t>::max()) < colours.size()){
        throw std::runtime_error("native colour count exceeds NumPy index limits");
    }
    py::array_t<std::uint32_t> result(static_cast<py::ssize_t>(colours.size()));
    std::copy(colours.cbegin(), colours.cend(), result.mutable_data());
    return result;
}

py::array_t<double> deformation_image_to_numpy(const planar_image<double, double> &image){
    if((image.rows <= 0) || (image.columns <= 0) || (image.channels != 3)
    || (std::numeric_limits<py::ssize_t>::max() < image.rows)
    || (std::numeric_limits<py::ssize_t>::max() < image.columns)){
        throw std::runtime_error("native deformation image has invalid dimensions");
    }
    const auto rows = static_cast<std::size_t>(image.rows);
    const auto columns = static_cast<std::size_t>(image.columns);
    if(std::numeric_limits<std::size_t>::max() / rows < columns
    || std::numeric_limits<std::size_t>::max() / (rows * columns) < 3){
        throw std::runtime_error("native deformation image dimensions overflow addressable storage");
    }
    const auto count = rows * columns * 3;
    if(image.data.size() != count){
        throw std::runtime_error("native deformation image dimensions do not match its buffer");
    }
    py::array_t<double> result(std::vector<py::ssize_t>{
        static_cast<py::ssize_t>(image.rows),
        static_cast<py::ssize_t>(image.columns),
        3
    });
    std::copy_n(image.data.cbegin(), count, result.mutable_data());
    return result;
}

py::dict transform_payload(const Transform3 &transform){
    py::dict payload;
    if(const auto *affine = std::get_if<affine_transform<double>>(&transform.transform)){
        payload["matrix"] = affine_to_numpy(*affine);
    }else if(const auto *tps = std::get_if<thin_plate_spline>(&transform.transform)){
        payload["control_points"] = vec3s_to_numpy(tps->control_points.points);
        payload["control_point_normals"] = vec3s_to_numpy(tps->control_points.normals);
        payload["control_point_colours"] = colours_to_numpy(tps->control_points.colours);
        payload["control_point_metadata"] = tps->control_points.metadata;
        payload["kernel_dimension"] = tps->kernel_dimension;
        payload["coefficients"] = num_array_to_numpy(tps->W_A);
    }else if(const auto *field = std::get_if<deformation_field>(&transform.transform)){
        py::list images;
        for(const auto &image : field->get_imagecoll_crefw().get().images){
            py::dict item;
            item["values"] = deformation_image_to_numpy(image);
            item["spacing"] = py::make_tuple(image.pxl_dx, image.pxl_dy, image.pxl_dz);
            item["anchor"] = as_tuple(image.anchor);
            item["offset"] = as_tuple(image.offset);
            item["row_direction"] = as_tuple(image.row_unit);
            item["column_direction"] = as_tuple(image.col_unit);
            item["metadata"] = image.metadata;
            images.append(std::move(item));
        }
        payload["images"] = std::move(images);
    }
    return payload;
}

std::string transform_kind(const Transform3 &transform){
    if(std::holds_alternative<std::monostate>(transform.transform)) return "disengaged";
    if(std::holds_alternative<affine_transform<double>>(transform.transform)) return "affine";
    if(std::holds_alternative<thin_plate_spline>(transform.transform)) return "thin_plate_spline";
    if(std::holds_alternative<deformation_field>(transform.transform)) return "deformation_field";
    throw std::logic_error("native transform has an unknown variant");
}

thin_plate_spline parse_tps(const py::dict &payload){
    point_set<double> control_points;
    const auto points_value = required_dict_value(payload, "control_points", "thin-plate-spline payload");
    if(!py::isinstance<py::array>(points_value)){
        throw py::type_error("thin-plate-spline control_points must be a NumPy array");
    }
    control_points.points = numpy_to_vec3s(py::reinterpret_borrow<py::array>(points_value), "control_points");
    if(control_points.points.empty()){
        throw py::value_error("thin-plate-spline control_points must not be empty");
    }
    const auto normals_value = required_dict_value(payload, "control_point_normals", "thin-plate-spline payload");
    if(!py::isinstance<py::array>(normals_value)){
        throw py::type_error("thin-plate-spline control_point_normals must be a NumPy array");
    }
    control_points.normals = numpy_to_vec3s(
        py::reinterpret_borrow<py::array>(normals_value), "control_point_normals");
    control_points.colours = dict_colours(payload, "control_point_colours", "thin-plate-spline payload");
    control_points.metadata = dict_metadata(payload, "control_point_metadata", "thin-plate-spline payload");
    if((!control_points.normals.empty() && (control_points.normals.size() != control_points.points.size()))
    || (!control_points.colours.empty() && (control_points.colours.size() != control_points.points.size()))){
        throw py::value_error("thin-plate-spline normals and colours must be empty or match the control-point count");
    }

    const auto kernel_dimension = dict_int64(payload, "kernel_dimension", "thin-plate-spline payload");
    if((kernel_dimension != 2) && (kernel_dimension != 3)){
        throw py::value_error("thin-plate-spline kernel_dimension must be 2 or 3");
    }
    auto coefficients = py::array_t<double, py::array::c_style | py::array::forcecast>::ensure(
        required_dict_value(payload, "coefficients", "thin-plate-spline payload"));
    if(!coefficients){
        throw py::type_error("thin-plate-spline coefficients must be convertible to a float64 NumPy array");
    }
    if(control_points.points.size() > static_cast<std::size_t>(std::numeric_limits<std::int64_t>::max() - 4)){
        throw py::value_error("thin-plate-spline control-point count exceeds native limits");
    }
    const auto rows = static_cast<std::int64_t>(control_points.points.size()) + 4;
    if((coefficients.ndim() != 2) || (coefficients.shape(0) != rows) || (coefficients.shape(1) != 3)){
        throw py::value_error("thin-plate-spline coefficients must have shape (control_points + 4, 3)");
    }

    thin_plate_spline tps(control_points, kernel_dimension);
    const auto *source = coefficients.data();
    for(std::int64_t row = 0; row < rows; ++row){
        for(std::int64_t column = 0; column < 3; ++column){
            tps.W_A.coeff(row, column) = *(source++);
        }
    }
    return tps;
}

deformation_field parse_deformation_field(const py::dict &payload){
    const auto images_value = required_dict_value(payload, "images", "deformation-field payload");
    if(!py::isinstance<py::list>(images_value) && !py::isinstance<py::tuple>(images_value)){
        throw py::type_error("deformation-field images must be a list or tuple");
    }
    planar_image_collection<double, double> collection;
    std::size_t image_index = 0;
    for(const auto &image_value : py::reinterpret_borrow<py::sequence>(images_value)){
        const auto context = "deformation-field image " + std::to_string(image_index);
        if(!py::isinstance<py::dict>(image_value)){
            throw py::type_error(context + " must be a dict");
        }
        const auto values = py::reinterpret_borrow<py::dict>(image_value);
        auto pixels = py::array_t<double, py::array::c_style | py::array::forcecast>::ensure(
            required_dict_value(values, "values", context));
        if(!pixels){
            throw py::type_error(context + " values must be convertible to a float64 NumPy array");
        }
        if((pixels.ndim() != 3) || (pixels.shape(2) != 3)){
            throw py::value_error(context + " values must have shape (rows, columns, 3)");
        }
        if((pixels.shape(0) <= 0) || (pixels.shape(1) <= 0)){
            throw py::value_error(context + " values must have non-empty row and column dimensions");
        }
        const auto spacing = dict_vec3(values, "spacing", context);
        planar_image<double, double> image;
        image.init_orientation(dict_vec3(values, "row_direction", context),
                               dict_vec3(values, "column_direction", context));
        image.init_buffer(pixels.shape(0), pixels.shape(1), 3);
        image.init_spatial(spacing.x, spacing.y, spacing.z,
                           dict_vec3(values, "anchor", context), dict_vec3(values, "offset", context));
        std::copy(pixels.data(), pixels.data() + pixels.size(), image.data.begin());
        image.metadata = dict_metadata(values, "metadata", context);
        collection.images.emplace_back(std::move(image));
        ++image_index;
    }
    return deformation_field(std::move(collection));
}

Transform3 parse_transform(const std::string &kind,
                           const py::dict &payload,
                           std::map<std::string, std::string> metadata){
    Transform3 replacement;
    if(kind == "disengaged"){
        if(py::len(payload) != 0){
            throw py::value_error("disengaged transform payload must be empty");
        }
    }else if(kind == "affine"){
        replacement.transform = parse_affine(payload);
    }else if(kind == "thin_plate_spline"){
        replacement.transform = parse_tps(payload);
    }else if(kind == "deformation_field"){
        replacement.transform = parse_deformation_field(payload);
    }else{
        throw py::value_error("transform kind must be disengaged, affine, thin_plate_spline, or deformation_field");
    }
    replacement.metadata = std::move(metadata);
    return replacement;
}

} // namespace

namespace dcma {
namespace python {

void BindModule(py::module &module){
    DisableForkForPythonInitialization();

    module.doc() = "Native DICOMautomaton session bindings";

    py::enum_<OpArgVisibility>(module, "OpArgVisibility")
        .value("SHOW", OpArgVisibility::Show)
        .value("HIDE", OpArgVisibility::Hide);

    py::enum_<OpArgFlow>(module, "OpArgFlow")
        .value("INGRESS", OpArgFlow::Ingress)
        .value("EGRESS", OpArgFlow::Egress)
        .value("INGRESS_EGRESS", OpArgFlow::IngressEgress)
        .value("UNKNOWN", OpArgFlow::Unknown);

    py::enum_<OpArgSamples>(module, "OpArgSamples")
        .value("EXAMPLES", OpArgSamples::Examples)
        .value("EXHAUSTIVE", OpArgSamples::Exhaustive);

    py::class_<OperationArgDoc>(module, "OperationArgDoc")
        .def_readonly("name", &OperationArgDoc::name)
        .def_readonly("description", &OperationArgDoc::desc)
        .def_readonly("default_value", &OperationArgDoc::default_val)
        .def_readonly("expected", &OperationArgDoc::expected)
        .def_readonly("examples", &OperationArgDoc::examples)
        .def_readonly("mime_type", &OperationArgDoc::mimetype)
        .def_readonly("visibility", &OperationArgDoc::visibility)
        .def_readonly("flow", &OperationArgDoc::flow)
        .def_readonly("samples", &OperationArgDoc::samples);

    py::class_<OperationDoc>(module, "OperationDoc")
        .def_readonly("name", &OperationDoc::name)
        .def_readonly("aliases", &OperationDoc::aliases)
        .def_readonly("description", &OperationDoc::desc)
        .def_readonly("notes", &OperationDoc::notes)
        .def_readonly("tags", &OperationDoc::tags)
        .def_readonly("arguments", &OperationDoc::args);

    py::class_<ImageSnapshot>(module, "ImageSnapshot")
        .def_property_readonly("shape", [](const ImageSnapshot &snapshot){
            return std::make_tuple(snapshot.image.rows, snapshot.image.columns, snapshot.image.channels);
        })
        .def_property_readonly("spacing", [](const ImageSnapshot &snapshot){
            return std::make_tuple(snapshot.image.pxl_dx, snapshot.image.pxl_dy, snapshot.image.pxl_dz);
        })
        .def_property_readonly("anchor", [](const ImageSnapshot &snapshot){
            return as_tuple(snapshot.image.anchor);
        })
        .def_property_readonly("offset", [](const ImageSnapshot &snapshot){
            return as_tuple(snapshot.image.offset);
        })
        .def_property_readonly("row_direction", [](const ImageSnapshot &snapshot){
            return as_tuple(snapshot.image.row_unit);
        })
        .def_property_readonly("column_direction", [](const ImageSnapshot &snapshot){
            return as_tuple(snapshot.image.col_unit);
        })
        .def_property_readonly("metadata", [](const ImageSnapshot &snapshot){
            return snapshot.image.metadata;
        })
        .def_readonly("filename", &ImageSnapshot::filename)
        .def("to_numpy", [](const ImageSnapshot &snapshot){
            const auto count = validated_pixel_count(snapshot.image);
            std::vector<py::ssize_t> shape{
                static_cast<py::ssize_t>(snapshot.image.rows),
                static_cast<py::ssize_t>(snapshot.image.columns),
                static_cast<py::ssize_t>(snapshot.image.channels)
            };
            py::array_t<float> result(shape);
            std::copy_n(snapshot.image.data.cbegin(), count, result.mutable_data());
            return result;
        });

    py::class_<ContourSnapshot>(module, "ContourSnapshot")
        .def_property_readonly("closed", [](const ContourSnapshot &snapshot){
            return snapshot.contour.closed;
        })
        .def_property_readonly("metadata", [](const ContourSnapshot &snapshot){
            return snapshot.contour.metadata;
        })
        .def("to_numpy", [](const ContourSnapshot &snapshot){
            if(static_cast<std::size_t>(std::numeric_limits<py::ssize_t>::max()) < snapshot.contour.points.size()){
                throw std::runtime_error("native contour size exceeds NumPy index limits");
            }
            std::vector<py::ssize_t> shape{
                static_cast<py::ssize_t>(snapshot.contour.points.size()), 3
            };
            py::array_t<double> result(shape);
            auto *destination = result.mutable_data();
            for(const auto &point : snapshot.contour.points){
                *(destination++) = point.x;
                *(destination++) = point.y;
                *(destination++) = point.z;
            }
            return result;
        });

    py::class_<PointCloudSnapshot>(module, "PointCloudSnapshot")
        .def_property_readonly("metadata", [](const PointCloudSnapshot &snapshot){
            return snapshot.points.metadata;
        })
        .def("points_to_numpy", [](const PointCloudSnapshot &snapshot){
            return vec3s_to_numpy(snapshot.points.points);
        })
        .def("normals_to_numpy", [](const PointCloudSnapshot &snapshot){
            return vec3s_to_numpy(snapshot.points.normals);
        })
        .def("colours_to_numpy", [](const PointCloudSnapshot &snapshot){
            if(static_cast<std::size_t>(std::numeric_limits<py::ssize_t>::max()) < snapshot.points.colours.size()){
                throw std::runtime_error("native colour count exceeds NumPy index limits");
            }
            py::array_t<std::uint32_t> result(static_cast<py::ssize_t>(snapshot.points.colours.size()));
            std::copy(snapshot.points.colours.cbegin(), snapshot.points.colours.cend(), result.mutable_data());
            return result;
        });

    py::class_<SurfaceMeshSnapshot>(module, "SurfaceMeshSnapshot")
        .def_property_readonly("faces", [](const SurfaceMeshSnapshot &snapshot){
            return snapshot.mesh.meshes.faces;
        })
        .def_property_readonly("metadata", [](const SurfaceMeshSnapshot &snapshot){
            return snapshot.mesh.meshes.metadata;
        })
        .def("vertices_to_numpy", [](const SurfaceMeshSnapshot &snapshot){
            return vec3s_to_numpy(snapshot.mesh.meshes.vertices);
        })
        .def("normals_to_numpy", [](const SurfaceMeshSnapshot &snapshot){
            return vec3s_to_numpy(snapshot.mesh.meshes.vertex_normals);
        })
        .def("colours_to_numpy", [](const SurfaceMeshSnapshot &snapshot){
            const auto &colours = snapshot.mesh.meshes.vertex_colours;
            if(static_cast<std::size_t>(std::numeric_limits<py::ssize_t>::max()) < colours.size()){
                throw std::runtime_error("native colour count exceeds NumPy index limits");
            }
            py::array_t<std::uint32_t> result(static_cast<py::ssize_t>(colours.size()));
            std::copy(colours.cbegin(), colours.cend(), result.mutable_data());
            return result;
        });

    py::class_<StaticMachineStateSnapshot>(module, "StaticMachineStateSnapshot")
        .def_property_readonly("cumulative_meterset_weight", [](const StaticMachineStateSnapshot &snapshot){
            return snapshot.state.CumulativeMetersetWeight;
        })
        .def_property_readonly("control_point_index", [](const StaticMachineStateSnapshot &snapshot){
            return snapshot.state.ControlPointIndex;
        })
        .def_property_readonly("gantry_angle", [](const StaticMachineStateSnapshot &snapshot){
            return snapshot.state.GantryAngle;
        })
        .def_property_readonly("gantry_rotation_direction", [](const StaticMachineStateSnapshot &snapshot){
            return snapshot.state.GantryRotationDirection;
        })
        .def_property_readonly("beam_limiting_device_angle", [](const StaticMachineStateSnapshot &snapshot){
            return snapshot.state.BeamLimitingDeviceAngle;
        })
        .def_property_readonly("beam_limiting_device_rotation_direction", [](const StaticMachineStateSnapshot &snapshot){
            return snapshot.state.BeamLimitingDeviceRotationDirection;
        })
        .def_property_readonly("patient_support_angle", [](const StaticMachineStateSnapshot &snapshot){
            return snapshot.state.PatientSupportAngle;
        })
        .def_property_readonly("patient_support_rotation_direction", [](const StaticMachineStateSnapshot &snapshot){
            return snapshot.state.PatientSupportRotationDirection;
        })
        .def_property_readonly("table_top_eccentric_angle", [](const StaticMachineStateSnapshot &snapshot){
            return snapshot.state.TableTopEccentricAngle;
        })
        .def_property_readonly("table_top_eccentric_rotation_direction", [](const StaticMachineStateSnapshot &snapshot){
            return snapshot.state.TableTopEccentricRotationDirection;
        })
        .def_property_readonly("table_top_vertical_position", [](const StaticMachineStateSnapshot &snapshot){
            return snapshot.state.TableTopVerticalPosition;
        })
        .def_property_readonly("table_top_longitudinal_position", [](const StaticMachineStateSnapshot &snapshot){
            return snapshot.state.TableTopLongitudinalPosition;
        })
        .def_property_readonly("table_top_lateral_position", [](const StaticMachineStateSnapshot &snapshot){
            return snapshot.state.TableTopLateralPosition;
        })
        .def_property_readonly("table_top_pitch_angle", [](const StaticMachineStateSnapshot &snapshot){
            return snapshot.state.TableTopPitchAngle;
        })
        .def_property_readonly("table_top_pitch_rotation_direction", [](const StaticMachineStateSnapshot &snapshot){
            return snapshot.state.TableTopPitchRotationDirection;
        })
        .def_property_readonly("table_top_roll_angle", [](const StaticMachineStateSnapshot &snapshot){
            return snapshot.state.TableTopRollAngle;
        })
        .def_property_readonly("table_top_roll_rotation_direction", [](const StaticMachineStateSnapshot &snapshot){
            return snapshot.state.TableTopRollRotationDirection;
        })
        .def_property_readonly("isocentre_position", [](const StaticMachineStateSnapshot &snapshot){
            return as_tuple(snapshot.state.IsocentrePosition);
        })
        .def_property_readonly("jaw_positions_x", [](const StaticMachineStateSnapshot &snapshot){
            return doubles_to_numpy(snapshot.state.JawPositionsX);
        })
        .def_property_readonly("jaw_positions_y", [](const StaticMachineStateSnapshot &snapshot){
            return doubles_to_numpy(snapshot.state.JawPositionsY);
        })
        .def_property_readonly("mlc_positions_x", [](const StaticMachineStateSnapshot &snapshot){
            return doubles_to_numpy(snapshot.state.MLCPositionsX);
        })
        .def_property_readonly("metadata", [](const StaticMachineStateSnapshot &snapshot){
            return snapshot.state.metadata;
        });

    py::class_<DynamicMachineStateSnapshot>(module, "DynamicMachineStateSnapshot")
        .def_property_readonly("beam_number", [](const DynamicMachineStateSnapshot &snapshot){
            return snapshot.state.BeamNumber;
        })
        .def_property_readonly("final_cumulative_meterset_weight", [](const DynamicMachineStateSnapshot &snapshot){
            return snapshot.state.FinalCumulativeMetersetWeight;
        })
        .def_property_readonly("static_states", [](const DynamicMachineStateSnapshot &snapshot){
            std::vector<StaticMachineStateSnapshot> states;
            states.reserve(snapshot.state.static_states.size());
            for(const auto &state : snapshot.state.static_states){
                states.push_back(StaticMachineStateSnapshot{state});
            }
            return states;
        })
        .def_property_readonly("metadata", [](const DynamicMachineStateSnapshot &snapshot){
            return snapshot.state.metadata;
        });

    py::class_<RTPlanSnapshot>(module, "RTPlanSnapshot")
        .def_property_readonly("dynamic_states", [](const RTPlanSnapshot &snapshot){
            std::vector<DynamicMachineStateSnapshot> states;
            states.reserve(snapshot.plan.dynamic_states.size());
            for(const auto &state : snapshot.plan.dynamic_states){
                states.push_back(DynamicMachineStateSnapshot{state});
            }
            return states;
        })
        .def_property_readonly("metadata", [](const RTPlanSnapshot &snapshot){
            return snapshot.plan.metadata;
        });

    py::class_<LineSampleSnapshot>(module, "LineSampleSnapshot")
        .def_property_readonly("uncertainties_independent", [](const LineSampleSnapshot &snapshot){
            return snapshot.line.uncertainties_known_to_be_independent_and_random;
        })
        .def_property_readonly("metadata", [](const LineSampleSnapshot &snapshot){
            return snapshot.line.metadata;
        })
        .def("to_numpy", [](const LineSampleSnapshot &snapshot){
            if(static_cast<std::size_t>(std::numeric_limits<py::ssize_t>::max()) < snapshot.line.samples.size()){
                throw std::runtime_error("native line sample size exceeds NumPy index limits");
            }
            std::vector<py::ssize_t> shape{
                static_cast<py::ssize_t>(snapshot.line.samples.size()), 4
            };
            py::array_t<double> result(shape);
            auto *destination = result.mutable_data();
            for(const auto &sample : snapshot.line.samples){
                std::copy(sample.cbegin(), sample.cend(), destination);
                destination += sample.size();
            }
            return result;
        });

    py::class_<SparseTableSnapshot>(module, "SparseTableSnapshot")
        .def_property_readonly("metadata", [](const SparseTableSnapshot &snapshot){
            return snapshot.table.metadata;
        })
        .def_property_readonly("cells", [](const SparseTableSnapshot &snapshot){
            std::vector<std::tuple<std::int64_t, std::int64_t, std::string>> cells;
            cells.reserve(snapshot.table.data.size());
            for(const auto &cell : snapshot.table.data){
                cells.emplace_back(cell.get_row(), cell.get_col(), cell.val);
            }
            return cells;
        });

    py::class_<TransformSnapshot>(module, "TransformSnapshot")
        .def_property_readonly("kind", [](const TransformSnapshot &snapshot){
            return transform_kind(snapshot.transform);
        })
        .def_property_readonly("metadata", [](const TransformSnapshot &snapshot){
            return snapshot.transform.metadata;
        })
        .def_property_readonly("payload", [](const TransformSnapshot &snapshot){
            return transform_payload(snapshot.transform);
        });

    py::class_<DataView>(module, "DataView")
        .def_property_readonly("has_contours", [](const DataView &view){
            return view.data().Has_Contour_Data();
        })
        .def_property_readonly("contour_collection_count", [](const DataView &view){
            const auto &contour_data = view.data().contour_data;
            return contour_data ? contour_data->ccs.size() : 0;
        })
        .def_property_readonly("image_array_count", [](const DataView &view){
            return view.data().image_data.size();
        })
        .def_property_readonly("point_cloud_count", [](const DataView &view){
            return view.data().point_data.size();
        })
        .def_property_readonly("surface_mesh_count", [](const DataView &view){
            return view.data().smesh_data.size();
        })
        .def_property_readonly("rtplan_count", [](const DataView &view){
            return view.data().rtplan_data.size();
        })
        .def_property_readonly("line_sample_count", [](const DataView &view){
            return view.data().lsamp_data.size();
        })
        .def_property_readonly("transform_count", [](const DataView &view){
            return view.data().trans_data.size();
        })
        .def_property_readonly("table_count", [](const DataView &view){
            return view.data().table_data.size();
        })
        .def("image_count", [](DataView &view, const py::ssize_t array_index){
            return get_image_array(view, array_index).imagecoll.images.size();
        }, py::arg("array_index"))
        .def("get_image", [](DataView &view,
                              const py::ssize_t array_index,
                              const py::ssize_t image_index){
            auto &array = get_image_array(view, array_index);
            return ImageSnapshot{get_image(view, array_index, image_index), array.filename};
        }, py::arg("array_index"), py::arg("image_index"))
        .def("set_image_pixels", [](DataView &view,
                                     const py::ssize_t array_index,
                                     const py::ssize_t image_index,
                                     const py::array &pixels){
            auto converted = py::array_t<float, py::array::c_style | py::array::forcecast>::ensure(pixels);
            if(!converted){
                throw py::type_error("pixels must be convertible to a C-contiguous float32 NumPy array");
            }
            auto &image = get_image(view, array_index, image_index);
            const auto count = validated_pixel_count(image);
            if(converted.ndim() != 3){
                throw py::value_error("pixels must have shape (rows, columns, channels)");
            }
            if((converted.shape(0) != image.rows)
            || (converted.shape(1) != image.columns)
            || (converted.shape(2) != image.channels)){
                std::ostringstream os;
                os << "pixel shape does not match native image; expected ("
                   << image.rows << ", " << image.columns << ", " << image.channels << ")";
                throw py::value_error(os.str());
            }
            std::vector<float> replacement(converted.data(), converted.data() + count);
            image.data = std::move(replacement);
        }, py::arg("array_index"), py::arg("image_index"), py::arg("pixels"))
        .def("contour_count", [](DataView &view, const py::ssize_t collection_index){
            return get_contour_collection(view, collection_index).contours.size();
        }, py::arg("collection_index"))
        .def("get_contour", [](DataView &view,
                                 const py::ssize_t collection_index,
                                 const py::ssize_t contour_index){
            return ContourSnapshot{get_contour(view, collection_index, contour_index)};
        }, py::arg("collection_index"), py::arg("contour_index"))
        .def("set_contour", [](DataView &view,
                                 const py::ssize_t collection_index,
                                 const py::ssize_t contour_index,
                                 const py::array &points,
                                 const bool closed,
                                 std::map<std::string, std::string> metadata){
            auto converted = py::array_t<double, py::array::c_style | py::array::forcecast>::ensure(points);
            if(!converted){
                throw py::type_error("points must be convertible to a C-contiguous float64 NumPy array");
            }
            if((converted.ndim() != 2) || (converted.shape(1) != 3)){
                throw py::value_error("points must have shape (point, xyz)");
            }

            auto &contour = get_contour(view, collection_index, contour_index);
            decltype(contour.points) replacement;
            const auto *source = converted.data();
            for(py::ssize_t i = 0; i < converted.shape(0); ++i){
                replacement.emplace_back(source[0], source[1], source[2]);
                source += 3;
            }
            contour.points = std::move(replacement);
            contour.closed = closed;
            contour.metadata = std::move(metadata);
        }, py::arg("collection_index"), py::arg("contour_index"), py::arg("points"),
           py::arg("closed"), py::arg("metadata"))
        .def("get_point_cloud", [](DataView &view, const py::ssize_t point_cloud_index){
            return PointCloudSnapshot{get_point_cloud(view, point_cloud_index).pset};
        }, py::arg("point_cloud_index"))
        .def("set_point_cloud", [](DataView &view,
                                     const py::ssize_t point_cloud_index,
                                     const py::array &points,
                                     const py::array &normals,
                                     const py::array &colours,
                                     std::map<std::string, std::string> metadata){
            auto replacement_points = numpy_to_vec3s(points, "points");
            auto replacement_normals = numpy_to_vec3s(normals, "normals");
            auto replacement_colours = py::array_t<std::uint32_t, py::array::c_style | py::array::forcecast>::ensure(colours);
            if(!replacement_colours){
                throw py::type_error("colours must be convertible to a C-contiguous uint32 NumPy array");
            }
            if(replacement_colours.ndim() != 1){
                throw py::value_error("colours must have shape (point,)");
            }
            if((!replacement_normals.empty() && (replacement_normals.size() != replacement_points.size()))
            || ((replacement_colours.size() != 0) && (static_cast<std::size_t>(replacement_colours.shape(0)) != replacement_points.size()))){
                throw py::value_error("normals and colours must be empty or match the point count");
            }

            std::vector<std::uint32_t> native_colours(replacement_colours.data(),
                                                       replacement_colours.data() + replacement_colours.size());
            auto &point_set = get_point_cloud(view, point_cloud_index).pset;
            point_set.points = std::move(replacement_points);
            point_set.normals = std::move(replacement_normals);
            point_set.colours = std::move(native_colours);
            point_set.metadata = std::move(metadata);
        }, py::arg("point_cloud_index"), py::arg("points"), py::arg("normals"),
           py::arg("colours"), py::arg("metadata"))
        .def("get_surface_mesh", [](DataView &view, const py::ssize_t mesh_index){
            return SurfaceMeshSnapshot{get_surface_mesh(view, mesh_index)};
        }, py::arg("mesh_index"))
        .def("set_surface_mesh", [](DataView &view,
                                      const py::ssize_t mesh_index,
                                      const py::array &vertices,
                                      const py::array &normals,
                                      const py::array &colours,
                                      const py::iterable &faces,
                                      std::map<std::string, std::string> metadata){
            fv_surface_mesh<double, std::uint64_t> replacement;
            replacement.vertices = numpy_to_vec3s(vertices, "vertices");
            replacement.vertex_normals = numpy_to_vec3s(normals, "normals");
            auto replacement_colours = py::array_t<std::uint32_t, py::array::c_style | py::array::forcecast>::ensure(colours);
            if(!replacement_colours){
                throw py::type_error("colours must be convertible to a C-contiguous uint32 NumPy array");
            }
            if(replacement_colours.ndim() != 1){
                throw py::value_error("colours must have shape (vertex,)");
            }
            if((!replacement.vertex_normals.empty() && (replacement.vertex_normals.size() != replacement.vertices.size()))
            || ((replacement_colours.size() != 0) && (static_cast<std::size_t>(replacement_colours.shape(0)) != replacement.vertices.size()))){
                throw py::value_error("normals and colours must be empty or match the vertex count");
            }
            replacement.vertex_colours.assign(replacement_colours.data(),
                                               replacement_colours.data() + replacement_colours.size());

            for(const auto &face_item : faces){
                if(!py::isinstance<py::list>(face_item) && !py::isinstance<py::tuple>(face_item)){
                    throw py::type_error("each face must be a non-empty list or tuple of vertex indices");
                }
                const auto face_values = py::reinterpret_borrow<py::sequence>(face_item);
                if(face_values.size() == 0){
                    throw py::value_error("each face must contain at least one vertex index");
                }
                std::vector<std::uint64_t> face;
                face.reserve(static_cast<std::size_t>(face_values.size()));
                for(const auto &index_item : face_values){
                    if(!py::isinstance<py::int_>(index_item) || py::isinstance<py::bool_>(index_item)){
                        throw py::type_error("face indices must be integers");
                    }
                    std::uint64_t index = 0;
                    try{
                        index = py::cast<std::uint64_t>(index_item);
                    }catch(const py::cast_error &){
                        throw py::value_error("face indices must be non-negative uint64 integers");
                    }
                    if(replacement.vertices.size() <= index){
                        throw py::value_error("face index is outside the vertex array");
                    }
                    face.emplace_back(index);
                }
                replacement.faces.emplace_back(std::move(face));
            }
            replacement.metadata = std::move(metadata);
            replacement.recreate_involved_face_index();

            auto &mesh = get_surface_mesh(view, mesh_index);
            if(!mesh.vertex_attributes.empty() || !mesh.face_attributes.empty()){
                throw std::runtime_error("surface meshes with opaque runtime attributes cannot be replaced from Python");
            }
            mesh.meshes.swap(replacement);
        }, py::arg("mesh_index"), py::arg("vertices"), py::arg("normals"),
           py::arg("colours"), py::arg("faces"), py::arg("metadata"))
        .def("get_rtplan", [](DataView &view, const py::ssize_t rtplan_index){
            return RTPlanSnapshot{get_rtplan(view, rtplan_index)};
        }, py::arg("rtplan_index"))
        .def("set_rtplan", [](DataView &view,
                                const py::ssize_t rtplan_index,
                                const py::iterable &dynamic_states,
                                std::map<std::string, std::string> metadata){
            RTPlan replacement;
            std::size_t beam_index = 0;
            for(const auto &dynamic_state : dynamic_states){
                replacement.dynamic_states.emplace_back(
                    parse_dynamic_machine_state(dynamic_state, beam_index));
                ++beam_index;
            }
            replacement.metadata = std::move(metadata);

            auto &rtplan = get_rtplan(view, rtplan_index);
            rtplan.dynamic_states.swap(replacement.dynamic_states);
            rtplan.metadata.swap(replacement.metadata);
        }, py::arg("rtplan_index"), py::arg("dynamic_states"), py::arg("metadata"))
        .def("get_transform", [](DataView &view, const py::ssize_t transform_index){
            return TransformSnapshot{get_transform(view, transform_index)};
        }, py::arg("transform_index"))
        .def("set_transform", [](DataView &view,
                                   const py::ssize_t transform_index,
                                   const std::string &kind,
                                   const py::dict &payload,
                                   std::map<std::string, std::string> metadata){
            auto replacement = parse_transform(kind, payload, std::move(metadata));
            get_transform(view, transform_index) = replacement;
        }, py::arg("transform_index"), py::arg("kind"), py::arg("payload"), py::arg("metadata"))
        .def("get_line_sample", [](DataView &view, const py::ssize_t line_sample_index){
            return LineSampleSnapshot{get_line_sample(view, line_sample_index).line};
        }, py::arg("line_sample_index"))
        .def("set_line_sample", [](DataView &view,
                                     const py::ssize_t line_sample_index,
                                     const py::array &samples,
                                     const bool uncertainties_independent,
                                     std::map<std::string, std::string> metadata){
            auto converted = py::array_t<double, py::array::c_style | py::array::forcecast>::ensure(samples);
            if(!converted){
                throw py::type_error("samples must be convertible to a C-contiguous float64 NumPy array");
            }
            if((converted.ndim() != 2) || (converted.shape(1) != 4)){
                throw py::value_error("samples must have shape (sample, [x, sigma_x, f, sigma_f])");
            }

            std::vector<std::array<double, 4>> replacement;
            replacement.reserve(static_cast<std::size_t>(converted.shape(0)));
            const auto *source = converted.data();
            for(py::ssize_t i = 0; i < converted.shape(0); ++i){
                replacement.push_back({source[0], source[1], source[2], source[3]});
                source += 4;
            }
            auto &line = get_line_sample(view, line_sample_index).line;
            line.samples = std::move(replacement);
            line.uncertainties_known_to_be_independent_and_random = uncertainties_independent;
            line.metadata = std::move(metadata);
        }, py::arg("line_sample_index"), py::arg("samples"),
           py::arg("uncertainties_independent"), py::arg("metadata"))
        .def("get_table", [](DataView &view, const py::ssize_t table_index){
            return SparseTableSnapshot{get_sparse_table(view, table_index).table};
        }, py::arg("table_index"))
        .def("set_table", [](DataView &view,
                               const py::ssize_t table_index,
                               const py::iterable &cells,
                               std::map<std::string, std::string> metadata){
            tables::table2 replacement;
            std::set<std::pair<std::int64_t, std::int64_t>> coordinates;
            for(const auto &item : cells){
                if(!py::isinstance<py::tuple>(item)){
                    throw py::type_error("each table cell must be a (row, column, string value) tuple");
                }
                const auto cell = py::cast<py::tuple>(item);
                if((cell.size() != 3)
                || !py::isinstance<py::int_>(cell[0])
                || py::isinstance<py::bool_>(cell[0])
                || !py::isinstance<py::int_>(cell[1])
                || py::isinstance<py::bool_>(cell[1])
                || !py::isinstance<py::str>(cell[2])){
                    throw py::type_error("each table cell must be a (row, column, string value) tuple");
                }

                const auto row = py::cast<std::int64_t>(cell[0]);
                const auto column = py::cast<std::int64_t>(cell[1]);
                const auto value = py::cast<std::string>(cell[2]);
                if(!coordinates.emplace(row, column).second){
                    throw py::value_error("duplicate table cell coordinate");
                }
                replacement.inject(row, column, value);
            }
            replacement.metadata = std::move(metadata);
            get_sparse_table(view, table_index).table = std::move(replacement);
        }, py::arg("table_index"), py::arg("cells"), py::arg("metadata"));

    py::class_<dcma::Session>(module, "Session")
        .def(py::init<>())
        .def("load", [](dcma::Session &session, const std::vector<std::string> &paths){
            dcma::Session::path_list_t native_paths;
            for(const auto &path : paths){
                native_paths.emplace_back(path);
            }
            bool loaded = false;
            try{
                loaded = session.load(native_paths);
            }catch(const std::exception &e){
                throw std::runtime_error(std::string(e.what())
                                         + "; native loading is non-transactional and partial changes may remain");
            }
            if(!loaded){
                std::ostringstream os;
                os << session.last_error()
                   << "; native loading is non-transactional and partial changes may remain";
                throw std::runtime_error(os.str());
            }
        }, py::arg("paths"))
        .def("run", [](dcma::Session &session, const std::string &name, const py::kwargs &arguments){
            if(!session.run(make_operation(name, arguments))){
                throw std::runtime_error("operation pipeline failed: " + session.last_error()
                                         + "; native operations are non-transactional");
            }
        }, py::arg("name"))
        .def("run_many", [](dcma::Session &session, const py::iterable &specifications){
            dcma::Session::operation_list_t operations;
            for(const auto &specification : specifications){
                if(!py::isinstance<py::tuple>(specification)){
                    throw py::type_error("each operation must be a (name, arguments) pair");
                }
                const auto tuple = py::cast<py::tuple>(specification);
                if(tuple.size() != 2 || !py::isinstance<py::str>(tuple[0]) || !py::isinstance<py::dict>(tuple[1])){
                    throw py::type_error("each operation must be a (name, arguments) pair");
                }
                operations.push_back(make_operation(py::cast<std::string>(tuple[0]), py::cast<py::dict>(tuple[1])));
            }
            if(!session.run(std::move(operations))){
                throw std::runtime_error("operation sequence failed: " + session.last_error()
                                         + "; native operations are non-transactional");
            }
        }, py::arg("operations"))
        .def("run_script", [](dcma::Session &session, const std::string &script){
            dcma::Session::feedback_list_t feedback;
            if(!session.run_script(script, feedback)){
                const auto error = session.last_error().empty()
                                 ? script_feedback_message(feedback)
                                 : session.last_error();
                throw std::runtime_error(error + "; native operations are non-transactional");
            }
        }, py::arg("script"))
        .def("operations", &dcma::Session::operation_docs)
        .def_property("metadata",
            [](const dcma::Session &session){ return session.metadata(); },
            [](dcma::Session &session, dcma::Session::metadata_t metadata){
                session.metadata() = std::move(metadata);
            })
        .def("set_metadata", [](dcma::Session &session, const std::string &key, const std::string &value){
            session.metadata()[key] = value;
        }, py::arg("key"), py::arg("value"))
        .def_property("lexicon_filename",
            [](const dcma::Session &session){ return session.lexicon_filename(); },
            [](dcma::Session &session, std::string filename){
                session.lexicon_filename() = std::move(filename);
            })
        .def_property_readonly("data", [](dcma::Session &session){
            return DataView{&session, nullptr};
        }, py::keep_alive<0, 1>());

    py::class_<EmbeddedSession>(module, "_EmbeddedSession")
        .def_property("metadata",
            [](const EmbeddedSession &session){
                session.require_active();
                return *session.state->metadata;
            },
            [](EmbeddedSession &session, std::map<std::string, std::string> metadata){
                session.require_active();
                *session.state->metadata = std::move(metadata);
            })
        .def("set_metadata", [](EmbeddedSession &session, const std::string &key, const std::string &value){
            session.require_active();
            (*session.state->metadata)[key] = value;
        }, py::arg("key"), py::arg("value"))
        .def_property_readonly("lexicon_filename", [](const EmbeddedSession &session){
            session.require_active();
            return *session.state->lexicon_filename;
        })
        .def_property_readonly("data", [](EmbeddedSession &session){
            session.require_active();
            return DataView{nullptr, session.state};
        }, py::keep_alive<0, 1>());
}

py::object MakeEmbeddedSession(const std::shared_ptr<EmbeddedSessionState> &state){
    return py::cast(EmbeddedSession{state});
}

} // namespace python
} // namespace dcma
