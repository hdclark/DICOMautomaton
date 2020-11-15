#include "CPD_Rigid.h"

RigidCPDTransform::RigidCPDTransform(int dimensionality) {
    this->R = Eigen::MatrixXd::Identity(dimensionality, dimensionality);
    this->t = Eigen::VectorXd::Zero(dimensionality);
    this->s = 1;
}

vec3<double>  RigidCPDTransform::transform(const vec3<double> &v) {

}

void RigidCPDTransform::apply_to(point_set<double> &ps) {

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
    float sigma_squared = Init_Sigma_Squared(X, Y);

    for (int i = 0; i < params.iterations; i++) {
        
    }

    return transform;
}

