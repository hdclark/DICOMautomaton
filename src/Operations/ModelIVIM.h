// ModelIVIM.h.

#pragma once

#include <map>
#include <string>

#include "../Structs.h"


OperationDoc OpArgDocModelIVIM();

Drover ModelIVIM(Drover DICOM_data,
                 const OperationArgPkg& /*OptArgs*/,
                 const std::map<std::string, std::string>& /*InvocationMetadata*/,
                 const std::string& /*FilenameLex*/);

double GetADCls(const std::vector<float> &bvalues, const std::vector<float> &vals);
double GetBiExpf(const std::vector<float> &bvalues, const std::vector<float> &vals, int numIterations);
double GetBayesianParams(const std::vector<float> &bvalues, const std::vector<float> &vals);
std::vector<double> GetHessianAndGradient(const std::vector<float> &bvalues, const std::vector<float> &vals, float f, double pseudoD, const double D);
std::vector<double> GetInverse(const std::vector<double> matrix);

