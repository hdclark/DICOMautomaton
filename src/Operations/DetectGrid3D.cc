//DetectGrid3D.cc - A part of DICOMautomaton 2019. Written by hal clark.

#include <experimental/any>
#include <experimental/optional>
#include <functional>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>    

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

    const vec3<double> zero_vec3( 0.0, 0.0, 0.0 );
    const vec3<double> NaN_vec3( std::numeric_limits<double>::quiet_NaN(),
                                 std::numeric_limits<double>::quiet_NaN(),
                                 std::numeric_limits<double>::quiet_NaN() );



    //////////////////////////////////////////////////////////////////////////////
    const auto Write_XYZ = [](const std::string &fname,
                              std::vector< std::pair< vec3<double>, long int > > points) -> void {
        std::ofstream OF(fname);
        for(const auto &pp : points){
            const auto v = pp.first;
            OF << v.x << " " << v.y << " " << v.z << std::endl;
        }
        OF.close();
        return;
    };

    const auto Write_Cube_OBJ = [](const std::string &fname,
                                   const vec3<double> &corner,
                                   const vec3<double> &edge1,
                                   const vec3<double> &edge2,
                                   const vec3<double> &edge3 ) -> void {
        
        // This routine takes a corner vertex and three edge vectors stemming from the corner and draws a cube.
        // The edges need not be orthogonal. They can also have different lengths.
        const auto A = corner;
        const auto B = corner + edge1;
        const auto C = corner + edge1 + edge3;
        const auto D = corner + edge3;

        const auto E = corner + edge2;
        const auto F = corner + edge1 + edge2;
        const auto G = corner + edge1 + edge2 + edge3;
        const auto H = corner + edge2 + edge3;

        std::ofstream OF(fname);
        OF << "# Wavefront OBJ file." << std::endl;

        // Vertices.
        OF << "v " << A.x << " " << A.y << " " << A.z << std::endl 
           << "v " << B.x << " " << B.y << " " << B.z << std::endl 
           << "v " << C.x << " " << C.y << " " << C.z << std::endl 
           << "v " << D.x << " " << D.y << " " << D.z << std::endl 

           << "v " << E.x << " " << E.y << " " << E.z << std::endl 
           << "v " << F.x << " " << F.y << " " << F.z << std::endl 
           << "v " << G.x << " " << G.y << " " << G.z << std::endl 
           << "v " << H.x << " " << H.y << " " << H.z << std::endl; 

        // Faces (n.b. one-indexed, not zero-indexed).
        OF << "f 1 2 5" << std::endl
           << "f 2 6 5" << std::endl

           << "f 2 3 6" << std::endl
           << "f 3 7 6" << std::endl

           << "f 3 4 7" << std::endl
           << "f 4 8 7" << std::endl

           << "f 4 1 8" << std::endl
           << "f 1 5 8" << std::endl

           << "f 5 6 7" << std::endl
           << "f 7 8 5" << std::endl

           << "f 2 1 3" << std::endl
           << "f 1 4 3" << std::endl;

        // Edges (n.b. one-indexed, not zero-indexed).
        OF << "l 1 2" << std::endl
           << "l 2 3" << std::endl
           << "l 3 4" << std::endl
           << "l 4 1" << std::endl

           << "l 1 5" << std::endl
           << "l 2 6" << std::endl
           << "l 3 7" << std::endl
           << "l 4 8" << std::endl

           << "l 5 6" << std::endl
           << "l 6 7" << std::endl
           << "l 7 8" << std::endl
           << "l 8 5" << std::endl;

        OF.close();
        return;
    };

    const auto Write_Grid_OBJ = [&DICOM_data](const std::string &fname,
                                   std::vector< std::pair< vec3<double>, long int > > points,
                                   const vec3<double> &corner,
                                   const vec3<double> &edge1,
                                   const vec3<double> &edge2,
                                   const vec3<double> &edge3 ) -> void {
        
        // This routine takes a grid intersection (i.e., a corner of a single grid voxel) and three edge vectors that
        // describe where the adjacent cells are and draws a 3D grid that tiles the region occupied by the given points.
        //

        // Find the grid lines that enclose the given points.
        Stats::Running_MinMax<double> mm_x;
        Stats::Running_MinMax<double> mm_y;
        Stats::Running_MinMax<double> mm_z;
        for(const auto &pp : points){
            const auto P = pp.first;

            //mm_x.Digest(P.Dot(edge1));
            //mm_y.Digest(P.Dot(edge2));
            //mm_z.Digest(P.Dot(edge3));

            mm_x.Digest(P.x);
            mm_y.Digest(P.y);
            mm_z.Digest(P.z);
        }

        const auto bounding_corner = vec3<double>( mm_x.Current_Min(),
                                                   mm_y.Current_Min(),
                                                   mm_z.Current_Min() );
        /*
        auto v = corner;

        const auto dx = (v - bounding_corner).Dot(edge1);
        const auto dy = (v - bounding_corner).Dot(edge2);
        const auto dz = (v - bounding_corner).Dot(edge3);

        v +=  edge1 * std::round(dx / edge1.length())
            + edge2 * std::round(dy / edge2.length())
            + edge3 * std::round(dz / edge3.length());
        */
        auto v = bounding_corner;



/*
        auto v = corner;
        while(true){
            const auto proj_v = v.Dot(edge1);
            const auto is_below = (proj_v < mm_x.Current_Min());
            const auto next_is_below = ((proj_v + edge1.length()) < mm_x.Current_Min());
            if(is_below && !next_is_below){
                break;
            }else if(is_below){
                v += edge1;
            }else if(!is_below){
                v -= edge1;
            }
        }
        while(true){
            const auto proj_v = v.Dot(edge2);
            const auto is_below = (proj_v < mm_y.Current_Min());
            const auto next_is_below = ((proj_v + edge2.length()) < mm_y.Current_Min());
            if(is_below && !next_is_below){
                break;
            }else if(is_below){
                v += edge2;
            }else if(!is_below){
                v -= edge2;
            }
        }
        while(true){
            const auto proj_v = v.Dot(edge3);
            const auto is_below = (proj_v < mm_z.Current_Min());
            const auto next_is_below = ((proj_v + edge3.length()) < mm_z.Current_Min());
            if(is_below && !next_is_below){
                break;
            }else if(is_below){
                v += edge3;
            }else if(!is_below){
                v -= edge3;
            }
        }
*/

        const auto N_lines_1 = static_cast<long int>( (mm_x.Current_Max() - mm_x.Current_Min() + edge1.length()) / edge1.length() );
        const auto N_lines_2 = static_cast<long int>( (mm_y.Current_Max() - mm_y.Current_Min() + edge2.length()) / edge2.length() );
        const auto N_lines_3 = static_cast<long int>( (mm_z.Current_Max() - mm_z.Current_Min() + edge3.length()) / edge3.length() );



        std::vector<plane<double>> planes;
        for(long int i = 0; i < N_lines_1; ++i){
            const auto l_corner = v + (edge1 * (i * 1.0));
            planes.emplace_back(edge1, l_corner);
        }
        for(long int i = 0; i < N_lines_2; ++i){
            const auto l_corner = v + (edge2 * (i * 1.0));
            planes.emplace_back(edge2, l_corner);
        }
        for(long int i = 0; i < N_lines_3; ++i){
            const auto l_corner = v + (edge3 * (i * 1.0));
            planes.emplace_back(edge3, l_corner);
        }

// Save the planes as contours on an image.
{
    const std::string ImageSelectionStr = "last";
    const std::string ROILabel = "grid";
    const std::string NormalizedROILabel = ROILabel;
    const auto contour_thickness = 0.02; // in DICOM units (i.e., mm).

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

/*
        std::ofstream OF(fname);
        OF << "# Wavefront OBJ file." << std::endl;

        for(long int i = 0; i < N_lines_1; ++i){
            for(long int j = 0; j < N_lines_2; ++j){
                for(long int k = 0; k < N_lines_3; ++k){

                       const auto l_corner = v + (edge1 * (i * 1.0))
                                               + (edge2 * (j * 1.0))
                                               + (edge3 * (k * 1.0));
                        const auto A = l_corner;
                        const auto B = l_corner + edge1;
                        const auto C = l_corner + edge1 + edge3;
                        const auto D = l_corner + edge3;

                        const auto E = l_corner + edge2;
                        const auto F = l_corner + edge1 + edge2;
                        const auto G = l_corner + edge1 + edge2 + edge3;
                        const auto H = l_corner + edge2 + edge3;

                        // All vertices and faces.
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

                        //// Minimal number of verts and faces to visualize the grid.
                        //// Vertices.
                        //OF << "v " << A.x << " " << A.y << " " << A.z << "\n" 
                        //   << "v " << B.x << " " << B.y << " " << B.z << "\n" 
                        //   << "v " << E.x << " " << E.y << " " << E.z << "\n" 
                        //   << "v " << F.x << " " << F.y << " " << F.z << "\n";
                        //
                        //// Faces (n.b. one-indexed, not zero-indexed).
                        //OF << "f -4 -3 -2 -1" << "\n";
                        //   //<< "f -3 -1 -2" << "\n";
                }
            }
        }

        OF.close();
*/        
        return;
    };
    //////////////////////////////////////////////////////////////////////////////


    auto PCs_all = All_PCs( DICOM_data );
    auto PCs = Whitelist( PCs_all, PointSelectionStr );
    for(auto & pcp_it : PCs){

        const vec3<double> x_unit(1.0, 0.0, 0.0);
        const vec3<double> y_unit(0.0, 1.0, 0.0);
        const vec3<double> z_unit(0.0, 0.0, 1.0);

        vec3<double> current_grid_x = x_unit;
        vec3<double> current_grid_y = y_unit;
        vec3<double> current_grid_z = z_unit;
        vec3<double> current_grid_anchor = zero_vec3;

        Write_XYZ("/tmp/original_points.xyz", (*pcp_it)->points);

// Loop point.
size_t loop_max = 50;
for(size_t loop = 0; loop < loop_max; ++loop){

        // Using the current grid axes directions and anchor point, project all points into the 'unit' cube.
        auto p_unit = (*pcp_it)->points; // Holds the projection of each point cloud point into the grid-defined unit cube.

        const auto project_into_unit_cube = [&](void) -> void {
            auto p_unit_it = std::begin(p_unit);
            for(const auto &pp : (*pcp_it)->points){
                const auto P = pp.first;

                // Vector rel. to grid anchor.
                const auto R = (P - current_grid_anchor);

                // Vector within the unit cube, described in the grid axes basis.
                auto C_x = std::fmod( R.Dot(current_grid_x), GridSeparation );
                auto C_y = std::fmod( R.Dot(current_grid_y), GridSeparation );
                auto C_z = std::fmod( R.Dot(current_grid_z), GridSeparation );
                if(C_x < 0.0) C_x += GridSeparation; // Ensure the result is within the cube (fmod can be negative).
                if(C_y < 0.0) C_y += GridSeparation;
                if(C_z < 0.0) C_z += GridSeparation;

                const auto C = current_grid_anchor
                             + current_grid_x * C_x
                             + current_grid_y * C_y
                             + current_grid_z * C_z;
                //const auto C = vec3<double>( std::fmod( R.Dot(current_grid_x), GridSeparation ),
                //                             std::fmod( R.Dot(current_grid_y), GridSeparation ),
                //                             std::fmod( R.Dot(current_grid_z), GridSeparation ) ) + current_grid_anchor;


                // Verify that the projected point is indeed within the unit cube.
                
                plane<double> pl_x_A( current_grid_x, current_grid_anchor );
                plane<double> pl_y_A( current_grid_y, current_grid_anchor );
                plane<double> pl_z_A( current_grid_z, current_grid_anchor );

                plane<double> pl_x_B( current_grid_x, current_grid_anchor + current_grid_x * GridSeparation );
                plane<double> pl_y_B( current_grid_y, current_grid_anchor + current_grid_y * GridSeparation );
                plane<double> pl_z_B( current_grid_z, current_grid_anchor + current_grid_z * GridSeparation );

/*
                plane<double> pl_x_A( current_grid_x, current_grid_anchor - current_grid_x * GridSeparation * 0.5 );
                plane<double> pl_y_A( current_grid_y, current_grid_anchor - current_grid_y * GridSeparation * 0.5 );
                plane<double> pl_z_A( current_grid_z, current_grid_anchor - current_grid_z * GridSeparation * 0.5 );

                plane<double> pl_x_B( current_grid_x, current_grid_anchor + current_grid_x * GridSeparation * 0.5 );
                plane<double> pl_y_B( current_grid_y, current_grid_anchor + current_grid_y * GridSeparation * 0.5 );
                plane<double> pl_z_B( current_grid_z, current_grid_anchor + current_grid_z * GridSeparation * 0.5 );
*/

                if(  ( pl_x_A.Is_Point_Above_Plane(C) == pl_x_B.Is_Point_Above_Plane(C) )
                ||   ( pl_y_A.Is_Point_Above_Plane(C) == pl_y_B.Is_Point_Above_Plane(C) )
                ||   ( pl_z_A.Is_Point_Above_Plane(C) == pl_z_B.Is_Point_Above_Plane(C) ) ){
                    std::stringstream ss;
                    ss << "Projection is outside the cube: "
                       << C 
                       << " originally: " 
                       << P
                       << " anchor: " 
                       << current_grid_anchor
                       << " axes: "
                       << current_grid_x * GridSeparation
                       << " " 
                       << current_grid_y * GridSeparation
                       << " " 
                       << current_grid_z * GridSeparation;
                    throw std::logic_error(ss.str());
                }

                p_unit_it->first = C;
                ++p_unit_it;
            }
        };
        project_into_unit_cube();


        // Determine the optimal translation.
        //
        // Along each grid direction, the distance from each point to the nearest grid plane will be recorded.
        // Note that we dramatically simplify determining distance to the cube face by adding or subtracting half the
        // scalar distance; since all points have been projecting into the unit cube, at most the point will be
        // 0.5*separation from the nearest plane. Thus if we subtract 1.0*separation for the points in the upper half, we can
        // use simple 1D distribution analysis to determine optimal translations of the anchor point.
        const auto translate_grid_optimally = [&](void) -> void {
            std::vector<double> dist_x;
            std::vector<double> dist_y;
            std::vector<double> dist_z;

            dist_x.reserve(p_unit.size());
            dist_y.reserve(p_unit.size());
            dist_z.reserve(p_unit.size());
            {
                auto p_unit_it = std::begin(p_unit);
                for(const auto &pp : (*pcp_it)->points){
                    //const auto P = pp.first;
                    const auto C = p_unit_it->first - current_grid_anchor;

                    const auto proj_x = current_grid_x.Dot(C - current_grid_anchor);
                    const auto proj_y = current_grid_y.Dot(C - current_grid_anchor);
                    const auto proj_z = current_grid_z.Dot(C - current_grid_anchor);

                    const auto dx = (0.5*GridSeparation < proj_x) ? proj_x - GridSeparation : proj_x;
                    const auto dy = (0.5*GridSeparation < proj_y) ? proj_y - GridSeparation : proj_y;
                    const auto dz = (0.5*GridSeparation < proj_z) ? proj_z - GridSeparation : proj_z;

                    dist_x.emplace_back(dx);
                    dist_y.emplace_back(dy);
                    dist_z.emplace_back(dz);

                    ++p_unit_it;
                }
            }


            const auto shift_x = Stats::Mean(dist_x);
            const auto shift_y = Stats::Mean(dist_y);
            const auto shift_z = Stats::Mean(dist_z);

/*
            const auto shift_x = Stats::Median(dist_x);
            const auto shift_y = Stats::Median(dist_y);
            const auto shift_z = Stats::Median(dist_z);
*/            

            current_grid_anchor += current_grid_x * shift_x
                                 + current_grid_y * shift_y
                                 + current_grid_z * shift_z;

        };
        translate_grid_optimally();

// TODO: Does this invalidate the optimal translation we just found? If so, can anything be done?
        project_into_unit_cube();


        // Determine correspondence points.
        //
        // Every unit-cube projected point is projected onto the faces of the cube. Only the projection on the nearest
        // face is kept.
        auto corresp = (*pcp_it)->points; // Holds the closest corresponding projected point for each point cloud point.
        auto find_corresponding_points = [&](void) -> void {
            // Creates plane for all faces.
            //
            // Note: There is a faster way to do this using the same approach as the translation routine above.
            //       This way is more convenient for testing.
            std::vector<plane<double>> planes;

            planes.emplace_back( current_grid_x, current_grid_anchor );
            planes.emplace_back( current_grid_y, current_grid_anchor );
            planes.emplace_back( current_grid_z, current_grid_anchor );

            planes.emplace_back( current_grid_x, current_grid_anchor + current_grid_x * GridSeparation );
            planes.emplace_back( current_grid_y, current_grid_anchor + current_grid_y * GridSeparation );
            planes.emplace_back( current_grid_z, current_grid_anchor + current_grid_z * GridSeparation );

            auto c_it = std::begin(corresp);
            for(const auto &pp : p_unit){
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
        };
        find_corresponding_points();


        if((loop+1) == loop_max){
            // Write the project points to a file for inspection.
            Write_XYZ("/tmp/cube_projected_points.xyz", p_unit);

            // Write the unit cube edges to a file for inspection.
            Write_Cube_OBJ("/tmp/cube.obj",
                           current_grid_anchor,
                           current_grid_x * GridSeparation,
                           current_grid_y * GridSeparation,
                           current_grid_z * GridSeparation );

            // Write the correspondence points to a file for inspection.
            Write_XYZ("/tmp/cube_corresp_points.xyz", corresp);

            // Write the grid for inspection.
            Write_Grid_OBJ("/tmp/grid.obj",
                           (*pcp_it)->points,
                           current_grid_anchor,
                           current_grid_x * GridSeparation,
                           current_grid_y * GridSeparation,
                           current_grid_z * GridSeparation );

        }

        // Compute some stats about the correspondence.
        //if((loop % 5) == 1){
        {
            std::vector<double> dists;
            auto c_it = std::begin(corresp);
            for(const auto &pp : p_unit){
                const auto P = pp.first;
                const auto C = c_it->first;

                const auto dist = P.distance(C);
                dists.emplace_back(dist);

                ++c_it;
            }

            std::cerr << "Loop: " << loop << std::endl
                      << "  Min:    " << Stats::Min(dists)    << std::endl
                      << "  Mean:   " << Stats::Mean(dists)   << std::endl
                      << "  Median: " << Stats::Median(dists) << std::endl
                      << "  Max:    " << Stats::Max(dists)    << std::endl;
        }

        

        // Determine optimal rotations.
        //
        // This routine rotates the grid axes unit vectors by estimating the optimal rotation of corresponding points.
        // A SVD decomposition provides the rotation matrix that minimizes the difference between corresponding points.
        {

// TODO: Translate about the centre of the unit cube rather than the current (arbitrary) origin. 
//       This will help de-couple the rotational and translational degrees of freedom. 
//       The tricky part will be applying the rotations to the grid afterward, but basically just amounting to a
//       shift->rotate->shift. Be sure the anchor handled correctly too!

            const auto unit_cube_centre = current_grid_x * GridSeparation * 0.5
                                        + current_grid_y * GridSeparation * 0.5
                                        + current_grid_z * GridSeparation * 0.5
                                        + current_grid_anchor;

            const auto N_rows = 3;
            const auto N_cols = (*pcp_it)->points.size();
            Eigen::MatrixXf A(N_rows, N_cols);
            Eigen::MatrixXf B(N_rows, N_cols);

            auto c_it = std::begin(corresp);
            size_t col = 0;
            for(const auto &pp : p_unit){
                const auto P = pp.first; // The point projected into the unit cube.
                const auto C = c_it->first; // The corresponding point somewhere on the unit cube surface.

                const auto P_0 = P - unit_cube_centre;
                const auto C_0 = C - unit_cube_centre;

                A(0, col) = C_0.x;
                A(1, col) = C_0.y;
                A(2, col) = C_0.z;

                B(0, col) = P_0.x;
                B(1, col) = P_0.y;
                B(2, col) = P_0.z;

                ++c_it;
                ++col;
            }
            auto AT = A.transpose();
            auto BAT = B * AT;
//FUNCINFO("A dimensions: " << A.rows() << "x" << A.cols());
//FUNCINFO("B dimensions: " << B.rows() << "x" << B.cols());
//FUNCINFO("AT dimensions: " << AT.rows() << "x" << AT.cols());
//FUNCINFO("BAT dimensions: " << BAT.rows() << "x" << BAT.cols());
//std::cerr << "A is: " << std::endl;
//std::cerr << A << std::endl;
//std::cerr << "B is: " << std::endl;
//std::cerr << B << std::endl;
//std::cerr << "BAT is: " << std::endl;
//std::cerr << BAT << std::endl;

            //Eigen::JacobiSVD<Eigen::MatrixXf> SVD(BAT, Eigen::ComputeThinU | Eigen::ComputeThinV);
            Eigen::JacobiSVD<Eigen::MatrixXf> SVD(BAT, Eigen::ComputeFullU | Eigen::ComputeFullV );
            auto U = SVD.matrixU();
            auto V = SVD.matrixV();
//FUNCINFO("Singular values are: " << SVD.singularValues());
            
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

//FUNCINFO("U dimensions: " << U.rows() << "x" << U.cols());
//FUNCINFO("V dimensions: " << V.rows() << "x" << V.cols());
//FUNCINFO("M dimensions: " << M.rows() << "x" << M.cols());

//std::cerr << "U is: " << std::endl;
//std::cerr << U << std::endl;
//std::cerr << "V is: " << std::endl;
//std::cerr << V << std::endl;
//std::cerr << "M is: " << std::endl;
//std::cerr << M << std::endl;


            Eigen::Vector3f new_grid_x( current_grid_x.x, current_grid_x.y, current_grid_x.z );
            Eigen::Vector3f new_grid_y( current_grid_y.x, current_grid_y.y, current_grid_y.z );
            Eigen::Vector3f new_grid_z( current_grid_z.x, current_grid_z.y, current_grid_z.z );

            auto rot_grid_x = M * new_grid_x;
            auto rot_grid_y = M * new_grid_y;
            auto rot_grid_z = M * new_grid_z;

            const auto previous_grid_x = current_grid_x;
            const auto previous_grid_y = current_grid_y;
            const auto previous_grid_z = current_grid_z;

            current_grid_x = vec3<double>( rot_grid_x(0), rot_grid_x(1), rot_grid_x(2) ).unit();
            current_grid_y = vec3<double>( rot_grid_y(0), rot_grid_y(1), rot_grid_y(2) ).unit();
            current_grid_z = vec3<double>( rot_grid_z(0), rot_grid_z(1), rot_grid_z(2) ).unit();

            //Determine how the anchor point moves.
            //
            // Since we permitted only rotations (relative to the centre of the unit cube) when optimizing the grid axes
            // unit vectors, the centre of the unit cube will be invariant after transforming the grid axes. When th
            // grid axes were rotated, the anchor point also rotated. Starting from the unit cube centre, the anchor can
            // be easily found by translating along the grid axes.
            const auto previous_grid_anchor = current_grid_anchor;
            current_grid_anchor = unit_cube_centre 
                                - current_grid_x * GridSeparation * 0.5
                                - current_grid_y * GridSeparation * 0.5
                                - current_grid_z * GridSeparation * 0.5 ;

std::cerr << "Dot product of new grid axes vectors: " 
          << current_grid_x.Dot(current_grid_y) << "  "
          << current_grid_x.Dot(current_grid_z) << "  "
          << current_grid_y.Dot(current_grid_z) << " # should be (0,0,0)." << std::endl;

std::cerr << "Grid anchor translation: " << std::endl;
std::cerr << (current_grid_anchor - previous_grid_anchor) << std::endl;
std::cerr << "Grid axes moving from: " << std::endl;
std::cerr << previous_grid_x << std::endl;
std::cerr << previous_grid_y << std::endl;
std::cerr << previous_grid_z << std::endl;
std::cerr << " to " << std::endl;
std::cerr << current_grid_x << std::endl;
std::cerr << current_grid_y << std::endl;
std::cerr << current_grid_z << std::endl;
std::cerr << " angle changes: " << std::endl;
std::cerr << current_grid_x.angle(previous_grid_x)*180.0/M_PI << " deg." << std::endl;
std::cerr << current_grid_y.angle(previous_grid_y)*180.0/M_PI << " deg." << std::endl;
std::cerr << current_grid_z.angle(previous_grid_z)*180.0/M_PI << " deg." << std::endl;
std::cerr << " " << std::endl;

        }

}

/*





        // Determine point cloud bounds.
        for(const auto &pp : (*pcp_it)->points){
            rmm_x.Digest( pp.first.Dot(x_unit) );
            rmm_y.Digest( pp.first.Dot(y_unit) );
            rmm_z.Digest( pp.first.Dot(z_unit) );
        }

        // Determine where to centre the grid.
        //
        // TODO: Use the median here instead of mean. It will more often correspond to an actual grid line if the
        //       point cloud orientation is comparable to the grid.
        const double x_mid = (rmm_x.Current_Max() + rmm_x.Current_Min()) * 0.5;
        const double y_mid = (rmm_y.Current_Max() + rmm_y.Current_Min()) * 0.5;
        const double z_mid = (rmm_z.Current_Max() + rmm_z.Current_Min()) * 0.5;

        const double x_diff = (rmm_x.Current_Max() - rmm_x.Current_Min());
        const double y_diff = (rmm_y.Current_Max() - rmm_y.Current_Min());
        const double z_diff = (rmm_z.Current_Max() - rmm_z.Current_Min());

        FUNCINFO("x width = " << x_mid << " +- " << x_diff * 0.5);
        FUNCINFO("y width = " << y_mid << " +- " << y_diff * 0.5);
        FUNCINFO("z width = " << z_mid << " +- " << z_diff * 0.5);

        current_grid_anchor = vec3<double>(x_mid, y_mid, z_mid);

        // Determine initial positions for planes.
        //
        // They should be spaced evenly and should adaquately cover the point cloud with at least one grid line margin.
        std::list<plane<double>> planes;

        for(size_t i = 0; ; ++i){
            const auto dR = static_cast<double>(i) * GridSeparation;

            //plane(const vec3<T> &N_0_in, const vec3<T> &R_0_in);
            const auto N_0_x = current_grid_x;
            const auto N_0_y = current_grid_y;
            const auto N_0_z = current_grid_z;

            planes.emplace_back(N_0_x, current_grid_anchor + N_0_x * dR);
            planes.emplace_back(N_0_y, current_grid_anchor + N_0_y * dR);
            planes.emplace_back(N_0_z, current_grid_anchor + N_0_z * dR);
            if(i != 0){
                planes.emplace_back(N_0_x, current_grid_anchor - N_0_x * dR);
                planes.emplace_back(N_0_y, current_grid_anchor - N_0_y * dR);
                planes.emplace_back(N_0_z, current_grid_anchor - N_0_z * dR);
            }

            if( (dR >= (x_diff * 0.5 + GridSeparation))
            &&  (dR >= (y_diff * 0.5 + GridSeparation))
            &&  (dR >= (z_diff * 0.5 + GridSeparation)) ){
                FUNCINFO("Placed " << 3 + (i-1)*2*3 << " planes in total");
                break;
            }
        }


        // Project every point onto every plane. Keep only the nearest projection.
        auto corresp = (*pcp_it)->points; // Holds the closest corresponding projected point for each point cloud point.
        auto c_it = std::begin(corresp);

        for(const auto &pp : (*pcp_it)->points){
            const auto P = pp.first;

            double closest_dist = std::numeric_limits<double>::infinity();
            vec3<double> closest_proj = NaN_vec3;
            for(const auto &p : planes){
                const auto dist = p.Get_Signed_Distance_To_Point(P);
                if(dist < closest_dist){
                    closest_proj = p.Project_Onto_Plane_Orthogonally(P);
                }
            }

            c_it->first = closest_proj;
            ++c_it;
        }

        // Determine the Procrustes solution with the given correspondence.


        // ...


        // Transform the planar grid according to the Procrustes solution.


        // ...


        // Go back to the loop point above and continue until the grid settles.

        // ...



        // Return the final grid via fit statistics and visually.
        //
        // Note: Either try using any available images or create a set of images with points blitted and grid contours?

        // ...




*/


         
            

//FUNCERR("This routine has not yet been implemented. Refusing to continue");

    }

    return DICOM_data;
}
