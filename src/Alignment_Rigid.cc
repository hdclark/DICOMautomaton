//Alignment_Rigid.cc - A part of DICOMautomaton 2020. Written by hal clark.

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

#include "Structs.h"
#include "Thread_Pool.h"

#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorLog.h"
#include "YgorStats.h"        //Needed for Stats:: namespace.
#include "YgorString.h"       //Needed for GetFirstRegex(...)

#include "Alignment_Rigid.h"


// This routine performs a simple centroid-based alignment.
//
// The resultant transformation is a rotation-less shift so the point cloud centres-of-mass overlap.
//
// Note that this routine only identifies a transform, it does not implement it by altering the inputs.
//
std::optional<affine_transform<double>>
AlignViaCentroid(const point_set<double> & moving,
                 const point_set<double> & stationary ){

    if( moving.points.empty() ){
        throw std::invalid_argument("Moving point set does not contain any points");
    }
    if( stationary.points.empty() ){
        throw std::invalid_argument("Stationary point set does not contain any points");
    }

    // Compute the centroid for both point clouds.
    const auto centroid_s = stationary.Centroid();
    const auto centroid_m = moving.Centroid();

    if( !centroid_m.isfinite() ){
        throw std::invalid_argument("Moving point set does not have a finite centroid");
    }
    if( !centroid_s.isfinite() ){
        throw std::invalid_argument("Stationary point set does not have a finite centroid");
    }

    const auto dcentroid = (centroid_s - centroid_m);
    affine_transform<double> t = affine_translate<double>(dcentroid);

    // Test if the transformation is valid.
    vec3<double> v_test(1.0, 1.0, 1.0);
    t.apply_to(v_test);
    if( !v_test.isfinite() ){
        return std::nullopt;
    }
    return t;
}
    
#ifdef DCMA_USE_EIGEN
// This routine performs a PCA-based alignment.
//
// First, the moving point cloud is translated the moving point cloud so the centre of mass aligns to the reference
// point cloud, performs PCA separately on the reference and moving point clouds, compute distribution moments along
// each axis to determine the direction, and then rotates the moving point cloud so the principle axes coincide.
//
// Note that this routine only identifies a transform, it does not implement it by altering the inputs.
//
std::optional<affine_transform<double>>
AlignViaPCA(const point_set<double> & moving,
            const point_set<double> & stationary ){
    affine_transform<double> t;

    if( moving.points.empty() ){
        throw std::invalid_argument("Moving point set does not contain any points");
    }
    if( stationary.points.empty() ){
        throw std::invalid_argument("Stationary point set does not contain any points");
    }

    // Compute the centroid for both point clouds.
    const auto centroid_s = stationary.Centroid();
    const auto centroid_m = moving.Centroid();
   
    if( !centroid_m.isfinite() ){
        throw std::invalid_argument("Moving point set does not have a finite centroid");
    }
    if( !centroid_s.isfinite() ){
        throw std::invalid_argument("Stationary point set does not have a finite centroid");
    }

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
        
        //const Eigen::VectorXd& evals = eig.eigenvalues();
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

    YLOGINFO("Stationary point cloud:");
    YLOGINFO("    centroid             : " << centroid_s);
    YLOGINFO("    pcomp_pc1            : " << pcomps_stationary.pc1);
    YLOGINFO("    pcomp_pc2            : " << pcomps_stationary.pc2);
    YLOGINFO("    pcomp_pc3            : " << pcomps_stationary.pc3);
    YLOGINFO("    reoriented_pcomp_pc1 : " << reoriented_pcomps_stationary.pc1);
    YLOGINFO("    reoriented_pcomp_pc2 : " << reoriented_pcomps_stationary.pc2);
    YLOGINFO("    reoriented_pcomp_pc3 : " << reoriented_pcomps_stationary.pc3);

    YLOGINFO("Moving point cloud:");
    YLOGINFO("    centroid             : " << centroid_m);
    YLOGINFO("    pcomp_pc1            : " << pcomps_moving.pc1);
    YLOGINFO("    pcomp_pc2            : " << pcomps_moving.pc2);
    YLOGINFO("    pcomp_pc3            : " << pcomps_moving.pc3);
    YLOGINFO("    reoriented_pcomp_pc1 : " << reoriented_pcomps_moving.pc1);
    YLOGINFO("    reoriented_pcomp_pc2 : " << reoriented_pcomps_moving.pc2);
    YLOGINFO("    reoriented_pcomp_pc3 : " << reoriented_pcomps_moving.pc3);

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
        t.coeff(1,0) = A(1,0);
        t.coeff(2,0) = A(2,0);
                   
        t.coeff(0,1) = A(0,1);
        t.coeff(1,1) = A(1,1);
        t.coeff(2,1) = A(2,1);
                   
        t.coeff(0,2) = A(0,2);
        t.coeff(1,2) = A(1,2);
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

            t.coeff(0,3) = centroid_s.x - A_e_centroid_m(0);
            t.coeff(1,3) = centroid_s.y - A_e_centroid_m(1);
            t.coeff(2,3) = centroid_s.z - A_e_centroid_m(2);
        }
    }

    //YLOGINFO("Final linear transform:");
    //YLOGINFO("    ( " << t.coeff(0,0) << "  " << t.coeff(0,1) << "  " << t.coeff(0,2) << " )");
    //YLOGINFO("    ( " << t.coeff(1,0) << "  " << t.coeff(1,1) << "  " << t.coeff(1,2) << " )");
    //YLOGINFO("    ( " << t.coeff(2,0) << "  " << t.coeff(2,1) << "  " << t.coeff(2,2) << " )");
    //YLOGINFO("Final translation:");
    //YLOGINFO("    ( " << t.coeff(0,3) << " )");
    //YLOGINFO("    ( " << t.coeff(1,3) << " )");
    //YLOGINFO("    ( " << t.coeff(2,3) << " )");
    //YLOGINFO("Final Affine transformation:");
    //t.write_to(std::cout);

    // Test if the transformation is valid.
    vec3<double> v_test(1.0, 1.0, 1.0);
    t.apply_to(v_test);
    if( !v_test.isfinite() ){
        return std::nullopt;
    }

    return t;
}
#endif // DCMA_USE_EIGEN

#ifdef DCMA_USE_EIGEN
// This routine performs an alignment using the "Orthogonal Procrustes" algorithm.
//
// This method is similar to PCA-based alignment, but singular-value decomposition (SVD) is used to estimate the best
// rotation. For more information, see the "Wahba problem" or the "Kabsch algorithm."
//
// In contrast to PCA this method disallows mirroring, making it suitable for low-density and/or symmetric point sets.
//
// Note that this routine only identifies a suitable transform, it does not implement it by altering the inputs.
//
// Moving and stationary sets must be paired and corresponding. Low-dimensional degeneracies are somewhat protected
// against, but the resulting transformation is not robust and may involve mirroring, so it is recommended to
// avoid cases with low-dimensional degeneracies.
//
std::optional<affine_transform<double>>
AlignViaOrthogonalProcrustes(AlignViaOrthogonalProcrustesParams & params,
                             const point_set<double> & moving,
                             const point_set<double> & stationary ){

    if( moving.points.empty() ){
        throw std::invalid_argument("Moving point set does not contain any points");
    }
    if( stationary.points.empty() ){
        throw std::invalid_argument("Stationary point set does not contain any points");
    }
    if( moving.points.size() != stationary.points.size() ){
        throw std::invalid_argument("Moving and stationary point sets differ in number of points");
    }

    // --- Translation ----

    // Compute the centroid for both point clouds.
    const auto centroid_s = stationary.Centroid();
    const auto centroid_m = moving.Centroid();

    if( !centroid_m.isfinite() ){
        throw std::invalid_argument("Moving point set does not have a finite centroid");
    }
    if( !centroid_s.isfinite() ){
        throw std::invalid_argument("Stationary point set does not have a finite centroid");
    }

    // --- Rotation ---

    const auto N_rows = 3ul; // Dimensions (note: this process does not generalize beyond 3).
    const auto N_cols = moving.points.size();
    Eigen::MatrixXd S(N_rows, N_cols);
    Eigen::MatrixXd M(N_rows, N_cols);
    for(size_t i = 0; i < N_cols; ++i){
        // Shift both sets so their centroids coincide with origin.
        S(0, i) = stationary.points[i].x - centroid_s.x; // The desired point location.
        S(1, i) = stationary.points[i].y - centroid_s.y;
        S(2, i) = stationary.points[i].z - centroid_s.z;

        M(0, i) = moving.points[i].x - centroid_m.x; // The actual point location.
        M(1, i) = moving.points[i].y - centroid_m.y;
        M(2, i) = moving.points[i].z - centroid_m.z;
    }
    auto ST = S.transpose();
    auto MST = M * ST;

    using SVD_t = Eigen::JacobiSVD<Eigen::MatrixXd>;
    SVD_t SVD;
    SVD.compute(MST, Eigen::ComputeFullU | Eigen::ComputeFullV );
    // TODO: how do I access the info function??
    //using SVD_base_t = Eigen::EigenBase<SVD_t>; // Needed to access status.
    //auto *SVD_base = reinterpret_cast<SVD_base_t*>(SVD.compute(MST, Eigen::ComputeFullU | Eigen::ComputeFullV ));
    //if(SVD_base->info() != Eigen::ComputationInfo::Success){
    //    throw std::runtime_error("SVD computation failed");
    //}
    auto U = SVD.matrixU();
    const auto& V = SVD.matrixV();

    // Handle mirroring.
    Eigen::Matrix3d A;
    if(params.permit_mirroring){
        // Use the SVD result directly.
        //
        // Note that spatial inversions (i.e., mirror flips) are permitted this way.
        A = V * U.transpose();
    }else{
        // Disallow spatial inversions, thus restricting solutions to rotations only.
        const auto s = std::copysign(1.0, ( V * U.transpose() ).determinant() );
        Eigen::Matrix3d PI;
        PI << 1.0 , 0.0 , 0.0
            , 0.0 , 1.0 , 0.0
            , 0.0 , 0.0 , s;
        A = V * PI * U.transpose();
    }

    // Handle isotropic scaling.
    //
    // Note that this scale factor is appropriate regardless of how the transformation is determined.
    //
    // See Section 3.3 (page 25) of Gower and Dijksterhuis's "Procrustes problems." Vol. 30. OUP Oxford, 2004.
    if(params.permit_isotropic_scaling){
        const auto numer = ( S.transpose() * (A * M) ).trace();
        const auto denom = ( (A * M).transpose() * (A * M) ).trace();
        const auto s = numer / denom;
        YLOGINFO("Isotropic scale factor: " << s);
        if(!std::isfinite(s)){
            throw std::runtime_error("Isotropic scaling factor is not finite");
        }
        A *= s;
    }

    // --- Combine translation and rotation ---

    // Transfer the transformation into a full Affine transformation.
    affine_transform<double> t;

    // Rotation and scaling components.
    t.coeff(0,0) = A(0,0);
    t.coeff(1,0) = A(1,0);
    t.coeff(2,0) = A(2,0);
               
    t.coeff(0,1) = A(0,1);
    t.coeff(1,1) = A(1,1);
    t.coeff(2,1) = A(2,1);
               
    t.coeff(0,2) = A(0,2);
    t.coeff(1,2) = A(1,2);
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

        t.coeff(0,3) = centroid_s.x - A_e_centroid(0);
        t.coeff(1,3) = centroid_s.y - A_e_centroid(1);
        t.coeff(2,3) = centroid_s.z - A_e_centroid(2);
    }

    YLOGINFO("Final linear transform:");
    YLOGINFO("    ( " << t.coeff(0,0) << "  " << t.coeff(0,1) << "  " << t.coeff(0,2) << " )");
    YLOGINFO("    ( " << t.coeff(1,0) << "  " << t.coeff(1,1) << "  " << t.coeff(1,2) << " )");
    YLOGINFO("    ( " << t.coeff(2,0) << "  " << t.coeff(2,1) << "  " << t.coeff(2,2) << " )");
    YLOGINFO("Final translation:");
    YLOGINFO("    ( " << t.coeff(0,3) << " )");
    YLOGINFO("    ( " << t.coeff(1,3) << " )");
    YLOGINFO("    ( " << t.coeff(2,3) << " )");
    //YLOGINFO("Final Affine transformation:");
    //t.write_to(std::cout);

    // Test if the transformation is valid.
    vec3<double> v_test(1.0, 1.0, 1.0);
    t.apply_to(v_test);
    if( !v_test.isfinite() ){
        return std::nullopt;
    }

    return t;

}
#endif // DCMA_USE_EIGEN


#ifdef DCMA_USE_EIGEN
// This routine performs an exhaustive iterative closest point (ICP) alignment.
//
// Note that this routine only identifies a transform, it does not implement it by altering the inputs.
//
std::optional<affine_transform<double>>
AlignViaExhaustiveICP( const point_set<double> & moving,
                       const point_set<double> & stationary,
                       long int max_icp_iters,
                       double f_rel_tol ){

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
            work_queue<std::function<void(void)>> wq;
            for(size_t i = 0; i < N_working_points; ++i){
                wq.submit_task([&,i]() -> void {
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
        const auto& V = SVD.matrixV();

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
        t.coeff(1,0) = A(1,0);
        t.coeff(2,0) = A(2,0);
                   
        t.coeff(0,1) = A(0,1);
        t.coeff(1,1) = A(1,1);
        t.coeff(2,1) = A(2,1);
                   
        t.coeff(0,2) = A(0,2);
        t.coeff(1,2) = A(1,2);
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

            t.coeff(0,3) = centroid_s.x - A_e_centroid(0);
            t.coeff(1,3) = centroid_s.y - A_e_centroid(1);
            t.coeff(2,3) = centroid_s.z - A_e_centroid(2);
        }

        // Evaluate whether the current transformation is sufficient. If so, terminate the loop.
        working.points = moving.points;
        t.apply_to(working);
        double f_curr = 0.0;
        for(size_t i = 0; i < N_working_points; ++i){
            const auto w_p = working.points[i];
            const auto c_p = corresp.points[i];
            const auto dist = c_p.distance(w_p);
            f_curr += dist;
        }

        YLOGINFO("Global distance using correspondence estimated during iteration " << icp_iter << " is " << f_curr);

        if(f_curr < f_best){
            f_best = f_curr;
            t_best = t;
        }
        if( std::isfinite(f_rel_tol) 
        &&  std::isfinite(f_curr)
        &&  std::isfinite(f_prev) ){
            const auto f_rel = std::fabs( (f_prev - f_curr) / f_prev );
            YLOGINFO("The relative change in global distance compared to the last iteration is " << f_rel);
            if(f_rel < f_rel_tol) break;
        }
        f_prev = f_curr;
    }

    // Select the best transformation observed so far.
    t = t_best;

    // Test if the transformation is valid.
    vec3<double> v_test(1.0, 1.0, 1.0);
    t.apply_to(v_test);
    if( !v_test.isfinite() ){
        return std::nullopt;
    }

    // Report the transformation and pass it to the user.
    //YLOGINFO("Final Affine transformation:");
    //t.write_to(std::cout);
    return t;
}
#endif // DCMA_USE_EIGEN


