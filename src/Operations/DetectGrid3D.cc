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
    double grid_sep = std::numeric_limits<double>::quiet_NaN();

    vec3<double> current_grid_anchor = vec3<double>(0.0, 0.0, 0.0);

    vec3<double> current_grid_x = vec3<double>(1.0, 0.0, 0.0);
    vec3<double> current_grid_y = vec3<double>(0.0, 1.0, 0.0);
    vec3<double> current_grid_z = vec3<double>(0.0, 0.0, 1.0);

    double score = std::numeric_limits<double>::quiet_NaN();
};

// Used to cache working state while fitting a 3D grid.
struct ICP_Context {

    vec3<double> ransac_centre = vec3<double>(0.0, 0.0, 0.0);
    vec3<double> rot_centre    = vec3<double>(0.0, 0.0, 0.0);

    using pcp_c_t = decltype(Point_Cloud().points); // Point_Cloud point container type.

    // Point cloud points participating in a single RANSAC phase.
    //
    // This list is regenerated for each round of RANSAC. Only some point cloud points within a fixed distance from
    // some randomly-selected point will be retained. The points are not altered, just copied for ease-of-use.
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
    for(long int i = -1; i <= N_lines_1; ++i){
        const auto l_corner = v + (edge1 * (i * 1.0));
        planes.emplace_back(edge1, l_corner);
    }
    for(long int i = -1; i <= N_lines_2; ++i){
        const auto l_corner = v + (edge2 * (i * 1.0));
        planes.emplace_back(edge2, l_corner);
    }
    for(long int i = -1; i <= N_lines_3; ++i){
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
Project_Into_Unit_Cube( Grid_Context &GC,
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

    // This routine takes every proto cube projected point and projects it onto the faces of the proto cube.
    // Only the projection on the nearest face is kept.
    //
    // Note: There is a faster way to do this using the same approach as the optimal translation routine.
    // This way is easy to debug and reason about.

    const vec3<double> NaN_vec3( std::numeric_limits<double>::quiet_NaN(),
                                 std::numeric_limits<double>::quiet_NaN(),
                                 std::numeric_limits<double>::quiet_NaN() );

    // Creates plane for all faces.
    std::vector<plane<double>> planes;

    planes.emplace_back( GC.current_grid_x, GC.current_grid_anchor );
    planes.emplace_back( GC.current_grid_y, GC.current_grid_anchor );
    planes.emplace_back( GC.current_grid_z, GC.current_grid_anchor );

    planes.emplace_back( GC.current_grid_x, GC.current_grid_anchor + GC.current_grid_x * GC.grid_sep );
    planes.emplace_back( GC.current_grid_y, GC.current_grid_anchor + GC.current_grid_y * GC.grid_sep );
    planes.emplace_back( GC.current_grid_z, GC.current_grid_anchor + GC.current_grid_z * GC.grid_sep );

    auto c_it = std::begin(ICPC.p_corr);
    for(const auto &pp : ICPC.p_cell){
        const auto P = pp.first;

        double closest_dist = std::numeric_limits<double>::infinity();
        vec3<double> closest_proj = NaN_vec3;
        for(const auto &pl : planes){
            const auto dist = std::abs(pl.Get_Signed_Distance_To_Point(P));
            if(dist < closest_dist){
                closest_dist = dist;
                closest_proj = pl.Project_Onto_Plane_Orthogonally(P);
            }
        }
        if(!closest_proj.isfinite()){
            throw std::logic_error("Projected point is not finite. Cannot continue");
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
    const auto N_cols = ICPC.cohort.size();
    Eigen::MatrixXf A(N_rows, N_cols);
    Eigen::MatrixXf B(N_rows, N_cols);

    auto o_it = std::begin(ICPC.cohort);
    auto c_it = std::begin(ICPC.p_corr);
    size_t col = 0;
    for(const auto &pp : ICPC.p_cell){
        const auto O = o_it->first; // The original point location.
        const auto P = pp.first; // The point projected into the unit cube.
        const auto C = c_it->first; // The corresponding point somewhere on the unit cube surface.

        const auto P_B = (O - ICPC.rot_centre); // O from the rotation centre; the actual point location.
        const auto P_A = P_B + (C - P); // O's corresponding point from the rotation centre; the desired point location.

        A(0, col) = P_A.x;
        A(1, col) = P_A.y;
        A(2, col) = P_A.z;

        B(0, col) = P_B.x;
        B(1, col) = P_B.y;
        B(2, col) = P_B.z;

        ++c_it;
        ++o_it;
        ++col;
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

    //Determine how the anchor point moves.
    //
    // Since we permitted only rotations relative to some fixed centre, the translation from the grid anchor to
    // the fixed rotation centre remains constant (within the grid coordinate system). So rotating and reversing
    // the old anchor -> rotation centre transformation will transform rotation centre -> new anchor.
    const auto previous_grid_anchor = GC.current_grid_anchor;
    const auto Rtn_cntr_to_new_Anchor = Apply_Rotation(Rtn_cntr_to_Anchor).unit() * Rtn_cntr_to_Anchor.length();
    GC.current_grid_anchor = (ICPC.rot_centre + Rtn_cntr_to_new_Anchor);

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
    return;
}


OperationDoc OpArgDocDetectGrid3D(void){
    OperationDoc out;
    out.name = "DetectGrid3D";

    out.desc = 
        "This routine fits a 3D grid to a point cloud using a Procrustes analysis with "
        " point-to-model correspondence estimated via an iterative closest point approach.";
    
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
        " If this cannot be guaranteed, it might be worthwhile to perform a bootstrap (or RANSAC)"
        " and discard all but the best fit."
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

    return out;
}

Drover DetectGrid3D(Drover DICOM_data, OperationArgPkg OptArgs, std::map<std::string,std::string> /*InvocationMetadata*/, std::string /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto PointSelectionStr = OptArgs.getValueStr("PointSelection").value();

    const auto GridSeparation = std::stod( OptArgs.getValueStr("GridSeparation").value() );
    const auto LineThickness = std::stod( OptArgs.getValueStr("LineThickness").value() );

    long int random_seed = 123456;
    const size_t ransac_max = 100;
    const double ransac_dist = GridSeparation * 1.5;
    //-----------------------------------------------------------------------------------------------------------------

    if(!std::isfinite(GridSeparation) || (GridSeparation <= 0.0)){
        throw std::invalid_argument("Grid separation is not valid. Cannot continue.");
    }
    if(!std::isfinite(LineThickness) || (LineThickness <= 0.0)){
        throw std::invalid_argument("Line thickness is not valid. Cannot continue.");
    }
    if(GridSeparation < LineThickness){
        throw std::invalid_argument("Line thickness is impossible with given grid spacing. Refusing to continue.");
    }

    std::mt19937 re( random_seed );


    auto PCs_all = All_PCs( DICOM_data );
    auto PCs = Whitelist( PCs_all, PointSelectionStr );
    for(auto & pcp_it : PCs){

        Write_XYZ("/tmp/original_points.xyz", (*pcp_it)->points);

        Grid_Context GC;
        GC.grid_sep = GridSeparation;

        ICP_Context ICPC;

        // Perform a RANSAC analysis by only analyzing the vicinity of a randomly selected point.
        for(size_t ransac_loop = 0; ransac_loop < ransac_max; ++ransac_loop){

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
                                   return (pcp.first.distance(ICPC.ransac_centre) > ransac_dist);
                               }),
                std::end(ICPC.cohort) );

            // Allocate storage for ICP loops.
            ICPC.p_cell = ICPC.cohort;
            ICPC.p_corr = ICPC.cohort;
            ICPC.rot_centre = ICPC.ransac_centre;

            // ICP loop.
            const size_t loop_max = 10;
            for(size_t loop = 1; loop <= loop_max; ++loop){
                std::cout << "====================================== " << "Loop: " << loop << std::endl;

                // Nominate a random point to be the rotation centre.
                /*
                std::uniform_int_distribution<long int> rd(0, ICPC.cohort.size());
                const auto N_c = rd(re);
                const auto ICPC.rot_centre = std::next( std::begin((ICPC.cohort), N_c )->first;
                */

                Project_Into_Unit_Cube(GC, ICPC);

                std::cout << " Optimal translation: " << std::endl
                          << "    " << "Begin GC.current_grid_anchor = " << GC.current_grid_anchor
                          << std::endl;

                Translate_Grid_Optimally(GC, ICPC);

                std::cout << "    " << "After GC.current_grid_anchor = " << GC.current_grid_anchor
                          << std::endl;

                // TODO: Does this invalidate the optimal translation we just found? If so, can anything be done?
                Project_Into_Unit_Cube(GC, ICPC);

                Find_Corresponding_Points(GC, ICPC);

                if(loop == loop_max){
                    // Write the project points to a file for inspection.
                    Write_XYZ("/tmp/cube_projected_points.xyz", ICPC.p_cell);

                    // Write the unit cube edges to a file for inspection.
                    Write_Cube_OBJ("/tmp/cube.obj",
                                   GC.current_grid_anchor,
                                   GC.current_grid_x * GC.grid_sep,
                                   GC.current_grid_y * GC.grid_sep,
                                   GC.current_grid_z * GC.grid_sep );

                    // Write the correspondence points to a file for inspection.
                    Write_XYZ("/tmp/cube_corresp_points.xyz", ICPC.p_corr);

                    // Write the grid for inspection.
                    Insert_Grid_Contours(DICOM_data,
                                   "grid_final",
                                   ICPC.cohort,
                                   GC.current_grid_anchor,
                                   GC.current_grid_x * GC.grid_sep,
                                   GC.current_grid_y * GC.grid_sep,
                                   GC.current_grid_z * GC.grid_sep );

                }

                Insert_Grid_Contours(DICOM_data,
                               "grid_"_s + std::to_string(loop),
                               ICPC.cohort,
                               GC.current_grid_anchor,
                               GC.current_grid_x * GC.grid_sep,
                               GC.current_grid_y * GC.grid_sep,
                               GC.current_grid_z * GC.grid_sep );

                Rotate_Grid_Optimally(GC, ICPC);

                Project_Into_Unit_Cube(GC, ICPC);

                // Evaluate over the entire point cloud, retaining the global best.
                if(false){
                    std::vector<double> dists;
                    auto c_it = std::begin(ICPC.p_corr);
                    for(const auto &pp : ICPC.p_cell){
                        const auto P = pp.first;
                        const auto C = c_it->first;

                        const auto dist = P.distance(C);
                        dists.emplace_back(dist);

                        ++c_it;
                    }

                    std::cout << " Stats:    " << std::endl;
                    std::cout << "    Min:    " << Stats::Min(dists)    << std::endl
                              << "    Mean:   " << Stats::Mean(dists)   << std::endl
                              << "    Median: " << Stats::Median(dists) << std::endl
                              << "    Max:    " << Stats::Max(dists)    << std::endl;

                    const auto proj_med = Stats::Median(dists);
                    GC.score = proj_med;
                }

            } // ICP loop.

        } // RANSAC loop.

    } // Point_Cloud loop.

    return DICOM_data;
}
