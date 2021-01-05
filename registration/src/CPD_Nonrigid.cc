#include "CPD_Nonrigid.h"
#include <chrono>
using namespace std::chrono;

NonRigidCPDTransform::NonRigidCPDTransform(int N_move_points, int dimensionality) {
    this->dim = dimensionality;
    this->W = Eigen::MatrixXd::Zero(N_move_points, this->dim); 
}

void NonRigidCPDTransform::apply_to(point_set<double> &ps) {
    FUNCINFO("Applying transform to point set")
    auto N_points = static_cast<long int>(ps.points.size());
    Eigen::MatrixXd Y = Eigen::MatrixXd::Zero(N_points, this->dim); 
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

Eigen::MatrixXd NonRigidCPDTransform::apply_to(const Eigen::MatrixXd &ps) {
    return ps + this->G + this->W;
}

double NR_Init_Sigma_Squared(const Eigen::MatrixXd & xPoints,
            const Eigen::MatrixXd & yPoints) {

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

    return normSum / (nRowsX * mRowsY * dim);
}

Eigen::MatrixXd GetGramMatrix(const Eigen::MatrixXd & yPoints, double betaSquared) {
    int mRowsY = yPoints.rows();
    double expArg;
    Eigen::MatrixXd gramMatrix = Eigen::MatrixXd::Zero(mRowsY,mRowsY);
    Eigen::MatrixXd tempVector;
    
    for (size_t i = 0; i < mRowsY; ++i) {
        for (size_t j = 0; j < mRowsY; ++j) {
            tempVector = yPoints.row(i) - yPoints.row(j);
            expArg = - 1 / (2 * betaSquared) * tempVector.squaredNorm();
            gramMatrix(i,j) = exp(expArg);
        }
    }

    return gramMatrix;
}

double GetSimilarity_NR(const Eigen::MatrixXd & xPoints,
            const Eigen::MatrixXd & yPoints,
            const Eigen::MatrixXd & postProb,
            const Eigen::MatrixXd & gramMatrix,
            const Eigen::MatrixXd & W,
            double sigmaSquared) {
    
    int mRowsY = yPoints.rows();
    int nRowsX = xPoints.rows(); 
    double dimensionality = xPoints.cols();
    double Np = postProb.sum();
    Eigen::MatrixXd tempVector;

    double leftSum = 0;
    for (size_t m = 0; m < mRowsY; ++m) {
        for (size_t n = 0; n < nRowsX; ++n) {
            tempVector = xPoints.row(n) - AlignedPointSet_NR(yPoints, gramMatrix, W).row(m);
            leftSum += postProb(m,n) * tempVector.squaredNorm();
        }
    }
    leftSum = leftSum / (2.0 * sigmaSquared);
    double rightSum = Np * dimensionality / 2.0 * log(sigmaSquared);
    return leftSum + rightSum;
}

Eigen::MatrixXd E_Step_NR(const Eigen::MatrixXd & xPoints,
            const Eigen::MatrixXd & yPoints,
            const Eigen::MatrixXd & gramMatrix,
            const Eigen::MatrixXd & W,
            double sigmaSquared,
            double w) {
    Eigen::MatrixXd postProb = Eigen::MatrixXd::Zero(yPoints.rows(),xPoints.rows());
    Eigen::MatrixXd expMat = Eigen::MatrixXd::Zero(yPoints.rows(),xPoints.rows());

    Eigen::MatrixXd tempVector;
    double expArg;
    double numerator;
    double denominator;

    int mRowsY = yPoints.rows();
    int nRowsX = xPoints.rows();
    int dimensionality = yPoints.cols();

    // std::cout << "\n";
    // std::cout << "\n";    
    auto start = high_resolution_clock::now();
    for (size_t m = 0; m < mRowsY; ++m) {
        for (size_t n = 0; n < nRowsX; ++n) {
            tempVector = xPoints.row(n) - (yPoints.row(m) + gramMatrix.row(m) * W);
            expArg = - 1 / (2 * sigmaSquared) * tempVector.squaredNorm();
            expMat(m,n) = exp(expArg);
        }
    }

    auto stop = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(stop - start); 
    // std::cout << duration.count() << std::endl; 

    for (size_t m = 0; m < mRowsY; ++m) {
        for (size_t n = 0; n < nRowsX; ++n) {
            numerator = expMat(m,n);
            denominator = expMat.col(n).sum() + 
                          pow((2 * M_PI * sigmaSquared),((double)(dimensionality/2.0))) * (w/(1-w)) * (double)(mRowsY / nRowsX);
            postProb(m,n) = numerator / denominator;
        }
    }

    stop = high_resolution_clock::now();
    duration = duration_cast<microseconds>(stop - start); 
    // std::cout << duration.count() << std::endl; 
    // std::cout << "\n";
    // std::cout << "\n";
    
    return postProb;
}

Eigen::MatrixXd GetW(const Eigen::MatrixXd & xPoints,
            const Eigen::MatrixXd & yPoints,
            const Eigen::MatrixXd & gramMatrix,
            const Eigen::MatrixXd & postProb,
            double sigmaSquared,
            double lambda){
    
    Eigen::MatrixXd oneVec = Eigen::MatrixXd::Ones(postProb.rows(),1);
    Eigen::MatrixXd postProbInvDiag = ((postProb * oneVec).asDiagonal()).inverse(); // d(P1)^-1
    Eigen::MatrixXd A = gramMatrix + lambda * sigmaSquared * postProbInvDiag;
    Eigen::MatrixXd b = postProbInvDiag * postProb * xPoints - yPoints;

    return A.llt().solve(b); // assumes A is positive definite, uses llt decomposition
}

double SigmaSquared(const Eigen::MatrixXd & xPoints,
            const Eigen::MatrixXd & postProb,
            const Eigen::MatrixXd & transformedPoints){

    int dim = xPoints.cols();
    double Np = postProb.sum();

    Eigen::MatrixXd oneVec = Eigen::MatrixXd::Ones(postProb.rows(),1);
    double firstTerm = (double)(xPoints.transpose() * (postProb.transpose() * oneVec).asDiagonal() * xPoints).trace();
    double secondTerm = (double)(2 * ((postProb * xPoints).transpose() * transformedPoints).trace());
    double thirdTerm = (double)(transformedPoints.transpose() * (postProb * oneVec).asDiagonal() * transformedPoints).trace();

    return (firstTerm - secondTerm + thirdTerm) / (Np * dim);
}

std::optional<NonRigidCPDTransform>
AlignViaNonRigidCPD(CPDParams & params,
            const point_set<double> & moving,
            const point_set<double> & stationary ) { 
    Eigen::MatrixXd GetGramMatrix(const Eigen::MatrixXd & yPoints, double betaSquared);
    const auto N_move_points = static_cast<long int>(moving.points.size());
    const auto N_stat_points = static_cast<long int>(stationary.points.size());

    // Prepare working buffers.
    //
    // Stationary point matrix
    Eigen::MatrixXd X = Eigen::MatrixXd::Zero(N_move_points, params.dimensionality);
    // Moving point matrix
    Eigen::MatrixXd Y = Eigen::MatrixXd::Zero(N_stat_points, params.dimensionality); 

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
    double sigma_squared = NR_Init_Sigma_Squared(X, Y);
    transform.G = GetGramMatrix(Y, params.beta * params.beta);
    Eigen::MatrixXd P;
    Eigen::MatrixXd T;

    for (int i = 0; i < params.iterations; i++) {
        P = E_Step_NR(X, Y, transform.G, transform.W, sigma_squared, params.distribution_weight);
        transform.W = GetW(X, Y, transform.G, P, sigma_squared, params.lambda);
        T = transform.apply_to(Y);
        SigmaSquared(X, P, T);
    }
}
