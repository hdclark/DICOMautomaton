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

<<<<<<< 95d34c915b56af925543d3814067d445958dde4e
=======
// The aim of the algorithm is to extract a transformation. Since we might want to apply this transformation to
// other objects (e.g., other point clouds, or images) we need to somehow return this transformation as a function
// that can be evaluated and passed around. A good way to do this is to split the transformation into a set of
// numbers and an algorithm that can make sense of the numbers. For example, a polynomial can be split into a set of
// coefficients and a generic algorithm that can be evaluated for any set of coefficients. Another example is a
// matrix, say an Affine matrix, which we can write to a file as a set of coefficients that can be applied to the
// positions of each point.
//
// However, actually extracting the algorithm may be an implementation detail. You should focus first on getting the
// deformable registration algorithm working first before worrying about how to extract the transformation.
struct CPDTransform {

};

>>>>>>> Added unit tests and header functions in Rigid
Eigen::MatrixXd E_Step(const Eigen::MatrixXd & xPoints,
            const Eigen::MatrixXd & yPoints,
            const Eigen::MatrixXd & BRMatrix,
            const Eigen::MatrixXd & t,
<<<<<<< 95d34c915b56af925543d3814067d445958dde4e
            double sigma,
            double w,
            double mRowsY,
            double nRowsX,
            double dimensionality);

Eigen::MatrixXd Center_Matrix(const Eigen::MatrixXd & points,
            const Eigen::MatrixXd & meanVector);

Eigen::MatrixXd Get_Translation_Vector(const Eigen::MatrixXd & rotationMatrix,
            const Eigen::MatrixXd & xMeanVector,
            const Eigen::MatrixXd & yMeanVector,
            float scale);
=======
            double sigmaSquared,
            double w);
>>>>>>> Added unit tests and header functions in Rigid

Eigen::MatrixXd Center_Matrix(const Eigen::MatrixXd & points,
            const Eigen::MatrixXd & meanVector);

Eigen::MatrixXd Get_Translation_Vector(const Eigen::MatrixXd & rotationMatrix,
            const Eigen::MatrixXd & xMeanVector,
            const Eigen::MatrixXd & yMeanVector,
            float scale);

 