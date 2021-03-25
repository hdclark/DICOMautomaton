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
    int max_truncation; // max truncation number (p)
    int p_max_total; // length of monomials after multi index expansion
    double bandwidth; 
    double epsilon;
    int n_clusters;
    double cutoff_radius;
    Eigen::MatrixXd source_pts;
    Eigen::MatrixXd constant_series;
    std::unique_ptr<Cluster> cluster; 

*/

// TODO: INTEGRATE WITH TESTING

IFGT::IFGT(const Eigen::MatrixXd & source_pts,
            double bandwidth, 
            double epsilon) {
    
    this->bandwidth = bandwidth;
    this->epsilon = epsilon;
    this->source_pts = source_pts;
    dim = source_pts.cols();

    auto time1 = std::chrono::high_resolution_clock::now();

    ifgt_choose_parameters();

    auto time2 = std::chrono::high_resolution_clock::now();
    auto time_span = std::chrono::duration_cast<std::chrono::duration<double>>(time2 - time1);
    std::cout << "ifgt_choose_parameters: " << time_span.count() << " s" << std::endl;
    // clustering goes here
    //Cluster cluster;
    cluster = std::make_unique<Cluster>(k_center_clustering(source_pts, n_clusters));
    //cluster = k_center_clustering(source_pts, n_clusters);

    std::cout << "Number of clusters: " << n_clusters << std::endl;

    auto time3 = std::chrono::high_resolution_clock::now();
    time_span = std::chrono::duration_cast<std::chrono::duration<double>>(time3 - time2);
    std::cout << "clustering time: " << time_span.count() << " s" << std::endl;

    cutoff_radius = cluster->rx_max;
    ifgt_update_max_truncation(cutoff_radius); // update max truncation
    p_max_total = nchoosek(max_truncation_p-1+dim,dim); 
    compute_constant_series();

    std::cout << "max_truncation_p: " << max_truncation_p << std::endl;
    std::cout << "p_max_total: " << p_max_total << std::endl;


    auto time4 = std::chrono::high_resolution_clock::now();
    time_span = std::chrono::duration_cast<std::chrono::duration<double>>(time4 - time3);
    std::cout << "compute_constant_series: " << time_span.count() << " s" << std::endl;
}

// autotuning of ifgt parameters
void IFGT::ifgt_choose_parameters() {

    double h_square = bandwidth * bandwidth;
    double min_complexity = std::numeric_limits<double>::max(); 
    int n_clusters = 0; 
    int max_clusters = std::round(0.2 * 100 / bandwidth); // an estimate of typical upper bound 
    int p_ul = 200; // estimate                           // from papers authors
    int max_truncation = 0;

    double radius = bandwidth * std::sqrt(std::log(1 / epsilon));

    for(int i = 0; i < max_clusters; i++) {
        double rx = std::pow((double)i + 1, -1.0 / (double) dim); // rx ~ K^{-1/dim} estimate for uniform datasets
		double rx_square = rx*rx;

		double n = std::min((double) (i+1), std::pow(radius/rx, (double) dim));
		double error = std::numeric_limits<double>::max();
	    double temp = 1;
		int p = 0;

		while((error > epsilon) && (p <= p_ul)){
			p++;
			double b = std::min((rx + std::sqrt(rx_square + 2.0 * p * h_square)) / 2.0, rx + radius);
			double c = rx - b;
			temp *= 2 * rx * b / h_square / (double)p;
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
    this->max_truncation_p = max_truncation; 
    this->cutoff_radius = radius;
}


// after clustering, reupdate the truncation based on actual max 
void IFGT::ifgt_update_max_truncation(double rx_max) {

    int truncation_number_ul = 100; // j
    double R = std::sqrt(dim);                            
    double h_square = bandwidth * bandwidth;
    double rx2 = rx_max * rx_max;
    double r = std::min(R, bandwidth * sqrt(log(1.0 / epsilon)));
    double error = std::numeric_limits<double>::max();

    std::cout << "r: " << r << " // h_square: " << h_square << std::endl;
    int p = 0;
    double temp = 1.0;
    while ((error > epsilon) && (p <= truncation_number_ul)) {
        ++p;
        double b = std::min((rx_max + std::sqrt(rx2 + 2 * double(p) * h_square)) / 2.0, rx_max + r);
        double c = rx_max - b;
        temp *= 2 * rx_max * b / h_square / double(p);
        error = temp * std::exp(-(c * c) / h_square);

        std::cout << "c: " << c << " // b: " << b  << " // temp: " << temp << " // rx_max: " << rx_max << std::endl;
        std::cout << "p: " << p << " // error: " << error << "\n" << std::endl;
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

    Eigen::MatrixXd constant_series = Eigen::MatrixXd::Ones(p_max_total,1);

    for (int k = 1, t = 1, tail = 1; k < max_truncation_p; k++, tail = t) {
        for (int i = 0; i < dim; ++i) {
            int head = heads[i];
            heads[i] = t;
            for (int j = head; j < tail; j++, t++) {
                cinds[t] = (j < heads[i + 1]) ? cinds[j] + 1 : 1;
                constant_series(t) = 2.0 * constant_series(j);
                constant_series(t) = constant_series(t) / double(cinds[t]);
            }
        }
    }
    delete [] cinds;
    delete [] heads; 

    this->constant_series = constant_series;
}

// computes [(y_j-c_k)/h]^{alpha} or [(x_i-c_k)/h]^{alpha}
// where y_j is target point, x_i is point inside cluster
Eigen::VectorXd IFGT::compute_monomials(const Eigen::MatrixXd & delta) {
    int *heads = new int[dim];

    Eigen::VectorXd monomials = Eigen::VectorXd::Ones(p_max_total);
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


Eigen::MatrixXd IFGT::compute_ck(const Eigen::ArrayXd & weights){

    int N_source_pts = source_pts.rows();
    double h_square = bandwidth * bandwidth;

    Eigen::MatrixXd C_k = Eigen::MatrixXd::Zero(n_clusters, p_max_total);
    for (int i = 0; i < N_source_pts; ++i) {

        // auto start = std::chrono::high_resolution_clock::now();

        double distance = 0.0;
        Eigen::MatrixXd dx = Eigen::MatrixXd::Zero(dim,1);
        int cluster_index = cluster->assignments(i);


        // auto time1 = std::chrono::high_resolution_clock::now();
 
        for (int k = 0; k < dim; ++k) {
            double delta = source_pts(i, k) -  cluster->k_centers(cluster_index, k); // consider using normsquared
            distance += delta * delta;
            dx(k) = delta / bandwidth;
        }

        // auto time2 = std::chrono::high_resolution_clock::now();
        // auto time_span = std::chrono::duration_cast<std::chrono::duration<double>>(time2 - time1);
        // std::cout << "computing norm_squared " << time_span.count() << " s, " << "iteration: " << i << std::endl;


        auto monomials = compute_monomials(dx);

        // auto time3 = std::chrono::high_resolution_clock::now();
        // time_span = std::chrono::duration_cast<std::chrono::duration<double>>(time3 - time2);
        // std::cout << "computing monomials " << time_span.count() << " s, " << "iteration: " << i << std::endl;


        double f = weights(i) * std::exp(-distance / h_square); // multiply by vector of weights if needed
                                                                
        for (int alpha = 0; alpha < p_max_total; ++alpha) {
            C_k(cluster_index, alpha) += f * monomials(alpha);
        }

        // auto time4 = std::chrono::high_resolution_clock::now();
        // time_span = std::chrono::duration_cast<std::chrono::duration<double>>(time4 - time3);
        // std::cout << "computing sum of f * monomials " << time_span.count() << " s, " << "iteration: " << i << std::endl;
        // time_span = std::chrono::duration_cast<std::chrono::duration<double>>(time4 - start);
        // std::cout << "Complete loop time: " << time_span.count() << " s, " << "iteration: " << i << "\n" <<std::endl;

    }

    for (int j = 0; j < n_clusters; j++) {
        for (int alpha = 0; alpha < p_max_total; alpha++) {
            C_k(j, alpha) *= constant_series(alpha); // computed in constructor
        }
    }


    return C_k;
} 

Eigen::MatrixXd IFGT::compute_gaussian(const Eigen::MatrixXd & target_pts,
                                       const Eigen::MatrixXd & C_k) {

    int M_target_pts = target_pts.rows();
    double h_square = bandwidth * bandwidth;

    Eigen::MatrixXd G_y = Eigen::MatrixXd::Zero(M_target_pts,1);

    double radius = bandwidth * std::sqrt(std::log(1 / epsilon));

    Eigen::VectorXd m_ry_square(n_clusters);

    for (int j = 0; j < n_clusters; ++j) {
        double ry = radius + cluster->radii(j); 
        m_ry_square(j) = ry * ry;
    }

    for (int i = 0; i < M_target_pts; ++i) {
        for (int j = 0; j < n_clusters; ++j) {
            double distance = 0.0;
            Eigen::MatrixXd dy = Eigen::MatrixXd::Zero(dim,1);

            for (int k = 0; k < dim; ++k) {
                double delta = target_pts(i, k) - cluster->k_centers(j, k); // struct names tbd 
                distance += delta * delta;
                if (distance > m_ry_square(j)) { // need to decide whereto get ry_square
                    break;
                }
                dy(k) = delta / bandwidth;
            }
            if (distance <= m_ry_square(j)) {
                auto monomials = compute_monomials(dy);
                double g = std::exp(-distance / h_square);
                G_y(i) += g * C_k.row(j).dot(monomials); // row vector * column vector = number
            }                                         // G_y(i) += g * C_k.row(j) * monomials; 
        }
    }

    return G_y;

}

// ttwo different computes for weights and no weights 
Eigen::MatrixXd IFGT::compute_ifgt(const Eigen::MatrixXd & target_pts) {
    Eigen::ArrayXd ones_array = Eigen::ArrayXd::Ones(target_pts.rows(),1);

    auto time1 = std::chrono::high_resolution_clock::now();

    Eigen::MatrixXd C_k = compute_ck(ones_array);

    auto time2 = std::chrono::high_resolution_clock::now();
    auto time_span = std::chrono::duration_cast<std::chrono::duration<double>>(time2 - time1);
    std::cout << "compute_ck: " << time_span.count() << " s" << std::endl;

    Eigen::MatrixXd G_y = compute_gaussian(target_pts, C_k);

    auto time3 = std::chrono::high_resolution_clock::now();
    time_span = std::chrono::duration_cast<std::chrono::duration<double>>(time3 - time2);
    std::cout << "compute_gaussian: " << time_span.count() << " s" << std::endl;

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
                                            double epsilon,
                                            double w) {
    
    int N_fixed_pts = fixed_pts.rows();
    int M_moving_pts = moving_pts.rows();
    int dim = fixed_pts.cols();
    
    Eigen::MatrixXd P1; 
    Eigen::MatrixXd Pt1; 
    Eigen::MatrixXd Kt1;
    Eigen::MatrixXd PX(M_moving_pts, dim);


    double bandwidth = std::sqrt(2.0 * sigmaSquared);
    auto ifgt_transform = std::make_unique<IFGT>(moving_pts, bandwidth, epsilon); // in this case, moving_pts = source_pts
                                                                                  // because we take the transpose of K(M x N)                                                 
    Kt1 = ifgt_transform->compute_ifgt(fixed_pts);                // so we'll get an N x 1 vector for Kt1       

    double c = w / (1.0 - w) * (double) M_moving_pts / N_fixed_pts * 
                                std::pow(2.0 * M_PI * sigmaSquared, 0.5 * dim);

    Eigen::ArrayXd denom_a = Kt1.array() + c; 
    Pt1 = (1 - c / denom_a).matrix(); // Pt1 = 1-c*a

    ifgt_transform = std::make_unique<IFGT>(fixed_pts, bandwidth, epsilon); 
    P1 = ifgt_transform->compute_ifgt(moving_pts, 1 / denom_a); // P1 = Ka 
    
    for (int i = 0; i < dim; ++i) {
        PX.col(i) = ifgt_transform->compute_ifgt(moving_pts, fixed_pts.col(i).array() / denom_a); // PX = K(a.*X)
    }
    return { P1, Pt1, PX };
    

}

Cluster k_center_clustering(const Eigen::MatrixXd & points, int num_clusters) {

    Eigen::MatrixXd k_centers = Eigen::MatrixXd::Zero(num_clusters, points.cols());
    Eigen::VectorXd assignments = Eigen::VectorXd::Zero(points.rows());
    Eigen::VectorXd distances = -1*Eigen::VectorXd::Ones(points.rows());
    Eigen::VectorXd radii = Eigen::VectorXd::Zero(num_clusters);

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
            if (dist < distances(j) || distances(j) == -1) {
                distances(j) = dist;
                assignments(j) = i;
            }

            if (distances(j) > largest_distance) {
                largest_distance = distances(j);
                index_of_largest = j;
            }
        }
    }
    for(long int i = 0; i < points.rows(); ++i) {
        if(distances(i) > radii(assignments(i))) {
            radii(assignments(i)) = distances(i);
        }
    }
 
    /*
    Eigen::MatrixXd k_centers;
    Eigen::VectorXd radii;
    Eigen::VectorXd assignments;
    Eigen::VectorXd distances;
    double rx_max;
    
    cluster.k_centers = k_centers;
    cluster.assignments = assignments;
    cluster.distances = distances;
    cluster.radii = radii;
    cluster.rx_max = largest_distance;
    */
   return {k_centers, radii, assignments, distances, largest_distance};
}


int nchoosek(int n, int k) {
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
