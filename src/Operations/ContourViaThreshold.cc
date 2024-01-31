//ContourViaThreshold.cc - A part of DICOMautomaton 2017. Written by hal clark.

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
#include <cstdint>

#include "Explicator.h"       //Needed for Explicator class.

#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorLog.h"
#include "YgorStats.h"        //Needed for Stats:: namespace.
#include "YgorString.h"       //Needed for GetFirstRegex(...)

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../Thread_Pool.h"
#include "../Metadata.h"
#ifdef DCMA_USE_CGAL
    #include "../Surface_Meshes.h"
#endif // DCMA_USE_CGAL

#include "ContourViaThreshold.h"


OperationDoc OpArgDocContourViaThreshold(){
    OperationDoc out;
    out.name = "ContourViaThreshold";
    out.aliases.emplace_back("ConvertImagesToContours");

    out.desc = 
        "This operation constructs ROI contours using images and pixel/voxel value thresholds."
        " There are three methods of contour generation available:"
        " a simple binary method in which voxels are either fully in or fully out of the contour,"
        " marching squares (which uses linear interpolation to give smooth contours),"
        " and a method based on 3D marching cubes that will also provide smooth contours."
        " The marching cubes method does **not** construct a full surface mesh; rather each"
        " individual image slice has their own mesh constructed in parallel.";
        
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
        " the computational penalty. The marching cubes approach will properly handle 'pinches' and contours should"
        " all be topologically valid."
    );
    out.notes.emplace_back(
        "Note that the marching-squares method currently only honours the lower threshold."
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
                           " Both percentages and percentiles are assessed per image array."
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
                           " Both percentages and percentiles are assessed per image array."
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
    out.args.back().name = "Method";
    out.args.back().desc = "There are currently three supported methods for generating contours:"
                           " (1) a simple (and fast) 'binary' inclusivity checker, that simply checks if a voxel is within"
                           " the ROI by testing the value at the voxel centre, (2) the 'marching-squares' method,"
                           " which samples the corners of every voxel and uses linear interpolation to estimate"
                           " the threshold value isoline crossings, and (3) a robust but slow method based"
                           " on 'marching-cubes'. The binary method is fast, but produces extremely jagged contours."
                           " It may also have problems with 'pinches' and topological consistency."
                           " Marching-squares is reasonably fast and general-purpose, and should produce good quality"
                           " contours that approximate the threshold value isocurves to first-order."
                           " It also handles boundaries well by inserting an extra virtual row and column around"
                           " the image to ensure contours are all closed."
                           " The marching-cubes method is more robust and should reliably produce contours for even"
                           " the most complicated topologies, but is considerably slower than the binary method."
                           " It may produce worse on boundaries, though otherwise it should produce the same"
                           " contours as marching-squares.";
#ifdef DCMA_USE_CGAL
    out.args.back().examples = { "binary",
                                 "marching-squares",
                                 "marching-cubes" };
#else
    out.args.back().desc += " Note that the 'marching' option is only available when CGAL support is enabled."
                            " This instance does not have CGAL support.";
    out.args.back().examples = { "binary",
                                 "marching-squares" };
#endif // DCMA_USE_CGAL
    out.args.back().default_val = "binary";
    out.args.back().expected = true;
    out.args.back().samples = OpArgSamples::Exhaustive;

    
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

bool ContourViaThreshold(Drover &DICOM_data,
                           const OperationArgPkg& OptArgs,
                           std::map<std::string, std::string>& /*InvocationMetadata*/,
                           const std::string& FilenameLex){

    Explicator X(FilenameLex);

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ROILabel = OptArgs.getValueStr("ROILabel").value();
    const auto LowerStr = OptArgs.getValueStr("Lower").value();
    const auto UpperStr = OptArgs.getValueStr("Upper").value();
    const auto ChannelStr = OptArgs.getValueStr("Channel").value();
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();
    const auto MethodStr = OptArgs.getValueStr("Method").value();
    const auto SimplifyMergeAdjacentStr = OptArgs.getValueStr("SimplifyMergeAdjacent").value();

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

    const auto binary_regex = Compile_Regex("^bi?n?a?r?y?$");
    const auto marching_squares_regex = Compile_Regex("^ma?r?c?h?i?n?g?[_-]?sq?u?a?r?e?s?$");
    const auto marching_cubes_regex = Compile_Regex("^ma?r?c?h?i?n?g?[_-]?cu?b?e?s?$");

    const auto TrueRegex = Compile_Regex("^tr?u?e?$");

    const auto SimplifyMergeAdjacent = std::regex_match(SimplifyMergeAdjacentStr, TrueRegex);

    const auto NormalizedROILabel = X(ROILabel);

    //Construct a destination for the ROI contours.
    DICOM_data.Ensure_Contour_Data_Allocated();
    DICOM_data.contour_data->ccs.emplace_back();

    const double MinimumSeparation = 1.0; // TODO: is there a routine to do this? (YES: Unique_Contour_Planes()...)

    //Iterate over each requested image_array. Each image is processed independently, so a thread pool is used.
    auto IAs_all = All_IAs( DICOM_data );
    auto IAs = Whitelist( IAs_all, ImageSelectionStr );
    for(auto & iap_it : IAs){
        const int64_t img_count = (*iap_it)->imagecoll.images.size();

        std::mutex saver_printer; // Who gets to save generated contours, print to the console, and iterate the counter.
        int64_t completed = 0;

        //Determine the bounds in terms of pixel-value thresholds.
        auto cl = Lower; // Will be replaced if percentages/percentiles requested.
        auto cu = Upper; // Will be replaced if percentages/percentiles requested.
        {
            //Percentage-based.
            if(Lower_is_Percent || Upper_is_Percent){
                Stats::Running_MinMax<float> rmm;
                for(const auto &animg : (*iap_it)->imagecoll.images){
                    animg.apply_to_pixels([&rmm,Channel](int64_t, int64_t, int64_t chnl, float val) -> void {
                         if(Channel == chnl) rmm.Digest(val);
                         return;
                    });
                }
                if(Lower_is_Percent) cl = (rmm.Current_Min() + (rmm.Current_Max() - rmm.Current_Min()) * Lower / 100.0);
                if(Upper_is_Percent) cu = (rmm.Current_Min() + (rmm.Current_Max() - rmm.Current_Min()) * Upper / 100.0);
            }

            //Percentile-based.
            if(Lower_is_Ptile || Upper_is_Ptile){
                std::vector<float> pixel_vals;
                //pixel_vals.reserve(animg.rows * animg.columns * animg.channels * img_count);
                for(const auto &animg : (*iap_it)->imagecoll.images){
                    animg.apply_to_pixels([&pixel_vals,Channel](int64_t, int64_t, int64_t chnl, float val) -> void {
                         if(Channel == chnl) pixel_vals.push_back(val);
                         return;
                    });
                }
                if(Lower_is_Ptile) cl = Stats::Percentile(pixel_vals, Lower / 100.0);
                if(Upper_is_Ptile) cu = Stats::Percentile(pixel_vals, Upper / 100.0);
            }
        }
        YLOGINFO("Using thresholds " << cl << " and " << cu);
        if( !std::isfinite(cl) 
        &&  !std::isfinite(cu)){
            throw std::invalid_argument("Both thresholds are not finite. Refusing to continue.");
        }
        if(cl > cu){
            throw std::invalid_argument("Thresholds conflict. Mesh will contain zero faces. Refusing to continue.");
        }
        
        auto cm = (*iap_it)->imagecoll.get_common_metadata({});
        cm = coalesce_metadata_for_rtstruct(cm);

        work_queue<std::function<void(void)>> wq;
        for(const auto &animg : (*iap_it)->imagecoll.images){
            if( (animg.rows < 1) || (animg.columns < 1) || (Channel >= animg.channels) ){
                throw std::runtime_error("Image or channel is empty -- cannot contour via thresholds.");
            }

            const auto animg_ptr = &(animg);
            wq.submit_task([cl,
                            cu,
                            cm,
                            animg_ptr,
                            MethodStr,
                            binary_regex,
                            marching_squares_regex,
                            marching_cubes_regex,
                            Channel,
                            SimplifyMergeAdjacent,
                            ROILabel,
                            NormalizedROILabel,
                            Lower,
                            Upper,
                            LowerStr,
                            UpperStr,
                            MinimumSeparation,
                            &saver_printer,
                            &DICOM_data,
                            &completed,
                            img_count]() -> void {

                // ---------------------------------------------------
                // The binary inclusivity method.
                if(std::regex_match(MethodStr, binary_regex)){

                    const auto R = animg_ptr->rows;
                    const auto C = animg_ptr->columns;

                    //Construct a pixel 'oracle' closure using the user-specified threshold criteria. This function identifies whether
                    //the pixel is within (true) or outside of (false) the final ROI.
                    auto pixel_oracle = [cu,cl](float p) -> bool {
                        return (cl <= p) && (p <= cu);
                    };

                    //Construct the vertex grid. Vertices are in the corners of pixels, but we also need a mapping from
                    // pixel coordinate space to the vertex grid storage indices.
                    const auto vert_count = (R+1)*(C+1);
                    std::vector<vec3<double>> verts( vert_count );
                    enum row_modif { r_neg = 0, r_pos = 1 }; // Modifiers which translate (in the img plane) +-1/2 of pxl_dx.
                    enum col_modif { c_neg = 0, c_pos = 1 }; // Modifiers which translate (in the img plane) +-1/2 of pxl_dy.

                    const auto vert_index = [C](int64_t vert_row, int64_t vert_col) -> int64_t {
                        return (C+1)*vert_row + vert_col;
                    };
                    const auto vert_mapping = [vert_index](int64_t r, int64_t c, row_modif rm, col_modif cm ) -> int64_t {
                        const auto vert_row = r + rm;
                        const auto vert_col = c + cm;
                        return vert_index(vert_row,vert_col);
                    };

                    //Pin each vertex grid element to the appropriate pixel corner.
                    const auto corner = animg_ptr->position(0,0)
                                      - animg_ptr->row_unit * animg_ptr->pxl_dx * 0.5
                                      - animg_ptr->col_unit * animg_ptr->pxl_dy * 0.5;
                    for(auto r = 0; r < (R+1); ++r){
                        for(auto c = 0; c < (C+1); ++c){
                            verts.at(vert_index(r,c)) = corner + animg_ptr->row_unit * animg_ptr->pxl_dx * c
                                                               + animg_ptr->col_unit * animg_ptr->pxl_dy * r;
                        }
                    }

                    //Construct a container for storing half-edges.
                    std::map<int64_t, std::set<int64_t>> half_edges;

                    //Iterate over each pixel. If the oracle tells us the pixel is within the ROI, add four half-edges
                    // around the pixel's perimeter.
                    for(auto r = 0; r < R; ++r){
                        for(auto c = 0; c < C; ++c){
                            if(pixel_oracle(animg_ptr->value(r, c, Channel))){
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
                    if(SimplifyMergeAdjacent){
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
                    std::list<contour_of_points<double>> copl;
                    if(!half_edges.empty()){
                        auto he_it = half_edges.begin();
                        while(he_it != half_edges.end()){
                            if(he_it->second.empty()){
                                ++he_it;
                                continue;
                            }

                            copl.emplace_back();
                            copl.back().closed = true;
                            copl.back().metadata = cm;
                            copl.back().metadata["ROIName"] = ROILabel;
                            copl.back().metadata["NormalizedROIName"] = NormalizedROILabel;
                            copl.back().metadata["Description"] = "Contoured via threshold ("_s + std::to_string(Lower)
                                                                 + " <= pixel_val <= " + std::to_string(Upper) + ")";
                            copl.back().metadata["ROINumber"] = std::to_string(10000); // TODO: find highest existing and ++ it.
                            copl.back().metadata["MinimumSeparation"] = std::to_string(MinimumSeparation);
                            copy_overwrite(animg_ptr->metadata, copl.back().metadata, "SOPClassUID", "ReferencedSOPClassUID"); 
                            copy_overwrite(animg_ptr->metadata, copl.back().metadata, "SOPInstanceUID", "ReferencedSOPInstanceUID"); 

                            const auto A = he_it->first; //The starting node.
                            auto B = A;
                            do{
                                const auto B_he_it = half_edges.find(B);
                                auto B_it = B_he_it->second.begin(); // TODO: pick left-most (relative to current direction) node.
                                                                     //       This is how you can will get consistent orientation handling!

                                B = *B_it;
                                copl.back().points.emplace_back(verts[B]); //Add the vertex to the current contour.
                                B_he_it->second.erase(B_it); //Retire the node.
                            }while(B != A);
                        }
                    }

                    // Try to simplify the contours as much as possible.
                    //
                    // The following will straddle each vertex, interpolate the adjacent vertices, and compare the
                    // interpolated vertex to the actual (straddled) vertex. If they differ by a small amount, the straddled
                    // vertex is pruned. 1% should be more than enough to account for numerical fluctuations.
                    /*
                    if(SimplifyMergeAdjacent){
                        const auto tolerance_sq_dist = 0.01*(animg_ptr->pxl_dx*animg_ptr->pxl_dx + animg_ptr->pxl_dy*animg_ptr->pxl_dy + animg_ptr->pxl_dz*animg_ptr->pxl_dz);
                        const auto verts_are_equal = [=](const vec3<double> &A, const vec3<double> &B) -> bool {
                            return A.sq_dist(B) < tolerance_sq_dist;
                        };
                        for(auto &cop : copl){
                            cop.Remove_Sequential_Duplicate_Points(verts_are_equal);
                            cop.Remove_Extraneous_Points(verts_are_equal);
                        }
                    }
                    */


                    //Save the contours and print some information to screen.
                    {
                        std::lock_guard<std::mutex> lock(saver_printer);
                        DICOM_data.contour_data->ccs.back().contours.splice(DICOM_data.contour_data->ccs.back().contours.end(), copl);

                        ++completed;
                        YLOGINFO("Completed " << completed << " of " << img_count
                              << " --> " << static_cast<int>(1000.0*(completed)/img_count)/10.0 << "% done");
                    }

                // ---------------------------------------------------
                // The marching squares method.
                }else if(std::regex_match(MethodStr, marching_squares_regex)){
                    //Prepare a mask image for contouring.
                    auto mask = *animg_ptr;
                    double inclusion_threshold = std::numeric_limits<double>::quiet_NaN();
                    double exterior_value = std::numeric_limits<double>::quiet_NaN();
                    bool below_is_interior = false;

                    if(std::isfinite(cl) && std::isfinite(cu)){
                        // Transform voxels by their |distance| from the midpoint. Only interior voxels will be within
                        // [0,width*0.5], and all others will be (width*0.5,inf).
                        const double midpoint = (cl + cu) * 0.5;
                        const double width = (cu - cl);

                        inclusion_threshold = width * 0.5;
                        exterior_value = inclusion_threshold + 1.0;
                        below_is_interior = true;
                        mask.apply_to_pixels([Channel,midpoint](int64_t, int64_t, int64_t chnl, float &val) -> void {
                                if(Channel == chnl){
                                    val = std::abs(val - midpoint);
                                }
                                return;
                            });

                    }else if(std::isfinite(cl)){
                        inclusion_threshold = cl;
                        exterior_value = inclusion_threshold - 1.0;
                        below_is_interior = false;

                    }else if(std::isfinite(cu)){
                        inclusion_threshold = cu;
                        exterior_value = inclusion_threshold + 1.0;
                        below_is_interior = true;

                    }else{ // Neither threshold is finite.
                        throw std::invalid_argument("Unable to discern finite threshold for meshing. Refusing to continue.");
                        // Note: it is possible to deal with these cases and generate meshings for each, i.e., either all
                        // voxels are included or no voxels are included (both are valid meshings). However, it seems
                        // most likely this is a user error. Add this functionality if necessary.

                    }

                    const auto R = animg_ptr->rows;
                    const auto C = animg_ptr->columns;

                    //Construct a pixel 'oracle' closure using the user-specified threshold criteria. This function identifies whether
                    //the pixel is within (true) or outside of (false) the final ROI.
                    const auto pixel_oracle = [inclusion_threshold,below_is_interior](float p) -> bool {
                        return below_is_interior ? (inclusion_threshold <= p) : (p <= inclusion_threshold);
                    };

                    const auto interpolate_pos = [inclusion_threshold](float f1, const vec3<double> &pos1,
                                                                       float f2, const vec3<double> &pos2) -> vec3<double> {
                        const auto num = f2 - inclusion_threshold;
                        const auto den = f2 - f1;
                        const auto m = num / den;
                        return pos2 + (pos1 - pos2) * m;
                    };

                    const uint8_t o_t = 1; // Edges of the unit cell pixel (top, bottom, left, right).
                    const uint8_t o_b = 2;
                    const uint8_t o_l = 3;
                    const uint8_t o_r = 4;

                    const vec3<double> zero3(0.0, 0.0, 0.0);
                    struct node {  // This class holds a contour edge with both vertices.
                        uint8_t tail_vert_pos; // = 0; // Which cell edge the tail vertex lies along.
                        uint8_t head_vert_pos; // = 0; // Which cell edge the head vertex lies along.
                        vec3<double> tail;
                        vec3<double> head;

                        node() : tail_vert_pos(0), head_vert_pos(0), tail(vec3<double>(0.0, 0.0, 0.0)), head(vec3<double>(0.0, 0.0, 0.0)) {};
                        node(uint8_t t_v_p, uint8_t h_v_p, const vec3<double> &t, const vec3<double> &h) : tail_vert_pos(t_v_p), head_vert_pos(h_v_p), tail(t), head(h) {};
                        node(const node &n){
                            (*this) = n;
                        };
                        node& operator=(const node& rhs){
                            if(this == &rhs) return *this;
                            this->tail_vert_pos = rhs.tail_vert_pos;
                            this->head_vert_pos = rhs.head_vert_pos;
                            this->tail = rhs.tail;
                            this->head = rhs.head;
                            return *this;
                        };
                    };
                    const node empty_node(0,0,zero3,zero3);

                    //std::vector< std::pair<node,node> > nodes((R-1)*(C-1), std::make_pair(empty_node,empty_node));
                    std::vector< std::pair<node,node> > nodes((R+1)*(C+1), std::make_pair(empty_node,empty_node));
                    const auto index = [C](int64_t r, int64_t c) -> int64_t {
                        return ( (C+1) * r + c);
                    };

                    // Override the normal voxel intensity and position getters to handle the 2 additional rows and columns.
                    const auto get_value = [mask,R,C,exterior_value,Channel](int64_t r, int64_t c) -> float {
                        float out;
                        if( (r == 0) || (r == (R+1))
                        ||  (c == 0) || (c == (C+1)) ){
                            out = exterior_value;
                        }else{
                            out = mask.value(r-1, c-1, Channel);
                        }
                        return out;
                    };
                    const auto get_position = [&mask,R,C,exterior_value,Channel](int64_t r, int64_t c){
                        return (  mask.anchor
                                + mask.offset
                                + mask.row_unit*(mask.pxl_dx*static_cast<double>(c-1))
                                + mask.col_unit*(mask.pxl_dy*static_cast<double>(r-1)) );
                    };

                    for(int64_t r = 0; r < (R+1); ++r){
                        for(int64_t c = 0; c < (C+1); ++c){
                            const auto tl = get_value(r  , c  );
                            const auto tr = get_value(r  , c+1);
                            const auto br = get_value(r+1, c+1);
                            const auto bl = get_value(r+1, c  );

                            const auto pos_tl = get_position(r  , c  );
                            const auto pos_tr = get_position(r  , c+1);
                            const auto pos_br = get_position(r+1, c+1);
                            const auto pos_bl = get_position(r+1, c  );

                            const auto TL = pixel_oracle(tl);
                            const auto TR = pixel_oracle(tr);
                            const auto BR = pixel_oracle(br);
                            const auto BL = pixel_oracle(bl);

                            // See enumerated cases visualized at https://en.wikipedia.org/wiki/File:Marching_squares_algorithm.svg (circa 20210604).
                            const auto i = index(r, c);
                            auto n_ptr = &(nodes.at(i));
                            if( false ){
                            }else if(  TL &&  TR &&  BL &&  BR ){// case 0
                                // No crossing. Do nothing.
                            }else if(  TL &&  TR && !BL &&  BR ){// case 1
                                const auto tail = interpolate_pos(tl, pos_tl,  bl, pos_bl);
                                const auto head = interpolate_pos(bl, pos_bl,  br, pos_br);
                                *n_ptr = std::make_pair(node(o_l, o_b, tail, head), empty_node);
                            }else if(  TL &&  TR &&  BL && !BR ){// case 2
                                const auto tail = interpolate_pos(bl, pos_bl,  br, pos_br);
                                const auto head = interpolate_pos(br, pos_br,  tr, pos_tr);
                                *n_ptr = std::make_pair( node(o_b, o_r, tail, head), empty_node);
                            }else if(  TL &&  TR && !BL && !BR ){// case 3
                                const auto tail = interpolate_pos(tl, pos_tl,  bl, pos_bl);
                                const auto head = interpolate_pos(tr, pos_tr,  br, pos_br);
                                *n_ptr = std::make_pair( node(o_l, o_r, tail, head), empty_node);
                            }else if(  TL && !TR &&  BL &&  BR ){// case 4
                                const auto tail = interpolate_pos(tr, pos_tr,  br, pos_br);
                                const auto head = interpolate_pos(tl, pos_tl,  tr, pos_tr);
                                *n_ptr = std::make_pair( node(o_r, o_t, tail, head), empty_node);
                            }else if(  TL && !TR && !BL &&  BR ){// case 5
                                // Ambiguous saddle-point case. Sample the centre of the voxel to disambiguate.
                                const auto centre = (tl * 0.25 + tr * 0.25 + br * 0.25 + bl * 0.25);
                                const auto C = pixel_oracle(centre);
                                if(C == TL){
                                    const auto tail1 = interpolate_pos(tl, pos_tl,  bl, pos_bl);
                                    const auto head1 = interpolate_pos(bl, pos_bl,  br, pos_br);

                                    const auto tail2 = interpolate_pos(br, pos_br,  tr, pos_tr);
                                    const auto head2 = interpolate_pos(tl, pos_tl,  tr, pos_tr);
                                    *n_ptr = std::make_pair( node(o_l, o_b, tail1, head1),
                                                             node(o_r, o_t, tail2, head2));
                                }else{
                                    const auto tail1 = interpolate_pos(bl, pos_bl,  tl, pos_tl);
                                    const auto head1 = interpolate_pos(tl, pos_tl,  tr, pos_tr);

                                    const auto tail2 = interpolate_pos(tr, pos_tr,  br, pos_br);
                                    const auto head2 = interpolate_pos(bl, pos_bl,  br, pos_br);
                                    *n_ptr = std::make_pair( node(o_l, o_t, tail1, head1),
                                                             node(o_r, o_b, tail2, head2));
                                }
                            }else if(  TL && !TR &&  BL && !BR ){// case 6
                                const auto tail = interpolate_pos(bl, pos_bl,  br, pos_br);
                                const auto head = interpolate_pos(tl, pos_tl,  tr, pos_tr);
                                *n_ptr = std::make_pair( node(o_b, o_t, tail, head), empty_node);
                            }else if(  TL && !TR && !BL && !BR ){// case 7
                                const auto tail = interpolate_pos(tl, pos_tl,  bl, pos_bl);
                                const auto head = interpolate_pos(tl, pos_tl,  tr, pos_tr);
                                *n_ptr = std::make_pair( node(o_l, o_t, tail, head), empty_node);
                            }else if( !TL &&  TR &&  BL &&  BR ){// case 8
                                const auto tail = interpolate_pos(tl, pos_tl,  tr, pos_tr);
                                const auto head = interpolate_pos(tl, pos_tl,  bl, pos_bl);
                                *n_ptr = std::make_pair( node(o_t, o_l, tail, head), empty_node);
                            }else if( !TL &&  TR && !BL &&  BR ){// case 9
                                const auto tail = interpolate_pos(tl, pos_tl,  tr, pos_tr);
                                const auto head = interpolate_pos(bl, pos_bl,  br, pos_br);
                                *n_ptr = std::make_pair( node(o_t, o_b, tail, head), empty_node);
                            }else if( !TL &&  TR &&  BL && !BR ){// case 10
                                // Ambiguous saddle-point case. Sample the centre of the voxel to disambiguate.
                                const auto centre = (tl * 0.25 + tr * 0.25 + br * 0.25 + bl * 0.25);
                                const auto C = pixel_oracle(centre);
                                if(C == TL){
                                    const auto tail1 = interpolate_pos(bl, pos_bl,  br, pos_br);
                                    const auto head1 = interpolate_pos(bl, pos_bl,  tl, pos_tl);

                                    const auto tail2 = interpolate_pos(tr, pos_tr,  tl, pos_tl);
                                    const auto head2 = interpolate_pos(tr, pos_tr,  br, pos_br);
                                    *n_ptr = std::make_pair( node(o_b, o_l, tail1, head1),
                                                             node(o_t, o_r, tail2, head2));
                                }else{
                                    const auto tail1 = interpolate_pos(bl, pos_bl,  br, pos_br);
                                    const auto head1 = interpolate_pos(br, pos_br,  tr, pos_tr);

                                    const auto tail2 = interpolate_pos(tr, pos_tr,  tl, pos_tl);
                                    const auto head2 = interpolate_pos(tl, pos_tl,  bl, pos_bl);
                                    *n_ptr = std::make_pair( node(o_b, o_r, tail1, head1),
                                                             node(o_t, o_l, tail2, head2));
                                }
                            }else if( !TL &&  TR && !BL && !BR ){// case 11
                                const auto tail = interpolate_pos(tr, pos_tr,  tl, pos_tl);
                                const auto head = interpolate_pos(tr, pos_tr,  br, pos_br);
                                *n_ptr = std::make_pair( node(o_t, o_r, tail, head), empty_node);
                            }else if( !TL && !TR &&  BL &&  BR ){// case 12
                                const auto tail = interpolate_pos(br, pos_br,  tr, pos_tr);
                                const auto head = interpolate_pos(bl, pos_bl,  tl, pos_tl);
                                *n_ptr = std::make_pair( node(o_r, o_l, tail, head), empty_node);
                            }else if( !TL && !TR && !BL &&  BR ){// case 13
                                const auto tail = interpolate_pos(br, pos_br,  tr, pos_tr);
                                const auto head = interpolate_pos(bl, pos_bl,  br, pos_br);
                                *n_ptr = std::make_pair( node(o_r, o_b, tail, head), empty_node);
                            }else if( !TL && !TR &&  BL && !BR ){// case 14
                                const auto tail = interpolate_pos(bl, pos_bl,  br, pos_br);
                                const auto head = interpolate_pos(tl, pos_tl,  bl, pos_bl);
                                *n_ptr = std::make_pair( node(o_b, o_l, tail, head), empty_node);
                            }else if( !TL && !TR && !BL && !BR ){// case 15 
                                // No crossing. Do nothing.
                            }
                        }
                    }

                    //Walk all available half-edges forming contour perimeters.
                    std::list<contour_of_points<double>> copl;
                    for(int64_t r = 0; r < (R+1); ++r){
                        for(int64_t c = 0; c < (C+1); ++c){
                            const auto i = index(r, c);
                            for(auto n1_ptr : { &(nodes.at(i).first), &(nodes.at(i).second) }){
                            
                                // Search for a valid contour edge.
                                if( (n1_ptr->head_vert_pos == 0)
                                ||  (n1_ptr->tail_vert_pos == 0) ){
                                    continue;
                                }
                                int64_t curr_r = r;
                                int64_t curr_c = c;

                                // Determine which node to look for next.
                                const auto find_next_node = [&](const node& n_curr){
                                    node* n_next_ptr = nullptr;
                                    
                                    const auto n_curr_head_vert_pos = n_curr.head_vert_pos;
                                    if(n_curr_head_vert_pos != 0){

                                        uint8_t n_next_tail_vert_pos = 0;
                                        auto n_next_r = curr_r;
                                        auto n_next_c = curr_c;
                                        if(false){
                                        }else if(n_curr_head_vert_pos == o_t){
                                            n_next_r -= 1;
                                            n_next_tail_vert_pos = o_b;
                                        }else if(n_curr_head_vert_pos == o_b){
                                            n_next_r += 1;
                                            n_next_tail_vert_pos = o_t;
                                        }else if(n_curr_head_vert_pos == o_l){
                                            n_next_c -= 1;
                                            n_next_tail_vert_pos = o_r;
                                        }else if(n_curr_head_vert_pos == o_r){
                                            n_next_c += 1;
                                            n_next_tail_vert_pos = o_l;
                                        }else{
                                            throw std::runtime_error("Invalid tail vert position encountered");
                                        }

                                        const auto n_next_i = index(n_next_r, n_next_c);
                                        const bool i_valid = isininc(0,n_next_r,R) && isininc(0,n_next_c,C);
                                        if( i_valid 
                                        &&  (nodes.at(n_next_i).first.tail_vert_pos == n_next_tail_vert_pos) ){
                                            curr_r = n_next_r;
                                            curr_c = n_next_c;
                                            n_next_ptr = &(nodes.at(n_next_i).first);
                                        }
                                        if( i_valid
                                        &&  (nodes.at(n_next_i).second.tail_vert_pos == n_next_tail_vert_pos) ){
                                            curr_r = n_next_r;
                                            curr_c = n_next_c;
                                            n_next_ptr = &(nodes.at(n_next_i).second);
                                        }
                                    }
                                    return n_next_ptr;
                                };

                                const auto find_prev_node = [&](const node& n_curr){
                                    node* n_prev_ptr = nullptr;
                                    
                                    const auto n_curr_tail_vert_pos = n_curr.tail_vert_pos;
                                    if(n_curr_tail_vert_pos != 0){

                                        uint8_t n_prev_head_vert_pos = 0;
                                        auto n_prev_r = curr_r;
                                        auto n_prev_c = curr_c;
                                        if(false){
                                        }else if(n_curr_tail_vert_pos == o_t){
                                            n_prev_r -= 1;
                                            n_prev_head_vert_pos = o_b;
                                        }else if(n_curr_tail_vert_pos == o_b){
                                            n_prev_r += 1;
                                            n_prev_head_vert_pos = o_t;
                                        }else if(n_curr_tail_vert_pos == o_l){
                                            n_prev_c -= 1;
                                            n_prev_head_vert_pos = o_r;
                                        }else if(n_curr_tail_vert_pos == o_r){
                                            n_prev_c += 1;
                                            n_prev_head_vert_pos = o_l;
                                        }else{
                                            throw std::runtime_error("Invalid head vert position encountered");
                                        }

                                        const auto n_prev_i = index(n_prev_r, n_prev_c);
                                        const bool i_valid = isininc(0,n_prev_r,R) && isininc(0,n_prev_c,C);
                                        if( i_valid
                                        && (nodes.at(n_prev_i).first.head_vert_pos == n_prev_head_vert_pos) ){
                                            curr_r = n_prev_r;
                                            curr_c = n_prev_c;
                                            n_prev_ptr = &(nodes.at(n_prev_i).first);
                                        }
                                        if( i_valid
                                        &&  (nodes.at(n_prev_i).second.head_vert_pos == n_prev_head_vert_pos) ){
                                            curr_r = n_prev_r;
                                            curr_c = n_prev_c;
                                            n_prev_ptr = &(nodes.at(n_prev_i).second);
                                        }
                                    }
                                    return n_prev_ptr;
                                };


                                // If a valid contour edge was found, start following along the contour.
                                copl.emplace_back();
                                copl.back().closed = true;
                                copl.back().metadata = cm;
                                copl.back().metadata["ROIName"] = ROILabel;
                                copl.back().metadata["NormalizedROIName"] = NormalizedROILabel;
                                copl.back().metadata["Description"] = "Contoured via threshold ("_s + std::to_string(Lower)
                                                                     + " <= pixel_val <= " + std::to_string(Upper) + ")";
                                copl.back().metadata["ROINumber"] = std::to_string(10000); // TODO: find highest existing and ++ it.
                                copl.back().metadata["MinimumSeparation"] = std::to_string(MinimumSeparation);
                                copy_overwrite(animg_ptr->metadata, copl.back().metadata, "SOPClassUID", "ReferencedSOPClassUID"); 
                                copy_overwrite(animg_ptr->metadata, copl.back().metadata, "SOPInstanceUID", "ReferencedSOPInstanceUID"); 

                                copl.back().points.push_back( n1_ptr->tail );

                                const auto n1_tail_vert_pos = n1_ptr->tail_vert_pos;
                                const auto n1_head_vert_pos = n1_ptr->head_vert_pos;
                                node* n_curr_ptr = n1_ptr;
                                int64_t loop_counter = 0L;
                                do{
                                    auto n_next_ptr = find_next_node(*n_curr_ptr);
                                    //*n_curr_ptr = empty_node;
                                    n_curr_ptr->tail_vert_pos = 0;
                                    n_curr_ptr->head_vert_pos = 0;

                                    if(n_next_ptr == nullptr) break; // End of the contour, or image boundary.
                                    copl.back().points.push_front( n_next_ptr->tail );
                                    n_curr_ptr = n_next_ptr;

                                    ++loop_counter;
                                    if( (loop_counter % 100'000L) == 0L ){
                                        YLOGWARN("Loop A iteration " << loop_counter);
                                    }
                                }while(true);

                                // Jump back to the original contour edge, reset it, and walk backwards.
                                n1_ptr->tail_vert_pos = n1_tail_vert_pos;
                                n1_ptr->head_vert_pos = n1_head_vert_pos;
                                curr_r = r;
                                curr_c = c;
                                n_curr_ptr = n1_ptr;
                                loop_counter = 0L;
                                do{
                                    auto n_prev_ptr = find_prev_node(*n_curr_ptr);
                                    //*n_curr_ptr = empty_node;
                                    n_curr_ptr->tail_vert_pos = 0;
                                    n_curr_ptr->head_vert_pos = 0;

                                    if(n_prev_ptr == nullptr) break; // End of the contour, or image boundary.
                                    copl.back().points.push_back( n_prev_ptr->head );
                                    n_curr_ptr = n_prev_ptr;

                                    ++loop_counter;
                                    if( (loop_counter % 100'000L) == 0L ){
                                        YLOGWARN("Loop B iteration " << loop_counter);
                                    }
                                }while(true);

                                // Nullify the original contour edge so it cannot be used again.
                                n1_ptr->tail_vert_pos = 0;
                                n1_ptr->head_vert_pos = 0;
                            }
                        }
                    }

                    //Save the contours and print some information to screen.
                    {
                        std::lock_guard<std::mutex> lock(saver_printer);
                        DICOM_data.contour_data->ccs.back().contours.splice(DICOM_data.contour_data->ccs.back().contours.end(), copl);

                        ++completed;
                        YLOGINFO("Completed " << completed << " of " << img_count
                              << " --> " << static_cast<int>(1000.0*(completed)/img_count)/10.0 << "% done");
                    }
#ifdef DCMA_USE_CGAL
                // ---------------------------------------------------
                // The marching cubes method.
                }else if(std::regex_match(MethodStr, marching_cubes_regex)){

                    //Prepare a mask image for contouring.
                    auto mask = *animg_ptr;
                    double inclusion_threshold = std::numeric_limits<double>::quiet_NaN();
                    double exterior_value = std::numeric_limits<double>::quiet_NaN();
                    bool below_is_interior = false;

                    if(std::isfinite(cl) && std::isfinite(cu)){
                        // Transform voxels by their |distance| from the midpoint. Only interior voxels will be within
                        // [0,width*0.5], and all others will be (width*0.5,inf).
                        const double midpoint = (cl + cu) * 0.5;
                        const double width = (cu - cl);

                        inclusion_threshold = width * 0.5;
                        exterior_value = inclusion_threshold + 1.0;
                        below_is_interior = true;
                        mask.apply_to_pixels([Channel,midpoint](int64_t, int64_t, int64_t chnl, float &val) -> void {
                                if(Channel == chnl){
                                    val = std::abs(val - midpoint);
                                }
                                return;
                            });

                    }else if(std::isfinite(cl)){
                        inclusion_threshold = cl;
                        exterior_value = inclusion_threshold - 1.0;
                        below_is_interior = false;

                    }else if(std::isfinite(cu)){
                        inclusion_threshold = cu;
                        exterior_value = inclusion_threshold + 1.0;
                        below_is_interior = true;

                    }else{ // Neither threshold is finite.
                        throw std::invalid_argument("Unable to discern finite threshold for meshing. Refusing to continue.");
                        // Note: it is possible to deal with these cases and generate meshings for each, i.e., either all
                        // voxels are included or no voxels are included (both are valid meshings). However, it seems
                        // most likely this is a user error. Add this functionality if necessary.

                    }

                    // Sandwich the mask with images that have no voxels included to give the mesh a valid pxl_dz to
                    // work with.
                    const auto N_0 = mask.image_plane().N_0;
                    auto above = *animg_ptr;
                    auto below = *animg_ptr;
                    above.fill_pixels(exterior_value);
                    below.fill_pixels(exterior_value);
                    above.offset += N_0 * mask.pxl_dz;
                    below.offset -= N_0 * mask.pxl_dz;

                    std::list<std::reference_wrapper<planar_image<float,double>>> grid_imgs;
                    grid_imgs = {{ 
                        std::ref(above),
                        std::ref(mask),
                        std::ref(below) }};

                    // Generate the surface mesh.
                    auto meshing_params = dcma_surface_meshes::Parameters();
                    meshing_params.MutateOpts.inclusivity = Mutate_Voxels_Opts::Inclusivity::Centre;
                    meshing_params.MutateOpts.contouroverlap = Mutate_Voxels_Opts::ContourOverlap::Ignore;
                    YLOGWARN("Ignoring contour orientations; assuming ROI polyhderon is simple");
                    auto surface_mesh = dcma_surface_meshes::Estimate_Surface_Mesh_Marching_Cubes( 
                                                                    grid_imgs,
                                                                    inclusion_threshold, 
                                                                    below_is_interior,
                                                                    meshing_params );
                    auto polyhedron = dcma_surface_meshes::FVSMeshToPolyhedron( surface_mesh );

                    // Slice the mesh along the image plane.
                    auto lcc = polyhedron_processing::Slice_Polyhedron( polyhedron,
                                                                        {{ mask.image_plane() }} );

                    // Tag the contours with metadata.
                    for(auto &cop : lcc.contours){
                        cop.closed = true;
                        cop.metadata = cm;
                        cop.metadata["ROIName"] = ROILabel;
                        cop.metadata["NormalizedROIName"] = NormalizedROILabel;
                        cop.metadata["Description"] = "Contoured via threshold ("_s + LowerStr
                                                     + " <= pixel_val <= " + UpperStr + ")";
                        cop.metadata["MinimumSeparation"] = std::to_string(MinimumSeparation);
                        cop.metadata["ROINumber"] = std::to_string(10000); // TODO: find highest existing and ++ it.
                        copy_overwrite(animg_ptr->metadata, cop.metadata, "SOPClassUID", "ReferencedSOPClassUID"); 
                        copy_overwrite(animg_ptr->metadata, cop.metadata, "SOPInstanceUID", "ReferencedSOPInstanceUID"); 
                    }

                    // Try to simplify the contours as much as possible.
                    //
                    // The following will straddle each vertex, interpolate the adjacent vertices, and compare the
                    // interpolated vertex to the actual (straddled) vertex. If they differ by a small amount, the straddled
                    // vertex is pruned. 1% should be more than enough to account for numerical fluctuations.
                    /*
                    if(SimplifyMergeAdjacent){
                        const auto tolerance_sq_dist = 0.01*(animg_ptr->pxl_dx*animg_ptr->pxl_dx + animg_ptr->pxl_dy*animg_ptr->pxl_dy + animg_ptr->pxl_dz*animg_ptr->pxl_dz);
                        const auto verts_are_equal = [=](const vec3<double> &A, const vec3<double> &B) -> bool {
                            return A.sq_dist(B) < tolerance_sq_dist;
                        };
                        for(auto &cop : lcc.contours){
                            cop.Remove_Sequential_Duplicate_Points(verts_are_equal);
                            cop.Remove_Extraneous_Points(verts_are_equal);
                        }
                    }
                    */

                    // Save the contours and print some information to screen.
                    {
                        std::lock_guard<std::mutex> lock(saver_printer);
                        DICOM_data.contour_data->ccs.back().contours.splice(DICOM_data.contour_data->ccs.back().contours.end(),
                                                                            lcc.contours);

                        ++completed;
                        YLOGINFO("Completed " << completed << " of " << img_count
                              << " --> " << static_cast<int>(1000.0*(completed)/img_count)/10.0 << "% done");
                    }
#endif //DCMA_USE_CGAL

                }else{
                    throw std::invalid_argument("The contouring method is not understood. Cannot continue.");
                }

            }); // thread pool task closure.
        }
    }

    return true;
}
