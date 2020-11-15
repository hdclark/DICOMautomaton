#include "CPD_Affine.h"


Eigen::MatrixXd calculate_B(const Eigen::MatrixXd & xHat,
            const Eigen::MatrixXd & yHat,
            const Eigen::MatrixXd & postProb) {
    
    Eigen::MatrixXd oneVec = Eigen::MatrixXd::Ones(postProb.cols(),1);
    Eigen::MatrixXd left = xHat.transpose() * postProb.transpose() * yHat;
    Eigen::MatrixXd right = (yHat.transpose() * (postProb * oneVec).asDiagonal() * yHat).inverse();
    return left * right;
}

double sigma_squared(CPDParams & params,
            double Np,
            const Eigen::MatrixXd & B,
            const Eigen::MatrixXd & xHat,
            const Eigen::MatrixXd & yHat,
            const Eigen::MatrixXd & postProb) {
    
    int dimensionality = (int)xHat.cols();
    
    Eigen::MatrixXd oneVec = Eigen::MatrixXd::Ones(postProb.rows(),1);
    double left = (double)(xHat.transpose() * (postProb.transpose() * oneVec).asDiagonal() * xHat).trace();
    double right = (double)(xHat.transpose() * postProb.transpose() * yHat * B.transpose()).trace();
    return (left - right) / (Np * dimensionality);
    
}


// This function is where the deformable registration algorithm should be implemented.
std::optional<CPDTransform>
AlignViaAffineCPD(CPDParams & params,
            const point_set<double> & moving,
            const point_set<double> & stationary ){

    if(moving.points.empty() || stationary.points.empty()){
        FUNCWARN("Unable to perform ABC alignment: a point set is empty");
        return std::nullopt;
    }

    const auto N_move_points = static_cast<long int>(moving.points.size());
    const auto N_stat_points = static_cast<long int>(stationary.points.size());

    // The point_set class is documented at
    // https://github.com/hdclark/Ygor/blob/5cffc24f3c662db116cc132da033bbc279e19d56/src/YgorMath.h#L575 but it is
    // effectively a very simple wrapper around a std::vector of vec3's, which are documented at
    // https://github.com/hdclark/Ygor/blob/5cffc24f3c662db116cc132da033bbc279e19d56/src/YgorMath.h#L28 .
    // At their core, vec3's are made of three numbers: x, y, and z coordinates.
    //
    // As an example, print out all point coordinates:
    std::cout << "Moving point set:" << std::endl;
    for(const auto & r : moving.points){
        std::cout << "    (" << r.x << ", " << r.y << ", " << r.z << ")" << std::endl;
    }

    // The following is an example of using Eigen for matrices, in case it is needed.
    {
        Eigen::Matrix4d A;
        A <<  1.0,  0.0,  0.0,  0.0,
              0.0,  0.0,  1.0,  0.0,
             -3.0,  3.0, -2.0, -1.0,
              2.0, -2.0,  1.0,  1.0;
        auto AT = A.transpose();

        auto C = A * AT;

        double coeff_sum = 0.0;
        for(int i = 0; i < 4; ++i){
            for(int j = 0; j < 4; ++j){
                coeff_sum += C(i,j) * 1.23;
            }
        }

        FUNCINFO("The Eigen example coefficient sum is " << coeff_sum);
    }


    // -----------------------------------------------
    // Implement algorithm here.
    //
    // Note that the moving point set should not be modified! A temporary should be made if this is needed, but often it
    // won't be because the transformation can create a temporary copy on-the-fly. Here is how to make a mutable copy in
    // case it is needed:
    point_set<double> mutable_moving = moving;

    // ...
    // ...
    // ...
    //    Note: if the algorithm fails at some point, emit a warning using FUNCWARN() (see above) and return std::nullopt .
    // ...
    // ...
    // ...

    // -----------------------------------------------


    // This structure is described in Alignment_ABC.h. Finding this transform is the ultimate goal of this algorithm.
    // For now, we'll leave it undefined. But a valid AlignViaABCTransform should be created and returned if the algorithm
    // successfully completes.
    CPDTransform transform;
    return transform;
}

