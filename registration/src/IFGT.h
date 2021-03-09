#ifndef IFGT_H_
#define IFGT_H_

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

class IFGT {
    public:
        // constructor
        IFGT(const Eigen::MatrixXd & source_points,
            double bandwidth, 
            double epsilon);

        Eigen::MatrixXd run_ifgt(const Eigen::MatrixXd & source_points,
                                const Eigen::MatrixXd & target_points);
        
    private: 
        int dim;
        int max_truncation; // max truncation number (p)
        int p_max_total; // length of monomials after multi index expansion
        std::unique_ptr<Cluster> cluster; 
        double bandwidth; 
        double epsilon;
        int n_clusters;
        Eigen::MatrixXd constant_series;
        double cutoff_radius;


        // autotuning of ifgt parameters
        void ifgt_choose_parameters();

        // after clustering, reupdate the truncation based on actual max 
        void ifgt_update_max_truncation(double rx_max); 

        // computes 2^alpha/alpha!
        void compute_constant_series(); 

        // computes [(y_j-c_k)/h]^{alpha} or [(x_i-c_k)/h]^{alpha}
        // where y_j is target point, x_i is point inside cluster
        Eigen::MatrixXd compute_monomials(const Eigen::MatrixXd & delta);

        // computes constant ck for each cluster 
        Eigen::MatrixXd compute_ck(const Eigen::MatrixXd & source_points);

        // computes G(yj) - the actual gaussian
        Eigen::MatrixXd IFGT::compute_gaussian(const Eigen::MatrixXd & target_points,
                                            const Eigen::MatrixXd & C_k);

};


int nchoosek(int n, int k);



#endif