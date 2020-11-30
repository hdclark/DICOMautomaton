#include "CPD_NonRigid.h"
#include <chrono>
using namespace std::chrono;

NonRigidCPDTransform::NonRigidCPDTransform(int dimensionality) {
    this->dim = dimensionality;
}

void NonRigidCPDTransform::apply_to(point_set<double> &ps) {
    const auto N_points = static_cast<long int>(ps.points.size());

    Eigen::MatrixXd Y = Eigen::MatrixXd::Zero(N_points, this->dim); 

}

double Init_Sigma_Squared(const Eigen::MatrixXd & xPoints,
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

Eigen::MatrixXd E_Step(const Eigen::MatrixXd & xPoints,
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
                          pow(2 * M_PI * sigmaSquared,((double)(dimensionality/2))) * (w/(1-w)) * (double)(mRowsY / nRowsX);
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
            double lambda){}

Eigen::MatrixXd TransformedPoints(const Eigen::MatrixXd & yPoints,
            const Eigen::MatrixXd & gramMatrix,
            const Eigen::MatrixXd & W){}

double SigmaSquared(const Eigen::MatrixXd & xPoints,
            const Eigen::MatrixXd & postProb,
            const Eigen::MatrixXd & transformedPoints){}

// std::optional<NonRigidCPDTransform>
// AlignViaNonRigidCPD(CPDParams & params,
//             const point_set<double> & moving,
//             const point_set<double> & stationary ) {       
// }

