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

Eigen::MatrixXd GetA(const Eigen::MatrixXd & xHat,
            const Eigen::MatrixXd & yHat,
            const Eigen::MatrixXd & postProb){
    
    return xHat.transpose() * postProb.transpose() * yHat; 
}

// R is a square D x D matrix, so U and V will both be square
Eigen::MatrixXd GetRotationMatrix(const Eigen::MatrixXd & U,
            const Eigen::MatrixXd & V){
     
    Eigen::MatrixXd C = Eigen::MatrixXd::Identity(U.cols(), V.cols());
    C(C.rows()-1, C.cols()-1) = (U * V.transpose()).determinant();

    return U * C * V.transpose();
}

double GetS(const Eigen::MatrixXd & A,
            const Eigen::MatrixXd & R,
            const Eigen::MatrixXd & yHat,
            const Eigen::MatrixXd & postProb ){
    Eigen::MatrixXd oneVec = Eigen::MatrixXd::Ones(postProb.cols(),1);
    double numer = (A.transpose() * R).trace();
    double denom = (yHat.transpose() * (postProb * oneVec).asDiagonal() * yHat).trace();
    return numer / denom;
}

double SigmaSquared(double s,
            const Eigen::MatrixXd & A,
            const Eigen::MatrixXd & R,
            const Eigen::MatrixXd & xHat,
            const Eigen::MatrixXd & postProb){

    double dimensionality = xHat.cols();
    double Np = postProb.sum();
    
    Eigen::MatrixXd oneVec = Eigen::MatrixXd::Ones(postProb.rows(),1);
    double left = (double)(xHat.transpose() * (postProb.transpose() * oneVec).asDiagonal() * xHat).trace();
    double right = s * (A.transpose() * R).trace();

    return (left - right) / (Np * dimensionality);
}

// This function is where the deformable registration algorithm should be implemented.
std::optional<RigidCPDTransform>
AlignViaRigidCPD(CPDParams & params,
            const point_set<double> & moving,
            const point_set<double> & stationary ){
    FUNCINFO("Performing rigid CPD")
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
    FUNCINFO(X)
    FUNCINFO(Y)
    RigidCPDTransform transform(params.dimensionality);
    double sigma_squared = Init_Sigma_Squared(X, Y);
    FUNCINFO(sigma_squared)

    FUNCINFO("Starting loop. Iterations: " << params.iterations)
    for (int i = 0; i < params.iterations; i++) {
        FUNCINFO(i)
        FUNCINFO("E step")
        Eigen::MatrixXd P = E_Step(X, Y, transform.get_sR(), \
            transform.t, sigma_squared, params.distribution_weight);
        FUNCINFO("Calculate Ux")
        Eigen::MatrixXd Ux = CalculateUx(X, P);
        FUNCINFO("Calculate Uy")
        Eigen::MatrixXd Uy = CalculateUy(Y, P);
        FUNCINFO("Calculate hats")
        Eigen::MatrixXd X_hat = CenterMatrix(X, Ux);
        Eigen::MatrixXd Y_hat = CenterMatrix(X, Uy);
        FUNCINFO("Calculate A")
        Eigen::MatrixXd A = GetA(X_hat, Y_hat, P);
        FUNCINFO("Jacobi")
        Eigen::JacobiSVD<Eigen::MatrixXd> svd( A, Eigen::ComputeFullV | Eigen::ComputeFullU );
        FUNCINFO("Rotation")
        transform.R = GetRotationMatrix(svd.matrixU(), svd.matrixV());
        FUNCINFO("Scale")
        transform.s = GetS(A, transform.R, Y_hat, P);
        FUNCINFO(transform.s)
        FUNCINFO("Translation")
        transform.t = GetTranslationVector(transform.R, Ux, Uy, transform.s);
        FUNCINFO("Getting sigma")
        sigma_squared = SigmaSquared(transform.s, A, transform.R, X_hat, P);
        FUNCINFO(sigma_squared)
        if(sigma_squared == 0)
            break;
    }
    FUNCINFO(X)
    FUNCINFO(Y)
    FUNCINFO(transform.s)
    FUNCINFO(transform.R)
    FUNCINFO(transform.t)
    FUNCINFO(transform.s * Y * transform.R.transpose() + Eigen::MatrixXd::Constant(N_move_points, 1, 1)*transform.t.transpose())
    return transform;
}

