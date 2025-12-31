// MRI_IVIM.h.

#pragma once

#include <vector>
#include <string>    
#include <optional>
#include <list>
#include <map>
#include <cmath>
#include <numeric>
#include <cstdint>


namespace MRI_IVIM {

    double GetADCls(const std::vector<float> &bvalues,
                    const std::vector<float> &vals);
    
    double GetKurtosisModel(float b,
                            const std::vector<double> &params);
    
    double GetKurtosisTheta(const std::vector<float> &bvalues,
                            const std::vector<float> &signals,
                            const std::vector<double> &params,
                            const std::vector<double> &priors);
    
    std::vector<double> GetKurtosisPriors(const std::vector<double> &params);
    
    std::array<double, 3> GetKurtosisParams(const std::vector<float> &bvalues,
                                            const std::vector<float> &vals,
                                            int numIterations = 10);
    
    std::array<double, 3> GetBiExp(const std::vector<float> &bvalues,
                                   const std::vector<float> &vals,
                                   int numIterations = 10,
                                   float b_value_threshold = 200.0f);
    
    std::vector<double> GetHessianAndGradient(const std::vector<float> &bvalues,
                                              const std::vector<float> &vals,
                                              float f,
                                              double pseudoD,
                                              double D);
    
    std::vector<double> GetInverse(const std::vector<double> &matrix);

} // namespace MRI_IVIM
