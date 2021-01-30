#ifndef CPDSHARED_H_
#define CPDSHARED_H_

#include <Eigen/Dense>

// A copy of this structure will be passed to the algorithm. It should be used to set parameters, if there are any, that affect
// how the algorithm is performed. It generally should not be used to pass information back to the caller.
struct CPDParams {
    // Poiunt cloud dimesionality
    int dimensionality = 3;
    // Weight of the uniform distribution for the GMM
    // Must be between 0 and 1
    double distribution_weight = 0.2;
    // Smoothness regulation
    // Represents trade-off between goodness of fit and regularization
    double lambda = 2;
    // Smoothness regulation
    double beta = 2;
    // Max iterations for algorithm
    int iterations = 100;
    // Similarity termination threshold for algorithm
    double similarity_threshold = -15000;
};

double Init_Sigma_Squared(const Eigen::MatrixXd & xPoints,
            const Eigen::MatrixXd & yPoints);

Eigen::MatrixXd E_Step(const Eigen::MatrixXd & xPoints,
            const Eigen::MatrixXd & yPoints,
            const Eigen::MatrixXd & rotationMatrix,
            const Eigen::MatrixXd & t,
            double sigmaSquared,
            double w,
            double scale);

Eigen::MatrixXd CenterMatrix(const Eigen::MatrixXd & points,
            const Eigen::MatrixXd & meanVector);

Eigen::MatrixXd GetTranslationVector(const Eigen::MatrixXd & rotationMatrix,
            const Eigen::MatrixXd & xMeanVector,
            const Eigen::MatrixXd & yMeanVector,
            double scale);

Eigen::MatrixXd CalculateUx(const Eigen::MatrixXd & xPoints, 
            const Eigen::MatrixXd & postProb);

Eigen::MatrixXd CalculateUy(const Eigen::MatrixXd & yPoints, 
            const Eigen::MatrixXd & postProb);

Eigen::MatrixXd AlignedPointSet(const Eigen::MatrixXd & yPoints,
            const Eigen::MatrixXd & rotationMatrix,
            const Eigen::MatrixXd & translation,
            double scale);

double GetSimilarity(const Eigen::MatrixXd & xPoints,
            const Eigen::MatrixXd & yPoints,
            const Eigen::MatrixXd & rotationMatrix,
            const Eigen::MatrixXd & translation,
            double scale);

double GetObjective(const Eigen::MatrixXd & xPoints,
            const Eigen::MatrixXd & yPoints,
            const Eigen::MatrixXd & postProb,
            const Eigen::MatrixXd & rotationMatrix,
            const Eigen::MatrixXd & translation,
            double scale, 
            double sigmaSquared);
 
 #endif
