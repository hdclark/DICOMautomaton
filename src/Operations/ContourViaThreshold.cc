//ContourViaThreshold.cc - A part of DICOMautomaton 2017. Written by hal clark.

#include <iostream>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <string>    
#include <vector>
#include <set> 
#include <map>
#include <unordered_map>
#include <list>
#include <functional>
#include <thread>
#include <array>
#include <mutex>
#include <limits>
#include <cmath>
#include <regex>

#include <getopt.h>           //Needed for 'getopts' argument parsing.
#include <cstdlib>            //Needed for exit() calls.
#include <utility>            //Needed for std::pair.
#include <algorithm>
#include <experimental/optional>

#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMathPlottingGnuplot.h" //Needed for YgorMathPlottingGnuplot::*.
#include "YgorMathChebyshev.h" //Needed for cheby_approx class.
#include "YgorStats.h"        //Needed for Stats:: namespace.
#include "YgorFilesDirs.h"    //Needed for Does_File_Exist_And_Can_Be_Read(...), etc..
#include "YgorContainers.h"   //Needed for bimap class.
#include "YgorPerformance.h"  //Needed for YgorPerformance_dt_from_last().
#include "YgorAlgorithms.h"   //Needed for For_Each_In_Parallel<..>(...)
#include "YgorArguments.h"    //Needed for ArgumentHandler class.
#include "YgorString.h"       //Needed for GetFirstRegex(...)
#include "YgorImages.h"
#include "YgorImagesIO.h"
#include "YgorImagesPlotting.h"

#include "Explicator.h"       //Needed for Explicator class.

#include "../Structs.h"
#include "../Thread_Pool.h"

#include "../YgorImages_Functors/Grouping/Misc_Functors.h"

#include "../YgorImages_Functors/Processing/DCEMRI_AUC_Map.h"
#include "../YgorImages_Functors/Processing/DCEMRI_S0_Map.h"
#include "../YgorImages_Functors/Processing/DCEMRI_T1_Map.h"
#include "../YgorImages_Functors/Processing/Highlight_ROI_Voxels.h"
#include "../YgorImages_Functors/Processing/Kitchen_Sink_Analysis.h"
#include "../YgorImages_Functors/Processing/IVIMMRI_ADC_Map.h"
#include "../YgorImages_Functors/Processing/Time_Course_Slope_Map.h"
#include "../YgorImages_Functors/Processing/CT_Perfusion_Clip_Search.h"
#include "../YgorImages_Functors/Processing/CT_Perf_Pixel_Filter.h"
#include "../YgorImages_Functors/Processing/CT_Convert_NaNs_to_Air.h"
#include "../YgorImages_Functors/Processing/Min_Pixel_Value.h"
#include "../YgorImages_Functors/Processing/Max_Pixel_Value.h"
#include "../YgorImages_Functors/Processing/CT_Reasonable_HU_Window.h"
#include "../YgorImages_Functors/Processing/Slope_Difference.h"
#include "../YgorImages_Functors/Processing/Centralized_Moments.h"
#include "../YgorImages_Functors/Processing/Logarithmic_Pixel_Scale.h"
#include "../YgorImages_Functors/Processing/Per_ROI_Time_Courses.h"
#include "../YgorImages_Functors/Processing/DBSCAN_Time_Courses.h"
#include "../YgorImages_Functors/Processing/In_Image_Plane_Bilinear_Supersample.h"
#include "../YgorImages_Functors/Processing/In_Image_Plane_Bicubic_Supersample.h"
#include "../YgorImages_Functors/Processing/In_Image_Plane_Pixel_Decimate.h"
#include "../YgorImages_Functors/Processing/Cross_Second_Derivative.h"
#include "../YgorImages_Functors/Processing/Orthogonal_Slices.h"

#include "../YgorImages_Functors/Transform/DCEMRI_C_Map.h"
#include "../YgorImages_Functors/Transform/DCEMRI_Signal_Difference_C.h"
#include "../YgorImages_Functors/Transform/CT_Perfusion_Signal_Diff.h"
#include "../YgorImages_Functors/Transform/DCEMRI_S0_Map_v2.h"
#include "../YgorImages_Functors/Transform/DCEMRI_T1_Map_v2.h"
#include "../YgorImages_Functors/Transform/Pixel_Value_Histogram.h"
#include "../YgorImages_Functors/Transform/Subtract_Spatially_Overlapping_Images.h"

#include "../YgorImages_Functors/Compute/Per_ROI_Time_Courses.h"
#include "../YgorImages_Functors/Compute/Contour_Similarity.h"
#include "../YgorImages_Functors/Compute/AccumulatePixelDistributions.h"

#include "ContourViaThreshold.h"



std::list<OperationArgDoc> OpArgDocContourViaThreshold(void){
    std::list<OperationArgDoc> out;

    // This operation constructs ROI contours using images and pixel/voxel value thresholds.
    // The output is 'ephemeral' and is not commited to any database.
    //
    // NOTE: This routine expects images to be non-overlapping. In other words, if images overlap then the contours
    //       generated may also overlap. This is probably not what you want (but there is nothing intrinsically wrong
    //       with presenting this routine with multiple images if you intentionally want overlapping contours). 
    //
    // NOTE: Existing contours are ignored and unaltered.
    //
    // NOTE: Contour orientation is (likely) not properly handled in this routine, so 'pinches' and holes will produce
    //       contours with inconsistent or invalid topology. If in doubt, disable merge simplifications and live with
    //       the computational penalty.
    //

    out.emplace_back();
    out.back().name = "ROILabel";
    out.back().desc = "A label to attach to the ROI contours.";
    out.back().default_val = "unspecified";
    out.back().expected = true;
    out.back().examples = { "unspecified", "body", "air", "bone", "invalid", "above_zero", "below_5.3" };


    out.emplace_back();
    out.back().name = "Lower";
    out.back().desc = "The lower bound (inclusive). Pixels with values < this number are excluded from the ROI.";
    out.back().default_val = "-inf";
    out.back().expected = true;
    out.back().examples = { "0.0", "-1E-99", "1.23" };


    out.emplace_back();
    out.back().name = "Upper";
    out.back().desc = "The upper bound (inclusive). Pixels with values > this number are excluded from the ROI.";
    out.back().default_val = "inf";
    out.back().expected = true;
    out.back().examples = { "1.0", "1E-99", "2.34" };


    out.emplace_back();
    out.back().name = "Channel";
    out.back().desc = "The image channel to use. Zero-based.";
    out.back().default_val = "0";
    out.back().expected = true;
    out.back().examples = { "0", "1", "2" };

    
    out.emplace_back();
    out.back().name = "ImageSelection";
    out.back().desc = "Images to operate on. Either 'none', 'last', or 'all'.";
    out.back().default_val = "last";
    out.back().expected = true;
    out.back().examples = { "none", "last", "all" };
   

    out.emplace_back();
    out.back().name = "SimplifyMergeAdjacent";
    out.back().desc = "Simplify contours by merging adjacent contours. This reduces the number of contours dramatically,"
                      " but will cause issues if there are holes (two contours are generated if there is a single hole,"
                      " but most DICOMautomaton code disregards orientation -- so the pixels within the hole will be"
                      " considered part of the ROI, possibly even doubly so depending on the algorithm). Disabling merges"
                      " is always safe (and is therefore the default) but can be extremely costly for large images."
                      " Furthermore, if you know the ROI does not have holes (or if you don't care) then it is safe to"
                      " enable merges.";
    out.back().default_val = "false";
    out.back().expected = true;
    out.back().examples = { "true", "false" };
    

    return out;
}



Drover ContourViaThreshold(Drover DICOM_data, OperationArgPkg OptArgs, std::map<std::string,std::string> /*InvocationMetadata*/, std::string FilenameLex){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ROILabel = OptArgs.getValueStr("ROILabel").value();
    const auto LowerStr = OptArgs.getValueStr("Lower").value();
    const auto UpperStr = OptArgs.getValueStr("Upper").value();
    const auto ChannelStr = OptArgs.getValueStr("Channel").value();
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();
    const auto SimplifyMergeAdjacent = OptArgs.getValueStr("SimplifyMergeAdjacent").value();

    //-----------------------------------------------------------------------------------------------------------------
    const auto Lower = std::stod( LowerStr );
    const auto Upper = std::stod( UpperStr );
    const auto Channel = std::stol( ChannelStr );

    const auto regex_none = std::regex("no?n?e?$", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto regex_last = std::regex("la?s?t?$", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto regex_all  = std::regex("al?l?$",   std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto TrueRegex = std::regex("^tr?u?e?$", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);

    const auto ShouldSimplifyMergeAdjacent = std::regex_match(SimplifyMergeAdjacent, TrueRegex);

    if( !std::regex_match(ImageSelectionStr, regex_none)
    &&  !std::regex_match(ImageSelectionStr, regex_last)
    &&  !std::regex_match(ImageSelectionStr, regex_all) ){
        throw std::invalid_argument("Image selection is not valid. Cannot continue.");
    }

    //Construct a pixel 'oracle' closure using the user-specified threshold criteria. This function identifies whether
    //the pixel is within (true) or outside of (false) the final ROI.
    auto pixel_oracle = [Lower,Upper](float p) -> bool {
        return (Lower <= p) && (p <= Upper);
    };

    //Construct a destination for the ROI contours.
    if(DICOM_data.contour_data == nullptr){
        std::unique_ptr<Contour_Data> output (new Contour_Data());
        DICOM_data.contour_data = std::move(output);
    }
    DICOM_data.contour_data->ccs.emplace_back();

    DICOM_data.contour_data->ccs.back().Raw_ROI_name = ROILabel;
    DICOM_data.contour_data->ccs.back().ROI_number = 10000; // TODO: find highest existing and ++ it.
    DICOM_data.contour_data->ccs.back().Minimum_Separation = 1.0; // TODO: is there a routine to do this? (YES: Unique_Contour_Planes()...)

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

        for(const auto &animg : (*iap_it)->imagecoll.images){
            if( (animg.rows < 1) || (animg.columns < 1) || (Channel >= animg.channels) ){
                throw std::runtime_error("Image or channel is empty -- cannot contour via thresholds.");
            }
            tp.submit_task([&](void) -> void {
                const auto R = animg.rows;
                const auto C = animg.columns;

                //Construct the vertex grid. Vertices are in the corners of pixels, but we also need a mapping from
                // pixel coordinate space to the vertex grid storage indices.
                const auto vert_count = (R+1)*(C+1);
                std::vector<vec3<double>> verts( vert_count );
                enum row_modif { r_neg = 0, r_pos = 1 }; // Modifiers which translate (in the img plane) +-1/2 of pxl_dx.
                enum col_modif { c_neg = 0, c_pos = 1 }; // Modifiers which translate (in the img plane) +-1/2 of pxl_dy.

                const auto vert_index = [R,C](long int vert_row, long int vert_col) -> long int {
                    return (C+1)*vert_row + vert_col;
                };
                const auto vert_mapping = [vert_index,R,C](long int r, long int c, row_modif rm, col_modif cm ) -> long int {
                    const auto vert_row = r + rm;
                    const auto vert_col = c + cm;
                    return vert_index(vert_row,vert_col);
                };

                //Pin each vertex grid element to the appropriate pixel corner.
                const auto corner = animg.position(0,0) - animg.row_unit*animg.pxl_dx*0.5 - animg.col_unit*animg.pxl_dy*0.5;
                for(auto r = 0; r < (R+1); ++r){
                    for(auto c = 0; c < (C+1); ++c){
                        verts.at(vert_index(r,c)) = corner + animg.row_unit*animg.pxl_dx*r
                                                           + animg.col_unit*animg.pxl_dy*c;
                    }
                }

                //Construct a container for storing half-edges.
                std::map<long int, std::set<long int>> half_edges;

                //Iterate over each pixel. If the oracle tells us the pixel is within the ROI, add four half-edges
                // around the pixel's perimeter.
                for(auto r = 0; r < R; ++r){
                    for(auto c = 0; c < C; ++c){
                        if(pixel_oracle(animg.value(r, c, Channel))){
                            const auto bot_l = vert_mapping(r,c,r_pos,c_neg);
                            const auto bot_r = vert_mapping(r,c,r_pos,c_pos);
                            const auto top_r = vert_mapping(r,c,r_neg,c_pos);
                            const auto top_l = vert_mapping(r,c,r_neg,c_neg);

                            half_edges[bot_l].insert(bot_r);
                            half_edges[bot_r].insert(top_r);
                            half_edges[top_r].insert(top_l);
                            half_edges[top_l].insert(bot_l);
                        }
                    }
                }

                //Find and remove all cancelling half-edges, which are equivalent to circular two-vertex loops.
                if(ShouldSimplifyMergeAdjacent){
                    //'Retire' half-edges by merely removing their endpoint, invalidating them.
                    for(auto &he_group : half_edges){
                        const auto A = he_group.first;
                        auto B_it = he_group.second.begin();
                        while(B_it != he_group.second.end()){
                            const auto he_group2_it = half_edges.find(*B_it);
                            if( (he_group2_it != half_edges.end()) 
                            &&  (he_group2_it->second.count(A) != 0) ){
                                //Cycle detected -- remove both half-edges.
                                he_group2_it->second.erase(A);
                                B_it = he_group.second.erase(B_it);
                            }else{
                                ++B_it;
                            }
                        }
                    }
                }

                // Additional simplification could be performed here...
                // 
                // Ideas:
                //   - Simplify straight lines by removing redundant vertices along line segments.
                //     (Can this be done on the contour later?)
                //
                //   - Remove vertices that do not affect the total area appreciably. (Note: do this for the contour.)
                //  

                
                //Walk all available half-edges forming contour perimeters.
                std::list<contour_of_points<double>> cop;
                {
                    auto he_it = half_edges.begin();
                    while(he_it != half_edges.end()){
                        if(he_it->second.empty()){
                            ++he_it;
                            continue;
                        }

                        cop.emplace_back();
                        cop.back().closed = true;
                        cop.back().metadata["ROIName"] = ROILabel;
                        cop.back().metadata["Description"] = "Contoured via threshold ("_s + std::to_string(Lower)
                                                             + " <= pixel_val <= " + std::to_string(Upper) + ")";
                        for(const auto &key : { "StudyInstanceUID", "FrameofReferenceUID" }){
                            if(animg.metadata.count(key) != 0) cop.back().metadata[key] = animg.metadata.at(key);
                        }

                        const auto A = he_it->first; //The starting node.
                        auto B = A;
                        do{
                            const auto B_he_it = half_edges.find(B);
                            auto B_it = B_he_it->second.begin(); // TODO: pick left-most (relative to current direction) node.
                                                                 //       This is how you can will get consistent orientation handling!

                            B = *B_it;
                            cop.back().points.emplace_back(verts[B]); //Add the vertex to the current contour.
                            B_he_it->second.erase(B_it); //Retire the node.
                        }while(B != A);
                    }
                }


                //Save the contours and print some information to screen.
                {
                    std::lock_guard<std::mutex> lock(saver_printer);
                    DICOM_data.contour_data->ccs.back().contours.splice( DICOM_data.contour_data->ccs.back().contours.end(), cop);

                    ++completed;
                    FUNCINFO("Completed " << completed << " of " << img_count
                          << " --> " << static_cast<int>(1000.0*(completed)/img_count)/10.0 << "\% done");
                }
            }); // thread pool task closure.
        }
    }

    return std::move(DICOM_data);
}
