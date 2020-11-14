#include "CPD_Shared.h"

Eigen::MatrixXd E_Step(CPDParams & params
            const point_set<double> & moving
            const point_set<double> & stationary
            Eigen::MatrixXd & BR_Matrix){

    Eigen::MatrixXd P_Matrix(len(moving), len(stationary))
    
    
    // Pseudo-code for calculating the matrix P in the E-step
    
    for n in range(0, len(stationary)):
        for m in range(0, len(moving)):
            exp_arg = -1 / (2*sigma**2) * (stationary(n) BR_Matrix * moving(m) + t)**2 
            numer = exp(exp_arg)
            
            denom_sum = 0
            for k in range(0,len(moving)):
                exp_arg = -1 / (2*sigma**2) * (stationary(n) BR_Matrix * moving(k) + t)**2 
                denom_sum += exp(exp_arg)
            denom += (2*pi*sigma**2)**(D/2) * (w/(1-w)) * M / N

            matrix_p[m][n] = numer / denom
    return matrix_p
}