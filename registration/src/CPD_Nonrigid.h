#ifndef CPDNONRIGID_H_
#define CPDNONRIGID_H_

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

#include "CPD_Shared.h"

#include <math.h>

class NonRigidCPDTransform {
    public:
        Eigen::MatrixXd G;
        Eigen::VectorXd W;
        int dim;
        NonRigidCPDTransform(int N_move_points, int dimensionality = 3);
        void apply_to(point_set<double> &ps);
        Eigen::MatrixXd apply_to(const Eigen::MatrixXd &ps);
        // Serialize and deserialize to a human- and machine-readable format.
        bool write_to( std::ostream &os );
        bool read_from( std::istream &is );
};

std::optional<NonRigidCPDTransform>
AlignViaNonRigidCPD(CPDParams & params,
            const point_set<double> & moving,
            const point_set<double> & stationary );

double Init_Sigma_Squared_NR(const Eigen::MatrixXd & xPoints,
            const Eigen::MatrixXd & yPoints);

Eigen::MatrixXd GetGramMatrix(const Eigen::MatrixXd & yPoints, double betaSquared);

Eigen::MatrixXd E_Step_NR(const Eigen::MatrixXd & xPoints,
            const Eigen::MatrixXd & yPoints,
            const Eigen::MatrixXd & gramMatrix,
            const Eigen::MatrixXd & W,
            double sigmaSquared,
            double w);

double GetSimilarity_NR(const Eigen::MatrixXd & xPoints,
            const Eigen::MatrixXd & yPoints,
            const Eigen::MatrixXd & postProb,
            const Eigen::MatrixXd & gramMatrix,
            const Eigen::MatrixXd & W,
            double sigmaSquared);

Eigen::MatrixXd GetW(const Eigen::MatrixXd & xPoints,
            const Eigen::MatrixXd & yPoints,
            const Eigen::MatrixXd & gramMatrix,
            const Eigen::MatrixXd & postProb,
            double sigmaSquared,
            double lambda);

Eigen::MatrixXd AlignedPointSet_NR(const Eigen::MatrixXd & yPoints,
            const Eigen::MatrixXd & gramMatrix,
            const Eigen::MatrixXd & W);

double SigmaSquared(const Eigen::MatrixXd & xPoints,
            const Eigen::MatrixXd & postProb,
            const Eigen::MatrixXd & transformedPoints);

#endif
