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
double GetBayesianParams(const std::vector<float> &bvalues, const std::vector<float> &vals);
std::array<double, 3> GetBiExpf(const std::vector<float> &bvalues, const std::vector<float> &vals, int numIterations);
std::vector<double> GetHessianAndGradient(const std::vector<float> &bvalues, const std::vector<float> &vals, float f, double pseudoD, const double D);
std::vector<double> GetInverse(const std::vector<double> matrix);

