//ThresholdImages.cc - A part of DICOMautomaton 2018. Written by hal clark.

#include <asio.hpp>
#include <algorithm>
#include <optional>
#include <fstream>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <regex>
#include <set> 
#include <stdexcept>
#include <string>    
#include <utility>            //Needed for std::pair.
#include <vector>

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../Thread_Pool.h"
#include "../YgorImages_Functors/ConvenienceRoutines.h"

#include "ThresholdImages.h"
#include "Explicator.h"       //Needed for Explicator class.
#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorStats.h"        //Needed for Stats:: namespace.
#include "YgorString.h"       //Needed for GetFirstRegex(...)



OperationDoc OpArgDocThresholdImages(){
    OperationDoc out;
    out.name = "ThresholdImages";

    out.desc = 
        "This operation applies thresholds to images. Both upper and lower thresholds can be specified.";
        
    out.notes.emplace_back(
        "This routine operates on individual images."
        " When thresholds are specified on a percentile basis, each image is considered separately and therefore"
        " each image may be thresholded with different values."
    );
    out.notes.emplace_back(
        "Both thresholds are inclusive. To binarize an image, use the same threshold for both upper and lower"
        " threshold parameters. Voxels that fall on the threshold will currently be treated as if they"
        " exclusively satisfy the upper threshold, but this behaviour is not guaranteed."
    );
        

    out.args.emplace_back();
    out.args.back().name = "Lower";
    out.args.back().desc = "The lower bound (inclusive). Pixels with values < this number are replaced with the"
                      " 'low' value."
                      " If this number is followed by a '%', the bound will be scaled between the min and max"
                      " pixel values [0-100%]. If this number is followed by 'tile', the bound will be replaced"
                      " with the corresponding percentile [0-100tile]."
                      " Note that upper and lower bounds can be specified separately (e.g., lower bound is a"
                      " percentage, but upper bound is a percentile).";
    out.args.back().default_val = "-inf";
    out.args.back().expected = true;
    out.args.back().examples = { "0.0", "-1E-99", "1.23", "0.2%", "23tile", "23.123 tile" };


    out.args.emplace_back();
    out.args.back().name = "Low";
    out.args.back().desc = "The value a pixel will take when below the lower threshold.";
    out.args.back().default_val = "-inf";
    out.args.back().expected = true;
    out.args.back().examples = { "0.0", "-1000.0", "-inf", "nan" };


    out.args.emplace_back();
    out.args.back().name = "Upper";
    out.args.back().desc = "The upper bound (inclusive). Pixels with values > this number are replaced with the"
                      " 'high' value."
                      " If this number is followed by a '%', the bound will be scaled between the min and max"
                      " pixel values [0-100%]. If this number is followed by 'tile', the bound will be replaced"
                      " with the corresponding percentile [0-100tile]."
                      " Note that upper and lower bounds can be specified separately (e.g., lower bound is a"
                      " percentage, but upper bound is a percentile).";
    out.args.back().default_val = "inf";
    out.args.back().expected = true;
    out.args.back().examples = { "1.0", "1E-99", "2.34", "98.12%", "94tile", "94.123 tile" };


    out.args.emplace_back();
    out.args.back().name = "High";
    out.args.back().desc = "The value a pixel will take when above the upper threshold.";
    out.args.back().default_val = "inf";
    out.args.back().expected = true;
    out.args.back().examples = { "0.0", "1000.0", "inf", "nan" };


    out.args.emplace_back();
    out.args.back().name = "Channel";
    out.args.back().desc = "The image channel to use. Zero-based.";
    out.args.back().default_val = "0";
    out.args.back().expected = true;
    out.args.back().examples = { "0", "1", "2" };

    
    out.args.emplace_back();
    out.args.back() = IAWhitelistOpArgDoc();
    out.args.back().name = "ImageSelection";
    out.args.back().default_val = "last";

    return out;
}



Drover ThresholdImages(Drover DICOM_data, OperationArgPkg OptArgs, std::map<std::string,std::string> /*InvocationMetadata*/, std::string FilenameLex){

    Explicator X(FilenameLex);

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto LowerStr = OptArgs.getValueStr("Lower").value();
    const auto LowStr = OptArgs.getValueStr("Low").value();

    const auto UpperStr = OptArgs.getValueStr("Upper").value();
    const auto HighStr = OptArgs.getValueStr("High").value();

    const auto ChannelStr = OptArgs.getValueStr("Channel").value();

    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();

    //-----------------------------------------------------------------------------------------------------------------
    const auto Lower = std::stod( LowerStr );
    const auto Low   = std::stod( LowStr );
    const auto Upper = std::stod( UpperStr );
    const auto High  = std::stod( HighStr );
    const auto Channel = std::stol( ChannelStr );

    const auto regex_is_percent = Compile_Regex(".*[%].*");
    const auto Lower_is_Percent = std::regex_match(LowerStr, regex_is_percent);
    const auto Upper_is_Percent = std::regex_match(UpperStr, regex_is_percent);

    const auto regex_is_tile = Compile_Regex(".*p?e?r?c?e?n?tile.*");
    const auto Lower_is_Ptile = std::regex_match(LowerStr, regex_is_tile);
    const auto Upper_is_Ptile = std::regex_match(UpperStr, regex_is_tile);

    //Iterate over each requested image_array. Each image is processed independently, so a thread pool is used.
    auto IAs_all = All_IAs( DICOM_data );
    auto IAs = Whitelist( IAs_all, ImageSelectionStr );
    for(auto & iap_it : IAs){
        asio_thread_pool tp;
        std::mutex saver_printer; // Who gets to save generated contours, print to the console, and iterate the counter.
        long int completed = 0;
        const long int img_count = (*iap_it)->imagecoll.images.size();

        for(auto &animg : (*iap_it)->imagecoll.images){
            if( (animg.rows < 1) || (animg.columns < 1) || (Channel >= animg.channels) ){
                throw std::runtime_error("Image or channel is empty -- cannot contour via thresholds.");
            }
            std::reference_wrapper<planar_image<float,double>> img_refw( std::ref(animg) );

            tp.submit_task([&,img_refw]() -> void {
                const auto R = img_refw.get().rows;
                const auto C = img_refw.get().columns;

                //Determine the bounds in terms of pixel-value thresholds.
                auto cl = Lower; // Will be replaced if percentages/percentiles requested.
                auto cu = Upper; // Will be replaced if percentages/percentiles requested.
          
                {
                    //Percentage-based.
                    if(Lower_is_Percent || Upper_is_Percent){
                        Stats::Running_MinMax<float> rmm;
                        img_refw.get().apply_to_pixels([&rmm,Channel](long int, long int, long int chnl, float val) -> void {
                             if(Channel == chnl) rmm.Digest(val);
                             return;
                        });
                        if(Lower_is_Percent) cl = (rmm.Current_Min() + (rmm.Current_Max() - rmm.Current_Min()) * Lower / 100.0);
                        if(Upper_is_Percent) cu = (rmm.Current_Min() + (rmm.Current_Max() - rmm.Current_Min()) * Upper / 100.0);
                    }

                    //Percentile-based.
                    if(Lower_is_Ptile || Upper_is_Ptile){
                        std::vector<float> pixel_vals;
                        pixel_vals.reserve(img_refw.get().rows * img_refw.get().columns * img_refw.get().channels);
                        img_refw.get().apply_to_pixels([&pixel_vals,Channel](long int, long int, long int chnl, float val) -> void {
                             if(Channel == chnl) pixel_vals.push_back(val);
                             return;
                        });
                        if(Lower_is_Ptile) cl = Stats::Percentile(pixel_vals, Lower / 100.0);
                        if(Upper_is_Ptile) cu = Stats::Percentile(pixel_vals, Upper / 100.0);
                    }
                }

                //Construct pixel 'oracle' closures using the user-specified threshold criteria. 
                // These functions identify whether pixels are within the threshold values.
                auto lower_pixel_oracle = [cl](float p) -> bool {
                    return (cl < p);
                };
                auto upper_pixel_oracle = [cu](float p) -> bool {
                    return (p < cu);
                };

                //Iterate over each pixel, asking the oracle to identify each.
                Stats::Running_MinMax<float> minmax_pixel;
                for(auto r = 0; r < R; ++r){
                    for(auto c = 0; c < C; ++c){
                        const auto v = img_refw.get().value(r, c, Channel);

                        if(!lower_pixel_oracle(v)){
                            img_refw.get().reference(r, c, Channel) = Low;
                        }
                        if(!upper_pixel_oracle(v)){
                            img_refw.get().reference(r, c, Channel) = High;
                        }
                        minmax_pixel.Digest( img_refw.get().value(r, c, Channel) );
                    }
                }

                UpdateImageDescription( img_refw, "Thresholded" );
                UpdateImageWindowCentreWidth( img_refw, minmax_pixel );

                //Report operation progress.
                {
                    std::lock_guard<std::mutex> lock(saver_printer);
                    ++completed;
                    FUNCINFO("Completed " << completed << " of " << img_count
                          << " --> " << static_cast<int>(1000.0*(completed)/img_count)/10.0 << "% done");
                }
            }); // thread pool task closure.
        }
    }

    return DICOM_data;
}
