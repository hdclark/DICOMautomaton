
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

Eigen::MatrixXf compute_naive_gt(const Eigen::MatrixXf & target_pts, 
                                const Eigen::MatrixXf & source_pts,
                                const Eigen::ArrayXf & weights,
                                double bandwidth);

struct Cluster {
    Eigen::MatrixXf k_centers; 
    Eigen::VectorXf radii;
    Eigen::VectorXf assignments;
    Eigen::VectorXf distances;
    double rx_max;
};
// Gonzalez' k-centre clustering algorithm 
// outputs struct Cluster
Cluster k_center_clustering(const Eigen::MatrixXf & points, int num_clusters);

// calculates the maximum and minimum values of the two pointsets
// used to normalize the points - normalizes overly conservative but it should not 
// affect the final result
std::pair<double, double> calc_max_range(const Eigen::MatrixXf & target_pts,
                                            const Eigen::MatrixXf & source_pts);

// rescales based on max/min points of point sets. Assumes both 
// point sets are spatially linked, so it preserves the relative 
// distance between the two point sets
double rescale_points(const Eigen::MatrixXf & fixed_pts,
                        const Eigen::MatrixXf & moving_pts,
                        Eigen::MatrixXf & fixed_pts_scaled,
                        Eigen::MatrixXf & moving_pts_scaled,
                        double bandwidth);

// Improved Fast Gauss Transform Class
// Depending on the estimated complexity of the IFGT, it will determine whether to
// calculate the Gauss transform naively or using the IFGT algorithm
class IFGT {
    public:
        // constructor precomputes many different things to speed up subsequent
        // runs. Useful in particular for P1 and PX where the same IFGT object
        // is used multiple times, so we don't waste time reclustering, etc.

        IFGT(const Eigen::MatrixXf & source_pts,
            double bandwidth, // larger bandwidth = more speedup compared to smallere bandwidth (in general)
            double epsilon); // in general, epsilon = 1E-3 - 1E-6 is good enough for the majority
                             // of applications. Anything less than 1E-8 is pretty overkill
                             

        // The only function you need to call for IFGT
        // computes IFGT with constant weight of 1
        Eigen::MatrixXf compute_ifgt(const Eigen::MatrixXf & target_pts);

        // computes ifgt with given weights 
        Eigen::MatrixXf compute_ifgt(const Eigen::MatrixXf & target_pts, 
                                const Eigen::ArrayXf & weights);
        
        int get_nclusters() const { return n_clusters; }

    private: 
        const Eigen::MatrixXf source_pts;
        const double bandwidth; 
        const double epsilon;
        const int dim;
        int max_truncation_p; // max truncation number (p)
        int p_max_total; // length of monomials after multi index expansion
        int n_clusters;
        double cutoff_radius;
        Eigen::MatrixXf constant_series;
        std::unique_ptr<Cluster> cluster; 


        // autotuning of ifgt parameters
        void ifgt_choose_parameters();

        // after clustering, reupdate the truncation based on actual max cluster radius 
        void ifgt_update_max_truncation(double rx_max); 

        // computes 2^alpha/alpha!, done in constructor
        void compute_constant_series(); 

        // computes [(y_j-c_k)/h]^{alpha} or [(x_i-c_k)/h]^{alpha}
        // where y_j is target point, x_i is point inside cluster
        Eigen::VectorXf compute_monomials(const Eigen::MatrixXf & delta);

        // computes constant ck for each cluster with constant weights
        Eigen::MatrixXf compute_ck(const Eigen::ArrayXf & weights);

        // computes G(yj) - the actual gaussian
        Eigen::MatrixXf compute_gaussian(const Eigen::MatrixXf & target_pts,
                                            const Eigen::MatrixXf & C_k);
        // // computes gauss transform naively if the estimated computational complexity
        // // is lower than IFGT
        // Eigen::MatrixXf compute_naive(const Eigen::MatrixXf & target_pts, 
        //                                     const Eigen::ArrayXf & weights);
        // estimates complexity of IFGT and naive implementations
        double compute_complexity(int M_target_pts);

};


int nchoosek(int n, int k);



#endif