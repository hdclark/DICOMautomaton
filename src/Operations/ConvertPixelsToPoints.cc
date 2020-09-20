//ConvertPixelsToPoints.cc - A part of DICOMautomaton 2019. Written by hal clark.

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
#include "ConvertPixelsToPoints.h"
#include "Explicator.h"       //Needed for Explicator class.
#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorStats.h"        //Needed for Stats:: namespace.
#include "YgorString.h"       //Needed for GetFirstRegex(...)



OperationDoc OpArgDocConvertPixelsToPoints(){
    OperationDoc out;
    out.name = "ConvertPixelsToPoints";

    out.desc = 
        "This operation extracts pixels from the selected images and converts them into a point cloud."
        " Images are not modified.";
        
    out.notes.emplace_back(
        "Existing point clouds are ignored and unaltered."
    );
        

    out.args.emplace_back();
    out.args.back().name = "Label";
    out.args.back().desc = "A label to attach to the point cloud.";
    out.args.back().default_val = "unspecified";
    out.args.back().expected = true;
    out.args.back().examples = { "unspecified", "POIs", "peaks", "above_zero", "below_5.3" };


    out.args.emplace_back();
    out.args.back().name = "Lower";
    out.args.back().desc = "The lower bound (inclusive). Pixels with values < this number are excluded from the ROI."
                           " If the number is followed by a '%', the bound will be scaled between the min and max"
                           " pixel values [0-100%]. If the number is followed by 'tile', the bound will be replaced"
                           " with the corresponding percentile [0-100tile]."
                           " Note that upper and lower bounds can be specified separately (e.g., lower bound is a"
                           " percentage, but upper bound is a percentile).";
    out.args.back().default_val = "-inf";
    out.args.back().expected = true;
    out.args.back().examples = { "0.0", "-1E-99", "1.23", "0.2%", "23tile", "23.123 tile" };


    out.args.emplace_back();
    out.args.back().name = "Upper";
    out.args.back().desc = "The upper bound (inclusive). Pixels with values > this number are excluded from the ROI."
                           " If the number is followed by a '%', the bound will be scaled between the min and max"
                           " pixel values [0-100%]. If the number is followed by 'tile', the bound will be replaced"
                           " with the corresponding percentile [0-100tile]."
                           " Note that upper and lower bounds can be specified separately (e.g., lower bound is a"
                           " percentage, but upper bound is a percentile).";
    out.args.back().default_val = "inf";
    out.args.back().expected = true;
    out.args.back().examples = { "1.0", "1E-99", "2.34", "98.12%", "94tile", "94.123 tile" };


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



Drover ConvertPixelsToPoints(Drover DICOM_data,
                             const OperationArgPkg& OptArgs,
                             const std::map<std::string, std::string>&
                             /*InvocationMetadata*/,
                             const std::string& FilenameLex){

    Explicator X(FilenameLex);

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto LabelStr = OptArgs.getValueStr("Label").value();
    const auto LowerStr = OptArgs.getValueStr("Lower").value();
    const auto UpperStr = OptArgs.getValueStr("Upper").value();
    const auto ChannelStr = OptArgs.getValueStr("Channel").value();
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();

    //-----------------------------------------------------------------------------------------------------------------
    const auto Lower = std::stod( LowerStr );
    const auto Upper = std::stod( UpperStr );
    const auto Channel = std::stol( ChannelStr );

    const auto regex_is_percent = Compile_Regex(".*[%].*");
    const auto Lower_is_Percent = std::regex_match(LowerStr, regex_is_percent);
    const auto Upper_is_Percent = std::regex_match(UpperStr, regex_is_percent);

    const auto regex_is_tile = Compile_Regex(".*p?e?r?c?e?n?tile.*");
    const auto Lower_is_Ptile = std::regex_match(LowerStr, regex_is_tile);
    const auto Upper_is_Ptile = std::regex_match(UpperStr, regex_is_tile);


    //Construct a destination for the point clouds.
    DICOM_data.point_data.emplace_back( std::make_unique<Point_Cloud>() );

    std::mutex point_pusher; // Who gets to save points.

    //Iterate over each requested image_array. Each image is processed independently, so a thread pool is used.
    auto IAs_all = All_IAs( DICOM_data );
    auto IAs = Whitelist( IAs_all, ImageSelectionStr );
    for(auto & iap_it : IAs){
        asio_thread_pool tp;
        std::mutex saver_printer; // Who gets to save generated contours, print to the console, and iterate the counter.
        long int completed = 0;
        const long int img_count = (*iap_it)->imagecoll.images.size();

        for(const auto &img : (*iap_it)->imagecoll.images){
            if( (img.rows < 1) || (img.columns < 1) || (Channel >= img.channels) ){
                continue;
            }

            std::reference_wrapper< const planar_image<float, double>> img_refw( std::ref(img) );

            tp.submit_task([&,img_refw]() -> void {

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

                //Construct a pixel 'oracle' closure using the user-specified threshold criteria. This function identifies whether
                //the pixel is within (true) or outside of (false) the final ROI.
                auto pixel_oracle = [cu,cl](float p) -> bool {
                    return (cl <= p) && (p <= cu);
                };

                //Perform the conversion.
                img_refw.get().apply_to_pixels([&,Channel](long int row, long int col, long int chnl, float val) -> void {
                    if((chnl == Channel) && pixel_oracle(val)){
                        const auto pos = img_refw.get().position(row, col);
                         
                        std::lock_guard<std::mutex> lock(point_pusher);
                        DICOM_data.point_data.back()->pset.points.emplace_back( pos );
                     }
                     return;
                });

            }); // Thread pool task.
        } // Loop over images.
    } // Loop over image arrays.


    // Determine the common set of image metadata and assign it to the point data.
    {
        planar_image_collection<float, double> dummy;
        decltype(dummy.get_all_images()) all_img_list_iters;
        for(auto & iap_it : IAs){
            for(auto img_it = (*iap_it)->imagecoll.images.begin(); img_it != (*iap_it)->imagecoll.images.end(); ++img_it){
                all_img_list_iters.emplace_back( img_it );
            }
        }
        std::map<std::string,std::string> cm = dummy.get_common_metadata(all_img_list_iters);

        DICOM_data.point_data.back()->pset.metadata = cm;
    }

    DICOM_data.point_data.back()->pset.metadata["Label"] = LabelStr;
    DICOM_data.point_data.back()->pset.metadata["Description"] = "Point cloud derived from volumetric images.";

    return DICOM_data;
}
