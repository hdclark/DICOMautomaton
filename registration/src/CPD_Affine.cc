#include "YgorFilesDirs.h"    //Needed for Does_File_Exist_And_Can_Be_Read(...), etc..
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorMath.h"         //Needed for samples_1D.
#include "YgorString.h"       //Needed for GetFirstRegex(...)
#include "CPD_Affine.h"
#include "YgorMathIOXYZ.h"    //Needed for ReadPointSetFromXYZ.
#include <iostream>
#include <cmath>
#include <chrono>
using namespace std::chrono;

// Initialize affine transform
AffineCPDTransform::AffineCPDTransform(int dimensionality) {
    this->B = Eigen::MatrixXf::Identity(dimensionality, dimensionality);
    this->t = Eigen::VectorXf::Zero(dimensionality);
    this->dim = dimensionality;
}

// Apply affine transform to point set
void AffineCPDTransform::apply_to(point_set<double> &ps) {
    auto N_points = static_cast<long int>(ps.points.size());
    Eigen::MatrixXf Y = Eigen::MatrixXf::Zero(N_points, this->dim); 
    
    // Fill the X vector with the corresponding points.
    for(long int j = 0; j < N_points; ++j) { // column
        auto P = ps.points[j];
        Y(j, 0) = P.x;
        Y(j, 1) = P.y;
        Y(j, 2) = P.z;
    }
    auto Y_hat = Y*this->B.transpose() + \
        Eigen::MatrixXf::Constant(N_points, 1, 1)*this->t.transpose();
    for(long int j = 0; j < N_points; ++j) { // column
        ps.points[j].x = Y_hat(j, 0);
        ps.points[j].y = Y_hat(j, 1);
        ps.points[j].z = Y_hat(j, 2);
    }
}

// Write affine transformation to file
void AffineCPDTransform::write_to( std::ostream &os ) {
    for(int i = 0; i < this->dim; i++) {
        for(int j = 0; j < this->dim; j++) {
            os << this->B(i, j);
            os << " ";
        }
        os << "0\n";
    }
    for(int j = 0; j < this->dim; j++) {
        os << this->t(j);
        os << " ";
    }
    os << "0\n";
}

// Read affine transformation from file
bool AffineCPDTransform::read_from( std::istream &is ) {
    affine_transform<double> tf;
    bool success = tf.read_from(is);
    if (!success)
        return success;
    for(int i = 0; i < this->dim; i++) {
        for(int j = 0; j < this->dim; j++) {
            this->B(i,j) = tf.coeff(i, j);
        }
    }
    for(int j = 0; j < this->dim; j++) {
        tf.coeff(3, j) = this->t(j);
    }
    return success;
}

// Function to calculate affine transformation matrix
Eigen::MatrixXf CalculateB(const Eigen::MatrixXf & xHat,
            const Eigen::MatrixXf & yHat,
            const Eigen::MatrixXf & postProb) {
    
    Eigen::MatrixXf oneVec = Eigen::MatrixXf::Ones(postProb.cols(),1);
    Eigen::MatrixXf left = xHat.transpose() * postProb.transpose() * yHat;
    Eigen::MatrixXf right = (yHat.transpose() * (postProb * oneVec).asDiagonal() * yHat).inverse();

    return left * right;
}

// Function to calculate sigma squared
double SigmaSquared(const Eigen::MatrixXf & B,
            const Eigen::MatrixXf & xHat,
            const Eigen::MatrixXf & yHat,
            const Eigen::MatrixXf & postProb) {
    
    double dimensionality = yHat.cols();
    double Np = postProb.sum();
    
    Eigen::MatrixXf oneVec = Eigen::MatrixXf::Ones(postProb.rows(),1);
    double left = (double)(xHat.transpose() * (postProb.transpose() * oneVec).asDiagonal() * xHat).trace();
    double right = (double)(xHat.transpose() * postProb.transpose() * yHat * B.transpose()).trace();

    return (left - right) / (Np * dimensionality);
    
}

// Main loop for affine registration algorithm
AffineCPDTransform
AlignViaAffineCPD(CPDParams & params,
            const point_set<double> & moving,
            const point_set<double> & stationary, 
            int iter_interval,
            std::string video,
            std::string xyz_outfile) {

    FUNCINFO("Performing Affine CPD");
    
    std::string temp_xyz_outfile;
    point_set<double> mutable_moving = moving;

    const auto N_move_points = static_cast<long int>(moving.points.size());
    const auto N_stat_points = static_cast<long int>(stationary.points.size());

    // Prepare working buffers:
    
    // Stationary point matrix
    Eigen::MatrixXf X = Eigen::MatrixXf::Zero(N_move_points, params.dimensionality);
    // Moving point matrix
    Eigen::MatrixXf Y = Eigen::MatrixXf::Zero(N_stat_points, params.dimensionality); 

    // Optional: print out the number of points in each point set
    // FUNCINFO("Number of moving points: " << N_move_points);
    // FUNCINFO("Number of stationary pointsL  " << N_stat_points);
    // FUNCINFO("Initializing...")

    // Fill the X vector with the corresponding points.
    for(long int j = 0; j < N_stat_points; ++j){ // column
        const auto P_stationary = stationary.points[j];
        X(j, 0) = P_stationary.x;
        X(j, 1) = P_stationary.y;
        X(j, 2) = P_stationary.z;
    }

    // Fill the Y vector with the corresponding points.
    for(long int j = 0; j < N_move_points; ++j){ // column
        const auto P_moving = moving.points[j];
        Y(j, 0) = P_moving.x;
        Y(j, 1) = P_moving.y;
        Y(j, 2) = P_moving.z;
    }

    AffineCPDTransform transform(params.dimensionality);
    double sigma_squared = Init_Sigma_Squared(X, Y);
    double similarity;
    double objective;
    double prev_objective = 0;
    Eigen::MatrixXf P;
    Eigen::MatrixXf Ux;
    Eigen::MatrixXf Uy;
    Eigen::MatrixXf X_hat;
    Eigen::MatrixXf Y_hat;

    // Print stats to outfile_stats.csv
    std::ofstream os(xyz_outfile + "_stats.csv");
    os << "iteration" << "," << "time" << "," << "similarity" << "," << "outfile" << "\n";
    os << 0 << "," << 0 << "," << similarity << "," << xyz_outfile + "_iter0.xyz" << "\n";
    high_resolution_clock::time_point start = high_resolution_clock::now();

    // Optional: verbose
    // FUNCINFO("Starting loop. Max Iterations: " << params.iterations)

    for (int i = 0; i < params.iterations; i++) {
        // Optional: print out current iteration
        FUNCINFO("Iteration: " << i)

        P = E_Step(X, Y, transform.B, transform.t, sigma_squared, params.distribution_weight, 1);
        Ux = CalculateUx(X, P);
        Uy = CalculateUy(Y, P);
        X_hat = CenterMatrix(X, Ux);
        Y_hat = CenterMatrix(Y, Uy);
        transform.B = CalculateB(X_hat, Y_hat, P);
        transform.t = GetTranslationVector(transform.B, Ux, Uy, 1);
        sigma_squared = SigmaSquared(transform.B, X_hat, Y_hat, P);

        mutable_moving = moving;
        transform.apply_to(mutable_moving);

        // Calculate similarity and objective functions
        similarity = GetSimilarity(X, Y, transform.B, transform.t, 1);
        objective = GetObjective(X, Y, P, transform.B, transform.t, 1, sigma_squared);
        
        // Optional: print out similarity and objective values
        FUNCINFO(similarity);
        FUNCINFO(objective);
        
        // Write point sets for video tool
        if (video == "True") {
            if (iter_interval > 0 && i % iter_interval == 0) {
                temp_xyz_outfile = xyz_outfile + "_iter" + std::to_string(i+1) + "_sim" + std::to_string(similarity) + ".xyz";
                std::ofstream PFO(temp_xyz_outfile);
                if(!WritePointSetToXYZ(mutable_moving, PFO))
                    FUNCERR("Error writing point set to " << xyz_outfile);
            }
        }
        
        // Break loop if difference in objective function is below the threshold
        if(abs(prev_objective-objective) < params.similarity_threshold)
            break;

        // Write stats for this iteration to outfile_stats.csv
        high_resolution_clock::time_point stop = high_resolution_clock::now();
        duration<double>  time_span = duration_cast<duration<double>>(stop - start);
        FUNCINFO("Excecution took time: " << time_span.count())

        os << i+1 << "," << time_span.count() << "," << similarity << "," << temp_xyz_outfile << "\n";
    }
    return transform;
}

