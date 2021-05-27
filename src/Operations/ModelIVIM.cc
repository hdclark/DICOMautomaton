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
                           " Currently, 'adc-simple' and 'adc-ls' are available."
                           " The 'adc-simple' model does not take into account perfusion, only free diffusion is"
                           " modeled. It only uses the minimum and maximum b-value images and analytically estimates"
                           " ADC."
                           " The 'adc-ls' model, like 'adc-simple', only models free diffusion."
                           " It fits a linear least-squares model that uses all available b-value images.";
    out.args.back().default_val = "adc-simple";
    out.args.back().expected = true;
    out.args.back().examples = { "adc-simple",
                                 "adc-ls" };
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

    //First get ADC from the formula -ADC = sum [ (b_i - b_avg) * (ln(S_i) - ln(S_avg)) ] / sum( b_i - b_avg )^2
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


