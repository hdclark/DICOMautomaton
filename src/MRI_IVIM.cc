// MRI_IVIM.cc - A part of DICOMautomaton 2025. Written by Caleb Sample, Hal Clark, and Arash Javanmardi.

#include <limits>
#include <optional>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>    
#include <utility>            //Needed for std::pair.
#include <vector>
#include <mutex>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <cstdint>

#include "YgorMisc.h"
#include "YgorMath.h"
#include "YgorImages.h"
#include "YgorString.h"       //Needed for GetFirstRegex(...)

#include "doctest20251212/doctest.h"

#ifdef DCMA_USE_EIGEN
#else
    #error "Attempted to compile without Eigen support, which is required."
#endif
#include <eigen3/Eigen/Dense>
#include <eigen3/Eigen/LU>

#include "Structs.h"
#include "Metadata.h"
#include "Regex_Selectors.h"
#include "BED_Conversion.h"
#include "YgorImages_Functors/Compute/Joint_Pixel_Sampler.h"

#include "MRI_IVIM.h"

using Eigen::MatrixXd;

namespace MRI_IVIM {


#ifdef DCMA_USE_EIGEN
double GetKurtosisModel(float b, const std::vector<double> &params){
    double f = params.at(0);
    double pseudoD = params.at(1);
    double D = params.at(2);
    double K = params.at(3);
    double NCF = params.at(4);

    // Note: this model assumes S(b) are all divided by S(b=0) to normalize model to [0:1].
    double model = f*exp(-b * pseudoD) + (1.0 - f) * exp(-b*D + std::pow((b*D), 2.0)*K/6.0);

    //now add noise floor:
    model = std::pow(model, 2.0) + std::pow(NCF, 2.0);
    model = std::pow(model, 0.5);
    return model;

}
double GetKurtosisTheta(const std::vector<float> &bvalues, const std::vector<float> &signals, const std::vector<double> &params, const std::vector<double> &priors){
    double theta = 0.0;
    //for now priors are uniform so not included in theta. The goal is to minimize. Reduces to a regression problem
    for (size_t i = 0; i < bvalues.size(); ++i){

        theta += std::pow((signals.at(i) - GetKurtosisModel(bvalues.at(i), params)), 2.0);  

    } 
    return theta;    

}
std::vector<double> GetKurtosisPriors(const std::vector<double> &params){
    //For now use uniform distributions for the priors (call a constant double to make simple)
    std::vector<double> priors;
    double prior_f = 1;
    double prior_pseudoD = 1;
    double prior_D = 1;
    double prior_K = 1; //Kurtosis factor
    double prior_NCF = 1; //Noise floor correction

    priors.push_back(prior_f);
    priors.push_back(prior_pseudoD);
    priors.push_back(prior_D);
    priors.push_back(prior_K);
    priors.push_back(prior_NCF);
    return priors;
}


void GetKurtosisGradient(MatrixXd &grad, const std::vector<float> &bvalues, const std::vector<float> &signals, const std::vector<double> &params, const std::vector<double> &priors){
    //the kurtosis model with a noise floor correction has 5 parameters, so the gradient will be set as a 5x1 matrix

    std::vector<double> paramsTemp = params;
    double deriv;
    //Numerically determine the derivatives
    double delta = 0.00001;
    
    paramsTemp.at(0) += delta; //first get f derivative
    deriv = GetKurtosisTheta(bvalues, signals, paramsTemp, priors);
    paramsTemp.at(0) -= 2.0 * delta;
    deriv -= GetKurtosisTheta(bvalues, signals, paramsTemp, priors);
    deriv /= 2.0 * delta;
    grad(0,0) = deriv;
    paramsTemp.at(0) = params.at(0); 

    paramsTemp.at(1) += delta; //get pseudoD derivative
    deriv = GetKurtosisTheta(bvalues, signals, paramsTemp, priors);
    paramsTemp.at(1) -= 2.0 * delta;
    deriv -= GetKurtosisTheta(bvalues, signals, paramsTemp, priors);
    deriv /= 2.0 * delta;
    grad(1,0) = deriv;
    paramsTemp.at(1) = params.at(1);

    paramsTemp.at(2) += delta; //get D derivative
    deriv = GetKurtosisTheta(bvalues, signals, paramsTemp, priors);
    paramsTemp.at(2) -= 2.0 * delta;
    deriv -= GetKurtosisTheta(bvalues, signals, paramsTemp, priors);
    deriv /= 2.0 * delta;
    grad(2,0) = deriv;
    paramsTemp.at(2) = params.at(2);

    paramsTemp.at(3) += delta; //get K derivative
    deriv = GetKurtosisTheta(bvalues, signals, paramsTemp, priors);
    paramsTemp.at(3) -= 2.0 * delta;
    deriv -= GetKurtosisTheta(bvalues, signals, paramsTemp, priors);
    deriv /= 2.0 * delta;
    grad(3,0) = deriv;
    paramsTemp.at(3) = params.at(3);

    paramsTemp.at(4) += delta; //get D derivative
    deriv = GetKurtosisTheta(bvalues, signals, paramsTemp, priors);
    paramsTemp.at(4) -= 2.0 * delta;
    deriv -= GetKurtosisTheta(bvalues, signals, paramsTemp, priors);
    deriv /= 2.0 * delta;
    grad(4,0) = deriv;

    return;
}


void GetHessian(MatrixXd &hessian, const std::vector<float> &bvalues, const std::vector<float> &signals, const std::vector<double> &params, const std::vector<double> &priors){
    //for 5 parameters we will have a 5x5 Hessian matrix
    MatrixXd gradDiff(5,1);
    MatrixXd temp(5,1);

    std::vector<double> paramsTemp = params;
    //Numerically determine the derivatives
    double delta = 0.00001;
    //second partial derivatives are approximated by the difference in the gradients

    //first row: 
    paramsTemp.at(0) += delta;
    GetKurtosisGradient(gradDiff, bvalues, signals, paramsTemp, priors); 
    
    paramsTemp.at(0) -= 2.0 * delta;
    GetKurtosisGradient(temp, bvalues, signals, paramsTemp, priors); 

    gradDiff -= temp;
    gradDiff /= 2.0 * delta;
    hessian(0,0) = gradDiff(0,0);
    hessian(0,1) = gradDiff(1,0);
    hessian(0,2) = gradDiff(2,0);
    hessian(0,3) = gradDiff(3,0);
    hessian(0,4) = gradDiff(4,0);

    paramsTemp.at(0) = params.at(0);

    //Second row: 
    paramsTemp.at(1) += delta;
    GetKurtosisGradient(gradDiff, bvalues, signals, paramsTemp, priors); 
    
    paramsTemp.at(1) -= 2.0 * delta;
    GetKurtosisGradient(temp, bvalues, signals, paramsTemp, priors); 

    gradDiff -= temp;
    gradDiff /= 2.0 * delta;

    hessian(1,0) = gradDiff(0,0);
    hessian(1,1) = gradDiff(1,0);
    hessian(1,2) = gradDiff(2,0);
    hessian(1,3) = gradDiff(3,0);
    hessian(1,4) = gradDiff(4,0);

    paramsTemp.at(1) = params.at(1);

    //Third row: 
    paramsTemp.at(2) += delta;
    GetKurtosisGradient(gradDiff, bvalues, signals, paramsTemp, priors); 
    
    paramsTemp.at(2) -= 2.0 * delta;
    GetKurtosisGradient(temp, bvalues, signals, paramsTemp, priors); 

    gradDiff -= temp;
    gradDiff /= 2.0 * delta;
    
    paramsTemp.at(2) = params.at(2); 
    hessian(2,0) = gradDiff(0,0);
    hessian(2,1) = gradDiff(1,0);
    hessian(2,2) = gradDiff(2,0);
    hessian(2,3) = gradDiff(3,0);
    hessian(2,4) = gradDiff(4,0);


    //Fourth row: 
    paramsTemp.at(3) += delta;
    GetKurtosisGradient(gradDiff, bvalues, signals, paramsTemp, priors); 
    
    paramsTemp.at(3) -= 2.0 * delta;
    GetKurtosisGradient(temp, bvalues, signals, paramsTemp, priors); 

    gradDiff -= temp;
    gradDiff /= 2.0 * delta;

    hessian(3,0) = gradDiff(0,0);
    hessian(3,1) = gradDiff(1,0);
    hessian(3,2) = gradDiff(2,0);
    hessian(3,3) = gradDiff(3,0);
    hessian(3,4) = gradDiff(4,0);

    paramsTemp.at(3) = params.at(3);

    //Fifth row: 
    paramsTemp.at(4) += delta;
    GetKurtosisGradient(gradDiff, bvalues, signals, paramsTemp, priors); 
    
    paramsTemp.at(4) -= 2.0 * delta;
    GetKurtosisGradient(temp, bvalues, signals, paramsTemp, priors); 

    gradDiff -= temp;
    gradDiff /= 2.0 * delta;
    
    paramsTemp.at(4) = params.at(4); 
    hessian(4,0) = gradDiff(0,0);
    hessian(4,1) = gradDiff(1,0);
    hessian(4,2) = gradDiff(2,0);
    hessian(4,3) = gradDiff(3,0);
    hessian(4,4) = gradDiff(4,0);

    return;
}


std::array<double, 3> GetKurtosisParams(const std::vector<float> &bvalues, const std::vector<float> &vals, int numIterations){
//This function will use a Bayesian regression approach to fit IVIM kurtosis model with noise floor parameters to the data.
//Kurtosis model: S(b)/S(0) = {(f exp(-bD*) + (1-f)exp(-bD + (bD)^2K/6))^2 + NCF}^1/2

    const auto nan = std::numeric_limits<double>::quiet_NaN();

    //First divide all signals by S(b=0)
    std::vector<float> signals;
    const auto number_bVals = bvalues.size();
    int b0_index = 0;

    for(size_t i = 0; i < number_bVals; ++i){
        if(bvalues.at(i) == 0){ //first get the index of b = 0 (I'm unsure if b values are in order already)
            b0_index = i;
            break;
        }
    }
    for(size_t i = 0; i < number_bVals; ++i){
        signals.push_back(vals.at(i) / vals.at(b0_index));  // Normalize each signal
    }

    double f = 0.1;
    double pseudoD = 0.02;
    double D = 0.002;
    double K = 0.0;
    double NCF = 0.0;

    std::vector<double> params;
    params.push_back(f);
    params.push_back(pseudoD);
    params.push_back(D);
    params.push_back(K);
    params.push_back(NCF);

    std::vector<double> priors = GetKurtosisPriors(params);

    float lambda = 50.0;


    double theta;
    double newTheta;
    MatrixXd H(5,5);
    MatrixXd inverse(5,5);
    MatrixXd gradient(5,1);
    

//Get the current function to maximize log[(likelihood)*(priors)]

    theta = GetKurtosisTheta(bvalues, signals, params, priors);
    std::vector<double> newParams;
    for(int i = 0; i < 5; i++){
        newParams.push_back(0.0);
    }
    
    for(int i = 0; i < numIterations; i++){
        //Now calculate the Hessian matrix which is in the form of a vector (columns then rows), which also contains the gradient at the end
        GetHessian(H, bvalues, signals, params, priors);
        GetKurtosisGradient(gradient, bvalues, signals, params, priors);
        //Now I need to calculate the inverse of (H + lamda I)
        MatrixXd lambda_I(5,5);
        for (int row = 0; row < 5; row ++){
            for (int col = 0; col < 5; col++){
                if (row == col){
                    lambda_I(row, col) = lambda;
                }else{
                    lambda_I(row,col) = 0.0;
                }
            }
        }
        H += lambda_I; //add identity to H 
        inverse = H.inverse();

        //Now update parameters 
        MatrixXd newParamMatrix = -inverse * gradient;

        newParams[0] = (newParamMatrix(0,0) + params[0]); //f
        newParams[1] = (newParamMatrix(1,0) + params[1]); //pseudoD
        newParams[2] = (newParamMatrix(2,0) + params[2]); //D
        newParams[3] = (newParamMatrix(3,0) + params[3]); //K
        newParams[4] = (newParamMatrix(4,0) + params[4]); //NCF
        //if f is less than 0 or greater than 1, rescale back to boundary, and don't let pseudoDD get smaller than D
        if (newParams[0] < 0){
            newParams[0] = 0.0;
        }else if (newParams[0] > 1){
            newParams[0] = 1.0;
        }
        if (newParams[1] < 0){
            newParams[1] = 0.0;
        }
        if (newParams[2] < 0){
            newParams[2] = 0.0;
        }

        //Now check if we have lowered the cost
        newTheta = GetKurtosisTheta(bvalues, signals, newParams, priors);
        //std::cout << params[0] << std::endl << newParams[0] << std::endl << std::endl;
        //accept changes if we have have reduced cost, and lower lambda
        if (newTheta < theta){
            theta = newTheta;
            lambda *= 0.8;
            params[0] = newParams[0];
            params[1] = newParams[1];
            params[2] = newParams[2];
            params[3] = newParams[3];
            params[4] = newParams[4];
        }else{
            lambda *= 2.0;
        }
    }
    return {params[0], params[1], params[2]};
}
#endif //DCMA_USE_EIGEN

double GetADCls(const std::vector<float> &bvalues, const std::vector<float> &vals){
    //This function uses linear regression to obtain the ADC value using the image arrays for all the different b values.
    //This uses the formula S(b) = S(0)exp(-b * ADC)
    // --> ln(S(b)) = ln(S(0)) + (-ADC) * b 

    //First get ADC from the formula -ADC = sum [ (b_i - b_avg) * (ln(S_i) - ln(S)_avg ] / sum( b_i - b_avg )^2
    const auto nan = std::numeric_limits<double>::quiet_NaN();

    //get b_avg and S_avg
    double b_avg = 0.0;
    double log_S_avg = 0.0;
    const auto number_bVals = bvalues.size();
    for(size_t i = 0; i < number_bVals; ++i){
        b_avg += bvalues[i];
        log_S_avg += std::log( vals[i] );
        if(!std::isfinite(log_S_avg)){
            return nan;
        }
    }
    b_avg /= static_cast<double>(number_bVals);
    log_S_avg /= static_cast<double>(number_bVals);

    //Now do the sums
    double sum_numerator = 0.0;
    double sum_denominator = 0.0;
    for(size_t i = 0; i < number_bVals; ++i){
        const double b = bvalues[i];
        const double log_S = std::log( vals[i] );
        sum_numerator += (b - b_avg) * (log_S - log_S_avg); 
        sum_denominator += std::pow((b-b_avg), 2.0);
    }

    const double ADC = std::max<double>(- sum_numerator / sum_denominator, 0.0);
    return ADC;
}

// GetBiExp implementation with consensus-aligned WLLS
double GetADC_WLLS(const std::vector<float> &bvalues,
                   const std::vector<float> &vals, 
                   int max_iterations = 10,
                   double tolerance = 1e-6){
    
    const auto nan = std::numeric_limits<double>::quiet_NaN();
    const auto n_points = bvalues.size();
    
    if(n_points < 2){
        return nan;
    }
    
    // Initial estimate using simple approach
    double D_current = 1e-3; // Reasonable initial guess for parotid
    double S0_current = 1.0;
    
    // Simple initial estimate if we have at least 2 points
    if(n_points >= 2){
        const auto b_min = *std::min_element(bvalues.begin(), bvalues.end());
        const auto b_max = *std::max_element(bvalues.begin(), bvalues.end());
        const auto idx_min = std::distance(bvalues.begin(), std::min_element(bvalues.begin(), bvalues.end()));
        const auto idx_max = std::distance(bvalues.begin(), std::max_element(bvalues.begin(), bvalues.end()));
        
        if(vals[idx_min] > 0 && vals[idx_max] > 0 && b_max > b_min){
            D_current = std::log(vals[idx_min] / vals[idx_max]) / (b_max - b_min);
            S0_current = vals[idx_min] / std::exp(-b_min * D_current);
        }
    }
    
    // Ensure positive initial guess
    D_current = std::max(D_current, 1e-6);
    S0_current = std::max(S0_current, 1e-6);
    
    std::vector<double> weights(n_points);
    
    for(int iter = 0; iter < max_iterations; ++iter){
        double D_previous = D_current;
        
        // Calculate weights based on current model prediction
        for(size_t i = 0; i < n_points; ++i){
            const double predicted_signal = S0_current * std::exp(-bvalues[i] * D_current);
            // Weight proportional to predicted signal squared (Rician noise model)
            weights[i] = std::max(predicted_signal * predicted_signal, 1e-12);
        }
        
        // Weighted linear regression on log-transformed data
        // Model: ln(S) = ln(S0) - b*D
        double sum_w = 0.0;
        double sum_wb = 0.0;
        double sum_wb2 = 0.0;
        double sum_wlogS = 0.0;
        double sum_wb_logS = 0.0;
        
        for(size_t i = 0; i < n_points; ++i){
            if(vals[i] <= 0) continue; // Skip invalid signals
            
            const double w = weights[i];
            const double b = bvalues[i];
            const double logS = std::log(vals[i]);
            
            sum_w += w;
            sum_wb += w * b;
            sum_wb2 += w * b * b;
            sum_wlogS += w * logS;
            sum_wb_logS += w * b * logS;
        }
        
        // Solve weighted normal equations
        const double denominator = sum_w * sum_wb2 - sum_wb * sum_wb;
        if( std::abs(denominator) < 1e-12 ){
            return nan; // Singular system
        }
        
        const double ln_S0_new = (sum_wb2 * sum_wlogS - sum_wb * sum_wb_logS) / denominator;
        const double D_new = (sum_wb * sum_wlogS - sum_w * sum_wb_logS) / denominator;
        
        // Ensure physical constraints
        D_current = std::max(D_new, 1e-8);  // Positive diffusion
        S0_current = std::exp(ln_S0_new);
        
        // Check convergence
        if( std::abs(D_current - D_previous) < (tolerance * std::abs(D_previous)) ){
            break;
        }
    }
    
    // Final validation
    if( !std::isfinite(D_current) || (D_current <= 0.0) ){
        return nan;
    }
    
    return D_current;
}

double
evaluate_biexp(double b,
               double S0,
               double f,
               double D,
               double pseudoD){
    return S0 * ( f * std::exp(-b * pseudoD) + (1.0 - f) * std::exp(-b * D) );
}

std::array<double, 7> GetBiExp(const std::vector<float> &bvalues,
                               const std::vector<float> &vals,
                               int numIterations,
                               float b_value_threshold){
    
    const auto nan = std::numeric_limits<double>::quiet_NaN();
    std::array<double, 7> default_out = {nan, nan, nan, nan, nan, nan, nan};
    constexpr auto index_vox_status = 6UL;
    const auto number_bVals = bvalues.size();
    
    std::get<index_vox_status>(default_out) = 1001.0;

    // Find b=0 index
    int b0_index = 0;
    for(size_t i = 0UL; (i < number_bVals); ++i){
        if (bvalues[i] == 0.0){ 
            b0_index = i;
            break;
        }
    }
    
    // Extract high b-values for D estimation
    std::vector<float> bvaluesH, signalsH;
    for(size_t i = 0UL; i < number_bVals; ++i){
        if (bvalues[i] > b_value_threshold){
            bvaluesH.push_back(bvalues[i]);
            signalsH.push_back(vals[i]);
        }
    }
    
    if(bvaluesH.size() < 2UL){
        std::get<index_vox_status>(default_out) = 1002.0;
        return default_out; // Insufficient high b-values
    }
    
    // Step 1: Estimate D using WLLS
    double D = GetADC_WLLS(bvaluesH, signalsH);
    
    // Fallback to ADC-ls if WLLS fails
    if(!std::isfinite(D) || (D <= 0.0)){
        D = GetADCls(bvaluesH, signalsH);
        if(!std::isfinite(D) || (D <= 0.0)){
            std::get<index_vox_status>(default_out) = 1003.0;
            return default_out;
        }
    }
    
    // Step 2: Prepare normalized signals for LM optimization
    std::vector<float> signals_normalized;
    for(size_t i = 0; i < number_bVals; ++i){
        const float norm_sig = vals[i] / vals[b0_index];
        if(!std::isfinite(norm_sig)){
            std::get<index_vox_status>(default_out) = 1004.0;
            return default_out;
        }
        signals_normalized.push_back(norm_sig);
    }
    
    // Convert to Eigen matrices
    MatrixXd sigs(number_bVals, 1);
    for(size_t i = 0; i < number_bVals; ++i){
        sigs(i, 0) = signals_normalized[i];
    }
    
    // Step 3: Estimate f and D* using Levenberg-Marquardt
    double lambda = 1.0;
    double pseudoD = D * 10.0;  // Initial guess
    float f = 0.15f;            // Initial guess (3% in brain, 20% in highly vascular organs, 30% in parotids)
    // Note: the b_value_threshold should fluctuate based on the fitted f accounting for the amount of signal decay,
    // but the threshold in practice impacts the fitted f. So a meta optimization would be needed if both were
    // fitted at the same time. In practice, selecting a threshold of 200 for brain and 400 for body seems reasonable,
    // though some tissues vary. The 'optimal' value also depends on the SNR and a variety of other factors.
    
    // Parameter bounds derived roughly from multiple sources. Selected mostly to be as accomodating as possible.
    //
    // See doi:10.1002/jmri.27875
    //     doi:10.1016/j.neuroimage.2017.03.004
    //     doi:10.1016/j.neuroimage.2017.12.062
    //     doi:10.1002/mrm.24277
    const float f_min = 0.0f;
    const float f_max = 0.5f;
    const double pseudoD_min = D * 3.0;
    const double pseudoD_max = D * 150.0;
    
    MatrixXd h(2,1);
    MatrixXd r(number_bVals, 1);
    MatrixXd J(number_bVals, 2);
    MatrixXd sigs_pred(number_bVals, 1);
    MatrixXd I = MatrixXd::Identity(2, 2);
    
    // Initial predictions and cost
    for(size_t i = 0; i < number_bVals; ++i){ 
        const double exp_diff   = std::exp(-bvalues[i] * D);
        const double exp_pseudo = std::exp(-bvalues[i] * pseudoD);
        sigs_pred(i,0) = f * exp_pseudo + (1.0 - f) * exp_diff;
    }
    r = sigs - sigs_pred;
    double cost = 0.5 * (r.transpose() * r)(0,0);
    
    // Check initial cost
    if(!std::isfinite(cost)){
        std::get<index_vox_status>(default_out) = 1005.0;
        return default_out;
    }
    
    int64_t iters_attempted = 0;
    int64_t successful_updates = 0;
    
    for(int64_t iter = 0; iter < numIterations; iter++){
        ++iters_attempted;
        
        // Compute Jacobian
        for(size_t i = 0; i < number_bVals; ++i){ 
            const double b = bvalues[i];
            const double exp_pseudo = std::exp(-b * pseudoD);
            const double exp_diff = std::exp(-b * D);
            
            // Check for numerical issues
            if(!std::isfinite(exp_pseudo) || !std::isfinite(exp_diff)){
                std::get<index_vox_status>(default_out) = 1006.0;
                return default_out;
            }
            
            // Partial derivatives from:
            //   S = f*exp(-b*D*) + (1-f)*exp(-b*D)
            J(i,0) = exp_pseudo - exp_diff; // ∂/∂f
            J(i,1) = -b * f * exp_pseudo;   // ∂/∂D*
        }
        
        // Levenberg-Marquardt update with numerical stability check
        MatrixXd JTJ = J.transpose() * J;
        MatrixXd JTr = J.transpose() * r;
        
        // Check determinant before inversion
        double det = JTJ.determinant();
        if(!std::isfinite(det) || (std::abs(det) < 1e-15)){
            if(successful_updates > 0){
                break;
            }
            std::get<index_vox_status>(default_out) = 1007.0;
            return default_out;
        }

        // Compute update
        // Key: r = sigs - sigs_pred (positive means we need MORE signal)
        // Gradient of cost w.r.t. params is -J^T*r (negative because we want to reduce residuals)
        // So solving (J^T*J + λI)*h = J^T*r gives us the descent direction
        MatrixXd A = JTJ + lambda * I;

        if(!std::isfinite(A.determinant())){
            if(successful_updates > 0){
                break;
            }
            std::get<index_vox_status>(default_out) = 1008.0;
            return default_out;
        }
        
        h = A.inverse() * JTr;
        
        // Check if h is finite
        if(!std::isfinite(h(0,0)) || !std::isfinite(h(1,0))){
            if(successful_updates > 0){
                break;
            }
            std::get<index_vox_status>(default_out) = 1009.0;
            return default_out;
        }
        
        // Update parameters with bounds
        float new_f = std::max(f_min, std::min(f_max, f + static_cast<float>(h(0,0))));
        double new_pseudoD = std::max(pseudoD_min, std::min(pseudoD_max, pseudoD + h(1,0)));
        
        // Compute new predictions and cost
        for(size_t i = 0; i < number_bVals; ++i){ 
            const double exp_diff = std::exp(-bvalues[i] * D);
            const double exp_new_pseudo = std::exp(-bvalues[i] * new_pseudoD);
            sigs_pred(i,0) = new_f * exp_new_pseudo + (1.0 - new_f) * exp_diff;
        }
        MatrixXd r_new = sigs - sigs_pred;
        double new_cost = 0.5 * (r_new.transpose() * r_new)(0,0);

        // Check new cost validity
        if(!std::isfinite(new_cost)){
            lambda *= 2.0;
            continue;
        }

        // Accept or reject update
        if(new_cost < cost){
            // Update accepted
            r = r_new;
            f = new_f;
            pseudoD = new_pseudoD;
            cost = new_cost;
            lambda /= 2.0; // Decrease damping
            successful_updates++;
            
            // Check for convergence
            double rel_change = std::abs(cost - new_cost) / (cost + 1e-12);
            if((rel_change < 1e-8) && (successful_updates >= 5)){
                // Converged...
                std::get<index_vox_status>(default_out) = 1010.0;
                break;
            }

        }else{
            // Update rejected
            lambda *= 2.0;  // Increase damping
        }
        
        // Prevent lambda from becoming too large
        if(lambda > 1e8){
            break;
        }

        // Clamp lambda to a minimum to prevent numerical issues
        if(lambda < 1e-10){
            lambda = 1e-10;
        }
    }
    
    // Ensure finite results
    if(!std::isfinite(f) || !std::isfinite(D) || !std::isfinite(pseudoD)){
        std::get<index_vox_status>(default_out) = 1011.0;
        return default_out;
    }
    
    return {f, D, pseudoD, static_cast<double>(iters_attempted), static_cast<double>(successful_updates), cost, 1012.0};
}


std::array<double, 5> GetBiExp_SegmentedOLS(const std::vector<float> &bvalues,
                                            const std::vector<float> &vals,
                                            float bvalue_threshold){
    // The bi-exponential model is:
    //
    //    S(b) = S0 * [ f * exp(-b * Dp) + (1 - f) * exp(-b * D) ]
    //
    // where
    //
    //    - D is the diffusion coefficient,
    //    - Dp is the pseduo-diffusion coefficient, which describes vascular perfusion, and
    //    - f is the perfusion fraction.
    //
    // Assume D < Dp by at least an order of magnitude, and that there is a 'threshold' b-value above
    // which the first term is suppressed. In that case, we can fit this simplified model for the 'upper'
    // data only:
    //
    //    S(b) = S0' * exp(-b * D)   where S0' = S0 * (1 - f)
    //
    // which we can linearize as:
    //
    //    ln( S(b) ) = -D * b + ln( S0' )
    //
    // Then we can use all data for a second model fit, rearranging the ordinate as a mix of measurement
    // and upper model predictions.
    //
    //    S(b) - S0' * exp(-b * D) = S0'' * exp(-b * Dp)   where S0'' = S0 * f
    //
    // which we can linearize as:
    //
    //    ln[ S(b) - S0' * exp(-b * D) ] = -Dp * b + ln( S0'' )
    //
    // After fitting this model, we have D and Dp estimates directly, and two indirect estimates for f
    // via the amplitude. We can solve for f via
    //
    //    S0' / S0'' = (1 - f) / f = (1 / f) - 1
    //
    // or, rearranging:
    //
    //    f = S0'' / (S0' + S0'') 
    //      = exp(ln(S0'')) / (exp(ln(S0')) + exp(ln(S0'')))
    //
    // written suggestively since we have fitted estimates of ln(S0') and ln(S0''). Similarly,
    //
    //    S0 = S0'' + S0'
    //
    // though we typically don't care about this amplitude.
    //
    const auto nan = std::numeric_limits<double>::quiet_NaN();
    std::array<double, 5> default_out = {nan, nan, nan, nan, nan};
    const auto N_bvalues = bvalues.size();

    // Stage 1.
    samples_1D<double> shtl;
    const bool inhibit_sort = true;
    for(size_t i = 0; i < N_bvalues; ++i){
        const auto S = vals.at(i);
        const auto b = bvalues.at(i);
        const auto y = (0.0 < S) ? std::log(S) : nan;

        if( (bvalue_threshold < b)
        &&  std::isfinite(y) ){
            shtl.push_back( b, 0.0, y, 0.0, inhibit_sort );
        }
    }
    shtl.stable_sort();
    if(shtl.size() < 2UL){
        return default_out;
    }

    bool OK = false;
    const bool skip_extras = false;
    const lin_reg_results<double> stage1 = shtl.Linear_Least_Squares_Regression(&OK, skip_extras);
    if(!OK){
        return default_out;
    }
    const auto D = stage1.slope * -1.0;
    const auto lnS0p = stage1.intercept;
    const auto S0p = std::exp(lnS0p);

    // Stage 2.
    shtl.samples.clear();
    for(size_t i = 0; i < N_bvalues; ++i){
        const auto S = vals.at(i);
        const auto b = bvalues.at(i);
        const auto t = S - S0p * std::exp(-b*D);
        const auto y = (0.0 < t) ? std::log(t) : nan;

        if( std::isfinite(y) ){
            shtl.push_back( b, 0.0, y, 0.0, inhibit_sort );
        }
    }
    shtl.stable_sort();
    if(shtl.size() < 2UL){
        return default_out;
    }
    
    OK = false;
    const lin_reg_results<double> stage2 = shtl.Linear_Least_Squares_Regression(&OK, skip_extras);
    if(!OK){
        return default_out;
    }
    const auto pseudoD = stage2.slope * -1.0;
    const auto lnS0pp = stage2.intercept;
    const auto S0pp = std::exp(lnS0pp);

    if( !std::isfinite(S0p)
    ||  !std::isfinite(S0pp) ){
        return default_out;
    }
    const auto f = S0pp / (S0p + S0pp);
    const auto stage1_gof = stage1.pvalue;
    const auto stage2_gof = stage2.pvalue;

    return {f, D, pseudoD, stage1_gof, stage2_gof};
}

} // namespace MRI_IVIM


TEST_CASE( "MRI_IVIM::GetBiExp_SegmentedOLS" ){
    // Verify that the model can be recovered correctly under a variety of situations.
    struct test_case {
        int sample;
        std::string name;
        std::string desc;
        std::string gen_f; // generating formula.

        double S0; // Model parameters used to generate the S(b) curve data.
        double f;
        double D;
        double Dp;

        double bvalue_threshold;

        double f_rtol;  // Relative tolerance recovering f, i.e., 0.01 = 1%.
        double D_rtol;  // Relative tolerance recovering D, i.e., 0.01 = 1%.
        double Dp_rtol; // Relative tolerance recovering Dp, i.e., 0.01 = 1%.

        std::vector<float> b_vals;
        std::vector<float> S_vals;
    };


    // Given a set of model parameters and b-values, generate synthetic voxel intensities.
    const auto generate_Ss = []( const test_case &t ) -> std::vector<float> {
        std::vector<float> S_vals;
        S_vals = t.b_vals;
        for(size_t i = 0UL; i < S_vals.size(); i++){
            S_vals.at(i) = MRI_IVIM::evaluate_biexp( t.b_vals.at(i), t.S0, t.f, t.D, t.Dp );
        }
        return S_vals;
    };

    std::vector<test_case> tcs;

    tcs.emplace_back();
    tcs.back().sample = 1;
    tcs.back().name   = "IVIM signal full";
    tcs.back().desc   = "two-compartment model";
    tcs.back().gen_f  = "S(b) = S0 * [ f*exp(-b*Dp) + (1-f)*exp(-b*D) ]";
    tcs.back().S0     = 1.0;   // arb units
    tcs.back().f      = 0.3;   // arb units
    tcs.back().D      = 0.001; // mm^2/s
    tcs.back().Dp     = 0.01;  // mm^2/s
    tcs.back().bvalue_threshold = 300.0;
    tcs.back().f_rtol  = 0.30;
    tcs.back().D_rtol  = 0.05;
    tcs.back().Dp_rtol = 0.30;
    tcs.back().b_vals = { 0,20,30,40,50,60,70,80,90,100,120,150,250,400,800,1000 };
    tcs.back().S_vals = generate_Ss(tcs.back());


//    tcs.emplace_back();
//    tcs.back().sample = 2;
//    tcs.back().name   = "IVIM signal gaussian SNR10";
//    tcs.back().desc   = "two-compartment model";
//    tcs.back().gen_f  = "S(b) = S0 * [ f*exp(-b*Dp) + (1-f)*exp(-b*D) ]";
//    tcs.back().S0     = 1.0;   // arb units
//    tcs.back().f      = 0.3;   // arb units
//    tcs.back().D      = 0.001; // mm^2/s
//    tcs.back().Dp     = 0.01;  // mm^2/s
//    tcs.back().bvalue_threshold = 200.0;
//    tcs.back().f_rtol  = 0.20;
//    tcs.back().D_rtol  = 0.02;
//    tcs.back().Dp_rtol = 0.02;
//    tcs.back().b_vals = { 0,20,30,40,50,60,70,80,90,100,120,150,250,400,800,1000 };
//    tcs.back().S_vals = generate_Ss(tcs.back());
//
//
//    tcs.emplace_back();
//    tcs.back().sample = 3;
//    tcs.back().name   = "IVIM signal gaussian SNR20";
//    tcs.back().desc   = "two-compartment model";
//    tcs.back().gen_f  = "S(b) = S0 * [ f*exp(-b*Dp) + (1-f)*exp(-b*D) ]";
//    tcs.back().S0     = 1.0;   // arb units
//    tcs.back().f      = 0.3;   // arb units
//    tcs.back().D      = 0.001; // mm^2/s
//    tcs.back().Dp     = 0.01;  // mm^2/s
//    tcs.back().bvalue_threshold = 200.0;
//    tcs.back().f_rtol  = 0.20;
//    tcs.back().D_rtol  = 0.02;
//    tcs.back().Dp_rtol = 0.02;
//    tcs.back().b_vals = { 0,20,30,40,50,60,70,80,90,100,120,150,250,400,800,1000, };
//    tcs.back().S_vals = generate_Ss(tcs.back());
//
//
//    tcs.emplace_back();
//    tcs.back().sample = 4;
//    tcs.back().name   = "IVIM signal gaussian SNR40";
//    tcs.back().desc   = "two-compartment model";
//    tcs.back().gen_f  = "S(b) = S0 * [ f*exp(-b*Dp) + (1-f)*exp(-b*D) ]";
//    tcs.back().S0     = 1.0;   // arb units
//    tcs.back().f      = 0.3;   // arb units
//    tcs.back().D      = 0.001; // mm^2/s
//    tcs.back().Dp     = 0.01;  // mm^2/s
//    tcs.back().bvalue_threshold = 200.0;
//    tcs.back().f_rtol  = 0.20;
//    tcs.back().D_rtol  = 0.02;
//    tcs.back().Dp_rtol = 0.02;
//    tcs.back().b_vals = { 0,20,30,40,50,60,70,80,90,100,120,150,250,400,800,1000 };
//    tcs.back().S_vals = generate_Ss(tcs.back());


    tcs.emplace_back();
    tcs.back().sample = 5;
    tcs.back().name   = "IVIM signal high";
    tcs.back().desc   = "two-compartment model";
    tcs.back().gen_f  = "S(b) = S0 * [ f*exp(-b*Dp) + (1-f)*exp(-b*D) ]";
    tcs.back().S0     = 1.0;   // arb units
    tcs.back().f      = 0.3;   // arb units
    tcs.back().D      = 0.001; // mm^2/s
    tcs.back().Dp     = 0.01;  // mm^2/s
    tcs.back().bvalue_threshold = 200.0;
    tcs.back().f_rtol  = 0.55;
    tcs.back().D_rtol  = 0.05;
    tcs.back().Dp_rtol = 3.00;
    tcs.back().b_vals = { 0,200,400,800 };
    tcs.back().S_vals = generate_Ss(tcs.back());


//    tcs.emplace_back();
//    tcs.back().sample = 6;
//    tcs.back().name   = "IVIM signal low";
//    tcs.back().desc   = "two-compartment model";
//    tcs.back().gen_f  = "S(b) = S0 * [ f*exp(-b*Dp) + (1-f)*exp(-b*D) ]";
//    tcs.back().S0     = 1.0;   // arb units
//    tcs.back().f      = 0.3;   // arb units
//    tcs.back().D      = 0.001; // mm^2/s
//    tcs.back().Dp     = 0.01;  // mm^2/s
//    tcs.back().bvalue_threshold = 200.0;
//    tcs.back().f_rtol  = 0.20;
//    tcs.back().D_rtol  = 0.02;
//    tcs.back().Dp_rtol = 0.02;
//    tcs.back().b_vals = { 0,30,70,100 };
//    tcs.back().S_vals = generate_Ss(tcs.back());


//    tcs.emplace_back();
//    tcs.back().sample = 7;
//    tcs.back().name   = "IVIM signal minimal";
//    tcs.back().desc   = "two-compartment model";
//    tcs.back().gen_f  = "S(b) = S0 * [ f*exp(-b*Dp) + (1-f)*exp(-b*D) ]";
//    tcs.back().S0     = 1.0;   // arb units
//    tcs.back().f      = 0.3;   // arb units
//    tcs.back().D      = 0.001; // mm^2/s
//    tcs.back().Dp     = 0.01;  // mm^2/s
//    tcs.back().bvalue_threshold = 200.0;
//    tcs.back().f_rtol  = 0.20;
//    tcs.back().D_rtol  = 0.02;
//    tcs.back().Dp_rtol = 0.02;
//    tcs.back().b_vals = { 0,100,800 };
//    tcs.back().S_vals = generate_Ss(tcs.back());


//    tcs.emplace_back();
//    tcs.back().sample = 8;
//    tcs.back().name   = "IVIM signal noise uniform";
//    tcs.back().desc   = "two-compartment model";
//    tcs.back().gen_f  = "S(b) = S0 * [ f*exp(-b*Dp) + (1-f)*exp(-b*D) ]";
//    tcs.back().S0     = 1.0;   // arb units
//    tcs.back().f      = 0.3;   // arb units
//    tcs.back().D      = 0.001; // mm^2/s
//    tcs.back().Dp     = 0.01;  // mm^2/s
//    tcs.back().bvalue_threshold = 400.0;
//    tcs.back().f_rtol  = 0.20;
//    tcs.back().D_rtol  = 0.02;
//    tcs.back().Dp_rtol = 0.02;
//    tcs.back().b_vals = { 0,20,30,40,50,60,70,80,90,100,120,150,250,400,800,1000 };
//    tcs.back().S_vals = generate_Ss(tcs.back());


    tcs.emplace_back();
    tcs.back().sample = 9;
    tcs.back().name   = "IVIM signal whitepaper";
    tcs.back().desc   = "two-compartment model";
    tcs.back().gen_f  = "S(b) = S0 * [ f*exp(-b*Dp) + (1-f)*exp(-b*D) ]";
    tcs.back().S0     = 1.0;   // arb units
    tcs.back().f      = 0.3;   // arb units
    tcs.back().D      = 0.001; // mm^2/s
    tcs.back().Dp     = 0.01;  // mm^2/s
    tcs.back().bvalue_threshold = 250.0;
    tcs.back().f_rtol  = 0.30;
    tcs.back().D_rtol  = 0.05;
    tcs.back().Dp_rtol = 3.00;
    tcs.back().b_vals = { 0,30,70,100,200,400,800 };
    tcs.back().S_vals = generate_Ss(tcs.back());


    for(const auto &tc : tcs){
        CAPTURE( tc.sample );
        CAPTURE( tc.name );
        CAPTURE( tc.desc );
        CAPTURE( tc.f_rtol );
        CAPTURE( tc.D_rtol );
        CAPTURE( tc.Dp_rtol );

        REQUIRE(tc.b_vals.size() == tc.S_vals.size());
        REQUIRE(tc.b_vals.size() > 2UL);

        const auto out  = MRI_IVIM::GetBiExp_SegmentedOLS(tc.b_vals, tc.S_vals, tc.bvalue_threshold);
        const auto m_f  = out.at(0);
        const auto m_D  = out.at(1);
        const auto m_Dp = out.at(2);

        CHECK(tc.f  == doctest::Approx(m_f).scale(tc.f).epsilon(tc.f_rtol));
        CHECK(tc.D  == doctest::Approx(m_D).scale(tc.D).epsilon(tc.D_rtol));
        CHECK(tc.Dp == doctest::Approx(m_Dp).scale(tc.Dp).epsilon(tc.Dp_rtol));

        //REQUIRE(tc.f  == doctest::Approx(m_f).epsilon(0.05));
        //REQUIRE(tc.D  == doctest::Approx(m_D).epsilon(0.05));
        //REQUIRE(tc.Dp == doctest::Approx(m_Dp).epsilon(0.05));
    }
}


TEST_CASE( "MRI_IVIM::GetBiExp" ){
    // Verify that the model can be recovered correctly under a variety of situations.
    struct test_case {
        int sample;
        std::string name;
        std::string desc;
        std::string gen_f; // generating formula.

        double S0; // Model parameters used to generate the S(b) curve data.
        double f;
        double D;
        double Dp;

        double bvalue_threshold;

        double f_rtol;  // Relative tolerance recovering f, i.e., 0.01 = 1%.
        double D_rtol;  // Relative tolerance recovering D, i.e., 0.01 = 1%.
        double Dp_rtol; // Relative tolerance recovering Dp, i.e., 0.01 = 1%.

        std::vector<float> b_vals;
        std::vector<float> S_vals;
    };


    // Given a set of model parameters and b-values, generate synthetic voxel intensities.
    const auto generate_Ss = []( const test_case &t ) -> std::vector<float> {
        std::vector<float> S_vals;
        S_vals = t.b_vals;
        for(size_t i = 0UL; i < S_vals.size(); i++){
            S_vals.at(i) = MRI_IVIM::evaluate_biexp( t.b_vals.at(i), t.S0, t.f, t.D, t.Dp );
        }
        return S_vals;
    };

    std::vector<test_case> tcs;

    tcs.emplace_back();
    tcs.back().sample = 1;
    tcs.back().name   = "IVIM signal full";
    tcs.back().desc   = "two-compartment model";
    tcs.back().gen_f  = "S(b) = S0 * [ f*exp(-b*Dp) + (1-f)*exp(-b*D) ]";
    tcs.back().S0     = 1.0;   // arb units
    tcs.back().f      = 0.3;   // arb units
    tcs.back().D      = 0.001; // mm^2/s
    tcs.back().Dp     = 0.01;  // mm^2/s
    tcs.back().bvalue_threshold = 300.0;
    tcs.back().f_rtol  = 0.05;
    tcs.back().D_rtol  = 0.05;
    tcs.back().Dp_rtol = 0.05;
    tcs.back().b_vals = { 0,20,30,40,50,60,70,80,90,100,120,150,250,400,800,1000 };
    tcs.back().S_vals = generate_Ss(tcs.back());


    tcs.emplace_back();
    tcs.back().sample = 5;
    tcs.back().name   = "IVIM signal high";
    tcs.back().desc   = "two-compartment model";
    tcs.back().gen_f  = "S(b) = S0 * [ f*exp(-b*Dp) + (1-f)*exp(-b*D) ]";
    tcs.back().S0     = 1.0;   // arb units
    tcs.back().f      = 0.3;   // arb units
    tcs.back().D      = 0.001; // mm^2/s
    tcs.back().Dp     = 0.01;  // mm^2/s
    tcs.back().bvalue_threshold = 200.0;
    tcs.back().f_rtol  = 0.05;
    tcs.back().D_rtol  = 0.05;
    tcs.back().Dp_rtol = 0.25;
    tcs.back().b_vals = { 0,200,400,800 };
    tcs.back().S_vals = generate_Ss(tcs.back());


    tcs.emplace_back();
    tcs.back().sample = 9;
    tcs.back().name   = "IVIM signal whitepaper";
    tcs.back().desc   = "two-compartment model";
    tcs.back().gen_f  = "S(b) = S0 * [ f*exp(-b*Dp) + (1-f)*exp(-b*D) ]";
    tcs.back().S0     = 1.0;   // arb units
    tcs.back().f      = 0.3;   // arb units
    tcs.back().D      = 0.001; // mm^2/s
    tcs.back().Dp     = 0.01;  // mm^2/s
    tcs.back().bvalue_threshold = 250.0;
    tcs.back().f_rtol  = 0.05;
    tcs.back().D_rtol  = 0.05;
    tcs.back().Dp_rtol = 0.05;
    tcs.back().b_vals = { 0,30,70,100,200,400,800 };
    tcs.back().S_vals = generate_Ss(tcs.back());


    for(const auto &tc : tcs){
        CAPTURE( tc.sample );
        CAPTURE( tc.name );
        CAPTURE( tc.desc );
        CAPTURE( tc.f_rtol );
        CAPTURE( tc.D_rtol );
        CAPTURE( tc.Dp_rtol );

        REQUIRE(tc.b_vals.size() == tc.S_vals.size());
        REQUIRE(tc.b_vals.size() > 2UL);

        const int num_iters = 100;
        const auto out  = MRI_IVIM::GetBiExp(tc.b_vals, tc.S_vals, num_iters, tc.bvalue_threshold);
        const auto m_f  = out.at(0);
        const auto m_D  = out.at(1);
        const auto m_Dp = out.at(2);

        CHECK(tc.f  == doctest::Approx(m_f).scale(tc.f).epsilon(tc.f_rtol));
        CHECK(tc.D  == doctest::Approx(m_D).scale(tc.D).epsilon(tc.D_rtol));
        CHECK(tc.Dp == doctest::Approx(m_Dp).scale(tc.Dp).epsilon(tc.Dp_rtol));
    }
}

