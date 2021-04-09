#ifndef CPDAFFINE_H_
#define CPDAFFINE_H_

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

class AffineCPDTransform {
    public:
        Eigen::MatrixXf B;
        Eigen::VectorXf t;
        int dim;
        AffineCPDTransform(int dimensionality);
        void apply_to(point_set<double> & ps);
        // Serialize and deserialize to a human- and machine-readable format.
        void write_to( std::ostream & os );
        bool read_from( std::istream & is );
};

AffineCPDTransform
AlignViaAffineCPD(CPDParams & params,
            const point_set<double> & moving,
            const point_set<double> & stationary,
            int iter_interval = 0,
            std::string video = "False",
            std::string xyz_outfile = "output" );

Eigen::MatrixXf
 CalculateB(const Eigen::MatrixXf & xHat,
            const Eigen::MatrixXf & yHat,
            const Eigen::MatrixXf & postProb );

double SigmaSquared(const Eigen::MatrixXf & B,
            const Eigen::MatrixXf & xHat,
            const Eigen::MatrixXf & yHat,
            const Eigen::MatrixXf & postProb);

#endif