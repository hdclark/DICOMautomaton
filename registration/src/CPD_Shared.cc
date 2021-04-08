#include <math.h>

#include "YgorFilesDirs.h"    //Needed for Does_File_Exist_And_Can_Be_Read(...), etc..
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorMath.h"         //Needed for samples_1D.
#include "YgorString.h"       //Needed for GetFirstRegex(...)

#include "CPD_Shared.h"

#include <chrono>
using namespace std::chrono;

Eigen::MatrixXd CenterMatrix(const Eigen::MatrixXd & points,
            const Eigen::MatrixXd & meanVector) {
    Eigen::MatrixXd oneVec = Eigen::MatrixXd::Ones(points.rows(),1);
    return points - oneVec * meanVector.transpose();
}

Eigen::MatrixXd GetTranslationVector(const Eigen::MatrixXd & rotationMatrix,
            const Eigen::MatrixXd & xMeanVector,
            const Eigen::MatrixXd & yMeanVector,
            double scale) {
    return xMeanVector - scale * rotationMatrix * yMeanVector;
}

Eigen::MatrixXd AlignedPointSet(const Eigen::MatrixXd & yPoints,
            const Eigen::MatrixXd & rotationMatrix,
            const Eigen::MatrixXd & translation,
            double scale) {
    
    Eigen::MatrixXd oneVec = Eigen::MatrixXd::Ones(yPoints.rows(),1);
    return scale * yPoints * rotationMatrix.transpose() + oneVec * translation.transpose();
}

double Init_Sigma_Squared(const Eigen::MatrixXd & xPoints,
            const Eigen::MatrixXd & yPoints) {
    double normSum = 0;
    int nRowsX = xPoints.rows();
    int mRowsY =  yPoints.rows();
    int dim = xPoints.cols();
    for (int i = 0; i < nRowsX; i++) {
        const auto xRow = xPoints.row(i).transpose();
        for (int j = 0; j < mRowsY; j++) {
            const auto yRow = yPoints.row(j).transpose();
            auto rowDiff = xRow - yRow;
            // FUNCINFO(normSum)
            normSum += rowDiff.squaredNorm();
        }
    }
    return 1.0 / (nRowsX * mRowsY * dim) * normSum;
}

double GetSimilarity(const Eigen::MatrixXd & xPoints,
            const Eigen::MatrixXd & yPoints,
            const Eigen::MatrixXd & rotationMatrix,
            const Eigen::MatrixXd & translation,
            double scale) {
    
    int mRowsY = yPoints.rows();
    int nRowsX = xPoints.rows(); 
    Eigen::MatrixXd alignedYPoints = AlignedPointSet(yPoints, rotationMatrix, translation, scale);
    Eigen::MatrixXd tempVector;

    double sum = 0;
    double min_distance = -1;
    for (int m = 0; m < mRowsY; ++m) {
        min_distance = -1;
        for (int n = 0; n < nRowsX; ++n) {
            tempVector = xPoints.row(n) - alignedYPoints.row(m);
            if (min_distance < 0 ||  tempVector.norm() < min_distance) {
                min_distance = tempVector.norm();
            }
        }
        sum += min_distance;
    }
    sum = sum / (mRowsY * 1.00);

    FUNCINFO(sum);
    FUNCINFO(mRowsY);
    return sum;
}

double GetObjective(const Eigen::MatrixXd & xPoints,
            const Eigen::MatrixXd & yPoints,
            const Eigen::MatrixXd & postProb,
            const Eigen::MatrixXd & rotationMatrix,
            const Eigen::MatrixXd & translation,
            double scale, 
            double sigmaSquared) {
    
    int mRowsY = yPoints.rows();
    int nRowsX = xPoints.rows(); 
    double dimensionality = xPoints.cols();
    double Np = postProb.sum();
    Eigen::MatrixXd alignedYPoints = AlignedPointSet(yPoints, rotationMatrix, translation, scale);
    Eigen::MatrixXd tempVector;

    double leftSum = 0;
    for (int m = 0; m < mRowsY; ++m) {
        for (int n = 0; n < nRowsX; ++n) {
            tempVector = xPoints.row(n) - alignedYPoints.row(m);
            leftSum += postProb(m,n) * tempVector.squaredNorm();
        }
    }
    leftSum = leftSum / (2.0 * sigmaSquared);
    double rightSum = Np * dimensionality / 2.0 * log(sigmaSquared);
    return leftSum + rightSum;
}

Eigen::MatrixXd E_Step(const Eigen::MatrixXd & xPoints,
            const Eigen::MatrixXd & yPoints,
            const Eigen::MatrixXd & rotationMatrix,
            const Eigen::MatrixXd & t,
            double sigmaSquared,
            double w,
            double scale) {
    Eigen::MatrixXd postProb = Eigen::MatrixXd::Zero(yPoints.rows(),xPoints.rows());
    Eigen::MatrixXd expMat = Eigen::MatrixXd::Zero(yPoints.rows(),xPoints.rows());

    Eigen::MatrixXd tempVector;
    double expArg;
    double numerator;
    double denominator;

    int mRowsY = yPoints.rows();
    int nRowsX = xPoints.rows();
    int dimensionality = yPoints.cols();

    for (int m = 0; m < mRowsY; ++m) {
        for (int n = 0; n < nRowsX; ++n) {
            tempVector = xPoints.row(n).transpose() - (scale * rotationMatrix * yPoints.row(m).transpose() + t);
            expArg = - 1.0 / (2 * sigmaSquared) * tempVector.squaredNorm();
            expMat(m,n) = exp(expArg);
        }
    }

    for (int m = 0; m < mRowsY; ++m) {
        for (int n = 0; n < nRowsX; ++n) {
            numerator = expMat(m,n);
            denominator = expMat.col(n).sum() + 
                          pow(2 * M_PI * sigmaSquared,((double)(dimensionality/2.0))) * (w/(1-w)) * ((double) mRowsY / nRowsX);
            postProb(m,n) = numerator / denominator;
        }
    }
    
    return postProb;
}

Eigen::MatrixXd CalculateUx(
            const Eigen::MatrixXd & xPoints, 
            const Eigen::MatrixXd & postProb){
    Eigen::MatrixXd oneVec = Eigen::MatrixXd::Ones(postProb.rows(),1);
    double oneOverNp = 1/(postProb.sum());
    return oneOverNp * (xPoints.transpose()) * postProb.transpose() * oneVec;
}

Eigen::MatrixXd CalculateUy( 
            const Eigen::MatrixXd & yPoints, 
            const Eigen::MatrixXd & postProb){
    Eigen::MatrixXd oneVec = Eigen::MatrixXd::Ones(postProb.cols(),1);
    double oneOverNp = 1/(postProb.sum());
    return oneOverNp * (yPoints.transpose()) * postProb * oneVec;
}
