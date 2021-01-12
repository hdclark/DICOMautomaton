#ifndef CPDRIGID_H_
#define CPDRIGID_H_

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

#include "CPD_Shared.h"

class RigidCPDTransform {
    public:
        Eigen::MatrixXd R;
        Eigen::VectorXd t;
        int dim;
        float s;
        RigidCPDTransform(int dimensionality = 3);
        Eigen::MatrixXd get_sR();
        void apply_to(point_set<double> &ps);
        // Serialize and deserialize to a human- and machine-readable format.
        bool write_to( std::ostream &os );
        bool read_from( std::istream &is );
};

RigidCPDTransform
AlignViaRigidCPD(CPDParams & params,
            const point_set<double> & moving,
            const point_set<double> & stationary );

Eigen::MatrixXd GetA(const Eigen::MatrixXd & xHat,
            const Eigen::MatrixXd & yHat,
            const Eigen::MatrixXd & postProb);

// Please calculate C inside this function
Eigen::MatrixXd GetRotationMatrix(const Eigen::MatrixXd & U,
            const Eigen::MatrixXd & V);

double GetS(const Eigen::MatrixXd & A,
            const Eigen::MatrixXd & R,
            const Eigen::MatrixXd & yHat,
            const Eigen::MatrixXd & postProb );

double SigmaSquared(double s,
            const Eigen::MatrixXd & A,
            const Eigen::MatrixXd & R,
            const Eigen::MatrixXd & xHat,
            const Eigen::MatrixXd & postProb);

#endif