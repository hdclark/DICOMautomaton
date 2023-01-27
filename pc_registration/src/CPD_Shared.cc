#include <math.h>

#include "YgorFilesDirs.h"    //Needed for Does_File_Exist_And_Can_Be_Read(...), etc..
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorLog.h"
#include "YgorMath.h"         //Needed for samples_1D.
#include "YgorString.h"       //Needed for GetFirstRegex(...)

#include "CPD_Shared.h"

#include <chrono>
using namespace std::chrono;

Eigen::MatrixXf CenterMatrix(const Eigen::MatrixXf & points,
            const Eigen::MatrixXf & meanVector) {
    Eigen::MatrixXf oneVec = Eigen::MatrixXf::Ones(points.rows(),1);
    return points - oneVec * meanVector.transpose();
}

Eigen::MatrixXf GetTranslationVector(const Eigen::MatrixXf & rotationMatrix,
            const Eigen::MatrixXf & xMeanVector,
            const Eigen::MatrixXf & yMeanVector,
            double scale) {
    return xMeanVector - scale * rotationMatrix * yMeanVector;
}

Eigen::MatrixXf AlignedPointSet(const Eigen::MatrixXf & yPoints,
            const Eigen::MatrixXf & rotationMatrix,
            const Eigen::MatrixXf & translation,
            double scale) {
    
    Eigen::MatrixXf oneVec = Eigen::MatrixXf::Ones(yPoints.rows(),1);
    return scale * yPoints * rotationMatrix.transpose() + oneVec * translation.transpose();
}

double Init_Sigma_Squared(const Eigen::MatrixXf & xPoints,
            const Eigen::MatrixXf & yPoints) {
    double normSum = 0;
    int nRowsX = xPoints.rows();
    int mRowsY =  yPoints.rows();
    int dim = xPoints.cols();
    for (int i = 0; i < nRowsX; i++) {
        const auto xRow = xPoints.row(i).transpose();
        for (int j = 0; j < mRowsY; j++) {
            const auto yRow = yPoints.row(j).transpose();
            auto rowDiff = xRow - yRow;
            // YLOGINFO(normSum)
            normSum += rowDiff.squaredNorm();
        }
    }
    return 1.0 / (nRowsX * mRowsY * dim) * normSum;
}

double GetSimilarity(const Eigen::MatrixXf & xPoints,
            const Eigen::MatrixXf & yPoints,
            const Eigen::MatrixXf & rotationMatrix,
            const Eigen::MatrixXf & translation,
            double scale) {
    
    int mRowsY = yPoints.rows();
    int nRowsX = xPoints.rows(); 
    Eigen::MatrixXf alignedYPoints = AlignedPointSet(yPoints, rotationMatrix, translation, scale);
    Eigen::MatrixXf tempVector;

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

    YLOGINFO(sum);
    YLOGINFO(mRowsY);
    return sum;
}

double GetObjective(const Eigen::MatrixXf & xPoints,
            const Eigen::MatrixXf & yPoints,
            const Eigen::MatrixXf & postProb,
            const Eigen::MatrixXf & rotationMatrix,
            const Eigen::MatrixXf & translation,
            double scale, 
            double sigmaSquared) {
    
    int mRowsY = yPoints.rows();
    int nRowsX = xPoints.rows(); 
    double dimensionality = xPoints.cols();
    double Np = postProb.sum();
    Eigen::MatrixXf alignedYPoints = AlignedPointSet(yPoints, rotationMatrix, translation, scale);
    Eigen::MatrixXf tempVector;

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

Eigen::MatrixXf E_Step(const Eigen::MatrixXf & xPoints,
            const Eigen::MatrixXf & yPoints,
            const Eigen::MatrixXf & rotationMatrix,
            const Eigen::MatrixXf & t,
            double sigmaSquared,
            double w,
            double scale) {
    Eigen::MatrixXf postProb = Eigen::MatrixXf::Zero(yPoints.rows(),xPoints.rows());
    Eigen::MatrixXf expMat = Eigen::MatrixXf::Zero(yPoints.rows(),xPoints.rows());

    Eigen::MatrixXf tempVector;
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

Eigen::MatrixXf CalculateUx(
            const Eigen::MatrixXf & xPoints, 
            const Eigen::MatrixXf & postProb){
    Eigen::MatrixXf oneVec = Eigen::MatrixXf::Ones(postProb.rows(),1);
    double oneOverNp = 1/(postProb.sum());
    return oneOverNp * (xPoints.transpose()) * postProb.transpose() * oneVec;
}

Eigen::MatrixXf CalculateUy( 
            const Eigen::MatrixXf & yPoints, 
            const Eigen::MatrixXf & postProb){
    Eigen::MatrixXf oneVec = Eigen::MatrixXf::Ones(postProb.cols(),1);
    double oneOverNp = 1/(postProb.sum());
    return oneOverNp * (yPoints.transpose()) * postProb * oneVec;
}
