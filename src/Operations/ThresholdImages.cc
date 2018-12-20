//ThresholdImages.cc - A part of DICOMautomaton 2018. Written by hal clark.

#include <asio.hpp>
#include <algorithm>
#include <experimental/optional>
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



OperationDoc OpArgDocThresholdImages(void){
    OperationDoc out;
    out.name = "ThresholdImages";

    out.desc = 
        "This operation applies thresholds to images.";
        
    out.notes.emplace_back(
        "This routine operates on individual images."
        " When thresholds are specified on a percentile basis, each image is considered separately and therefore"
        " each image may be thresholded with different values."
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
    out.args.back().name = "ImageSelection";
    out.args.back().desc = "Images to operate on. Either 'none', 'last', or 'all'.";
    out.args.back().default_val = "last";
    out.args.back().expected = true;
    out.args.back().examples = { "none", "last", "all" };

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

    const auto regex_is_percent = std::regex(".*[%].*", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto Lower_is_Percent = std::regex_match(LowerStr, regex_is_percent);
    const auto Upper_is_Percent = std::regex_match(UpperStr, regex_is_percent);

    const auto regex_is_tile = std::regex(".*p?e?r?c?e?n?tile.*", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto Lower_is_Ptile = std::regex_match(LowerStr, regex_is_tile);
    const auto Upper_is_Ptile = std::regex_match(UpperStr, regex_is_tile);

    const auto regex_none = std::regex("no?n?e?$", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto regex_last = std::regex("la?s?t?$", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto regex_all  = std::regex("al?l?$",   std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);

    if( !std::regex_match(ImageSelectionStr, regex_none)
    &&  !std::regex_match(ImageSelectionStr, regex_last)
    &&  !std::regex_match(ImageSelectionStr, regex_all) ){
        throw std::invalid_argument("Image selection is not valid. Cannot continue.");
    }

    //Iterate over each requested image_array. Each image is processed independently, so a thread pool is used.
    auto iap_it = DICOM_data.image_data.begin();
    if(false){
    }else if(std::regex_match(ImageSelectionStr, regex_none)){ iap_it = DICOM_data.image_data.end();
    }else if(std::regex_match(ImageSelectionStr, regex_last)){
        if(!DICOM_data.image_data.empty()) iap_it = std::prev(DICOM_data.image_data.end());
    }
    for( ; iap_it != DICOM_data.image_data.end(); ++iap_it){
        asio_thread_pool tp;
        std::mutex saver_printer; // Who gets to save generated contours, print to the console, and iterate the counter.
        long int completed = 0;
        const long int img_count = (*iap_it)->imagecoll.images.size();

        for(auto &animg : (*iap_it)->imagecoll.images){
            if( (animg.rows < 1) || (animg.columns < 1) || (Channel >= animg.channels) ){
                throw std::runtime_error("Image or channel is empty -- cannot contour via thresholds.");
            }
            tp.submit_task([&](void) -> void {
                const auto R = animg.rows;
                const auto C = animg.columns;

                //Determine the bounds in terms of pixel-value thresholds.
                auto cl = Lower; // Will be replaced if percentages/percentiles requested.
                auto cu = Upper; // Will be replaced if percentages/percentiles requested.
          
                {
                    //Percentage-based.
                    if(Lower_is_Percent || Upper_is_Percent){
                        Stats::Running_MinMax<float> rmm;
                        animg.apply_to_pixels([&rmm,Channel](long int, long int, long int chnl, float val) -> void {
                             if(Channel == chnl) rmm.Digest(val);
                             return;
                        });
                        if(Lower_is_Percent) cl = (rmm.Current_Min() + (rmm.Current_Max() - rmm.Current_Min()) * Lower / 100.0);
                        if(Upper_is_Percent) cu = (rmm.Current_Min() + (rmm.Current_Max() - rmm.Current_Min()) * Upper / 100.0);
                    }

                    //Percentile-based.
                    if(Lower_is_Ptile || Upper_is_Ptile){
                        std::vector<float> pixel_vals;
                        pixel_vals.reserve(animg.rows * animg.columns * animg.channels);
                        animg.apply_to_pixels([&pixel_vals,Channel](long int, long int, long int chnl, float val) -> void {
                             if(Channel == chnl) pixel_vals.push_back(val);
                             return;
                        });
                        if(Lower_is_Ptile) cl = Stats::Percentile(pixel_vals, Lower / 100.0);
                        if(Upper_is_Ptile) cu = Stats::Percentile(pixel_vals, Upper / 100.0);
                    }
                }
FUNCINFO("cl = " << cl);
FUNCINFO("cu = " << cu);

                //Construct pixel 'oracle' closures using the user-specified threshold criteria. 
                // These functions identify whether pixels are within the threshold values.
                auto lower_pixel_oracle = [cu,cl](float p) -> bool {
                    return (cl <= p);
                };
                auto upper_pixel_oracle = [cu,cl](float p) -> bool {
                    return (p <= cu);
                };

                //Iterate over each pixel, asking the oracle to identify each.
                Stats::Running_MinMax<float> minmax_pixel;
                for(auto r = 0; r < R; ++r){
                    for(auto c = 0; c < C; ++c){
                        const auto v = animg.value(r, c, Channel);

                        if(!lower_pixel_oracle(v)){
                            animg.reference(r, c, Channel) = Low;
                        }
                        if(!upper_pixel_oracle(v)){
                            animg.reference(r, c, Channel) = High;
                        }
                        minmax_pixel.Digest( animg.value(r, c, Channel) );
                    }
                }

                UpdateImageDescription( std::ref(animg), "Thresholded" );
                UpdateImageWindowCentreWidth( std::ref(animg), minmax_pixel );

                //Save the contours and print some information to screen.
                {
                    std::lock_guard<std::mutex> lock(saver_printer);
                    ++completed;
                    FUNCINFO("Completed " << completed << " of " << img_count
                          << " --> " << static_cast<int>(1000.0*(completed)/img_count)/10.0 << "\% done");
                }
            }); // thread pool task closure.
        }
    }

    return DICOM_data;
}
