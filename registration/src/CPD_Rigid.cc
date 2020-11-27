#include "CPD_Rigid.h"

RigidCPDTransform::RigidCPDTransform(int dimensionality) {
    this->dim = dimensionality;
    this->R = Eigen::MatrixXd::Identity(dimensionality, dimensionality);
    this->t = Eigen::MatrixXd::Zero(dimensionality, 1);
    this->s = 1;
}

void RigidCPDTransform::apply_to(point_set<double> &ps) {
    const auto N_points = static_cast<long int>(ps.points.size());

    Eigen::MatrixXd Y = Eigen::MatrixXd::Zero(N_points, this->dim); 

    // Fill the X vector with the corresponding points.
    for(long int j = 0; j < N_points; ++j) { // column
        const auto P = ps.points[j];
        Y(j, 0) = P.x;
        Y(j, 1) = P.y;
        Y(j, 2) = P.z;
    }

    auto Y_hat = this->s*Y*this->R.transpose() + \
        Eigen::MatrixXd::Constant(N_points, 1, 1)*this->t.transpose();
    
    for(long int j = 0; j < N_points; ++j) { // column
        ps.points[j].x = Y_hat(j, 0);
        ps.points[j].y = Y_hat(j, 1);
        ps.points[j].z = Y_hat(j, 2);
    }
}

Eigen::MatrixXd RigidCPDTransform::get_sR() {
    return this->s * this->R;
}

bool RigidCPDTransform::write_to( std::ostream &os ) {

}

bool RigidCPDTransform::read_from( std::istream &is ) {

}

// This function is where the deformable registration algorithm should be implemented.
std::optional<RigidCPDTransform>
AlignViaRigidCPD(CPDParams & params,
            const point_set<double> & moving,
            const point_set<double> & stationary ){

    if(moving.points.empty() || stationary.points.empty()){
        FUNCWARN("Unable to perform ABC alignment: a point set is empty");
        return std::nullopt;
    }

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
    RigidCPDTransform transform(params.dimensionality);
    double sigma_squared = Init_Sigma_Squared(X, Y);
    double Np;
    double Ux;
    double Uy;
    for (int i = 0; i < params.iterations; i++) {
        Eigen::MatrixXd P = E_Step(X, Y, transform.get_sR(), \
            transform.t, sigma_squared, params.distribution_weight);
        Np = CalculateNp(P);
        Eigen::MatrixXd Ux = CalculateUx(Np, X, P);
        Eigen::MatrixXd Uy = CalculateUy(Np, Y, P);
        Eigen::MatrixXd X_hat = CenterMatrix(X, Ux);
        Eigen::MatrixXd Y_hat = CenterMatrix(X, Uy);
        Eigen::MatrixXd A = GetA(X_hat, Y_hat, P);
        Eigen::JacobiSVD<Eigen::MatrixXd> svd( A, Eigen::ComputeFullV | Eigen::ComputeFullU );
        transform.R = GetRotationMatrix(svd.matrixU(), svd.matrixV());
        transform.s = GetS(A, transform.R, Y_hat, P);
        transform.t = GetTranslationVector(transform.R, Ux, Uy, transform.s);
        sigma_squared = SigmaSquared(Np, transform.s, A, transform.R, X_hat, P);
    }

    return transform;
}

