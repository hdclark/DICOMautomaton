//DumpImageMeshes.cc - A part of DICOMautomaton 2019. Written by hal clark.

#include <optional>
#include <fstream>
#include <functional>
#include <iomanip>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>    
#include <vector>

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../Colour_Maps.h"
#include "DumpImageMeshes.h"
#include "YgorFilesDirs.h"    //Needed for Does_File_Exist_And_Can_Be_Read(...), etc..
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorString.h"       //Needed for GetFirstRegex(...)



OperationDoc OpArgDocDumpImageMeshes(){
    OperationDoc out;
    out.name = "DumpImageMeshes";

    out.desc = 
        "This operation exports images as a 3D surface mesh model (structured ASCII Wavefront OBJ)"
        " that can be manipulated in various ways (e.g., stereographic projection)."
        " Note that the mesh will be a 3D depiction of the image(s) as they naturally are "
        " -- meshes will always be rectangular."
        " A companion material library file (MTL) assigns colours to each ROI based on the voxel intensity.";

    out.notes.emplace_back(
        "Each image is processed separately. Each mesh effectively produces a 2D relief map embedded into"
        " a 3D model that can be easily rendered to produce various effects (e.g., perspective, stereoscopy,"
        " extrusion, surface smoothing, etc.)."
    );

    out.args.emplace_back();
    out.args.back() = IAWhitelistOpArgDoc();
    out.args.back().name = "ImageSelection";
    out.args.back().default_val = "all";

    out.args.emplace_back();
    out.args.back().name = "OutBase";
    out.args.back().desc = "A base filename (or full path) in which to (over)write image mesh and"
                           " material library files. File formats are Wavefront Object (obj) and"
                           " Material Library (mtl). Every image will receive one unique and "
                           " sequentially-numbered obj and mtl file using this prefix.";
    out.args.back().default_val = "/tmp/dicomautomaton_dumpimagemeshes_";
    out.args.back().expected = true;
    out.args.back().examples = { "/tmp/image_mesh_", "./", "../model_" };
    //out.args.back().mimetype = "application/obj"; // "application/mtl";

    out.args.emplace_back();
    out.args.back().name = "HistogramBins";
    out.args.back().desc = "The number of equal-width bins pixel intensities should be grouped into."
                           " Binning is performed in order to more easily associate material properties with pixels."
                           " If pixel intensities were continuous, each pixel would receive its own material definition."
                           " This could result in enormous MTL files and wasted disk space. Binning solves this issue."
                           " However, if images are small or must be differentiated precisely consider using a"
                           " large number of bins. Otherwise 150-1000 bins should suffice for display purposes.";
    out.args.back().default_val = "255";
    out.args.back().expected = true;
    out.args.back().examples = { "10", "50", "100", "200", "500" };

    out.args.emplace_back();
    out.args.back().name = "MagnitudeAmplification";
    out.args.back().desc = "Pixel magnitudes (i.e., intensities) are scaled according to the image thickness, but"
                           " a small gap is left between meshes so that abutting images do not quite intersect"
                           " (this can cause non-manifold scenarios). However, if stackability is not a concern"
                           " then pixel magnitudes can be magnified to exaggerate the relief effect."
                           " A value of 1.0 provides no magnification. A value of 2.0 provides 2x magnification,"
                           " but note that the base of each pixel is slightly offset from the top to avoid top-bottom face"
                           " intersections, even when magnification is 0.0.";
    out.args.back().default_val = "1.0";
    out.args.back().expected = true;
    out.args.back().examples = { "0.75", "1.0", "2.0", "5.0", "75.6" };

    out.args.emplace_back();
    out.args.back().name = "Normalize";
    out.args.back().desc = "This parameter controls whether the model will be 'normalized,' which effectively makes"
                           " the outgoing model more consistent for all images. Currently this means centring the"
                           " model at (0,0,0), mapping the row and column directions to (1,0,0) and (0,1,0)"
                           " respectively, and scaling the image (respecting the aspect ratio) to fit within a"
                           " bounding square of size 100x100 (DICOM units; mm). If normalization is *not* used,"
                           " the image mesh will inherit the spatial characteristics of the image it is derived"
                           " from.";
    out.args.back().default_val = "false";
    out.args.back().expected = true;
    out.args.back().examples = { "true", "false" };

    return out;
}



Drover DumpImageMeshes(Drover DICOM_data, OperationArgPkg OptArgs, std::map<std::string,std::string>, std::string ){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();

    const auto OutBase = OptArgs.getValueStr("OutBase").value();

    const auto HistogramBins = std::stol( OptArgs.getValueStr("HistogramBins").value() );
    const auto MagnitudeAmplification = std::stod( OptArgs.getValueStr("MagnitudeAmplification").value() );

    const auto NormalizeStr = OptArgs.getValueStr("Normalize").value();

    //-----------------------------------------------------------------------------------------------------------------

    const auto regex_true = Compile_Regex("^tr?u?e?$");
    const auto ShouldNormalize = std::regex_match(NormalizeStr, regex_true);

    auto IAs_all = All_IAs( DICOM_data );
    auto IAs = Whitelist( IAs_all, ImageSelectionStr );
    for(const auto & iap_it : IAs){
        for(const auto & img : (*iap_it)->imagecoll.images){

            const auto DumpFileName = Get_Unique_Sequential_Filename(OutBase, 6, ".obj");
            const auto MTLFileName  = Get_Unique_Sequential_Filename(OutBase, 6, ".mtl");
            FUNCINFO("Using OBJ filename '" 
                     << DumpFileName 
                     << "' and MTL filename '"
                     << MTLFileName
                     << "'");

            // Determine the min and max pixel values.
            const auto mm = img.minmax();

            //Generate a Wavefront materials file to colour the contours differently.
            std::map<long int, std::string> mats; // List of materials defined.
            {
                std::ofstream FO(MTLFileName, std::fstream::out);

                const auto write_mat = [&](const std::string &name,
                                           const vec3<double> &c_amb,
                                           const vec3<double> &c_dif,
                                           const vec3<double> &c_spc,
                                           const double s_exp,
                                           const double trans) -> void {

                        FO << "newmtl " << name << "\n";
                        FO << "Ka " << c_amb.x << " " << c_amb.y << " " << c_amb.z << "\n"; //Ambient colour.
                        FO << "Kd " << c_dif.x << " " << c_dif.y << " " << c_dif.z << "\n"; //Diffuse colour.
                        FO << "Ks " << c_spc.x << " " << c_spc.y << " " << c_spc.z << "\n"; //Specular colour.
                        FO << "Ns " << s_exp << "\n"; //Specular exponent.
                        FO << "d " << trans << "\n"; //Transparency ("dissolved"); d=1 is fully opaque.
                        FO << "illum 2" << "\n"; //Illumination model (2 = "Colour on, Ambient on").
                        FO << std::endl;
                };

                //write_mat("Red", vec3<double>(R, G, B), vec3<double>(R, G, B), vec3<double>(R, G, B), 10.0, 0.9);
                // Create a colour for each histogram bin.
                for(long int i = 0; i <= HistogramBins; ++i){
                    const auto c = ColourMap_MorelandBlackBody( static_cast<double>(i)/static_cast<double>(HistogramBins) );
                    const auto name = "colour"_s + std::to_string(i);
                    mats[i] = name;
                    write_mat(name, 
                              vec3<double>(c.R, c.G, c.B),
                              vec3<double>(c.R, c.G, c.B),
                              vec3<double>(c.R, c.G, c.B),
                              10.0,
                              0.9 );
                }

                FO.flush();
            }

            //Dump the pixel data in a structured ASCII Wavefront OBJ format using native polygons.
            //
            // NOTE: This routine creates a single polygon for each contour. Some programs might not be able to handle this,
            //       and may require triangles or quads at most.
            {
                std::ofstream FO(DumpFileName, std::fstream::out);

                //Reference the MTL file, but use relative paths to make moving files around easier without having to modify them.
                //FO << "mtllib " << MTLFileName << std::endl;
                FO << "mtllib " << SplitStringToVector(MTLFileName, '/', 'd').back() << std::endl;
                FO << std::endl;
         
                // For each pixel, pre-compute the vertices of the 8 corners (i.e., including the adjacent neighbours).
                // If a vertex would be duplicated, then re-use the existing vertex instead.
                // Four of the vertices are defined by this pixel. The other four are determined by the
                // nearest-neighbours. 
                //
                // Note that this approach will not result in a watertight mesh. Watertight meshes could be achieved by
                // spliting the sides though...

                // Get voxel position, including virtual voxels that do not exist so we can determine where the voxel
                // boundaries are. This routine will happily accept out-of-bounds and negative voxel coordinates.
                std::function<vec3<double>(long int, long int)> get_virtual_position;
                double scale = 1.0; // Scale spatial characteristics iff normalizing.
                if(ShouldNormalize){
                    // Note: width and height extended by 1 to account for true image extent.
                    const auto width  = img.pxl_dx * static_cast<double>(img.rows + 1);
                    const auto height = img.pxl_dy * static_cast<double>(img.columns + 1);
                    scale = ((width / height) < 1.0) ? 100.0 / height
                                                     : 100.0 / width;
                    if(!std::isfinite(scale)) throw std::logic_error("Computed scale factor is invalid. Cannot continue.");

                    get_virtual_position = [&,width,height](long int row, long int col){
                        // Transform the image spatial characteristics to lie in the plane intersecting (0,0,0) and
                        // orthogonal to (0,0,1), with row and unit vectors (1,0,0) and (0,1,0), respectively, and
                        // uniformly scaled such that the entire image fits within a bounding square of size 100x100.
                        // The centre of the image should coincide with (0,0,0).

                        const auto c = (img.pxl_dx * (static_cast<double>(row) + 0.5) - (width  * 0.5)) * scale;
                        const auto r = (img.pxl_dy * (static_cast<double>(col) + 0.5) - (height * 0.5)) * scale;

                        return vec3<double>( r, c, 0.0 );
                    };
                }else{
                    get_virtual_position = [&](long int row, long int col){
                        // Use the image spatial characteristics as-is.
                        return (  img.anchor
                                + img.offset
                                + img.row_unit*( img.pxl_dx * static_cast<double>(row))
                                + img.col_unit*( img.pxl_dy * static_cast<double>(col)) );
                    };
                }

                // Get voxel binned intensity, returning 0 when out of bounds.
                const auto get_virtual_intensity = [&](long int row, long int col) -> long int {
                    if( (row < 0) || (img.rows <= row)
                    ||  (col < 0) || (img.columns <= col) ) return static_cast<long int>(0);

                    const long int chan = 0;
                    const auto val = img.value(row, col, chan);
                    const auto clamped = (val - mm.first)/(mm.second - mm.first);
                    const auto v_binned = static_cast<long int>( std::round(clamped * HistogramBins) );
                    return v_binned;
                };

                const auto emit_vert = [&](vec3<double> pos){
                    const auto defaultprecision = FO.precision();
                    FO << std::setprecision(std::numeric_limits<long double>::max_digits10); 
                    FO << "v " << pos.x << " " << pos.y << " " << pos.z << std::endl;
                    FO << std::setprecision(defaultprecision);
                    return;
                };

                // Give the entire image a simple name. This makes is easier to address by name in, e.g., Blender.
                FO << "o ImageMesh" << std::endl;
                FO << std::endl;

                const auto ortho_unit = img.col_unit.Cross( img.row_unit ).unit();
                long int gvc = 1; // Global vertex counter. Used to track vert number because they have whole-file scope.
                                  // Indices start at 1.
                for(long int row = 0; row <= img.rows; ++row){
                    for(long int col = 0; col <= img.columns; ++col){

                        const auto pos_r0c0 = get_virtual_position(row, col);
                        const auto pos_rpcp = (pos_r0c0 + get_virtual_position(row+1, col+1)) * 0.5;
                        const auto pos_rmcp = (pos_r0c0 + get_virtual_position(row-1, col+1)) * 0.5;
                        const auto pos_rmcm = (pos_r0c0 + get_virtual_position(row-1, col-1)) * 0.5;
                        const auto pos_rpcm = (pos_r0c0 + get_virtual_position(row+1, col-1)) * 0.5;

                        const auto val_r0c0 = get_virtual_intensity(row, col);
                        //const auto val_rpc0 = get_virtual_intensity(row+1, col);
                        //const auto val_r0cp = get_virtual_intensity(row, col+1);
                        //const auto val_rmc0 = get_virtual_intensity(row-1, col);
                        //const auto val_r0cm = get_virtual_intensity(row, col-1);

                        // Generate an object name.
                        //FO << "o Pixel_r" << row << "_c" << col << std::endl;
                        //FO << std::endl;

                        // Choose a face colour.
                        //
                        // Note: Simply add more colours above if you need more colours here.
                        // Note: The obj format does not support per-vertex colours.
                        // Note: The usmtl statement should be before the vertices because some loaders (e.g., Meshlab)
                        //       apply the material to vertices instead of faces.
                        //
                        
                        //if(mats.count(val_r0c0) == 0){
                        //    throw std::logic_error("Pixel value does not have corresponding histogram bin. Cannot continue.");
                        //}
                        FO << "usemtl " << mats.at(val_r0c0) << std::endl;

                        // Print vertices.
                        emit_vert( pos_rmcm ); // Bases are in the plane of the image.
                        emit_vert( pos_rpcm );
                        emit_vert( pos_rpcp );
                        emit_vert( pos_rmcp );

                        const auto offset_unit = (ShouldNormalize ) ? vec3<double>(0.0,0.0,1.0) 
                                                                    : ortho_unit;
                        const auto vertical_offset = offset_unit * (img.pxl_dz * scale * 0.945) 
                                                                 * static_cast<double>(val_r0c0)/static_cast<double>(HistogramBins)
                                                                 * MagnitudeAmplification
                                                   + offset_unit * (img.pxl_dz * scale * 0.005); // Give all pixels a small space between top and bottom.
                        emit_vert( pos_rmcm + vertical_offset ); // Tops are scaled according to pxl_dz (for stackability) and intensity.
                        emit_vert( pos_rpcm + vertical_offset );
                        emit_vert( pos_rpcp + vertical_offset );
                        emit_vert( pos_rmcp + vertical_offset );

                        // Print the face linkages. 
                        // 
                        // Note: The obj format starts at 1, not 0.
                        //
                        // Note: Polygons are implicitly closed and do not need to include a duplicate vertex.
                        FO << "f " << (gvc+0) << " " << (gvc+1) << " " << (gvc+5) << " " << (gvc+4) << "\n"
                           << "f " << (gvc+1) << " " << (gvc+2) << " " << (gvc+6) << " " << (gvc+5) << "\n"
                           << "f " << (gvc+2) << " " << (gvc+3) << " " << (gvc+7) << " " << (gvc+6) << "\n"
                           << "f " << (gvc+3) << " " << (gvc+0) << " " << (gvc+4) << " " << (gvc+7) << "\n"
                           << "f " << (gvc+5) << " " << (gvc+6) << " " << (gvc+7) << " " << (gvc+4) << "\n" // Top.
                           << "f " << (gvc+0) << " " << (gvc+3) << " " << (gvc+2) << " " << (gvc+1) << "\n"; // Bottom.
                        FO << std::endl;

                        gvc += 8;
                    }
                }

                FO.flush();
            }

        } // Loop over images.
    } // Loop over IAs.

    return DICOM_data;
}
