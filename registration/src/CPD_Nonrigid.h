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

#include "CPD_Shared.h"
#include "IFGT.h"

#include <math.h>

class NonRigidCPDTransform {
    public:
        Eigen::MatrixXd G;
        Eigen::MatrixXd W;
        int dim;
        NonRigidCPDTransform(int N_move_points, int dimensionality = 3);
        void apply_to(point_set<double> &ps);
        Eigen::MatrixXd apply_to(const Eigen::MatrixXd &ps);
        // Serialize and deserialize to a human- and machine-readable format.
        void write_to( std::ostream &os );
        bool read_from( std::istream &is );
};

NonRigidCPDTransform
AlignViaNonRigidCPD(CPDParams & params,
            const point_set<double> & moving,
            const point_set<double> & stationary,
            int iter_interval = 0,
            std::string video = "False",
            std::string xyz_outfile = "output" );

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
            const Eigen::MatrixXd & gramMatrix,
            const Eigen::MatrixXd & W);

double GetObjective_NR(const Eigen::MatrixXd & xPoints,
            const Eigen::MatrixXd & yPoints,
            const Eigen::MatrixXd & postProb,
            const Eigen::MatrixXd & gramMatrix,
            const Eigen::MatrixXd & W,
            double sigmaSquared);

Eigen::MatrixXd GetW(const Eigen::MatrixXd & yPoints,
            const Eigen::MatrixXd & gramMatrix,
            const Eigen::MatrixXd & postProbOne,
            const Eigen::MatrixXd & postProbX,
            double sigmaSquared,
            double lambda);

Eigen::MatrixXd LowRankGetW(const Eigen::MatrixXd & yPoints,
            const Eigen::VectorXd & gramValues,
            const Eigen::MatrixXd & gramVectors,
            const Eigen::MatrixXd & postProbOne,
            const Eigen::MatrixXd & postProbX,
            double sigmaSquared,
            double lambda);

Eigen::MatrixXd AlignedPointSet_NR(const Eigen::MatrixXd & yPoints,
            const Eigen::MatrixXd & gramMatrix,
            const Eigen::MatrixXd & W);

double SigmaSquared(const Eigen::MatrixXd & xPoints,
            const Eigen::MatrixXd & postProbOne,
            const Eigen::MatrixXd & postProbTransOne,
            const Eigen::MatrixXd & postProbX,
            const Eigen::MatrixXd & transformedPoints);

void GetNLargestEigenvalues_V2(const Eigen::MatrixXd & m,
            Eigen::MatrixXd & vector_matrix,
            Eigen::VectorXd & value_matrix,
            int num_eig,
            int size);

void GetNLargestEigenvalues(const Eigen::MatrixXd & m,
            Eigen::MatrixXd & vector_matrix,
            Eigen::VectorXd & value_matrix,
            int num_eig,
            int size,
            int power_iter,
            double power_tol);

double PowerIteration(const Eigen::MatrixXd & m,
            Eigen::VectorXd & v, 
            int num_iter,
            double tolerance);

// matrix vector products P1, Pt1, PX
struct CPD_MatrixVector_Products {
    Eigen::MatrixXd P1; 
    Eigen::MatrixXd Pt1; 
    Eigen::MatrixXd PX; 
    double L;
};

CPD_MatrixVector_Products compute_cpd_products_ifgt(const Eigen::MatrixXd & source_pts,
                                                    const Eigen::MatrixXd & target_pts,
                                                    double bandwidth, 
                                                    double epsilon,
                                                    double w);

CPD_MatrixVector_Products compute_cpd_products_naive(const Eigen::MatrixXd & fixed_pts,
                                                    const Eigen::MatrixXd & moving_pts,
                                                    double sigmaSquared, 
                                                    double w);

#endif
