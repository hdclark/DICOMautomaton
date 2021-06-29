// ModelIVIM.h.

#pragma once

#include <map>
#include <string>
#include <array>

#include "../Structs.h"


OperationDoc OpArgDocModelIVIM();

Drover ModelIVIM(Drover DICOM_data,
                 const OperationArgPkg& /*OptArgs*/,
                 const std::map<std::string, std::string>& /*InvocationMetadata*/,
                 const std::string& /*FilenameLex*/);

double GetADCls(const std::vector<float> &bvalues, const std::vector<float> &vals);

#ifdef DCMA_USE_EIGEN
double GetKurtosisModel(float b, const std::vector<double> &params);
double GetKurtosisTheta(const std::vector<float> &bvalues, const std::vector<float> &signals, const std::vector<double> &params, const std::vector<double> &priors);
// void GetKurtosisGradient(MatrixXd &grad, const std::vector<float>bvalues, const std::vector<float> signals, std::vector<double> params, std::vector<double> priors);
// void GetHessian(MatrixXd &hessian, const std::vector<float>bvalues, const std::vector<float> signals, std::vector<double> params, std::vector<double> priors);
std::vector<double> GetKurtosisPriors(const std::vector<double> &params);
std::array<double, 3> GetKurtosisParams(const std::vector<float> &bvalues, const std::vector<float> &vals, int numIterations);
#endif //DCMA_USE_EIGEN

std::array<double, 3> GetBiExpf(const std::vector<float> &bvalues, const std::vector<float> &vals, int numIterations);
std::vector<double> GetHessianAndGradient(const std::vector<float> &bvalues, const std::vector<float> &vals, float f, double pseudoD, const double D);
std::vector<double> GetInverse(const std::vector<double> &matrix);

