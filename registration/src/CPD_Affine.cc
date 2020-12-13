#include "CPD_Affine.h"

AffineCPDTransform::AffineCPDTransform(int dimensionality) {
    this->B = Eigen::MatrixXd::Identity(dimensionality, dimensionality);
    this->t = Eigen::VectorXd::Zero(dimensionality);
    this->dim = dimensionality;
}

void AffineCPDTransform::apply_to(point_set<double> &ps) {
    FUNCINFO("Applying transform to point set")
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

bool AffineCPDTransform::write_to( std::ostream &os ) {

}

bool AffineCPDTransform::read_from( std::istream &is ) {

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

    std::cout << "\n diff";
    std::cout << (left - right);
    std::cout << "\n Np: ";
    std::cout << Np; 
    std::cout << "\n dims: ";
    std::cout << dimensionality; 


    return (left - right) / (Np * dimensionality);
    
}

// This function is where the deformable registration algorithm should be implemented.
AffineCPDTransform
AlignViaAffineCPD(CPDParams & params,
            const point_set<double> & moving,
            const point_set<double> & stationary ){

    // if(moving.points.empty() || stationary.points.empty()){
    //     FUNCWARN("Unable to perform ABC alignment: a point set is empty");
    //     return std::nullopt;
    // }

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
    double prev_sigma_squared;
    double sigma_squared = Init_Sigma_Squared(X, Y);
    Eigen::MatrixXd P;
    Eigen::MatrixXd Ux;
    Eigen::MatrixXd Uy;
    Eigen::MatrixXd X_hat;
    Eigen::MatrixXd Y_hat;
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
        if (sigma_squared < 0.00001)
            break;
    }
    return transform;
}

