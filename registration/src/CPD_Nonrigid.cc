#include "YgorFilesDirs.h"    //Needed for Does_File_Exist_And_Can_Be_Read(...), etc..
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorMath.h"         //Needed for samples_1D.
#include "YgorString.h"       //Needed for GetFirstRegex(...)
#include "CPD_Nonrigid.h"
#include <iostream>
#include <chrono>
#include <cmath>
#include <eigen3/Eigen/Core>
#include "YgorMathIOXYZ.h"    //Needed for ReadPointSetFromXYZ.
using namespace std::chrono;

// Initialize non rigid transform
NonRigidCPDTransform::NonRigidCPDTransform(int N_move_points, int dimensionality) {
    this->dim = dimensionality;
    this->W = Eigen::MatrixXf::Zero(N_move_points, this->dim); 
}

// Apply non-rigid transform to point set
void NonRigidCPDTransform::apply_to(point_set<double> &ps) {
    auto N_points = static_cast<long int>(ps.points.size());
    Eigen::MatrixXf Y = Eigen::MatrixXf::Zero(N_points, this->dim); 
    
    // Fill the X vector with the corresponding points.
    for(long int j = 0; j < N_points; ++j) { // column
        auto P = ps.points[j];
        Y(j, 0) = P.x;
        Y(j, 1) = P.y;
        Y(j, 2) = P.z;
    }
    auto Y_hat = apply_to(Y);
    for(long int j = 0; j < N_points; ++j) { // column
        ps.points[j].x = Y_hat(j, 0);
        ps.points[j].y = Y_hat(j, 1);
        ps.points[j].z = Y_hat(j, 2);
    }
}

// Write non-rigid transformation to file
void NonRigidCPDTransform::write_to( std::ostream &os ) {
    Eigen::MatrixXf m = this->G * this->W;
    int rows = m.rows();
    for(int i = 0; i < rows; i++) {
        for(int j = 0; j < this->dim; j++) {
            os << m(i, j);
            os << " ";
        }
        os << "\n";
    }
}

// Apply transform to y points in matrix form
Eigen::MatrixXf NonRigidCPDTransform::apply_to(const Eigen::MatrixXf & ps) {
    return ps + this->G * this->W;
}

// Compute initial sigma squared for non-rigid algorithm
double Init_Sigma_Squared_NR(const Eigen::MatrixXf & xPoints,
            const Eigen::MatrixXf & yPoints) {

    double normSum = 0;
    int nRowsX = xPoints.rows();
    int mRowsY =  yPoints.rows();
    int dim = xPoints.cols();
    
    for (int i = 0; i < nRowsX; i++) {
        const auto xRow = xPoints.row(i).transpose();
        for (int j = 0; j < mRowsY; j++) {
            const auto yRow = yPoints.row(j).transpose();
            auto rowDiff = xRow - yRow;
            normSum += rowDiff.squaredNorm();
        }
    }
    return normSum / ((double)nRowsX * mRowsY * dim);
}

// Calculate gram matrix for non-rigid algorithm
Eigen::MatrixXf GetGramMatrix(const Eigen::MatrixXf & yPoints, double betaSquared) {
    int mRowsY = yPoints.rows();
    double expArg;
    Eigen::MatrixXf gramMatrix = Eigen::MatrixXf::Zero(mRowsY,mRowsY);
    Eigen::MatrixXf tempVector;
    
    for (int i = 0; i < mRowsY; ++i) {
        for (int j = 0; j < mRowsY; ++j) {
            tempVector = yPoints.row(i) - yPoints.row(j);
            expArg = - 1 / (2 * betaSquared) * tempVector.squaredNorm();
            gramMatrix(i,j) = exp(expArg);
        }
    }

    return gramMatrix;
}

// Calculate similarity score for non-rigid algorithm
double GetSimilarity_NR(const Eigen::MatrixXf & xPoints,
            const Eigen::MatrixXf & yPoints,
            const Eigen::MatrixXf & gramMatrix,
            const Eigen::MatrixXf & W) {
    
    int mRowsY = yPoints.rows();
    int nRowsX = xPoints.rows(); 
    Eigen::MatrixXf alignedYPoints = AlignedPointSet_NR(yPoints, gramMatrix, W);
    Eigen::MatrixXf tempVector;

    double sum = 0;
    double min_distance = -1;
    for (int m = 0; m < mRowsY; ++m) {
        min_distance = -1;
        for (int n = 0; n < nRowsX; ++n) {
            tempVector = xPoints.row(n) - alignedYPoints.row(m);
            if (min_distance < 0 || tempVector.norm() < min_distance) {
                min_distance = tempVector.norm();
            }
        }
        sum += min_distance;
    }

    sum = sum / (mRowsY * 1.00);

    return sum;
}

// Calculate objective function for non-rigid algorithm
double GetObjective_NR(const Eigen::MatrixXf & xPoints,
            const Eigen::MatrixXf & yPoints,
            const Eigen::MatrixXf & postProb,
            const Eigen::MatrixXf & gramMatrix,
            const Eigen::MatrixXf & W,
            double sigmaSquared) {
    
    int mRowsY = yPoints.rows();
    int nRowsX = xPoints.rows(); 
    double dimensionality = xPoints.cols();
    double Np = postProb.sum();
    Eigen::MatrixXf alignedYPoints = AlignedPointSet_NR(yPoints, gramMatrix, W);
    Eigen::MatrixXf tempVector;
    double leftSum = 0;
    for (int m = 0; m < mRowsY; ++m) {
        for (int n = 0; n < nRowsX; ++n) {
            tempVector = xPoints.row(n) - alignedYPoints.row(m);
            leftSum += postProb(m,n) * tempVector.squaredNorm();
        }
    }
    leftSum = leftSum / (2.0 * sigmaSquared);
    double rightSum = Np * dimensionality / 2.0 * log(sigmaSquared);
    return leftSum + rightSum;
}

// Calculate posterior probability matrix for E step in non-rigid algorithm
Eigen::MatrixXf E_Step_NR(const Eigen::MatrixXf & xPoints,
            const Eigen::MatrixXf & yPoints,
            const Eigen::MatrixXf & gramMatrix,
            const Eigen::MatrixXf & W,
            double sigmaSquared,
            double w) {
    Eigen::MatrixXf postProb = Eigen::MatrixXf::Zero(yPoints.rows(),xPoints.rows());
    Eigen::MatrixXf expMat = Eigen::MatrixXf::Zero(yPoints.rows(),xPoints.rows());

    Eigen::MatrixXf tempVector;
    double expArg;
    double numerator;
    double denominator;

    int mRowsY = yPoints.rows();
    int nRowsX = xPoints.rows();
    int dimensionality = yPoints.cols();

    for (int m = 0; m < mRowsY; ++m) {
        tempVector = yPoints.row(m) + gramMatrix.row(m) * W;
        for (int n = 0; n < nRowsX; ++n) {
            expArg = - 1 / (2 * sigmaSquared) * (xPoints.row(n) - tempVector).squaredNorm();
            expMat(m,n) = exp(expArg);
        }
    }

    for (int n = 0; n < nRowsX; ++n) {
        denominator = expMat.col(n).sum() + 
                          pow((2 * M_PI * sigmaSquared),((double)(dimensionality/2.0))) * (w/(1-w)) * (double)(mRowsY / nRowsX);
        for (int m = 0; m < mRowsY; ++m) {
            numerator = expMat(m,n);
            postProb(m,n) = numerator / denominator;
        }
    }

    return postProb;
}

// Calculate W matrix of coefficients for non-rigid algorithm
Eigen::MatrixXf GetW(const Eigen::MatrixXf & yPoints,
            const Eigen::MatrixXf & gramMatrix,
            const Eigen::MatrixXf & postProbInvDiag, // d(P1)^-1
            const Eigen::MatrixXf & postProbX,
            double sigmaSquared,
            double lambda){
    
    Eigen::MatrixXf A = gramMatrix + lambda * sigmaSquared * postProbInvDiag;
    Eigen::MatrixXf b = postProbInvDiag * postProbX - yPoints;

    return A.llt().solve(b); // assumes A is positive definite, uses llt decomposition
}

// Calculate W matrix of coefficients in the low rank approximation of the non-rigid algorithm
Eigen::MatrixXf LowRankGetW(const Eigen::MatrixXf & yPoints,
            const Eigen::VectorXf & gramValues,
            const Eigen::MatrixXf & gramVectors,
            const Eigen::MatrixXf & postProbOne,
            const Eigen::MatrixXf & postProbX,
            double sigmaSquared,
            double lambda) {
    double coef = 1/(lambda * sigmaSquared);
    Eigen::MatrixXf postProbInvDiag = ((postProbOne).asDiagonal()).inverse(); // d(P1)^-1
    Eigen::MatrixXf first = coef * (postProbOne).asDiagonal();
    Eigen::MatrixXf invertedValues = gramValues.asDiagonal().inverse();
    Eigen::MatrixXf toInvert = invertedValues + coef * gramVectors.transpose()*(postProbOne).asDiagonal()*gramVectors;
    Eigen::MatrixXf inverted = toInvert.llt().solve(Eigen::MatrixXf::Identity(gramValues.size(), gramValues.size()));
    Eigen::MatrixXf b = postProbInvDiag * postProbX - yPoints;
    return (first - pow(coef, 2) * (postProbOne).asDiagonal() * gramVectors * inverted * gramVectors.transpose() * (postProbOne).asDiagonal()) * b;
}

// Compute aligned point set from y points and transformation matrices for non-rigid algorithm
Eigen::MatrixXf AlignedPointSet_NR(const Eigen::MatrixXf & yPoints,
            const Eigen::MatrixXf & gramMatrix,
            const Eigen::MatrixXf & W){

    return yPoints + (gramMatrix * W);
}

// Calculate sigma squared for non-rigid algorithm
double SigmaSquared(const Eigen::MatrixXf & xPoints,
            const Eigen::MatrixXf & postProbOne,
            const Eigen::MatrixXf & postProbTransOne,
            const Eigen::MatrixXf & postProbX,
            const Eigen::MatrixXf & transformedPoints){

    int dim = xPoints.cols();
    double Np = postProbOne.sum();
    double firstTerm = (double)(xPoints.transpose() * (postProbTransOne).asDiagonal() * xPoints).trace();
    double secondTerm = (double)(2 * ((postProbX).transpose() * transformedPoints).trace());
    double thirdTerm = (double)(transformedPoints.transpose() * (postProbOne).asDiagonal() * transformedPoints).trace();

    return (firstTerm - secondTerm + thirdTerm) / (Np * dim);
}

// Updated function to calculate N largest eigenvalues for low rank approximation. This function is currently being used
void GetNLargestEigenvalues_V2(const Eigen::MatrixXf & m,
            Eigen::MatrixXf & vector_matrix,
            Eigen::VectorXf & value_matrix,
            int num_eig,
            int size) {
    Eigen::SelfAdjointEigenSolver<Eigen::MatrixXf> solver(m);
    Eigen::VectorXf values = solver.eigenvalues();
    Eigen::MatrixXf vectors = solver.eigenvectors();
    value_matrix = values.tail(num_eig);
    vector_matrix = vectors.block(0, size - num_eig, size, num_eig);
}

// Function to calculate N largest eigenvalues for low rank approximation
void GetNLargestEigenvalues(const Eigen::MatrixXf & m,
            Eigen::MatrixXf & vector_matrix,
            Eigen::VectorXf & value_matrix,
            int num_eig,
            int size,
            int power_iter,
            double power_tol) {
    double ev;
    Eigen::MatrixXf working_m = m.replicate(1, 1);
    Eigen::VectorXf working_v = Eigen::VectorXf::Random(size);
    FUNCINFO(num_eig)
    for(int i = 0; i < num_eig; i++) {
        working_v = Eigen::VectorXf::Random(size);
        ev = PowerIteration(working_m, working_v, power_iter, power_tol);
        value_matrix(i) = ev; 
        Eigen::VectorXf v = working_v;

        vector_matrix.col(i) = v;
        working_m = working_m-ev * working_v * working_v.transpose();
    }
}

// Function for power iteration function for low rank approximation
double PowerIteration(const Eigen::MatrixXf & m,
            Eigen::VectorXf & v, 
            int num_iter,
            double tolerance) {
    double norm;
    double prev_ev = 0;
    double ev = 0;
    Eigen::VectorXf new_v;
    v.normalize();
    for (int i =0; i < num_iter; i++) {
        prev_ev = ev;
        new_v = m * v;
        ev = v.dot(new_v);
        norm = new_v.norm();
        v = new_v / norm;
        if(abs(ev - prev_ev) < tolerance) {
            break;
        }
    }
    return ev;
}

// Function to compute CPD products in IFGT algorithm
// Y = target_pts = moving_pts
// X = source_pts = fixed_pts 
// epsilon is error, w is a parameter from cpd
CPD_MatrixVector_Products ComputeCPDProductsIfgt(const Eigen::MatrixXf & fixed_pts,
                                                    const Eigen::MatrixXf & moving_pts,
                                                    double sigmaSquared, 
                                                    double epsilon,
                                                    double w) {
    
    int N_fixed_pts = fixed_pts.rows();
    int M_moving_pts = moving_pts.rows();
    int dim = fixed_pts.cols();
    double bandwidth = std::sqrt(2.0 * sigmaSquared);

    double c = w / (1.0 - w) * (double) M_moving_pts / N_fixed_pts * 
                            std::pow(2.0 * M_PI * sigmaSquared, 0.5 * dim); // const in denom of P matrix
    
    Eigen::MatrixXf fixed_pts_scaled;
    Eigen::MatrixXf moving_pts_scaled;
    
    // Optional: check time taken during IFGT function, several lines involving timing throughout the function
    // duration<double>  time_span;
    // auto bw_scale_start = std::chrono::high_resolution_clock::now();

    double bandwidth_scaled = rescale_points(fixed_pts, moving_pts, fixed_pts_scaled, 
                                moving_pts_scaled, bandwidth);
    
    FUNCINFO("bandwidth scaled: " << bandwidth_scaled);
    auto ifgt_transform = std::make_unique<IFGT>(moving_pts_scaled, bandwidth_scaled, epsilon); // in this case, moving_pts = source_pts
                                                                                                // because we take the transpose of K(M x N)                                                 
    auto Kt1 = ifgt_transform->compute_ifgt(fixed_pts_scaled);                                       // so we'll get an N x 1 vector for Kt1 


    // auto ifgt1 = std::chrono::high_resolution_clock::now();
	// time_span = std::chrono::duration_cast<std::chrono::duration<double>>(ifgt1 - bw_scale_start);
    // std::cout << "First IFGT (Kt1) took time: " << time_span.count() << " s" << std::endl;

    auto denom_a = Kt1.array() + c; 
    auto Pt1 = (1 - c / denom_a).matrix(); // Pt1 = 1-c*a

    // auto Pt1_time = std::chrono::high_resolution_clock::now();
	// time_span = std::chrono::duration_cast<std::chrono::duration<double>>(Pt1_time - ifgt1);
    // std::cout << "Calculating Pt1 took time: " << time_span.count() << " s" << std::endl;

    ifgt_transform = std::make_unique<IFGT>(fixed_pts_scaled, bandwidth_scaled, epsilon); 
    auto P1 = ifgt_transform->compute_ifgt(moving_pts_scaled, 1 / denom_a); // P1 = Ka 

    // auto P1_time = std::chrono::high_resolution_clock::now();
	// time_span = std::chrono::duration_cast<std::chrono::duration<double>>(P1_time - Pt1_time);
    // std::cout << "IFGT (P1) took time: " << time_span.count() << " s" << std::endl;

    Eigen::MatrixXf PX(M_moving_pts, dim);
    for (int i = 0; i < dim; ++i) {
        // auto PX_start = std::chrono::high_resolution_clock::now();

        PX.col(i) = ifgt_transform->compute_ifgt(moving_pts_scaled, fixed_pts.col(i).array() / denom_a); // PX = K(a.*X)

        // auto PX_end = std::chrono::high_resolution_clock::now();
        // time_span = std::chrono::duration_cast<std::chrono::duration<double>>(PX_end - PX_start);
        // std::cout << "IFGT (PX) column " << i << " took time: " << time_span.count() << " s" << std::endl;
    }
    
    // auto L_start = std::chrono::high_resolution_clock::now();
    // objective function estimate
    
    double L = -log(denom_a).sum() + dim * N_fixed_pts * std::log(sigmaSquared) / 2.0;

    // auto L_end = std::chrono::high_resolution_clock::now();
    // time_span = std::chrono::duration_cast<std::chrono::duration<double>>(L_end - L_start);
    // std::cout << "Calculating L took time: " << time_span.count() << " s" << std::endl;

    return { P1, Pt1, PX, L};
}

// Naive implementation to compute several CPD Products used in non-rigid algorithm
CPD_MatrixVector_Products ComputeCPDProductsNaive(const Eigen::MatrixXf & fixed_pts,
                                                    const Eigen::MatrixXf & moving_pts,
                                                    double sigmaSquared, 
                                                    double w) {
    
    int N_fixed_pts = fixed_pts.rows();
    int M_moving_pts = moving_pts.rows();
    int dim = fixed_pts.cols();
    double bandwidth = std::sqrt(2.0 * sigmaSquared);

    double c = w / (1.0 - w) * (double) M_moving_pts / N_fixed_pts * 
                            std::pow(2.0 * M_PI * sigmaSquared, 0.5 * dim); // const in denom of P matrix

    Eigen::MatrixXf N_ones = Eigen::ArrayXf::Ones(N_fixed_pts, 1);
    auto Kt1 = compute_naive_gt(fixed_pts, moving_pts, N_ones, bandwidth);

    auto denom_a = Kt1.array() + c; 
    auto Pt1 = (1 - c / denom_a).matrix(); // Pt1 = 1-c*a

    auto P1 = compute_naive_gt(moving_pts, fixed_pts, 1.0 / denom_a, bandwidth);

    Eigen::MatrixXf PX(M_moving_pts, dim);
    for (int i = 0; i < dim; ++i) {
        PX.col(i) = compute_naive_gt(moving_pts, fixed_pts, fixed_pts.col(i).array() / denom_a, bandwidth); // PX = K(a.*X)
    }

    double L = -log(denom_a).sum() + dim * N_fixed_pts * std::log(sigmaSquared) / 2.0;

    return { P1, Pt1, PX, L};
}

// Function to get L in the naive implementation in order to determine convergence
// run this to get L_temp when using E_step_NR
double UpdateNaiveConvergenceL(const Eigen::MatrixXf & postProbTransOne,
                            const Eigen::MatrixXf & xPoints,
                            const Eigen::MatrixXf & yPoints,
                            double sigmaSquared,
                            double w, 
                            int dim) {

    Eigen::ArrayXf denom_a;
    if (w > 0.0) {
        double c = w / (1.0 - w) * (double) yPoints.rows() / xPoints.rows() * 
                            std::pow(2.0 * M_PI * sigmaSquared, 0.5 * dim);
        denom_a = c / (1 - postProbTransOne.array());
    }
    else {
        Eigen::MatrixXf N_ones = Eigen::ArrayXf::Ones(xPoints.rows(), 1);
        denom_a = compute_naive_gt(xPoints, yPoints, N_ones, std::sqrt(2.0 * sigmaSquared));
    }
     
    return -log(denom_a).sum() + dim * xPoints.rows() * std::log(sigmaSquared) / 2.0;
    
}

// Function to update L in order to determine convergence
double UpdateConvergenceL(const Eigen::MatrixXf & gramMatrix,
                        const Eigen::MatrixXf & W,
                        double L_computed,
                        double lambda) {

    return L_computed + lambda / 2.0 * (W.transpose() * gramMatrix * W).trace();
}

// Main loop for non-rigid registration algorithm
NonRigidCPDTransform
AlignViaNonRigidCPD(CPDParams & params,
            const point_set<double> & moving,
            const point_set<double> & stationary,
            int iter_interval,
            std::string video,
            std::string xyz_outfile) { 
    
    FUNCINFO("Performing nonrigid CPD");

    // Calculate time performance of the algorithm
    high_resolution_clock::time_point start_all = high_resolution_clock::now();

    std::string temp_xyz_outfile;
    point_set<double> mutable_moving = moving;
    
    Eigen::MatrixXf GetGramMatrix(const Eigen::MatrixXf & yPoints, double betaSquared);
    const auto N_move_points = static_cast<long int>(moving.points.size());
    const auto N_stat_points = static_cast<long int>(stationary.points.size());

    // Prepare working buffers.
    
    // Stationary point matrix
    Eigen::MatrixXf Y = Eigen::MatrixXf::Zero(N_move_points, params.dimensionality);
    // Moving point matrix
    Eigen::MatrixXf X = Eigen::MatrixXf::Zero(N_stat_points, params.dimensionality); 

    // Fill the X vector with the corresponding points.
    for(long int j = 0; j < N_stat_points; ++j){ // column
        const auto P_stationary = stationary.points[j];
        X(j, 0) = P_stationary.x;
        X(j, 1) = P_stationary.y;
        X(j, 2) = P_stationary.z;
    }

    // Fill the Y vector with the corresponding points.
    for(long int j = 0; j < N_move_points; ++j){ // column
        const auto P_moving = moving.points[j];
        Y(j, 0) = P_moving.x;
        Y(j, 1) = P_moving.y;
        Y(j, 2) = P_moving.z;
    }

    NonRigidCPDTransform transform(N_move_points, params.dimensionality);
    double sigma_squared = Init_Sigma_Squared_NR(X, Y);
    double similarity;
    // double objective = 0;
    // double prev_objective = 0;
    int num_eig = params.ev_ratio * N_stat_points;

    Eigen::MatrixXf vector_matrix = Eigen::MatrixXf::Zero(num_eig, num_eig);
    Eigen::VectorXf value_matrix = Eigen::VectorXf::Zero(num_eig);
    transform.G = GetGramMatrix(Y, params.beta * params.beta);

    if(params.use_low_rank) {
        high_resolution_clock::time_point start = high_resolution_clock::now();
        GetNLargestEigenvalues_V2(transform.G, vector_matrix, value_matrix, num_eig, N_stat_points);
        // GetNLargestEigenvalues(transform.G, vector_matrix, value_matrix, num_eig, N_stat_points, params.power_iter, params.power_tol);
        high_resolution_clock::time_point stop = high_resolution_clock::now();
        duration<double>  time_span = duration_cast<duration<double>>(stop - start);
        FUNCINFO("Excecution took time: " << time_span.count())
    }
    Eigen::MatrixXf postProbX;
    Eigen::VectorXf postProbOne, postProbTransOne;
    Eigen::MatrixXf T;

    Eigen::MatrixXf oneVecRow = Eigen::MatrixXf::Ones(Y.rows(), 1);
    Eigen::MatrixXf oneVecCol = Eigen::MatrixXf::Ones(X.rows(),1);

    double L = 1.0; 

    // Print stats to outfile_stats.csv
    std::ofstream os(xyz_outfile + "_stats.csv");
    os << "iteration" << "," << "time" << "," << "similarity" << "," << "outfile" << "\n";
    similarity = GetSimilarity_NR(X, Y, transform.G, transform.W);
    os << 0 << "," << 0 << "," << similarity << "," << xyz_outfile + "_iter0.xyz" << "\n";
    
    for (int i = 0; i < params.iterations; i++) {
        FUNCINFO("Iteration: " << i)
        high_resolution_clock::time_point time_sofar = high_resolution_clock::now();
        duration<double>  time_span_sofar = duration_cast<duration<double>>(time_sofar - start_all);
        FUNCINFO("Time elapsed so far: " << time_span_sofar.count());

        double L_old = L;

        // E step 
        high_resolution_clock::time_point start_estep = high_resolution_clock::now();

        double L_temp = 1.0;

        // Calculate CPD parameters in naive and IFGT implementaitons, depending on option
        if(params.use_fgt) {
            // X = fixed points = source points 
	        // Y = moving points = target points
            // smaller epsilon = smaller error (epsilon > 0)
            auto cpd_products = ComputeCPDProductsIfgt(X, Y + transform.G * transform.W, sigma_squared, params.epsilon, 
                                                        params.distribution_weight);
            postProbX = cpd_products.PX;
            postProbTransOne = cpd_products.Pt1;
            postProbOne = cpd_products.P1;
            L_temp = cpd_products.L;
        } else {
            // calculating postProb is faste than calculating naive matrix vector products
            auto postProb = E_Step_NR(X, Y,transform.G, transform.W, sigma_squared, params.distribution_weight);
            postProbOne = postProb * oneVecRow;
            postProbTransOne = postProb.transpose() * oneVecRow;
            postProbX = postProb * X;
            L_temp = UpdateNaiveConvergenceL(postProbTransOne, X, Y, sigma_squared, params.distribution_weight, X.cols());
        }

        high_resolution_clock::time_point end_estep = high_resolution_clock::now();
        duration<double>  time_span_estep = duration_cast<duration<double>>(end_estep - start_estep);
        FUNCINFO("E Step took time: " << time_span_estep.count());

        L = UpdateConvergenceL(transform.G, transform.W, L_temp, params.lambda);

        high_resolution_clock::time_point start_w = high_resolution_clock::now();

        // Calculate W in naive and low rank implementations
        if(params.use_low_rank) {
            // check if W is invertible
            double product_postprob = 1.0;
            for (int k = 0; k < postProbOne.rows(); k++) {
                product_postprob = product_postprob * postProbOne(k);
            }
            if (product_postprob == 0.0) {
                FUNCINFO("ILL DEFINED P -- FINAL SIMILARITY: " << similarity);
                break;
            }
            transform.W = LowRankGetW(Y, value_matrix, vector_matrix, postProbOne, postProbX, sigma_squared, params.lambda);

        } else {
            // Check if the matrix is invertible before finding w
            Eigen::MatrixXf postProbInvDiag = ((postProbOne).asDiagonal()).inverse();

            if(isnan(postProbInvDiag(0,0)) || isinf(postProbInvDiag(0,0))) {
                FUNCINFO("ILL DEFINED P -- FINAL SIMILARITY: " << similarity);
                break;
            }

            transform.W = GetW(Y, transform.G, postProbInvDiag, postProbX, sigma_squared, params.lambda);
        }

        high_resolution_clock::time_point end_w = high_resolution_clock::now();
        duration<double>  time_span_w = duration_cast<duration<double>>(end_w - start_w);
        FUNCINFO("GetW took time: " << time_span_w.count());

        // Calculate sigma squared, similarity, and objective
        
        T = transform.apply_to(Y);
        sigma_squared = SigmaSquared(X, postProbOne, postProbTransOne, postProbX, T);

        FUNCINFO("Sigma Squared: " << sigma_squared);

        if (isnan(sigma_squared)) {
            FUNCINFO("FINAL SIMILARITY: " << similarity);
            break;
        }

        similarity = GetSimilarity_NR(X, Y, transform.G, transform.W);
        FUNCINFO("Similarity: " << similarity);

        // Calculate objective. Old version involves the change in tolerance, the new version involves calculating the change in L
        // prev_objective = objective;
        // objective = GetObjective_NR(X, Y, P, transform.G, transform.W, sigma_squared);

        double objective_tolerance = abs((L - L_old) / L);
        FUNCINFO("Objective: " << objective_tolerance);

      
        // Write point sets for video tool
        if (video == "True") {
            if (iter_interval > 0 && i % iter_interval == 0) {
                temp_xyz_outfile = xyz_outfile + "_iter" + std::to_string(i+1) + "_sim" + std::to_string(similarity) + ".xyz";
                std::ofstream PFO(temp_xyz_outfile);
                mutable_moving = moving;
                transform.apply_to(mutable_moving);
                if(!WritePointSetToXYZ(mutable_moving, PFO))
                    FUNCERR("Error writing point set to " << xyz_outfile);
            }
        }

        // Break loop if difference in objective function is below the threshold
        if (objective_tolerance < params.similarity_threshold || isnan(objective_tolerance) || isnan(sigma_squared)) {
            FUNCINFO("FINAL SIMILARITY: " << similarity);
            break;
        }

        // Write stats for this iteration to outfile_stats.csv
        high_resolution_clock::time_point stop = high_resolution_clock::now();
        duration<double>  time_span = duration_cast<duration<double>>(stop - time_sofar);
        FUNCINFO("Excecution took time: " << time_span.count())
        os << i+1 << "," << time_span.count() << "," << similarity << "," << temp_xyz_outfile << "\n";
    }
    return transform;
}
