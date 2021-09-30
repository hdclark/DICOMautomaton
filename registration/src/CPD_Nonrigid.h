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
        Eigen::MatrixXf G;
        Eigen::MatrixXf W;
        int dim;
        NonRigidCPDTransform(int N_move_points, int dimensionality = 3);
        void apply_to(point_set<double> &ps);
        Eigen::MatrixXf apply_to(const Eigen::MatrixXf &ps);
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

double Init_Sigma_Squared_NR(const Eigen::MatrixXf & xPoints,
            const Eigen::MatrixXf & yPoints);

Eigen::MatrixXf GetGramMatrix(const Eigen::MatrixXf & yPoints, double betaSquared);

Eigen::MatrixXf E_Step_NR(const Eigen::MatrixXf & xPoints,
            const Eigen::MatrixXf & yPoints,
            const Eigen::MatrixXf & gramMatrix,
            const Eigen::MatrixXf & W,
            double sigmaSquared,
            double w);

double GetSimilarity_NR(const Eigen::MatrixXf & xPoints,
            const Eigen::MatrixXf & yPoints,
            const Eigen::MatrixXf & gramMatrix,
            const Eigen::MatrixXf & W);

double GetObjective_NR(const Eigen::MatrixXf & xPoints,
            const Eigen::MatrixXf & yPoints,
            const Eigen::MatrixXf & postProb,
            const Eigen::MatrixXf & gramMatrix,
            const Eigen::MatrixXf & W,
            double sigmaSquared);

Eigen::MatrixXf GetW(const Eigen::MatrixXf & yPoints,
            const Eigen::MatrixXf & gramMatrix,
            const Eigen::MatrixXf & postProbInvDiag,
            const Eigen::MatrixXf & postProbX,
            double sigmaSquared,
            double lambda);

Eigen::MatrixXf LowRankGetW(const Eigen::MatrixXf & yPoints,
            const Eigen::VectorXf & gramValues,
            const Eigen::MatrixXf & gramVectors,
            const Eigen::MatrixXf & postProbOne,
            const Eigen::MatrixXf & postProbX,
            double sigmaSquared,
            double lambda);

Eigen::MatrixXf AlignedPointSet_NR(const Eigen::MatrixXf & yPoints,
            const Eigen::MatrixXf & gramMatrix,
            const Eigen::MatrixXf & W);

double SigmaSquared(const Eigen::MatrixXf & xPoints,
            const Eigen::MatrixXf & postProbOne,
            const Eigen::MatrixXf & postProbTransOne,
            const Eigen::MatrixXf & postProbX,
            const Eigen::MatrixXf & transformedPoints);

void GetNLargestEigenvalues_V2(const Eigen::MatrixXf & m,
            Eigen::MatrixXf & vector_matrix,
            Eigen::VectorXf & value_matrix,
            int num_eig,
            int size);

void GetNLargestEigenvalues(const Eigen::MatrixXf & m,
            Eigen::MatrixXf & vector_matrix,
            Eigen::VectorXf & value_matrix,
            int num_eig,
            int size,
            int power_iter,
            double power_tol);

double PowerIteration(const Eigen::MatrixXf & m,
            Eigen::VectorXf & v, 
            int num_iter,
            double tolerance);

// matrix vector products P1, Pt1, PX
struct CPD_MatrixVector_Products {
    Eigen::MatrixXf P1; 
    Eigen::MatrixXf Pt1; 
    Eigen::MatrixXf PX; 
    double L;
};

CPD_MatrixVector_Products ComputeCPDProductsIfgt(const Eigen::MatrixXf & xPoints,
                                                    const Eigen::MatrixXf & yPoints,
                                                    double sigmaSquared, 
                                                    double epsilon,
                                                    double w);

CPD_MatrixVector_Products ComputeCPDProductsNaive(const Eigen::MatrixXf & xPoints,
                                                    const Eigen::MatrixXf & yPoints,
                                                    double sigmaSquared, 
                                                    double w);

double UpdateNaiveConvergenceL(const Eigen::MatrixXf & postProbTransOne,
                            const Eigen::MatrixXf & xPoints,
                            const Eigen::MatrixXf & yPoints,
                            double sigmaSquared,
                            double w,
                            int dim);

double UpdateConvergenceL(const Eigen::MatrixXf & gramMatrix,
                        const Eigen::MatrixXf & W,
                        double L_computed,
                        double lambda);        

#endif
