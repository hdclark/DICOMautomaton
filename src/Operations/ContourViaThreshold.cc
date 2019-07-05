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
        " There are two methods of contour generation available:"
        " a simple binary method in which voxels are either fully in or fully out of the contour,"
        " and a method based on marching cubes that will provide smoother contours.";
        
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
    out.args.back().name = "Method";
    out.args.back().desc = "There are currently two supported methods for generating contours:"
                           " (1) a simple (and fast) binary inclusivity checker, that simply checks if a voxel is within"
                           " the ROI by testing the value at the voxel centre, and (2) a robust (but slow) method based"
                           " on marching cubes. The binary method is fast, but produces extremely jagged contours."
                           " It may also have problems with 'pinches' and topological consistency."
                           " The marching method is more robust and should reliably produce contours for even"
                           " the most complicated topologies, but is considerably slower than the binary method.";
    out.args.back().default_val = "binary";
    out.args.back().expected = true;
    out.args.back().examples = { "binary",
                                 "marching" };

    
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
    const auto marching_regex = Compile_Regex("^ma?r?c?h?i?n?g?$");

    const auto TrueRegex = Compile_Regex("^tr?u?e?$");

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
        const long int img_count = (*iap_it)->imagecoll.images.size();

        asio_thread_pool tp;
        std::mutex saver_printer; // Who gets to save generated contours, print to the console, and iterate the counter.
        long int completed = 0;

        //Determine the bounds in terms of pixel-value thresholds.
        auto cl = Lower; // Will be replaced if percentages/percentiles requested.
        auto cu = Upper; // Will be replaced if percentages/percentiles requested.
        {
            //Percentage-based.
            if(Lower_is_Percent || Upper_is_Percent){
                Stats::Running_MinMax<float> rmm;
                for(const auto &animg : (*iap_it)->imagecoll.images){
                    animg.apply_to_pixels([&rmm,Channel](long int, long int, long int chnl, float val) -> void {
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
                    animg.apply_to_pixels([&pixel_vals,Channel](long int, long int, long int chnl, float val) -> void {
                         if(Channel == chnl) pixel_vals.push_back(val);
                         return;
                    });
                }
                if(Lower_is_Ptile) cl = Stats::Percentile(pixel_vals, Lower / 100.0);
                if(Upper_is_Ptile) cu = Stats::Percentile(pixel_vals, Upper / 100.0);
            }
        }
        if(cl > cu){
            throw std::invalid_argument("Thresholds conflict. Mesh will contain zero faces. Refusing to continue.");
        }

        //Construct a pixel 'oracle' closure using the user-specified threshold criteria. This function identifies whether
        //the pixel is within (true) or outside of (false) the final ROI.
        auto pixel_oracle = [cu,cl](float p) -> bool {
            return (cl <= p) && (p <= cu);
        };

        for(const auto &animg : (*iap_it)->imagecoll.images){
            if( (animg.rows < 1) || (animg.columns < 1) || (Channel >= animg.channels) ){
                throw std::runtime_error("Image or channel is empty -- cannot contour via thresholds.");
            }
            tp.submit_task([&](void) -> void {

                // ---------------------------------------------------
                // The binary inclusivity method.
                if(false){
                }else if(std::regex_match(MethodStr, binary_regex)){

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
                            copl.back().metadata["ROIName"] = ROILabel;
                            copl.back().metadata["NormalizedROIName"] = NormalizedROILabel;
                            copl.back().metadata["Description"] = "Contoured via threshold ("_s + std::to_string(Lower)
                                                                 + " <= pixel_val <= " + std::to_string(Upper) + ")";
                            copl.back().metadata["MinimumSeparation"] = MinimumSeparation;
                            for(const auto &key : { "StudyInstanceUID", "FrameofReferenceUID" }){
                                if(animg.metadata.count(key) != 0) copl.back().metadata[key] = animg.metadata.at(key);
                            }

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
                        const auto tolerance_sq_dist = 0.01*(animg.pxl_dx*animg.pxl_dx + animg.pxl_dy*animg.pxl_dy + animg.pxl_dz*animg.pxl_dz);
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
                        FUNCINFO("Completed " << completed << " of " << img_count
                              << " --> " << static_cast<int>(1000.0*(completed)/img_count)/10.0 << "\% done");
                    }

                // ---------------------------------------------------
                // The marching cubes method.
                }else if(std::regex_match(MethodStr, marching_regex)){

                    //Prepare a mask image for contouring.
                    auto mask = animg;
                    double inclusion_threshold = std::numeric_limits<double>::quiet_NaN();
                    double exterior_value = std::numeric_limits<double>::quiet_NaN();
                    bool below_is_interior = false;

                    if(false){
                    }else if(std::isfinite(cl) && std::isfinite(cu)){
                        // Transform voxels by their |distance| from the midpoint. Only interior voxels will be within
                        // [0,width*0.5], and all others will be (width*0.5,inf).
                        const double midpoint = (cl + cu) * 0.5;
                        const double width = (cu - cl);

                        inclusion_threshold = width * 0.5;
                        exterior_value = inclusion_threshold + 1.0;
                        below_is_interior = true;
                        mask.apply_to_pixels([Channel,midpoint](long int, long int, long int chnl, float &val) -> void {
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

                    // Generate the surface mesh.
                    auto meshing_params = dcma_surface_meshes::Parameters();
                    meshing_params.MutateOpts.inclusivity = Mutate_Voxels_Opts::Inclusivity::Centre;
                    meshing_params.MutateOpts.contouroverlap = Mutate_Voxels_Opts::ContourOverlap::Ignore;
                    FUNCWARN("Ignoring contour orientations; assuming ROI polyhderon is simple");
                    auto surface_mesh = dcma_surface_meshes::Estimate_Surface_Mesh_Marching_Cubes( 
                                                                    grid_imgs,
                                                                    inclusion_threshold, 
                                                                    below_is_interior,
                                                                    meshing_params );

                    // Slice the mesh along the image plane.
                    auto lcc = polyhedron_processing::Slice_Polyhedron( surface_mesh, 
                                                                        {{ mask.image_plane() }} );

                    // Tag the contours with metadata.
                    for(auto &cop : lcc.contours){
                        cop.closed = true;
                        cop.metadata["ROIName"] = ROILabel;
                        cop.metadata["NormalizedROIName"] = NormalizedROILabel;
                        cop.metadata["Description"] = "Contoured via threshold ("_s + LowerStr
                                                     + " <= pixel_val <= " + UpperStr + ")";
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
                    /*
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
                    */

                    // Save the contours and print some information to screen.
                    {
                        std::lock_guard<std::mutex> lock(saver_printer);
                        DICOM_data.contour_data->ccs.back().contours.splice(DICOM_data.contour_data->ccs.back().contours.end(),
                                                                            lcc.contours);

                        ++completed;
                        FUNCINFO("Completed " << completed << " of " << img_count
                              << " --> " << static_cast<int>(1000.0*(completed)/img_count)/10.0 << "\% done");
                    }

                }else{
                    throw std::invalid_argument("The contouring method is not understood. Cannot continue.");
                }

            }); // thread pool task closure.
        }
    }

    DICOM_data.contour_data->ccs.back().Raw_ROI_name = ROILabel;
    return DICOM_data;
}
