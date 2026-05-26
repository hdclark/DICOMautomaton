// Sketch_Mesh_Builder.cc - A part of DICOMautomaton 2026. Written by hal clark.

#include "Sketch_Mesh_Builder.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>

#include "YgorLog.h"
#include "YgorMathIOOBJ.h"

// ---------------------------------------------------------------------------
// Procedure kind string conversions.
// ---------------------------------------------------------------------------
std::string sketch_procedure_kind_to_string(sketch_procedure_kind_t kind){
    switch(kind){
        case sketch_procedure_kind_t::clear:        return "clear";
        case sketch_procedure_kind_t::noop:         return "noop";
        case sketch_procedure_kind_t::extrusion:    return "extrusion";
        case sketch_procedure_kind_t::through_hole: return "through_hole";
    }
    return "clear";
}

bool string_to_sketch_procedure_kind(const std::string &s, sketch_procedure_kind_t &out){
    if(s == "clear"){        out = sketch_procedure_kind_t::clear;        return true; }
    if(s == "noop"){         out = sketch_procedure_kind_t::noop;         return true; }
    if(s == "extrusion"){    out = sketch_procedure_kind_t::extrusion;    return true; }
    if(s == "through_hole"){ out = sketch_procedure_kind_t::through_hole; return true; }
    return false;
}

// ---------------------------------------------------------------------------
// Sketch_Procedure I/O.
// ---------------------------------------------------------------------------
bool Sketch_Procedure::write_to(std::ostream &os) const {
    const auto defaultprecision = os.precision();
    os.precision(std::numeric_limits<double>::max_digits10 + 1);

    os << "procedure_begin\n";
    os << "procedure_kind " << sketch_procedure_kind_to_string(kind) << "\n";
    os << "extrusion_into_frame_length " << extrusion_options.into_frame_length << "\n";
    os << "extrusion_out_of_frame_length " << extrusion_options.out_of_frame_length << "\n";
    os << "extrusion_into_frame_angle_degrees " << extrusion_options.into_frame_angle_degrees << "\n";
    os << "extrusion_out_of_frame_angle_degrees " << extrusion_options.out_of_frame_angle_degrees << "\n";
    os << "extrusion_curve_segments " << extrusion_options.curve_segments << "\n";
    os << "extrusion_max_discretization_error " << extrusion_options.max_discretization_error << "\n";
    os << "procedure_end\n";

    os.precision(defaultprecision);
    return os.good();
}

bool Sketch_Procedure::read_from(std::istream &is, Sketch_Procedure &out){
    out = Sketch_Procedure{};
    std::string line;
    bool found_begin = false;
    while(std::getline(is, line)){
        std::istringstream iss(line);
        std::string keyword;
        iss >> keyword;
        if(keyword == "procedure_begin"){
            found_begin = true;
        }else if(keyword == "procedure_end"){
            return found_begin;
        }else if(keyword == "procedure_kind"){
            std::string kind_str;
            if(!(iss >> kind_str) || !string_to_sketch_procedure_kind(kind_str, out.kind)){
                return false;
            }
        }else if(keyword == "extrusion_into_frame_length"){
            if(!(iss >> out.extrusion_options.into_frame_length)) return false;
        }else if(keyword == "extrusion_out_of_frame_length"){
            if(!(iss >> out.extrusion_options.out_of_frame_length)) return false;
        }else if(keyword == "extrusion_into_frame_angle_degrees"){
            if(!(iss >> out.extrusion_options.into_frame_angle_degrees)) return false;
        }else if(keyword == "extrusion_out_of_frame_angle_degrees"){
            if(!(iss >> out.extrusion_options.out_of_frame_angle_degrees)) return false;
        }else if(keyword == "extrusion_curve_segments"){
            if(!(iss >> out.extrusion_options.curve_segments)) return false;
        }else if(keyword == "extrusion_max_discretization_error"){
            if(!(iss >> out.extrusion_options.max_discretization_error)) return false;
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// Sketch_Mesh_Builder implementation.
// ---------------------------------------------------------------------------
Sketch_Mesh_Builder::Sketch_Mesh_Builder(){
    nodes_.emplace_back();
}

std::size_t Sketch_Mesh_Builder::node_count() const {
    return nodes_.size();
}

Sketch_Mesh_Builder_Node& Sketch_Mesh_Builder::node(std::size_t idx){
    return nodes_.at(idx);
}

const Sketch_Mesh_Builder_Node& Sketch_Mesh_Builder::node(std::size_t idx) const {
    return nodes_.at(idx);
}

std::size_t Sketch_Mesh_Builder::active_node_index() const {
    return active_index_;
}

void Sketch_Mesh_Builder::set_active_node_index(std::size_t idx){
    active_index_ = std::min(idx, nodes_.empty() ? 0U : nodes_.size() - 1U);
}

Sketch_Mesh_Builder_Node& Sketch_Mesh_Builder::active_node(){
    return nodes_.at(active_index_);
}

const Sketch_Mesh_Builder_Node& Sketch_Mesh_Builder::active_node() const {
    return nodes_.at(active_index_);
}

void Sketch_Mesh_Builder::append_default_node(){
    nodes_.emplace_back();
    active_index_ = nodes_.size() - 1U;
}

std::size_t Sketch_Mesh_Builder::delete_leaf_node(std::size_t idx){
    if(nodes_.size() <= 1U){
        nodes_.clear();
        nodes_.emplace_back();
        active_index_ = 0U;
        return 0U;
    }
    if(idx < nodes_.size()){
        nodes_.erase(std::next(std::begin(nodes_), static_cast<std::ptrdiff_t>(idx)));
    }
    active_index_ = std::min(active_index_, nodes_.size() - 1U);
    return active_index_;
}

bool Sketch_Mesh_Builder::compute_node(std::size_t idx, std::string *error_message){
    if(idx >= nodes_.size()){
        if(error_message) *error_message = "Invalid node index";
        return false;
    }

    auto &current = nodes_.at(idx);

    // Determine parent mesh (if any).
    std::optional<fv_surface_mesh<double, uint64_t>> parent_mesh;
    if(idx > 0U && nodes_.at(idx - 1U).mesh.has_value()){
        parent_mesh = nodes_.at(idx - 1U).mesh;
    }

    switch(current.procedure.kind){
        case sketch_procedure_kind_t::clear:
        {
            // Discard parent mesh. If sketch has primitives, extrude it; otherwise leave empty.
            if(current.sketch.primitive_count() > 0U){
                auto extrude_sketch = current.sketch;
                // Remove support primitives.
                for(std::size_t i = 0; i < extrude_sketch.primitive_count(); ){
                    auto *primitive = extrude_sketch.primitive(i);
                    if(primitive != nullptr && primitive->tag == Sketch::geometry_tag_t::support){
                        extrude_sketch.delete_primitive(i);
                    }else{
                        ++i;
                    }
                }
                extrude_sketch.delete_unreferenced_vertices();
                fv_surface_mesh<double, uint64_t> mesh;
                std::string build_error_message;
                if(!extrude_sketch.build_extruded_surface_mesh(current.procedure.extrusion_options,
                                                               mesh,
                                                               nullptr,
                                                               &build_error_message)){
                    if(build_error_message == "does not contain any extrudable primitives"){
                        current.mesh = fv_surface_mesh<double, uint64_t>();
                        return true;
                    }
                    if(error_message) *error_message = build_error_message;
                    return false;
                }
                current.mesh = std::move(mesh);
            }else{
                current.mesh = fv_surface_mesh<double, uint64_t>();
            }
            return true;
        }
        case sketch_procedure_kind_t::noop:
        {
            // Pass parent mesh through.
            current.mesh = parent_mesh.value_or(fv_surface_mesh<double, uint64_t>());
            return true;
        }
        case sketch_procedure_kind_t::extrusion:
        {
            auto extrude_sketch = current.sketch;
            for(std::size_t i = 0; i < extrude_sketch.primitive_count(); ){
                auto *primitive = extrude_sketch.primitive(i);
                if(primitive != nullptr && primitive->tag == Sketch::geometry_tag_t::support){
                    extrude_sketch.delete_primitive(i);
                }else{
                    ++i;
                }
            }
            extrude_sketch.delete_unreferenced_vertices();
            fv_surface_mesh<double, uint64_t> extruded;
            if(!extrude_sketch.build_extruded_surface_mesh(current.procedure.extrusion_options, extruded, nullptr, error_message)){
                return false;
            }
            // Merge with parent mesh if available.
            if(parent_mesh.has_value()){
                const auto vertex_offset = static_cast<uint64_t>(parent_mesh->vertices.size());
                for(const auto &v : extruded.vertices){
                    parent_mesh->vertices.push_back(v);
                }
                for(auto face : extruded.faces){
                    for(auto &idx_val : face){
                        idx_val += vertex_offset;
                    }
                    parent_mesh->faces.push_back(std::move(face));
                }
                parent_mesh->recreate_involved_face_index();
                current.mesh = std::move(parent_mesh);
            }else{
                current.mesh = std::move(extruded);
            }
            return true;
        }
        case sketch_procedure_kind_t::through_hole:
        {
            // Through-hole: extrude the sketch and conceptually subtract from parent.
            // Full CSG boolean subtraction is beyond scope; for now, extrude and merge
            // (the mesh will contain intersecting geometry that downstream tools can resolve).
            auto extrude_sketch = current.sketch;
            for(std::size_t i = 0; i < extrude_sketch.primitive_count(); ){
                auto *primitive = extrude_sketch.primitive(i);
                if(primitive != nullptr && primitive->tag == Sketch::geometry_tag_t::support){
                    extrude_sketch.delete_primitive(i);
                }else{
                    ++i;
                }
            }
            extrude_sketch.delete_unreferenced_vertices();
            fv_surface_mesh<double, uint64_t> hole_mesh;
            if(!extrude_sketch.build_extruded_surface_mesh(current.procedure.extrusion_options, hole_mesh, nullptr, error_message)){
                return false;
            }
            if(parent_mesh.has_value()){
                const auto vertex_offset = static_cast<uint64_t>(parent_mesh->vertices.size());
                for(const auto &v : hole_mesh.vertices){
                    parent_mesh->vertices.push_back(v);
                }
                // Reverse face winding for hole mesh to represent subtraction.
                for(auto face : hole_mesh.faces){
                    for(auto &idx_val : face){
                        idx_val += vertex_offset;
                    }
                    std::reverse(std::begin(face), std::end(face));
                    parent_mesh->faces.push_back(std::move(face));
                }
                parent_mesh->recreate_involved_face_index();
                current.mesh = std::move(parent_mesh);
            }else{
                current.mesh = std::move(hole_mesh);
            }
            return true;
        }
    }
    if(error_message) *error_message = "Unknown procedure kind";
    return false;
}

bool Sketch_Mesh_Builder::compute_all(std::string *error_message){
    for(std::size_t i = 0U; i < nodes_.size(); ++i){
        if(!compute_node(i, error_message)) return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// I/O.
// ---------------------------------------------------------------------------
bool Sketch_Mesh_Builder::write_to(std::ostream &os) const {
    const auto defaultprecision = os.precision();
    os.precision(std::numeric_limits<double>::max_digits10 + 1);

    os << "sketch_mesh_builder_format_version 1\n";
    os << "node_count " << nodes_.size() << "\n";
    os << "active_node " << active_index_ << "\n";

    for(std::size_t i = 0U; i < nodes_.size(); ++i){
        os << "node_begin " << i << "\n";

        // Write sketch.
        os << "sketch_begin\n";
        if(nodes_[i].sketch.has_plane()){
            nodes_[i].sketch.write_to(os);
        }else{
            // Write a minimal empty sketch with a default plane.
            os << "sketch_format_version 1\n";
            os << "plane_origin 0 0 0\n";
            os << "plane_row_unit 1 0 0\n";
            os << "plane_col_unit 0 1 0\n";
            os << "sketch_end\n";
        }

        // Write procedure.
        nodes_[i].procedure.write_to(os);

        // Write mesh (if present).
        if(nodes_[i].mesh.has_value()){
            os << "mesh_begin\n";
            if(!WriteFVSMeshToOBJ(nodes_[i].mesh.value(), os)){
                os.precision(defaultprecision);
                os.setstate(std::ios::failbit);
                return false;
            }
            os << "mesh_end\n";
        }else{
            os << "no_mesh\n";
        }

        os << "node_end\n";
    }

    os << "sketch_mesh_builder_end\n";
    os.precision(defaultprecision);
    return os.good();
}

bool Sketch_Mesh_Builder::read_from(std::istream &is, Sketch_Mesh_Builder &out){
    out.nodes_.clear();
    out.active_index_ = 0U;

    std::string line;
    std::optional<std::size_t> expected_count;
    bool found_end = false;

    while(std::getline(is, line)){
        std::istringstream iss(line);
        std::string keyword;
        iss >> keyword;

        if(keyword == "sketch_mesh_builder_end"){
            found_end = true;
            break;
        }else if(keyword == "sketch_mesh_builder_format_version"){
            int version = 0;
            iss >> version;
            if(version != 1) return false;
        }else if(keyword == "node_count"){
            std::size_t parsed_expected_count = 0U;
            if(!(iss >> parsed_expected_count)) return false;
            expected_count = parsed_expected_count;
        }else if(keyword == "active_node"){
            iss >> out.active_index_;
        }else if(keyword == "node_begin"){
            Sketch_Mesh_Builder_Node node;

            // Read until node_end.
            while(std::getline(is, line)){
                std::istringstream inner(line);
                std::string inner_keyword;
                inner >> inner_keyword;

                if(inner_keyword == "node_end"){
                    break;
                }else if(inner_keyword == "sketch_begin"){
                    if(!Sketch::read_from(is, node.sketch)){
                        return false;
                    }
                }else if(inner_keyword == "procedure_begin"){
                    // Reconstruct the full procedure block, including the already-consumed
                    // "procedure_begin" marker, so the canonical parser can validate it.
                    std::stringstream proc_ss;
                    proc_ss << "procedure_begin\n";

                    bool found_procedure_end = false;
                    while(std::getline(is, line)){
                        proc_ss << line << "\n";

                        std::istringstream piss(line);
                        std::string pkw;
                        piss >> pkw;
                        if(pkw == "procedure_end"){
                            found_procedure_end = true;
                            break;
                        }
                    }

                    if(!found_procedure_end) return false;

                    Sketch_Procedure proc;
                    if(!Sketch_Procedure::read_from(proc_ss, proc)) return false;
                    node.procedure = proc;
                }else if(inner_keyword == "mesh_begin"){
                    // Collect lines until mesh_end, then parse as OBJ.
                    std::stringstream mesh_ss;
                    while(std::getline(is, line)){
                        if(line == "mesh_end") break;
                        mesh_ss << line << "\n";
                    }
                    fv_surface_mesh<double, uint64_t> mesh;
                    if(ReadFVSMeshFromOBJ(mesh, mesh_ss)){
                        node.mesh = std::move(mesh);
                    }
                }else if(inner_keyword == "no_mesh"){
                    node.mesh = std::nullopt;
                }
            }
            out.nodes_.push_back(std::move(node));
        }
    }

    if(!found_end){
        return false;
    }
    if(expected_count.has_value() && (out.nodes_.size() != expected_count.value())){
        return false;
    }
    if(out.nodes_.empty()){
        out.nodes_.emplace_back();
    }
    out.active_index_ = std::min(out.active_index_, out.nodes_.size() - 1U);
    return true;
}

bool Sketch_Mesh_Builder::save_to_file(const std::filesystem::path &path, std::string *error_message) const {
    try{
        std::ofstream fout(path);
        if(!fout.is_open() || !fout.good()){
            if(error_message) *error_message = "Unable to open file for writing";
            return false;
        }
        if(!write_to(fout)){
            if(error_message) *error_message = "Unable to write builder data";
            return false;
        }
        fout.close();
        return true;
    }catch(const std::exception &e){
        if(error_message) *error_message = std::string("Exception while saving: ") + e.what();
        return false;
    }
}

std::optional<std::size_t> Sketch_Mesh_Builder::last_mesh_node_index() const {
    for(std::size_t idx = nodes_.size(); 0U < idx; --idx){
        if(nodes_.at(idx - 1U).mesh.has_value()){
            return idx - 1U;
        }
    }
    return {};
}

fv_surface_mesh<double, uint64_t>* Sketch_Mesh_Builder::last_mesh(){
    const auto idx = last_mesh_node_index();
    if(!idx){
        return nullptr;
    }
    return &nodes_.at(*idx).mesh.value();
}

const fv_surface_mesh<double, uint64_t>* Sketch_Mesh_Builder::last_mesh() const {
    const auto idx = last_mesh_node_index();
    if(!idx){
        return nullptr;
    }
    return &nodes_.at(*idx).mesh.value();
}

bool Sketch_Mesh_Builder::load_from_file(const std::filesystem::path &path, Sketch_Mesh_Builder &out, std::string *error_message){
    try{
        std::ifstream fin(path);
        if(!fin.is_open() || !fin.good()){
            if(error_message) *error_message = "Unable to open file for reading";
            return false;
        }
        if(!read_from(fin, out)){
            if(error_message) *error_message = "Unable to parse builder data";
            return false;
        }
        return true;
    }catch(const std::exception &e){
        if(error_message) *error_message = std::string("Exception while loading: ") + e.what();
        return false;
    }
}
