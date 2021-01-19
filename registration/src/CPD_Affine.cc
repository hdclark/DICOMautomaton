#include "YgorFilesDirs.h"    //Needed for Does_File_Exist_And_Can_Be_Read(...), etc..
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorMath.h"         //Needed for samples_1D.
#include "YgorString.h"       //Needed for GetFirstRegex(...)

#include "CPD_Affine.h"
#include "YgorMathIOXYZ.h"    //Needed for ReadPointSetFromXYZ.

AffineCPDTransform::AffineCPDTransform(int dimensionality) {
    this->B = Eigen::MatrixXd::Identity(dimensionality, dimensionality);
    this->t = Eigen::VectorXd::Zero(dimensionality);
    this->dim = dimensionality;
}

void AffineCPDTransform::apply_to(point_set<double> &ps) {
    auto N_points = static_cast<long int>(ps.points.size());
    Eigen::MatrixXd Y = Eigen::MatrixXd::Zero(N_points, this->dim); 
    // Fill the X vector with the corresponding points.
    for(long int j = 0; j < N_points; ++j) { // column
        auto P = ps.points[j];
        Y(j, 0) = P.x;
        Y(j, 1) = P.y;
        Y(j, 2) = P.z;
    }
    auto Y_hat = Y*this->B.transpose() + \
        Eigen::MatrixXd::Constant(N_points, 1, 1)*this->t.transpose();
    for(long int j = 0; j < N_points; ++j) { // column
        ps.points[j].x = Y_hat(j, 0);
        ps.points[j].y = Y_hat(j, 1);
        ps.points[j].z = Y_hat(j, 2);
    }
}

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

Eigen::MatrixXd CalculateB(const Eigen::MatrixXd & xHat,
            const Eigen::MatrixXd & yHat,
            const Eigen::MatrixXd & postProb) {
    
    Eigen::MatrixXd oneVec = Eigen::MatrixXd::Ones(postProb.cols(),1);
    Eigen::MatrixXd left = xHat.transpose() * postProb.transpose() * yHat;
    Eigen::MatrixXd right = (yHat.transpose() * (postProb * oneVec).asDiagonal() * yHat).inverse();

    return left * right;
}

double SigmaSquared(const Eigen::MatrixXd & B,
            const Eigen::MatrixXd & xHat,
            const Eigen::MatrixXd & yHat,
            const Eigen::MatrixXd & postProb) {
    
    double dimensionality = yHat.cols();
    double Np = postProb.sum();
    
    Eigen::MatrixXd oneVec = Eigen::MatrixXd::Ones(postProb.rows(),1);
    double left = (double)(xHat.transpose() * (postProb.transpose() * oneVec).asDiagonal() * xHat).trace();
    double right = (double)(xHat.transpose() * postProb.transpose() * yHat * B.transpose()).trace();

    return (left - right) / (Np * dimensionality);
    
}

// This function is where the deformable registration algorithm should be implemented.
AffineCPDTransform
AlignViaAffineCPD(CPDParams & params,
            const point_set<double> & moving,
            const point_set<double> & stationary, 
            int iter_interval /*= 0*/,
            std::string video /*= "False"*/,
            std::string xyz_outfile /*= "output"*/){

    FUNCINFO("Performing Affine CPD");
    
    std::string temp_xyz_outfile;
    point_set<double> mutable_moving = moving;

    const auto N_move_points = static_cast<long int>(moving.points.size());
    const auto N_stat_points = static_cast<long int>(stationary.points.size());

    // Prepare working buffers.
    //
    // Stationary point matrix
    Eigen::MatrixXd X = Eigen::MatrixXd::Zero(N_move_points, params.dimensionality);
    // Moving point matrix
    Eigen::MatrixXd Y = Eigen::MatrixXd::Zero(N_stat_points, params.dimensionality); 

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
    Eigen::MatrixXd P;
    Eigen::MatrixXd Ux;
    Eigen::MatrixXd Uy;
    Eigen::MatrixXd X_hat;
    Eigen::MatrixXd Y_hat;
    // params.iterations = 50;
    for (int i = 0; i < params.iterations; i++) {
        FUNCINFO("Iteration: " << i)
        P = E_Step(X, Y, transform.B, \
            transform.t, sigma_squared, params.distribution_weight, 1);
        Ux = CalculateUx(X, P);
        Uy = CalculateUy(Y, P);
        X_hat = CenterMatrix(X, Ux);
        Y_hat = CenterMatrix(Y, Uy);
        transform.B = CalculateB(X_hat, Y_hat, P);
        transform.t = GetTranslationVector(transform.B, Ux, Uy, 1);
        sigma_squared = SigmaSquared(transform.B, X_hat, Y_hat, P);

        mutable_moving = moving;
        transform.apply_to(mutable_moving);
        
        if (video == "True") {
            if (iter_interval > 0 && i % iter_interval == 0) {
                temp_xyz_outfile = xyz_outfile + "_iter" + std::to_string(i) + ".xyz";
                std::ofstream PFO(temp_xyz_outfile);
                if(!WritePointSetToXYZ(mutable_moving, PFO))
                    FUNCERR("Error writing point set to " << xyz_outfile);
            }
        }

        similarity = GetSimilarity(X, Y, P, transform.B, transform.t, 1, sigma_squared);
        if(similarity < params.similarity_threshold)
            break;
    }
    return transform;
}

