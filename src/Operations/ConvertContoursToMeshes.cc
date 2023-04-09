//ConvertContoursToMeshes.cc - A part of DICOMautomaton 2020. Written by hal clark.

#include <algorithm>
#include <cmath>
#include <cstdlib>            //Needed for exit() calls.
#include <fstream>
#include <functional>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <set> 
#include <stdexcept>
#include <string>    
#include <random>

#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMathIOOBJ.h"
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorLog.h"

#include "Explicator.h"

#include "../Contour_Boolean_Operations.h"
#include "../Structs.h"
#include "../Regex_Selectors.h"

#include "../Simple_Meshing.h"
#include "../Surface_Meshes.h"

#include "ConvertContoursToMeshes.h"


OperationDoc OpArgDocConvertContoursToMeshes(){
    OperationDoc out;
    out.name = "ConvertContoursToMeshes";

    out.desc = 
        "This routine creates a mesh from contours. There are four supported methods:"
        "\n\n"
        " - 'direct', which stitches"
        " together contours (polygons) by finding a correspondence between adjacent contours"
        " and zippering them together;"
        "\n\n"
        " - 'marching', which uses contours to first generate an image mask and"
        " then uses Marching Cubes to extract a mesh;"
        "\n\n"
        " - 'convex-hull', which builds the convex hull around the provided contours; and"
        "\n\n"
        " - 'contours', which converts the contours into thin triangle strips."
        "\n\n"
        " The 'direct' method, when it can be used appropriately, should be significantly faster than meshing"
        " via voxelization (e.g., marching cubes)."
        " It will also insert few (or zero) additional vertices on the original contour planes, meaning the"
        " resulting mesh can be sliced to give (nearly) the exact original contours."
        " However, please note that the 'direct' method is not robust and should only be expected"
        " to work for simple, sphere-like contours (i.e., convex polyhedra and mostly-convex polyhedra with only"
        " small concavities; see notes for additional information)."
        "\n\n"
        "The 'marching' method is robust, but slow since it requires conversion to an intermediate bitmask."
        " There is also a loss of spatial resolution due to use of bitmasks."
        "\n\n"
        "The 'convex-hull' method is reasonably accurate, but scales poorly."
        " It will also provide a zero-volume (i.e., non-manifold) surface if only a single contour is present."
        "\n\n"
        "The 'contours' method will not produce a manifold surface mesh, but will symmetrically extrude each"
        " contour to make a thin strip. This method is best suited for display purposes."
        ;

    out.notes.emplace_back(
        "The 'direct' method is experimental and currently relies on simple heuristics to find an adjacent contour"
        " correspondence."
    );
    out.notes.emplace_back(
        "Using the 'direct' method, meshes sliced on the same planes as the original contours *should* reproduce"
        " the original contours (barring numerical instabilities)."
        " In between the original slices, the mesh may exhibit distortions or obviously invalid correspondence"
        " with adjacent contours."
        " Using the 'marching' method, contours may vary somewhat."
    );
    out.notes.emplace_back(
        "For the 'direct' method, mesh 'pairing' on adjacent slices is evaluated using a mutual overlap heuristic."
        " The following adjacent slice pairing scenarios are supported:"
        " 1-0, 1-1, N-0, N-1, and N-M (for any N and M greater than 1)."
        " Adjacent contours with inconsistent orientations will either be reordered or wholly disregarded."
        " For N-0, N-1, and N-M pairings all contours in N (and M) are fused using with a simple distance heuristic;"
        " the fusion bridges are extended off the original contour plane so that mesh slicing will recover the"
        " original contours."
        " For 1-0 and N-0 pairings the 'hole' is filled by placing a single vertex offset from the occupied contour"
        " plane from the centroid and connecting all vertices; mesh slicing should also recover the original contours"
        " in this case."
    );
    out.notes.emplace_back(
        "For the 'direct' method, overlapping contours **on the same plane** are **not** currently supported."
        " Only the contour with the largest area will be retained."
    );
    out.notes.emplace_back(
        "The 'direct' method should only be expected to work for simple, sphere-like geometries (i.e., convex polyhedra)."
        " Some concavities can be tolerated, but not all."
        " For example, tori can only be meshed if the 'hole' is oriented away from the contour normal."
        " (Otherwise the 'hole' produces concentric contours -- which are not supported.)"
        " Contours representing convex polyhedra **should** result in manifold meshes, though they may"
        " not be watertight and if contour"
        " vertices are degenerate (or too close together numerically) meshes will fail to remain manifold."
    );
    out.notes.emplace_back(
        "The 'convex-hull' method uses an algorithm that scales poorly, especially when the contours are mostly convex"
        " (and thus have many vertices on the hull)."
    );

        
    out.args.emplace_back();
    out.args.back() = NCWhitelistOpArgDoc();
    out.args.back().name = "NormalizedROILabelRegex";
    out.args.back().default_val = ".*";


    out.args.emplace_back();
    out.args.back() = RCWhitelistOpArgDoc();
    out.args.back().name = "ROILabelRegex";
    out.args.back().default_val = ".*";


    out.args.emplace_back();
    out.args.back().name = "MeshLabel";
    out.args.back().desc = "A label to attach to the surface mesh.";
    out.args.back().default_val = "unspecified";
    out.args.back().expected = true;
    out.args.back().examples = { "unspecified", "body", "air", "bone", "invalid", "above_zero", "below_5.3" };

    
    out.args.emplace_back();
    out.args.back().name = "Method";
    out.args.back().desc = "There are currently three supported methods:"
                           " 'direct' -- a simplistic but fast contour stitching method;"
                           " 'marching' -- a method that first converts contours to a binary bitmask and then"
                           " uses Marching Cubes to extract meshes; and 'convex-hull' -- a robust routine that"
                           " only works for convex contours."
                           " See operation description and notes for more details.";
    out.args.back().default_val = "direct";
    out.args.back().expected = true;
    out.args.back().examples = { "direct",
                                 "marching",
                                 "convex-hull",
                                 "contours" };
    out.args.back().samples = OpArgSamples::Exhaustive;

    return out;
}



bool ConvertContoursToMeshes(Drover &DICOM_data,
                               const OperationArgPkg& OptArgs,
                               std::map<std::string, std::string>& /*InvocationMetadata*/,
                               const std::string& FilenameLex){

    Explicator X(FilenameLex);

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto NormalizedROILabelRegex = OptArgs.getValueStr("NormalizedROILabelRegex").value();
    const auto ROILabelRegex = OptArgs.getValueStr("ROILabelRegex").value();
    const auto MeshLabel = OptArgs.getValueStr("MeshLabel").value();
    const auto MethodStr = OptArgs.getValueStr("Method").value();

    //-----------------------------------------------------------------------------------------------------------------
    const auto NormalizedMeshLabel = X(MeshLabel);

    const auto direct_regex = Compile_Regex("^di?r?e?c?t?$");
    const auto marching_regex = Compile_Regex("^ma?r?c?h?i?n?g?$");
    const auto convex_regex = Compile_Regex("^conve?x?[-_]?h?u?l?l?$");
    const auto contours_regex = Compile_Regex("^conto?u?r?s?$");

    auto cc_all = All_CCs( DICOM_data );
    auto cc_ROIs = Whitelist( cc_all, { { "ROIName", ROILabelRegex },
                                        { "NormalizedROIName", NormalizedROILabelRegex } } );
    if(cc_ROIs.empty()){
        throw std::invalid_argument("No contours selected. Cannot continue.");
    }


    std::list<std::reference_wrapper<contour_collection<double>>> cc_ref;
    std::list<std::reference_wrapper<contour_of_points<double>>> cops;
    for(auto &cc_refw : cc_ROIs){
        // Contour collections.
        if(cc_refw.get().contours.empty()) continue;
        auto cc_base_ptr = reinterpret_cast<contour_collection<double> *>(&cc_refw.get());
        cc_ref.push_back( std::ref(*cc_base_ptr) );

        // Contours.
        for(auto &c : cc_refw.get().contours){
            if(c.points.empty()) continue;
            auto cop_base_ptr = reinterpret_cast<contour_of_points<double> *>(&c);
            cops.push_back( std::ref(*cop_base_ptr) );
        }
    }

    const auto common_metadata = contour_collection<double>().get_common_metadata( cc_ref , { } );
    fv_surface_mesh<double, uint64_t> amesh;

    // -------------------------- Direct (stitching) method --------------------------
    if(std::regex_match(MethodStr, direct_regex)){
        // Identify the unique planes.
        //const auto est_cont_normal = cc.contours.front().Estimate_Planar_Normal();
        const auto est_cont_normal = [&]() {
            auto est_cont_normal = Average_Contour_Normals(cc_ref);
            if(!est_cont_normal.isfinite()) {
                est_cont_normal = cc_ref.front().get().contours.front().Estimate_Planar_Normal();
            };
            return est_cont_normal;
        }();

        const auto contour_sep_eps = 0.005;
        auto ucps = Unique_Contour_Planes(cc_ref, est_cont_normal, contour_sep_eps);
        if(ucps.size() < 2){
            throw std::runtime_error("Unable to handle single contour planes at this time.");
        }
        ucps.reverse(); // Reverse sorted order so the bottom-most plane appears first.

        //const double fallback_spacing = 0.005; // in DICOM units (usually mm).
        //const auto cont_sep_range = std::abs(ucps.front().Get_Signed_Distance_To_Point( ucps.back().R_0 ));
        //const double est_cont_spacing = (ucps.size() <= 1) ? fallback_spacing : cont_sep_range / static_cast<double>(ucps.size() - 1);
        //const double est_cont_thickness = 0.5005 * est_cont_spacing; // Made slightly thicker to avoid gaps.
        const double contour_sep = Estimate_Contour_Separation(cc_ROIs, est_cont_normal, contour_sep_eps);

        // Add two empty contour planes to facilitate easier mesh closing on the boundaries.
        {
            const auto btm_plane = ucps.front();
            const auto top_plane = ucps.back();

            ucps.emplace_back(  plane<double>( top_plane.N_0, top_plane.R_0 + top_plane.N_0 * contour_sep) );
            ucps.emplace_front( plane<double>( btm_plane.N_0, btm_plane.R_0 - btm_plane.N_0 * contour_sep) );
        }

        const auto locate_contours_on_plane = [&](const plane<double> &P){
            decltype(cops) out;
            for(const auto &cop_refw : cops){
                const auto p = cop_refw.get().First_N_Point_Avg(1);
                const auto P_p_dist = std::abs(P.Get_Signed_Distance_To_Point(p));
                if(P_p_dist < (contour_sep * 0.5)){
                    out.emplace_back(cop_refw);
                }
            }
            return out;
        };

        using cop_refw_t = std::reference_wrapper<contour_of_points<double>>;

        const auto projected_contours_overlap = [&](const plane<double> &pln_A, cop_refw_t A,
                                                    const plane<double> &pln_B, cop_refw_t B) -> bool {
            // Evaluate whether the two contours, which will typically be on separate (but adjacent) planes, should be
            // linked together. This routine is a primitive that ideally would consider the Boolean overlap; at the moment a
            // slow and simplistic Boolean check that amounts to 'is the overlap nonzero?' is computed.

            // Check if *any* vertex appears inside the other polygon.
            //
            // This is not a perfect check, since contours can overlap without any vertex from either appearing inside the
            // other. (For example, consider two rectangles centred on the same point with one rotated pi/2 about the centre
            // relative to the other.) Nevertheless, it should work reasonable well for most realistic contours that are
            // more highly-sampled.
            for(const auto &p_A : A.get().points){
                if(B.get().Is_Point_In_Polygon_Projected_Orthogonally(pln_B, p_A)){
                    return true;
                }
            }
            for(const auto &p_B : B.get().points){
                if(A.get().Is_Point_In_Polygon_Projected_Orthogonally(pln_A, p_B)){
                    return true;
                }
            }
            return false;
        };

        const auto projected_contours_intersect = [&](const plane<double> &pln_A, cop_refw_t A,
                                                            const plane<double> &pln_B, cop_refw_t B) -> bool {
            // Checks whether two contors intersect
            // if a contour is completely enclosed by another contour, they will not intersect
            // Given that the contours overlap, if one does not enclose the other, they must intersect.
            // A countour is enclosed if *all* vertices of a contour lies inside the other polygon.
            // Should work well for most realistic contours that are highly sampled

            if (!projected_contours_overlap(pln_A, A, pln_B, B)){
                return false;
            }

            bool a_in_b = true;
            bool b_in_a = true;

            for(const auto &p_A : A.get().points){
                if(!B.get().Is_Point_In_Polygon_Projected_Orthogonally(pln_B, p_A)){
                    a_in_b = false;
                    break;
                }
            }
            for(const auto &p_B : B.get().points){
                if(!A.get().Is_Point_In_Polygon_Projected_Orthogonally(pln_A, p_B)){
                    b_in_a = false;
                    break;
                }
            }

            return !(a_in_b || b_in_a);
        };


        // Cycle over unique planes.
        for(auto m_cp_it = std::cbegin(ucps); m_cp_it != std::cend(ucps); ++m_cp_it){

            // Locate all contours on this plane.
            auto m_cops = locate_contours_on_plane(*m_cp_it);

            // Identify whether there are adjacent planes within the contour spacing on either side.
            auto l_cp_it = std::prev(m_cp_it);
            auto h_cp_it = std::next(m_cp_it);

            bool cap_roof_of_m_cops = false;

            if(l_cp_it == std::cend(ucps)){
                continue;
            }else if(l_cp_it != std::cend(ucps)){
                const auto l_cp_dist = std::abs(m_cp_it->Get_Signed_Distance_To_Point(l_cp_it->R_0));
                if((1.5 * contour_sep) < l_cp_dist) l_cp_it = std::cend(ucps);
            }
            if(h_cp_it != std::cend(ucps)){
                const auto h_cp_dist = std::abs(m_cp_it->Get_Signed_Distance_To_Point(h_cp_it->R_0));
                if((1.5 * contour_sep) < h_cp_dist) {
                    h_cp_it = std::cend(ucps);
                    cap_roof_of_m_cops = true;
                }
            }

            auto l_cops = locate_contours_on_plane(*l_cp_it);
            if( (l_cops.size() == 0) && (m_cops.size() == 0) ){
                throw std::logic_error("Unable to find any contours on contour plane.");
            }

            // Eliminate intersecting contours on both planes.
            for(auto m1_cop_it = std::begin(m_cops); m1_cop_it != std::end(m_cops); ){
                for(auto m2_cop_it = std::next(m1_cop_it); m2_cop_it != std::end(m_cops); ){
                    if(projected_contours_intersect(*m_cp_it, *m1_cop_it,
                                                    *m_cp_it, *m2_cop_it)){

                        // Cull the smaller contour.
                        const auto m1_area = std::abs( m1_cop_it->get().Get_Signed_Area() );
                        const auto m2_area = std::abs( m2_cop_it->get().Get_Signed_Area() );
                        
                        YLOGWARN("Found intersecting upper-plane contours, trimmed smallest-area contour");
                        if(m1_area < m2_area){
                            m1_cop_it = m_cops.erase(m1_cop_it);
                            m2_cop_it = std::next(m1_cop_it);
                        }else{
                            m2_cop_it = m_cops.erase(m2_cop_it);
                        }
                    }else{
                        ++m2_cop_it;
                    }
                }
                ++m1_cop_it;
            }
            for(auto l1_cop_it = std::begin(l_cops); l1_cop_it != std::end(l_cops); ){
                for(auto l2_cop_it = std::next(l1_cop_it); l2_cop_it != std::end(l_cops); ){
                    if(projected_contours_intersect(*l_cp_it, *l1_cop_it,
                                                    *l_cp_it, *l2_cop_it)){

                        // Cull the smaller contour.
                        const auto l1_area = std::abs( l1_cop_it->get().Get_Signed_Area() );
                        const auto l2_area = std::abs( l2_cop_it->get().Get_Signed_Area() );
                        
                        YLOGWARN("Found intersecting lower-plane contours, trimmed smallest-area contour");
                        if(l1_area < l2_area){
                            l1_cop_it = l_cops.erase(l1_cop_it);
                            l2_cop_it = std::next(l1_cop_it);
                        }else{
                            l2_cop_it = l_cops.erase(l2_cop_it);
                        }
                    }else{
                        ++l2_cop_it;
                    }
                }
                ++l1_cop_it;
            }

            // Identify how contours are paired together via computing the projected overlap.
            struct mapping_t {
                std::list<cop_refw_t> upper;
                std::list<cop_refw_t> lower;
            };
            std::list<mapping_t> pairings;

            // Pair based on some simple metrics.
            {
                struct pairing_t {
                    std::set<size_t> upper;
                    std::set<size_t> lower;
                };
                std::list<pairing_t> pairs;

                const auto set_union_is_empty = [](const std::set<size_t> &A, const std::set<size_t> &B) -> bool {
                    //if both sets are empty their union is not empty
                    if (A.empty() && B.empty()) return false;

                    for(const auto &a : A){
                        if(B.count(a) != 0){
                            return false;
                        }
                    }
                    return true;
                };

                const auto add_pair = [&](long u, long int l) -> void {
                    // Add a new pairing.
                    pairs.emplace_back();
                    if(0 <= u) pairs.back().upper.insert( static_cast<size_t>(u) );
                    if(0 <= l) pairs.back().lower.insert( static_cast<size_t>(l) );

                    // Continually cycle until no changes are made.
                    while(true){
                        // Cycle through all pairings, looking for duplicates.
                        bool altered = false;
                        for(auto a_it = std::begin(pairs); a_it != std::end(pairs); ){
                            for(auto b_it = std::next(a_it); b_it != std::end(pairs); ){
                                // If the same contour appears in multiple pairings, then both pairings can be merged.
                                if( !set_union_is_empty(a_it->upper, b_it->upper)
                                ||  !set_union_is_empty(a_it->lower, b_it->lower) ){
                                    altered = true;
                                    a_it->upper.merge( b_it->upper );
                                    a_it->lower.merge( b_it->lower );
                                    b_it = pairs.erase(b_it);
                                }else{
                                    ++b_it;
                                }
                            }
                            ++a_it;
                        }
                        if(!altered) break;
                    }
                    return;
                };

                // Search for overlap on the adjacent plane bi-directionally.
                {
                    long int N_m = 0;
                    for(auto m_cop_it = std::begin(m_cops); m_cop_it != std::end(m_cops); ++m_cop_it, ++N_m){
                        long int N_l = 0;
                        bool is_solitary = true;
                        for(auto l_cop_it = std::begin(l_cops); l_cop_it != std::end(l_cops); ++l_cop_it, ++N_l){
                            if(projected_contours_overlap(*m_cp_it, *m_cop_it,
                                                          *l_cp_it, *l_cop_it)){
                                add_pair(N_m, N_l);
                                is_solitary = false;
                            }
                        }
                        if(is_solitary) add_pair(N_m, -1);
                    }
                }
                {
                    long int N_l = 0;
                    for(auto l_cop_it = std::begin(l_cops); l_cop_it != std::end(l_cops); ++l_cop_it, ++N_l){
                        long int N_m = 0;
                        bool is_solitary = true;
                        for(auto m_cop_it = std::begin(m_cops); m_cop_it != std::end(m_cops); ++m_cop_it, ++N_m){
                            if(projected_contours_overlap(*m_cp_it, *m_cop_it,
                                                          *l_cp_it, *l_cop_it)){
                                add_pair(N_m, N_l);
                                is_solitary = false;
                            }
                        }
                        if(is_solitary) add_pair(-1, N_l);
                    }
                }

                // Convert from integer numbering to direct pairing info.
                for(const auto &p : pairs){
                    pairings.emplace_back();
                    for(const auto &u : p.upper){
                        pairings.back().upper.emplace_back( *std::next( std::begin(m_cops), u ) );
                    }
                    for(const auto &l : p.lower){
                        pairings.back().lower.emplace_back( *std::next( std::begin(l_cops), l ) );
                    }
                }
            }

            // These routines close the top or bottom of a mesh such that interpolation on the original slices will generate
            // the original contours. Slicing elsewhere should be sensible for convex polyhedra, but may not be for concave
            // polyhedra (e.g., horseshoes).
            const auto close_hole_in_roof = [&](cop_refw_t cop_refw) -> void {
                const auto old_face_count = amesh.vertices.size();
                const auto N_verts = cop_refw.get().points.size();

                for(const auto &p : cop_refw.get().points) amesh.vertices.emplace_back(p);
                const auto offset = (m_cp_it->N_0 * contour_sep * 0.49);
                const auto cap = cop_refw.get().Centroid() + offset;
                amesh.vertices.emplace_back(cap);

                for(size_t j = 0; j < N_verts; ++j){
                    size_t i = (j == 0) ? (N_verts - 1) : (j - 1);
                    const auto f_A = static_cast<uint64_t>(j + old_face_count);
                    const auto f_B = static_cast<uint64_t>(i + old_face_count);
                    const auto f_C = static_cast<uint64_t>(N_verts + old_face_count); // cap.
                    amesh.faces.emplace_back( std::vector<uint64_t>{{f_A, f_B, f_C}} );
                }
                return;
            };

            const auto close_hole_in_floor = [&](cop_refw_t cop_refw) -> void {
                const auto old_face_count = amesh.vertices.size();
                const auto N_verts = cop_refw.get().points.size();

                for(const auto &p : cop_refw.get().points) amesh.vertices.emplace_back(p);
                const auto offset = (m_cp_it->N_0 * contour_sep * -0.49);
                const auto cap = cop_refw.get().Centroid() + offset;
                amesh.vertices.emplace_back(cap);

                for(size_t j = 0; j < N_verts; ++j){
                    size_t i = (j == 0) ? (N_verts - 1) : (j - 1);
                    const auto f_A = static_cast<uint64_t>(i + old_face_count);
                    const auto f_B = static_cast<uint64_t>(j + old_face_count);
                    const auto f_C = static_cast<uint64_t>(N_verts + old_face_count); // cap.
                    amesh.faces.emplace_back( std::vector<uint64_t>{{f_A, f_B, f_C}} );
                }
                return;
            };

            const auto contours_are_enclosed = [&](const plane<double> &pln, cop_refw_t A, cop_refw_t B) -> bool {
                return (projected_contours_overlap(pln, A, pln, B) && !projected_contours_intersect(pln, A, pln, B));
            };

            const auto add_faces_to_mesh = [&](cop_refw_t cop_refw_A, cop_refw_t cop_refw_B,std::vector<std::array<size_t, 3UL>> new_faces) -> void{
                const auto old_face_count = amesh.vertices.size();
                for(const auto &p : cop_refw_A.get().points) amesh.vertices.emplace_back(p);
                for(const auto &p : cop_refw_B.get().points) amesh.vertices.emplace_back(p);
                for(const auto &fs : new_faces){
                    const auto f_A = static_cast<uint64_t>(fs[0] + old_face_count);
                    const auto f_B = static_cast<uint64_t>(fs[1] + old_face_count);
                    const auto f_C = static_cast<uint64_t>(fs[2] + old_face_count);
                    amesh.faces.emplace_back( std::vector<uint64_t>{{f_A, f_B, f_C}} );
                }
            };

            // Estimate connectivity and append triangles.
            for(auto &pcs : pairings){
                const auto N_upper = pcs.upper.size();
                const auto N_lower = pcs.lower.size();
                //YLOGINFO("Processing contour map from " << N_upper << " to " << N_lower);

                if( (N_upper != 0) && (N_lower == 0) ){
                    //If the upper plane contains 2 contours and one is enclosed in the other,
                    //tile the contours together instead of closing the floor
                    //this routine is for pipe like structures.
                    if(N_upper == 2){
                        if(contours_are_enclosed(*m_cp_it, pcs.upper.front(), pcs.upper.back())){
                            auto new_faces = Estimate_Contour_Correspondence(pcs.upper.front(), pcs.upper.back());
                            add_faces_to_mesh(pcs.upper.front(), pcs.upper.back(), new_faces);
                        }
                    }else{
                        for(const auto &cop_refw : pcs.upper) close_hole_in_floor(cop_refw);
                    }

                }else if( (N_upper == 0) && (N_lower != 0) ){
                    //if the upper plane contains 2 contours and one is enclosed in the other,
                    //tile the contours together instead of closing the roof
                    //this routine is for pipe like structures.
                    if (N_lower == 2){
                        if(contours_are_enclosed(*l_cp_it, pcs.lower.front(), pcs.lower.back())){
                            auto new_faces = Estimate_Contour_Correspondence(pcs.lower.front(), pcs.lower.back());
                            add_faces_to_mesh(pcs.lower.front(), pcs.lower.back(), new_faces);
                        }
                    }else{
                        for(const auto &cop_refw : pcs.lower) close_hole_in_roof(cop_refw);
                    }

                }else if( (N_upper == 1) && (N_lower == 1) ){
                    auto new_faces = Estimate_Contour_Correspondence(pcs.upper.front(), pcs.lower.front());
                    add_faces_to_mesh(pcs.upper.front(), pcs.lower.front(), new_faces);

                }else if((N_upper == 2) && (N_lower == 1) ){
                    //check if the upper plane contains enclosed contours
                        if(contours_are_enclosed(*m_cp_it, pcs.upper.front(), pcs.upper.back())){
                        //get contour areas to determine which contour is enclosed by the other
                        auto contour = std::begin(pcs.upper);
                        const auto c1_area = std::abs(contour->get().Get_Signed_Area());
                        ++contour;
                        const auto c2_area = std::abs(contour->get().Get_Signed_Area());

                        //cap the smaller contour and tile the larger contour with the lower plane contour
                        if  (c1_area < c2_area){
                            close_hole_in_floor(pcs.upper.front());
                            auto new_faces = Estimate_Contour_Correspondence(pcs.upper.back(), pcs.lower.front());
                            add_faces_to_mesh(pcs.upper.back(), pcs.lower.front(), new_faces);
                        }else{
                            close_hole_in_floor(pcs.upper.back());
                            auto new_faces = Estimate_Contour_Correspondence(pcs.upper.front(), pcs.lower.front());
                            add_faces_to_mesh(pcs.upper.front(), pcs.lower.front(), new_faces);
                        }
                    }else{
                        //do two-to-one branching
                    }

                }else if( (N_upper == 1) && (N_lower == 2) ){
                    //check if the lower plane contains enclosed contours
                    if(contours_are_enclosed(*l_cp_it, pcs.lower.front(), pcs.lower.back())){
                        //get contour areas to determine which contour is enclosed by the other
                        auto contour = std::begin(pcs.lower);
                        const auto c1_area = std::abs(contour->get().Get_Signed_Area());
                        ++contour;
                        const auto c2_area = std::abs(contour->get().Get_Signed_Area());

                        //cap the smaller contour and tile the larger contour with the lower plane contour
                        if  (c1_area < c2_area){
                            close_hole_in_roof(pcs.lower.front());
                            auto new_faces = Estimate_Contour_Correspondence(pcs.lower.back(), pcs.upper.front());
                            add_faces_to_mesh(pcs.lower.back(), pcs.upper.front(), new_faces);
                        }else{
                            close_hole_in_roof(pcs.lower.back());
                            auto new_faces = Estimate_Contour_Correspondence(pcs.lower.front(), pcs.upper.front());
                            add_faces_to_mesh(pcs.lower.front(), pcs.upper.front(), new_faces);
                        }
                    }else{
                        //do one-to-two branching
                    }

                }else{
                    //YLOGINFO("Performing N-to-N meshing..");

                    //routine for hollow structures with an inner contour and an outer contour on both planes
                    if((N_upper == 2) && (N_lower == 2)){
                        //check if both planes have enclosed contours
                        if (contours_are_enclosed(*m_cp_it, pcs.upper.front(), pcs.upper.back())
                            && contours_are_enclosed(*l_cp_it, pcs.lower.front(), pcs.lower.back())){
                            //check areas to determine which the inner and outer contours are on both planes

                            //assume 2nd contour is the inner to begin with
                            auto upper_inner = pcs.upper.back();
                            auto upper_outer = pcs.upper.front();

                            auto contour = std::begin(pcs.upper);
                            auto c1_area = std::abs(contour->get().Get_Signed_Area());
                            ++contour;
                            auto c2_area = std::abs(contour->get().Get_Signed_Area());

                            if (c1_area < c2_area){
                                upper_inner = pcs.upper.front();
                                upper_outer = pcs.upper.back();
                            }

                            //do the same for the lower plane
                            auto lower_inner = pcs.lower.back();
                            auto lower_outer = pcs.lower.front();

                            contour = std::begin(pcs.lower);
                            c1_area = std::abs(contour->get().Get_Signed_Area());
                            ++contour;
                            c2_area = std::abs(contour->get().Get_Signed_Area());

                            if (c1_area < c2_area){
                                lower_inner = pcs.lower.front();
                                lower_outer = pcs.lower.back();
                            }

                            //connect inner contours together and outer contours together
                            auto new_faces = Estimate_Contour_Correspondence(lower_inner, upper_inner);
                            add_faces_to_mesh(lower_inner, upper_inner, new_faces);
                            
                            new_faces = Estimate_Contour_Correspondence(lower_outer, upper_outer);
                            add_faces_to_mesh(lower_outer, upper_outer, new_faces);
                            //move to next iteration of for loop since we have tiled it
                            continue;
                        }
                    }

                    auto ofst_upper = m_cp_it->N_0 * contour_sep * -0.49;
                    auto ofst_lower = m_cp_it->N_0 * contour_sep *  0.49;
            
                    // will modify contours in m_cops and l_cops, ok if only processing in one direction
                    auto amal_upper = Minimally_Amalgamate_Contours(m_cp_it->N_0, ofst_upper, pcs.upper); 
                    auto amal_lower = Minimally_Amalgamate_Contours(m_cp_it->N_0, ofst_lower, pcs.lower); 

                    /*
                    // Leaving this here for future debugging, for which it will no-doubt be needed...
                    {
                        const auto amal_cop_str = amal_upper.write_to_string();
                        const auto fname = Get_Unique_Sequential_Filename("/tmp/amal_upper_", 6, ".txt");
                        OverwriteStringToFile(amal_cop_str, fname);
                    }
                    {
                        const auto amal_cop_str = amal_lower.write_to_string();
                        const auto fname = Get_Unique_Sequential_Filename("/tmp/amal_lower_", 6, ".txt");
                        OverwriteStringToFile(amal_cop_str, fname);
                    }
                    */
                    auto new_faces = Estimate_Contour_Correspondence(std::ref(amal_upper), std::ref(amal_lower));
                    add_faces_to_mesh(std::ref(amal_upper), std::ref(amal_lower), new_faces);
                }

            }
            //caps contours that have no corresponding contours on the lower plane
            if (cap_roof_of_m_cops) {
                for (auto &cop : m_cops) {
                    close_hole_in_roof(cop);
                }
            }
        }

        // Create the mesh.
        amesh.recreate_involved_face_index();
        const auto machine_eps = std::sqrt( 10.0 * std::numeric_limits<double>::epsilon() );
        amesh.merge_duplicate_vertices(machine_eps);
        amesh.recreate_involved_face_index();

        /*
        // Leaving this here for future debugging, for which it will no-doubt be needed...
        std::ofstream os("/tmp/mesh.obj");
        if(!WriteFVSMeshToOBJ(amesh, os)){
            throw std::runtime_error("Unable to write mesh to file.");
        }
        */


    }else if(std::regex_match(MethodStr, marching_regex)){
        auto meshing_params = dcma_surface_meshes::Parameters();
        amesh = dcma_surface_meshes::Estimate_Surface_Mesh_Marching_Cubes( cc_ref, meshing_params );

    }else if(std::regex_match(MethodStr, convex_regex)){
        // Gather all available vertices.
        for(const auto &cc_refw : cc_ROIs){
            for(const auto &c : cc_refw.get().contours){
                for(const auto &p : c.points){
                    amesh.vertices.emplace_back(p);

                }
            }
        }

//        // Sort and remove exact duplicates.
//        std::sort( std::begin(amesh.vertices), std::end(amesh.vertices) );
//        amesh.vertices.erase( std::unique( std::begin(amesh.vertices), std::end(amesh.vertices) ),
//                              std::end(amesh.vertices) );

        YLOGINFO("Generating convex hull from " << amesh.vertices.size() << " vertices");

/*
        // Sort and remove near duplicates by truncating decimal precision.
        const auto N_digits = 3;
        const auto truncate = [](const vec3<double> &v, long int N_digits){
            const auto pwr = std::pow(10.0, static_cast<double>(N_digits));
            return vec3<double>(  static_cast<double>( static_cast<int64_t>( std::round(v.x * pwr) ) ) / pwr,
                                  static_cast<double>( static_cast<int64_t>( std::round(v.y * pwr) ) ) / pwr,
                                  static_cast<double>( static_cast<int64_t>( std::round(v.z * pwr) ) ) / pwr );
        };
        std::sort( std::begin(amesh.vertices),
                   std::end(amesh.vertices),
                   [&truncate,&N_digits](const vec3<double>& A, const vec3<double>& B){
                       return truncate(A, N_digits) < truncate(B, N_digits);
                   } );
        amesh.vertices.erase( std::unique( std::begin(amesh.vertices),
                                           std::end(amesh.vertices),
                                           [&truncate,&N_digits](const vec3<double>& A, const vec3<double>& B){
                                               return truncate(A, N_digits) == truncate(B, N_digits);
                                           } ),
                              std::end(amesh.vertices) );

        YLOGINFO("Deduplicated vertices via truncation to " << amesh.vertices.size() << " vertices");
*/

        // Randomly shuffle vertices to (hopefully) speed up hull extraction.
//        std::random_device rd;
//        std::mt19937 rng(rd());
//        std::shuffle(std::begin(amesh.vertices), std::end(amesh.vertices), rng);

        // Construct the convex hull.
        using vert_vec_t = decltype(std::begin(amesh.vertices));
        amesh.faces = Convex_Hull_3<vert_vec_t,uint64_t>( std::begin(amesh.vertices),
                                                          std::end(amesh.vertices) );

        // Prune unneeded vertices.
        amesh.remove_disconnected_vertices();
        //amesh.recreate_involved_face_index();

    }else if(std::regex_match(MethodStr, contours_regex)){
        const auto orientation_normal = Average_Contour_Normals(cc_ROIs);
        const double dz = 1E-3;

        for(const auto &cc_refw : cc_ROIs){
            for(const auto &c : cc_refw.get().contours){
                const auto first_vert = static_cast<uint64_t>(amesh.vertices.size());
                const auto N_verts = static_cast<uint64_t>(c.points.size());
                const auto N_new_verts = N_verts * 2U;
                auto i = static_cast<uint64_t>(0U);
                for(const auto &p : c.points){
                    amesh.vertices.emplace_back(p + orientation_normal * dz);
                    amesh.vertices.emplace_back(p - orientation_normal * dz);

                    const auto v_A = first_vert + (i + 0U);
                    const auto v_B = first_vert + (i + 1U);
                    const auto v_C = first_vert + ((i + 2U) % N_new_verts);
                    const auto v_D = first_vert + ((i + 3U) % N_new_verts);

                    amesh.faces.emplace_back( std::vector<uint64_t>{{ v_A, v_B, v_C }} );
                    amesh.faces.emplace_back( std::vector<uint64_t>{{ v_B, v_D, v_C }} );
                    i += 2;
                }
            }
        }

        YLOGINFO("Generating contours mesh from " << amesh.vertices.size() << " vertices");

    }else{
        throw std::invalid_argument("Unrecognized method");
    }

    DICOM_data.smesh_data.emplace_back( std::make_unique<Surface_Mesh>() );
    DICOM_data.smesh_data.back()->meshes = amesh;
    DICOM_data.smesh_data.back()->meshes.metadata = common_metadata;
    DICOM_data.smesh_data.back()->meshes.metadata["MeshLabel"] = MeshLabel;
    DICOM_data.smesh_data.back()->meshes.metadata["NormalizedMeshLabel"] = NormalizedMeshLabel;
    DICOM_data.smesh_data.back()->meshes.metadata["Description"] = "Extracted surface mesh";

    return true;
}
