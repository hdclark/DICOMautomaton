//ConvertImageToMeshes.cc - A part of DICOMautomaton 2019. Written by hal clark.

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

#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorLog.h"
#include "YgorStats.h"        //Needed for Stats:: namespace.
#include "YgorString.h"       //Needed for GetFirstRegex(...)
#include "YgorMathIOOFF.h"

#include "Explicator.h"       //Needed for Explicator class.

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../Thread_Pool.h"

#include "ConvertImageToMeshes.h"

#include "../Surface_Meshes.h"

OperationDoc OpArgDocConvertImageToMeshes(){
    OperationDoc out;
    out.name = "ConvertImageToMeshes";

    out.desc = 
        "This operation extracts surface meshes from images and pixel/voxel value thresholds."
        " Meshes are appended to the back of the Surface_Mesh stack."
        " There are three methods of mesh extract available:"
        " (1) a simple 'binary' method in which voxels are either fully in or fully out of the contour,"
        " (2) a method based on 'marching' cubes that will provide smoother contours, and"
        " (3) a purely 'geometrical' method that extracts only the shape and extent of images but"
        " does not use the pixel intensities."
        " Both pixel-based methods (2) and (3) make use of marching cubes -- the binary method involves pre-processing.";
        
    out.notes.emplace_back(
        "This routine requires images to be regular (i.e., exactly abut nearest adjacent images without"
        " any overlap)."
    );

    out.args.emplace_back();
    out.args.back() = IAWhitelistOpArgDoc();
    out.args.back().name = "ImageSelection";
    out.args.back().default_val = "last";
   

    out.args.emplace_back();
    out.args.back().name = "Lower";
    out.args.back().desc = "The lower bound (inclusive). Pixels with values < this number are excluded from the ROI."
                           " If the number is followed by a '%', the bound will be scaled between the min and max"
                           " pixel values [0-100%]. If the number is followed by 'tile', the bound will be replaced"
                           " with the corresponding percentile [0-100tile]."
                           " Note that upper and lower bounds can be specified separately (e.g., lower bound is a"
                           " percentage, but upper bound is a percentile)."
                           " Note that computed bounds (i.e., percentages and percentiles) consider the entire"
                           " image volume.";
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
                           " percentage, but upper bound is a percentile)."
                           " Note that computed bounds (i.e., percentages and percentiles) consider the entire"
                           " image volume.";
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
    out.args.back().name = "Method";
    out.args.back().desc = "There are currently three supported methods for generating meshes:"
                           "\n\n"
                           "1. A simple (and fast) binary inclusivity checker, that simply checks if a voxel is within"
                           " an ROI by testing the value at the voxel centre. This method is fast, but produces"
                           " extremely jagged contours."
                           " It may also have problems with 'pinches' and topological consistency."
                           "\n\n"
                           "2. A robust (but comparatively slower) method based on marching cubes."
                           " This method is more robust than the binary method and should reliably produce meshes for even"
                           " the most complicated topologies. It is expected to run slower than the binary method."
                           "\n\n"
                           "3. A method that only extracts the geometrical aspects of images, including"
                           " orientation, position, and spatial extent. This method does not use pixel intensities."
                           " It is useful for inspecting or debugging spatial alignment.";
    out.args.back().default_val = "marching";
    out.args.back().expected = true;
    out.args.back().examples = { "binary",
                                 "marching",
                                 "geometrical" };
    out.args.back().samples = OpArgSamples::Exhaustive;

    
    out.args.emplace_back();
    out.args.back().name = "MeshLabel";
    out.args.back().desc = "A label to attach to the surface mesh.";
    out.args.back().default_val = "unspecified";
    out.args.back().expected = true;
    out.args.back().examples = { "unspecified", "body", "air", "bone", "invalid", "above_zero", "below_5.3" };

    return out;
}



bool ConvertImageToMeshes(Drover &DICOM_data,
                            const OperationArgPkg& OptArgs,
                            std::map<std::string, std::string>& /*InvocationMetadata*/,
                            const std::string& FilenameLex){

    Explicator X(FilenameLex);

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();
    const auto LowerStr = OptArgs.getValueStr("Lower").value();
    const auto UpperStr = OptArgs.getValueStr("Upper").value();
    const auto ChannelStr = OptArgs.getValueStr("Channel").value();
    const auto MethodStr = OptArgs.getValueStr("Method").value();
    const auto MeshLabel = OptArgs.getValueStr("MeshLabel").value();

    //-----------------------------------------------------------------------------------------------------------------
    const auto NormalizedMeshLabel = X(MeshLabel);

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
    const auto geom_regex = Compile_Regex("^ge?o?m?e?t?r?[iy]?c?a?l?$");

    //Iterate over each requested image_array. Each image is processed independently, so a thread pool is used.
    auto IAs_all = All_IAs( DICOM_data );
    auto IAs = Whitelist( IAs_all, ImageSelectionStr );
    for(auto & iap_it : IAs){
        // The mesh will inheret image metadata.
        auto ia_metadata = (*iap_it)->imagecoll.get_common_metadata({});

        // Pixel-based methods.
        if( std::regex_match(MethodStr, binary_regex)
        ||  std::regex_match(MethodStr, marching_regex) ){
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
            // the pixel is within (true) or outside of (false) the final ROI. This is used for the binary method.
            auto pixel_oracle = [cu,cl](float p) -> bool {
                return (cl <= p) && (p <= cu);
            };

            double inclusion_threshold = std::numeric_limits<double>::quiet_NaN();
            double exterior_value = std::numeric_limits<double>::quiet_NaN();
            bool below_is_interior = false;

            std::list<planar_image<float,double>> masks;
            for(auto &animg : (*iap_it)->imagecoll.images){
                if( (animg.rows < 1) || (animg.columns < 1) || (Channel >= animg.channels) ){
                    throw std::runtime_error("Image or channel is empty -- cannot generate surface mesh.");
                }

                //Prepare a mask image for contouring.
                masks.push_back(animg);
                
                if(std::regex_match(MethodStr, binary_regex)){
                    inclusion_threshold = 0.0;
                    below_is_interior = true;
                    exterior_value = 1.0;
                    const auto interior_value = -exterior_value;
                    masks.back().apply_to_pixels([pixel_oracle,
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

                }else if(std::regex_match(MethodStr, marching_regex)){
                    if(std::isfinite(cl) && std::isfinite(cu)){
                        // Transform voxels by their |distance| from the midpoint. Only interior voxels will be within
                        // [0,width*0.5], and all others will be (width*0.5,inf).
                        const double midpoint = (cl + cu) * 0.5;
                        const double width = (cu - cl);

                        inclusion_threshold = width * 0.5;
                        exterior_value = inclusion_threshold + 1.0;
                        below_is_interior = true;
                        masks.back().apply_to_pixels([Channel,midpoint](long int, long int, long int chnl, float &val) -> void {
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

                }else{
                    throw std::invalid_argument("Meshing method not recognized. Refusing to continue.");
                }
            }


            // Generate the surface mesh.
            std::list<std::reference_wrapper<planar_image<float,double>>> mask_imgs;
            for(auto &amask : masks){
                mask_imgs.push_back(std::ref(amask));
            }
            // Note: meshing parameter MutateOpts are irrelevant since we supply our own mask.
            auto meshing_params = dcma_surface_meshes::Parameters();
            auto output_mesh = dcma_surface_meshes::Estimate_Surface_Mesh_Marching_Cubes( 
                                                            mask_imgs,
                                                            inclusion_threshold, 
                                                            below_is_interior,
                                                            meshing_params );

            DICOM_data.smesh_data.emplace_back( std::make_unique<Surface_Mesh>() );
            DICOM_data.smesh_data.back()->meshes = output_mesh;

        // Geometrical methods.
        }else if( std::regex_match(MethodStr, geom_regex) ){
            auto sm = std::make_unique<Surface_Mesh>();
            
            for(const auto &animg : (*iap_it)->imagecoll.images){
                // Avoid creating degenerate meshes.
                bool isvalid =    animg.row_unit.isfinite()
                               && animg.col_unit.isfinite() 
                               && std::isfinite(animg.pxl_dx)
                               && std::isfinite(animg.pxl_dy)
                               && std::isfinite(animg.pxl_dz)
                               && (0 < animg.pxl_dx)
                               && (0 < animg.pxl_dy)
                               && (0 < animg.pxl_dz)
                               && (0 < animg.rows)
                               && (0 < animg.columns);
                const auto ortho_unit = isvalid ? animg.col_unit.Cross(animg.row_unit).unit() : animg.col_unit;
                if( !isvalid
                ||  !ortho_unit.isfinite() ){
                    YLOGWARN("Skipping image with no spatial extent");
                    continue;
                }

                const auto c = animg.corners2D();
                const auto orig_N_verts = static_cast<uint64_t>(sm->meshes.vertices.size());

                for(const auto& v : c) sm->meshes.vertices.push_back( v - ortho_unit * animg.pxl_dz * 0.5 );
                for(const auto& v : c) sm->meshes.vertices.push_back( v + ortho_unit * animg.pxl_dz * 0.5 );

                // Bottom.
                sm->meshes.faces.emplace_back( std::vector<uint64_t>{ orig_N_verts + 0, orig_N_verts + 3, orig_N_verts + 2 } );
                sm->meshes.faces.emplace_back( std::vector<uint64_t>{ orig_N_verts + 0, orig_N_verts + 2, orig_N_verts + 1 } );

                // Sides.
                sm->meshes.faces.emplace_back( std::vector<uint64_t>{ orig_N_verts + 0, orig_N_verts + 1, orig_N_verts + 5 } );
                sm->meshes.faces.emplace_back( std::vector<uint64_t>{ orig_N_verts + 0, orig_N_verts + 5, orig_N_verts + 4 } );
                sm->meshes.faces.emplace_back( std::vector<uint64_t>{ orig_N_verts + 1, orig_N_verts + 2, orig_N_verts + 6 } );
                sm->meshes.faces.emplace_back( std::vector<uint64_t>{ orig_N_verts + 1, orig_N_verts + 6, orig_N_verts + 5 } );
                sm->meshes.faces.emplace_back( std::vector<uint64_t>{ orig_N_verts + 2, orig_N_verts + 3, orig_N_verts + 7 } );
                sm->meshes.faces.emplace_back( std::vector<uint64_t>{ orig_N_verts + 2, orig_N_verts + 7, orig_N_verts + 6 } );
                sm->meshes.faces.emplace_back( std::vector<uint64_t>{ orig_N_verts + 3, orig_N_verts + 0, orig_N_verts + 4 } );
                sm->meshes.faces.emplace_back( std::vector<uint64_t>{ orig_N_verts + 3, orig_N_verts + 4, orig_N_verts + 7 } );

                // Top.
                sm->meshes.faces.emplace_back( std::vector<uint64_t>{ orig_N_verts + 4, orig_N_verts + 5, orig_N_verts + 6 } );
                sm->meshes.faces.emplace_back( std::vector<uint64_t>{ orig_N_verts + 4, orig_N_verts + 6, orig_N_verts + 7 } );

            }
            DICOM_data.smesh_data.emplace_back( std::move(sm) );

        }else{
            throw std::invalid_argument("Meshing method not recognized. Refusing to continue.");
        }

        DICOM_data.smesh_data.back()->meshes.metadata = ia_metadata;
        DICOM_data.smesh_data.back()->meshes.metadata["MeshLabel"] = MeshLabel;
        DICOM_data.smesh_data.back()->meshes.metadata["NormalizedMeshLabel"] = NormalizedMeshLabel;
        DICOM_data.smesh_data.back()->meshes.metadata["Description"] = "Extracted surface mesh";
    }

    return true;
}
