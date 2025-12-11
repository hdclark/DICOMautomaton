// MRI_IVIM_2.cc - A part of DICOMautomaton 2025. Written by Caleb Sample, Hal Clark, and Arash Javanmardi.

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

#include "YgorImages.h"
#include "YgorString.h"       //Needed for GetFirstRegex(...)

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

#include "MRI_IVIM_2.h"

using Eigen::MatrixXd;

namespace MRI_IVIM_2 {

std::vector<double> GetHessianAndGradient(const std::vector<float> &bvalues, const std::vector<float> &vals, float f, double pseudoD, const double D){
    //This function returns the hessian as the first 4 elements in the vector (4 matrix elements, goes across columns and then rows) and the last two elements are the gradient (derivative_f, derivative_pseudoD)

    const auto F = static_cast<double>(f);
    double derivative_f = 0.0;
    double derivative_ff = 0.0;
    double derivative_pseudoD = 0.0;
    double derivative_pseudoD_pseudoD = 0.0;
    double derivative_fpseudoD = 0.0;
    double derivative_pseudoDf = 0.0;
    const auto number_bVals = static_cast<double>( bvalues.size() );

    for(size_t i = 0; i < number_bVals; ++i){
        const double c = exp(-bvalues[i] * D);
        float b = bvalues[i];
        double expon = exp(-b * pseudoD);
        float signal = vals.at(i);
        

        derivative_f += 2.0 * (signal - F*expon - (1.0-F)*c) * (-expon + c);
        derivative_pseudoD += 2.0 * ( signal - F*expon - (1.0-F)*c ) * (b*F*expon);

        derivative_ff += 2.0 * std::pow((c - expon), 2.0);
        derivative_pseudoD_pseudoD += 2.0 * (b*F*expon) - 2.0 * (signal - F*expon-(1.0-F)*c)*(b*b*F*expon);

        derivative_fpseudoD += (2.0 * (c - expon)*b*F*expon) + (2.0 * (signal - F*expon - (1.0-F)*c) * b*expon );
        derivative_pseudoDf += (2.0 * (b*F*expon)*(-expon + c)) + (2.0*(signal - F*expon - (1.0-F)*c)*(b*expon));

    }   
    std::vector<double> H;
    H.push_back(derivative_ff); 
    H.push_back(derivative_fpseudoD);
    H.push_back(derivative_pseudoDf);
    H.push_back(derivative_pseudoD_pseudoD);
    H.push_back(derivative_f);
    H.push_back(derivative_pseudoD);

    return H;
}

std::vector<double> GetInverse(const std::vector<double> &matrix){
    std::vector<double> inverse;
    double determinant = 1 / (matrix.at(0)*matrix.at(3) - matrix.at(1)*matrix.at(2));

    inverse.push_back(determinant * matrix.at(3));
    inverse.push_back(- determinant * matrix.at(1));
    inverse.push_back(- determinant * matrix.at(2));
    inverse.push_back(determinant * matrix.at(0));
    return inverse;
}


#ifdef DCMA_USE_EIGEN
double GetKurtosisModel(float b, const std::vector<double> &params){
    double f = params.at(0);
    double pseudoD = params.at(1);
    double D = params.at(2);
    double K = params.at(3);
    double NCF = params.at(4);

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
        if (bvalues.at(i) == 0){ //first get the index of b = 0 (I'm unsure if b values are in order already)
            b0_index = i;
            break;
        }         
        
    }
    for(size_t i = 0; i < number_bVals; ++i){
        signals.push_back(vals.at(b0_index));           
        
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
    for (int i = 0; i < 5; i++){
        newParams.push_back(0.0);
    }
    
    for (int i = 0; i < numIterations; i++){
         
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
    b_avg /= (1.0 * number_bVals);
    log_S_avg /= (1.0 * number_bVals);

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

// Fixed GetBiExp implementation with consensus-aligned WLLS
// Drop-in replacement for ModelIVIM.cc

double GetADC_WLLS(const std::vector<float> &bvalues, const std::vector<float> &vals, 
                   int max_iterations = 10, double tolerance = 1e-6){
    
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
        if(std::abs(denominator) < 1e-12){
            return nan; // Singular system
        }
        
        const double ln_S0_new = (sum_wb2 * sum_wlogS - sum_wb * sum_wb_logS) / denominator;
        const double D_new = (sum_wb * sum_wlogS - sum_w * sum_wb_logS) / denominator;
        
        // Ensure physical constraints
        D_current = std::max(D_new, 1e-8);  // Positive diffusion
        S0_current = std::exp(ln_S0_new);
        
        // Check convergence
        if(std::abs(D_current - D_previous) < tolerance * std::abs(D_previous)){
            break;
        }
    }
    
    // Final validation
    if(!std::isfinite(D_current) || D_current <= 0 || D_current > 0.01){
        return nan;
    }
    
    return D_current;
}

std::array<double, 3> GetBiExp(const std::vector<float> &bvalues, const std::vector<float> &vals, int numIterations){
    
    const auto nan = std::numeric_limits<double>::quiet_NaN();
    const auto number_bVals = bvalues.size();
    
    // Find b=0 index
    int b0_index = 0;
    for(size_t i = 0; i < number_bVals; ++i){
        if (bvalues[i] == 0.0){ 
            b0_index = i;
            break;
        }
    }
    
    // Extract high b-values for D estimation (consensus: use raw signals for WLLS)
    std::vector<float> bvaluesH, signalsH;
    for(size_t i = 0; i < number_bVals; ++i){
        if (bvalues[i] > 200){         
            bvaluesH.push_back(bvalues[i]);
            signalsH.push_back(vals[i]); // Use raw signals for WLLS
        }
    }
    
    if(bvaluesH.size() < 2){
        return {nan, nan, nan}; // Insufficient high b-values
    }
    
    // Step 1: Estimate D using consensus-recommended WLLS
    double D = GetADC_WLLS(bvaluesH, signalsH);
    
    // Fallback to original method if WLLS fails
    if(!std::isfinite(D) || D <= 0){
        D = GetADCls(bvaluesH, signalsH);
        if(!std::isfinite(D) || D <= 0){
            return {nan, nan, nan};
        }
    }
    
    // Step 2: Prepare normalized signals for LM optimization
    std::vector<float> signals_normalized;
    for(size_t i = 0; i < number_bVals; ++i){
        signals_normalized.push_back(vals[i] / vals[b0_index]);
    }
    
    // Convert to Eigen matrices for LM optimization
    MatrixXd sigs(number_bVals, 1);
    for(size_t i = 0; i < number_bVals; ++i){
        sigs(i, 0) = signals_normalized[i];
    }
    
    // Step 3: Estimate f and D* using Levenberg-Marquardt
    float lambda = 1.0;  // Start with smaller lambda
    double pseudoD = 10.0 * D;  // Consensus initial guess
    float f = 0.3;  // Better initial guess for parotid glands
    
    // Parameter bounds for parotid glands
    const float f_min = 0.0, f_max = 0.4;
    const double pseudoD_min = 3.0 * D, pseudoD_max = 0.15;
    
    MatrixXd h(2,1);
    MatrixXd r(number_bVals, 1);
    MatrixXd J(number_bVals, 2);
    MatrixXd sigs_pred(number_bVals, 1);
    MatrixXd I = MatrixXd::Identity(2, 2); // Fixed: properly initialized identity matrix
    
    // Initial predictions and cost
    for(size_t i = 0; i < number_bVals; ++i){ 
        const double exp_diff   = std::exp(-bvalues[i] * D);
        const double exp_pseudo = std::exp(-bvalues[i] * pseudoD);
        sigs_pred(i,0) = f * exp_pseudo + (1.0 - f) * exp_diff;
    }
    r = sigs - sigs_pred;
    double cost = 0.5 * (r.transpose() * r)(0,0);
    
    int successful_updates = 0;
    double tolerance = 1e-6;
    
    for (int iter = 0; iter < numIterations; iter++){
        
        // Compute Jacobian
        for(size_t i = 0; i < number_bVals; ++i){ 
            const double exp_pseudo = std::exp(-bvalues[i] * pseudoD);
            const double exp_diff = std::exp(-bvalues[i] * D);
            
            J(i,0) = -exp_pseudo + exp_diff;  // ∂/∂f
            J(i,1) = bvalues[i] * f * exp_pseudo;  // ∂/∂D*
        }
        
        // Levenberg-Marquardt update with numerical stability check
        MatrixXd JTJ = J.transpose() * J;
        MatrixXd JTr = J.transpose() * r;
        
        // Check for numerical issues
        if(JTJ.determinant() < 1e-12){
            break; // Matrix near singular
        }
        
        h = (JTJ + lambda * I).inverse() * JTr;
        
        // Update parameters with bounds enforcement
        float new_f = std::max(f_min, std::min(f_max, f + static_cast<float>(h(0,0))));
        double new_pseudoD = std::max(pseudoD_min, std::min(pseudoD_max, pseudoD + h(1,0)));
        
        // Compute new predictions and cost
        for(size_t i = 0; i < number_bVals; ++i){ 
            const double exp_diff = std::exp(-bvalues[i] * D);
            const double exp_new_pseudo = std::exp(-bvalues[i] * new_pseudoD);
            sigs_pred(i,0) = new_f * exp_new_pseudo + (1.0 - new_f) * exp_diff;
        }
        r = sigs - sigs_pred;
        double new_cost = 0.5 * (r.transpose() * r)(0,0);
        
        // Accept or reject update
        if (new_cost < cost){
            f = new_f;
            pseudoD = new_pseudoD;
            cost = new_cost;
            lambda *= 0.7;  // Reduce damping
            successful_updates++;
            
            // Check for convergence
            if(std::abs(h(0,0)) < tolerance && std::abs(h(1,0)) < tolerance * pseudoD){
                break; // Converged
            }
        } else {
            lambda *= 1.5;  // Increase damping
        }
        
        // Prevent lambda from becoming too large
        if(lambda > 1e6){
            break;
        }
    }
    
    // Final parameter validation for parotid glands
    const bool valid_D = (0.0008 <= D && D <= 0.002);
    const bool valid_f = (0.05 <= f && f <= 0.35);
    const bool valid_pseudoD = (0.01 <= pseudoD && pseudoD <= 0.12) && (pseudoD > 2*D);
    
    if(!valid_D || !valid_f || !valid_pseudoD || successful_updates < 3){
        // Could log warnings or return constrained values instead of NaN
        // For now, return the fitted values even if outside expected ranges
        // since parotid glands may have different characteristics
    }
    
    // Ensure finite results
    if(!std::isfinite(f) || !std::isfinite(D) || !std::isfinite(pseudoD)){
        return {nan, nan, nan};
    }
    
    return {f, D, pseudoD};
}

} // namespace MRI_IVIM_2
