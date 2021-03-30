
/* 
 * Improved Fast Gauss Transform implementation based on FIGTREE 
 * implementation by Vlad Morariu, the IFGT source code by Vikas Raykar
 * and Changjiang Yang, as well as fgt library by Pete Gadomski
 */

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
#include <chrono>

#include <cstdlib>            //Needed for exit() calls.
#include <utility>            //Needed for std::pair.


#include <boost/filesystem.hpp> // Needed for Boost filesystem access.

#include <eigen3/Eigen/Dense> //Needed for Eigen library dense matrices.

// matrix vector products P1, Pt1, PX
struct CPD_MatrixVector_Products {
    Eigen::MatrixXd P1; 
    Eigen::MatrixXd Pt1; 
    Eigen::MatrixXd PX; 
};

CPD_MatrixVector_Products compute_cpd_products(const Eigen::MatrixXd & source_pts,
                                            const Eigen::MatrixXd & target_pts,
                                            double bandwidth, 
                                            double epsilon,
                                            double w);

struct Cluster {
   // int num_clusters;
    Eigen::MatrixXd k_centers;
    Eigen::VectorXd radii;
    Eigen::VectorXd assignments;
    Eigen::VectorXd distances;
    double rx_max;
};

Cluster k_center_clustering(const Eigen::MatrixXd & points, int num_clusters);

std::pair<double, double> calc_max_range(const Eigen::MatrixXd & target_pts,
                                            const Eigen::MatrixXd & source_pts);

double rescale_points(const Eigen::MatrixXd & fixed_pts,
                        const Eigen::MatrixXd & moving_pts,
                        Eigen::MatrixXd & fixed_pts_scaled,
                        Eigen::MatrixXd & moving_pts_scaled,
                        double bandwidth);

//void cluster_increment(const Eigen::MatrixXd & points, std::unique_ptr<Cluster> & );

class IFGT {
    public:
        // constructor
        IFGT(const Eigen::MatrixXd & source_pts,
            double bandwidth, 
            double epsilon);

        // computes ifgt with constant weight of 1
        Eigen::MatrixXd compute_ifgt(const Eigen::MatrixXd & target_pts);
        // computes ifgt with given weights 
        Eigen::MatrixXd compute_ifgt(const Eigen::MatrixXd & target_pts, 
                                const Eigen::ArrayXd & weights);
        
        int get_nclusters() const { return n_clusters; }

    private: 
        int dim;
        int max_truncation_p; // max truncation number (p)
        int p_max_total; // length of monomials after multi index expansion
        const double bandwidth; 
        const double epsilon;
        int n_clusters;
        double cutoff_radius;
        const Eigen::MatrixXd source_pts;
        Eigen::MatrixXd constant_series;
        std::unique_ptr<Cluster> cluster; 


        // autotuning of ifgt parameters
        void ifgt_choose_parameters();

        // after clustering, reupdate the truncation based on actual max 
        void ifgt_update_max_truncation(double rx_max); 

        // computes 2^alpha/alpha!
        void compute_constant_series(); 

        // computes [(y_j-c_k)/h]^{alpha} or [(x_i-c_k)/h]^{alpha}
        // where y_j is target point, x_i is point inside cluster
        Eigen::VectorXd compute_monomials(const Eigen::MatrixXd & delta);

        // computes constant ck for each cluster with constant weights
        Eigen::MatrixXd compute_ck(const Eigen::ArrayXd & weights);

        // computes G(yj) - the actual gaussian
        Eigen::MatrixXd compute_gaussian(const Eigen::MatrixXd & target_pts,
                                            const Eigen::MatrixXd & C_k);
        // computes gauss transform naively if the estimated computational complexity
        // is lower than IFGT
        Eigen::MatrixXd compute_naive(const Eigen::MatrixXd & target_pts, 
                                            const Eigen::ArrayXd & weights);

        double compute_complexity(int M_target_pts);

};


int nchoosek(int n, int k);



#endif