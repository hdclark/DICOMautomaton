//Alignment_TPSRPM.cc - A part of DICOMautomaton 2020. Written by hal clark.

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

#include "Structs.h"
#include "Regex_Selectors.h"
#include "Thread_Pool.h"

#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorStats.h"        //Needed for Stats:: namespace.
#include "YgorString.h"       //Needed for GetFirstRegex(...)

#include "Alignment_TPSRPM.h"


#ifdef DCMA_USE_EIGEN
// This routine finds a non-rigid alignment using the 'robust point matching: thin plate spline' algorithm.
//
// Note that this routine only identifies a transform, it does not implement it by altering the inputs.
//
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

