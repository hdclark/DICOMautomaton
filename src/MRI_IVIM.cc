// MRI_IVIM.cc - A part of DICOMautomaton 2022. Written by Caleb Sample and Hal Clark.

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

#include "MRI_IVIM.h"

using Eigen::MatrixXd;

namespace MRI_IVIM {

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

std::array<double, 3> GetBiExp(const std::vector<float> &bvalues, const std::vector<float> &vals, int numIterations){
    //This function will use the a segmented approach with Marquardts method of squared residuals minimization to fit the signal to a biexponential 
    const auto nan = std::numeric_limits<double>::quiet_NaN();
    //The biexponential model
    //S(b) = S(0)[f * exp(-b D*) + (1-f) * exp(-b D)]

    
    const auto number_bVals = bvalues.size();
    int b0_index = 0;

    //first get the index of b = 0 (I'm unsure if b values are in order already)
    for(size_t i = 0; i < number_bVals; ++i){
        if (bvalues[i] == 0.0){ 
            b0_index = i;
            break;
        }
    }
    //Divide all signals by S(0)
    //also create a vector for signals and b-values

    std::vector<float> bvaluesH; //for high b values and signals
    std::vector<float> signalsH;

    MatrixXd sigs(number_bVals,1);
    MatrixXd b_vals(number_bVals,1);   
    std::vector<float> signals;

    for(size_t i = 0; i < number_bVals; ++i){

        const auto normalized = vals.at(i)/vals.at(b0_index);
        signals.push_back(normalized);
        sigs(i,0) = normalized;
        b_vals(i,0) = bvalues.at(i);

        //we use a cutoff of b values greater than 200 to have signals of the form S(b) = S(0) * exp(-b D) to obtain the diffusion coefficient
        //make a vector for the high b values and their signals
        if (bvalues[i] > 200){         
            bvaluesH.push_back(bvalues.at(i));
            signalsH.push_back(vals.at(i));            
        }
    }

    
    //Now use least squares regression to obtain D from the high b value signals: 
    double D = GetADCls(bvaluesH, signalsH);

    //Now we can use this D value and fit f and D* with Marquardts method
    //Cost function is sum (Signal_i - (fexp(-bD*) + (1-f)exp(-bD)) )^2
    float lambda = 10;
    double pseudoD = 10.0 * D;
    float f = 0.5;
    double new_pseudoD;
    float new_f;
    double cost; 
    double new_cost;

    MatrixXd h(2,1);
    MatrixXd r(number_bVals, 1); //residuals
    MatrixXd J(number_bVals, 2);
    MatrixXd sigs_pred(number_bVals, 1);
    MatrixXd I(2,2);

//Get the current cost
    // cost = 0;
    // for(size_t i = 0; i < number_bVals; ++i){
    //     bTemp = bvalues[i];         
    //     signalTemp = signals.at(i);
    //     cost += std::pow( ( (signalTemp) - f*exp(-bTemp * pseudoD) - (1-f)*exp(-bTemp * D)   ), 2.0);
    // }  
    
    //Get initial predictions
    for(size_t i = 0; i < number_bVals; ++i){ 
        const double exp_diff   = std::exp(-bvalues.at(i) * D);
        const double exp_pseudo = std::exp(-bvalues.at(i) * pseudoD);
        sigs_pred(i,0) = f * exp_pseudo + (1.0 - f) * exp_diff;
    }
    //Get the initial residuals:
    r = sigs - sigs_pred;
    //get initial cost
    cost = 0.5 * (r.transpose() * r)(0);

    //iteratively adjust parameters to minimize cost
    for (int i = 0; i < numIterations; i++){

        //Get Jacobian
        for(size_t i = 0; i < number_bVals; ++i){ 
            const double exp_pseudo = std::exp(-bvalues[i]*pseudoD);
            //get Jacobian
            J(i,0) = -exp_pseudo + std::exp(-bvalues[i]*D);
            J(i,1) = bvalues[i] * f * exp_pseudo;
        }
        
        //levenberg marquardt --> update increment is h = (J^T * J + lambda I )^-1 * J^T r
        h = ((J.transpose() * J + lambda * I).inverse() * J.transpose() * r);
        //update parameters
        new_f = f + h(0,0);
        new_pseudoD = pseudoD + h(1,0); 

        //get new predictions and residuals and cost
        //Get predictions
        for(size_t i = 0; i < number_bVals; ++i){ 
            const double exp_diff       = std::exp(-bvalues.at(i) * D);
            const double exp_new_pseudo = std::exp(-bvalues.at(i) * new_pseudoD);
            sigs_pred(i,0) = new_f * exp_new_pseudo + (1.0 - new_f) * exp_diff;
        }
        //Get residuals:
        r = sigs - sigs_pred;
        //get cost
        new_cost = 0.5 * (r.transpose() * r)(0);

        if (new_cost < cost){
            f = new_f;
            pseudoD = new_pseudoD;
            cost = new_cost;
            lambda /= 1.5;

        }else{
            lambda *= 1.1;
        }
    }
    return { f, D, pseudoD };
}

} // namespace MRI_IVIM
