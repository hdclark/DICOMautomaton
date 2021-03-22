#include <math.h>
#include <stdlib.h>

// #include "YgorFilesDirs.h"    //Needed for Does_File_Exist_And_Can_Be_Read(...), etc..
// #include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
// #include "YgorMath.h"         //Needed for samples_1D.
// #include "YgorString.h"       //Needed for GetFirstRegex(...)

#include "IFGT.h"
#include <limits>
#include <algorithm> // dunno if its in docker
#include <cmath>

/*
    int dim;
    int max_truncation_p; // max truncation number (p)
    int p_max_total; // length of monomials after multi index expansion
    std::unique_ptr<Cluster> cluster; 
    double bandwidth; 
    double epsilon;
    int n_clusters;
    double cutoff_radius;

*/

// TODO: INTEGRATE WITH TESTING

IFGT::IFGT(const Eigen::MatrixXd & source_pts,
            double bandwidth, 
            double epsilon) {
    
    this->bandwidth = bandwidth;
    this->epsilon = epsilon;
    dim = source_pts.cols();
    this->source_pts = source_pts;

    ifgt_choose_parameters();
    // clustering goes here
    double rx_max = cluster.rx_max;
    ifgt_update_max_truncation(double rx_max); // update max truncation
    p_max_total = nchoosek(max_truncation_p-1+dim,dim); 
    compute_constant_series();
}

// autotuning of ifgt parameters
void IFGT::ifgt_choose_parameters() {

    double h_square = bandwidth * bandwidth;
    double min_complexity = std::numeric_limits<double>::max(); 
    int n_clusters = 0; 
    int max_clusters = std::round(0.2 * 100 / bandwidth); // dont know where this comes from lol
    int p_ul = 200; // estimate 
    int max_truncation = 0;
    double R = sqrt(dim);

    double radius = std::min(R, bandwidth * std::sqrt(std::log(1 / epsilon)));

    for(int i = 0; i < max_clusters; i++) {
        double rx = std::pow((double)i + 1, -1.0 / (double) dim); // rx ~ K^{-1/dim} estimate for uniform datasets
		double rx_square = rx*rx;

		double n = std::min((double) (i+1), std::pow(radius/rx, (double) dim));
		double error = 1;
	    double temp = 1;
		int p = 0;

		while((error > epsilon) & (p <= p_ul)){
			p++;
			double b = std::min(((rx + std::sqrt((rx_square)+(2*p*h_square)))/2), rx + radius);
			double c = rx - b;
			temp = temp * (((2 * rx * b) / h_square)/p);
			error = temp * (std::exp(-(c*c)/h_square));			
		}	
		double complexity = (i + 1) + std::log((double)i + 1) + ((1 + n) * nchoosek(p - 1 + dim, dim));

		if (complexity < min_complexity ){
			min_complexity = complexity;
			n_clusters = i+1;
			max_truncation = p;		
		}
	}
    this->n_clusters = n_clusters; // class variables
    max_truncation_p = max_truncation; 
    cutoff_radius = radius;
}


// after clustering, reupdate the truncation based on actual max 
void IFGT::ifgt_update_max_truncation(double rx_max) {

    int truncation_number_ul = 200; // j
    double R = std::sqrt(dim);                            
    double h_square = bandwidth * bandwidth;
    double rx2 = rx_max * rx_max;
    double r = std::min(R, bandwidth * sqrt(log(1.0 / epsilon)));
    double error = std::numeric_limits<double>::max();

    int p = 0;
    double temp = 1.0;
    while ((error > epsilon) && (p <= truncation_number_ul)) {
        ++p;
        double b = std::min((rx_max + sqrt(rx2 + 2 * double(p) * h_square)) / 2.0, rx_max + r);
        double c = rx_max - b;
        temp *= 2 * rx_max * b / h_square / double(p);
        error = temp * std::exp(-(c * c) / h_square);
    }
    max_truncation_p = p;
}

// computes 2^alpha/alpha!
// use in constructor
void IFGT::compute_constant_series() {
    int *heads = new int[dim+1];
    int *cinds = new int[p_max_total];

    for (int i = 0; i < dim; i++) {
        heads[i] = 0;
    } 
    heads[dim] = std::numeric_limits<int>::max();

    Eigen::MatrixXd constant_series = MatrixXd::Ones(p_max_total,1);

    for (int k = 1, t = 1, tail = 1; k < max_truncation_p; k++, tail = t) {
        for (int i = 0; i < dim; ++i) {
            int head = heads[i];
            heads[i] = t;
            for (int j = head; j < tail; j++, t++) {
                cinds[t] = (j < heads[i + 1]) ? cinds[j] + 1 : 1;
                constant_series(t) = 2.0 * constant_series(j);
                constant_series(t) = constant_series(t) / double(cinds[t]));
            }
        }
    }
    delete [] cinds;
    delete [] heads; 

    this->constant_series = constant_series;
}

// computes [(y_j-c_k)/h]^{alpha} or [(x_i-c_k)/h]^{alpha}
// where y_j is target point, x_i is point inside cluster
Eigen::MatrixXd IFGT::compute_monomials(const Eigen::MatrixXd & delta) {
    int *heads = new int[dim];

    Eigen::MatrixXd monomials = Eigen::MatrixXd::Ones(p_max_total,1);
    for (int i = 0; i < dim; i++){
        heads[i] = 0;
    }

    monomials(0) = 1.0;
    for (int k = 1, t = 1, tail = 1; k < max_truncation_p; k++, tail = t) {
        for (int i = 0; i < dim; i++){
            int head = heads[i];
            heads[i] = t;
            for (int j = head; j < tail; j++, t++) {
                monomials(t) = delta(i) * monomials(j);
            }
        }            
    }          

    delete [] heads;

    return monomials;
}
// assumes clusters is a struct with parameters 
// int indices[] and MatrixXd centres
// also assumes all weights are 1 (which is the case for cpd)
Eigen::MatrixXd IFGT::compute_ck(const Eigen::ArrayXd & weights){

    int N_source_pts = source_pts.rows();
    double h_square = bandwidth * bandwidth;

    Eigen::MatrixXd C = Eigen::Matrix::Zero(n_clusters, p_max_total);
    for (int i = 0; i < N_source_pts; ++i) {
        double distance = 0.0;
        Eigen::MatrixXd dx = Eigen::MatrixXd::Zero(dim,1);
        int cluster_index = clusters.indices[i];

        for (int k = 0; k < dim; ++k) {
            double delta = source_pts(i, k) -  clusters.centres(cluster_index, k);
            distance += delta * delta;
            dx(k) = delta / bandwidth;
        }
        Eigen::MatrixXd monomials = compute_monomials(dx);
        double f = weightst(i) * std::exp(-distance / h_square); // multiply by vector of weights if needed
                                                   // cpd has all weights of 1, so it was omitted
        for (int alpha = 0; alpha < p_max_total; ++alpha) {
            C_k(cluster_index, alpha) += f * monomials(alpha);
        }
    }

    Eigen::MatrixXd constant_series = compute_constant_series(dim, p_max_total, max_truncation_p);
    for (int j = 0; j < n_clusters; j++) {
        for (int alpha = 0; alpha < p_max_total; alpha++) {
            C_k(j, alpha) *= constant_series(alpha);
        }
    }

    return C_k;
} 

Eigen::MatrixXd IFGT::compute_gaussian(const Eigen::MatrixXd & target_pts
                                       const Eigen::MatrixXd & C_k){

    int M_target_pts = target_pts.rows();
    double h_square = bandwidth * bandwidth;

    Eigen::MatrixXd G_y = Eigen::MatrixXd::Zero(M_target_pts,1);

    double m_ry_square[n_clusters];
    for (int j = 0; j < n_clusters; ++j) {
        double ry = radius + clusters.radii[j]; //UPDATE WITH CLUSTERS
        m_ry_square[j] = ry * ry;
    }

    for (int i = 0; i < M_target_pts; ++i) {
        for (int j = 0; j < n_clusters; ++j) {
            double distance = 0.0;
            Eigen::MatrixXd dy = Eigen::MatrixXd::Zero(dim,1);

            for (int k = 0; k < dim; ++k) {
                double delta = target_pts(i, k) - clusters.cluster_centres(j, k); // struct names tbd 
                distance += delta * delta;
                if (distance > m_ry_square[j]) { // need to decide whereto get ry_square
                    break;
                }
                dy(k) = delta / bandwidth;
            }
            if (distance <= m_ry_square[j]) {
                Eigen::MatrixXd monomials = compute_monomials(dy);
                double g = std::exp(-distance / h_square);
                G_y(i) += C.row(j) * g * monomials; // row vector * column vector
            }
        }
    }

    return G_y;


}
// ttwo different computes for weights and no weights 
Eigen::MatrixXd IFGT::compute_ifgt(const Eigen::MatrixXd & target_pts) {
    Eigen::ArrayXd ones_array = Eigen::ArrayXd::Ones(target_pts.rows(),1);
    Eigen::MatrixXd C_k = compute_ck(ones_array);
    Eigen::MatrixXd G_y = compute_gaussian(target_pts, C_k);

    return G_y;
}
Eigen::MatrixXd IFGT::compute_ifgt(const Eigen::MatrixXd & target_pts, const Eigen::ArrayXd & weights) {
    Eigen::MatrixXd C_k = compute_ck(weights);
    Eigen::MatrixXd G_y = compute_gaussian(target_pts, C_k);

    return G_y;
}
// Y = target_pts = fixed_pts
// X = source_pts = moving_pts (in general)
// epsilon is error, w is a parameter from cpd
CPD_MatrixVector_Products compute_cpd_products(const Eigen::MatrixXd & fixed_pts,
                                            const Eigen::MatrixXd & moving_pts,
                                            double sigmaSquared, 
                                            double epsilon
                                            double w) {
    Eigen::MatrixXd P1; 
    Eigen::MatrixXd Pt1; 

    int N_fixed_pts = fixed_pts.rows();
    int M_moving_pts = moving_pts.rows();
    int dim = fixed_pts.cols();


    double bandwidth = std::sqrt(2.0 * sigmaSquared);
    auto ifgt_transform = std::make_unique<IFGT>(moving_pts, bandwidth, epsilon); // in this case, moving_pts = source_pts
                                                                                  // because we take the transpose of K(M x N)                                                 
    Eigen::MatrixXd Kt1 = ifgt_transform->compute_ifgt(fixed_pts);                // so we'll get an N x 1 vector for Kt1       

    double c = w / (1.0 - w) * (double) M_moving_pts / N_fixed_pts * 
                                std::pow(2.0 * M_PI * sigmaSquared, 0.5 * dim);

    Eigen::ArrayXd denom_a = Kt1.array() + c; 
    Eigen::MatrixXd Pt1 = (1 - c / denom_a).matrix(); // Pt1 = 1-c*a

    ifgt_transform = std::make_unique<IFGT>(fixed_pts, bandwidth, epsilon); 
    Eigen::MatrixXd P1 = ifgt_transform->compute(moving_pts, 1 / denom_a); // P1 = Ka 
    Eigen::MatrixXd PX(M_moving_pts, dim);
    for (int i = 0; i < dim; ++i) {
        PX.col(i) = ifgt_transform->compute_ifgt(moving_pts, fixed_pts.col(i).array() / denom_a); // PX = K(a.*X)
    }
    return { P1, Pt1, PX };
    

}

std::tuple<Eigen::MatrixXd, Eigen::MatrixXd, double> k_center_clustering(const Eigen::MatrixXd & points, int num_clusters) {

    Eigen::MatrixXd k_centers = Eigen::MatrixXd::Zero(num_clusters, points.cols());
    Eigen::MatrixXd assigned_points = points.conservativeResize(points.rows(), points.cols() + 2);

    Eigen::MatrixXd assignments = Eigen::MatrixXd::Zero(points.rows(),1);
    Eigen::MatrixXd distances = -1*Eigen::MatrixXd::Ones(points.rows(),1);

    int seed = rand() % points.rows() + 1;
    double largest_distance = 0;
    int index_of_largest = seed;
    double dist = 0;

    k_centers.row(0) = points.row(seed);

    for(long int i = 0; i < num_clusters; ++i) { 

        k_centers.row(i) = points.row(index_of_largest);

        largest_distance = 0;
        index_of_largest = 0;

        for(long int j = 0; j < points.rows(); ++j) {
            dist = (points.row(j) - k_centers.row(i)).norm();
            if (dist < distances(j,0)) {
                distances(j,0) = dist;
                assignments(j,0) = i;
            }

            if (distances(j,0) > largest_distance) {
                largest_distance = distances(j,0);
                index_of_largest = j
            }
        }
    }

    assigned_points.col(points.cols()) = assignments;
    assigned_points.col(points.cols()+1) = distances;

    return std::make_tuple(k_centers, assigned_points, largest_distane);
}


int nchoosek(int n, int k){
	int n_k = n - k;
	
	if (k < n_k) {
		k = n_k;
		n_k = n - k;
	}
	int nchsk = 1; 
	for ( int i = 1; i <= n_k; i++) {
		nchsk *= (++k);
		nchsk /= i;
	}
	return nchsk;
}

int main (int argc, char *argv[]) {

    return 0;
 }