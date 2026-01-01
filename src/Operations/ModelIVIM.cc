//ModelIVIM.cc - A part of DICOMautomaton 2025. Written by Caleb Sample, Hal Clark, and Arash Javanmardi.

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
#include "../MRI_IVIM.h"
using namespace MRI_IVIM;

#include "ModelIVIM.h"


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
    YLOGDEBUG("Selected " << RIAs.size() << " reference image arrays");
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
    YLOGDEBUG("Selected " << IAs.size() << " working image arrays");
    for(auto & iap_it : IAs){
        for(auto &img : (*iap_it)->imagecoll.images){
//            img.fill_pixels( std::numeric_limits<float>::quiet_NaN() );
            img.fill_pixels( 0.0f );
        }

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
                //img.fill_pixels( std::numeric_limits<float>::quiet_NaN() );
                img.fill_pixels( 0.0f );
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
                img.init_buffer( img.rows, img.columns, 5 ); // for f, D, pseudoD, num_iters, fitted tolerance.
                //img.fill_pixels( std::numeric_limits<float>::quiet_NaN() );
                img.fill_pixels( 0.0f );
            }
            const int64_t chan_f  = 0;
            const int64_t chan_D  = 1;
            const int64_t chan_pD = 2;
            const int64_t chan_is = 3;
            const int64_t chan_t  = 4;


            ud.description = "f, D, pseudo-D (Bi-exponential segmented fit)";
            ud.f_reduce = [bvalues,
                           bvalue_min_i,
                           bvalue_max_i,
                           imgarr_ptr,
                           chan_D,
                           chan_pD,
                           chan_is,
                           chan_t  ]( std::vector<float> &vals, 
                                      vec3<double> pos ) -> float {
                vals.erase(vals.begin()); // Remove the base image's value.
                if(vals.size() != bvalues.size()){
                    throw std::runtime_error("Unmatched voxel and b-value vectors. Refusing to continue.");
                }
                if(vals.empty()){
                    throw std::runtime_error("No overlapping images detected. Unable to continue.");
                }
                int numIterations = 1000;
                const auto [f, D, pseudoD, num_iters, tol] = GetBiExp(bvalues, vals, numIterations);
                if(!std::isfinite( f )) throw std::runtime_error("f is not finite");

                // The image/voxel iterator interface isn't capable of handling multiple-channel values,
                // so we have to explicitly lookup the position and insert it directly.
                const auto img_it_l = imgarr_ptr->get_images_which_encompass_point(pos);
                if(img_it_l.size() != 1){
                    throw std::logic_error("Unable to find singular overlapping image.");
                }
                const auto index_D  = img_it_l.front()->index(pos, chan_D);
                const auto index_pD = img_it_l.front()->index(pos, chan_pD);
                const auto index_is = img_it_l.front()->index(pos, chan_is);
                const auto index_t  = img_it_l.front()->index(pos, chan_t);
                if( (index_D < 0)
                ||  (index_pD < 0)
                ||  (index_is < 0)
                ||  (index_t < 0) ){
                    throw std::logic_error("Unable to locate voxel via position");
                }
                //img_it_l.front()->reference(index_D) = 2.22;
                //img_it_l.front()->reference(index_pD) = 3.33;
                //img_it_l.front()->reference(index_is) = 4.44;
                //img_it_l.front()->reference(index_t) = 5.55;
                //return 1.11;
                img_it_l.front()->reference(index_D) = D;
                img_it_l.front()->reference(index_pD) = pseudoD;
                img_it_l.front()->reference(index_is) = num_iters;
                img_it_l.front()->reference(index_t) = tol;
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

