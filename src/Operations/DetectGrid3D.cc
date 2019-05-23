//DetectGrid3D.cc - A part of DICOMautomaton 2019. Written by hal clark.

#include <experimental/any>
#include <experimental/optional>
#include <functional>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <regex>
#include <random>
#include <stdexcept>
#include <string>    
#include <algorithm>    

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../Insert_Contours.h"
#include "../YgorImages_Functors/ConvenienceRoutines.h"
#include "../YgorImages_Functors/Grouping/Misc_Functors.h"
#include "../YgorImages_Functors/Compute/Volumetric_Neighbourhood_Sampler.h"

#include "DetectGrid3D.h"
#include "YgorImages.h"
#include "YgorString.h"       //Needed for GetFirstRegex(...)
#include "YgorStats.h"       //Needed for Stats:: namespace.

/*
#include <boost/geometry.hpp>
#include <boost/geometry/geometries/point.hpp>
#include <boost/geometry/geometries/box.hpp>

#include <boost/geometry/index/parameters.hpp>
#include <boost/geometry/index/rtree.hpp>

#include <boost/geometry/algorithms/within.hpp>
*/

#include <eigen3/Eigen/Dense>
#include <eigen3/Eigen/Eigenvalues>
#include <eigen3/Eigen/SVD>

#include "YgorClustering.hpp"

// Used to store state about a fitted 3D grid.
struct Grid_Context {
    // Controls how the corresponding points are determined. See operation documentation for more information.
    size_t grid_sampling;

    // The distance between nearest-neighbour grid lines.
    // Note: an isotropic grid is assumed, so this number is valid for all three directions.
    double grid_sep = std::numeric_limits<double>::quiet_NaN();

    // A location in space in which a grid line intersection occurs.
    vec3<double> current_grid_anchor = vec3<double>(0.0, 0.0, 0.0);

    // The grid line directions. These should always be orthonormal.
    vec3<double> current_grid_x = vec3<double>(1.0, 0.0, 0.0);
    vec3<double> current_grid_y = vec3<double>(0.0, 1.0, 0.0);
    vec3<double> current_grid_z = vec3<double>(0.0, 0.0, 1.0);

    // A number describing how good this grid fits the point cloud.
    // The lower the number, the better the fit.
    double score = std::numeric_limits<double>::quiet_NaN();
};

// Used to cache working state while fitting a 3D grid.
struct ICP_Context {

    // A point selected by the RANSAC procedure. Only the near vicinity of this point is used for coarse grid fitting.
    vec3<double> ransac_centre = vec3<double>(0.0, 0.0, 0.0);

    // A point selected by the ICP procedure. The optimal grid rotation about this affixed point is estimated.
    vec3<double> rot_centre    = vec3<double>(0.0, 0.0, 0.0);

    // Point cloud points participating in a single RANSAC phase.
    //
    // This list is regenerated for each round of RANSAC. Only some point cloud points within a fixed distance from
    // some randomly-selected point will be retained. The points are not altered, just copied for ease-of-use.
    using pcp_c_t = decltype(Point_Cloud().points); // Point_Cloud point container type.
    pcp_c_t cohort;

    // Cohort points projected into a single volumetric proto cell.
    pcp_c_t p_cell;

    // Holds projected points for each cohort point.
    //
    // The projection is on the surface of the proto cell.
    pcp_c_t p_corr;
};


static
void
Write_XYZ( const std::string &fname,
           decltype(Point_Cloud().points) points ){

    // This routine writes or appends to a simple "XYZ"-format file which contains point cloud vertices.
    //
    // Appending to any (valid) XYZ file will create a valid combined point cloud.
    std::ofstream OF(fname, std::ios::out | std::ios::app);
    OF << "# XYZ point cloud file." << std::endl;
    for(const auto &pp : points){
        const auto v = pp.first;
        OF << v.x << " " << v.y << " " << v.z << std::endl;
    }
    OF.close();
    return;
};

static
void
Write_PLY( const std::string &fname,
           decltype(Point_Cloud().points) points ){

    // This routine write to a simple "PLY"-format file which contains point cloud vertices.
    //
    // An existing file will be overwritten.
    std::ofstream OF(fname, std::ios::out);
    OF << "ply" << "\n"
       << "format ascii 1.0" << "\n"
       << "comment This file contains a point cloud." << "\n"
       << "comment This file was produced by DICOMautomaton." << "\n"
       << "element vertex " << points.size() << "\n"
       << "property double x" << "\n"
       << "property double y" << "\n"
       << "property double z" << "\n"
       << "end_header" << std::endl;
    for(const auto &pp : points){
        const auto v = pp.first;
        OF << v.x << " " << v.y << " " << v.z << std::endl;
    }
    OF.close();
    return;
};

static
void
Write_Cube_OBJ(const std::string &fname,
               const vec3<double> &corner,
               const vec3<double> &edge1,
               const vec3<double> &edge2,
               const vec3<double> &edge3 ){
        
    // This routine takes a corner vertex and three edge vectors (originating from the corner) and writes a cube.
    // The edges need not be orthogonal. They can also have different lengths; the lengths provide the cube size.
    //
    // Note: This routine creates relative OBJ files. It can append to an existing (valid, relative) OBJ file. 
    //       The resulting file will be valid, all existing geometry will remain valid, and the new geometry will be
    //       valid too. This routine can also append to non-relative OBJ files and everything will be valid, but
    //       later appending a non-relative file will cause the additions to be invalid. So it is best not to mix
    //       relative and non-relative geometry if possible.
    const auto A = corner;
    const auto B = corner + edge1;
    const auto C = corner + edge1 + edge3;
    const auto D = corner + edge3;

    const auto E = corner + edge2;
    const auto F = corner + edge1 + edge2;
    const auto G = corner + edge1 + edge2 + edge3;
    const auto H = corner + edge2 + edge3;

    std::ofstream OF(fname, std::ios::out | std::ios::app);
    OF << "# Wavefront OBJ file." << std::endl;

    // Vertices.
    OF << "v " << A.x << " " << A.y << " " << A.z << "\n" 
       << "v " << B.x << " " << B.y << " " << B.z << "\n" 
       << "v " << C.x << " " << C.y << " " << C.z << "\n" 
       << "v " << D.x << " " << D.y << " " << D.z << "\n" 

       << "v " << E.x << " " << E.y << " " << E.z << "\n" 
       << "v " << F.x << " " << F.y << " " << F.z << "\n" 
       << "v " << G.x << " " << G.y << " " << G.z << "\n" 
       << "v " << H.x << " " << H.y << " " << H.z << std::endl; 

    // Faces (n.b. one-indexed, not zero-indexed).
    OF << "f -8 -7 -4" << "\n"
       << "f -7 -3 -4" << "\n"

       << "f -7 -6 -3" << "\n"
       << "f -6 -2 -3" << "\n"

       << "f -6 -5 -2" << "\n"
       << "f -5 -1 -2" << "\n"

       << "f -5 -8 -1" << "\n"
       << "f -8 -4 -1" << "\n"

       << "f -4 -3 -2" << "\n"
       << "f -2 -1 -4" << "\n"

       << "f -7 -8 -6" << "\n"
       << "f -8 -5 -6" << std::endl;

    // Edges (n.b. one-indexed, not zero-indexed).
    OF << "l -8 -7" << "\n"
       << "l -7 -6" << "\n"
       << "l -6 -5" << "\n"
       << "l -5 -8" << "\n"

       << "l -8 -4" << "\n"
       << "l -7 -3" << "\n"
       << "l -6 -2" << "\n"
       << "l -5 -1" << "\n"

       << "l -4 -3" << "\n"
       << "l -3 -2" << "\n"
       << "l -2 -1" << "\n"
       << "l -1 -4" << std::endl;

    OF.close();
    return;
}

static
void
Insert_Grid_Contours(Drover &DICOM_data,
                     const std::string &ROILabel,
                     decltype(Point_Cloud().points) points,
                     const vec3<double> &corner,
                     const vec3<double> &edge1,
                     const vec3<double> &edge2,
                     const vec3<double> &edge3 ){
        
    // This routine takes a grid intersection (i.e., a corner of a single grid voxel) and three edge vectors that
    // describe where the adjacent cells are and draws a 3D grid that tiles the region occupied by the given points.
    //

    // Find the grid lines that enclose the given points.
    Stats::Running_MinMax<double> mm_x;
    Stats::Running_MinMax<double> mm_y;
    Stats::Running_MinMax<double> mm_z;
    for(const auto &pp : points){
        const auto P = pp.first;

        mm_x.Digest(P.x);
        mm_y.Digest(P.y);
        mm_z.Digest(P.z);
    }

    const auto bounding_corner = vec3<double>( mm_x.Current_Min(),
                                               mm_y.Current_Min(),
                                               mm_z.Current_Min() );
    auto v = bounding_corner;

    const auto dx = (corner - v).Dot(edge1.unit());
    const auto dy = (corner - v).Dot(edge2.unit());
    const auto dz = (corner - v).Dot(edge3.unit());
    v += (edge1.unit() * std::fmod(dx, edge1.length()) * 1.0)
      +  (edge2.unit() * std::fmod(dy, edge2.length()) * 1.0)
      +  (edge3.unit() * std::fmod(dz, edge3.length()) * 1.0);

    if(false){ // Debugging.
        const auto dx = std::remainder( (v - corner).Dot(edge1.unit()), edge1.length() );
        const auto dy = std::remainder( (v - corner).Dot(edge2.unit()), edge2.length() );
        const auto dz = std::remainder( (v - corner).Dot(edge3.unit()), edge3.length() );
        FUNCINFO("dx, dy, dz = " << dx << "  " << dy << "  " << dz);
    }

    // The number of lines needed to bound the point cloud.
    const auto N_lines_1 = static_cast<long int>( (mm_x.Current_Max() - mm_x.Current_Min()) / edge1.length() );
    const auto N_lines_2 = static_cast<long int>( (mm_y.Current_Max() - mm_y.Current_Min()) / edge2.length() );
    const auto N_lines_3 = static_cast<long int>( (mm_z.Current_Max() - mm_z.Current_Min()) / edge3.length() );

    // Create planes for every grid line.
    //
    // Note: one extra grid line will flank the point cloud on all sides. 
    std::vector<plane<double>> planes;
    for(long int i = -2; i < (2+N_lines_1); ++i){
        const auto l_corner = v + (edge1 * (i * 1.0));
        planes.emplace_back(edge1, l_corner);
    }
    for(long int i = -2; i < (2+N_lines_2); ++i){
        const auto l_corner = v + (edge2 * (i * 1.0));
        planes.emplace_back(edge2, l_corner);
    }
    for(long int i = -2; i < (2+N_lines_3); ++i){
        const auto l_corner = v + (edge3 * (i * 1.0));
        planes.emplace_back(edge3, l_corner);
    }

    // Save the planes as contours on an image.
    {
        const std::string ImageSelectionStr = "last";
        const std::string NormalizedROILabel = ROILabel;
        const auto contour_thickness = 0.001; // in DICOM units (i.e., mm).

        std::list<contours_with_meta> new_contours;

        auto IAs_all = All_IAs( DICOM_data );
        auto IAs = Whitelist( IAs_all, ImageSelectionStr );
        if(IAs.empty()){
            throw std::runtime_error("No images to place contours onto. Cannot continue.");
        }
        for(auto & iap_it : IAs){
            if((*iap_it)->imagecoll.images.empty()) throw std::invalid_argument("Unable to find images to place contours on.");
            for(auto &animg : (*iap_it)->imagecoll.images){

                auto contour_metadata = animg.metadata;
                contour_metadata["ROIName"] = ROILabel;
                contour_metadata["NormalizedROIName"] = NormalizedROILabel;

                new_contours.emplace_back();

                for(const auto &aplane : planes){
                    //contour_metadata["OutlineColour"] = PFC.leaf_line_colour[leaf_num];
                    //contour_metadata["PicketFenceLeafNumber"] = std::to_string(leaf_num);

                    try{ // Will throw if grossly out-of-bounds, but it's a pain to pre-filter -- ignore exceptions for now... TODO
                        Inject_Thin_Plane_Contour(animg,
                                                 aplane, 
                                                 new_contours.back(),
                                                 contour_metadata, 
                                                 contour_thickness);
                    }catch(const std::exception &){};
                }
            }
        }

        // Insert contours.
        if(DICOM_data.contour_data == nullptr){
            std::unique_ptr<Contour_Data> output (new Contour_Data());
            DICOM_data.contour_data = std::move(output);
        }
        DICOM_data.contour_data->ccs.splice( DICOM_data.contour_data->ccs.end(), new_contours );

    }
    return;
}

static
void
Project_Into_Proto_Cube( Grid_Context &GC,
                        ICP_Context &ICPC ){
    // Using the current grid axes directions and anchor point, project all points into the proto cell.
    auto p_cell_it = std::begin(ICPC.p_cell);
    for(const auto &pp : ICPC.cohort){
        const auto P = pp.first;

        // Vector rel. to grid anchor.
        const auto R = (P - GC.current_grid_anchor);

        // Vector within the unit cube, described in the grid axes basis.
        auto C_x = std::fmod( R.Dot(GC.current_grid_x), GC.grid_sep );
        auto C_y = std::fmod( R.Dot(GC.current_grid_y), GC.grid_sep );
        auto C_z = std::fmod( R.Dot(GC.current_grid_z), GC.grid_sep );
        if(C_x < 0.0) C_x += GC.grid_sep; // Ensure the result is within the cube (fmod can be negative).
        if(C_y < 0.0) C_y += GC.grid_sep;
        if(C_z < 0.0) C_z += GC.grid_sep;

        const auto C = GC.current_grid_anchor
                     + GC.current_grid_x * C_x
                     + GC.current_grid_y * C_y
                     + GC.current_grid_z * C_z;

        // Verify that the projected point is indeed within the unit cube.
        if(false){ // Debugging.
            plane<double> pl_x_A( GC.current_grid_x, GC.current_grid_anchor );
            plane<double> pl_y_A( GC.current_grid_y, GC.current_grid_anchor );
            plane<double> pl_z_A( GC.current_grid_z, GC.current_grid_anchor );

            plane<double> pl_x_B( GC.current_grid_x, GC.current_grid_anchor + GC.current_grid_x * GC.grid_sep );
            plane<double> pl_y_B( GC.current_grid_y, GC.current_grid_anchor + GC.current_grid_y * GC.grid_sep );
            plane<double> pl_z_B( GC.current_grid_z, GC.current_grid_anchor + GC.current_grid_z * GC.grid_sep );

            if(  ( pl_x_A.Is_Point_Above_Plane(C) == pl_x_B.Is_Point_Above_Plane(C) )
            ||   ( pl_y_A.Is_Point_Above_Plane(C) == pl_y_B.Is_Point_Above_Plane(C) )
            ||   ( pl_z_A.Is_Point_Above_Plane(C) == pl_z_B.Is_Point_Above_Plane(C) ) ){
                std::stringstream ss;
                ss << "Projection is outside the cube: "
                   << C 
                   << " originally: " 
                   << P
                   << " anchor: " 
                   << GC.current_grid_anchor
                   << " axes: "
                   << GC.current_grid_x * GC.grid_sep
                   << " " 
                   << GC.current_grid_y * GC.grid_sep
                   << " " 
                   << GC.current_grid_z * GC.grid_sep;
                throw std::logic_error(ss.str());
            }
        }

        p_cell_it->first = C;
        ++p_cell_it;
    }
    return;
}

static
void
Translate_Grid_Optimally( Grid_Context &GC,
                          ICP_Context &ICPC ){

    // Determine the optimal translation.
    //
    // Along each grid direction, the distance from each point to the nearest grid plane will be recorded.
    // Note that we dramatically simplify determining distance to the cube face by adding or subtracting half the
    // scalar distance; since all points have been projecting into the unit cube, at most the point will be
    // 0.5*separation from the nearest plane. Thus if we subtract 1.0*separation for the points in the upper half, we can
    // use simple 1D distribution analysis to determine optimal translations of the anchor point.
    std::vector<double> dist_x;
    std::vector<double> dist_y;
    std::vector<double> dist_z;

    dist_x.reserve(ICPC.p_cell.size());
    dist_y.reserve(ICPC.p_cell.size());
    dist_z.reserve(ICPC.p_cell.size());
    {
        auto p_cell_it = std::begin(ICPC.p_cell);
        for(const auto &pp : ICPC.cohort){
            //const auto P = pp.first;
            const auto C = p_cell_it->first - GC.current_grid_anchor;

            const auto proj_x = GC.current_grid_x.Dot(C);
            const auto proj_y = GC.current_grid_y.Dot(C);
            const auto proj_z = GC.current_grid_z.Dot(C);

            const auto dx = (0.5*GC.grid_sep < proj_x) ? proj_x - GC.grid_sep : proj_x;
            const auto dy = (0.5*GC.grid_sep < proj_y) ? proj_y - GC.grid_sep : proj_y;
            const auto dz = (0.5*GC.grid_sep < proj_z) ? proj_z - GC.grid_sep : proj_z;

            dist_x.emplace_back(dx);
            dist_y.emplace_back(dy);
            dist_z.emplace_back(dz);

            ++p_cell_it;
        }
    }

    const auto shift_x = Stats::Mean(dist_x);
    const auto shift_y = Stats::Mean(dist_y);
    const auto shift_z = Stats::Mean(dist_z);

    GC.current_grid_anchor += GC.current_grid_x * shift_x
                         + GC.current_grid_y * shift_y
                         + GC.current_grid_z * shift_z;
    return;
}

static
void
Find_Corresponding_Points( Grid_Context &GC,
                           ICP_Context &ICPC ){

    // This routine takes every proto cube projected point and projects it onto the faces, edges, or corners of the
    // proto cube. The projection that is the smallest distance from the proto cube projected point is kept.
    //
    // Note: There is likely a faster way to do the following using the same approach as the optimal translation routine.
    // This way is easy to debug and reason about.

    if(ICPC.p_corr.size() != ICPC.cohort.size() ){
        throw std::logic_error("Insufficient working space allocated. Cannot continue.");
    }

    // There are three different cases that depend on how the grid is sampled. They all amount to the same basic
    // procedure -- project the proto cube point to the boundary of the proto cube.
    // Creates plane for all faces.

    const vec3<double> NaN_vec3( std::numeric_limits<double>::quiet_NaN(),
                                 std::numeric_limits<double>::quiet_NaN(),
                                 std::numeric_limits<double>::quiet_NaN() );

    const auto anchor = GC.current_grid_anchor;
    const auto edge_x = GC.current_grid_x * GC.grid_sep;
    const auto edge_y = GC.current_grid_y * GC.grid_sep;
    const auto edge_z = GC.current_grid_z * GC.grid_sep;

    // Corners of the proto cube.
    const auto c_A = anchor;
    const auto c_B = anchor + edge_x;
    const auto c_C = anchor + edge_x + edge_z;
    const auto c_D = anchor + edge_z;

    const auto c_E = anchor + edge_y;
    const auto c_F = anchor + edge_y + edge_x;
    const auto c_G = anchor + edge_y + edge_x + edge_z;
    const auto c_H = anchor + edge_y + edge_z;

    // List of planar faces of the proto cube.
    std::vector<plane<double>> planes;

    planes.emplace_back( GC.current_grid_x, anchor );
    planes.emplace_back( GC.current_grid_y, anchor );
    planes.emplace_back( GC.current_grid_z, anchor );

    planes.emplace_back( GC.current_grid_x, anchor + edge_x );
    planes.emplace_back( GC.current_grid_y, anchor + edge_y );
    planes.emplace_back( GC.current_grid_z, anchor + edge_z );


    // List of corners of the proto cube.
    std::vector<vec3<double>> corners = { {
        c_A, c_B, c_C, c_D,
        c_E, c_F, c_G, c_H
    } };

    // List of lines that overlap with the edge line segments.
    std::vector<line<double>> lines;

    lines.emplace_back( c_A, c_B );
    lines.emplace_back( c_B, c_C );
    lines.emplace_back( c_C, c_D );
    lines.emplace_back( c_D, c_A );

    lines.emplace_back( c_A, c_E );
    lines.emplace_back( c_B, c_F );
    lines.emplace_back( c_C, c_G );
    lines.emplace_back( c_D, c_H );

    lines.emplace_back( c_E, c_F );
    lines.emplace_back( c_F, c_G );
    lines.emplace_back( c_G, c_H );
    lines.emplace_back( c_H, c_E );


    // Find the corresponding point for each projected proto cube point.
    auto closest_dist = std::numeric_limits<double>::quiet_NaN();
    auto closest_proj = NaN_vec3;
    auto c_it = std::begin(ICPC.p_corr);
    for(const auto &pp : ICPC.p_cell){
        const auto P = pp.first;

        closest_dist = std::numeric_limits<double>::quiet_NaN();
        closest_proj = NaN_vec3;

        if(false){
        }else if(GC.grid_sampling == 1){ // Grid cell corners (i.e., "0D" grid intersections) are sampled.
            for(const auto &c : corners){
                const auto dist = c.distance(P);
                if(!std::isfinite(closest_dist) || (dist < closest_dist)){
                    closest_dist = dist;
                    closest_proj = c;
                }
            }

        }else if(GC.grid_sampling == 2){ // Grid cell edges (i.e., 1D grid lines) are sampled.
            for(const auto &l : lines){
                const auto dist = l.Distance_To_Point(P);
                if(!std::isfinite(closest_dist) || (dist < closest_dist)){
                    const auto proj = l.Project_Point_Orthogonally(P);
                    if(!proj.isfinite()){
                        throw std::logic_error("Projected point is not finite. Cannot continue.");
                    }
                    closest_dist = dist;
                    closest_proj = proj;
                }
            }

        }else if(GC.grid_sampling == 3){ // Grid cell faces (i.e., 2D planar faces) are sampled.
            for(const auto &pl : planes){
                const auto dist = std::abs(pl.Get_Signed_Distance_To_Point(P));
                if(!std::isfinite(closest_dist) || (dist < closest_dist)){
                    const auto proj = pl.Project_Onto_Plane_Orthogonally(P);
                    if(!proj.isfinite()){
                        throw std::logic_error("Projected point is not finite. Cannot continue.");
                    }
                    closest_dist = dist;
                    closest_proj = proj;
                }
            }
        }else{
            throw std::logic_error("Invalid grid sampling method. Cannot continue.");
        }

        c_it->first = closest_proj;
        ++c_it;
    }
    return;
}

static
void
Rotate_Grid_Optimally( Grid_Context &GC,
                       ICP_Context &ICPC ){

    // Determine optimal rotations.
    //
    // This routine rotates the grid axes unit vectors by estimating the optimal rotation of corresponding points.
    // A SVD decomposition provides the rotation matrix that minimizes the difference between corresponding points.
    const auto Anchor_to_Rtn_cntr = (ICPC.rot_centre - GC.current_grid_anchor);
    const auto Rtn_cntr_to_Anchor = (GC.current_grid_anchor - ICPC.rot_centre);

    const auto N_rows = 3;
    const auto N_cols = ICPC.p_corr.size();
    Eigen::MatrixXf A(N_rows, N_cols);
    Eigen::MatrixXf B(N_rows, N_cols);

    auto o_it = std::begin(ICPC.cohort);
    auto c_it = std::begin(ICPC.p_corr);
    auto p_it = std::begin(ICPC.p_cell);
    size_t col = 0;
    while(c_it != std::end(ICPC.p_corr)){
        const auto O = o_it->first; // The original point location.
        const auto P = p_it->first; // The point projected into the unit cube.
        const auto C = c_it->first; // The corresponding point somewhere on the unit cube surface.

        const auto P_B = (O - ICPC.rot_centre); // O from the rotation centre; the actual point location.
        const auto P_A = P_B + (C - P); // O's corresponding point from the rotation centre; the desired point location.

        A(0, col) = P_A.x;
        A(1, col) = P_A.y;
        A(2, col) = P_A.z;

        B(0, col) = P_B.x;
        B(1, col) = P_B.y;
        B(2, col) = P_B.z;

        ++col;
        ++c_it;
        ++o_it;
        ++p_it;
    }
    auto AT = A.transpose();
    auto BAT = B * AT;

    //Eigen::JacobiSVD<Eigen::MatrixXf> SVD(BAT, Eigen::ComputeThinU | Eigen::ComputeThinV);
    Eigen::JacobiSVD<Eigen::MatrixXf> SVD(BAT, Eigen::ComputeFullU | Eigen::ComputeFullV );
    auto U = SVD.matrixU();
    auto V = SVD.matrixV();
    
    // Use the SVD result directly.
    auto M = U * V.transpose();

    // Attempt to restrict to rotations only.
    //Eigen::Matrix3f PI;
    //PI << 1.0 , 0.0 , 0.0,
    //      0.0 , 1.0 , 0.0,
    //      0.0 , 0.0 , ( U * V.transpose() ).determinant();
    //auto M = U * PI * V.transpose();

    // Restrict the solution to rotations only. (Refer to the 'Kabsch algorithm' for more info.)
    // NOTE: Probably requires Nx3 matrices rather than 3xN matrices...
    //Eigen::Matrix3f PI;
    //PI << 1.0 << 0.0 << 0.0
    //   << 0.0 << 1.0 << 0.0
    //   << 0.0 << 0.0 << Eigen::Determinant( V * U.transpose() );
    //auto M = V * PI * U.transpose();

    // Apply the transformation to the grid axis unit vectors.
    auto Apply_Rotation = [&](const vec3<double> &v) -> vec3<double> {
        Eigen::Vector3f e_vec3(v.x, v.y, v.z);
        auto new_v = M * e_vec3;
        return vec3<double>( new_v(0), new_v(1), new_v(2) );
    };

    const auto previous_grid_x = GC.current_grid_x;
    const auto previous_grid_y = GC.current_grid_y;
    const auto previous_grid_z = GC.current_grid_z;

    GC.current_grid_x = Apply_Rotation(GC.current_grid_x).unit();
    GC.current_grid_y = Apply_Rotation(GC.current_grid_y).unit();
    GC.current_grid_z = Apply_Rotation(GC.current_grid_z).unit();

    // Ensure the grid axes are orthonormal.
    GC.current_grid_z.GramSchmidt_orthogonalize(GC.current_grid_x, GC.current_grid_y);
    GC.current_grid_x = GC.current_grid_x.unit();
    GC.current_grid_y = GC.current_grid_y.unit();
    GC.current_grid_z = GC.current_grid_z.unit();

    //Determine how the anchor point moves.
    //
    // Since we permitted only rotations relative to some fixed centre, the translation from the grid anchor to
    // the fixed rotation centre remains constant (within the grid coordinate system). So rotating and reversing
    // the old anchor -> rotation centre transformation will transform rotation centre -> new anchor.
    const auto previous_grid_anchor = GC.current_grid_anchor;
    const auto Rtn_cntr_to_new_Anchor = Apply_Rotation(Rtn_cntr_to_Anchor).unit() * Rtn_cntr_to_Anchor.length();
    GC.current_grid_anchor = (ICPC.rot_centre + Rtn_cntr_to_new_Anchor);

    if(false){ // Debugging.
        std::cout << " Rotations: " << std::endl;
        std::cout << "    Rotating about point at " << ICPC.rot_centre << std::endl;
        std::cout << "    Dot product of new grid axes vectors: " 
          << GC.current_grid_x.Dot(GC.current_grid_y) << "  "
          << GC.current_grid_x.Dot(GC.current_grid_z) << "  "
          << GC.current_grid_y.Dot(GC.current_grid_z) << " # should be (0,0,0)." << std::endl;

        std::cout << "    Grid anchor was " << previous_grid_anchor << " and is now " << GC.current_grid_anchor;
        std::cout << "  translation was " << (GC.current_grid_anchor - previous_grid_anchor) << std::endl;
        std::cout << "    Grid axes moving from: " << std::endl;
        std::cout << "        " << previous_grid_x;
        std::cout << "  " << previous_grid_y;
        std::cout << "  " << previous_grid_z << std::endl;
        std::cout << "      to " << std::endl;
        std::cout << "        " << GC.current_grid_x;
        std::cout << "  " << GC.current_grid_y;
        std::cout << "  " << GC.current_grid_z << std::endl;
        std::cout << "      angle changes: " << std::endl;
        std::cout << "        " << GC.current_grid_x.angle(previous_grid_x)*180.0/M_PI << " deg.";
        std::cout << "  " << GC.current_grid_y.angle(previous_grid_y)*180.0/M_PI << " deg.";
        std::cout << "  " << GC.current_grid_z.angle(previous_grid_z)*180.0/M_PI << " deg." << std::endl;
        std::cout << " " << std::endl;
     }
     return;
}

static
double
Score_Fit( Grid_Context &GC,
           ICP_Context &ICPC, 
           bool verbose = false){

    // Evaluate the fit using the corresponding points.
    std::vector<double> dists;
    dists.reserve(ICPC.p_corr.size());
    auto c_it = std::begin(ICPC.p_corr);
    for(const auto &pp : ICPC.p_cell){
        const auto P = pp.first;

        const auto C = c_it->first;
        const auto dist = P.distance(C);
        dists.emplace_back(dist);

        ++c_it;
    }

    if(verbose){
        std::cout << " Score fit stats:    " << std::endl;
        std::cout << "    Min:    " << Stats::Min(dists)    << std::endl
                  << "    Mean:   " << Stats::Mean(dists)   << std::endl
                  << "    Median: " << Stats::Median(dists) << std::endl
                  << "    Max:    " << Stats::Max(dists)    << std::endl;
    }

    const auto score = Stats::Mean(dists); // Better scores should be less than worse scores.
    return score;
}

static
void
ICP_Fit_Grid( Drover &DICOM_data,
              std::mt19937 re, 
              long int icp_max_loops,
              Grid_Context &GC,
              ICP_Context &ICPC ){

    // Re-score the existing grid arrangment since the cohort has most likely changed.
    Project_Into_Proto_Cube(GC, ICPC);
    Find_Corresponding_Points(GC, ICPC);
    GC.score = Score_Fit(GC, ICPC);

    Grid_Context best_GC = GC;

    for(long int loop = 1; loop <= icp_max_loops; ++loop){
//        std::cout << "====================================== " << "Loop: " << loop << std::endl;

        // Nominate a random point to be the rotation centre.
        //
        // Note: This *might* be wasteful, but it will also help protect against picking an irrelevant point and being
        // stuck with it for the entire ICP procedure. TODO: try commenting out this code to always use the ransac point
        // as the rotation centre.
        std::uniform_int_distribution<long int> rd(0, ICPC.cohort.size());
        const auto N_select = rd(re);
        ICPC.rot_centre = std::next( std::begin(ICPC.cohort), N_select )->first;

        Project_Into_Proto_Cube(GC, ICPC);
        Translate_Grid_Optimally(GC, ICPC);

        // TODO: Does this invalidate the optimal translation we just found? If so, can anything be done?
        Project_Into_Proto_Cube(GC, ICPC);
        Find_Corresponding_Points(GC, ICPC);
        Rotate_Grid_Optimally(GC, ICPC);

        // Evaluate over the entire point cloud, retaining the global best.
        Project_Into_Proto_Cube(GC, ICPC);
        Find_Corresponding_Points(GC, ICPC);

        GC.score = Score_Fit(GC, ICPC);
        if(!std::isfinite(best_GC.score) || (GC.score < best_GC.score)){
            best_GC = GC;
        }else{                           // NOTE: Not sure about this one ... will it confine to local minima only?   TODO
            GC = best_GC;
        }

    } // ICP loop.

    //GC = best_GC; 
    return;
}

OperationDoc OpArgDocDetectGrid3D(void){
    OperationDoc out;
    out.name = "DetectGrid3D";

    out.desc = 
        "This routine fits a 3D grid to a point cloud using a Procrustes analysis with "
        " point-to-model correspondence estimated via an iterative closest point approach."
        " A RANSAC-powered loop is used to (1) randomly select a subset of the grid for coarse"
        " iterative closest point grid fitting, and then (2) use the coarse fit results as a"
        " guess for the whole point cloud in a refinement stage.";
    
    out.notes.emplace_back(
        "Traditional Procrustes analysis requires a priori point-to-point correspondence knowledge."
        " Because this operation fits a model (with infinite extent), point-to-point correspondence"
        " is not known and the model is effectively an infinite continuum of potential points."
        " To overcome this problem, correspondence is estimated by projecting each point in the point"
        " cloud onto every grid line and selecting the closest projected point."
        " The point cloud point and the project point are then treated as corresponding points."
        " Using this phony correspondence, the Procrustes problem is solved and the grid is reoriented."
        " This is performed iteratively. However **there is no guarantee the procedure will converge**"
        " and furthermore, even if it does converge, **there is no guarantee that the grid will be"
        " appropriately fit**. The best results will occur when the grid is already closely aligned"
        " with the point cloud (i.e., when the first guess is very close to a solution)."
        " If this cannot be guaranteed, it may be advantageous to have a nearly continuous point"
        " cloud to avoid gaps in which the iteration can get stuck in a local minimum."
        " For this reason, RANSAC is applied to continuously reboot the fitting procedure."
        " All but the best fit are discarded."
    );
    out.notes.emplace_back(
        "A two-stage RANSAC inner-loop iterative closest point fitting procedure is used."
        " Coarse grid fitting is first performed with a limited subset of the whole point cloud."
        " This is followed with a refinment stage in which the enire point cloud is fitted using an"
        " initial guess carried forward from the coarse fitting stage."
        " This guess is expected to be reasonably close to the true grid in cases where the coarse"
        " fitting procedure was not tainted by outliers, but is only derived from a small portion"
        " of the point cloud."
        " (Thus RANSAC is used to perform this coarse-fine iterative procedure multiple times to"
        " provide resilience to poor-quality coarse fits.)"
        " CoarseICPMaxLoops is the maximum number of iterative-closest point loop iterations"
        " performed during the coarse grid fitting stage (on a subset of the point cloud),"
        " and FineICPMaxLoops is the maximum number of iterative-closest point loop iterations"
        " performed during the refinement stage (using the whole point cloud)."
        " Note that, depending on the noise level and number of points considered (i.e.,"
        " whether the RANSACDist parameter is sufficiently small to avoid spatial wrapping of"
        " corresponding points into adjacent grid cells, but sufficiently large to enclose at"
        " least one whole grid cell), the coarse phase should converge within a few iterations."
        " However, on each loop a single point is selected as the"
        " grid's rotation centre. This means that a few extra iterations should always be used"
        " in case outliers are selected as rotation centres. Additionally, if the point cloud"
        " is dense or there are lots of outliers present, increase CoarseICPMaxLoops to ensure"
        " there is a reasonable chance of selecting legitimate rotation points. On the other"
        " hand, be aware that the coarse-fine iterative procedure is performed afresh for every"
        " RANSAC loop, and RANSAC loops are better able to ensure the point cloud is sampled"
        " ergodically. It might therefore be more productive to increase the RANSACMaxLoops"
        " parameter and reduce the number of CoarseICPMaxLoops."
        " FineICPMaxLoops should converge quickly if the coarse fitting stage was representative"
        " of the true grid. However, as in the coarse stage a rotation centre is nominated in"
        " each loop, so it will be a good idea to keep a sufficient number of loops to ensure"
        " a legitimate and appropriate non-outlier point is nominated during this stage."
        " Given the complicated interplay between parameters and stages, it is always best"
        " to tune using a representative sample of the point cloud you need to fit!"
    );

    out.args.emplace_back();
    out.args.back() = PCWhitelistOpArgDoc();
    out.args.back().name = "PointSelection";
    out.args.back().default_val = "last";


    out.args.emplace_back();
    out.args.back().name = "GridSeparation";
    out.args.back().desc = "The separation of the grid (in DICOM units; mm) being fit."
                           " This parameter describes how close adjacent grid lines are to one another."
                           " Separation is measured from one grid line centre to the nearest adjacent grid line centre.";
    out.args.back().default_val = "10.0";
    out.args.back().expected = true;
    out.args.back().examples = { "10.0", 
                                 "15.5",
                                 "25.0",
                                 "1.23E4" };

    out.args.emplace_back();
    out.args.back().name = "RANSACDist";
    out.args.back().desc = "Every iteration of RANSAC selects a single point from the point cloud. Only the"
                           " near-vicinity of points are retained for iterative-closest-point Procrustes solving."
                           " This parameter determines the maximum radial distance from the RANSAC point within which"
                           " point cloud points will be retained; all points further than this distance away"
                           " will be pruned for a given round of RANSAC. This is needed because corresponding"
                           " points begin to alias to incorrect cell faces when the ICP procedure begins with"
                           " a poor guess. Pruning points in a spherical neighbourhood with a diameter 2-4x the"
                           " GridSeparation (so a radius 1-2x GridSeparation) will help mitigate"
                           " aliasing even when the initial guess is poor. However, smaller windows may increase"
                           " susceptibility to noise/outliers, and RANSACDist should never be smaller than a"
                           " grid voxel. If RANSACDist is not provided, a default of"
                           " (1.5 * GridSeparation) is used.";
    out.args.back().default_val = "nan";
    out.args.back().expected = false;
    out.args.back().examples = { "7.0", 
                                 "10.0",
                                 "2.46E4" };

    out.args.emplace_back();
    out.args.back().name = "GridSampling";
    out.args.back().desc = "Specifies how the grid data has been sampled."
                           " Use value '1' if only grid cell corners (i.e., '0D' grid intersections) are sampled."
                           " Use value '2' if grid cell edges (i.e., 1D grid lines) are sampled."
                           " Use value '3' if grid cell faces (i.e., 2D planar faces) are sampled.";
    out.args.back().default_val = "1";
    out.args.back().expected = true;
    out.args.back().examples = { "1", 
                                 "2",
                                 "3" };

    out.args.emplace_back();
    out.args.back().name = "LineThickness";
    out.args.back().desc = "The thickness of grid lines (in DICOM units; mm)."
                           " If zero, lines are treated simply as lines."
                           " If non-zero, grid lines are treated as hollow cylinders with a diameter of this thickness.";
    out.args.back().default_val = "0.0";
    out.args.back().expected = true;
    out.args.back().examples = { "1.0", 
                                 "1.5",
                                 "10.0",
                                 "1.23E4" };

    out.args.emplace_back();
    out.args.back().name = "RandomSeed";
    out.args.back().desc = "A whole number seed value to use for random number generation.";
    out.args.back().default_val = "1317";
    out.args.back().expected = true;
    out.args.back().examples = { "1", 
                                 "2",
                                 "1113523431" };

    out.args.emplace_back();
    out.args.back().name = "RANSACMaxLoops";
    out.args.back().desc = "The maximum number of iterations of RANSAC."
                           " (See operation notes for further details.)";
    out.args.back().default_val = "100";
    out.args.back().expected = true;
    out.args.back().examples = { "100", 
                                 "2000",
                                 "1E4" };

    out.args.emplace_back();
    out.args.back().name = "CoarseICPMaxLoops";
    out.args.back().desc = "Coarse grid fitting is performed with a limited subset of the whole point cloud."
                           " This is followed with a refinment stage in which the enire point is fitted using an"
                           " initial guess from the coarse fitting stage."
                           " CoarseICPMaxLoops is the maximum number of iterative-closest point loop iterations"
                           " performed during the coarse grid fitting stage."
                           " (See operation notes for further details.)";
    out.args.back().default_val = "10";
    out.args.back().expected = true;
    out.args.back().examples = { "10", 
                                 "100",
                                 "1E4" };

    out.args.emplace_back();
    out.args.back().name = "FineICPMaxLoops";
    out.args.back().desc = "Coarse grid fitting is performed with a limited subset of the whole point cloud."
                           " This is followed with a refinment stage in which the enire point is fitted using an"
                           " initial guess from the coarse fitting stage."
                           " FineICPMaxLoops is the maximum number of iterative-closest point loop iterations"
                           " performed during the refinement stage."
                           " (See operation notes for further details.)";
    out.args.back().default_val = "20";
    out.args.back().expected = true;
    out.args.back().examples = { "10", 
                                 "50",
                                 "100" };

    return out;
}

Drover DetectGrid3D(Drover DICOM_data, OperationArgPkg OptArgs, std::map<std::string,std::string> /*InvocationMetadata*/, std::string /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto PointSelectionStr = OptArgs.getValueStr("PointSelection").value();

    const auto GridSeparation = std::stod( OptArgs.getValueStr("GridSeparation").value() );
    const auto RANSACDist = std::stod( OptArgs.getValueStr("RANSACDist").value_or(std::to_string(GridSeparation * 1.5)) );
    const auto GridSampling = std::stol( OptArgs.getValueStr("GridSampling").value() ); 

    const auto LineThickness = std::stod( OptArgs.getValueStr("LineThickness").value() );
    const auto RandomSeed = std::stol( OptArgs.getValueStr("RandomSeed").value() );
    const auto RANSACMaxLoops = std::stol( OptArgs.getValueStr("RANSACMaxLoops").value() );

    const auto CoarseICPMaxLoops = std::stol( OptArgs.getValueStr("CoarseICPMaxLoops").value() ); 
    const auto FineICPMaxLoops = std::stol( OptArgs.getValueStr("FineICPMaxLoops").value() ); 

    //-----------------------------------------------------------------------------------------------------------------

    if(!std::isfinite(RANSACDist)){
        throw std::invalid_argument("RANSAC distance is not valid. Cannot continue.");
    }

    if(!std::isfinite(GridSeparation) || (GridSeparation <= 0.0)){
        throw std::invalid_argument("Grid separation is not valid. Cannot continue.");
    }
    if(!std::isfinite(LineThickness) || (LineThickness <= 0.0)){
        throw std::invalid_argument("Line thickness is not valid. Cannot continue.");
    }
    if(GridSeparation < LineThickness){
        throw std::invalid_argument("Line thickness is impossible with given grid spacing. Refusing to continue.");
    }

    std::mt19937 re( RandomSeed );


    auto PCs_all = All_PCs( DICOM_data );
    auto PCs = Whitelist( PCs_all, PointSelectionStr );
    for(auto & pcp_it : PCs){

        Grid_Context best_GC; // The current best estimate of the grid position.

        Grid_Context GC; // A working estimate of the grid position.
        GC.grid_sep = GridSeparation;
        GC.grid_sampling = GridSampling;

        ICP_Context ICPC; // Working ICP context.

        ICP_Context whole_ICPC; // Whole (i.e., entire point cloud) context.
        whole_ICPC.cohort = (*pcp_it)->points;
        whole_ICPC.p_cell = whole_ICPC.cohort; // Prime the container with dummy info.
        whole_ICPC.p_corr = whole_ICPC.cohort; // Prime the container with dummy info.
        //whole_ICPC.p_corr.resize(whole_ICPC.cohort.size());
        //whole_ICPC.rot_centre = whole_ICPC.ransac_centre;

        // Some RANSAC failures are expected due to outliers and noisy data, so a fairly number of failures will be
        // tolerated. However, RANSAC must eventually terminate if too many errors are encountered. It is tricky to
        // identify a reasonable default threshold. Here we tailor to the case of extremely noisy data and try allow for
        // *most* points to be randomly sampled. This might result in an excessive amount of tries for large data sets,
        // but it will also minimize the likelihood that valid cases will erroneously be rejected.
        //
        // The routine below can be called only a certain number of times before throwing.
        long int RANSACFails = 0;
        const long int PermittedRANSACFails = std::max(100L, static_cast<long int>((*pcp_it)->points.size() * 2));
        auto Handle_RANSAC_Failure = [&](void) -> void {
            ++RANSACFails;
            if(RANSACFails > PermittedRANSACFails){
                std::stringstream ss;
                ss << "Encountered too many RANSAC failures."
                   << " Confirm GridSeparation and RANSACDist are valid and appropriate for the point cloud density.";
                throw std::runtime_error(ss.str());
            }
            return;
        };

        // Perform a RANSAC analysis by only analyzing the vicinity of a randomly selected point.
        long int ransac_loop = 0;
        std::mutex saver_printer;
        while(ransac_loop < RANSACMaxLoops){
            {
                std::lock_guard<std::mutex> lock(saver_printer);
                FUNCINFO("Completed RANSAC loop " << ransac_loop << " of " << RANSACMaxLoops
                      << " --> " << static_cast<int>(1000.0*(ransac_loop)/RANSACMaxLoops)/10.0 << "\% done");
            }
            
            // Randomly select a point from the cloud.
            std::uniform_int_distribution<long int> rd(0, (*pcp_it)->points.size());
            const auto N = rd(re);
            ICPC.ransac_centre = std::next( std::begin((*pcp_it)->points), N )->first;

            // Retain only the points within a small distance of the RANSAC centre.
            using pcp_t = decltype((*pcp_it)->points.front());
            ICPC.cohort = (*pcp_it)->points;
            ICPC.cohort.erase(
                std::remove_if(std::begin(ICPC.cohort), 
                               std::end(ICPC.cohort),
                               [&](const pcp_t &pcp) -> bool {
                                   return (pcp.first.distance(ICPC.ransac_centre) > RANSACDist);
                               }),
                std::end(ICPC.cohort) );

            if(ICPC.cohort.size() < 3){
                // If there are too few points to meaningfully continue, then the only thing we can assume is that the
                // selected point is in a region with a low density of points. So re-do the loop. However, if multiple
                // failures occur then we can probably conclude that the grid parameters are inappropriate. For example,
                // if the GridSeparation is too small then all points will appear to be in regions of low density.
                FUNCWARN("Too few adjacent points (" << ICPC.cohort.size() << "), rebooting RANSAC loop.");
                Handle_RANSAC_Failure(); // Will throw if too many failures encountered.
                continue;
            }

            // Allocate storage for ICP loops.
            ICPC.p_cell = ICPC.cohort;
            ICPC.p_corr = ICPC.cohort;
            //ICPC.p_corr.resize(ICPC.cohort.size());
            //ICPC.rot_centre = ICPC.ransac_centre;

            // Perform ICP on the sub-set cohort.
            try{
                ICP_Fit_Grid(DICOM_data, re, CoarseICPMaxLoops, GC, ICPC);
            }catch(const std::exception &e){
                FUNCWARN("Error encountered during coarse ICP (" << e.what() << "), rebooting RANSAC loop.");
                Handle_RANSAC_Failure(); // Will throw if too many failures encountered.
                continue;
            }

            // Invalidate the coarse fit score since it is not applicable to the whole point cloud.
            GC.score = std::numeric_limits<double>::quiet_NaN();

            // Using the subset cohort fit, perform an ICP using the whole point cloud.
            //Grid_Context whole_GC = GC; // Needed?
            //whole_GC.grid_sep = GridSeparation;
            whole_ICPC.ransac_centre = ICPC.ransac_centre;

            try{
                ICP_Fit_Grid(DICOM_data, re, FineICPMaxLoops, GC, whole_ICPC);
            }catch(const std::exception &e){
                FUNCWARN("Error encountered during fine ICP (" << e.what() << "), rebooting RANSAC loop.");
                Handle_RANSAC_Failure(); // Will throw if too many failures encountered.
                continue;
            }

            // Evaluate over the entire point cloud, retaining the global best.
            GC.score = Score_Fit(GC, whole_ICPC);
            if(!std::isfinite(best_GC.score) || (GC.score < best_GC.score)){
                best_GC = GC;
            }

            ++ransac_loop;
        } // RANSAC loop.

        // Do something with the results.
        if(true){
            FUNCINFO("Grid estimate found");
            Project_Into_Proto_Cube(best_GC, whole_ICPC);
            Find_Corresponding_Points(best_GC, whole_ICPC);

            const bool verbose = true;
            const auto best_score = Score_Fit(best_GC, whole_ICPC, verbose);
            FUNCINFO("Best score: " << best_score);

            Write_XYZ("/tmp/original_points.xyz", (*pcp_it)->points);
            Write_PLY("/tmp/original_points.ply", (*pcp_it)->points);

            // Write the project points to a file for inspection.
            Write_XYZ("/tmp/cube_proj_points.xyz", whole_ICPC.p_cell);
            Write_PLY("/tmp/cube_proj_points.ply", whole_ICPC.p_cell);

            // Write the correspondence points to a file for inspection.
            Write_XYZ("/tmp/cube_corr_points.xyz", whole_ICPC.p_corr);
            Write_PLY("/tmp/cube_corr_points.ply", whole_ICPC.p_corr);

            // Write the proto cube edges to a file for inspection.
            Write_Cube_OBJ("/tmp/proto_cube.obj",
                           best_GC.current_grid_anchor,
                           best_GC.current_grid_x * best_GC.grid_sep,
                           best_GC.current_grid_y * best_GC.grid_sep,
                           best_GC.current_grid_z * best_GC.grid_sep );

            // Write the grid for inspection.
            Insert_Grid_Contours(DICOM_data,
                           "best_grid",
                           whole_ICPC.cohort,
                           best_GC.current_grid_anchor,
                           best_GC.current_grid_x * best_GC.grid_sep,
                           best_GC.current_grid_y * best_GC.grid_sep,
                           best_GC.current_grid_z * best_GC.grid_sep );

            // Evaluate the fit using the corresponding points.
            {
                std::vector<double> dists;
                std::vector<double> dists_x;
                std::vector<double> dists_y;
                std::vector<double> dists_z;
                dists.reserve(ICPC.p_corr.size());
                dists_x.reserve(ICPC.p_corr.size());
                dists_y.reserve(ICPC.p_corr.size());
                dists_z.reserve(ICPC.p_corr.size());
                const double proj_eps = 1.0E-4; // The amount of numerical uncertainty in the planar projection.

                auto c_it = std::begin(ICPC.p_corr);
                for(const auto &pp : ICPC.p_cell){
                    const auto P = pp.first;
                    const auto C = c_it->first;
                    const auto R = (C - P);

                    const auto dist = R.length();
                    const auto dist_x = R.Dot(best_GC.current_grid_x);
                    const auto dist_y = R.Dot(best_GC.current_grid_y);
                    const auto dist_z = R.Dot(best_GC.current_grid_z);
                    dists.emplace_back(dist);

                    // Only consider the two largest projections since the third will be close to zero due to the
                    // projection.
                    if(false){
                    }else if( (std::abs(dist_y) > std::abs(dist_x))
                          &&  (std::abs(dist_z) > std::abs(dist_x)) ){
                        dists_y.emplace_back(dist_y);
                        dists_z.emplace_back(dist_z);
                    }else if( (std::abs(dist_x) > std::abs(dist_y))
                          &&  (std::abs(dist_z) > std::abs(dist_y)) ){
                        dists_x.emplace_back(dist_x);
                        dists_z.emplace_back(dist_z);
                    }else if( (std::abs(dist_x) > std::abs(dist_z))
                          &&  (std::abs(dist_y) > std::abs(dist_z)) ){
                        dists_x.emplace_back(dist_x);
                        dists_y.emplace_back(dist_y);
                    }

                    ++c_it;
                }

                const long int N_bins = 100;
                const bool explicitbins = true;
                const auto hist_dists  = Bag_of_numbers_to_N_equal_bin_samples_1D_histogram(dists, N_bins, explicitbins);
                const auto hist_dist_x = Bag_of_numbers_to_N_equal_bin_samples_1D_histogram(dists_x, N_bins, explicitbins);
                const auto hist_dist_y = Bag_of_numbers_to_N_equal_bin_samples_1D_histogram(dists_y, N_bins, explicitbins);
                const auto hist_dist_z = Bag_of_numbers_to_N_equal_bin_samples_1D_histogram(dists_z, N_bins, explicitbins);

                if( !hist_dists.Write_To_File("/tmp/hist_distance.data")
                ||  !hist_dist_x.Write_To_File("/tmp/hist_distance_x.data")
                ||  !hist_dist_y.Write_To_File("/tmp/hist_distance_y.data")
                ||  !hist_dist_z.Write_To_File("/tmp/hist_distance_z.data") ){
                    throw std::runtime_error("Unable to write histograms to file. Refusing to continue.");
                }
            }
        }

    } // Point_Cloud loop.

    return DICOM_data;
}
