// Sketch_Mesh_Builder.h.

#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "YgorMath.h"

#include "Sketch.h"

// Supported procedures that combine a parent mesh with a current-node sketch.
enum class sketch_procedure_kind_t {
    clear,        // Discard the parent mesh and start fresh.
    noop,         // Pass the parent mesh through unchanged (sketch is ignored).
    extrusion,    // Extrude the current sketch and merge with the parent mesh.
    through_hole, // Extrude the current sketch and subtract from the parent mesh.
};

std::string sketch_procedure_kind_to_string(sketch_procedure_kind_t kind);
bool string_to_sketch_procedure_kind(const std::string &s, sketch_procedure_kind_t &out);

// Holds all information needed to apply a procedure that combines
// a parent mesh with a sketch to produce a child mesh.
struct Sketch_Procedure {
    sketch_procedure_kind_t kind = sketch_procedure_kind_t::clear;

    // Extrusion parameters (used when kind == extrusion or through_hole).
    Sketch::extrusion_options_t extrusion_options;

    bool write_to(std::ostream &os) const;
    static bool read_from(std::istream &is, Sketch_Procedure &out);
};

// A single node in the Sketch_Mesh_Builder sequence.
// Each node owns a sketch, an optional mesh, and a procedure.
struct Sketch_Mesh_Builder_Node {
    Sketch sketch;
    Sketch_Procedure procedure;
    std::optional<fv_surface_mesh<double, uint64_t>> mesh;
};

// Manages a linear sequence of sketch/mesh/procedure nodes.
// The root node may hold a sketch, mesh, or both.
// Subsequent child nodes always hold a sketch and procedure;
// a mesh is only populated when the user explicitly computes one.
class Sketch_Mesh_Builder {
public:
    Sketch_Mesh_Builder();

    // Node access.
    std::size_t node_count() const;
    Sketch_Mesh_Builder_Node& node(std::size_t idx);
    const Sketch_Mesh_Builder_Node& node(std::size_t idx) const;

    // Active node (the one currently being edited/viewed).
    std::size_t active_node_index() const;
    void set_active_node_index(std::size_t idx);
    Sketch_Mesh_Builder_Node& active_node();
    const Sketch_Mesh_Builder_Node& active_node() const;

    // Append a default leaf node (empty sketch, clear procedure).
    void append_default_node();

    // Delete the node at idx from the linear sequence. Any later nodes shift
    // down by one position and become descendants of their new immediate predecessor.
    // If deleting the root leaves the sequence empty, a new default root is created.
    // Returns the new active node index.
    std::size_t delete_leaf_node(std::size_t idx);

    // Compute the mesh for a single node by combining the parent mesh with
    // the node's sketch using the node's procedure.
    // Returns true on success.
    bool compute_node(std::size_t idx, std::string *error_message = nullptr);

    // Compute all nodes sequentially from root to the last node.
    bool compute_all(std::string *error_message = nullptr);

    // I/O: write/read the entire builder (all nodes) to/from a stream.
    bool write_to(std::ostream &os) const;
    static bool read_from(std::istream &is, Sketch_Mesh_Builder &out);

    // File-based I/O convenience wrappers.
    bool save_to_file(const std::filesystem::path &path, std::string *error_message = nullptr) const;
    static bool load_from_file(const std::filesystem::path &path, Sketch_Mesh_Builder &out, std::string *error_message = nullptr);

private:
    std::vector<Sketch_Mesh_Builder_Node> nodes_;
    std::size_t active_index_ = 0;
};
