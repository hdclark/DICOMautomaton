// Sketch.h.

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "YgorMath.h"

class Sketch {
public:
    using vertex_index_t = std::size_t;
    using primitive_index_t = std::size_t;
    using constraint_index_t = std::size_t;

    enum class geometry_tag_t {
        normal,
        support,
    };

    enum class primitive_kind_t {
        vertex,
        line,
        circle,
        arc,
        bezier,
    };

    enum class constraint_kind_t {
        horizontal,
        vertical,
        distance,
        parallel,
        perpendicular,
        pin,
        tangent,
        mirror,
        overlap,
    };

    struct plane_frame_t {
        vec3<double> origin;
        vec3<double> row_unit;
        vec3<double> col_unit;

        vec3<double> normal() const;
    };

    struct projection_t {
        double u = 0.0;
        double v = 0.0;
    };

    struct bounding_box_t {
        // Invalid-until-populated sentinel bounds. Call is_valid() before use when needed.
        projection_t min = {  std::numeric_limits<double>::infinity(),  std::numeric_limits<double>::infinity() };
        projection_t max = { -std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity() };

        bool is_valid() const;
        bool contains(const projection_t &p) const;
        bool contains(const bounding_box_t &b) const;
    };

    struct dof_summary_t {
        std::size_t total = 0U;
        std::size_t constrained = 0U;
        std::size_t remaining = 0U;
        std::size_t overconstrained = 0U;
        std::size_t enabled_constraints = 0U;
        std::size_t disabled_constraints = 0U;
    };

    struct solve_options_t {
        // LM generally needs more iterations than the legacy projection-only placeholder solver.
        std::size_t max_iterations = 128U;
        double absolute_tolerance = 0.0;
        double relative_tolerance = 0.0;
        double max_time_seconds = 0.0;
        bool constrain_to_bounds = false;
        std::optional<bounding_box_t> bounds = {};
        double sticky_weight = 1.0E-4;
        bool enable_sticky_constraints = true;
        double residual_tolerance = 1.0E-4;
        double finite_difference_step = 1.0E-6;
        double initial_lambda = 1.0E-3;
        double lambda_increase_factor = 10.0;
        double lambda_decrease_factor = 0.1;
    };

    struct extrusion_options_t {
        // Positive values extrude away from the sketch plane along the named direction.
        // Negative values are also legitimate and allow the user to place either cap on the
        // opposite side of the sketch plane, provided the combined span remains positive.
        double into_frame_length = 10.0;
        double out_of_frame_length = 10.0;
        double into_frame_angle_degrees = 0.0;
        double out_of_frame_angle_degrees = 0.0;
        std::size_t curve_segments = 48U;
    };

    struct solve_report_t {
        std::size_t unresolved_constraints = 0U;
        std::size_t enabled_constraints = 0U;
        std::size_t residual_count = 0U;
        std::size_t jacobian_rank = 0U;
        double cost = std::numeric_limits<double>::quiet_NaN();
        double conflict_norm = 0.0;
        int64_t iterations = 0;
        bool converged = false;
        bool used_svd = false;
        bool conflicting_constraints = false;
        std::string reason;
    };

    struct primitive_t {
        geometry_tag_t tag = geometry_tag_t::normal;

        virtual ~primitive_t() = default;
        virtual std::unique_ptr<primitive_t> clone() const = 0;
        virtual primitive_kind_t kind() const = 0;
        virtual std::vector<vertex_index_t> referenced_vertices() const = 0;
    };

    struct vertex_primitive_t : public primitive_t {
        vertex_index_t vertex = 0U;

        std::unique_ptr<primitive_t> clone() const override;
        primitive_kind_t kind() const override;
        std::vector<vertex_index_t> referenced_vertices() const override;
    };

    struct line_primitive_t : public primitive_t {
        std::array<vertex_index_t, 2> vertices = {};

        std::unique_ptr<primitive_t> clone() const override;
        primitive_kind_t kind() const override;
        std::vector<vertex_index_t> referenced_vertices() const override;
    };

    struct circle_primitive_t : public primitive_t {
        vertex_index_t center = 0U;
        vertex_index_t radius_point = 0U;
        double radius = 0.0;

        std::unique_ptr<primitive_t> clone() const override;
        primitive_kind_t kind() const override;
        std::vector<vertex_index_t> referenced_vertices() const override;
    };

    struct arc_primitive_t : public primitive_t {
        vertex_index_t center = 0U;
        vertex_index_t start = 0U;
        vertex_index_t stop = 0U;
        double radius = 0.0;
        double start_angle = 0.0;
        double stop_angle = 0.0;

        std::unique_ptr<primitive_t> clone() const override;
        primitive_kind_t kind() const override;
        std::vector<vertex_index_t> referenced_vertices() const override;
    };

    struct bezier_primitive_t : public primitive_t {
        std::array<vertex_index_t, 4> control_vertices = {};

        std::unique_ptr<primitive_t> clone() const override;
        primitive_kind_t kind() const override;
        std::vector<vertex_index_t> referenced_vertices() const override;
    };

    struct constraint_t {
        bool enabled = true;

        virtual ~constraint_t() = default;
        virtual std::unique_ptr<constraint_t> clone() const = 0;
        virtual constraint_kind_t kind() const = 0;
        virtual std::vector<primitive_index_t> referenced_primitives() const = 0;
        virtual std::vector<vertex_index_t> referenced_vertices() const;
    };

    struct horizontal_constraint_t : public constraint_t {
        primitive_index_t line = 0U;

        std::unique_ptr<constraint_t> clone() const override;
        constraint_kind_t kind() const override;
        std::vector<primitive_index_t> referenced_primitives() const override;
    };

    struct vertical_constraint_t : public constraint_t {
        primitive_index_t line = 0U;

        std::unique_ptr<constraint_t> clone() const override;
        constraint_kind_t kind() const override;
        std::vector<primitive_index_t> referenced_primitives() const override;
    };

    struct distance_constraint_t : public constraint_t {
        primitive_index_t primitive = 0U;
        double target_distance = 0.0;

        std::unique_ptr<constraint_t> clone() const override;
        constraint_kind_t kind() const override;
        std::vector<primitive_index_t> referenced_primitives() const override;
    };

    struct parallel_constraint_t : public constraint_t {
        primitive_index_t line_a = 0U;
        primitive_index_t line_b = 0U;

        std::unique_ptr<constraint_t> clone() const override;
        constraint_kind_t kind() const override;
        std::vector<primitive_index_t> referenced_primitives() const override;
    };

    struct perpendicular_constraint_t : public constraint_t {
        primitive_index_t line_a = 0U;
        primitive_index_t line_b = 0U;

        std::unique_ptr<constraint_t> clone() const override;
        constraint_kind_t kind() const override;
        std::vector<primitive_index_t> referenced_primitives() const override;
    };

    struct pin_constraint_t : public constraint_t {
        vertex_index_t vertex = 0U;
        vec3<double> pinned_position;

        std::unique_ptr<constraint_t> clone() const override;
        constraint_kind_t kind() const override;
        std::vector<primitive_index_t> referenced_primitives() const override;
        std::vector<vertex_index_t> referenced_vertices() const override;
    };

    struct tangent_constraint_t : public constraint_t {
        primitive_index_t primitive_a = 0U;
        primitive_index_t primitive_b = 0U;

        std::unique_ptr<constraint_t> clone() const override;
        constraint_kind_t kind() const override;
        std::vector<primitive_index_t> referenced_primitives() const override;
    };

    struct mirror_constraint_t : public constraint_t {
        primitive_index_t line = 0U;
        vertex_index_t vertex_a = 0U;
        vertex_index_t vertex_b = 0U;

        std::unique_ptr<constraint_t> clone() const override;
        constraint_kind_t kind() const override;
        std::vector<primitive_index_t> referenced_primitives() const override;
        std::vector<vertex_index_t> referenced_vertices() const override;
    };

    struct overlap_constraint_t : public constraint_t {
        vertex_index_t vertex_a = 0U;
        vertex_index_t vertex_b = 0U;

        std::unique_ptr<constraint_t> clone() const override;
        constraint_kind_t kind() const override;
        std::vector<primitive_index_t> referenced_primitives() const override;
        std::vector<vertex_index_t> referenced_vertices() const override;
    };

    Sketch();
    Sketch(const Sketch &in);
    Sketch& operator=(const Sketch &in);

    void clear();
    bool empty() const;

    bool has_plane() const;
    void set_plane(const plane_frame_t &frame);
    const plane_frame_t& plane() const;

    projection_t project(const vec3<double> &point) const;
    vec3<double> lift(const projection_t &point) const;

    vertex_index_t append_vertex(const vec3<double> &point);

    primitive_index_t add_vertex_primitive(vertex_index_t vertex, geometry_tag_t tag);
    primitive_index_t add_vertex_primitive(const vec3<double> &point, geometry_tag_t tag);
    primitive_index_t add_line(vertex_index_t start, vertex_index_t stop, geometry_tag_t tag);
    primitive_index_t add_line(const vec3<double> &start, const vec3<double> &stop, geometry_tag_t tag);
    primitive_index_t add_circle(vertex_index_t center, vertex_index_t radius_point, geometry_tag_t tag);
    primitive_index_t add_circle(const vec3<double> &center, const vec3<double> &radius_point, geometry_tag_t tag);
    primitive_index_t add_arc(vertex_index_t center, vertex_index_t start, vertex_index_t stop, geometry_tag_t tag);
    primitive_index_t add_arc(const vec3<double> &center, const vec3<double> &start, const vec3<double> &stop, geometry_tag_t tag);
    primitive_index_t add_bezier(const std::array<vertex_index_t, 4> &control_vertices, geometry_tag_t tag);
    primitive_index_t add_bezier(const std::array<vec3<double>, 4> &control_points, geometry_tag_t tag);

    std::size_t vertex_count() const;
    std::size_t primitive_count() const;
    std::size_t constraint_count() const;

    vec3<double>& vertex(vertex_index_t idx);
    const vec3<double>& vertex(vertex_index_t idx) const;
    primitive_t* primitive(primitive_index_t idx);
    const primitive_t* primitive(primitive_index_t idx) const;
    constraint_t* constraint(constraint_index_t idx);
    const constraint_t* constraint(constraint_index_t idx) const;

    bounding_box_t primitive_bounds(primitive_index_t idx) const;
    std::vector<vec3<double>> sample_primitive(primitive_index_t idx, std::size_t segments = 32U) const;

    std::optional<primitive_index_t> nearest_primitive(const vec3<double> &point, double tolerance) const;
    std::optional<vertex_index_t> nearest_vertex(const vec3<double> &point,
                                                 double tolerance,
                                                 const std::set<primitive_index_t> &primitive_mask = {}) const;
    std::optional<std::pair<primitive_index_t, vertex_index_t>> nearest_primitive_vertex(
        const vec3<double> &point,
        double tolerance,
        const std::set<primitive_index_t> &primitive_mask = {}) const;

    std::vector<primitive_index_t> primitives_inside_box(const vec3<double> &a, const vec3<double> &b) const;
    std::set<vertex_index_t> collect_vertices(const std::set<primitive_index_t> &primitives) const;
    std::vector<primitive_index_t> primitives_referencing_vertex(vertex_index_t idx) const;

    void set_vertex(vertex_index_t idx, const vec3<double> &point);
    void translate_vertices(const std::set<vertex_index_t> &indices, const vec3<double> &delta);
    void refresh_geometry();
    bool clamp_vertices_to_bounds(const bounding_box_t &bounds);
    dof_summary_t summarize_degrees_of_freedom() const;
    std::set<vertex_index_t> fully_constrained_vertices() const;
    std::set<primitive_index_t> fully_constrained_primitives() const;

    bool delete_vertex(vertex_index_t idx);
    bool delete_primitive(primitive_index_t idx);
    bool delete_constraint(constraint_index_t idx);
    std::size_t delete_unreferenced_vertices();
    std::optional<vertex_index_t> insert_vertex(primitive_index_t idx, const vec3<double> &point);
    bool collapse_vertices(vertex_index_t keep_idx, vertex_index_t remove_idx);
    void clear_vertices();
    void clear_primitives();
    void clear_constraints();

    constraint_index_t add_horizontal_constraint(primitive_index_t line_idx);
    constraint_index_t add_vertical_constraint(primitive_index_t line_idx);
    constraint_index_t add_distance_constraint(primitive_index_t primitive_idx, double target_distance = std::numeric_limits<double>::quiet_NaN());
    constraint_index_t add_parallel_constraint(primitive_index_t line_a, primitive_index_t line_b);
    constraint_index_t add_perpendicular_constraint(primitive_index_t line_a, primitive_index_t line_b);
    constraint_index_t add_pin_constraint(vertex_index_t vertex_idx);
    constraint_index_t add_tangent_constraint(primitive_index_t primitive_a, primitive_index_t primitive_b);
    constraint_index_t add_mirror_constraint(primitive_index_t line_idx, vertex_index_t vertex_a, vertex_index_t vertex_b);
    constraint_index_t add_overlap_constraint(vertex_index_t vertex_a, vertex_index_t vertex_b);

    std::size_t solve_constraints(std::size_t max_iterations = 128U);
    std::size_t solve_constraints(const solve_options_t &options);
    const solve_report_t& last_solve_report() const;
    bool build_extruded_surface_mesh(const extrusion_options_t &options,
                                     fv_surface_mesh<double, uint64_t> &mesh,
                                     std::vector<fv_surface_mesh<double, uint64_t>> *cap_meshes = nullptr,
                                     std::string *error_message = nullptr) const;
    std::string describe_constraint(constraint_index_t idx) const;
    bool save_to_file(const std::filesystem::path &path, std::string *error_message = nullptr) const;
    static bool load_from_file(const std::filesystem::path &path, Sketch &out, std::string *error_message = nullptr);

private:
    std::vector<vec3<double>> vertices_;
    std::vector<std::unique_ptr<primitive_t>> primitives_;
    std::vector<std::unique_ptr<constraint_t>> constraints_;
    bool has_plane_ = false;
    plane_frame_t plane_ = {};
    solve_report_t last_solve_report_ = {};

    static double normalize_angle(double x);
    static double squared_distance_to_segment(const projection_t &p, const projection_t &a, const projection_t &b);

    void copy_from(const Sketch &in);
    primitive_index_t append_primitive(std::unique_ptr<primitive_t> primitive);
    constraint_index_t append_constraint(std::unique_ptr<constraint_t> constraint);
    void enforce_pinned_vertices();
    void refresh_all_derived_geometry();
    bool refresh_primitive_geometry(primitive_index_t idx, const std::set<vertex_index_t> &pinned_vertices);
    bool primitive_index_valid(primitive_index_t idx) const;
    bool vertex_index_valid(vertex_index_t idx) const;
};
