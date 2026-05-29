//Alignment_TPSRPM.cc - A part of DICOMautomaton 2020. Written by hal clark.

#include <algorithm>
#include <chrono>
#include <optional>
#include <fstream>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <regex>
#include <set> 
#include <sstream>
#include <stdexcept>
#include <string>    
#include <utility>            //Needed for std::pair.
#include <vector>
#include <iomanip>
#include <cstdint>

#ifdef DCMA_USE_EIGEN    
    #include <eigen3/Eigen/Dense>
    #include <eigen3/Eigen/Eigenvalues>
    #include <eigen3/Eigen/SVD>
    #include <eigen3/Eigen/QR>
    #include <eigen3/Eigen/Cholesky>
#endif

#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorLog.h"
#include "YgorStats.h"        //Needed for Stats:: namespace.
#include "YgorString.h"       //Needed for GetFirstRegex(...)

#include "Structs.h"
#include "Regex_Selectors.h"
#include "Thread_Pool.h"

#include "Alignment_Rigid.h"
#include "Alignment_TPSRPM.h"

thin_plate_spline::thin_plate_spline(std::istream &is){
    if(!this->read_from(is)){
        throw std::invalid_argument("Input not understood, refusing to contruct empty TPS");
    }
}

thin_plate_spline::thin_plate_spline(const point_set<double> &ps,
                                     int64_t k_dim){
    const auto N = ps.points.size();
    this->control_points = ps;
    this->kernel_dimension = k_dim;
    this->W_A = num_array<double>(N + 3 + 1, 3, 0.0); // Initialize to all zeros.

    // Default to an identity affine transform without any warp components.
    this->W_A.coeff(N + 1, 0) = 1.0; // x-component.
    this->W_A.coeff(N + 2, 1) = 1.0; // y-component.
    this->W_A.coeff(N + 3, 2) = 1.0; // z-component.
}

double
thin_plate_spline::eval_kernel(const double &dist) const {
    double out = std::numeric_limits<double>::quiet_NaN();
    if(this->kernel_dimension == 2){
        // 2D case.
        //
        // Note: this is the 2D fundamental sol'n to biharmonic equation. It seems to also work well for the 3D case,
        // even better than the actual solution in 3D. Not sure why...
        const auto log_d2 = std::log(dist * dist);
        const auto u = log_d2 * dist * dist;
        // Note: If points overlap exactly, this assumes they are actually infinitesimally separated.
        out = std::isfinite(u) ? u : 0.0;
    }else if(this->kernel_dimension == 3){
        // 3D case.
        //
        // Note: this is the 3D fundamental sol'n to biharmonic equation. It doesn't work well in practice though.
        out = dist;
    }else{
        throw std::invalid_argument("Kernel dimension not currently supported. Cannot continue.");
        // Note: if this is truly desired, the kernel for arbitrary dimensions is available. But often for $3 < D$ the
        // $D = 3$ case is used since evaluation is problematic at the control points.
    }
    return out;
}

vec3<double> 
thin_plate_spline::transform(const vec3<double> &v) const {
    const auto N = static_cast<int64_t>(this->control_points.points.size());
    Stats::Running_Sum<double> x;
    Stats::Running_Sum<double> y;
    Stats::Running_Sum<double> z;

    // affine component.
    x.Digest(W_A.read_coeff(N + 0, 0));
    x.Digest(W_A.read_coeff(N + 1, 0) * v.x);
    x.Digest(W_A.read_coeff(N + 2, 0) * v.y);
    x.Digest(W_A.read_coeff(N + 3, 0) * v.z);

    y.Digest(W_A.read_coeff(N + 0, 1));
    y.Digest(W_A.read_coeff(N + 1, 1) * v.x);
    y.Digest(W_A.read_coeff(N + 2, 1) * v.y);
    y.Digest(W_A.read_coeff(N + 3, 1) * v.z);

    z.Digest(W_A.read_coeff(N + 0, 2));
    z.Digest(W_A.read_coeff(N + 1, 2) * v.x);
    z.Digest(W_A.read_coeff(N + 2, 2) * v.y);
    z.Digest(W_A.read_coeff(N + 3, 2) * v.z);

    // Warp component.
    for(int64_t i = 0; i < N; ++i){
        const auto P_i = this->control_points.points[i];
        const auto dist = P_i.distance(v);
        const auto ki = this->eval_kernel(dist);

        x.Digest(W_A.read_coeff(i, 0) * ki);
        y.Digest(W_A.read_coeff(i, 1) * ki);
        z.Digest(W_A.read_coeff(i, 2) * ki);
    }

    const vec3<double> f_v( x.Current_Sum(),
                            y.Current_Sum(),
                            z.Current_Sum() );
    if(!f_v.isfinite()){
        throw std::runtime_error("Failed to evaluate TPS mapping function. Cannot continue.");
    }
    return f_v;
}

void
thin_plate_spline::apply_to(point_set<double> &ps) const {
    for(auto &p : ps.points){
        p = this->transform(p);
    }
    return;
}

void
thin_plate_spline::apply_to(vec3<double> &v) const {
    v = this->transform(v);
    return;
}

bool
thin_plate_spline::write_to( std::ostream &os ) const {
    // Maximize precision prior to emitting any floating-point numbers.
    const auto original_precision = os.precision();
    os.precision( std::numeric_limits<double>::max_digits10 );

    os << this->control_points.points.size() << std::endl; 
    for(const auto &p : this->control_points.points){
        os << p << std::endl;
    }
    
    os << this->kernel_dimension << std::endl; 

    this->W_A.write_to(os);

    os.precision( original_precision );
    os.flush();
    return (!os.fail());
}

bool
thin_plate_spline::read_from( std::istream &is ){
    int64_t N_control_points = 0;
    is >> N_control_points;
    if( is.fail()
    ||  !isininc(1,N_control_points,1'000'000'000) ){
        YLOGWARN("Number of control points could not be read, or is invalid.");
        return false;
    }
    this->control_points.points.resize(N_control_points);

    try{
        for(int64_t i = 0; i < N_control_points; ++i){
            is >> this->control_points.points[i];
        }
    }catch(const std::exception &e){
        YLOGWARN("Failed to read control points: " << e.what());
        return false;
    }

    is >> this->kernel_dimension;
    if( is.fail()
    ||  !isininc(2,this->kernel_dimension,3) ){
        YLOGWARN("Kernel dimension could not be read, or is invalid.");
        return false;
    }

    if(!(this->W_A.read_from(is))){
        YLOGWARN("Transformation coefficients could not be read or are invalid.");
        return false;
    }

    if( (this->W_A.num_rows() != (N_control_points + 3 + 1))
    ||  (this->W_A.num_cols() != 3) ){
        YLOGWARN("Transformation coefficient matrix has invalid dimensions.");
        return false;
    }

    return (!is.fail());
}

#ifdef DCMA_USE_EIGEN
// This routine finds a non-rigid alignment using thin plate splines.
//
// Note that the point sets must be ordered and have the same number of points, and each pair (i.e., the nth moving
// point and the nth stationary point) correspond.
//
// Note that this routine only identifies a transform, it does not implement it by altering the inputs.
//
std::optional<thin_plate_spline>
AlignViaTPS(AlignViaTPSParams & params,
            const point_set<double> & moving,
            const point_set<double> & stationary ){

    const auto N_move_points = static_cast<int64_t>(moving.points.size());
    const auto N_stat_points = static_cast<int64_t>(stationary.points.size());
    if(N_move_points != N_stat_points){
        YLOGWARN("Unable to perform TPS alignment: point sets have different number of points");
        return std::nullopt;
    }

    thin_plate_spline t(moving, params.kernel_dimension);

    // Prepare working buffers.
    //
    // Main system matrix.
    Eigen::MatrixXd L = Eigen::MatrixXd::Zero(N_move_points + 4, N_move_points + 4);
    // Corresponding points working buffer.
    Eigen::MatrixXd Y = Eigen::MatrixXd::Zero(N_move_points + 4, 3); 

    // TPS model parameters.
    //
    // Will contain the 'warp' component (W) and an affine component (A) coefficients.
    // Note that, to avoid a later copy, these coefficients are directly mapped to the transform buffer.
    if(static_cast<int64_t>(t.W_A.size()) != (N_move_points + 4) * 3){
        throw std::logic_error("TPS coefficients allocated with incorrect size. Refusing to continue.");
    }
    Eigen::Map<Eigen::Matrix< double,
                              Eigen::Dynamic,
                              Eigen::Dynamic,
                              Eigen::ColMajor >> W_A(&(*(t.W_A.begin())),
                                                     N_move_points + 4,  3);
    if( (t.W_A.num_rows() != W_A.rows())
    ||  (t.W_A.num_cols() != W_A.cols()) ){
        throw std::logic_error("TPS coefficient matrix dimesions do not match. Refusing to continue.");
    }

    // Populate static elements.
    //
    // L matrix: "K" kernel part.
    //
    // Note: "K"s diagonals are later adjusted using the regularization parameter. They are set to zero initially.
    //for(int64_t i = 0; i < N_move_points; ++i) L(i, i) = params.lambda;
    for(int64_t i = 0; i < (N_move_points + 4); ++i) L(i, i) = params.lambda;
    for(int64_t i = 0; i < N_move_points; ++i){
        const auto P_i = moving.points[i];
        for(int64_t j = i + 1; j < N_move_points; ++j){
            const auto P_j = moving.points[j];
            const auto dist = P_i.distance(P_j);
            const auto kij = t.eval_kernel(dist);
            L(i, j) = kij;
            L(j, i) = kij;
        }
    }

    // L matrix: "P" and "PT" parts.
    for(int64_t i = 0; i < N_move_points; ++i){
        const auto P_moving = moving.points[i];
        L(i, N_move_points + 0) = 1.0;
        L(i, N_move_points + 1) = P_moving.x;
        L(i, N_move_points + 2) = P_moving.y;
        L(i, N_move_points + 3) = P_moving.z;

        L(N_move_points + 0, i) = 1.0;
        L(N_move_points + 1, i) = P_moving.x;
        L(N_move_points + 2, i) = P_moving.y;
        L(N_move_points + 3, i) = P_moving.z;
    }

    // Fill the Y vector with the corresponding points.
    for(int64_t j = 0; j < N_stat_points; ++j){ // column
        const auto P_stationary = stationary.points[j];
        Y(j, 0) = P_stationary.x;
        Y(j, 1) = P_stationary.y;
        Y(j, 2) = P_stationary.z;
    }

    // Use pseudo-inverse method.
    if(params.solution_method == AlignViaTPSParams::SolutionMethod::PseudoInverse){
        Eigen::MatrixXd L_pinv = L.completeOrthogonalDecomposition().pseudoInverse();

        // Update W_A.
        W_A = L_pinv * Y;

    // Use LDLT method.
    }else if(params.solution_method == AlignViaTPSParams::SolutionMethod::LDLT){
        Eigen::LDLT<Eigen::MatrixXd> LDLT;
        LDLT.compute(L.transpose() * L);
        if(LDLT.info() != Eigen::Success){
            throw std::runtime_error("Unable to update transformation: LDLT decomposition failed.");
        }
        
        W_A = LDLT.solve(L.transpose() * Y);
        if(LDLT.info() != Eigen::Success){
            throw std::runtime_error("Unable to update transformation: LDLT solve failed.");
        }
    }else{
        throw std::logic_error("Solution method not understood. Cannot continue.");
    }

    if(!W_A.allFinite()){
        YLOGWARN("Failed to solve for a finite-valued transform");
        return std::nullopt;
    }

    return t;
}
#endif // DCMA_USE_EIGEN



#ifdef DCMA_USE_EIGEN
// This routine finds a non-rigid alignment using the 'robust point matching: thin plate spline' algorithm.
//
// Note that this routine only identifies a transform, it does not implement it by altering the inputs.
//
std::optional<thin_plate_spline>
AlignViaTPSRPM(AlignViaTPSRPMParams & params,
               const point_set<double> & moving,
               const point_set<double> & stationary ){

    const auto N_move_points = static_cast<int64_t>(moving.points.size());
    const auto N_stat_points = static_cast<int64_t>(stationary.points.size());

    // Normalize both point clouds to [-1, 1] for numerical stability.
    //
    // This is critical for real-world data where coordinates can be large (e.g., hundreds of mm).
    // Without normalization, the TPS kernel values r^2*log(r^2) can become enormous, the correspondence
    // weights exp(-d^2/T) can underflow to zero, and the linear system can become ill-conditioned.
    // The algorithm operates entirely in normalized space, and the result is denormalized at the end.
    const auto centroid_moving = moving.Centroid();
    const auto centroid_stationary = stationary.Centroid();

    point_set<double> moving_n;
    point_set<double> stationary_n;
    for(const auto &p : moving.points) moving_n.points.emplace_back(p - centroid_moving);
    for(const auto &p : stationary.points) stationary_n.points.emplace_back(p - centroid_stationary);

    double norm_scale = 0.0;
    for(const auto &p : moving_n.points){
        norm_scale = std::max(norm_scale, std::max(std::abs(p.x), std::max(std::abs(p.y), std::abs(p.z))));
    }
    for(const auto &p : stationary_n.points){
        norm_scale = std::max(norm_scale, std::max(std::abs(p.x), std::max(std::abs(p.y), std::abs(p.z))));
    }
    if(norm_scale < 1e-10) norm_scale = 1.0;
    const double inv_norm_scale = 1.0 / norm_scale;
    for(auto &p : moving_n.points) p = p * inv_norm_scale;
    for(auto &p : stationary_n.points) p = p * inv_norm_scale;

    thin_plate_spline t(moving_n, params.kernel_dimension);

    // Estimate determinstic annealing parameters.
    //
    // Find the largest 'square distance' between (all) points and the average separation of nearest-neighbour points
    // (in the moving cloud). This info is needed to tune the annealing energy to ensure (1) deformations can initially
    // 'reach' across the point cloud, and (2) deformations are not considered much below the nearest-neighbour spacing
    // (i.e., overfitting).
    double mean_nn_sq_dist = std::numeric_limits<double>::quiet_NaN();
    double max_sq_dist = 0.0;
    {
        YLOGDEBUG("Locating mean nearest-neighbour separation in moving point cloud");
        Stats::Running_Sum<double> rs;
        {
            for(int64_t i = 0; i < N_move_points; ++i){
                double min_sq_dist = std::numeric_limits<double>::infinity();
                for(int64_t j = 0; j < N_move_points; ++j){
                    if(i == j) continue;
                    const auto sq_dist = (moving_n.points[i]).sq_dist( moving_n.points[j] );
                    if(sq_dist < min_sq_dist) min_sq_dist = sq_dist;
                }
                if(!std::isfinite(min_sq_dist)){
                    throw std::runtime_error("Unable to estimate nearest neighbour distance.");
                }
                rs.Digest(min_sq_dist);
            }
        }
        mean_nn_sq_dist = rs.Current_Sum() / static_cast<double>( N_move_points );

        YLOGDEBUG("Locating max square-distance between all points");
        {
            for(int64_t i = 0; i < (N_move_points + N_stat_points); ++i){
                for(int64_t j = 0; j < i; ++j){
                    const auto A = (i < N_move_points) ? moving_n.points[i] : stationary_n.points[i - N_move_points];
                    const auto B = (j < N_move_points) ? moving_n.points[j] : stationary_n.points[j - N_move_points];
                    const auto sq_dist = A.sq_dist(B);
                    if(max_sq_dist < sq_dist){
                        //std::lock_guard<std::mutex> lock(saver_printer);
                        max_sq_dist = sq_dist;
                    }
                }
            }
        } // Wait until all threads are done.
    }

    const double T_start = params.T_start_scale * max_sq_dist;
    const double T_end = params.T_end_scale * mean_nn_sq_dist;
    const double L_1_start = params.lambda_start * std::sqrt( mean_nn_sq_dist );
    const double L_2_start = params.zeta_start * L_1_start;

    if(!isininc(0.00001, params.T_step, 0.99999)){
        throw std::invalid_argument("Temperature step parameter is invalid. Cannot continue.");
    }
    if( (std::abs(T_start) == 0.0)
    ||  (std::abs(T_end) == 0.0)
    ||  (T_start <= T_end) ){
        throw std::invalid_argument("Start or end temperatures are invalid. Cannot continue.");
    }
    if( (L_1_start < 0.0)
    ||  (L_2_start < 0.0) ){
        throw std::invalid_argument("Regularization parameters are invalid. Cannot continue.");
    }
    if( !std::isfinite(params.row_sum_weight_threshold)
    ||  (params.row_sum_weight_threshold <= 0.0) ){
        throw std::invalid_argument("Row-sum weight threshold parameter is invalid. Cannot continue.");
    }
    YLOGDEBUG("T_start, T_step, and T_end are " << T_start << ", " << params.T_step << ", " << T_end);

    // Ensure any forced correpondences are valid and unique.
    {
        std::set<int64_t> s_m;
        std::set<int64_t> s_s;
        for(const auto &apair : params.forced_correspondence){
            const auto i_m = apair.first;
            const auto j_s = apair.second;
 
            const auto i_is_valid = isininc(0, i_m, N_move_points - 1);
            const auto j_is_valid = isininc(0, j_s, N_stat_points - 1);

            if( !i_is_valid && !j_is_valid ){
                throw std::invalid_argument("Forced contains a double-outlier constraint. Cannot continue.");
            }
            if( i_is_valid ){
                const auto ret_pair_m = s_m.insert(i_m);
                if( !ret_pair_m.second ){
                    throw std::invalid_argument("Forced correspondence contains same moving set point multiple times. Cannot continue.");
                }
            }
            if( j_is_valid ){
                const auto ret_pair_s = s_s.insert(j_s);
                if( !ret_pair_s.second ){
                    throw std::invalid_argument("Forced correspondence contains same stationary set point multiple times. Cannot continue.");
                }
            }
            if( !j_is_valid
            &&  !params.permit_move_outliers ){
                throw std::invalid_argument("Cannot force moving point outlier and also disable moving set outliers. Cannot continue.");
            }
            if( !i_is_valid
            &&  !params.permit_stat_outliers ){
                throw std::invalid_argument("Cannot force stationary point outliers and also disable stationary set outliers. Cannot continue.");
            }
        }
    }
    
    // Warn when the Sinkhorn procedure is likely to fail.
    {
        if( (N_stat_points < N_move_points)
        &&  (params.permit_move_outliers == false) ){
            YLOGWARN("Sinkhorn normalization is likely to fail since outliers in the larger point cloud are disallowed");
        }
        if( (N_move_points < N_stat_points)
        &&  (params.permit_stat_outliers == false) ){
            YLOGWARN("Sinkhorn normalization is likely to fail since outliers in the larger point cloud are disallowed");
        }
    }

    // Prepare working buffers.
    //
    // Main system matrix.
    Eigen::MatrixXd L = Eigen::MatrixXd::Zero(N_move_points + 4, N_move_points + 4);
    // Corresponding points working buffer.
    Eigen::MatrixXd Y = Eigen::MatrixXd::Zero(N_move_points + 4, 3); 
    // Identity matrix used for regularization.
    Eigen::MatrixXd I_N4 = Eigen::MatrixXd::Identity(N_move_points + 4, N_move_points + 4);
    // Weighting matrix needed for 'double-sided outlier handling' -- Yang et al. (2011).
    Eigen::MatrixXd W;
    if(params.double_sided_outliers){
        W = Eigen::MatrixXd::Zero(N_move_points + 4, N_move_points + 4);
    }

    // Corresponence matrix.
    Eigen::MatrixXd M = Eigen::MatrixXd::Zero(N_move_points + 1, N_stat_points + 1);

    // TPS model parameters.
    //
    // Will contain the 'warp' component (W) and an affine component (A) coefficients.
    //
    // Note: To avoid a later copy, these coefficients are directly mapped to the transform buffer.
    //
    // Note: These are the parameters that get updated during the transformation update phase.
    if(static_cast<int64_t>(t.W_A.size()) != (N_move_points + 4) * 3){
        throw std::logic_error("TPS coefficients allocated with incorrect size. Refusing to continue.");
    }
    Eigen::Map<Eigen::Matrix< double,
                              Eigen::Dynamic,
                              Eigen::Dynamic,
                              Eigen::ColMajor >> W_A(&(*(t.W_A.begin())),
                                                     N_move_points + 4,  3);
    if( (t.W_A.num_rows() != W_A.rows())
    ||  (t.W_A.num_cols() != W_A.cols()) ){
        throw std::logic_error("TPS coefficient matrix dimesions do not match. Refusing to continue.");
    }


    Eigen::MatrixXd X(N_move_points, 4); // Stacked matrix of homogeneous moving set points.
    Eigen::MatrixXd Z(N_move_points, 4); // Stacked matrix of corresponding homogeneous fixed set points.
    Eigen::MatrixXd k(1, N_move_points); // TPS kernel vector.
    Eigen::MatrixXd K(N_move_points, N_move_points); // TPS kernel matrix.

    // Populate static elements.
    //
    // L matrix: "K" kernel part.
    //
    //
    // Note: The kernel matrix "K" diagonals are set to zero here to match the standard TPS formulation.
    //       Numerical regularization is introduced later when assembling the full system, either by adding a
    //       scaled identity to the kernel block or, when double-sided outlier handling is enabled, via the W*lambda
    //       weighting used in the objective.
    for(int64_t i = 0; i < N_move_points; ++i) L(i, i) = 0.0;
    for(int64_t i = 0; i < N_move_points; ++i){
        const auto P_i = moving_n.points[i];
        for(int64_t j = i + 1; j < N_move_points; ++j){
            const auto P_j = moving_n.points[j];
            const auto dist = P_i.distance(P_j);
            const auto kij = t.eval_kernel(dist);
            L(i, j) = kij;
            L(j, i) = kij;
        }
    }

    // L matrix: "P" and "PT" parts.
    for(int64_t i = 0; i < N_move_points; ++i){
        const auto P_moving = moving_n.points[i];
        L(i, N_move_points + 0) = 1.0;
        L(i, N_move_points + 1) = P_moving.x;
        L(i, N_move_points + 2) = P_moving.y;
        L(i, N_move_points + 3) = P_moving.z;

        L(N_move_points + 0, i) = 1.0;
        L(N_move_points + 1, i) = P_moving.x;
        L(N_move_points + 2, i) = P_moving.y;
        L(N_move_points + 3, i) = P_moving.z;
    }
 
    // Identity matrix used for regularization.
    //
    // Note: The affine part (last 4 diagonal elements) is kept at 1.0 to regularize the affine
    // transformation toward the identity. Without affine regularization, the optimizer can freely
    // collapse A to zero (shrinking the point cloud to a single point) without incurring any
    // bending energy penalty, since the TPS bending energy only penalizes the warp component W.

    // Prime the transformation with an identity affine component and no warp component.
    //
    // Note that the RPM-TPS method gradually progresses from global to local transformations, so if the initial
    // temperature is sufficiently high then something like centroid-matching and PCA-alignment will naturally occur.
    // Conversely, if the temperature is set below the threshold required for global transformations, then only local
    // transformations (waprs) will occur; this may be what the user intends!
    W_A(N_move_points + 1, 0) = 1.0; // x-component.
    W_A(N_move_points + 2, 1) = 1.0; // y-component.
    W_A(N_move_points + 3, 2) = 1.0; // z-component.

    if(params.seed_with_centroid_shift){
        // Seed the affine transformation with the output from a simpler rigid registration.
        // Note: With normalization, both clouds are centered at origin, so a centroid shift
        // in normalized space is essentially zero. This seed is computed in normalized space.
        auto t_com = AlignViaCentroid(moving_n, stationary_n);
        if(!t_com){
            YLOGWARN("Unable to compute centroid seed transformation");
            return std::nullopt;
        }

        W_A(N_move_points + 0, 0) = t_com.value().read_coeff(3,0);
        W_A(N_move_points + 0, 1) = t_com.value().read_coeff(3,1);
        W_A(N_move_points + 0, 2) = t_com.value().read_coeff(3,2);
    }

    // Invert the system matrix.
    //
    // Note: only necessary when regularization is disabled.
    Eigen::MatrixXd L_pinv;
    if( (params.solution_method == AlignViaTPSRPMParams::SolutionMethod::PseudoInverse)
    &&  (std::abs(L_1_start) == 0.0) ){
        L_pinv = L.completeOrthogonalDecomposition().pseudoInverse();
    }

    // Prime the correspondence matrix with uniform correspondence terms.
    for(int64_t i = 0; i < N_move_points; ++i){ // row
        for(int64_t j = 0; j < N_stat_points; ++j){ // column
            M(i, j) = 1.0 / static_cast<double>(N_move_points);
        }
    }
    {
        const auto i = N_move_points; // row
        for(int64_t j = 0; j < N_stat_points; ++j){ // column
            M(i, j) = 0.01 / static_cast<double>(N_move_points);
        }
    }
    for(int64_t i = 0; i < N_move_points; ++i){ // row
        const auto j = N_stat_points; // column
        M(i, j) = 0.01 / static_cast<double>(N_move_points);
    }
    M(N_move_points, N_stat_points) = 0.0;

    // Implement the user-provided forced correspondences, if any exist, by overwriting the correspondence matrix.
    //
    // Note: As far as I can tell, this approach ruins any convergence guarantees the TPS-RPM algorithm would otherwise
    //       provide. Use of correspondence may require fine-tuning of the TPM-RPM algorithm parameters, especially the
    //       number of softassign iterations required.
    const auto implement_forced_correspondence = [&]() -> void {
        for(const auto &apair : params.forced_correspondence){
            const auto i_m = apair.first;
            const auto j_s = apair.second;

            // Invalid indices indicate the valid point is an outlier.
            const auto i_is_valid = isininc(0, i_m, N_move_points - 1);
            const auto j_is_valid = isininc(0, j_s, N_stat_points - 1);

            // Zero-out rows and columns.
            if( i_is_valid ){
                for(int64_t j = 0; j < (N_stat_points + 1); ++j){ // column
                    M(i_m, j) = 0.0;
                }
            }
            if( j_is_valid ){
                for(int64_t i = 0; i < (N_move_points + 1); ++i){ // row
                    M(i, j_s) = 0.0;
                }
            }

            // Place the correspondence coefficient.
            if( i_is_valid && j_is_valid )   M(i_m, j_s) = 1.0;
            if( !i_is_valid && j_is_valid )  M(N_move_points, j_s) = 1.0;
            if( i_is_valid && !j_is_valid )  M(i_m, N_stat_points) = 1.0;
        }
        return;
    };

    // Disable the outlier detection aspect of the Sinkhorn procedure.
    //
    // Note: As far as I can tell, this approach ruins any convergence guarantees the TPS-RPM algorithm would otherwise
    //       provide. Use of correspondence may require fine-tuning of the TPM-RPM algorithm parameters, especially the
    //       number of softassign iterations required.
    const auto disable_outlier_detection = [&]() -> void {

        // Full disallow non-zero outlier coefficients.
        //
        // Note: In some cases this causes the Sinkhorn tehnique to fail. Suppressing, but not altogether disallowing
        //       outlier coefficients does *not* seem to salvage the Sinkhorn method in these cases.
        if(!params.permit_move_outliers){
            for(int64_t i = 0; i < N_move_points; ++i){ // row
                M(i, N_stat_points) = 0.0;
            }
        }
        if(!params.permit_stat_outliers){
            for(int64_t j = 0; j < N_stat_points; ++j){ // column
                M(N_move_points, j) = 0.0;
            }
        }

        return;
    };

    // Report the row- or column-sum (including outlier gutters, but only in the sum part) that deviates the most from
    // the normalization (i.e., every row and every column sums to one, except the row and column including the
    // bottom-right coefficient).
    const auto worst_row_col_sum_deviation = [&]() -> double {
        double w = 0.0;
        for(int64_t i = 0; i < N_move_points; ++i){
            const auto ds = std::abs(M.row(i).sum() - 1.0);
            if( w < ds ) w = ds;
        }
        for(int64_t j = 0; j < N_stat_points; ++j){
            const auto ds = std::abs(M.col(j).sum() - 1.0);
            if( w < ds ) w = ds;
        }
        return w;
    };

    // Update the correspondence.
    //
    // Note: This sub-routine solves for the point cloud correspondence using the current TPS transformation.
    //
    // Note: This sub-routine implements a 'soft-assign' technique for evaluating the correspondence.
    //       It supports outliers in either point cloud set.
    const auto update_correspondence = [&](double T_now, double s_reg) -> void {
        // Non-outlier coefficients.
        Stats::Running_Sum<double> com_moved_x;
        Stats::Running_Sum<double> com_moved_y;
        Stats::Running_Sum<double> com_moved_z;
        for(int64_t i = 0; i < N_move_points; ++i){ // row
            const auto P_moving = moving_n.points[i];
            const auto P_moved = t.transform(P_moving); // Transform the point.
            com_moved_x.Digest(P_moved.x);
            com_moved_y.Digest(P_moved.y);
            com_moved_z.Digest(P_moved.z);
            for(int64_t j = 0; j < N_stat_points; ++j){ // column
                const auto P_stationary = stationary_n.points[j];
                const auto dP = P_stationary - P_moved;
                M(i, j) = (1.0 / T_now)
                        * std::exp(s_reg / T_now)
                        * std::exp( -dP.Dot(dP) / T_now);
            }
        }
        const vec3<double> com_moved( com_moved_x.Current_Sum() / static_cast<double>(N_move_points), 
                                      com_moved_y.Current_Sum() / static_cast<double>(N_move_points), 
                                      com_moved_z.Current_Sum() / static_cast<double>(N_move_points) );

        // Moving outlier coefficients.
        // According to the TPS-RPM algorithm (Chui & Rangarajan), outlier "gutter" coefficients
        // should be uniform across all points, representing the cost of declaring a point an outlier.
        // The outlier coefficient uses the mean nearest-neighbor squared distance as a baseline,
        // ensuring that perfect correspondences (dP = 0) are preferred over declaring points as outliers.
        // This prevents point cloud collapse when correspondences are ambiguous.
        {
            const auto i = N_move_points; // row
            const double outlier_coeff = (1.0 / T_now) 
                                       * std::exp(s_reg / T_now)
                                       * std::exp( -mean_nn_sq_dist / T_now);
            for(int64_t j = 0; j < N_stat_points; ++j){ // column
                M(i, j) = outlier_coeff;
            }
        }

        // Stationary outlier coefficients.
        {
            const auto j = N_stat_points; // column
            const double outlier_coeff = (1.0 / T_now)
                                       * std::exp(s_reg / T_now)
                                       * std::exp( -mean_nn_sq_dist / T_now);
            for(int64_t i = 0; i < N_move_points; ++i){ // row
                M(i, j) = outlier_coeff;
            }
        }

        // Override forced correspondences and disable outlier detection (iff user specifies to do so).
        //
        // Note: Since the Skinhorn normalization procedure only modifies the coefficients via scaling (i.e.,
        // multiplication), hard constraints won't be able to 'un-zero' the zeroed-out coefficients. So updating the
        // hard constraints just prior to normalization is sufficient for achieving forced correspondence.
        implement_forced_correspondence();
        disable_outlier_detection();

        // Normalize the rows and columns iteratively using the Sinkhorn procedure so that the non-outlier part of M
        // becomes doubly-stochastic.
        {
            double w_last = -1.0; // Used to detect if the method stalls.
            const auto machine_eps = 100.0 * std::sqrt( std::numeric_limits<double>::epsilon() );
            for(int64_t norm_iter = 0; norm_iter < params.N_Sinkhorn_iters; ++norm_iter){

                // Tally the current row sums and re-scale the correspondence coefficients.
                for(int64_t i = 0; i < N_move_points; ++i){ // row
                    Stats::Running_Sum<double> rs;
                    for(int64_t j = 0; j < (N_stat_points+1); ++j){ // column
                        rs.Digest( M(i,j) );
                    }
                    const auto s = rs.Current_Sum();
                    if(s < machine_eps){
                        // Option A: error.
                        //throw std::runtime_error("Unable to normalize column");
                        // Option B: forgo normalization.
                        // This might ruin the transform scaling, but it might also self-correct (n.b. verified below!).
                        continue;
                        // Option C: nominate this point as an outlier.
                        // This may work, but I can't say for sure...
                        //row_sums[i] += 1.0;
                        //M(i,N_stat_points) += 1.0;
                    }
                    for(int64_t j = 0; j < (N_stat_points+1); ++j){ // column, intentionally ignoring the outlier coeff.
                        M(i,j) /= s;
                    }
                }

                // Tally the current column sums and re-scale the correspondence coefficients.
                for(int64_t j = 0; j < N_stat_points; ++j){ // column
                    Stats::Running_Sum<double> rs;
                    for(int64_t i = 0; i < (N_move_points+1); ++i){ // row
                        rs.Digest( M(i,j) );
                    }
                    const auto s = rs.Current_Sum();
                    if(s < machine_eps){
                        // Option A: error.
                        //throw std::runtime_error("Unable to normalize row");
                        // Option B: forgo normalization.
                        // This might ruin the transform scaling, but it might also self-correct (n.b. verified below!).
                        continue;
                        // Option C: nominate this point as an outlier.
                        // This may work, but I can't say for sure...
                        //col_sums[j] += 1.0;
                        //M(N_move_points,j) += 1.0;
                    }
                    for(int64_t i = 0; i < (N_move_points+1); ++i){ // row, intentionally ignoring the outlier coeff.
                        M(i,j) /= s;
                    }
                }
                
                // Determine whether convergence has been reached and we can break early.
                const auto w = worst_row_col_sum_deviation();
                if(w < params.Sinkhorn_tolerance){ 
                    break;
                }

                // Determine if the Sinkhorn technique has stalled.
                //
                // Note: Uses *exact* floating-point equality for the most stringent stall check.
                if(w == w_last){
                    throw std::runtime_error("Sinkhorn technique stalled. Unable to normalize correspondence matrix. Cannot continue.");
                }
                w_last = w;

                //YLOGDEBUG("On normalization iteration " << norm_iter << " the mean col sum was " << Stats::Mean(col_sums));
                //YLOGDEBUG("On normalization iteration " << norm_iter << " the mean row sum was " << Stats::Mean(row_sums));
            }
        }

        // Explicitly confirm that normalization was successful.
        //
        // Note: Since we do not use the typical QR decomposition solver with homogeneous coordinates, we have to ensure
        // that M successfully normalizes. If it fails, and using more Sinkhorn iterations doesn't work, consider
        // overriding the spline evaluation function to ensure the W_A spline coefficients sum to zero.
        {
            const auto w = worst_row_col_sum_deviation();
            if(params.Sinkhorn_tolerance < w){
                throw std::runtime_error("Sinkhorn technique failed to normalize correspondence matrix. Consider more Sinkhorn iterations.");
            }
        }

        if(!M.allFinite()){
            throw std::runtime_error("Failed to compute coefficient matrix.");
        }
        return;
    };

    // Estimates how the correspondence matrix will binarize when T -> 0.
    const auto update_final_correspondence = [&]() -> void {
        for(int64_t i = 0; i < N_move_points; ++i){ // row
            double max_coeff = -(std::numeric_limits<double>::infinity());
            int64_t max_j = -1;
            for(int64_t j = 0; j < (N_stat_points + 1); ++j){ // column
                const auto m = M(i,j);
                if(max_coeff < m){
                    max_coeff = m;
                    max_j = j;
                }
            }
            if(!std::isfinite(max_coeff)){
                throw std::logic_error("Unable to estimate binary correspondence.");
            }
            params.final_move_correspondence.emplace_back( std::make_pair(i, max_j) );
        }

        for(int64_t j = 0; j < N_stat_points; ++j){ // column
            double max_coeff = -(std::numeric_limits<double>::infinity());
            int64_t max_i = -1;
            for(int64_t i = 0; i < (N_move_points + 1); ++i){ // row
                const auto m = M(i,j);
                if(max_coeff < m){
                    max_coeff = m;
                    max_i = i;
                }
            }
            if(!std::isfinite(max_coeff)){
                throw std::logic_error("Unable to estimate binary correspondence.");
            }
            params.final_stat_correspondence.emplace_back( std::make_pair(max_i, j) );
        }

        return;
    };

    // Update the transformation.
    //
    // Note: This sub-routine solves for the TPS solution using the current correspondence.
    const auto update_transformation = [&](double lambda) -> void {

        // Fill the Y vector with the corresponding points.
        for(int64_t i = 0; i < N_move_points; ++i){
            // Compute the sum of non-outlier correspondence coefficients for this moving point.
            Stats::Running_Sum<double> row_sum_rs;
            for(int64_t j = 0; j < N_stat_points; ++j){ // column
                row_sum_rs.Digest( M(i,j) );
            }
            const double row_sum = row_sum_rs.Current_Sum();

            if(params.double_sided_outliers){
                const double row_sum_clamped = std::max(row_sum, params.row_sum_weight_threshold);
                double row_sum_inv = 1.0 / row_sum_clamped;
                if(!std::isfinite(row_sum_inv)){
                    row_sum_inv = std::sqrt( std::numeric_limits<double>::max() );
                }
                W(i,i) = row_sum_inv;
            }

            const auto P_moving = moving_n.points[i];
            const auto P_moved = t.transform(P_moving);

            // Compute weighted average of stationary points.
            // When row_sum is near zero (no good correspondences), fall back to the current
            // transformed position to prevent collapse.
            if(row_sum > params.row_sum_weight_threshold){
                Stats::Running_Sum<double> c_x;
                Stats::Running_Sum<double> c_y;
                Stats::Running_Sum<double> c_z;
                for(int64_t j = 0; j < N_stat_points; ++j){ // column
                    const auto P_stationary = stationary_n.points[j];
                    const double weight = M(i,j) / row_sum;
                    c_x.Digest(P_stationary.x * weight);
                    c_y.Digest(P_stationary.y * weight);
                    c_z.Digest(P_stationary.z * weight);
                }
                Y(i, 0) = c_x.Current_Sum();
                Y(i, 1) = c_y.Current_Sum();
                Y(i, 2) = c_z.Current_Sum();
            }else{
                // No good correspondences: use current transformed position as fallback.
                Y(i, 0) = P_moved.x;
                Y(i, 1) = P_moved.y;
                Y(i, 2) = P_moved.z;
            }
        }

        // Add affine regularization bias toward identity transformation.
        //
        // The regularization term lambda * I on the affine rows biases [d; A] toward zero.
        // To bias A toward the identity matrix instead, we add lambda * [0; I] to the
        // right-hand side Y for the affine rows. This prevents the optimizer from collapsing
        // A to zero (which would shrink the point cloud to a single point).
        if(std::abs(lambda) > 0.0){
            Y(N_move_points + 0, 0) = 0.0;
            Y(N_move_points + 0, 1) = 0.0;
            Y(N_move_points + 0, 2) = 0.0;
            Y(N_move_points + 1, 0) = lambda;
            Y(N_move_points + 1, 1) = 0.0;
            Y(N_move_points + 1, 2) = 0.0;
            Y(N_move_points + 2, 0) = 0.0;
            Y(N_move_points + 2, 1) = lambda;
            Y(N_move_points + 2, 2) = 0.0;
            Y(N_move_points + 3, 0) = 0.0;
            Y(N_move_points + 3, 1) = 0.0;
            Y(N_move_points + 3, 2) = lambda;
        }else{
            Y(N_move_points + 0, 0) = 0.0;
            Y(N_move_points + 0, 1) = 0.0;
            Y(N_move_points + 0, 2) = 0.0;
            Y(N_move_points + 1, 0) = 0.0;
            Y(N_move_points + 1, 1) = 0.0;
            Y(N_move_points + 1, 2) = 0.0;
            Y(N_move_points + 2, 0) = 0.0;
            Y(N_move_points + 2, 1) = 0.0;
            Y(N_move_points + 2, 2) = 0.0;
            Y(N_move_points + 3, 0) = 0.0;
            Y(N_move_points + 3, 1) = 0.0;
            Y(N_move_points + 3, 2) = 0.0;
        }

        // Also add affine regularization for double-sided outlier handling.
        if(params.double_sided_outliers){
            W(N_move_points + 0, N_move_points + 0) = 1.0;
            W(N_move_points + 1, N_move_points + 1) = 1.0;
            W(N_move_points + 2, N_move_points + 2) = 1.0;
            W(N_move_points + 3, N_move_points + 3) = 1.0;
        }

        // Use pseudo-inverse method.
        if(params.solution_method == AlignViaTPSRPMParams::SolutionMethod::PseudoInverse){
            // Update the L matrix inverse using current regularization lambda.
            if(std::abs(L_1_start) != 0.0){
                Eigen::MatrixXd R;
                if(params.double_sided_outliers){
                    R = L + W * lambda; // * static_cast<double>(N_stat_points);
                    // Note: Yang et al. (2011) suggest scaling lambda by N_stat_points, but this is not done here for
                    // reasons of parity; the scale of the lambda regularization parameter seems to remain more
                    // comparable with the original algorithm.
                }else{
                    R = L + I_N4 * lambda;
                }

                L_pinv = R.completeOrthogonalDecomposition().pseudoInverse();
            }
            
            if( (L_pinv.rows() == 0) 
            ||  (L_pinv.cols() == 0) ){
                throw std::runtime_error("Matrix inverse not pre-computed. Refusing to continue.");
            }

            // Update W_A.
            W_A = L_pinv * Y;

        // Use LDLT method.
        }else if(params.solution_method == AlignViaTPSRPMParams::SolutionMethod::LDLT){
            Eigen::MatrixXd LHS;
            if(std::abs(L_1_start) != 0.0){
                if(params.double_sided_outliers){
                    LHS = L + W * lambda; // * static_cast<double>(N_stat_points);
                    // Note: Yang et al. (2011) suggest scaling lambda by N_stat_points, but this is not done here for
                    // reasons of parity; the scale of the lambda regularization parameter seems to remain more
                    // comparable with the original algorithm.
                }else{
                    LHS = L + I_N4 * lambda; // Regularized version of L.
                }
            }else{
                LHS = L;
            }
            
            Eigen::LDLT<Eigen::MatrixXd> LDLT;
            LDLT.compute(LHS.transpose() * LHS);
            if(LDLT.info() != Eigen::Success){
                throw std::runtime_error("Unable to update transformation: LDLT decomposition failed.");
            }
            
            W_A = LDLT.solve(LHS.transpose() * Y);
            if(LDLT.info() != Eigen::Success){
                throw std::runtime_error("Unable to update transformation: LDLT solve failed.");
            }
        }else{
            throw std::logic_error("Solution method not understood. Cannot continue.");
        }

        if(!W_A.allFinite()){
            throw std::runtime_error("Failed to update transformation.");
        }

        // Validate transformation to detect potential point cloud collapse or invalid scaling.
        // Check if the transformed points maintain reasonable variance.
        {
            Stats::Running_Variance<double> var_x, var_y, var_z;
            for(int64_t i = 0; i < N_move_points; ++i){
                const auto P_moving = moving_n.points[i];
                const auto P_moved = t.transform(P_moving);
                var_x.Digest(P_moved.x);
                var_y.Digest(P_moved.y);
                var_z.Digest(P_moved.z);
            }
            
            const double total_variance = var_x.Current_Variance() + var_y.Current_Variance() + var_z.Current_Variance();
            if(!std::isfinite(total_variance)){
                throw std::runtime_error("Transformation validation failed: non-finite variance detected.");
            }
            
            // Check for dramatic collapse (variance approaching zero)
            // Note: This threshold may need adjustment based on the scale of the input point clouds
            const double collapse_threshold = 1e-20;
            if(total_variance < collapse_threshold){
                YLOGWARN("Point cloud may be collapsing: total variance = " << total_variance);
            }
        }

        return;
    };

    // Print information about the optimization.
    const auto print_optimizer_progress = [&](double T_now, double /*lambda*/) -> void {

        // Correspondence coefficients.
        //
        // These will approach a binary state (min=0 and max=1) when the temperature is low.
        // Whether these are binary or not fully depends on the temperature, so they can be used to tweak the annealing
        // schedule.
        const auto mean_row_min_coeff = M.rowwise().minCoeff().sum() / static_cast<double>( M.rows() );
        const auto mean_row_max_coeff = M.rowwise().maxCoeff().sum() / static_cast<double>( M.rows() );

        YLOGDEBUG("Optimizer state: T = " << std::setw(12) << T_now 
                   << ", mean min,max corr coeffs = " << std::setw(12) << mean_row_min_coeff
                   << ", " << std::setw(12) << mean_row_max_coeff );
        return;
    };

    // Estimate the current bending energy. Each dimension contributes a separate energy, and in-plane deformations are
    // not accounted for.
    //
    // Note: This estimate comes from Bookstein. It is NOT the full energy, which would also include the square
    //       differences and possibily additional terms (e.g., when double-sided outlier handling is being used).
    //       It is also claimed to merely be *proportional* to the bending energy, so may be off by a constant factor.
    struct bending_energies {
        double x = 0.0;
        double y = 0.0;
        double z = 0.0;
    };
    const auto estimate_bending_energies = [&]() -> bending_energies {

        // Compute (approximate) bending energy.
        const auto E_x = (  W_A.block(0,0, N_move_points,1).transpose()
                            * L.block(0,0, N_move_points,N_move_points) 
                            * W_A.block(0,0, N_move_points,1) ).sum();
        const auto E_y = (  W_A.block(0,1, N_move_points,1).transpose()
                            * L.block(0,0, N_move_points,N_move_points) 
                            * W_A.block(0,1, N_move_points,1) ).sum();
        const auto E_z = (  W_A.block(0,2, N_move_points,1).transpose()
                            * L.block(0,0, N_move_points,N_move_points) 
                            * W_A.block(0,2, N_move_points,1) ).sum();
        return bending_energies{ E_x, E_y, E_z };
    };

/*
    // Debugging routine....
    const auto write_to_xyz_file = [&](const std::string &base){
        const auto fname = Get_Unique_Sequential_Filename(base, 6, ".xyz");

        std::ofstream of(fname);
        for(int64_t i = 0; i < N_move_points; ++i){
            const auto P_moving = moving.points[i];
            const auto P_moved = t.transform(P_moving);
            of << P_moved.x << " " << P_moved.y << " " << P_moved.z << std::endl;
        }
        return;
    };
*/

    // Anneal deterministically.
    for(double T_now = T_start; T_now >= T_end; T_now *= params.T_step){
        // Regularization parameter: controls how smooth the TPS interpolation is.
        const double L_1 = T_now * L_1_start;

        // Regularization parameter: controls bias toward declaring a point an outlier. Chui and Rangarajan recommend
        // setting it "close to zero."
        const double L_2 = T_now * L_2_start;

        for(int64_t iter_at_fixed_T = 0; iter_at_fixed_T < params.N_iters_at_fixed_T; ++iter_at_fixed_T){

            // Update correspondence matrix.
            //
            // Note: When using double-sided outlier handling, the correspondence update should occur first.
            update_correspondence(T_now, L_2);

            // Update transformation.
            update_transformation(L_1);
        }

        print_optimizer_progress(T_now, L_1);

        //write_to_xyz_file("warped_tps-rpm_");
    }

    // Imbue the outgoing structs with information from the registration.
    if(params.report_final_correspondence){
        update_final_correspondence();
    }

    // Report final fit parameters to the user.
    {
        const auto E = estimate_bending_energies();
        const double E_sum = E.x + E.y + E.z;
        YLOGDEBUG("Final bending energy is propto " << E_sum << " with " << E.x << " from x, " << E.y << " from y, and " << E.z << " from z");
    }

/*
// Sample the point cloud COM.
{
    const auto com_move = moving.Centroid();

    std::string base = "moving_com_";
    const auto fname = Get_Unique_Sequential_Filename(base, 6, ".xyz");

    std::ofstream of(fname);
    of << com_move.x << " "
       << com_move.y << " "
       << com_move.z << std::endl;
}

// Sample the transform along cardinal axes across the extent of the object.
{
    const auto com_move = moving.Centroid();

    // Determine the extents of the original point cloud.
    const vec3<double> x_unit(1.0, 0.0, 0.0);
    const vec3<double> y_unit(0.0, 1.0, 0.0);
    const vec3<double> z_unit(0.0, 0.0, 1.0);
    double min_x = std::numeric_limits<double>::infinity();
    double min_y = std::numeric_limits<double>::infinity();
    double min_z = std::numeric_limits<double>::infinity();
    double max_x = -min_x;
    double max_y = -min_y;
    double max_z = -min_z;
    {
        for(const auto &p : moving.points){
            const auto p_com = p - com_move;
            if(p_com.Dot(x_unit) < min_x) min_x = p_com.Dot(x_unit);
            if(p_com.Dot(y_unit) < min_y) min_y = p_com.Dot(y_unit);
            if(p_com.Dot(z_unit) < min_z) min_z = p_com.Dot(z_unit);

            if(max_x < p_com.Dot(x_unit)) max_x = p_com.Dot(x_unit);
            if(max_y < p_com.Dot(y_unit)) max_y = p_com.Dot(y_unit);
            if(max_z < p_com.Dot(z_unit)) max_z = p_com.Dot(z_unit);
        }
    }

    const vec3<double> x_min = com_move + x_unit * (min_x - 1.0);
    const vec3<double> x_max = com_move + x_unit * (max_x + 1.0);
    const vec3<double> y_min = com_move + y_unit * (min_y - 1.0);
    const vec3<double> y_max = com_move + y_unit * (max_y + 1.0);
    const vec3<double> z_min = com_move + z_unit * (min_z - 1.0);
    const vec3<double> z_max = com_move + z_unit * (max_z + 1.0);

    // Sample the transform along these axes.
    std::string base = "final_cardinal_axes_";
    const auto fname = Get_Unique_Sequential_Filename(base, 6, ".xyz");

    std::ofstream of(fname);
    for(double dt = 0.0; dt < 1.0; dt += 0.01){
        //const auto p_x = tps_func(x_min + (x_max - x_min) * dt);
        //const auto p_y = tps_func(y_min + (y_max - y_min) * dt);
        //const auto p_z = tps_func(z_min + (z_max - z_min) * dt);
        const auto p_x = t.transform(x_min + (x_max - x_min) * dt);
        const auto p_y = t.transform(y_min + (y_max - y_min) * dt);
        const auto p_z = t.transform(z_min + (z_max - z_min) * dt);
        of << p_x.x << " " << p_x.y << " " << p_x.z << std::endl;
        of << p_y.x << " " << p_y.y << " " << p_y.z << std::endl;
        of << p_z.x << " " << p_z.y << " " << p_z.z << std::endl;
    }
}
*/

    // Denormalize analytically: convert the TPS from normalized space back to original coordinates
    // without solving a second TPS system.
    {
        thin_plate_spline t_denorm(moving, params.kernel_dimension);

        const double c_m_x = centroid_moving.x;
        const double c_m_y = centroid_moving.y;
        const double c_m_z = centroid_moving.z;

        const double c_s_x = centroid_stationary.x;
        const double c_s_y = centroid_stationary.y;
        const double c_s_z = centroid_stationary.z;

        Stats::Running_Sum<double> weighted_sqnorm_x;
        Stats::Running_Sum<double> weighted_sqnorm_y;
        Stats::Running_Sum<double> weighted_sqnorm_z;
        for(int64_t i = 0; i < N_move_points; ++i){
            const auto &p = moving.points[i];
            const double sq_norm = (p.x * p.x) + (p.y * p.y) + (p.z * p.z);

            const double w_norm_x = W_A(i, 0);
            const double w_norm_y = W_A(i, 1);
            const double w_norm_z = W_A(i, 2);

            if(params.kernel_dimension == 2){
                t_denorm.W_A.coeff(i, 0) = w_norm_x / norm_scale;
                t_denorm.W_A.coeff(i, 1) = w_norm_y / norm_scale;
                t_denorm.W_A.coeff(i, 2) = w_norm_z / norm_scale;
            }else if(params.kernel_dimension == 3){
                t_denorm.W_A.coeff(i, 0) = w_norm_x;
                t_denorm.W_A.coeff(i, 1) = w_norm_y;
                t_denorm.W_A.coeff(i, 2) = w_norm_z;
            }else{
                point_set<double> denorm_targets;
                for(int64_t j = 0; j < N_move_points; ++j){
                    const auto p_norm = moving_n.points[j];
                    const auto t_norm = t.transform(p_norm);
                    denorm_targets.points.emplace_back(vec3<double>(
                        t_norm.x * norm_scale + centroid_stationary.x,
                        t_norm.y * norm_scale + centroid_stationary.y,
                        t_norm.z * norm_scale + centroid_stationary.z
                    ));
                }

                AlignViaTPSParams tps_params;
                tps_params.kernel_dimension = params.kernel_dimension;
                tps_params.lambda = params.lambda_start;
                tps_params.solution_method = (params.solution_method == AlignViaTPSRPMParams::SolutionMethod::PseudoInverse)
                                             ? AlignViaTPSParams::SolutionMethod::PseudoInverse
                                             : AlignViaTPSParams::SolutionMethod::LDLT;
                return AlignViaTPS(tps_params, moving, denorm_targets);
            }

            weighted_sqnorm_x.Digest(w_norm_x * sq_norm);
            weighted_sqnorm_y.Digest(w_norm_y * sq_norm);
            weighted_sqnorm_z.Digest(w_norm_z * sq_norm);
        }

        t_denorm.W_A.coeff(N_move_points + 1, 0) = W_A(N_move_points + 1, 0);
        t_denorm.W_A.coeff(N_move_points + 2, 0) = W_A(N_move_points + 2, 0);
        t_denorm.W_A.coeff(N_move_points + 3, 0) = W_A(N_move_points + 3, 0);
        t_denorm.W_A.coeff(N_move_points + 1, 1) = W_A(N_move_points + 1, 1);
        t_denorm.W_A.coeff(N_move_points + 2, 1) = W_A(N_move_points + 2, 1);
        t_denorm.W_A.coeff(N_move_points + 3, 1) = W_A(N_move_points + 3, 1);
        t_denorm.W_A.coeff(N_move_points + 1, 2) = W_A(N_move_points + 1, 2);
        t_denorm.W_A.coeff(N_move_points + 2, 2) = W_A(N_move_points + 2, 2);
        t_denorm.W_A.coeff(N_move_points + 3, 2) = W_A(N_move_points + 3, 2);

        double t0_x = c_s_x
                    + norm_scale * W_A(N_move_points + 0, 0)
                    - W_A(N_move_points + 1, 0) * c_m_x
                    - W_A(N_move_points + 2, 0) * c_m_y
                    - W_A(N_move_points + 3, 0) * c_m_z;
        double t0_y = c_s_y
                    + norm_scale * W_A(N_move_points + 0, 1)
                    - W_A(N_move_points + 1, 1) * c_m_x
                    - W_A(N_move_points + 2, 1) * c_m_y
                    - W_A(N_move_points + 3, 1) * c_m_z;
        double t0_z = c_s_z
                    + norm_scale * W_A(N_move_points + 0, 2)
                    - W_A(N_move_points + 1, 2) * c_m_x
                    - W_A(N_move_points + 2, 2) * c_m_y
                    - W_A(N_move_points + 3, 2) * c_m_z;

        if(params.kernel_dimension == 2){
            const double log_scale_sq_over_scale = std::log(norm_scale * norm_scale) / norm_scale;
            t0_x -= log_scale_sq_over_scale * weighted_sqnorm_x.Current_Sum();
            t0_y -= log_scale_sq_over_scale * weighted_sqnorm_y.Current_Sum();
            t0_z -= log_scale_sq_over_scale * weighted_sqnorm_z.Current_Sum();
        }

        t_denorm.W_A.coeff(N_move_points + 0, 0) = t0_x;
        t_denorm.W_A.coeff(N_move_points + 0, 1) = t0_y;
        t_denorm.W_A.coeff(N_move_points + 0, 2) = t0_z;

        if(!std::isfinite(t0_x) || !std::isfinite(t0_y) || !std::isfinite(t0_z)){
            throw std::runtime_error("Failed to denormalize TPS mapping function.");
        }

        return t_denorm;
    }
}
#endif // DCMA_USE_EIGEN

