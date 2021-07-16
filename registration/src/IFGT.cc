#include <math.h>
#include <stdlib.h>

// #include "YgorFilesDirs.h"    //Needed for Does_File_Exist_And_Can_Be_Read(...), etc..
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
// #include "YgorMath.h"         //Needed for samples_1D.
// #include "YgorString.h"       //Needed for GetFirstRegex(...)

#include "IFGT.h"
#include <limits>
#include <algorithm> // dunno if its in docker
#include <cmath>

IFGT::IFGT(const Eigen::MatrixXf & source_pts,
            double bandwidth, 
            double epsilon) :
            source_pts  (source_pts),
            bandwidth   (bandwidth),
            epsilon     (epsilon), 
            dim         (source_pts.cols()){

    // Optional: check time taken for different ifgt functions
    // auto time1 = std::chrono::high_resolution_clock::now();

    ifgt_choose_parameters();

    // auto time2 = std::chrono::high_resolution_clock::now();
    // auto time_span = std::chrono::duration_cast<std::chrono::duration<double>>(time2 - time1);
    // std::cout << "ifgt_choose_parameters: " << time_span.count() << " s" << std::endl;

    // std::cout << "Number of clusters: " << n_clusters << std::endl;

    this->cluster = std::make_unique<Cluster>(k_center_clustering(source_pts, n_clusters));

    // auto time3 = std::chrono::high_resolution_clock::now();
    // time_span = std::chrono::duration_cast<std::chrono::duration<double>>(time3 - time2);
    // std::cout << "clustering time: " << time_span.count() << " s" << std::endl;

    cutoff_radius = cluster->rx_max;
    ifgt_update_max_truncation(cutoff_radius); // update max truncation
    p_max_total = nchoosek(max_truncation_p-1+dim,dim); 
    compute_constant_series();

    // std::cout << "max_truncation_p: " << max_truncation_p << std::endl;
    // std::cout << "p_max_total: " << p_max_total << std::endl;

    // auto time4 = std::chrono::high_resolution_clock::now();
    // time_span = std::chrono::duration_cast<std::chrono::duration<double>>(time4 - time3);
    // std::cout << "compute_constant_series: " << time_span.count() << " s" << std::endl;   
}


// autotuning of ifgt parameters
void IFGT::ifgt_choose_parameters() {

    double h_square = bandwidth * bandwidth;
    double min_complexity = std::numeric_limits<double>::max(); 
    int n_clusters = 1; 
    int max_clusters = std::round(0.2 * 100 * std::sqrt(dim) / bandwidth); // an estimate of typical upper bound 
    if (max_clusters > source_pts.rows()) {
        max_clusters = source_pts.rows() / 2; //  just a guess
    }
    
    // std::cout << "MAX clusters: " << max_clusters << std::endl;
    int p_ul = 200; // estimate                           // from papers authors
    int max_truncation = 1;

    double R = std::sqrt((double) dim);
    double radius = std::min(R, bandwidth * std::sqrt(std::log(1.0 / epsilon)));

    for(int i = 0; i < max_clusters; i++) {
        double rx = std::pow((double)i + 1, -1.0 / (double) dim); // rx ~ K^{-1/dim} estimate for uniform datasets
		double rx_square = rx*rx;

		double n = std::min((double) (i+1), std::pow(radius/rx, (double) dim));
		double error = std::numeric_limits<double>::max();
	    double temp = 1;
		int p = 0;

        // std::cout << "IFGT_CHOOSE_PARAMETERS, clusters: " << i+1 << "\n" << std::endl;

		while((error > epsilon) && (p <= p_ul)){
			p++;
			double b = std::min((rx + std::sqrt(rx_square + 2.0 * p * h_square)) / 2.0, rx + radius);
			double c = rx - b;
			temp *= 2 * rx * b / h_square / (double)p;
			error = temp * (std::exp(-(c*c)/h_square));

            // std::cout << "c: " << c << " // b: " << b  << " // temp: " << temp << " // rx: " << rx << std::endl;
            // std::cout << "p: " << p << " // error: " << error << "\n" << std::endl;			
		}	
		double complexity = (i + 1) + std::log((double)i + 1) + ((1 + n) * nchoosek(p - 1 + dim, dim));

        // std::cout << "complexity: " << complexity << "\n" << std::endl;	

		if ((complexity < min_complexity) && (error < epsilon)){
			min_complexity = complexity;
			n_clusters = i+1;
			max_truncation = p;		
		}
	}
    // std::cout << " max_truncation initial: " << max_truncation << std::endl;
    this->n_clusters = n_clusters; // class variables
    this->max_truncation_p = max_truncation; 
    this->cutoff_radius = radius;
}


// after clustering, reupdate the truncation based on actual max 
void IFGT::ifgt_update_max_truncation(double rx_max) {

    int truncation_number_ul = 200; // j
    double R = std::sqrt(dim);                            
    double h_square = bandwidth * bandwidth;
    double rx2 = rx_max * rx_max;
    double radius = std::min(R, bandwidth * sqrt(log(1.0 / epsilon)));
    double error = std::numeric_limits<double>::max();

    // std::cout << "r: " << radius << " // h_square: " << h_square << std::endl;
    int p = 0;
    double temp = 1.0;
    while ((error > epsilon) && (p <= truncation_number_ul)) {
        ++p;
        double b = std::min((rx_max + std::sqrt(rx2 + 2.0 * p * h_square)) / 2.0, rx_max + radius);
        double c = rx_max - b;
        
        temp *= 2 * rx_max * b / h_square / double(p);
        error = temp * std::exp(-(c * c) / h_square);

        // std::cout << "c: " << c << " // b: " << b  << " // temp: " << temp << " // rx_max: " << rx_max << std::endl;
        // std::cout << "p: " << p << " // error: " << error << "\n" << std::endl;
    }
    this->max_truncation_p = p;
}

// computes 2 ^ alpha / alpha!
// use in constructor
void IFGT::compute_constant_series() {
    int *heads = new int[dim+1];
    int *cinds = new int[p_max_total];

    for (int i = 0; i < dim; i++) {
        heads[i] = 0;
        
    } 
    cinds[0] = 0; 
    heads[dim] = std::numeric_limits<int>::max();
    

    Eigen::MatrixXf constant_series = Eigen::MatrixXf::Ones(p_max_total,1);

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
Eigen::VectorXf IFGT::compute_monomials(const Eigen::MatrixXf & delta) {
    int *heads = new int[dim];

    Eigen::VectorXf monomials = Eigen::VectorXf::Ones(p_max_total);
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


Eigen::MatrixXf IFGT::compute_ck(const Eigen::ArrayXf & weights){

    int N_source_pts = source_pts.rows();
    double h_square = bandwidth * bandwidth;

    Eigen::MatrixXf C_k = Eigen::MatrixXf::Zero(n_clusters, p_max_total);
    
    for (int i = 0; i < N_source_pts; ++i) {
        // auto start = std::chrono::high_resolution_clock::now();
        double distance = 0.0;
        Eigen::MatrixXf dx = Eigen::MatrixXf::Zero(dim,1);
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

Eigen::MatrixXf IFGT::compute_gaussian(const Eigen::MatrixXf & target_pts,
                                       const Eigen::MatrixXf & C_k) {

    int M_target_pts = target_pts.rows();
    double h_square = bandwidth * bandwidth;
    // std::cout << "max truncation: " << max_truncation_p << " // nclusters: " << n_clusters << std::endl;

    Eigen::MatrixXf G_y = Eigen::MatrixXf::Zero(M_target_pts,1);

    double radius = bandwidth * std::sqrt(std::log(1 / epsilon));

    Eigen::VectorXf ry_square(n_clusters);

    for (int j = 0; j < n_clusters; ++j) {
        double ry = radius + cluster->radii(j); 
        ry_square(j) = ry * ry;
    }

    for (int i = 0; i < M_target_pts; ++i) {
        for (int j = 0; j < n_clusters; ++j) {
            double distance = 0.0;
            Eigen::MatrixXf dy = Eigen::MatrixXf::Zero(dim,1);

            for (int k = 0; k < dim; ++k) {
                double delta = target_pts(i, k) - cluster->k_centers(j, k); 
                distance += delta * delta;
                if (distance > ry_square(j)) { 
                    break;
                }
                dy(k) = delta / bandwidth;
            }
            if (distance <= ry_square(j)) {
                auto monomials = compute_monomials(dy);
                double g = std::exp(-distance / h_square);
                G_y(i) += g * C_k.row(j).dot(monomials); // row vector * column vector = number
            }                                         // G_y(i) += g * C_k.row(j) * monomials; 
        }
    }

    return G_y;

}

// two different computes for weights and no weights 
Eigen::MatrixXf IFGT::compute_ifgt(const Eigen::MatrixXf & target_pts) {
    Eigen::ArrayXf ones_array = Eigen::ArrayXf::Ones(source_pts.rows(),1);

    double ifgt_complexity = compute_complexity(target_pts.rows());
    double naive_complexity = dim * target_pts.rows() * source_pts.rows();

    // std::cout << "ifgt complexity: " << ifgt_complexity << " // naive_complexity: " << naive_complexity << std::endl;
    Eigen::MatrixXf G_y = Eigen::MatrixXf::Zero(target_pts.rows(),1);

    // auto time1 = std::chrono::high_resolution_clock::now();

    if (ifgt_complexity < naive_complexity) {
        FUNCINFO("Running IFGT");
        Eigen::MatrixXf C_k = compute_ck(ones_array);
        // auto time2 = std::chrono::high_resolution_clock::now();
        // auto time_span = std::chrono::duration_cast<std::chrono::duration<double>>(time2 - time1);
        // std::cout << "compute_ck: " << time_span.count() << " s" << std::endl;

        G_y = compute_gaussian(target_pts, C_k);
        // auto time3 = std::chrono::high_resolution_clock::now();
        // time_span = std::chrono::duration_cast<std::chrono::duration<double>>(time3 - time2);
        // std::cout << "compute_gaussian: " << time_span.count() << " s" << std::endl;
    } 
    else {
        FUNCINFO("Running Naive");
        G_y = compute_naive_gt(target_pts, source_pts, ones_array, bandwidth);
        // auto time4 = std::chrono::high_resolution_clock::now();
        // auto time_span = std::chrono::duration_cast<std::chrono::duration<double>>(time4 - time1);
        // std::cout << "naive computation: " << time_span.count() << " s" << std::endl;
    }

    return G_y;
}


Eigen::MatrixXf IFGT::compute_ifgt(const Eigen::MatrixXf & target_pts, const Eigen::ArrayXf & weights) {
    double ifgt_complexity = compute_complexity(target_pts.rows());
    double naive_complexity = dim * target_pts.rows() * source_pts.rows();

    Eigen::MatrixXf G_y = Eigen::MatrixXf::Zero(target_pts.rows(),1);
    // std::cout << "ifgt complexity: " << ifgt_complexity << " // naive_complexity: " << naive_complexity << std::endl;
    // auto time1 = std::chrono::high_resolution_clock::now();
    // estimates ifgt complexity very conservatively --> can change it to a 
    // certain proportion of the complexity to run the ifgt more often
    if (ifgt_complexity < naive_complexity) {
        FUNCINFO("Running IFGT (weighted)");
        Eigen::MatrixXf C_k = compute_ck(weights);

        // auto time2 = std::chrono::high_resolution_clock::now();
        // auto time_span = std::chrono::duration_cast<std::chrono::duration<double>>(time2 - time1);
        // std::cout << "compute_ck: " << time_span.count() << " s" << std::endl;

        G_y = compute_gaussian(target_pts, C_k);

        // auto time3 = std::chrono::high_resolution_clock::now();
        // time_span = std::chrono::duration_cast<std::chrono::duration<double>>(time3 - time2);
        // std::cout << "compute_gaussian: " << time_span.count() << " s" << std::endl;
    } 
    else {
        FUNCINFO("Running Naive (weighted)");
        G_y = compute_naive_gt(target_pts, source_pts, weights, bandwidth);
    }
    return G_y;
}


double IFGT::compute_complexity(int M_target_pts) {
    int N_source_pts = source_pts.rows();
    return (double(N_source_pts) + M_target_pts * double(n_clusters)) * p_max_total
                + double(dim) * N_source_pts * n_clusters;
}


double rescale_points(const Eigen::MatrixXf & fixed_pts,
                        const Eigen::MatrixXf & moving_pts,
                        Eigen::MatrixXf & fixed_pts_scaled,
                        Eigen::MatrixXf & moving_pts_scaled,
                        double bandwidth) {
    
    auto min_max = calc_max_range(fixed_pts, moving_pts);
    double min = min_max.first;
    double max = min_max.second;

    

    double max_range = max - min;
    if (max_range <= 1.0) {
        max_range = 1.0;
    }
    fixed_pts_scaled = ((fixed_pts.array() - min) / max_range).matrix();
    moving_pts_scaled = ((moving_pts.array() - min) / max_range).matrix();

    // std::cout << "max_range: " << max_range << " // min: " << min << " // max: " << max << std::endl;
    return bandwidth / max_range; // scale bandwidth

}


// computes gauss transform naively in O(NM) time
Eigen::MatrixXf compute_naive_gt(const Eigen::MatrixXf & target_pts, 
                                const Eigen::MatrixXf & source_pts,
                                const Eigen::ArrayXf & weights,
                                double bandwidth) {

    Eigen::MatrixXf G_naive = Eigen::MatrixXf::Zero(target_pts.rows(),1);
    double h2 = bandwidth * bandwidth;
    for (int m = 0; m < target_pts.rows(); ++m) {
        for (int n = 0; n < source_pts.rows(); ++n) {
            double expArg = - 1.0 / h2 * (target_pts.row(m) - source_pts.row(n)).squaredNorm();
            G_naive(m) += weights(n) * std::exp(expArg);
        }
    }
    return G_naive;
}


Cluster k_center_clustering(const Eigen::MatrixXf & points, int num_clusters) {

    Eigen::MatrixXf k_centers = Eigen::MatrixXf::Zero(num_clusters, points.cols());
    Eigen::VectorXf assignments = Eigen::VectorXf::Zero(points.rows());
    Eigen::VectorXf distances = -1*Eigen::VectorXf::Ones(points.rows());
    Eigen::VectorXf radii = Eigen::VectorXf::Zero(num_clusters);

    double largest_distance = 0;
    int index_of_largest = 0;
    double dist = 0;
    k_centers.row(0) = points.row(0);

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
 
   return {k_centers, radii, assignments, distances, largest_distance};
}

std::pair<double, double> calc_max_range(const Eigen::MatrixXf & target_pts,
                                                const Eigen::MatrixXf & source_pts) {

    double min = std::numeric_limits<double>::max();
    double max = std::numeric_limits<double>::min();
    
    // target points
    for(long int i = 0; i < target_pts.rows(); ++i) {
        for(long int j = 0; j < target_pts.cols(); ++j) {
            if(target_pts(i,j) < min) {
                min = target_pts(i,j);
            }
            else if(target_pts(i,j) > max) {
                max = target_pts(i,j);
            }
        }
    }
    // source points
    for(long int i = 0; i < source_pts.rows(); ++i) {
        for(long int j = 0; j < source_pts.cols(); ++j) {
            if(source_pts(i,j) < min) {
                min = source_pts(i,j);
            }
            else if(source_pts(i,j) > max) {
                max = source_pts(i,j);
            }
        }
    }
    return std::make_pair(min, max);
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
