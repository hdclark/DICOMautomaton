//ModelIVIM.cc - A part of DICOMautomaton 2021. Written by hal clark.

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
#include <math.h>

#include "YgorImages.h"
#include "YgorString.h"       //Needed for GetFirstRegex(...)

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../BED_Conversion.h"
#include "../YgorImages_Functors/Compute/Joint_Pixel_Sampler.h"
#include "../Eigen/Core"
#include "../Eigen/LU"
#include "ModelIVIM.h"

using Eigen::MatrixXd;


OperationDoc OpArgDocModelIVIM(){
    OperationDoc out;
    out.name = "ModelIVIM";
    out.desc = 
        "This operation fits an Intra-voxel Incoherent Motion (IVIM) model to a series of diffusion-weighted"
        " MR images.";

    out.notes.emplace_back(
        "Images are overwritten, but their geometry is used to define the final map."
        " ReferenceImages are used for modeling, but are treated as read-only."
        " ReferenceImages should correspond to unique b-values, one b-value per ReferenceImages array."
    );
    out.notes.emplace_back(
        "The reference image array must be rectilinear. (This is a requirement specific to this"
        " implementation, a less restrictive implementation could overcome the issue.)"
    );
    out.notes.emplace_back(
        "For the fastest and most accurate results, test and reference image arrays should spatially align."
        " However, alignment is **not** necessary. If test and reference image arrays are aligned,"
        " image adjacency can be precomputed and the analysis will be faster. If not, image adjacency"
        " must be evaluated for each image slice. If this also fails, it will be evaluated for every voxel."
    );
    out.notes.emplace_back(
        "This operation will make use of interpolation if corresponding voxels do not exactly overlap."
    );


    out.args.emplace_back();
    out.args.back() = IAWhitelistOpArgDoc();
    out.args.back().name = "ImageSelection";
    out.args.back().default_val = "first";
    out.args.back().desc = "The transformed image array where voxel intensities represent the Apparent"
                           " Diffusion Coefficient (ADC). "
                         + out.args.back().desc;

    out.args.emplace_back();
    out.args.back() = IAWhitelistOpArgDoc();
    out.args.back().name = "ReferenceImageSelection";
    out.args.back().default_val = "!first";
    out.args.back().desc = "The 3D image arrays where each 3D volume corresponds to a single b-value. "
                         + out.args.back().desc;

    out.args.emplace_back();
    out.args.back() = NCWhitelistOpArgDoc();
    out.args.back().name = "NormalizedROILabelRegex";
    out.args.back().default_val = ".*";

    out.args.emplace_back();
    out.args.back() = RCWhitelistOpArgDoc();
    out.args.back().name = "ROILabelRegex";
    out.args.back().default_val = ".*";

    out.args.emplace_back();
    out.args.back().name = "Model";
    out.args.back().desc = "The model that will be fitted.."
                           " Currently, 'adc-simple' , 'adc-ls' , 'biexp', and 'kurtosis' are available."
                           " The 'adc-simple' model does not take into account perfusion, only free diffusion is"
                           " modeled. It only uses the minimum and maximum b-value images and analytically estimates"
                           " ADC."
                           " The 'adc-ls' model, like 'adc-simple', only models free diffusion."
                           " It fits a linear least-squares model that uses all available b-value images."
                           "The 'biexp' model uses a segmented fitting approach along with Marquardts method to fit a biexponential decay model, obtaining the pseudodiffusion fraction, pseudodiffusion coefficient, and diffusion coefficient for each voxel."
                           "The 'kurtosis' model returns 5 parameters corresponding to a biexponential diffsion model with a Kurtosis adjustment, as well as a noise floor parameter added in quadrature with the model.";
                           
    out.args.back().default_val = "adc-simple";
    out.args.back().expected = true;
    out.args.back().examples = { "adc-simple",
                                 "adc-ls" ,
                                 "biexp", 
                                 "kurtosis"};
    out.args.back().samples = OpArgSamples::Exhaustive;

    out.args.emplace_back();
    out.args.back().name = "Channel";
    out.args.back().desc = "The channel to compare (zero-based)."
                           " Setting to -1 will compare each channel separately."
                           " Note that both test images and reference images must share this specifier.";
    out.args.back().default_val = "0";
    out.args.back().expected = true;
    out.args.back().examples = { "-1",
                                 "0",
                                 "1",
                                 "2" };

    out.args.emplace_back();
    out.args.back().name = "TestImgLowerThreshold";
    out.args.back().desc = "Pixel lower threshold for the test images."
                           " Only voxels with values above this threshold (inclusive) will be altered.";
    out.args.back().default_val = "-inf";
    out.args.back().expected = true;
    out.args.back().examples = { "-inf",
                                 "0.0",
                                 "200" };

    out.args.emplace_back();
    out.args.back().name = "TestImgUpperThreshold";
    out.args.back().desc = "Pixel upper threshold for the test images."
                           " Only voxels with values below this threshold (inclusive) will be altered.";
    out.args.back().default_val = "inf";
    out.args.back().expected = true;
    out.args.back().examples = { "inf",
                                 "1.23",
                                 "1000" };

    return out;
}



Drover ModelIVIM(Drover DICOM_data,
                 const OperationArgPkg& OptArgs,
                 const std::map<std::string, std::string>& /*InvocationMetadata*/,
                 const std::string& /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();
    const auto ReferenceImageSelectionStr = OptArgs.getValueStr("ReferenceImageSelection").value();

    const auto NormalizedROILabelRegex = OptArgs.getValueStr("NormalizedROILabelRegex").value();
    const auto ROILabelRegex = OptArgs.getValueStr("ROILabelRegex").value();

    const auto ModelStr = OptArgs.getValueStr("Model").value();
    const auto Channel = std::stol( OptArgs.getValueStr("Channel").value() );
    const auto TestImgLowerThreshold = std::stod( OptArgs.getValueStr("TestImgLowerThreshold").value() );
    const auto TestImgUpperThreshold = std::stod( OptArgs.getValueStr("TestImgUpperThreshold").value() );

    //-----------------------------------------------------------------------------------------------------------------
    const auto model_adc_simple = Compile_Regex("^ad?c?[-_]?si?m?p?l?e?$");
    const auto model_adc_ls = Compile_Regex("^ad?c?[-_]?ls?$");
    const auto model_biexp = Compile_Regex("^bie?x?p?");
    const auto model_kurtosis = Compile_Regex("^ku?r?t?o?s?i?s?");

    //-----------------------------------------------------------------------------------------------------------------

    //Stuff references to all contours into a list. Remember that you can still address specific contours through
    // the original holding containers (which are not modified here).
    auto cc_all = All_CCs( DICOM_data );
    auto cc_ROIs = Whitelist( cc_all, { { "ROIName", ROILabelRegex },
                                        { "NormalizedROIName", NormalizedROILabelRegex } } );
    if(cc_ROIs.empty()){
        throw std::invalid_argument("No contours selected. Cannot continue.");
    }

    auto RIAs_all = All_IAs( DICOM_data );
    auto RIAs = Whitelist( RIAs_all, ReferenceImageSelectionStr );
    if(RIAs.size() < 2){
        throw std::invalid_argument("At least two b-value images are required to model ADC.");
    }
    std::list<std::reference_wrapper<planar_image_collection<float, double>>> RIARL;
    for(const auto & RIA : RIAs){
        RIARL.emplace_back( std::ref( (*RIA)->imagecoll ) );
    }

    // Identify the b-value of each reference image, which is needed for later analysis.
    std::vector<float> bvalues;
    for(const auto &RIA_refw : RIARL){
        const auto vals = RIA_refw.get().get_distinct_values_for_key("DiffusionBValue");
        if(vals.size() != 1){
            throw std::invalid_argument("Reference image does not contain a single distinct b-value.");
        }
        bvalues.emplace_back( std::stod(vals.front()) );
    }

    // Sort the RIARL using bvalues, to simplify access later.
    // ...
    const auto bvalue_min_i = std::distance( std::begin(bvalues), std::min_element( std::begin(bvalues), std::end(bvalues) ) );
    const auto bvalue_max_i = std::distance( std::begin(bvalues), std::max_element( std::begin(bvalues), std::end(bvalues) ) );
    const auto nan = std::numeric_limits<double>::quiet_NaN();

    FUNCINFO("Detected minimum bvalue is b(" << bvalue_min_i << ") = " << bvalues.at( bvalue_min_i ));
    FUNCINFO("Detected maximum bvalue is b(" << bvalue_max_i << ") = " << bvalues.at( bvalue_max_i ));
    if( bvalues.at( bvalue_min_i ) == bvalues.at( bvalue_max_i ) ){
        throw std::runtime_error("Insufficient number of distinct b-value images to perform modeling");
    }
    auto IAs_all = All_IAs( DICOM_data );
    auto IAs = Whitelist( IAs_all, ImageSelectionStr );
    for(auto & iap_it : IAs){
        ComputeJointPixelSamplerUserData ud;
        ud.sampling_method = ComputeJointPixelSamplerUserData::SamplingMethod::LinearInterpolation;
        ud.channel = Channel;
        ud.inc_lower_threshold = TestImgLowerThreshold;
        ud.inc_upper_threshold = TestImgUpperThreshold;

        if(std::regex_match(ModelStr, model_adc_simple)){
            ud.description = "ADC (simple model)";
            ud.f_reduce = [bvalues, bvalue_min_i, bvalue_max_i]( std::vector<float> &vals, 
                                                                 vec3<double> ) -> float {
                vals.erase(vals.begin()); // Remove the base image's value.
                if(vals.size() != bvalues.size()){
                    FUNCERR("Unmatched voxel and b-value vectors. Refusing to continue.");
                }

                const auto bvalue_min = bvalues.at( bvalue_min_i );
                const auto bvalue_max = bvalues.at( bvalue_max_i );

                const auto signal_at_bvalue_min = vals.at( bvalue_min_i);
                const auto signal_at_bvalue_max = vals.at( bvalue_max_i);

                const auto adc = std::log( signal_at_bvalue_min / signal_at_bvalue_max) / (bvalue_max - bvalue_min);
                if(!std::isfinite( adc )) throw std::runtime_error("adc is not finite");
                return adc;
                
            };

        }else if(std::regex_match(ModelStr, model_adc_ls)){
            ud.description = "ADC (linear least squares)";
            ud.f_reduce = [bvalues, bvalue_min_i, bvalue_max_i]( std::vector<float> &vals, 
                                                                 vec3<double> ) -> float {
                vals.erase(vals.begin()); // Remove the base image's value.
                if(vals.size() != bvalues.size()){
                    FUNCERR("Unmatched voxel and b-value vectors. Refusing to continue.");
                }

                const auto adc = GetADCls(bvalues, vals);
                if(!std::isfinite( adc )) throw std::runtime_error("adc is not finite");
                return adc;
                
            };

        }else if(std::regex_match(ModelStr, model_kurtosis)){
            // Add channels to each image for each model parameter.
            auto imgarr_ptr = &((*iap_it)->imagecoll);
            for(auto &img : imgarr_ptr->images){
                img.add_channel( nan ); // for D.
                img.add_channel( nan ); // for pseduoD.
            }
            const long int chan_f = Channel;
            const long int chan_D = chan_f + 1;
            const long int chan_pD = chan_f + 2;

            ud.description = "f, D, pseudoD (Kurtosis Model fit)";
            ud.f_reduce = [bvalues,
                           bvalue_min_i,
                           bvalue_max_i,
                           imgarr_ptr,
                           chan_D,
                           chan_pD ]( std::vector<float> &vals, 
                                      vec3<double> pos ) -> float {
                vals.erase(vals.begin()); // Remove the base image's value.
                if(vals.size() != bvalues.size()){
                    FUNCERR("Unmatched voxel and b-value vectors. Refusing to continue.");
                }
                int numIterations = 1000;
                
                const auto [f, D, pseudoD] = GetKurtosisParams(bvalues, vals, numIterations);
                if(!std::isfinite( f )) throw std::runtime_error("f is not finite");

                // The image/voxel iterator interface isn't capable of handling multiple-channel values,
                // so we have to explicitly lookup the position and insert it directly.
                const auto img_it_l = imgarr_ptr->get_images_which_encompass_point(pos);
                if(img_it_l.size() != 1){
                    throw std::logic_error("Unable to find singular overlapping image.");
                }
                const auto index_D  = img_it_l.front()->index(pos, chan_D);
                const auto index_pD = img_it_l.front()->index(pos, chan_pD);
                if( (index_D < 0)
                ||  (index_pD < 0) ){
                    throw std::logic_error("Unable to locate voxel via position");
                }
                img_it_l.front()->reference(index_D) = D;
                img_it_l.front()->reference(index_pD) = pseudoD;

                return f;
                
            };

        }else if(std::regex_match(ModelStr, model_biexp)){
            // Add channels to each image for each model parameter.
            auto imgarr_ptr = &((*iap_it)->imagecoll);
            for(auto &img : imgarr_ptr->images){
                img.add_channel( nan ); // for D.
                img.add_channel( nan ); // for pseduoD.
            }
            const long int chan_f = Channel;
            const long int chan_D = chan_f + 1;
            const long int chan_pD = chan_f + 2;

            ud.description = "f, D, pseudoD (Bi-exponential segmented fit)";
            ud.f_reduce = [bvalues,
                           bvalue_min_i,
                           bvalue_max_i,
                           imgarr_ptr,
                           chan_D,
                           chan_pD ]( std::vector<float> &vals, 
                                      vec3<double> pos ) -> float {
                vals.erase(vals.begin()); // Remove the base image's value.
                if(vals.size() != bvalues.size()){
                    FUNCERR("Unmatched voxel and b-value vectors. Refusing to continue.");
                }
                int numIterations = 1000;
                const auto [f, D, pseudoD] = GetBiExpf(bvalues, vals, numIterations);
                if(!std::isfinite( f )) throw std::runtime_error("f is not finite");

                // The image/voxel iterator interface isn't capable of handling multiple-channel values,
                // so we have to explicitly lookup the position and insert it directly.
                const auto img_it_l = imgarr_ptr->get_images_which_encompass_point(pos);
                if(img_it_l.size() != 1){
                    throw std::logic_error("Unable to find singular overlapping image.");
                }
                const auto index_D  = img_it_l.front()->index(pos, chan_D);
                const auto index_pD = img_it_l.front()->index(pos, chan_pD);
                if( (index_D < 0)
                ||  (index_pD < 0) ){
                    throw std::logic_error("Unable to locate voxel via position");
                }
                img_it_l.front()->reference(index_D) = D;
                img_it_l.front()->reference(index_pD) = pseudoD;

                return f;
                
            };

        }
        else{
            throw std::invalid_argument("Model not understood. Cannot continue.");
        }

        if(!(*iap_it)->imagecoll.Compute_Images( ComputeJointPixelSampler, 
                                                 RIARL, cc_ROIs, &ud )){
            throw std::runtime_error("Unable to analyze images.");
        }
    }

    return DICOM_data;
}



std::vector<double> GetHessianAndGradient(const std::vector<float> &bvalues, const std::vector<float> &vals, float f, double pseudoD, const double D){
    //This function returns the hessian as the first 4 elements in the vector (4 matrix elements, goes across columns and then rows) and the last two elements are the gradient (derivative_f, derivative_pseudoD)

    double derivative_f;
    double derivative_ff;
    double derivative_pseudoD;
    double derivative_pseudoD_pseudoD;
    double derivative_fpseudoD;
    double derivative_pseudoDf;
    const auto number_bVals = static_cast<double>( bvalues.size() );

    for(size_t i = 0; i < number_bVals; ++i){
        const double c = exp(-bvalues[i] * D);
        float b = bvalues[i];
        double expon = exp(-b * pseudoD);
        float signal = vals.at(i);
        

        derivative_f += 2 * (signal - f*expon - (1-f)*c) * (-expon + c);
        derivative_pseudoD += 2 * ( signal - f*expon - (1-f)*c ) * (b*f*expon);

        derivative_ff += 2 * std::pow((c - expon), 2.0);
        derivative_pseudoD_pseudoD += 2 * (b*f*expon) - 2 * (signal - f*expon-(1-f)*c)*(b*b*f*expon);

        derivative_fpseudoD += ( 2 * (c - expon)*b*f*expon) + ( 2 * (signal - f*expon - (1-f)*c) * b*expon );
        derivative_pseudoDf += (2 * (b*f*expon)*(-expon + c)) + (2*(signal - f*expon - (1-f)*c)*(b*expon));

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

std::vector<double> GetInverse(const std::vector<double> matrix){
    std::vector<double> inverse;
    double determinant = 1 / (matrix[0]*matrix[3] - matrix[1]*matrix[2]);

    inverse.push_back(determinant * matrix[3]);
    inverse.push_back(- determinant * matrix[1]);
    inverse.push_back(- determinant * matrix[2]);
    inverse.push_back(determinant * matrix[0]);
    return inverse;


}


double GetKurtosisModel(float b, const std::vector<double> params){
    double f = params[0];
    double pseudoD = params[1];
    double D = params[2];
    double K = params[3];
    double NCF = params[4];

    double model = f*exp(-b * pseudoD) + (1-f) * exp(-b*D + std::pow((b*D), 2)*K/6);

    //now add noise floor:
    model = std::pow(model, 2) + std::pow(NCF, 2);
    model = std::pow(model, 0.5);
    return model;

}
double GetKurtosisTheta(const std::vector<float>bvalues, const std::vector<float> signals, std::vector<double> params, std::vector<double> priors){
    const auto number_bVals = static_cast<double>( bvalues.size() );
    double theta = 0;
    //for now priors are uniform so not included in theta. The goal is to maximize theta (not minimize)
    for (size_t i = 0; i < number_bVals; ++i){

        theta -= std::pow((signals.at(i) - GetKurtosisModel(bvalues.at(i), params)), 2);  

    } 
    return theta;    

}
std::vector<double> GetKurtosisPriors(std::vector<double> params){
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

}


void GetKurtosisGradient(MatrixXd &grad, const std::vector<float>bvalues, const std::vector<float> signals, std::vector<double> params, std::vector<double> priors){
    //the kurtosis model with a noise floor correction has 5 parameters, so the gradient will be set as a 5x1 matrix

    std::vector<double> paramsTemp = params;
    double deriv;
    //Numerically determine the derivatives
    double delta = 0.00001;
    
    paramsTemp[0] += delta; //first get f derivative
    deriv = GetKurtosisTheta(bvalues, signals, paramsTemp, priors);
    paramsTemp[0] -= 2 * delta;
    deriv -= GetKurtosisTheta(bvalues, signals, paramsTemp, priors);
    deriv /= 2 * delta;
    grad(0,0) = deriv;
    paramsTemp[0] = params[0]; 

    paramsTemp[1] += delta; //get pseudoD derivative
    deriv = GetKurtosisTheta(bvalues, signals, paramsTemp, priors);
    paramsTemp[1] -= 2 * delta;
    deriv -= GetKurtosisTheta(bvalues, signals, paramsTemp, priors);
    deriv /= 2 * delta;
    grad(1,0) = deriv;
    paramsTemp[1] = params[1];

    paramsTemp[2] += delta; //get D derivative
    deriv = GetKurtosisTheta(bvalues, signals, paramsTemp, priors);
    paramsTemp[2] -= 2 * delta;
    deriv -= GetKurtosisTheta(bvalues, signals, paramsTemp, priors);
    deriv /= 2 * delta;
    grad(2,0) = deriv;
    paramsTemp[2] = params[2];

    paramsTemp[3] += delta; //get K derivative
    deriv = GetKurtosisTheta(bvalues, signals, paramsTemp, priors);
    paramsTemp[3] -= 2 * delta;
    deriv -= GetKurtosisTheta(bvalues, signals, paramsTemp, priors);
    deriv /= 2 * delta;
    grad(3,0) = deriv;
    paramsTemp[3] = params[3];

    paramsTemp[4] += delta; //get D derivative
    deriv = GetKurtosisTheta(bvalues, signals, paramsTemp, priors);
    paramsTemp[2] -= 2 * delta;
    deriv -= GetKurtosisTheta(bvalues, signals, paramsTemp, priors);
    deriv /= 2 * delta;
    grad(4,0) = deriv;
    paramsTemp[4] = params[4];
}


void GetHessian(MatrixXd &hessian, const std::vector<float>bvalues, const std::vector<float> signals, std::vector<double> params, std::vector<double> priors){
    //for 5 parameters we will have a 5x5 Hessian matrix
    MatrixXd gradDiff(5,1);
    std::vector<double> paramsTemp = params;
    //Numerically determine the derivatives
    double delta = 0.00001;
    //second partial derivatives are approximated by the difference in the gradients

    //first row: 
    paramsTemp[0] += delta;
    GetKurtosisGradient(gradDiff, bvalues, signals, paramsTemp, priors); 
    
    paramsTemp[0] -= 2 * delta;
    GetKurtosisGradient(gradDiff, bvalues, signals, paramsTemp, priors); 

    gradDiff /= 2 * delta;
    paramsTemp[0] = params[0]; 
    hessian(0,0) = gradDiff(0,0);
    hessian(0,1) = gradDiff(1,0);
    hessian(0,2) = gradDiff(2,0);
    hessian(0,3) = gradDiff(3,0);
    hessian(0,4) = gradDiff(4,0);

    paramsTemp[0] = params[0];

    //Second row: 
    paramsTemp[1] += delta;
    GetKurtosisGradient(gradDiff, bvalues, signals, paramsTemp, priors); 
    
    paramsTemp[1] -= 2 * delta;
    GetKurtosisGradient(gradDiff, bvalues, signals, paramsTemp, priors); 

    gradDiff /= 2 * delta;
    paramsTemp[1] = params[1]; 
    hessian(1,0) = gradDiff(0,0);
    hessian(1,1) = gradDiff(1,0);
    hessian(1,2) = gradDiff(2,0);
    hessian(1,3) = gradDiff(3,0);
    hessian(1,4) = gradDiff(4,0);

    paramsTemp[1] = params[1];

    //Third row: 
    paramsTemp[2] += delta;
    GetKurtosisGradient(gradDiff, bvalues, signals, paramsTemp, priors); 
    
    paramsTemp[2] -= 2 * delta;
    GetKurtosisGradient(gradDiff, bvalues, signals, paramsTemp, priors); 

    gradDiff /= 2 * delta;
    paramsTemp[2] = params[2]; 
    hessian(2,0) = gradDiff(0,0);
    hessian(2,1) = gradDiff(1,0);
    hessian(2,2) = gradDiff(2,0);
    hessian(2,3) = gradDiff(3,0);
    hessian(2,4) = gradDiff(4,0);

    paramsTemp[2] = params[2];

    //Fourth row: 
    paramsTemp[3] += delta;
    GetKurtosisGradient(gradDiff, bvalues, signals, paramsTemp, priors); 
    
    paramsTemp[3] -= 2 * delta;
    GetKurtosisGradient(gradDiff, bvalues, signals, paramsTemp, priors); 
    gradDiff /= 2 * delta;
    hessian(3,1) = gradDiff(1,0);
    hessian(3,2) = gradDiff(2,0);
    hessian(3,3) = gradDiff(3,0);
    hessian(3,4) = gradDiff(4,0);

    paramsTemp[3] = params[3];

    //Fifth row: 
    paramsTemp[4] += delta;
    GetKurtosisGradient(gradDiff, bvalues, signals, paramsTemp, priors); 
    
    paramsTemp[4] -= 2 * delta;
    GetKurtosisGradient(gradDiff, bvalues, signals, paramsTemp, priors); 

    gradDiff /= 2 * delta;
    paramsTemp[4] = params[1]; 
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
const auto number_bVals = static_cast<double>( bvalues.size() );
int b0_index = 0;
    for(size_t i = 0; i < number_bVals; ++i){
         
        if (bvalues[i] == 0){ //first get the index of b = 0 (I'm unsure if b values are in order already)
            b0_index = i;
            break;
        }         
        
    }
    for(size_t i = 0; i < number_bVals; ++i){
         
        signals.push_back(vals.at(b0_index));           
        
    }

    double f = 0.5;
    double pseudoD = 0.02;
    double D = 0.002;
    double K = 1;
    double NCF = 0;

    std::vector<double> params(5);
    params.push_back(f);
    params.push_back(pseudoD);
    params.push_back(D);
    params.push_back(K);
    params.push_back(NCF);

    std::vector<double> priors = GetKurtosisPriors(params);

    float lambda = 50;

    double new_pseudoD;
    double new_f;
    double new_D;
    double new_K;
    double new_NCF;

    double theta;
    double newTheta;
    MatrixXd H(5,5);
    MatrixXd inverse(5,5);
    MatrixXd gradient(5,1);
    

//Get the current function to maximize log[(likelihood)*(priors)]
    theta = 0;
    

    theta = GetKurtosisTheta(bvalues, signals, params, priors);
    
    
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
                    lambda_I(row,col) = 0;
                }
            }
        }
        H += lambda_I; //add identity to H 
        inverse = H.inverse();
        
        //Now update parameters 
        MatrixXd newParams = inverse * gradient;
        new_f = newParams(0,0);
        new_pseudoD = newParams(1,0);
        new_D = newParams(2,0);
        new_K = newParams(3,0);
        new_NCF = newParams(4,0);
        //if f is less than 0 or greater than 1, rescale back to boundary, and don't let pseudoDD get smaller than D
        if (new_f < 0){
            newParams(0,0) = 0;
        }else if (f > 1){
            f = 1;
        }
        if (pseudoD < 0){
            pseudoD = 0;
        }
        if (D < 0){
            D = 0;
        }
        

        //Now check if we have lowered the cost
        newTheta = GetKurtosisTheta(bvalues, signals, params, priors);

        //accept changes if we have have reduced cost, and lower lambda
        if (newTheta < theta){
            theta = newTheta;
            lambda *= 0.8;
            params[0] = new_f;
            params[1] = new_pseudoD;
            params[2] = new_D;
            params[3] = new_K;
            params[4] = new_NCF;
                      
        }else{
            lambda *= 2;
        }


    }
    return {params[0], params[1], params[2]};
}
double GetADCls(const std::vector<float> &bvalues, const std::vector<float> &vals){
    //This function uses linear regression to obtain the ADC value using the image arrays for all the different b values.
    //This uses the formula S(b) = S(0)exp(-b * ADC)
    // --> ln(S(b)) = ln(S(0)) + (-ADC) * b 

    //First get ADC from the formula -ADC = sum [ (b_i - b_avg) * (ln(S_i) - ln(S)_avg ] / sum( b_i - b_avg )^2
    const auto nan = std::numeric_limits<double>::quiet_NaN();

    //get b_avg and S_avg
    double b_avg = 0.0;
    double log_S_avg = 0.0;
    const auto number_bVals = static_cast<double>( bvalues.size() );
    for(size_t i = 0; i < number_bVals; ++i){
        b_avg += bvalues[i]; 
        log_S_avg += std::log( vals[i] );
        if(!std::isfinite(log_S_avg)){
            return nan;
        }
    }
    b_avg /= number_bVals;
    log_S_avg /= number_bVals;

    //Now do the sums
    double sum_numerator = 0.0;
    double sum_denominator = 0.0;
    for(size_t i = 0; i < number_bVals; ++i){
        const double b = bvalues[i];
        const double log_S = std::log( vals[i] );
        sum_numerator += (b - b_avg) * (log_S - log_S_avg); 
        sum_denominator += std::pow((b-b_avg), 2.0);
    }

    const double ADC = - sum_numerator / sum_denominator;
    return ADC;
}

std::array<double, 3> GetBiExpf(const std::vector<float> &bvalues, const std::vector<float> &vals, int numIterations){
    //This function will use the a segmented approach with Marquardts method of squared residuals minimization to fit the signal to a biexponential 
    const auto nan = std::numeric_limits<double>::quiet_NaN();
    //The biexponential model
    //S(b) = S(0)[f * exp(-b D*) + (1-f) * exp(-b D)]

    //Divide all signals by S(0)
    float signalTemp; 
    std::vector<float> signals;
    const auto number_bVals = static_cast<double>( bvalues.size() );
    int b0_index = 0;
    for(size_t i = 0; i < number_bVals; ++i){
         
        if (bvalues[i] == 0){ //first get the index of b = 0 (I'm unsure if b values are in order already)
            b0_index = i;
            break;
        }         
        
    }
    for(size_t i = 0; i < number_bVals; ++i){
         
        signals.push_back(vals.at(b0_index));           
        
    }

    //First we use a cutoff of b values greater than 200 to have signals of the form S(b) = S(0) * exp(-b D) to obtain the diffusion coefficient

    //make a vector for the high b values and their signals
    

    std::vector<float> bvaluesH;
    std::vector<float> signalsH;
    float bTemp;
    
    for(size_t i = 0; i < number_bVals; ++i){
        if (bvalues[i] > 200){
            bTemp = bvalues.at(i);      
            signalTemp = vals.at(i);           
            bvaluesH.push_back(bTemp);
            signalsH.push_back(signalTemp); 
            
            
        }
    }
    
    //Now use least squares regression to obtain D from the high b value signals: 
    double D = GetADCls(bvaluesH, signalsH);
    if (D < 0){
            D = 0;
        }
    //Now we can use this D value and fit f and D* with Marquardts method
    //Cost function is sum (Signal_i - (fexp(-bD*) + (1-f)exp(-bD)) )^2
    float lambda = 50;
    double pseudoD = 10.0 * D;
    float f = 0.5;
    double new_pseudoD;
    float newf;
    double cost;
    double newCost;
    std::vector<double> H;
    std::vector<double> inverse;
    std::vector<double> gradient;
    

//Get the current cost
    cost = 0;
    for(size_t i = 0; i < number_bVals; ++i){
        bTemp = bvalues[i];         
        signalTemp = signals.at(i);
        cost += std::pow( ( (signalTemp) - f*exp(-bTemp * pseudoD) - (1-f)*exp(-bTemp * D)   ), 2.0);
    }  
    
    
    for (int i = 0; i < numIterations; i++){
         
        //Now calculate the Hessian matrix which is in the form of a vector (columns then rows), which also contains the gradient at the end
        H = GetHessianAndGradient(bvalues, signals, f, pseudoD, D);
        //Now I need to calculate the inverse of (H + lamda I)
        H[0] += lambda;
        H[3] += lambda;
        inverse = GetInverse(H);
        
        //Now update parameters 
        newf = f + -inverse[0]*H[4] - inverse[1]*H[5];
        new_pseudoD = pseudoD - inverse[2]*H[4] - inverse[3] * H[5];  

        //if f is less than 0 or greater than 1, rescale back to boundary, and don't let pseudoDD get smaller than D
        if (newf < 0){
            f = 0;
        }else if (f > 1){
            f = 1;
        }
        if (pseudoD < 0){
            pseudoD = 0;
        }
        

        //Now check if we have lowered the cost
        newCost = 0;
        for(size_t i = 0; i < number_bVals; ++i){
            newCost += std::pow( ( signals.at(i) - newf*exp(-bvalues[i] * new_pseudoD) - (1-newf)*exp(-bvalues[i] * D)  ), 2.0);
        }  
        //accept changes if we have have reduced cost, and lower lambda
        if (newCost < cost){
            cost = newCost;
            lambda *= 0.8;
            f = newf;
            pseudoD = new_pseudoD;
            
            
        }else{
            lambda *= 2;
        }


    }
    return { f, D, pseudoD };
}