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

#ifndef CPDAFFINE_H_
#define CPDAFFINE_H_
#endif

class AffineCPDTransform {
    public:
        Eigen::MatrixXd B;
        Eigen::VectorXd t;
        int dim;
        AffineCPDTransform(int dimensionality = 3);
        void apply_to(point_set<double> &ps);
        // Serialize and deserialize to a human- and machine-readable format.
        bool write_to( std::ostream &os );
        bool read_from( std::istream &is );
};

std::optional<AffineCPDTransform>
AlignViaAffineCPD(CPDParams & params,
            const point_set<double> & moving,
            const point_set<double> & stationary );

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