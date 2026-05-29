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

    double
    GetADCls(const std::vector<float> &bvalues,
             const std::vector<float> &vals);
    

    std::array<double, 3>
    GetKurtosisParams(const std::vector<float> &bvalues,
                      const std::vector<float> &vals,
                      int numIterations = 500);
    

    double
    evaluate_biexp(double b,
                   double S0,
                   double f,
                   double D,
                   double pseudoD);

    std::array<double, 7>
    GetBiExp(const std::vector<float> &bvalues,
             const std::vector<float> &vals,
             int numIterations = 500,
             float b_value_threshold = 200.0f);
    

    std::array<double, 6>
    GetBiExp_SegmentedOLS(const std::vector<float> &bvalues,
                          const std::vector<float> &vals,
                          float bvalue_threshold = 200.0f);

} // namespace MRI_IVIM
