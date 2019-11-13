//VoxelRANSAC.cc - A part of DICOMautomaton 2019. Written by hal clark.

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
#include "../YgorImages_Functors/ConvenienceRoutines.h"
#include "../YgorImages_Functors/Grouping/Misc_Functors.h"
#include "../YgorImages_Functors/Compute/Volumetric_Neighbourhood_Sampler.h"
#include "VoxelRANSAC.h"
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

#ifdef DCMA_USE_EIGEN    
#else
    #error "Attempting to compile this operation without Eigen, which is required."
#endif

#include <eigen3/Eigen/Dense>
#include <eigen3/Eigen/Eigenvalues>

#include "YgorClustering.hpp"


OperationDoc OpArgDocVoxelRANSAC(void){
    OperationDoc out;
    out.name = "VoxelRANSAC";

    out.desc = 
        "This routine performs RANSAC fitting using voxel positions as inputs."
        " The search can be confined within ROIs and a range of voxel intensities.";
    
    out.notes.emplace_back(
        "This operation does not make use of voxel intensities during the RANSAC procedure."
        " Voxel intensities are only used to identify which voxel positions are considered."
    );

    out.args.emplace_back();
    out.args.back() = IAWhitelistOpArgDoc();
    out.args.back().name = "ImageSelection";
    out.args.back().default_val = "last";

    out.args.emplace_back();
    out.args.back().name = "NormalizedROILabelRegex";
    out.args.back().desc = "A regex matching ROI labels/names to consider. The default will match"
                      " all available ROIs. Be aware that input spaces are trimmed to a single space."
                      " If your ROI name has more than two sequential spaces, use regex to avoid them."
                      " All ROIs have to match the single regex, so use the 'or' token if needed."
                      " Regex is case insensitive and uses extended POSIX syntax.";
    out.args.back().default_val = ".*";
    out.args.back().expected = true;
    out.args.back().examples = { ".*", ".*Body.*", "Body", "Gross_Liver",
                            R"***(.*Left.*Parotid.*|.*Right.*Parotid.*|.*Eye.*)***",
                            R"***(Left Parotid|Right Parotid)***" };

    out.args.emplace_back();
    out.args.back().name = "ROILabelRegex";
    out.args.back().desc = "A regex matching ROI labels/names to consider. The default will match"
                      " all available ROIs. Be aware that input spaces are trimmed to a single space."
                      " If your ROI name has more than two sequential spaces, use regex to avoid them."
                      " All ROIs have to match the single regex, so use the 'or' token if needed."
                      " Regex is case insensitive and uses extended POSIX syntax.";
    out.args.back().default_val = ".*";
    out.args.back().expected = true;
    out.args.back().examples = { ".*", ".*body.*", "body", "Gross_Liver",
                            R"***(.*left.*parotid.*|.*right.*parotid.*|.*eyes.*)***",
                            R"***(left_parotid|right_parotid)***" };

    out.args.emplace_back();
    out.args.back().name = "ContourOverlap";
    out.args.back().desc = "Controls overlapping contours are treated."
                           " The default 'ignore' treats overlapping contours as a single contour, regardless of"
                           " contour orientation. The option 'honour_opposite_orientations' makes overlapping contours"
                           " with opposite orientation cancel. Otherwise, orientation is ignored. The latter is useful"
                           " for Boolean structures where contour orientation is significant for interior contours (holes)."
                           " The option 'overlapping_contours_cancel' ignores orientation and cancels all contour overlap.";
    out.args.back().default_val = "ignore";
    out.args.back().expected = true;
    out.args.back().examples = { "ignore", "honour_opposite_orientations", 
                                 "overlapping_contours_cancel", "honour_opps", "overlap_cancel" }; 

    out.args.emplace_back();
    out.args.back().name = "Inclusivity";
    out.args.back().desc = "Controls how voxels are deemed to be 'within' the interior of the selected ROI(s)."
                           " The default 'center' considers only the central-most point of each voxel."
                           " There are two corner options that correspond to a 2D projection of the voxel onto the image plane."
                           " The first, 'planar_corner_inclusive', considers a voxel interior if ANY corner is interior."
                           " The second, 'planar_corner_exclusive', considers a voxel interior if ALL (four) corners are interior.";
    out.args.back().default_val = "center";
    out.args.back().expected = true;
    out.args.back().examples = { "center", "centre", 
                                 "planar_corner_inclusive", "planar_inc",
                                 "planar_corner_exclusive", "planar_exc" };

    out.args.emplace_back();
    out.args.back().name = "Channel";
    out.args.back().desc = "The channel to operated on (zero-based)."
                           " Negative values will cause all channels to be operated on.";
    out.args.back().default_val = "0";
    out.args.back().expected = true;
    out.args.back().examples = { "-1",
                                 "0",
                                 "1" };

    out.args.emplace_back();
    out.args.back().name = "Lower";
    out.args.back().desc = "Lower threshold (inclusive) below which voxels will be ignored by this routine.";
    out.args.back().default_val = "-inf";
    out.args.back().expected = true;
    out.args.back().examples = { "-inf",
                                 "0.0",
                                 "1024" };

    out.args.emplace_back();
    out.args.back().name = "Upper";
    out.args.back().desc = "Upper threshold (inclusive) above which voxels will be ignored by this routine.";
    out.args.back().default_val = "inf";
    out.args.back().expected = true;
    out.args.back().examples = { "inf",
                                 "1.0",
                                 "2048" };

    out.args.emplace_back();
    out.args.back().name = "GridSeparation";
    out.args.back().desc = "The known separation of the grid (in DICOM units; mm) being sought.";
    out.args.back().default_val = "nan";
    out.args.back().expected = true;
    out.args.back().examples = { "1.0", 
                                 "1.5",
                                 "10.0",
                                 "1.23E4" };

/*
    out.args.emplace_back();
    out.args.back().name = "Shape";
    out.args.back().desc = "The shape or geometry model to search for.";
    out.args.back().default_val = "vertex_grid";
    out.args.back().expected = true;
    out.args.back().examples = { "vertex_grid" };
*/

    return out;
}

Drover VoxelRANSAC(Drover DICOM_data, OperationArgPkg OptArgs, std::map<std::string,std::string> /*InvocationMetadata*/, std::string /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();

    const auto NormalizedROILabelRegex = OptArgs.getValueStr("NormalizedROILabelRegex").value();
    const auto ROILabelRegex = OptArgs.getValueStr("ROILabelRegex").value();

    const auto InclusivityStr = OptArgs.getValueStr("Inclusivity").value();
    const auto ContourOverlapStr = OptArgs.getValueStr("ContourOverlap").value();

    const auto Channel = std::stol( OptArgs.getValueStr("Channel").value() );

    const auto Lower = std::stod( OptArgs.getValueStr("Lower").value());
    const auto Upper = std::stod( OptArgs.getValueStr("Upper").value());

    const auto GridSeparation = std::stod( OptArgs.getValueStr("GridSeparation").value() );

    //-----------------------------------------------------------------------------------------------------------------
    const auto regex_centre = Compile_Regex("^cent.*");
    const auto regex_pci = Compile_Regex("^planar_?c?o?r?n?e?r?s?_?inc?l?u?s?i?v?e?$");
    const auto regex_pce = Compile_Regex("^planar_?c?o?r?n?e?r?s?_?exc?l?u?s?i?v?e?$");

    const auto regex_ignore = Compile_Regex("^ig?n?o?r?e?$");
    const auto regex_honopps = Compile_Regex("^ho?n?o?u?r?_?o?p?p?o?s?i?t?e?_?o?r?i?e?n?t?a?t?i?o?n?s?$");
    const auto regex_cancel = Compile_Regex("^ov?e?r?l?a?p?p?i?n?g?_?c?o?n?t?o?u?r?s?_?c?a?n?c?e?l?s?$");


    if(!std::isfinite(GridSeparation) || (GridSeparation <= 0.0)){
        throw std::invalid_argument("Grid separation is not valid. Cannot continue.");
    }

    //Stuff references to all contours into a list. Remember that you can still address specific contours through
    // the original holding containers (which are not modified here).
    auto cc_all = All_CCs( DICOM_data );
    auto cc_ROIs = Whitelist( cc_all, { { "ROIName", ROILabelRegex },
                                        { "NormalizedROIName", NormalizedROILabelRegex } } );
    if(cc_ROIs.empty()){
        throw std::invalid_argument("No contours selected. Cannot continue.");
    }


    auto IAs_all = All_IAs( DICOM_data );
    auto IAs = Whitelist( IAs_all, ImageSelectionStr );
    for(auto & iap_it : IAs){

        // --------------------------------
        // Prepare to gather the voxel positions.
        std::mutex p_locker;
        std::vector<vec3<double>> p;

        PartitionedImageVoxelVisitorMutatorUserData ud;

        ud.mutation_opts.editstyle = Mutate_Voxels_Opts::EditStyle::InPlace;
        ud.mutation_opts.aggregate = Mutate_Voxels_Opts::Aggregate::First;
        ud.mutation_opts.adjacency = Mutate_Voxels_Opts::Adjacency::SingleVoxel;
        ud.mutation_opts.maskmod   = Mutate_Voxels_Opts::MaskMod::Noop;
        //ud.description = "DBSCAN clustering: read-only scan performed";

        if(false){
        }else if( std::regex_match(ContourOverlapStr, regex_ignore) ){
            ud.mutation_opts.contouroverlap = Mutate_Voxels_Opts::ContourOverlap::Ignore;
        }else if( std::regex_match(ContourOverlapStr, regex_honopps) ){
            ud.mutation_opts.contouroverlap = Mutate_Voxels_Opts::ContourOverlap::HonourOppositeOrientations;
        }else if( std::regex_match(ContourOverlapStr, regex_cancel) ){
            ud.mutation_opts.contouroverlap = Mutate_Voxels_Opts::ContourOverlap::ImplicitOrientations;
        }else{
            throw std::invalid_argument("ContourOverlap argument '"_s + ContourOverlapStr + "' is not valid");
        }
        if(false){
        }else if( std::regex_match(InclusivityStr, regex_centre) ){
            ud.mutation_opts.inclusivity = Mutate_Voxels_Opts::Inclusivity::Centre;
        }else if( std::regex_match(InclusivityStr, regex_pci) ){
            ud.mutation_opts.inclusivity = Mutate_Voxels_Opts::Inclusivity::Inclusive;
        }else if( std::regex_match(InclusivityStr, regex_pce) ){
            ud.mutation_opts.inclusivity = Mutate_Voxels_Opts::Inclusivity::Exclusive;
        }else{
            throw std::invalid_argument("Inclusivity argument '"_s + InclusivityStr + "' is not valid");
        }

        ud.f_bounded = [&](long int row, long int col, long int chan, std::reference_wrapper<planar_image<float,double>> img_refw, float &voxel_val) {
            if( (Channel < 0) || (Channel == chan) ){
                if(isininc(Lower, voxel_val, Upper)){
                    const auto pos = img_refw.get().position(row,col);

                    std::lock_guard<std::mutex> lock(p_locker);
                    p.push_back(pos);
                }
            }
            return;
        };

        // Locate voxels to consider.
        if(!(*iap_it)->imagecoll.Process_Images_Parallel( GroupIndividualImages,
                                                          PartitionedImageVoxelVisitorMutator,
                                                          {}, cc_ROIs, &ud )){
            throw std::runtime_error("Unable to locate voxels to be used for RANSAC.");
        }
        const long int BeforeCount = p.size();

        // --------------------------------
        // Perform RANSAC.
        FUNCINFO("Number of voxels being used for RANSAC: " << BeforeCount);

/*
        const vec3<double> z_unit(0.0, 0.0, 1.0);
        const vec3<double> x_unit(1.0, 0.0, 0.0);

        for(const auto &anchor : p){
            // A 3D vertex grid can be parameterized with 8 scalars:
            //   - an anchor point (the '0,0,0' vertex, which is bounded within the volume of one cubic proto cell),
            //   - two angles orienting the 3D grid,
            //   - the isotropic separation between vertices (along x-, y-, and z-axes).
            // A grid with known separation needs only 7 numbers. 
            //
            // It is not trivial to locate a 3D grid. Even the 2D case is difficult. 
            // The issue is that the grid is infinite and a small number of vertices can have multiple
            // good-quality grids if the vertices are separated by a distance at least somewhat larger than
            // the grid spacing.
            //
            // This problem can be found in the literature as referred to as the 
            //    "no a priori correspondence Procrustes problem", 
            // though it is often disscused in 2D, not 3D. Iterative solutions are known,
            // such as the softassign procrustes method, but they are generally computationally involved,
            // not guaranteed to converge, and not easy to tailor to an infinite (3D) grid.
            //
            // The approach taken here simplifies the problem considerably, but relies on numerical optimization
            // and accepts some positional error as inevitable.
            // In a nutshell, a random vertex is selected and a candidate 3D grid is anchored to it.
            // Grid orientation is determined by repeatedly projecting the remaining vertices into the (0,0,0) cell
            // and computing the distance to the nearest corner. Fit quality is assessed using residuals derived
            // from the distances. The best fits are scored, and the overall best is selected.
            //
            // Note that this is not really true RANSAC since every vertex could potentially be sampled, even for
            // fairly large data sets. However, some sampling could be performed to speed up analysis.

            // Initial conditions.
            vec3<double> grid_z = z_unit;
            vec3<double> grid_x = x_unit; 


        }
*/

/*
        // Perform a true RANSAC fit of planes.
        //
        // This might not be the most economical approach because the grid is volumetric and there will also be lots of
        // coincidences along diagonals.

        const vec3<double> x_unit(1.0, 0.0, 0.0);
        const vec3<double> y_unit(0.0, 1.0, 0.0);
        const vec3<double> z_unit(0.0, 0.0, 1.0);

        double best_x_plane_score = -1.0;
        double best_y_plane_score = -1.0;
        double best_z_plane_score = -1.0;

        plane<double> best_x_plane;
        plane<double> best_y_plane;
        plane<double> best_z_plane;

        long int random_seed = 11;
        std::mt19937 re( random_seed );

        long int rounds_max = 50'000;
        double tolerance_dist = 3.0; // in mm.

        long int failures = 0;

        for(long int round = 0; round < rounds_max; ++round){
            try{
        
                // 1: Select randomly the minimum number of points required to determine the model parameters.
                std::vector<vec3<double>> samples;
                std::sample(std::begin(p), std::end(p), std::back_inserter(samples), 3, re);

                // 2: Solve for the parameters of the model.
                const auto plane = Plane_Orthogonal_Regression(samples);

                // 3: Determine how many points from the set of all points fit with a predefined tolerance.
                double score = 0.0;

                for(const auto &v : p){
                    const auto dist = plane.Get_Signed_Distance_To_Point(v);

                    // Option 1: merely check for vertices within some tolerance distance. This is the vanilla RANSAC
                    //           approach. For regular grids this approach will likely end up selecting diagonals,
                    //           depending on the grid dimensions.
                    //if(dist <= std::abs(tolerance_dist)){
                    //    ++score;
                    //}

                    // Option 2: project the vertex into a virtual slice of the grid adjacent to the plane. Sum the
                    //           distance of each projection with the plane (and its neighbours) to compute the total
                    //           penalty.
                    const auto remainder = std::remainder(dist, GridSeparation);
                    if(remainder > 0.5*GridSeparation) remainder -= GridSeparation;
                    const auto proj_dist = plane.Get_Signed_Distance_To_Point( plane.N_0 * remainder );

                    // FIX THE SIGN! TODO!

                }

                // 4: If the fraction of the number of inliers over the total number points in the set exceeds a predefined
                // threshold number, re-estimate the model parameters using all the identified inliers and terminate.
                const auto proj_x = std::abs( x_unit.Dot( plane.N_0 ) );
                const auto proj_y = std::abs( y_unit.Dot( plane.N_0 ) );
                const auto proj_z = std::abs( z_unit.Dot( plane.N_0 ) );

                if( (proj_x >= proj_y) && (proj_x >= proj_z) ){
                    if( score > best_x_plane_score ){
                        best_x_plane = plane;
                        best_x_plane_score = score;
                    }
                }else if( proj_y >= proj_z ){
                    if( score > best_y_plane_score ){
                        best_y_plane = plane;
                        best_y_plane_score = score;
                    }
                }else{
                    if( score > best_z_plane_score ){
                        best_z_plane = plane;
                        best_z_plane_score = score;
                    }
                }

                // 5: Otherwise, repeat steps 1 through 4 (maximum of N times).

                //...
            }catch(const std::exception &){ 
                ++failures;

                if( (round > 100) && (failures >= round) ){
                    break;
                }
            }
        }

        if(best_plane_score >= 0){
            // Prune the lines that were used and try again.

            // for(all points)
            //     if(dist_to_plane is within tolerance)
            //         remove point

            // Add the plane to the list of accepted planes.
        }
*/

/*

// Breaking this problem down into smaller parts, we will independently estimate the grid orientation and then fit the
// grid.
//
// Grid orientation can be estimated in a number of ways. PCA might be able to estimate it out of the box, but I worry
// that outliers (which are expected due to the grid isolation pre-processing not being perfect) will impact the
// results. Instead, the local neighbourhood of every vertex will be queried and the unit vector connections will be
// aggregated. Then the normals will be condensed into groupings that are as orthogonal as possible (perhaps using PCA),
// then all normals will be classified as belonging to one of the three groupings, and the three group orientations will
// be estimated using a robust estimator.
//
// After the grid orientation has been estimated, it can be more easily fit using 1D translation (three times; once for
// each grid plane). There might be an easy way to merely project all datum to a 2-cell-wide slice (perhaps by
// duplicating points to be above AND below the midplane) and fitting using least-median-squares, but I'm not sure
// exactly how this would work. The key point is that fixing the orientation will eliminate the link between grid plane
// estimation, simplifying the problem immensely.
//


*/
        // Stage 1: grid orientation estimation.
        //
        // The local neighbourhood surrounding each vertex needs to be queryable, so an r*-tree is used to index the
        // vertices.

/*
using SpatialType = double;
using SpatialDimensionCount = 3;
std::array<SpatialType, SpatialDimensionCount> Coordinates;
*/

/*
        p.clear();
        p.push_back( vec3<double>(  0.0,  0.0,  0.0 ) );
        p.push_back( vec3<double>( 15.0,  0.0,  0.0 ) );
        p.push_back( vec3<double>(  0.0, 15.0,  0.0 ) );
        p.push_back( vec3<double>(  0.0,  0.0, 15.0 ) );
        p.push_back( vec3<double>( 15.0, 15.0,  0.0 ) );
        p.push_back( vec3<double>( 15.0,  0.0, 15.0 ) );
        p.push_back( vec3<double>(  0.0, 15.0, 15.0 ) );
        p.push_back( vec3<double>( 15.0, 15.0, 15.0 ) );

        p.push_back( vec3<double>( 30.0, 15.0, 15.0 ) );
        p.push_back( vec3<double>( 15.0, 30.0, 15.0 ) );
        p.push_back( vec3<double>( 15.0, 15.0, 30.0 ) );
        p.push_back( vec3<double>( 30.0, 30.0, 15.0 ) );
        p.push_back( vec3<double>( 30.0, 15.0, 30.0 ) );
        p.push_back( vec3<double>( 15.0, 30.0, 30.0 ) );
        p.push_back( vec3<double>( 30.0, 30.0, 30.0 ) );
*/


        // DBSCAN setup.
        constexpr size_t MaxElementsInANode = 6; // 16, 32, 128, 256, ... ?
        using RTreeParameter_t = boost::geometry::index::rstar<MaxElementsInANode>;

        using UserData_t = uint8_t; //std::pair< planar_image<float,double>*, long int >;
        using CDat_t = ClusteringDatum<3, double, // Spatial dimensions.
                                       0, double, // Attribute dimensions (not used).
                                       uint8_t,  // Cluster ID type.
                                       UserData_t>;
        //typedef boost::geometry::model::box<CDat_t> Box_t;
        using RTree_t = boost::geometry::index::rtree<CDat_t,RTreeParameter_t>;


        RTree_t rtree;
        for(const auto &v : p){
            rtree.insert(CDat_t({ v.x, v.y, v.z }));
        }

/*
    OnEachDatum<RTree_t,CDat_t,std::function<void(const typename RTree_t::const_query_iterator &)>>(rtree,
        [&ClusterIDMap,&ClusterIDUniques](const typename RTree_t::const_query_iterator &it) -> void {
            ClusterIDMap[ it->UserData ] = it->CID;
            ClusterIDUniques.insert( it->CID );
        });
*/
        long int random_seed = 11;
        std::mt19937 re( random_seed );

        std::vector<vec3<double>> samples;
        std::sample(std::begin(p), std::end(p), std::back_inserter(samples), 100, re);

        // Query the local neighbourhood for the nearest N vertices. Remember that the self will be present and we
        // cannot derive any useful orientation from it. 
        const long int N_neighbours = 6; // legitimate neighbours.
        const double min_separation = 0.1; // minimal distance needed between vertices to consider a pair (in DICOM units; mm).
        std::vector<vec3<double>> unit_vecs;
        for(const auto &v : samples){
            long int actual_neighbours = 0;
            constexpr auto RTreeSpatialQueryGetAll = [](const CDat_t &) -> bool { return true; };
            RTree_t::const_query_iterator it;
            it = rtree.qbegin(boost::geometry::index::satisfies( RTreeSpatialQueryGetAll ));
            for( ; it != rtree.qend(); ++it){
                const auto l_v = vec3<double>( std::get<0>(it->Coordinates),
                                               std::get<1>(it->Coordinates),
                                               std::get<2>(it->Coordinates) );
                // Check if the point is separated a reasonable distance away.
                const auto d = v.distance(l_v);
                if(d > min_separation) continue;
                
                // Estimate the unit vector between vertices.
                auto U = (v - l_v).unit();

                // Ensure it points along the positive direction, reversing it if necessary.
                // This approach does not result in any loss of generality.
                if(U.x < 0.0) U.x *= -1.0;
                if(U.y < 0.0) U.y *= -1.0;
                if(U.z < 0.0) U.z *= -1.0;

                unit_vecs.push_back(U);

                ++actual_neighbours;
                if(actual_neighbours >= N_neighbours) break;
            }
        }

        FUNCINFO("The number of unit vectors to analyze: " << unit_vecs.size());

        // Determine the three most prominent unit vectors.
        // This is accomplished via PCA.
        Eigen::MatrixXd mat;
        const size_t mat_rows = p.size();
        const size_t mat_cols = 3;
        mat.resize(mat_rows, mat_cols);
        {
            size_t i = 0;
            for(const auto &v : p){
                mat(i, 0) = static_cast<double>(v.x);
                mat(i, 1) = static_cast<double>(v.y);
                mat(i, 2) = static_cast<double>(v.z);
                ++i;
            }
        }

        Eigen::MatrixXd centered = mat.rowwise() - mat.colwise().mean();
        Eigen::MatrixXd cov = centered.adjoint() * centered;
        Eigen::SelfAdjointEigenSolver<Eigen::MatrixXd> eig(cov);
        
        Eigen::VectorXd evals = eig.eigenvalues();
        Eigen::MatrixXd evecs = eig.eigenvectors().real();

        const vec3<double> grid_u_a( evecs(0,0), evecs(1,0), evecs(2,0) );
        const vec3<double> grid_u_b( evecs(0,1), evecs(1,1), evecs(2,1) );
        const vec3<double> grid_u_c( evecs(0,2), evecs(1,2), evecs(2,2) );

        FUNCINFO(" grid units:  " << grid_u_a << ", " << grid_u_b << ", " << grid_u_c );



        // Visualize the plane for debugging/development purposes. 
        //
        //  ... TODO ...

        // --------------------------------
        // 

        //FUNCINFO("Number of voxels with valid cluster IDs: " << AfterCount 
        //    << " (" << (1.0 / 100.0) * static_cast<long int>( 10000.0 * AfterCount / BeforeCount ) << "%)");

    }

    return DICOM_data;
}
