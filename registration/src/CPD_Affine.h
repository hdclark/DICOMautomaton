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
// #include "CPD_Shared.h"

std::optional<CPDTransform>
AlignViaAffineCPD(CPDParams & params,
            const point_set<double> & moving,
            const point_set<double> & stationary );

Eigen::MatrixXd calculate_B(CPDParams & params,
            const point_set<double> & x_hat,
            const point_set<double> & y_hat,
            const Eigen::MatrixXd & post_prob;

double sigma_squared(CPDParams & params,
            float & Np,
            float & dimensionality,
            const Eigen::MatrixXd & B,
            const point_set<double> & x_hat,
            const point_set<double> & y_hat);