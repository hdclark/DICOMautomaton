#include <math.h>

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
// TODO: finish IFGT constructor 
// TODO: INTEGRATE WITH TESTING

IFGT::IFGT(const Eigen::MatrixXd & source_points,
            double bandwidth, 
            double epsilon) {
    
    this->bandwidth = bandwidth;
    this->epsilon = epsilon;
    dim = source_points.cols();

    ifgt_choose_parameters();
    // clustering goes here
    double rx_max = cluster.rx_max;
    ifgt_update_max_truncation(double rx_max); // update max truncation
    p_max_total = nchoosek(max_truncation_p-1+dim,dim); 
    compute_constant_series();

        
}
// TODO MAIN FUNCTION 
Eigen::MatrixXd run_ifgt(const Eigen::MatrixXd & source_points,
                        const Eigen::MatrixXd & target_points) {
    



}

// autotuning of ifgt parameters
void IFGT::ifgt_choose_parameters() {

    double h_square = bandwidth * bandwidth;
    double min_complexity = std::numeric_limits<double>::max(); 
    int n_clusters = 0; 
    int max_clusters = round(0.2 * 100 / bandwidth); // dont know where this comes from lol
    int p_ul = 200; // estimate 
    int max_truncation = 0;
    double R = sqrt(dim);

    double radius = std::min(R, bandwidth * sqrt(log(1 / epsilon)));

    for(int i = 0; i < max_clusters; i++) {
        double rx = pow((double)i + 1, -1.0 / (double) dim); // rx ~ K^{-1/dim} estimate for uniform datasets
		double rx_square = rx*rx;

		double n = std::min((double) (i+1), pow(radius/rx, (double) dim));
		double error = 1;
	    double temp = 1;
		int p = 0;

		while((error > epsilon) & (p <= p_ul)){
			p++;
			double b = std::min(((rx + sqrt((rx_square)+(2*p*h_square)))/2), rx + radius);
			double c = rx - b;
			temp = temp * (((2 * rx * b) / h_square)/p);
			error = temp * (exp(-(c*c)/h_square));			
		}	
		double complexity = (i+1) + log((double)i+1) + ((1+n)*nchoosek(p - 1 + dim, dim));

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
    double R = sqrt(dim);                            
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
Eigen::MatrixXd IFGT::compute_ck(const Eigen::MatrixXd & source_points){

    int N_source_points = source_points.rows();
    double h_square = bandwidth * bandwidth;

    Eigen::MatrixXd C = Eigen::Matrix::Zero(n_clusters, p_max_total);
    for (int i = 0; i < N_source_points; ++i) {
        double distance = 0.0;
        Eigen::MatrixXd dx = Eigen::MatrixXd::Zero(dim,1);
        int cluster_index = clusters.indices[i];

        for (int k = 0; k < dim; ++k) {
            double delta = source_points(i, k) -  clusters.centres(cluster_index, k);
            distance += delta * delta;
            dx(k) = delta / bandwidth;
        }
        Eigen::MatrixXd monomials = compute_monomials(dx);
        double f = std::exp(-distance / h_square); // multiply by vector of weights if needed
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

Eigen::VectorXd IFGT::compute_gaussian(const Eigen::MatrixXd & target_points
                                       const Eigen::MatrixXd & C_k){

    int M_target_points = target_points.rows();
    double h_square = bandwidth * bandwidth;

    Eigen::MatrixXd G_y = Eigen::MatrixXd::Zero(M_target_points,1);

    double m_ry_square[n_clusters];
    for (int j = 0; j < n_clusters; ++j) {
        double ry = radius + clusters.radii[j]; //UPDATE WITH CLUSTERS
        m_ry_square[j] = ry * ry;
    }

    for (int i = 0; i < M_target_points; ++i) {
        for (int j = 0; j < n_clusters; ++j) {
            double distance = 0.0;
            Eigen::MatrixXd dy = Eigen::VectorXd::Zero(dim);

            for (int k = 0; k < dim; ++k) {
                double delta = target_points(i, k) - clusters.cluster_centres(j, k); // struct names tbd 
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

Eigen::MatrixXd IFGT::run_ifgt(const Eigen::MatrixXd & target_points) {


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