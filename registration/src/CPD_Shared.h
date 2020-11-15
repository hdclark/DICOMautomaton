#include <exception>
#include <functional>
#include <optional>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <string>    
#include <vector>

#include <cstdlib>            //Needed for exit() calls.
#include <utility>            //Needed for std::pair.

#include "YgorMath.h"         //Needed for samples_1D.

#include <math.h>
#include <Eigen/Dense>

#include <math.h>
#include <Eigen/Dense>

// A copy of this structure will be passed to the algorithm. It should be used to set parameters, if there are any, that affect
// how the algorithm is performed. It generally should not be used to pass information back to the caller.
struct CPDParams {
    // Poiunt cloud dimesionality
    int dimensionality = 3;
    // Weight of the uniform distribution for the GMM
    // Must be between 0 and 1
    double distribution_weight = 0.5;
    // Max iterations for algorithm
    int iterations = 50;
};

float Init_Sigma_Squared(const Eigen::MatrixXd & xPoints,
            const Eigen::MatrixXd & yPoints);

Eigen::MatrixXd E_Step(const Eigen::MatrixXd & xPoints,
            const Eigen::MatrixXd & yPoints,
            const Eigen::MatrixXd & BRMatrix,
            const Eigen::MatrixXd & t,
            double sigma,
            double w,
            double mRowsY,
            double nRowsX,
            double dimensionality);
