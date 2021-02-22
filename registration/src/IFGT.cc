#include <math.h>
/*
#include "YgorFilesDirs.h"    //Needed for Does_File_Exist_And_Can_Be_Read(...), etc..
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorMath.h"         //Needed for samples_1D.
#include "YgorString.h"       //Needed for GetFirstRegex(...)
*/
#include "IFGT.h"
#include <limits>
#include <algorithm> // dunno if its in docker
#include <cmath>


// autotuning of ifgt parameters
Ifgt_params ifgt_choose_parameters(int dimensionality, 
                        double bandwidth, 
                        double epsilon) {

    int d = dimensionality;
    double h_square = bandwidth * bandwidth;
    double min_complexity = std::numeric_limits<double>::max(); 
    int nclusters = 0; 
    int max_clusters = round(0.2 * 100 / bandwidth); // dont know where this comes from lol
    int p_ul = 200; // estimatee 
    int p_max = 0;
    double R = sqrt(d);

    double r = std::min(R, bandwidth * sqrt(log(1 / epsilon)));


    for(int i = 0; i < max_clusters; i++) {
        double rx = pow((double)i+1,-1.0 / (double)d); // rx ~ K^{-1/d} estimate for uniform datasets
		double rx_square = rx*rx;

		double n = std::min((double) (i+1), pow(r/rx, (double) d));
		double error=1;
	    double temp=1;
		int p = 0;

		while((error > epsilon) & (p <= p_ul)){
			p++;
			double b = std::min(((rx + sqrt((rx_square)+(2*p*h_square)))/2), rx + r);
			double c = rx - b;
			temp = temp * (((2 * rx * b) / h_square)/p);
			error = temp * (exp(-(c*c)/h_square));			
		}	
		double complexity = (i+1) + log((double)i+1) + ((1+n)*nchoosek(p - 1 + d,d));

		std::cout << "complexity " << complexity << std::endl;
        std::cout << "error " << error << std::endl;

		if (complexity < min_complexity ){
			min_complexity = complexity;
			nclusters = i+1;
			p_max = p;		
		}
	}
    Ifgt_params params; 
    params.nclusters = nclusters;
    params.p_max = p_max; 
    return params;
}


// after clustering, reupdate the truncation based on actual max 
int ifgt_update_max_truncation(int dimensionality, 
                            double bandwidth, 
                            double epsilon,
                            double rx_max) {

    int truncation_number_ul = 200; // j
    int d = dimensionality;
    double R = sqrt(d);                            
    double h2 = bandwidth * bandwidth;
    double rx2 = rx_max * rx_max;
    double r = std::min(R, bandwidth * sqrt(log(1.0 / epsilon)));
    double error = std::numeric_limits<double>::max();

    int p = 0;
    double temp = 1.0;
    while ((error > epsilon) && (p <= truncation_number_ul)) {
        ++p;
        double b = std::min((rx_max + sqrt(rx2 + 2 * double(p) * h2)) / 2.0, rx_max + r);
        double c = rx_max - b;
        temp *= 2 * rx_max * b / h2 / double(p);
        error = temp * std::exp(-(c * c) / h2);
    }
    return p;
}
/*
// computes 2^alpha/alpha!
Eigen::MatrixXd compute_constant_series(int dimensionality, 
                                        int pmax_total) {}

// computes [(y_j-c_k)/h]^{alpha} or [(x_i-c_k)/h]^{alpha}
// where y_j is target point, x_i is point inside cluster
Eigen::MatrixXd compute_target_monomials(int dimensionality,
                                        double bandwidth,
                                        const Eigen::VectorXd & delta) {}

// computes (y_j-c_k) or (x_i-c_k)
Eigen::MatrixXd compute_d(int dimensionality,
                        const Eigen::MatrixXd & cluster_centre,
                        const Eigen::MatrixXd & point) {}

Eigen::MatrixXd evaluate_ifgt(Ifgt_params parameters, 
                            const Eigen::MatrixXd & points, 
                            double bandwidth, 
                            double epsilon) {}

Eigen::MatrixXd compute_ck(int dimensionality,
                        double bandwidth
                        int pmax_total,
                        int p_max
                        int n_clusters,
                        Cluster * clusters,
                        const Eigen::MatrixXd & points){}

*/
int nchoosek(int n, int k){
	int n_k = n - k;
	
	if (k < n_k)
	{
		k = n_k;
		n_k = n - k;
	}
	int nchsk = 1; 
	for ( int i = 1; i <= n_k; i++)
	{
		nchsk *= (++k);
		nchsk /= i;
	}
	return nchsk;
}

int main (int argc, char *argv[]) {
    int dimensionality = 3;
    double bandwidth = 2.0;
    double epsilon = 0.001;

    Ifgt_params params;
    
    params = ifgt_choose_parameters(dimensionality, bandwidth, epsilon);

    std::cout << params.nclusters << std::endl;
    std::cout << params.p_max << std::endl;

    return 0;
 }