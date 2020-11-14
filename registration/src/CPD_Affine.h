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

#include <boost/filesystem.hpp> // Needed for Boost filesystem access.

#include <eigen3/Eigen/Dense> //Needed for Eigen library dense matrices.

#include "YgorFilesDirs.h"    //Needed for Does_File_Exist_And_Can_Be_Read(...), etc..
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorMath.h"         //Needed for samples_1D.
#include "YgorString.h"       //Needed for GetFirstRegex(...)
// CPD_Shared.h file
#ifndef CPDSHARED_H_
#define CPDSHARED_H_
#include "CPD_Shared.h"
#endif

std::optional<CPDTransform>
AlignViaAffineCPD(CPDParams & params,
            const point_set<double> & moving,
            const point_set<double> & stationary );

<<<<<<< 9bc73d4eff9fb1cc10dd15e17bbb430712d199df
Eigen::MatrixXd calculate_B(const Eigen::MatrixXd & xHat,
            const Eigen::MatrixXd & yHat,
            const Eigen::MatrixXd & postProb );

double sigma_squared(CPDParams & params,
            double Np,
            double dimensionality,
            const Eigen::MatrixXd & B,
            const Eigen::MatrixXd & xHat,
            const Eigen::MatrixXd & yHat,
            const Eigen::MatrixXd & postProb );
=======
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
>>>>>>> Added some function headers for affine and E-step
