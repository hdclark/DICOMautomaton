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
    int iterations = 50;
    // Similarity termination threshold for algorithm
    double similarity_threshold = 1;
    // Use low-rank approximation
    bool use_low_rank = false;
    double ev_ratio = 0;
    int power_iter = 1000;
    double power_tol = 0.000001;
    //FGT
    bool use_fgt = false;
};

double Init_Sigma_Squared(const Eigen::MatrixXf & xPoints,
            const Eigen::MatrixXf & yPoints);

Eigen::MatrixXf E_Step(const Eigen::MatrixXf & xPoints,
            const Eigen::MatrixXf & yPoints,
            const Eigen::MatrixXf & rotationMatrix,
            const Eigen::MatrixXf & t,
            double sigmaSquared,
            double w,
            double scale);

Eigen::MatrixXf CenterMatrix(const Eigen::MatrixXf & points,
            const Eigen::MatrixXf & meanVector);

Eigen::MatrixXf GetTranslationVector(const Eigen::MatrixXf & rotationMatrix,
            const Eigen::MatrixXf & xMeanVector,
            const Eigen::MatrixXf & yMeanVector,
            double scale);

Eigen::MatrixXf CalculateUx(const Eigen::MatrixXf & xPoints, 
            const Eigen::MatrixXf & postProb);

Eigen::MatrixXf CalculateUy(const Eigen::MatrixXf & yPoints, 
            const Eigen::MatrixXf & postProb);

Eigen::MatrixXf AlignedPointSet(const Eigen::MatrixXf & yPoints,
            const Eigen::MatrixXf & rotationMatrix,
            const Eigen::MatrixXf & translation,
            double scale);

double GetSimilarity(const Eigen::MatrixXf & xPoints,
            const Eigen::MatrixXf & yPoints,
            const Eigen::MatrixXf & rotationMatrix,
            const Eigen::MatrixXf & translation,
            double scale);

double GetObjective(const Eigen::MatrixXf & xPoints,
            const Eigen::MatrixXf & yPoints,
            const Eigen::MatrixXf & postProb,
            const Eigen::MatrixXf & rotationMatrix,
            const Eigen::MatrixXf & translation,
            double scale, 
            double sigmaSquared);
 
 #endif
