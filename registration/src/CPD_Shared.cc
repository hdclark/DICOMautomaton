#include "CPD_Shared.h"

Eigen::MatrixXd E_Step(const Eigen::MatrixXd & xPoints,
            const Eigen::MatrixXd & yPoints,
            const Eigen::MatrixXd & BRMatrix,
            const Eigen::MatrixXd & t,
            double sigma,
            double w,
            double mRowsY,
            double nRowsX,
            double dimensionality) {

    Eigen::MatrixXd postProb = Eigen::MatrixXd::Zero(yPoints.rows(),xPoints.rows());

    Eigen::MatrixXd tempVector;
    double expArg;
    double numerator;
    double denomSum;
    double denominator;
    
    for (size_t m = 0; m < mRowsY; ++m) {
        for (size_t n = 0; n < nRowsX; ++n) {

            tempVector = xPoints.row(n) - (BRMatrix * yPoints.row(m) + t);
            expArg = - 1 / (2 * pow(sigma, 2)) * pow(tempVector.norm(),2);
            numerator = exp(expArg);
            
            denomSum = 0;
            
            for (size_t k = 0; k < mRowsY; ++k) {
                tempVector = xPoints.row(n) - (BRMatrix * yPoints.row(k) + t);
                expArg = - 1 / (2 * pow(sigma, 2)) * pow(tempVector.norm(),2);
                denomSum += exp(expArg);
            }

            // Check if there is an issue with pi using math.h
            denominator = denomSum + pow(2 * M_PI * pow(sigma, 2),((double)(dimensionality/2))) * (w/(1-w)) * (double)(mRowsY / nRowsX);

            postProb(m,n) = numerator / denominator;

        }
    }

    return postProb;

}