//ModelIVIM.cc - A part of DICOMautomaton 2022. Written by Caleb Sample and Hal Clark.

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

#include "../Structs.h"
#include "../Metadata.h"
#include "../Regex_Selectors.h"
#include "../BED_Conversion.h"
#include "../YgorImages_Functors/Compute/Joint_Pixel_Sampler.h"

#include "ModelIVIM.h"

using Eigen::MatrixXd;


double GetADCls(const std::vector<float> &bvalues, const std::vector<float> &vals);

double GetKurtosisModel(float b, const std::vector<double> &params);
double GetKurtosisTheta(const std::vector<float> &bvalues, const std::vector<float> &signals, const std::vector<double> &params, const std::vector<double> &priors);
// void GetKurtosisGradient(MatrixXd &grad, const std::vector<float>bvalues, const std::vector<float> signals, std::vector<double> params, std::vector<double> priors);
// void GetHessian(MatrixXd &hessian, const std::vector<float>bvalues, const std::vector<float> signals, std::vector<double> params, std::vector<double> priors);
std::vector<double> GetKurtosisPriors(const std::vector<double> &params);
std::array<double, 3> GetKurtosisParams(const std::vector<float> &bvalues, const std::vector<float> &vals, int numIterations);

std::array<double, 3> GetBiExp(const std::vector<float> &bvalues, const std::vector<float> &vals, int numIterations);
std::vector<double> GetHessianAndGradient(const std::vector<float> &bvalues, const std::vector<float> &vals, float f, double pseudoD, const double D);
std::vector<double> GetInverse(const std::vector<double> &matrix);



OperationDoc OpArgDocModelIVIM(){
    OperationDoc out;
    out.name = "ModelIVIM";

    out.tags.emplace_back("category: image processing");
    out.tags.emplace_back("category: modeling");
    out.tags.emplace_back("category: perfusion");
    out.tags.emplace_back("category: diffusion");

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
    out.args.back() = CCWhitelistOpArgDoc();
    out.args.back().name = "ROISelection";
    out.args.back().default_val = "all";

    out.args.emplace_back();
    out.args.back().name = "Model";
    out.args.back().desc = "The model that will be fitted."
#ifdef DCMA_USE_EIGEN
                           " Currently, 'adc-simple' , 'adc-ls' , 'auc-simple', 'biexp', and 'kurtosis' are available."
#else
                           " Currently, 'adc-simple' , 'adc-ls' , 'auc-simple', and 'biexp' are available."
#endif //DCMA_USE_EIGEN
                           "\n\n"
                           "The 'adc-simple' is a simplistic diffusion model that ignores perfusion."
                           " It models only free diffusion using only the minimum and maximum b-values."
                           " An analytical estimate of ADC (i.e., the apparent diffusion coefficient) is generated."
                           "\n\n"
                           "The 'adc-ls' model, like 'adc-simple', is a simplistic model that ignores perfusion."
                           " It fits a linearized least-squares model that uses all available b-value images."
                           " Like 'adc-simple', this model only estimates ADC."
                           "\n\n"
                           "The 'auc-simple' model is a simplistic, nonparametric model that integrates the area under"
                           " the intensity-vs-b-value curve. Note that no model fitting is performed; the voxel"
                           " intensity-b-value product is summed directly. No extrapolation is performed."
                           "\n\n"
                           "The 'biexp' model uses a segmented fitting approach along with Marquardt's method to fit a"
                           " biexponential model, which estimates the pseudodiffusion fraction, the diffusion"
                           " coefficient, and the pseudodiffusion coefficient for each voxel."
#ifdef DCMA_USE_EIGEN
                           "\n\n"
                           "The 'kurtosis' model returns three parameters corresponding to a biexponential diffusion"
                           " model with a kurtosis adjustment and a noise floor parameter added in quadrature"
                           " (pseudodiffusion fraction, diffusion, and pseudodiffusion coefficient for each voxel)."
#endif //DCMA_USE_EIGEN
                           "";
                           
    out.args.back().default_val = "adc-simple";
    out.args.back().expected = true;
    out.args.back().examples = { "adc-simple",
                                 "adc-ls",
                                 "auc-simple",
                                 "biexp",
#ifdef DCMA_USE_EIGEN
                                 "kurtosis",
#endif //DCMA_USE_EIGEN
                               };
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



bool ModelIVIM(Drover &DICOM_data,
                 const OperationArgPkg& OptArgs,
                 std::map<std::string, std::string>& /*InvocationMetadata*/,
                 const std::string& /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();
    const auto ReferenceImageSelectionStr = OptArgs.getValueStr("ReferenceImageSelection").value();

    const auto NormalizedROILabelRegex = OptArgs.getValueStr("NormalizedROILabelRegex").value();
    const auto ROILabelRegex = OptArgs.getValueStr("ROILabelRegex").value();
    const auto ROISelection = OptArgs.getValueStr("ROISelection").value();

    const auto ModelStr = OptArgs.getValueStr("Model").value();
    const auto Channel = std::stol( OptArgs.getValueStr("Channel").value() );
    const auto TestImgLowerThreshold = std::stod( OptArgs.getValueStr("TestImgLowerThreshold").value() );
    const auto TestImgUpperThreshold = std::stod( OptArgs.getValueStr("TestImgUpperThreshold").value() );

    //-----------------------------------------------------------------------------------------------------------------
    const auto model_adc_simple = Compile_Regex("^adc?[-_]?si?m?p?l?e?$");
    const auto model_adc_ls = Compile_Regex("^adc?[-_]?ls?$");
    const auto model_biexp = Compile_Regex("^bi[-_]?e?x?p?");
    const auto model_kurtosis = Compile_Regex("^ku?r?t?o?s?i?s?");
    const auto model_auc = Compile_Regex("^auc?[-_]?si?m?p?l?e?$");

    //-----------------------------------------------------------------------------------------------------------------

    auto RIAs_all = All_IAs( DICOM_data );
    auto RIAs = Whitelist( RIAs_all, ReferenceImageSelectionStr );
    if(RIAs.size() < 2){
        throw std::invalid_argument("At least two b-value images are required to model ADC.");
    }
    std::list<std::reference_wrapper<planar_image_collection<float, double>>> RIARL;
    std::list<planar_image_collection<float, double>::images_list_it_t> ref_img_iters;
    for(auto & RIA : RIAs){
        RIARL.emplace_back( std::ref( (*RIA)->imagecoll ) );
        for(auto it = (*RIA)->imagecoll.images.begin(); it != (*RIA)->imagecoll.images.end(); ++it){
            ref_img_iters.emplace_back(it);
        }
    }

    // Identify the b-value of each reference image, which is needed for later analysis.
    std::vector<float> bvalues;
    for(const auto &RIA_refw : RIARL){
        // Exact key lookup.
        //const auto vals = RIA_refw.get().get_distinct_values_for_key("DiffusionBValue");

        // Fuzzy (regex) lookup.
        // Search for any metadata keys that match, and gather all the distinct values for all matching keys.
        const auto key_regex = Compile_Regex(".*DiffusionBValue$|.*CSAImage.*[bB].[vV]alue$");
        std::list<std::string> vals;
        for(const auto &animg : RIA_refw.get().images){
            for(const auto &kv : animg.metadata){
                if(std::regex_match(kv.first, key_regex)){ // Check if key matches regex.
                    vals.emplace_back(kv.second);
                }
            }
        }

        // Only keep distinct values.
        vals.sort();
        vals.unique();

        if(vals.size() != 1){
            throw std::invalid_argument("Reference image does not contain a single distinct b-value.");
        }
        bvalues.emplace_back( std::stod(vals.front()) );
    }

    // Determine the sorted order of the b-values. We do this *without* sorting since the order of the 
    // reference images isn't changed, so the b-values and voxel intensities would be out-of-order if we
    // only sorted the b-values.
    std::vector<size_t> bvalues_order(std::size(bvalues));
    std::iota(bvalues_order.begin(), bvalues_order.end(), 0UL);
    std::sort(bvalues_order.begin(), bvalues_order.end(),
              [&](size_t i, size_t j){
                  return (bvalues[i] < bvalues[j]); 
              } );

    // Extract common metadata from reference images.
    auto cm = planar_image_collection<float, double>().get_common_metadata(ref_img_iters);
    cm = coalesce_metadata_for_basic_mr_image(cm);

    //Stuff references to all contours into a list. Remember that you can still address specific contours through
    // the original holding containers (which are not modified here).
    auto cc_all = All_CCs( DICOM_data );
    auto cc_ROIs = Whitelist( cc_all, ROILabelRegex, NormalizedROILabelRegex, ROISelection );
    if(cc_ROIs.empty()){
        throw std::invalid_argument("No contours selected. Cannot continue.");
    }

    // Sort the RIARL using bvalues, to simplify access later.
    // ...
    const auto bvalue_min_i = std::distance( std::begin(bvalues), std::min_element( std::begin(bvalues), std::end(bvalues) ) );
    const auto bvalue_max_i = std::distance( std::begin(bvalues), std::max_element( std::begin(bvalues), std::end(bvalues) ) );
    const auto nan = std::numeric_limits<double>::quiet_NaN();

    YLOGINFO("Detected minimum bvalue is b(" << bvalue_min_i << ") = " << bvalues.at( bvalue_min_i ));
    YLOGINFO("Detected maximum bvalue is b(" << bvalue_max_i << ") = " << bvalues.at( bvalue_max_i ));
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
                    throw std::runtime_error("Unmatched voxel and b-value vectors. Refusing to continue.");
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
                    throw std::runtime_error("Unmatched voxel and b-value vectors. Refusing to continue.");
                }

                const auto adc = GetADCls(bvalues, vals);
                if(!std::isfinite( adc )) throw std::runtime_error("adc is not finite");
                return adc;
                
            };

#ifdef DCMA_USE_EIGEN
        }else if(std::regex_match(ModelStr, model_kurtosis)){
            // Add channels to each image for each model parameter.
            auto imgarr_ptr = &((*iap_it)->imagecoll);
            for(auto &img : imgarr_ptr->images){
                img.init_buffer( img.rows, img.columns, 3 ); // for f, D, pseudoD.
            }
            const int64_t chan_f  = 0;
            const int64_t chan_D  = 1;
            const int64_t chan_pD = 2;

            ud.description = "f, D, pseudo-D (Kurtosis Model fit)";
            ud.f_reduce = [bvalues,
                           bvalue_min_i,
                           bvalue_max_i,
                           imgarr_ptr,
                           chan_D,
                           chan_pD ]( std::vector<float> &vals, 
                                      vec3<double> pos ) -> float {
                vals.erase(vals.begin()); // Remove the base image's value.
                if(vals.size() != bvalues.size()){
                    throw std::runtime_error("Unmatched voxel and b-value vectors. Refusing to continue.");
                }
                int numIterations = 600;
                
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
#endif //DCMA_USE_EIGEN

        }else if(std::regex_match(ModelStr, model_biexp)){
            // Add channels to each image for each model parameter.
            auto imgarr_ptr = &((*iap_it)->imagecoll);
            for(auto &img : imgarr_ptr->images){
                img.init_buffer( img.rows, img.columns, 3 ); // for f, D, pseudoD.
            }
            const int64_t chan_f  = 0;
            const int64_t chan_D  = 1;
            const int64_t chan_pD = 2;


            ud.description = "f, D, pseudo-D (Bi-exponential segmented fit)";
            ud.f_reduce = [bvalues,
                           bvalue_min_i,
                           bvalue_max_i,
                           imgarr_ptr,
                           chan_D,
                           chan_pD ]( std::vector<float> &vals, 
                                      vec3<double> pos ) -> float {
                vals.erase(vals.begin()); // Remove the base image's value.
                if(vals.size() != bvalues.size()){
                    throw std::runtime_error("Unmatched voxel and b-value vectors. Refusing to continue.");
                }
                if(vals.empty()){
                    throw std::runtime_error("No overlapping images detected. Unable to continue.");
                }
                int numIterations = 1000;
                const auto [f, D, pseudoD] = GetBiExp(bvalues, vals, numIterations);
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

        }else if(std::regex_match(ModelStr, model_auc)){
            ud.description = "AUC";
            ud.f_reduce = [bvalues, bvalues_order, bvalue_min_i, bvalue_max_i]( std::vector<float> &vals, 
                                                                                vec3<double> ) -> float {
                vals.erase(vals.begin()); // Remove the base image's value.
                if(vals.size() != bvalues.size()){
                    throw std::runtime_error("Unmatched voxel and b-value vectors. Refusing to continue.");
                }
                if(vals.empty()){
                    throw std::runtime_error("No overlapping images detected. Unable to continue.");
                }

                double auc = 0.0;

                // Note: we traverse b-values from lowest to highest, processing two adjacent values at a time.
                const auto N = bvalues_order.size();
                for(size_t k = 0UL; (k+1UL) < N; ++k){
                    const auto i = bvalues_order.at(k);
                    const auto j = bvalues_order.at(k+1UL);

                    const auto b_i = bvalues.at(i);
                    const auto b_j = bvalues.at(j);

                    const auto I_i = vals.at(i);
                    const auto I_j = vals.at(j);

                    // Trapezoidal summation.
                    const auto dauc = (b_j - b_i) * (I_i + I_j) * 0.5;
                    auc += dauc;
                }

                if(!std::isfinite( auc )) throw std::runtime_error("auc is not finite");
                return auc;
            };

        }else{
            throw std::invalid_argument("Model not understood. Cannot continue.");
        }

        if(!(*iap_it)->imagecoll.Compute_Images( ComputeJointPixelSampler, 
                                                 RIARL, cc_ROIs, &ud )){
            throw std::runtime_error("Unable to analyze images.");
        }

        // Assign common metadata.
        for(auto & img : (*iap_it)->imagecoll.images){
            auto l_cm = cm;
            l_cm["Description"] = img.metadata["Description"];

            img.metadata.clear();
            inject_metadata( img.metadata, std::move(l_cm) );
            cm = coalesce_metadata_for_basic_mr_image(cm, meta_evolve::iterate);
        }
    }

    return true;
}



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
