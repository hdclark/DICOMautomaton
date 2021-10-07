#include "YgorFilesDirs.h"    //Needed for Does_File_Exist_And_Can_Be_Read(...), etc..
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorMath.h"         //Needed for samples_1D.
#include "YgorString.h"       //Needed for GetFirstRegex(...)
#include "CPD_Rigid.h"
#include "YgorMathIOXYZ.h"    //Needed for ReadPointSetFromXYZ.
#include "YgorFilesDirs.h"    //Needed for Does_File_Exist_And_Can_Be_Read(...), etc..
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorMath.h"         //Needed for samples_1D.
#include <chrono>
#include <cmath>
#include <iostream>
#include <math.h>
using namespace std::chrono;
RigidCPDTransform::RigidCPDTransform(int dimensionality) {
    this->dim = dimensionality;
    this->R = Eigen::MatrixXf::Identity(dimensionality, dimensionality);
    this->t = Eigen::MatrixXf::Zero(dimensionality, 1);
    this->s = 1;
}

void RigidCPDTransform::apply_to(point_set<double> &ps) {
    const auto N_points = static_cast<long int>(ps.points.size());

    Eigen::MatrixXf Y = Eigen::MatrixXf::Zero(N_points, this->dim); 

    // Fill the X vector with the corresponding points.
    for(long int j = 0; j < N_points; ++j) { // column
        const auto P = ps.points[j];
        Y(j, 0) = P.x;
        Y(j, 1) = P.y;
        Y(j, 2) = P.z;
    }

    auto Y_hat = this->s*Y*this->R.transpose() + \
        Eigen::MatrixXf::Constant(N_points, 1, 1)*this->t.transpose();
    
    for(long int j = 0; j < N_points; ++j) { // column
        ps.points[j].x = Y_hat(j, 0);
        ps.points[j].y = Y_hat(j, 1);
        ps.points[j].z = Y_hat(j, 2);
    }
}

Eigen::MatrixXf RigidCPDTransform::get_sR() {
    return this->s * this->R;
}

void RigidCPDTransform::write_to( std::ostream &os ) {
    affine_transform<double> tf;
    Eigen::MatrixXf sR = get_sR();
    for(int i = 0; i < this->dim; i++) {
        for(int j = 0; j < this->dim; j++) {
            os << sR(i, j);
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

bool RigidCPDTransform::read_from( std::istream &is ) {
    affine_transform<double> tf;
    bool success = tf.read_from(is);
    if (!success)
        return success;
    this->s = 1;
    for(int i = 0; i < this->dim; i++) {
        for(int j = 0; j < this->dim; j++) {
            this->R(i,j) = tf.coeff(i, j);
        }
    }
    for(int j = 0; j < this->dim; j++) {
        tf.coeff(3, j) = this->t(j);
    }
    return success;
}

Eigen::MatrixXf GetA(const Eigen::MatrixXf & xHat,
            const Eigen::MatrixXf & yHat,
            const Eigen::MatrixXf & postProb){
    
    return xHat.transpose() * postProb.transpose() * yHat; 
}

// R is a square D x D matrix, so U and V will both be square
Eigen::MatrixXf GetRotationMatrix(const Eigen::MatrixXf & U,
            const Eigen::MatrixXf & V){
     
    Eigen::MatrixXf C = Eigen::MatrixXf::Identity(U.cols(), V.cols());
    C(C.rows()-1, C.cols()-1) = (U * V.transpose()).determinant();
    return U * C * V.transpose();
}

double GetS(const Eigen::MatrixXf & A,
            const Eigen::MatrixXf & R,
    // params.iterations = 50;
            const Eigen::MatrixXf & yHat,
            const Eigen::MatrixXf & postProb ){
    Eigen::MatrixXf oneVec = Eigen::MatrixXf::Ones(postProb.cols(),1);
    double numer = (A.transpose() * R).trace();
    double denom = (yHat.transpose() * (postProb * oneVec).asDiagonal() * yHat).trace();
    return numer / denom;
}

double SigmaSquared(double s,
            const Eigen::MatrixXf & A,
            const Eigen::MatrixXf & R,
            const Eigen::MatrixXf & xHat,
            const Eigen::MatrixXf & postProb){

    double dimensionality = xHat.cols();
    double Np = postProb.sum();
    
    Eigen::MatrixXf oneVec = Eigen::MatrixXf::Ones(postProb.rows(),1);
    double left = (double)(xHat.transpose() * (postProb.transpose() * oneVec).asDiagonal() * xHat).trace();
    double right = s * (A.transpose() * R).trace();

    return (left - right) / (Np * dimensionality);
}

// This function is where the deformable registration algorithm should be implemented.
RigidCPDTransform
AlignViaRigidCPD(CPDParams & params,
            const point_set<double> & moving,
            const point_set<double> & stationary,
            int iter_interval /*= 0*/,
            std::string video /*= "False"*/,
            std::string xyz_outfile /*= "output"*/ ){
    FUNCINFO("Performing rigid CPD")
    high_resolution_clock::time_point start = high_resolution_clock::now();
    std::string temp_xyz_outfile;
    point_set<double> mutable_moving = moving;

    const auto N_move_points = static_cast<long int>(moving.points.size());
    const auto N_stat_points = static_cast<long int>(stationary.points.size());

    // Prepare working buffers.
    //
    // Stationary point matrix
    Eigen::MatrixXf X = Eigen::MatrixXf::Zero(N_move_points, params.dimensionality);
    // Moving point matrix
    Eigen::MatrixXf Y = Eigen::MatrixXf::Zero(N_stat_points, params.dimensionality); 
    FUNCINFO("Number of moving points: " << N_move_points);
    FUNCINFO("Number of stationary pointsL  " << N_stat_points);
    FUNCINFO("Initializing...")
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

    RigidCPDTransform transform(params.dimensionality);
    double sigma_squared = Init_Sigma_Squared(X, Y);
    double similarity;
    double objective;
    double prev_objective = 0;

    Eigen::MatrixXf P;
    Eigen::MatrixXf Ux;
    Eigen::MatrixXf Uy;
    Eigen::MatrixXf X_hat;
    Eigen::MatrixXf Y_hat;
    Eigen::MatrixXf A;
    
    std::ofstream os(xyz_outfile + "_stats.csv");
    FUNCINFO("Starting loop. Max Iterations: " << params.iterations)
    for (int i = 0; i < params.iterations; i++) {
        FUNCINFO("Starting Iteration: " << i)
        P = E_Step(X, Y, transform.R, \
            transform.t, sigma_squared, params.distribution_weight, transform.s);
        Ux = CalculateUx(X, P);
        Uy = CalculateUy(Y, P);
        X_hat = CenterMatrix(X, Ux);
        Y_hat = CenterMatrix(Y, Uy);
        A = GetA(X_hat, Y_hat, P);
        Eigen::JacobiSVD<Eigen::MatrixXf> svd( A, Eigen::ComputeFullV | Eigen::ComputeFullU );
        transform.R = GetRotationMatrix(svd.matrixU(), svd.matrixV());
        transform.s = GetS(A, transform.R, Y_hat, P);
        transform.t = GetTranslationVector(transform.R, Ux, Uy, transform.s);
        sigma_squared = SigmaSquared(transform.s, A, transform.R, X_hat, P);

        if (isnan(sigma_squared)) {
            FUNCINFO("FINAL SIMILARITY: " << similarity);
            break;
        }

        mutable_moving = moving;
        transform.apply_to(mutable_moving);

        similarity = GetSimilarity(X, Y, transform.R, transform.t, transform.s);
        objective = GetObjective(X, Y, P, transform.R, transform.t, transform.s, sigma_squared);
        FUNCINFO("Similarity: " << similarity);
        FUNCINFO("Objective: " << objective);
        
        if (video == "True") {
            if (iter_interval > 0 && i % iter_interval == 0) {
                temp_xyz_outfile = xyz_outfile + "_iter" + std::to_string(i+1) + "_sim" + std::to_string(similarity) + ".xyz";
                std::ofstream PFO(temp_xyz_outfile);
                if(!WritePointSetToXYZ(mutable_moving, PFO))
                    FUNCERR("Error writing point set to " << xyz_outfile);
            }
        }
        if(abs(prev_objective-objective) < params.similarity_threshold)
            break;
        high_resolution_clock::time_point stop = high_resolution_clock::now();
        duration<double>  time_span = duration_cast<duration<double>>(stop - start);
        FUNCINFO("Excecution took time: " << time_span.count())

        os << i+1 << "," << time_span.count() << "," << similarity << "," << temp_xyz_outfile << "\n";

    }
    return transform;
}
