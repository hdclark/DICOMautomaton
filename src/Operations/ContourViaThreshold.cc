//ContourViaThreshold.cc - A part of DICOMautomaton 2017. Written by hal clark.

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
#include "ContourViaThreshold.h"
#include "Explicator.h"       //Needed for Explicator class.
#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorStats.h"        //Needed for Stats:: namespace.
#include "YgorString.h"       //Needed for GetFirstRegex(...)
#include "../Surface_Meshes.h"



OperationDoc OpArgDocContourViaThreshold(void){
    OperationDoc out;
    out.name = "ContourViaThreshold";

    out.desc = 
        "This operation constructs ROI contours using images and pixel/voxel value thresholds."
        " The output is 'ephemeral' and is not commited to any database.";
        
    out.notes.emplace_back(
        "This routine expects images to be non-overlapping. In other words, if images overlap then the contours"
        " generated may also overlap. This is probably not what you want (but there is nothing intrinsically wrong"
        " with presenting this routine with multiple images if you intentionally want overlapping contours)."
    );
        
    out.notes.emplace_back(
        "Existing contours are ignored and unaltered."
    );
        
    out.notes.emplace_back(
        "Contour orientation is (likely) not properly handled in this routine, so 'pinches' and holes will produce"
        " contours with inconsistent or invalid topology. If in doubt, disable merge simplifications and live with"
        " the computational penalty."
    );
        

    out.args.emplace_back();
    out.args.back().name = "ROILabel";
    out.args.back().desc = "A label to attach to the ROI contours.";
    out.args.back().default_val = "unspecified";
    out.args.back().expected = true;
    out.args.back().examples = { "unspecified", "body", "air", "bone", "invalid", "above_zero", "below_5.3" };


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
   

    out.args.emplace_back();
    out.args.back().name = "SimplifyMergeAdjacent";
    out.args.back().desc = "Simplify contours by merging adjacent contours. This reduces the number of contours dramatically,"
                      " but will cause issues if there are holes (two contours are generated if there is a single hole,"
                      " but most DICOMautomaton code disregards orientation -- so the pixels within the hole will be"
                      " considered part of the ROI, possibly even doubly so depending on the algorithm). Disabling merges"
                      " is always safe (and is therefore the default) but can be extremely costly for large images."
                      " Furthermore, if you know the ROI does not have holes (or if you don't care) then it is safe to"
                      " enable merges.";
    out.args.back().default_val = "false";
    out.args.back().expected = true;
    out.args.back().examples = { "true", "false" };
    

    return out;
}



Drover ContourViaThreshold(Drover DICOM_data, OperationArgPkg OptArgs, std::map<std::string,std::string> /*InvocationMetadata*/, std::string FilenameLex){

    Explicator X(FilenameLex);

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ROILabel = OptArgs.getValueStr("ROILabel").value();
    const auto LowerStr = OptArgs.getValueStr("Lower").value();
    const auto UpperStr = OptArgs.getValueStr("Upper").value();
    const auto ChannelStr = OptArgs.getValueStr("Channel").value();
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();
    const auto SimplifyMergeAdjacentStr = OptArgs.getValueStr("SimplifyMergeAdjacent").value();

    //-----------------------------------------------------------------------------------------------------------------
    const auto Lower = std::stod( LowerStr );
    const auto Upper = std::stod( UpperStr );
    const auto Channel = std::stol( ChannelStr );

    const auto regex_is_percent = std::regex(".*[%].*", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto Lower_is_Percent = std::regex_match(LowerStr, regex_is_percent);
    const auto Upper_is_Percent = std::regex_match(UpperStr, regex_is_percent);

    const auto regex_is_tile = std::regex(".*p?e?r?c?e?n?tile.*", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto Lower_is_Ptile = std::regex_match(LowerStr, regex_is_tile);
    const auto Upper_is_Ptile = std::regex_match(UpperStr, regex_is_tile);

    const auto TrueRegex = std::regex("^tr?u?e?$", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);

    const auto SimplifyMergeAdjacent = std::regex_match(SimplifyMergeAdjacentStr, TrueRegex);

    const auto NormalizedROILabel = X(ROILabel);

    //Construct a destination for the ROI contours.
    if(DICOM_data.contour_data == nullptr){
        std::unique_ptr<Contour_Data> output (new Contour_Data());
        DICOM_data.contour_data = std::move(output);
    }
    DICOM_data.contour_data->ccs.emplace_back();

    const double MinimumSeparation = 1.0; // TODO: is there a routine to do this? (YES: Unique_Contour_Planes()...)
    DICOM_data.contour_data->ccs.back().Raw_ROI_name = ROILabel;
    DICOM_data.contour_data->ccs.back().ROI_number = 10000; // TODO: find highest existing and ++ it.
    DICOM_data.contour_data->ccs.back().Minimum_Separation = MinimumSeparation;

    //Iterate over each requested image_array. Each image is processed independently, so a thread pool is used.
    auto IAs_all = All_IAs( DICOM_data );
    auto IAs = Whitelist( IAs_all, ImageSelectionStr );
    for(auto & iap_it : IAs){
        asio_thread_pool tp;
        std::mutex saver_printer; // Who gets to save generated contours, print to the console, and iterate the counter.
        long int completed = 0;
        const long int img_count = (*iap_it)->imagecoll.images.size();

        for(const auto &animg : (*iap_it)->imagecoll.images){
            if( (animg.rows < 1) || (animg.columns < 1) || (Channel >= animg.channels) ){
                throw std::runtime_error("Image or channel is empty -- cannot contour via thresholds.");
            }
            tp.submit_task([&](void) -> void {

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

                //Construct a pixel 'oracle' closure using the user-specified threshold criteria. This function identifies whether
                //the pixel is within (true) or outside of (false) the final ROI.
                auto pixel_oracle = [cu,cl](float p) -> bool {
                    return (cl <= p) && (p <= cu);
                };

                //Prepare a mask image for contouring.
                auto mask = animg;
                const double interior_value = -1.0;
                const double exterior_value =  1.0;
                const double inclusion_threshold = 0.0;
                const bool below_is_interior = true;
                mask.apply_to_pixels([pixel_oracle,
                                      Channel,
                                      interior_value,
                                      exterior_value](long int, 
                                                      long int,
                                                      long int chnl,
                                                      float &val) -> void {
                        if(Channel == chnl){
                            val = (pixel_oracle(val) ? interior_value : exterior_value);
                        }
                        return;
                    });

                const auto N_0 = mask.image_plane().N_0;
                auto above = animg;
                auto below = animg;
                above.fill_pixels(exterior_value);
                below.fill_pixels(exterior_value);
                above.offset += N_0 * mask.pxl_dz;
                below.offset -= N_0 * mask.pxl_dz;

                std::list<std::reference_wrapper<planar_image<float,double>>> grid_imgs;
                grid_imgs = {{ 
                    std::ref(above),
                    std::ref(mask),
                    std::ref(below) }};

                //Generate the surface mesh.
                auto surface_mesh = dcma_surface_meshes::Estimate_Surface_Mesh_Marching_Cubes( 
                                                                grid_imgs,
                                                                inclusion_threshold, 
                                                                below_is_interior,
                                                                dcma_surface_meshes::Parameters() );

                //Slice the mesh along the image plane.
                auto lcc = polyhedron_processing::Slice_Polyhedron( surface_mesh, 
                                                                    {{ mask.image_plane() }} );

                //Tag the contours with metadata.
                for(auto &cop : lcc.contours){
                    cop.closed = true;
                    cop.metadata["ROIName"] = ROILabel;
                    cop.metadata["NormalizedROIName"] = NormalizedROILabel;
                    cop.metadata["Description"] = "Contoured via threshold ("_s + std::to_string(Lower)
                                                 + " <= pixel_val <= " + std::to_string(Upper) + ")";
                    cop.metadata["MinimumSeparation"] = MinimumSeparation;
                    for(const auto &key : { "StudyInstanceUID", "FrameofReferenceUID" }){
                        if(animg.metadata.count(key) != 0) cop.metadata[key] = animg.metadata.at(key);
                    }
                }

                // Try to simplify the contours as much as possible.
                //
                // The following will straddle each vertex, interpolate the adjacent vertices, and compare the
                // interpolated vertex to the actual (straddled) vertex. If they differ by a small amount, the straddled
                // vertex is pruned. 1% should be more than enough to account for numerical fluctuations.
                if(SimplifyMergeAdjacent){
                    const auto tolerance_sq_dist = 0.01*(animg.pxl_dx*animg.pxl_dx + animg.pxl_dy*animg.pxl_dy + animg.pxl_dz*animg.pxl_dz);
                    const auto verts_are_equal = [=](const vec3<double> &A, const vec3<double> &B) -> bool {
                        return A.sq_dist(B) < tolerance_sq_dist;
                    };
                    for(auto &cop : lcc.contours){
                        cop.Remove_Sequential_Duplicate_Points(verts_are_equal);
                        cop.Remove_Extraneous_Points(verts_are_equal);
                    }
                }

                //Save the contours and print some information to screen.
                {
                    std::lock_guard<std::mutex> lock(saver_printer);
                    DICOM_data.contour_data->ccs.back().contours.splice(DICOM_data.contour_data->ccs.back().contours.end(),
                                                                        lcc.contours);

                    ++completed;
                    FUNCINFO("Completed " << completed << " of " << img_count
                          << " --> " << static_cast<int>(1000.0*(completed)/img_count)/10.0 << "\% done");
                }
            }); // thread pool task closure.
        }
    }

    DICOM_data.contour_data->ccs.back().Raw_ROI_name = ROILabel;
    return DICOM_data;
}
