//AlignPoints.cc - A part of DICOMautomaton 2020. Written by hal clark.

#include <asio.hpp>
#include <algorithm>
#include <optional>
#include <fstream>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <regex>
#include <set> 
#include <stdexcept>
#include <string>    
#include <utility>            //Needed for std::pair.
#include <vector>

#ifdef DCMA_USE_EIGEN    
    #include <eigen3/Eigen/Dense>
    #include <eigen3/Eigen/Eigenvalues>
    #include <eigen3/Eigen/SVD>
    #include <eigen3/Eigen/QR>
    #include <eigen3/Eigen/Cholesky>
#endif

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../Thread_Pool.h"
#include "AlignPoints.h"
#include "Explicator.h"       //Needed for Explicator class.
#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorStats.h"        //Needed for Stats:: namespace.
#include "YgorString.h"       //Needed for GetFirstRegex(...)



// This routine performs a simple centroid-based alignment.
//
// The resultant transformation is a rotation-less shift so the point cloud centres-of-mass overlap.
//
// Note that this routine only identifies a transform, it does not implement it by altering the point clouds.
//
static
std::optional<affine_transform<double>>
AlignViaCentroid(const point_set<double> & moving,
                 const point_set<double> & stationary ){
    affine_transform<double> t;

    // Compute the centroid for both point clouds.
    const auto centroid_s = stationary.Centroid();
    const auto centroid_m = moving.Centroid();

    const auto dcentroid = (centroid_s - centroid_m);
    t.coeff(3,0) = dcentroid.x;
    t.coeff(3,1) = dcentroid.y;
    t.coeff(3,2) = dcentroid.z;

    return t;
}
    
#ifdef DCMA_USE_EIGEN
// This routine performs a PCA-based alignment.
//
// First, the moving point cloud is translated the moving point cloud so the centre of mass aligns to the reference
// point cloud, performs PCA separately on the reference and moving point clouds, compute distribution moments along
// each axis to determine the direction, and then rotates the moving point cloud so the principle axes coincide.
//
// Note that this routine only identifies a transform, it does not implement it by altering the point clouds.
//
static
std::optional<affine_transform<double>>
AlignViaPCA(const point_set<double> & moving,
            const point_set<double> & stationary ){
    affine_transform<double> t;

    // Compute the centroid for both point clouds.
    const auto centroid_s = stationary.Centroid();
    const auto centroid_m = moving.Centroid();
    
    // Compute the PCA for both point clouds.
    struct pcomps {
        vec3<double> pc1;
        vec3<double> pc2;
        vec3<double> pc3;
    };
    const auto est_PCA = [](const point_set<double> &ps) -> pcomps {
        // Determine the three most prominent unit vectors via PCA.
        Eigen::MatrixXd mat;
        const size_t mat_rows = ps.points.size();
        const size_t mat_cols = 3;
        mat.resize(mat_rows, mat_cols);
        {
            size_t i = 0;
            for(const auto &v : ps.points){
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

        pcomps out;
        out.pc1 = vec3<double>( evecs(0,0), evecs(1,0), evecs(2,0) ).unit();
        out.pc2 = vec3<double>( evecs(0,1), evecs(1,1), evecs(2,1) ).unit();
        out.pc3 = vec3<double>( evecs(0,2), evecs(1,2), evecs(2,2) ).unit();
        return out;
    };

    const auto pcomps_stationary = est_PCA(stationary);
    const auto pcomps_moving = est_PCA(moving);

    // Compute centroid-centered third-order moments (i.e., skew) along each component and use them to reorient the principle components.
    // The third order is needed since the first-order (mean) is eliminated via centroid-shifting, and the second order
    // (variance) cannot differentiate positive and negative directions.
    const auto reorient_pcomps = [](const vec3<double> &centroid,
                                    const pcomps &comps,
                                    const point_set<double> &ps) {

        Stats::Running_Sum<double> rs_pc1;
        Stats::Running_Sum<double> rs_pc2;
        Stats::Running_Sum<double> rs_pc3;
        for(const auto &v : ps.points){
            const auto sv = (v - centroid);

            const auto proj_pc1 = sv.Dot(comps.pc1);
            rs_pc1.Digest( std::pow(proj_pc1, 3.0) );
            const auto proj_pc2 = sv.Dot(comps.pc2);
            rs_pc2.Digest( std::pow(proj_pc2, 3.0) );
            const auto proj_pc3 = sv.Dot(comps.pc3);
            rs_pc3.Digest( std::pow(proj_pc3, 3.0) );
        }

        pcomps out;
        out.pc1 = (comps.pc1 * rs_pc1.Current_Sum()).unit(); // Will be either + or - the original pcomps.
        out.pc2 = (comps.pc2 * rs_pc2.Current_Sum()).unit(); // Will be either + or - the original pcomps.
        out.pc3 = (comps.pc3 * rs_pc3.Current_Sum()).unit(); // Will be either + or - the original pcomps.


        // Handle 2D degeneracy.
        //
        // If the space is degenerate with all points being coplanar, then the first (strongest) principle component
        // will be orthogonal to the plane and the corresponding moment will be zero. The other two reoriented
        // components will still be valid, and the underlying principal component is correct; we just don't know the
        // direction because the moment is zero. However, we can determine it in a consistent way by relying on the
        // other two (valid) adjusted components.
        if( !(out.pc1.isfinite())
        &&  out.pc2.isfinite() 
        &&  out.pc3.isfinite() ){
            out.pc1 = out.pc3.Cross( out.pc2 ).unit();
        }

        // Handle 1D degeneracy (somewhat).
        //
        // If the space is degenerate with all points being colinear, then the first two principle components
        // will be randomly oriented orthgonal to the line and the last component will be tangential to the line
        // with a direction derived from the moment. We cannot unambiguously recover the first two components, but we
        // can at least fall back on the original principle components.
        if( !(out.pc1.isfinite()) ) out.pc1 = comps.pc1;
        if( !(out.pc2.isfinite()) ) out.pc2 = comps.pc2;
        //if( !(out.pc3.isfinite()) ) out.pc3 = comps.pc3;

        return out;
    };

    const auto reoriented_pcomps_stationary = reorient_pcomps(centroid_s,
                                                              pcomps_stationary,
                                                              stationary);
    const auto reoriented_pcomps_moving = reorient_pcomps(centroid_m,
                                                          pcomps_moving,
                                                          moving);

    FUNCINFO("Stationary point cloud:");
    FUNCINFO("    centroid             : " << centroid_s);
    FUNCINFO("    pcomp_pc1            : " << pcomps_stationary.pc1);
    FUNCINFO("    pcomp_pc2            : " << pcomps_stationary.pc2);
    FUNCINFO("    pcomp_pc3            : " << pcomps_stationary.pc3);
    FUNCINFO("    reoriented_pcomp_pc1 : " << reoriented_pcomps_stationary.pc1);
    FUNCINFO("    reoriented_pcomp_pc2 : " << reoriented_pcomps_stationary.pc2);
    FUNCINFO("    reoriented_pcomp_pc3 : " << reoriented_pcomps_stationary.pc3);

    FUNCINFO("Moving point cloud:");
    FUNCINFO("    centroid             : " << centroid_m);
    FUNCINFO("    pcomp_pc1            : " << pcomps_moving.pc1);
    FUNCINFO("    pcomp_pc2            : " << pcomps_moving.pc2);
    FUNCINFO("    pcomp_pc3            : " << pcomps_moving.pc3);
    FUNCINFO("    reoriented_pcomp_pc1 : " << reoriented_pcomps_moving.pc1);
    FUNCINFO("    reoriented_pcomp_pc2 : " << reoriented_pcomps_moving.pc2);
    FUNCINFO("    reoriented_pcomp_pc3 : " << reoriented_pcomps_moving.pc3);

    // Determine the linear transformation that will align the reoriented principle components.
    //
    // If we assemble the orthonormal principle component vectors for each cloud into a 3x3 matrix (i.e., three column
    // vectors) we get an orthonormal matrix. The transformation matrix 'A' needed to transform the moving matrix 'M'
    // into the stationary matrix 'S' can be found from $S = AM$. Since M is orthonormal, $M^{-1}$ always exists and
    // also $M^{-1} = M^{T}$. So $A = SM^{T}$.

    {
        Eigen::Matrix3d S;
        S(0,0) = reoriented_pcomps_stationary.pc1.x;
        S(1,0) = reoriented_pcomps_stationary.pc1.y;
        S(2,0) = reoriented_pcomps_stationary.pc1.z;

        S(0,1) = reoriented_pcomps_stationary.pc2.x;
        S(1,1) = reoriented_pcomps_stationary.pc2.y;
        S(2,1) = reoriented_pcomps_stationary.pc2.z;

        S(0,2) = reoriented_pcomps_stationary.pc3.x;
        S(1,2) = reoriented_pcomps_stationary.pc3.y;
        S(2,2) = reoriented_pcomps_stationary.pc3.z;

        Eigen::Matrix3d M;
        M(0,0) = reoriented_pcomps_moving.pc1.x;
        M(1,0) = reoriented_pcomps_moving.pc1.y;
        M(2,0) = reoriented_pcomps_moving.pc1.z;

        M(0,1) = reoriented_pcomps_moving.pc2.x;
        M(1,1) = reoriented_pcomps_moving.pc2.y;
        M(2,1) = reoriented_pcomps_moving.pc2.z;

        M(0,2) = reoriented_pcomps_moving.pc3.x;
        M(1,2) = reoriented_pcomps_moving.pc3.y;
        M(2,2) = reoriented_pcomps_moving.pc3.z;

        auto A = S * M.transpose();
        // Force the transform to be the identity for debugging.
        //A << 1.0, 0.0, 0.0,
        //     0.0, 1.0, 0.0, 
        //     0.0, 0.0, 1.0;

        t.coeff(0,0) = A(0,0);
        t.coeff(0,1) = A(1,0);
        t.coeff(0,2) = A(2,0);

        t.coeff(1,0) = A(0,1);
        t.coeff(1,1) = A(1,1);
        t.coeff(1,2) = A(2,1);

        t.coeff(2,0) = A(0,2);
        t.coeff(2,1) = A(1,2);
        t.coeff(2,2) = A(2,2);

        // Work out the translation vector.
        //
        // Because the centroid is not explicitly subtracted, we have to incorporate the subtraction into the translation term.
        // Ideally we would perform $A * (M - centroid_{M}) + centroid_{S}$ explicitly; to emulate this, we can rearrange to find
        // $A * M + \left( centroid_{S} - A * centroid_{M} \right) \equiv A * M + b$ where $b = centroid_{S} - A * centroid_{M}$ is the
        // necessary translation term.
        {
            Eigen::Vector3d e_centroid_m(centroid_m.x, centroid_m.y, centroid_m.z);
            auto A_e_centroid_m = A * e_centroid_m; 

            t.coeff(3,0) = centroid_s.x - A_e_centroid_m(0);
            t.coeff(3,1) = centroid_s.y - A_e_centroid_m(1);
            t.coeff(3,2) = centroid_s.z - A_e_centroid_m(2);
        }
    }

    //FUNCINFO("Final linear transform:");
    //FUNCINFO("    ( " << t.coeff(0,0) << "  " << t.coeff(1,0) << "  " << t.coeff(2,0) << " )");
    //FUNCINFO("    ( " << t.coeff(0,1) << "  " << t.coeff(1,1) << "  " << t.coeff(2,1) << " )");
    //FUNCINFO("    ( " << t.coeff(0,2) << "  " << t.coeff(1,2) << "  " << t.coeff(2,2) << " )");
    //FUNCINFO("Final translation:");
    //FUNCINFO("    ( " << t.coeff(3,0) << " )");
    //FUNCINFO("    ( " << t.coeff(3,1) << " )");
    //FUNCINFO("    ( " << t.coeff(3,2) << " )");
    //FUNCINFO("Final Affine transformation:");
    //t.write_to(std::cout);

    return t;
}
#endif // DCMA_USE_EIGEN


#ifdef DCMA_USE_EIGEN
// This routine performs an exhaustive iterative closest point (ICP) alignment.
//
// Note that this routine only identifies a transform, it does not implement it by altering the point clouds.
//
static
std::optional<affine_transform<double>>
AlignViaExhaustiveICP( const point_set<double> & moving,
                       const point_set<double> & stationary,
                       long int max_icp_iters = 100,
                       double f_rel_tol = std::numeric_limits<double>::quiet_NaN() ){

    // The WIP transformation.
    affine_transform<double> t;

    // The transformation that resulted in the lowest cost estimate so far.
    affine_transform<double> t_best;
    double f_best = std::numeric_limits<double>::infinity();

    // Compute the centroid for both point clouds.
    const auto centroid_s = stationary.Centroid();
    const auto centroid_m = moving.Centroid();

    point_set<double> working(moving);
    point_set<double> corresp(moving);
    
    // Prime the transformation using a simplistic alignment.
    //
    // Note: The initial transformation will only be used to establish correspondence in the first iteration, so it
    // might be tolerable to be somewhat coarse. Note, however, that a bad initial guess (in the sense that the true
    // optimal alignment is impeded by many local minima) will certainly negatively impact the convergence rate, and may
    // actually make it impossible to find the true alignment using this alignment method. Therefore, the PCA method is
    // used by default. If problems are encountered with the PCA method, resorting to the centroid method may be
    // sufficient.
    //
    // Default:
    t = AlignViaPCA(moving, stationary).value();
    //
    // Fallback:
    //t = AlignViaCentroid(moving, stationary).value();

    double f_prev = std::numeric_limits<double>::quiet_NaN();
    for(long int icp_iter = 0; icp_iter < max_icp_iters; ++icp_iter){
        // Copy the original points.
        working.points = moving.points;

        // Apply the current transformation to the working points.
        t.apply_to(working);
        const auto centroid_w = working.Centroid();

        // Exhaustively determine the correspondence between stationary and working points under the current
        // transformation. Note that multiple working points may correspond to the same stationary point.
        const auto N_working_points = working.points.size();
        if(N_working_points != corresp.points.size()) throw std::logic_error("Encountered inconsistent working buffers. Cannot continue.");
        {
            asio_thread_pool tp;
            for(size_t i = 0; i < N_working_points; ++i){
                tp.submit_task([&,i](void) -> void {
                    const auto w_p = working.points[i];
                    double min_sq_dist = std::numeric_limits<double>::infinity();
                    for(const auto &s_p : stationary.points){
                        const auto sq_dist = w_p.sq_dist(s_p);
                        if(sq_dist < min_sq_dist){
                            min_sq_dist = sq_dist;
                            corresp.points[i] = s_p;
                        }
                    }
                }); // thread pool task closure.
            }
        } // Wait until all threads are done.


        ///////////////////////////////////

        // Using the correspondence, estimate the linear transformation that will maximize alignment between
        // centroid-shifted point clouds.
        //
        // Note: the transformation we seek here ignores translations by explicitly subtracting the centroid from each
        // point cloud. Translations will be added into the full transformation later. 
        const auto N_rows = 3;
        const auto N_cols = N_working_points;
        Eigen::MatrixXd S(N_rows, N_cols);
        Eigen::MatrixXd M(N_rows, N_cols);

        for(size_t i = 0; i < N_working_points; ++i){
            // Note: Find the transform using the original point clouds (with a centroid shift) and the updated
            // correspondence information.

            S(0, i) = corresp.points[i].x - centroid_s.x; // The desired point location.
            S(1, i) = corresp.points[i].y - centroid_s.y;
            S(2, i) = corresp.points[i].z - centroid_s.z;

            M(0, i) = moving.points[i].x - centroid_w.x; // The actual point location.
            M(1, i) = moving.points[i].y - centroid_w.y;
            M(2, i) = moving.points[i].z - centroid_w.z;
        }
        auto ST = S.transpose();
        auto MST = M * ST;

        //Eigen::JacobiSVD<Eigen::MatrixXd> SVD(MST, Eigen::ComputeThinU | Eigen::ComputeThinV);
        Eigen::JacobiSVD<Eigen::MatrixXd> SVD(MST, Eigen::ComputeFullU | Eigen::ComputeFullV );
        auto U = SVD.matrixU();
        auto V = SVD.matrixV();

        // Use the SVD result directly.
        //
        // Note that spatial inversions are permitted this way.
        //auto A = U * V.transpose();

        // Attempt to restrict to rotations only.    NOTE: Does not appear to work?
        //Eigen::Matrix3d PI;
        //PI << 1.0 , 0.0 , 0.0,
        //      0.0 , 1.0 , 0.0,
        //      0.0 , 0.0 , ( U * V.transpose() ).determinant();
        //auto A = U * PI * V.transpose();

        // Restrict the solution to rotations only. (Refer to the 'Kabsch algorithm' for more info.)
        Eigen::Matrix3d PI;
        PI << 1.0 , 0.0 , 0.0
            , 0.0 , 1.0 , 0.0
            , 0.0 , 0.0 , ( V * U.transpose() ).determinant();
        auto A = V * PI * U.transpose();

/*
        // Apply the linear transformation to a point directly.
        auto Apply_Rotation = [&](const vec3<double> &v) -> vec3<double> {
            Eigen::Vector3f e_vec3(v.x, v.y, v.z);
            auto new_v = A * e_vec3;
            return vec3<double>( new_v(0), new_v(1), new_v(2) );
        };
*/

        // Transfer the transformation into a full Affine transformation.
        t = affine_transform<double>();

        // Rotation and scaling components.
        t.coeff(0,0) = A(0,0);
        t.coeff(0,1) = A(1,0);
        t.coeff(0,2) = A(2,0);

        t.coeff(1,0) = A(0,1);
        t.coeff(1,1) = A(1,1);
        t.coeff(1,2) = A(2,1);

        t.coeff(2,0) = A(0,2);
        t.coeff(2,1) = A(1,2);
        t.coeff(2,2) = A(2,2);

        // The complete transformation we have found for bringing the moving points $P_{M}$ into alignment with the
        // stationary points is:
        //
        //   $centroid_{S} + A * \left( P_{M} - centroid_{M} \right)$.
        //
        // Rearranging, an Affine transformation of the form $A * P_{M} + b$ can be written as:
        //
        //   $A * P_{M} + \left( centroid_{S} - A * centroid_{M} \right)$.
        // 
        // Specifically, the transformed moving point cloud centroid component needs to be pre-subtracted for each
        // vector $P_{M}$ to anticipate not having an explicit centroid subtraction step prior to applying the
        // scale/rotation matrix.
        {
            Eigen::Vector3d e_centroid(centroid_m.x, centroid_m.y, centroid_m.z);
            auto A_e_centroid = A * e_centroid; 

            t.coeff(3,0) = centroid_s.x - A_e_centroid(0);
            t.coeff(3,1) = centroid_s.y - A_e_centroid(1);
            t.coeff(3,2) = centroid_s.z - A_e_centroid(2);
        }

        // Evaluate whether the current transformation is sufficient. If so, terminate the loop.
        working.points = moving.points;
        t.apply_to(working);
        double f_curr = 0.0;
        for(size_t i = 0; i < N_working_points; ++i){
            const auto w_p = working.points[i];
            const auto s_p = stationary.points[i];
            const auto dist = s_p.distance(w_p);
            f_curr += dist;
        }

        FUNCINFO("Global distance using correspondence estimated during iteration " << icp_iter << " is " << f_curr);

        if(f_curr < f_best){
            f_best = f_curr;
            t_best = t;
        }
        if( std::isfinite(f_rel_tol) 
        &&  std::isfinite(f_curr)
        &&  std::isfinite(f_prev) ){
            const auto f_rel = std::fabs( (f_prev - f_curr) / f_prev );
            FUNCINFO("The relative change in global distance compared to the last iteration is " << f_rel);
            if(f_rel < f_rel_tol) break;
        }
        f_prev = f_curr;
    }

    // Select the best transformation observed so far.
    t = t_best;

    // Report the transformation and pass it to the user.
    //FUNCINFO("Final Affine transformation:");
    //t.write_to(std::cout);
    return t;
}
#endif // DCMA_USE_EIGEN

#ifdef DCMA_USE_EIGEN
// This routine finds a non-rigid alignment using the 'robust point matching: thin plate spline' algorithm.
//
// TODO: This algorithm is a WIP!
static
std::optional<affine_transform<double>>
AlignViaTPSRPM(const point_set<double> & moving,
               const point_set<double> & stationary ){
    affine_transform<double> t;

    // Compute the centroid for both point clouds.
    const auto centroid_s = stationary.Centroid();
    const auto centroid_m = moving.Centroid();
    
    const auto tps_kernel = [](double dR) -> double {
        // 3D case.
        return dR;

        //// 2D case.
        //const auto log_dist = std::log(dist);
        //// Note: If points overlap exactly, this assumes they are actually infinitesimally separated.
        //return std::isfinite(log_dist) ? log_dist * dist * dist : 0.0;
    };

    // Find the largest 'square distance' between (all) points and the average separation of nearest-neighbour points
    // (in the moving cloud). This info is needed to tune the annealing energy to ensure (1) deformations can initially
    // 'reach' across the point cloud, and (2) deformations are not considered much below the nearest-neighbour spacing
    // (i.e., overfitting).

    const auto N_moving_points = moving.points.size();
    const auto N_stationary_points = stationary.points.size();
    double mean_nn_sq_dist = std::numeric_limits<double>::quiet_NaN();
    double max_sq_dist = 0.0;
    {
        FUNCINFO("Locating mean nearest-neighbour separation in moving point cloud");
        Stats::Running_Sum<double> rs;
        {
            //asio_thread_pool tp;
            for(size_t i = 0; i < N_moving_points; ++i){
                //tp.submit_task([&,i](void) -> void {
                double min_sq_dist = std::numeric_limits<double>::infinity();
                for(size_t j = 0; j < N_moving_points; ++j){
                    if(i == j) continue;
                    const auto sq_dist = (moving.points[i]).sq_dist( moving.points[j] );
                    if(sq_dist < min_sq_dist) min_sq_dist = sq_dist;
                }
                if(!std::isfinite(min_sq_dist)){
                    throw std::runtime_error("Unable to estimate nearest neighbour distance.");
                }
                rs.Digest(min_sq_dist);
                //}); // thread pool task closure.
            }
        }
        mean_nn_sq_dist = rs.Current_Sum() / static_cast<double>( N_moving_points );

        FUNCINFO("Locating max square-distance between all points");
        {
            //asio_thread_pool tp;
            //std::mutex saver_printer;
            for(size_t i = 0; i < (N_moving_points + N_stationary_points); ++i){
                //tp.submit_task([&,i](void) -> void {
                    for(size_t j = 0; j < i; ++j){
                        const auto A = (i < N_moving_points) ? moving.points[i] : stationary.points[i - N_moving_points];
                        const auto B = (j < N_moving_points) ? moving.points[j] : stationary.points[j - N_moving_points];
                        const auto sq_dist = A.sq_dist(B);
                        if(max_sq_dist < sq_dist){
                            //std::lock_guard<std::mutex> lock(saver_printer);
                            max_sq_dist = sq_dist;
                        }
                    }
                //}); // thread pool task closure.
            }
        } // Wait until all threads are done.
    }

    // Estimate determinstic annealing parameters.
    const double T_step = 0.93; // Should be [0.9:0.99] or so.
    const double T_start = 1.05 * max_sq_dist; // Slightly larger than all possible to allow any pairing.
    const double T_end = 0.01 * mean_nn_sq_dist;
    const double L_1_start = 1.0;
    const double L_2_start = 0.01 * L_1_start;

    FUNCINFO("T_start, T_step, and T_end are " << T_start << ", " << T_step << ", " << T_end);

    // Prepare working buffers.
    const auto M_N_rows = N_moving_points + 1;
    const auto M_N_cols = N_stationary_points + 1;
    Eigen::MatrixXd M(M_N_rows, M_N_cols); // The correspondence matrix.

    Eigen::MatrixXd X(N_moving_points, 4); // Stacked matrix of homogeneous moving set points.
    Eigen::MatrixXd Z(N_moving_points, 4); // Stacked matrix of corresponding homogeneous fixed set points.
    Eigen::MatrixXd k(1, N_moving_points); // TPS kernel vector.
    Eigen::MatrixXd K(N_moving_points, N_moving_points); // TPS kernel matrix.

    // Populate static elements.
    for(size_t i = 0; i < N_moving_points; ++i){
        const auto P_moving = moving.points[i];
        X(i, 0) = P_moving.x;
        X(i, 1) = P_moving.y;
        X(i, 2) = P_moving.z;
        X(i, 3) = 1.0;
    }
    for(size_t i = 0; i < N_moving_points; ++i) K(i, i) = 0.0;
    for(size_t i = 0; i < N_moving_points; ++i){
        const auto P_i = moving.points[i];
        for(size_t j = i + 1; j < N_moving_points; ++j){
            const auto P_j = moving.points[j];
            const auto dist = P_i.distance(P_j);
            const auto kij = tps_kernel(dist);
            K(i, j) = kij;
            K(j, i) = kij;
        }
    }

    // QR decompose X.
    Eigen::HouseholderQR<Eigen::MatrixXd> QR(X);
    Eigen::MatrixXd Q = QR.householderQ();
    Eigen::MatrixXd R_whole = QR.matrixQR().triangularView<Eigen::Upper>();
    Eigen::MatrixXd R = R_whole.block(0, 0, 4, 4);

    Eigen::MatrixXd Q1 = Q.block(0, 0, N_moving_points, 4);
    Eigen::MatrixXd Q2 = Q.block(0, 4, N_moving_points, N_moving_points - 4);
    Eigen::MatrixXd I  = Eigen::MatrixXd::Identity(N_moving_points - 4,N_moving_points - 4);

    // TPS model parameters.
    //
    // Note: These get updated during the transformation update phase.
    Eigen::MatrixXd a_0(4, 4); // Affine transformation component.
    Eigen::MatrixXd w_0(N_moving_points, 4); // Non-Affine warping component.

    // Prime the transformation with an identity Affine component and no warp.
    //
    // Note that the RPM-TPS method gradually progresses from global to local transformations, so if the initial
    // temperature is sufficiently high then something like centroid-matching and PCA-alignment will naturally occur.
    // Conversely, if the temperature is set below the threshold required for global transformations, then only local
    // transformations (waprs) will occur; this may be what the user intends!
    a_0 = Eigen::MatrixXd::Identity(4, 4);
    w_0 = Eigen::MatrixXd::Zero(N_moving_points, 4);

    // Prime the correspondence matrix with uniform correspondence terms.
    for(size_t i = 0; i < N_moving_points; ++i){ // row
        for(size_t j = 0; j < N_stationary_points; ++j){ // column
            M(i, j) = 1.0 / static_cast<double>(N_moving_points);
        }
    }
    {
        const auto i = N_moving_points; // row
        for(size_t j = 0; j < N_stationary_points; ++j){ // column
            M(i, j) = 0.01 / static_cast<double>(N_moving_points);
        }
    }
    for(size_t i = 0; i < N_moving_points; ++i){ // row
        const auto j = N_stationary_points; // column
        M(i, j) = 0.01 / static_cast<double>(N_moving_points);
    }
    M(N_moving_points, N_stationary_points) = 0.0;

    // Implement the thin-plate spline (TPS) warping function.
    const auto f_tps = [&](const vec3<double> &v) -> vec3<double> {
        // Update kernel vector.
        for(size_t i = 0; i < N_moving_points; ++i){
            const auto P_i = moving.points[i];
            const auto dist = P_i.distance(v);
            const auto ki = tps_kernel(dist);
            k(0, i) = ki;
        }
        
        Eigen::MatrixXd x(1,4);
        x(0,0) = v.x;
        x(0,1) = v.y;
        x(0,2) = v.z;
        x(0,3) = 1.0;

        Eigen::MatrixXd f = (x * a_0) + (k * w_0);

        //if(v == moving.points[0]){
        //    FUNCINFO("For " << v << " predicted f is " << f);
        //}

        // Convert back from homogeneous coordinates.
        return vec3<double>( f(0,0) / f(0,3),
                             f(0,1) / f(0,3),
                             f(0,2) / f(0,3) );
    };



    // Update the correspondence.
    //
    // Note: This sub-routine solves for the point cloud correspondence using the current TPS transformation.
    //
    // Note: This sub-routine implements a 'soft-assign' technique for evaluating the correspondence.
    //       It supports outliers in either point cloud set.
    const auto update_correspondence = [&](double T_now, double s_reg) -> void {
        // Non-outlier coefficients.
        for(size_t i = 0; i < N_moving_points; ++i){ // row
            const auto P_moving = moving.points[i];
            const auto P_moved = f_tps(P_moving); // Transform the point.
            for(size_t j = 0; j < N_stationary_points; ++j){ // column
                const auto P_stationary = stationary.points[j];
                const auto dP = P_stationary - P_moved;
                //M(i, j) = std::exp(-dP.Dot(dP) / (2.0 * T_now)) / T_now;
                M(i, j) = std::sqrt( 1.0 / T_now ) * std::exp( (s_reg / T_now) - dP.Dot(dP) / (2.0 * T_now ) );
            }
        }

        // Moving outlier coefficients.
        {
            const auto i = N_moving_points; // row
            const auto P_moving = moving.points[i];
            for(size_t j = 0; j < N_stationary_points; ++j){ // column
                const auto P_stationary = stationary.points[j];
                const auto dP = P_stationary - P_moving; // Note: intentionally not transformed.
                //M(i, j) = std::exp(-dP.Dot(dP) / T_start) / T_start; // Note: always use initial start temperature.
                M(i, j) = std::sqrt( 1.0 / T_start ) * std::exp( - dP.Dot(dP) / (2.0 * T_start ) );
            }
        }

        // Stationary outlier coefficients.
        for(size_t i = 0; i < N_moving_points; ++i){ // row
            const auto P_moving = moving.points[i];
            const auto P_moved = f_tps(P_moving); // Transform the point.
            const auto j = N_stationary_points; // column
            const auto P_stationary = stationary.points[j];
            const auto dP = P_stationary - P_moved;
            //M(i, j) = std::exp(-dP.Dot(dP) / T_start) / T_start; // Note: always use initial start temperature.
            M(i, j) = std::sqrt( 1.0 / T_start ) * std::exp( - dP.Dot(dP) / (2.0 * T_start ) );
        }

        // Normalize the rows and columns iteratively.
        {
            std::vector<double> row_sums(N_moving_points+1, 0.0);
            std::vector<double> col_sums(N_stationary_points+1, 0.0);
            for(size_t norm_iter = 0; norm_iter < 10; ++norm_iter){

                // Tally the current column sums and re-scale the corespondence coefficients.
                for(size_t j = 0; j < (N_stationary_points+1); ++j){ // column
                    col_sums[j] = 0.0;
                    for(size_t i = 0; i < (N_moving_points+1); ++i){ // row
                        col_sums[j] += M(i,j);
                    }
                }
                for(size_t j = 0; j < (N_stationary_points+1); ++j){ // column
                    if(col_sums[j] < 1.0E-5){ // If too far away, nominate this point as an outlier.
                        col_sums[j] += 1.0;
                        M(N_moving_points,j) += 1.0;
                    }
                    for(size_t i = 0; i < (N_moving_points+1); ++i){ // row
                        M(i,j) = M(i,j) / col_sums[j];
                    }
                }
                
                // Tally the current row sums and re-scale the corespondence coefficients.
                for(size_t i = 0; i < (N_moving_points+1); ++i){ // row
                    row_sums[i] = 0.0;
                    for(size_t j = 0; j < (N_stationary_points+1); ++j){ // column
                        row_sums[i] += M(i,j);
                    }
                }
                for(size_t i = 0; i < (N_moving_points+1); ++i){ // row
                    if(row_sums[i] < 1.0E-5){ // If too far away, nominate this point as an outlier.
                        row_sums[i] += 1.0;
                        M(i,N_stationary_points) += 1.0;
                    }
                    for(size_t j = 0; j < (N_stationary_points+1); ++j){ // column
                        M(i,j) = M(i,j) / row_sums[i];
                    }
                }

                //FUNCINFO("On normalization iteration " << norm_iter << " the mean col sum was " << Stats::Mean(col_sums));
                //FUNCINFO("On normalization iteration " << norm_iter << " the mean row sum was " << Stats::Mean(row_sums));
            }
        }

        if(!M.allFinite()){
            throw std::runtime_error("Failed to compute coefficient matrix.");
        }

        return;
    };

    // Update the transformation.
    //
    // Note: This sub-routine solves for the TPS solution using the current correspondence.
    const auto update_transformation = [&](double T_now, double lambda) -> void {
        const auto softassign_outlier_min = 1.0E-4;

        // Update the elements that depend on the correspondence.
        for(size_t i = 0; i < N_moving_points; ++i){
            // Only needed for 'Double-sided outlier handling' approach from Yang et al (2011).
            //const auto col_sum = std::max(M.row(i).sum(), softassign_outlier_min);

            vec3<double> z(0.0, 0.0, 0.0);
            for(size_t j = 0; j < N_stationary_points; ++j){ // column
                const auto P_stationary = stationary.points[j];

                // Original formulation from Chui and Rangaran.
                const auto weight = M(i,j);

                // 'Double-sided outlier handling' approach from Yang et al (2011).
                //const auto weight = M(i,j) / col_sum;

                z += P_stationary * weight;
            }
            Z(i, 0) = z.x;
            Z(i, 1) = z.y;
            Z(i, 2) = z.z;
            Z(i, 3) = 1.0;
        }

        // Original paper and Chui's thesis version as written.
        //
        // Note: this implementation is numerically unstable!
        //Eigen::MatrixXd inner_prod = (Q2.transpose() * K * Q2 + I * lambda).inverse();
        //w_0 = Q2 * inner_prod * Q2.transpose() * Z;
        //a_0 = R.inverse() * Q1.transpose() * (Z - K * w_0);

//        // Same as Chui's original version, but using more numerically stable LDLT decomposition approach.
//        const auto N_pts = static_cast<double>(N_moving_points);
//        
//        Eigen::MatrixXd LHS = (Q2.transpose() * K * Q2 + I * lambda * N_pts);
//        //Eigen::MatrixXd LHS = (Q2.transpose() * K * Q2 + I * lambda);
//        
//        Eigen::LDLT<Eigen::MatrixXd> s;
//        s.compute(LHS.transpose() * LHS);
//        if(s.info() != Eigen::Success){
//            throw std::runtime_error("Unable to update transformation: Cholesky decomposition A failed.");
//        }
//        
//        Eigen::MatrixXd g = s.solve(LHS.transpose() * Q2.transpose() * Z);
//        if(s.info() != Eigen::Success){
//            throw std::runtime_error("Unable to update transformation: Cholesky solve A failed.");
//        }
//        w_0 = Q2 * g;
//        
//        s.compute(R.transpose() * R);
//        if(s.info() != Eigen::Success){
//            throw std::runtime_error("Unable to update transformation: Cholesky decomposition B failed.");
//        }
//        
//        a_0 = s.solve(LHS.transpose() * Q1.transpose() * (Z - K * w_0));
//        if(s.info() != Eigen::Success){
//            throw std::runtime_error("Unable to update transformation: Cholesky solve B failed.");
//        }

        // More stable LDLT decomposition approach where the Affine component is also regularized to suppress mirroring.
        const auto N_pts = static_cast<double>(N_moving_points);
        {
            Eigen::MatrixXd LHS = (Q2.transpose() * K * Q2 + I * lambda);
        
            Eigen::LDLT<Eigen::MatrixXd> s;
            s.compute(LHS.transpose() * LHS);
            if(s.info() != Eigen::Success){
                throw std::runtime_error("Unable to update transformation: Cholesky decomposition A failed.");
            }
        
            w_0 = Q2 * s.solve(LHS.transpose() * Q2.transpose() * Z);
            if(s.info() != Eigen::Success){
                throw std::runtime_error("Unable to update transformation: Cholesky solve A failed.");
            }
        }
        
        {
            const double lambda_d = N_pts * lambda * 1E-4; // <--- Magic factor! Tweaking may be necessary...
            Eigen::MatrixXd LHS(4 * 2, 4);
            LHS << R,
                   ( Eigen::MatrixXd::Identity(4, 4) * lambda_d );
        
            Eigen::LDLT<Eigen::MatrixXd> s;
            s.compute(LHS.transpose() * LHS);
            if(s.info() != Eigen::Success){
                throw std::runtime_error("Unable to update transformation: Cholesky decomposition B failed.");
            }
        
            Eigen::MatrixXd RHS(4 * 2, 4);
            RHS << Q1.transpose() * (Z - K * w_0),
                   ( Eigen::MatrixXd::Identity(4, 4) * lambda_d );
        
            a_0 = s.solve(LHS.transpose() * RHS);
            if(s.info() != Eigen::Success){
                throw std::runtime_error("Unable to update transformation: Cholesky solve B failed.");
            }
        }

//        // 'Double-sided outlier handling' as outlined by Yang et al in 2011.
//        // It seems to be even less stable than the above, but perhaps I've implemented it wrong?
//        const auto N_stat_pts = static_cast<double>(N_stationary_points);
//
//        {
//            Eigen::MatrixXd W = Eigen::MatrixXd::Zero(N_moving_points, N_moving_points);
//            for(size_t i = 0; i < N_moving_points; ++i){
//                W(i, i) = 1.0 / std::max(M.row(i).sum(), softassign_outlier_min);
//            }
//
//            Eigen::MatrixXd inner = K + (W * lambda * N_stat_pts);
//            Eigen::MatrixXd LHS = Q2.transpose() * inner * Q2;
//
//            Eigen::LDLT<Eigen::MatrixXd> s;
//            s.compute(LHS.transpose() * LHS);
//            if(s.info() != Eigen::Success){
//                throw std::runtime_error("Unable to update transformation: Cholesky decomposition A failed.");
//            }
//
//            w_0 = Q2 * s.solve(LHS.transpose() * Q2.transpose() * Z);
//            if(s.info() != Eigen::Success){
//                throw std::runtime_error("Unable to update transformation: Cholesky solve A failed.");
//            }
//
//            const double lambda_d = N_stat_pts * lambda * 1E-4; // <--- Magic factor! Tweaking may be necessary...
//            Eigen::MatrixXd LHS2(4 * 2, 4);
//            LHS2 << R,
//                    ( Eigen::MatrixXd::Identity(4, 4) * lambda_d );
//
//            s.compute(LHS2.transpose() * LHS2);
//            if(s.info() != Eigen::Success){
//                throw std::runtime_error("Unable to update transformation: Cholesky decomposition B failed.");
//            }
//
//            Eigen::MatrixXd RHS(4 * 2, 4);
//            RHS << Q1.transpose() * (Z - inner * w_0),
//                   ( Eigen::MatrixXd::Identity(4, 4) * lambda_d );
//
//            a_0 = s.solve(LHS2.transpose() * RHS);
//            if(s.info() != Eigen::Success){
//                throw std::runtime_error("Unable to update transformation: Cholesky solve B failed.");
//            }
//        }

        if(!w_0.allFinite() || !a_0.allFinite()){
            throw std::runtime_error("Failed to compute transformation.");
        }

        //FUNCINFO("x   = " << x);
        //FUNCINFO("w_0 = " << w_0);
        //FUNCINFO("a_0 = " << a_0);
        return;
    };

    // Print information about the optimization.
    const auto print_optimizer_progress = [&](double T_now, double lambda) -> void {

        // Correspondence coefficients.
        //
        // These will approach a binary state (min=0 and max=1) when the temperature is low.
        // Whether these are binary or not fully depends on the temperature, so they can be used to tweak the annealing
        // schedule.
        const auto mean_row_min_coeff = M.rowwise().minCoeff().sum() / static_cast<double>( M.rows() );
        const auto mean_row_max_coeff = M.rowwise().maxCoeff().sum() / static_cast<double>( M.rows() );

        // Warp component bending energy.
        const auto E_bending = std::abs( (w_0 * Z.transpose()).trace() * lambda );

        // Mean error, assuming the current correspondence is both binary and optimal (neither are likely to be true!).
        const auto mean_error = (X - Z).rowwise().norm().sum() / static_cast<double>(X.rows());

        FUNCINFO("Optimizer state: T = " << std::setw(12) << T_now 
                   << ", E_bending = " << std::setw(12) << E_bending 
                   << ", mean err = " << std::setw(12) << mean_error
                   << ", mean min,max corr coeffs = " << std::setw(12) << mean_row_min_coeff
                   << ", " << std::setw(12) << mean_row_max_coeff );
        //FUNCINFO("x   = " << x);
        //FUNCINFO("w_0 = " << w_0);
        //FUNCINFO("a_0 = " << a_0);
        return;
    };

    const auto write_to_xyz_file = [&](const std::string &base){
        const auto fname = Get_Unique_Sequential_Filename(base, 6, ".xyz");

        std::ofstream of(fname);
        for(size_t i = 0; i < N_moving_points; ++i){
            const auto P_moving = moving.points[i];
            const auto P_moved = f_tps(P_moving);
            of << P_moved.x << " " << P_moved.y << " " << P_moved.z << std::endl;
        }
        return;
    };

    // Anneal deterministically.
    for(double T_now = T_start; T_now >= T_end; T_now *= T_step){
        // Regularization parameter: controls how smooth the TPS interpolation is.
        const double L_1 = T_now * L_1_start;

        const double L_2 = T_now * L_2_start;
        const size_t N_iters_at_fixed_T = 5;

        // Regularization parameter: controls bias toward declaring a point an outlier. Chui and Rangarajan recommend
        // setting it "close to zero."
        const double smoothness_regularization = 0.1 * T_now;
        //const double smoothness_regularization = L_2;

        for(size_t iter_at_fixed_T = 0; iter_at_fixed_T < N_iters_at_fixed_T; ++iter_at_fixed_T){

            // Update correspondence matrix.
            update_correspondence(T_now, smoothness_regularization);

            // Update transformation.
            update_transformation(T_now, L_1);
        }

        print_optimizer_progress(T_now, L_1);

        write_to_xyz_file("warped_tps-rpm_");
    }
    write_to_xyz_file("warped_tps-rpm_");

    return t;
}
#endif // DCMA_USE_EIGEN


OperationDoc OpArgDocAlignPoints(void){
    OperationDoc out;
    out.name = "AlignPoints";

    out.desc = 
        "This operation aligns (i.e., 'registers') a 'moving' point cloud to a 'stationary' (i.e., 'reference') point cloud.";
        
    out.notes.emplace_back(
        "The 'moving' point cloud is transformed after the final transformation has been estimated."
        " It should be copied if a pre-transformed copy is required."
    );
        
#ifdef DCMA_USE_EIGEN
#else
    out.notes.emplace_back(
        "Functionality provided by Eigen has been disabled. The available transformation methods have been reduced."
    );
#endif

    out.args.emplace_back();
    out.args.back() = PCWhitelistOpArgDoc();
    out.args.back().name = "MovingPointSelection";
    out.args.back().default_val = "last";
    out.args.back().desc = "The point cloud that will be transformed. "_s
                         + out.args.back().desc;


    out.args.emplace_back();
    out.args.back() = PCWhitelistOpArgDoc();
    out.args.back().name = "ReferencePointSelection";
    out.args.back().default_val = "last";
    out.args.back().desc = "The stationary point cloud to use as a reference for the moving point cloud. "_s
                         + out.args.back().desc
                         + " Note that this point cloud is not modified.";


    out.args.emplace_back();
    out.args.back().name = "Method";
    out.args.back().desc = "The alignment algorithm to use."
                           " The following alignment options are available: 'centroid'"
#ifdef DCMA_USE_EIGEN
                           ", 'PCA', and 'exhaustive_icp'"
#endif
                           "."
                           " The 'centroid' option finds a rotationless translation the aligns the centroid"
                           " (i.e., the centre of mass if every point has the same 'mass')"
                           " of the moving point cloud with that of the stationary point cloud."
                           " It is susceptible to noise and outliers, and can only be reliably used when the point"
                           " cloud has complete rotational symmetry (i.e., a sphere). On the other hand, 'centroid'"
                           " alignment should never fail, can handle a large number of points,"
                           " and can be used in cases of 2D and 1D degeneracy."
                           " centroid alignment is frequently used as a pre-processing step for more advanced algorithms."
                           ""
#ifdef DCMA_USE_EIGEN
                           " The 'PCA' option finds an Affine transformation by performing centroid alignment,"
                           " performing principle component analysis (PCA) separately on the reference and moving"
                           " point clouds, computing third-order point distribution moments along each principle axis"
                           " to establish a consistent orientation,"
                           " and then rotates the moving point cloud so the principle axes of the stationary and"
                           " moving point clouds coincide."
                           " The 'PCA' method may be suitable when: (1) both clouds are not contaminated with extra"
                           " noise points (but some Gaussian noise in the form of point 'jitter' should be tolerated)"
                           " and (2) the clouds are not perfectly spherical (i.e., so they have valid principle"
                           " components)."
                           " However, note that the 'PCA' method is susceptible to outliers and can not scale"
                           " a point cloud."
                           " The 'PCA' method will generally fail when the distribution of points shifts across the"
                           " centroid (i.e., comparing reference and moving point clouds) since the orientation of"
                           " the components will be inverted, however 2D degeneracy is handled in a 3D-consistent way,"
                           " and 1D degeneracy is handled in a 1D-consistent way (i.e, the components orthogonal to"
                           " the common line will be completely ambiguous, so spurious rotations will result)."
                           ""
                           " The 'exhaustive_icp' option finds an Affine transformation by first performing PCA-based"
                           " alignment and then iteratively alternating between (1) estimating point-point"
                           " correspondence and (1) solving for a least-squares optimal transformation given this"
                           " correspondence estimate. 'ICP' stands for 'iterative closest point.'"
                           " Each iteration uses the previous transformation *only* to estimate correspondence;"
                           " a least-squares optimal linear transform is estimated afresh each iteration."
                           " The 'exhaustive_icp' method is most suitable when both point clouds consist of"
                           " approximately 50k points or less. Beyond this, ICP will still work but runtime"
                           " scales badly."
                           " ICP is susceptible to outliers and will not scale a point cloud."
                           " It can be used for 2D and 1D degenerate problems, but is not guaranteed to find the"
                           " 'correct' orientation of degenerate or symmetrical point clouds."
#endif
                           "";
    out.args.back().default_val = "centroid";
    out.args.back().expected = true;
#ifdef DCMA_USE_EIGEN
    out.args.back().examples = { "centroid", "pca", "exhaustive_icp" };
#else
    out.args.back().examples = { "centroid" };
#endif


    out.args.emplace_back();
    out.args.back().name = "MaxIterations";
    out.args.back().desc = "If the method is iterative, only permit this many iterations to occur."
                           " Note that this parameter will not have any effect on non-iterative methods.";
    out.args.back().default_val = "100";
    out.args.back().expected = true;
    out.args.back().examples = { "5",
                                 "20",
                                 "100",
                                 "1000" };


    out.args.emplace_back();
    out.args.back().name = "RelativeTolerance";
    out.args.back().desc = "If the method is iterative, terminate the loop when the cost function changes between"
                           " successive iterations by this amount or less."
                           " The magnitude of the cost function will generally depend on the number of points"
                           " (in both point clouds), the scale (i.e., 'width') of the point clouds, the amount"
                           " of noise and outlier points, and any method-specific"
                           " parameters that impact the cost function (if applicable);"
                           " use of this tolerance parameter may be impacted by these characteristics."
                           " Verifying that a given tolerance is of appropriate magnitude is recommended."
                           " Relative tolerance checks can be disabled by setting to non-finite or negative value."
                           " Note that this parameter will not have any effect on non-iterative methods.";
    out.args.back().default_val = "nan";
    out.args.back().expected = true;
    out.args.back().examples = { "-1",
                                 "1E-2",
                                 "1E-3",
                                 "1E-5" };


    out.args.emplace_back();
    out.args.back().name = "Filename";
    out.args.back().desc = "The filename (or full path name) to which the transformation should be written."
                           " Existing files will be overwritten."
                           " The file format is a 4x4 Affine matrix."
                           " If no name is given, a unique name will be chosen automatically.";
    out.args.back().default_val = "";
    out.args.back().expected = true;
    out.args.back().examples = { "transformation.trans",
                                 "trans.txt",
                                 "/path/to/some/trans.txt" };
    out.args.back().mimetype = "text/plain";


    return out;
}



Drover AlignPoints(Drover DICOM_data, OperationArgPkg OptArgs, std::map<std::string,std::string> /*InvocationMetadata*/, std::string FilenameLex){

    Explicator X(FilenameLex);

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto MovingPointSelectionStr = OptArgs.getValueStr("MovingPointSelection").value();
    const auto ReferencePointSelectionStr = OptArgs.getValueStr("ReferencePointSelection").value();

    const auto MethodStr = OptArgs.getValueStr("Method").value();

    const auto MaxIters = std::stol( OptArgs.getValueStr("MaxIterations").value() );
    const auto RelativeTol = std::stod( OptArgs.getValueStr("RelativeTolerance").value() );

    const auto FilenameStr = OptArgs.getValueStr("Filename").value();

    //-----------------------------------------------------------------------------------------------------------------
    const auto regex_com    = Compile_Regex("^ce?n?t?r?o?i?d?$");
#ifdef DCMA_USE_EIGEN    
    const auto regex_pca    = Compile_Regex("^pc?a?$");
    const auto regex_exhicp = Compile_Regex("^ex?h?a?u?s?t?i?v?e?[-_]?i?c?p?$");
    const auto regex_tpsrpm = Compile_Regex("^tp?s?-?r?p?m?$");
#endif // DCMA_USE_EIGEN

    auto PCs_all = All_PCs( DICOM_data );
    auto ref_PCs = Whitelist( PCs_all, ReferencePointSelectionStr );
    if(ref_PCs.size() != 1){
        throw std::invalid_argument("A single reference point cloud must be selected. Cannot continue.");
    }

    // Iterate over the moving point clouds, aligning each to the reference point cloud.
    auto moving_PCs = Whitelist( PCs_all, MovingPointSelectionStr );
    for(auto & pcp_it : moving_PCs){
        FUNCINFO("There are " << (*ref_PCs.front())->pset.points.size() << " points in the reference point cloud");
        FUNCINFO("There are " << (*pcp_it)->pset.points.size() << " points in the moving point cloud");

        // Determine which filename to use.
        auto FN = FilenameStr;
        if(FN.empty()){
            FN = Get_Unique_Sequential_Filename("/tmp/dcma_alignpoints_", 6, ".trans");
        }
        std::fstream FO(FN, std::fstream::out);

        if(false){
        }else if( std::regex_match(MethodStr, regex_com) ){
            auto t_opt = AlignViaCentroid( (*pcp_it)->pset,
                                           (*ref_PCs.front())->pset );
 
            if(t_opt){
                FUNCINFO("Transforming the point cloud using centre-of-mass alignment");
                t_opt.value().apply_to((*pcp_it)->pset);

                if(!(t_opt.value().write_to(FO))){
                    std::runtime_error("Unable to write transformation to file. Cannot continue.");
                }
            }

#ifdef DCMA_USE_EIGEN    
        }else if( std::regex_match(MethodStr, regex_pca) ){
            auto t_opt = AlignViaPCA( (*pcp_it)->pset,
                                      (*ref_PCs.front())->pset );
 
            if(t_opt){
                FUNCINFO("Transforming the point cloud using principle component alignment");
                t_opt.value().apply_to((*pcp_it)->pset);

                if(!(t_opt.value().write_to(FO))){
                    std::runtime_error("Unable to write transformation to file. Cannot continue.");
                }
            }

        }else if( std::regex_match(MethodStr, regex_exhicp) ){
            auto t_opt = AlignViaExhaustiveICP( (*pcp_it)->pset,
                                                (*ref_PCs.front())->pset,
                                                MaxIters,
                                                RelativeTol );
 
            if(t_opt){
                FUNCINFO("Transforming the point cloud using exhaustive iterative closes point alignment");
                t_opt.value().apply_to((*pcp_it)->pset);

                if(!(t_opt.value().write_to(FO))){
                    std::runtime_error("Unable to write transformation to file. Cannot continue.");
                }
            }

        }else if( std::regex_match(MethodStr, regex_tpsrpm) ){
            auto t_opt = AlignViaTPSRPM( (*pcp_it)->pset,
                                         (*ref_PCs.front())->pset );
 
            if(t_opt){
                FUNCINFO("Transforming the point cloud using TPS-RPM alignment");
                t_opt.value().apply_to((*pcp_it)->pset);

                if(!(t_opt.value().write_to(FO))){
                    std::runtime_error("Unable to write transformation to file. Cannot continue.");
                }
            }

#endif // DCMA_USE_EIGEN

        }else{
            throw std::invalid_argument("Method not understood. Cannot continue.");
        }

    } // Loop over point clouds.

    return DICOM_data;
}
