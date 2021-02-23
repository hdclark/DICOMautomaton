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


// #include <boost/filesystem.hpp> // Needed for Boost filesystem access.

// #include <eigen3/Eigen/Dense> //Needed for Eigen library dense matrices.


struct Ifgt_params { 
    int nclusters; // number of clusters 
    int p_max; // max truncation 
};

// autotuning of ifgt parameters
Ifgt_params ifgt_choose_parameters(int dimensionality, 
                        double bandwidth, 
                        double epsilon);
/*
Eigen::MatrixXd run_ifgt(Ifgt_params parameters, 
                        const Eigen::MatrixXd & points, 
                        double bandwidth, 
                        double epsilon);
*/
// after clustering, reupdate the truncation based on actual max 
int ifgt_update_max_truncation(int dimensionality, 
                            double bandwidth, 
                            double epsilon,
                            double rx_max); 
/*
// computes 2^alpha/alpha!
Eigen::MatrixXd compute_constant_series(int dimensionality, 
                                        int pmax_total); 

// computes [(y_j-c_k)/h]^{alpha} or [(x_i-c_k)/h]^{alpha}
// where y_j is target point, x_i is point inside cluster
Eigen::MatrixXd compute_monomials(int dimensionality,
                                        double bandwidth,
                                        const Eigen::MatrixXd & delta); // d is either (y_j-c_k) or (x_i-c_k)


// computes (y_j-c_k) or (x_i-c_k)
Eigen::MatrixXd compute_d(int dimensionality,
                        const Eigen::MatrixXd & cluster_centre,
                        const Eigen::MatrixXd & point);

// computes constant ck for each cluster 
Eigen::MatrixXd compute_ck(int dimensionality,
                        double bandwidth
                        int pmax_total,
                        int p_max
                        int n_clusters,
                        Cluster * clusters,
                        const Eigen::MatrixXd & points);
*/
int nchoosek(int n, int k);



#endif