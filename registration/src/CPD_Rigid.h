#pragma once

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
AlignViaRigidCPD(CPDParams & params,
            const point_set<double> & moving,
            const point_set<double> & stationary );

double CalculateS(const Eigen::MatrixXd & A,
            const Eigen::MatrixXd & R,
            const Eigen::MatrixXd & yHat,
            const Eigen::MatrixXd & postProb );

double SigmaSquared(double Np,
            double s,
            const Eigen::MatrixXd & A,
            const Eigen::MatrixXd & R,
            const Eigen::MatrixXd & xHat,
            const Eigen::MatrixXd & postProb);