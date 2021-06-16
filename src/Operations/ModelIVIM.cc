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

#include "ModelIVIM.h"


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
                           " Currently, 'adc-simple' , 'adc-ls' and 'f_biexp' are available."
                           " The 'adc-simple' model does not take into account perfusion, only free diffusion is"
                           " modeled. It only uses the minimum and maximum b-value images and analytically estimates"
                           " ADC."
                           " The 'adc-ls' model, like 'adc-simple', only models free diffusion."
                           " It fits a linear least-squares model that uses all available b-value images."
                           "The 'f_biexp' model uses a segmented fitting approach along with Marquardts method to obtain the pseudodiffusion fraction"
                           "It uses a biexponential model";
    out.args.back().default_val = "adc-simple";
    out.args.back().expected = true;
    out.args.back().examples = { "adc-simple",
                                 "adc-ls" ,
                                 "f_biexp"};
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
    const auto model_f_biexp = Compile_Regex("^fr?a?c?t?i?o?n?[-_]?bie?x?p?");

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

        }else if(std::regex_match(ModelStr, model_f_biexp)){
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

        }else{
            throw std::invalid_argument("Model not understood. Cannot continue.");
        }

        if(!(*iap_it)->imagecoll.Compute_Images( ComputeJointPixelSampler, 
                                                 RIARL, cc_ROIs, &ud )){
            throw std::runtime_error("Unable to analyze images.");
        }
    }

    return DICOM_data;
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

    //The biexponential model
    //S(b) = S(0)[f * exp(-b D*) + (1-f) * exp(-b D)]

    //First we use a cutoff of b values greater than 200 to have signals of the form S(b) = S(0) * exp(-b D) to obtain the diffusion coefficient

    //make a vector for the high b values and their signals
    const auto nan = std::numeric_limits<double>::quiet_NaN();

    std::vector<float> bvaluesH;
    std::vector<float> signalsH;
    float bTemp;
    float signalTemp; //In loops have to take values from vector and make a variable first or get an error

    const auto number_bVals = static_cast<double>( bvalues.size() );
    for(size_t i = 0; i < number_bVals; ++i){
        if (bvalues[i] > 200){
            bTemp = bvalues.at(i);      
            signalTemp = vals.at(i);           
            bvaluesH.push_back(bTemp);
            signalsH.push_back(signalTemp); 
            
            
        }
    }
    
    

    //Now use least squares regression to obtain D from the high b value signals: 
    const double D = GetADCls(bvaluesH, signalsH);
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
        signalTemp = vals.at(i);
        cost += std::pow( ( (signalTemp) - f*exp(-bTemp * pseudoD) - (1-f)*exp(-bTemp * D)   ), 2.0);
    }  
    
    
    for (int i = 0; i < numIterations; i++){
         
        //Now calculate the Hessian matrix which is in the form of a vector (columns then rows), which also contains the gradient at the end
        H = GetHessianAndGradient(bvalues, vals, f, pseudoD, D);
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
        if (pseudoD < D){
            pseudoD = D;
        }

        //Now check if we have lowered the cost
        newCost = 0;
        for(size_t i = 0; i < number_bVals; ++i){
            newCost += std::pow( ( vals.at(i) - newf*exp(-bvalues[i] * new_pseudoD) - (1-newf)*exp(-bvalues[i] * D)  ), 2.0);
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



