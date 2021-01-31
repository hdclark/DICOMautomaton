#include "YgorFilesDirs.h"    //Needed for Does_File_Exist_And_Can_Be_Read(...), etc..
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorMath.h"         //Needed for samples_1D.
#include "YgorString.h"       //Needed for GetFirstRegex(...)

#include "CPD_Rigid.h"
#include "YgorMathIOXYZ.h"    //Needed for ReadPointSetFromXYZ.

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

void RigidCPDTransform::write_to( std::ostream &os ) {
    affine_transform<double> tf;
    Eigen::MatrixXd sR = get_sR();
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
RigidCPDTransform
AlignViaRigidCPD(CPDParams & params,
            const point_set<double> & moving,
            const point_set<double> & stationary,
            int iter_interval /*= 0*/,
            std::string video /*= "False"*/,
            std::string xyz_outfile /*= "output"*/ ){
    FUNCINFO("Performing rigid CPD")

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

    Eigen::MatrixXd P;
    Eigen::MatrixXd Ux;
    Eigen::MatrixXd Uy;
    Eigen::MatrixXd X_hat;
    Eigen::MatrixXd Y_hat;
    Eigen::MatrixXd A;
    
    params.iterations = 50;

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
        Eigen::JacobiSVD<Eigen::MatrixXd> svd( A, Eigen::ComputeFullV | Eigen::ComputeFullU );
        transform.R = GetRotationMatrix(svd.matrixU(), svd.matrixV());
        transform.s = GetS(A, transform.R, Y_hat, P);
        transform.t = GetTranslationVector(transform.R, Ux, Uy, transform.s);
        sigma_squared = SigmaSquared(transform.s, A, transform.R, X_hat, P);

        mutable_moving = moving;
        transform.apply_to(mutable_moving);

        similarity = GetSimilarity(X, Y, transform.R, transform.t, transform.s);
        objective = GetObjective(X, Y, P, transform.R, transform.t, transform.s, sigma_squared);
        FUNCINFO(similarity);
        FUNCINFO(objective);
        FUNCINFO(sigma_squared);
        
        if (video == "True") {
            if (iter_interval > 0 && i % iter_interval == 0) {
                temp_xyz_outfile = xyz_outfile + "_iter" + std::to_string(i+1) + "_sim" + std::to_string(similarity) + ".xyz";
                std::ofstream PFO(temp_xyz_outfile);
                if(!WritePointSetToXYZ(mutable_moving, PFO))
                    FUNCERR("Error writing point set to " << xyz_outfile);
            }
        }

        if(similarity < params.similarity_threshold)
            break;

    }
    return transform;
}