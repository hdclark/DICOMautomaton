//DCEMRI_Nonparametric_CE.cc - A part of DICOMautomaton 2015, 2016. Written by hal clark.

#include <algorithm>
#include <any>
#include <fstream>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>    
#include <vector>

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../YgorImages_Functors/Grouping/Misc_Functors.h"
#include "../YgorImages_Functors/Transform/CT_Perfusion_Signal_Diff.h"
#include "../YgorImages_Functors/Transform/DCEMRI_Signal_Difference_C.h"
#include "DCEMRI_Nonparametric_CE.h"
#include "YgorImages.h"
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.


OperationDoc OpArgDocDCEMRI_Nonparametric_CE(){
    OperationDoc out;
    out.name = "DCEMRI_Nonparametric_CE";

    out.desc = 
        "This operation takes a single DCE-MRI scan ('measurement') and generates a \"poor-mans's\" contrast enhancement"
        " signal. This is accomplished by subtracting the pre-contrast injection images average ('baseline') from later"
        " images (and then possibly/optionally averaging relative to the baseline).";
        
    out.notes.emplace_back(
        "Only a single image volume is required. It is expected to have temporal sampling beyond the contrast injection"
        " timepoint (or some default value -- currently around ~30s). The resulting images retain the baseline portion, so"
        " you'll need to trim yourself if needed."
    );
        
    out.notes.emplace_back(
        "Be aware that this method of deriving contrast enhancement is not valid! It ignores nuances due to differing T1"
        " or T2 values due to the presence of contrast agent. It should only be used for exploratory purposes or cases"
        " where the distinction with reality is irrelevant."
    );

    return out;
}

Drover DCEMRI_Nonparametric_CE(Drover DICOM_data,
                               const OperationArgPkg& /*OptArgs*/,
                               const std::map<std::string, std::string>& InvocationMetadata,
                               const std::string& /*FilenameLex*/){

    //Verify there is data to work on.
    if( DICOM_data.image_data.empty() ){
        throw std::invalid_argument("No data to work on. Unable to estimate contrast enhancement.");
    }

    std::map<std::string,std::string> l_InvocationMetadata(InvocationMetadata);

    //Get handles for each of the original image arrays so we can easily refer to them later.
    std::vector<std::shared_ptr<Image_Array>> orig_img_arrays;
    for(auto & img_arr : DICOM_data.image_data) orig_img_arrays.push_back(img_arr);

    //Complain if there are several images, but continue on using only the first volume.
    if(orig_img_arrays.size() != 1){
        FUNCWARN("Several image volumes detected."
                 " Proceeding to generate non-parametric DCE contrast enhancement with the first only.");
    }
    orig_img_arrays.resize(1); // NOTE: Later assumptions are made about image ordering!

    //Figure out how much time elapsed before contrast injection began.
    double ContrastInjectionLeadTime = 35.0; //Seconds. 
    if(l_InvocationMetadata.count("ContrastInjectionLeadTime") == 0){
        FUNCWARN("Unable to locate 'ContrastInjectionLeadTime' invocation metadata key. Assuming the default lead time " 
                 << ContrastInjectionLeadTime << "s is appropriate");
    }else{
        ContrastInjectionLeadTime = std::stod( l_InvocationMetadata["ContrastInjectionLeadTime"] );
        if(ContrastInjectionLeadTime < 0.0) throw std::runtime_error("Non-sensical 'ContrastInjectionLeadTime' found.");
        FUNCINFO("Found 'ContrastInjectionLeadTime' invocation metadata key. Using value " << ContrastInjectionLeadTime << "s"); 
    }

    //Spatially blur the images. This may help if the measurements are noisy.
    //
    // NOTE: Blurring the baseline but not the rest of the data can result in odd results. It's best to uniformly blur
    //       all images before trying to derive contrast enhancement (i.e., low-pass filtering).
    if(false){
        for(const auto& img_ptr : orig_img_arrays){
            if(!img_ptr->imagecoll.Gaussian_Pixel_Blur({ }, 1.5)){
                FUNCERR("Unable to blur temporally averaged images");
            }
        }
    }
    
    //Compute a temporally-averaged baseline.
    auto PurgeAboveNSeconds = std::bind(PurgeAboveTemporalThreshold, std::placeholders::_1, ContrastInjectionLeadTime);
    std::vector<std::shared_ptr<Image_Array>> baseline_img_arrays;
    for(auto & img_arr : orig_img_arrays){
        DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>( *img_arr ) );
        baseline_img_arrays.emplace_back( DICOM_data.image_data.back() );

        baseline_img_arrays.back()->imagecoll.Prune_Images_Satisfying(PurgeAboveNSeconds);

        if(!baseline_img_arrays.back()->imagecoll.Condense_Average_Images(GroupSpatiallyOverlappingImages)){
            throw std::runtime_error("Cannot temporally average data set. Is it able to be averaged?");
        }
    }

    //Subtract the baseline from the measurement images.
    bool Normalize_to_Baseline = true; //Whether to normalize
    auto SigDiffC = (Normalize_to_Baseline) ? DCEMRISigDiffC : CTPerfusionSigDiffC;
    {
        auto baseline_img_arr_it = baseline_img_arrays.begin();
        for(auto & img_arr : orig_img_arrays){
            auto baseline_img_ptr = *(baseline_img_arr_it++);
            if(!img_arr->imagecoll.Transform_Images( SigDiffC,
                                                     { baseline_img_ptr->imagecoll },
                                                     { } )){
                throw std::runtime_error("Unable to subtract baseline from measurement images.");
            }
            /*
            if(Normalize_to_Baseline)
                if(!img_arr->imagecoll.Transform_Images( DCEMRISigDiffC,
                                                         { baseline_img_ptr->imagecoll },
                                                         { } )){
                    throw std::runtime_error("Unable to subtract baseline from measurement images.");
                }
            }else{
                if(!img_arr->imagecoll.Transform_Images( CTPerfusionSigDiffC,
                                                         { baseline_img_ptr->imagecoll },
                                                         { } )){
                    throw std::runtime_error("Unable to subtract baseline from measurement images.");
                }
            }
            */
        }
    }

    //Erase the baseline images.
    for(auto & img_arr : baseline_img_arrays){
        //"Erase-remove" idiom for vectors.
        auto ddid_beg = DICOM_data.image_data.begin();
        auto ddid_end = DICOM_data.image_data.end();
        DICOM_data.image_data.erase(std::remove(ddid_beg, ddid_end, img_arr), ddid_end);
    }

    return DICOM_data;
}
