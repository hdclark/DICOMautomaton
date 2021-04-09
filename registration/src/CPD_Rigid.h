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
        Eigen::MatrixXf R;
        Eigen::MatrixXf t;
        int dim;
        float s;
        RigidCPDTransform(int dimensionality = 3);
        Eigen::MatrixXf get_sR();
        void apply_to(point_set<double> &ps);
        // Serialize and deserialize to a human- and machine-readable format.
        void write_to( std::ostream &os );
        bool read_from( std::istream &is );
};

RigidCPDTransform
AlignViaRigidCPD(CPDParams & params,
            const point_set<double> & moving,
            const point_set<double> & stationary,
            int iter_interval = 0,
            std::string video = "False",
            std::string xyz_outfile = "output" );

Eigen::MatrixXf GetA(const Eigen::MatrixXf & xHat,
            const Eigen::MatrixXf & yHat,
            const Eigen::MatrixXf & postProb);

// Please calculate C inside this function
Eigen::MatrixXf GetRotationMatrix(const Eigen::MatrixXf & U,
            const Eigen::MatrixXf & V);

double GetS(const Eigen::MatrixXf & A,
            const Eigen::MatrixXf & R,
            const Eigen::MatrixXf & yHat,
            const Eigen::MatrixXf & postProb );

double SigmaSquared(double s,
            const Eigen::MatrixXf & A,
            const Eigen::MatrixXf & R,
            const Eigen::MatrixXf & xHat,
            const Eigen::MatrixXf & postProb);

#endif