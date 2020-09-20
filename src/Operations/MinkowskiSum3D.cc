//MinkowskiSum3D.cc - A part of DICOMautomaton 2018. Written by hal clark.

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

#include <getopt.h>           //Needed for 'getopts' argument parsing.
#include <cstdlib>            //Needed for exit() calls.
#include <utility>            //Needed for std::pair.
#include <algorithm>
#include <optional>

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

#ifdef DCMA_USE_CGAL
#else
    #error "Attempted to compile without CGAL support, which is required."
#endif

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../Surface_Meshes.h"

#include "MinkowskiSum3D.h"


OperationDoc OpArgDocMinkowskiSum3D(){
    OperationDoc out;
    out.name = "MinkowskiSum3D";

    out.desc = 
        "This operation computes a Minkowski sum or symmetric difference of a 3D surface mesh generated from the"
        " selected ROIs with a sphere."
        " The effect is that a margin is added or subtracted to the ROIs, causing them to 'grow' outward or 'shrink'"
        " inward. Exact and inexact routines can be used.";

    out.args.emplace_back();
    out.args.back().name = "NormalizedROILabelRegex";
    out.args.back().desc = "A regex matching ROI labels/names to consider. The default will match"
                      " all available ROIs. Be aware that input spaces are trimmed to a single space."
                      " If your ROI name has more than two sequential spaces, use regex to avoid them."
                      " All ROIs have to match the single regex, so use the 'or' token if needed."
                      " Regex is case insensitive and uses extended POSIX syntax.";
    out.args.back().default_val = ".*";
    out.args.back().expected = true;
    out.args.back().examples = { ".*", ".*Body.*", "Body", "Gross_Liver",
                            R"***(.*Left.*Parotid.*|.*Right.*Parotid.*|.*Eye.*)***",
                            R"***(Left Parotid|Right Parotid)***" };

    out.args.emplace_back();
    out.args.back().name = "ROILabelRegex";
    out.args.back().desc = "A regex matching ROI labels/names to consider. The default will match"
                           " all available ROIs. Be aware that input spaces are trimmed to a single space."
                           " If your ROI name has more than two sequential spaces, use regex to avoid them."
                           " All ROIs have to match the single regex, so use the 'or' token if needed."
                           " Regex is case insensitive and uses grep syntax.";
    out.args.back().default_val = ".*";
    out.args.back().expected = true;
    out.args.back().examples = { ".*", ".*body.*", "body", "Gross_Liver", 
                                 R"***(.*parotid.*|.*sub.*mand.*)***", 
                                 R"***(left_parotid|right_parotid|eyes)***" };

    out.args.emplace_back();
    out.args.back() = IAWhitelistOpArgDoc();
    out.args.back().name = "ImageSelection";
    out.args.back().desc += " Note that the selected images are used to sample the new contours on."
                            " Image planes need not match the original since a full 3D mesh surface is generated.";
    out.args.back().default_val = "last";

    out.args.emplace_back();
    out.args.back().name = "Operation";
    out.args.back().desc = "The specific operation to perform."
                           " Available options are:"
                           " 'dilate_exact_surface',"
                           " 'dilate_exact_vertex',"
                           " 'dilate_inexact_isotropic',"
                           " 'erode_inexact_isotropic', and"
                           " 'shell_inexact_isotropic'.";
    out.args.back().default_val = "dilate_inexact_isotropic";
    out.args.back().expected = true;
    out.args.back().examples = { "dilate_exact_surface", 
                                 "dilate_exact_vertex", 
                                 "dilate_inexact_isotropic",
                                 "erode_inexact_isotropic", 
                                 "shell_inexact_isotropic" };

    out.args.emplace_back();
    out.args.back().name = "Distance";
    out.args.back().desc = "For dilation and erosion operations, this parameter controls the distance the surface should travel."
                           " For shell operations, this parameter controls the resultant thickness of the shell."
                           " In all cases DICOM units are assumed.";
    out.args.back().default_val = "1.0";
    out.args.back().expected = true;
    out.args.back().examples = { "0.5", "1.0", "2.0", "3.0", "5.0" };


/*
    out.args.emplace_back();
    out.args.back().name = "ContourOverlap";
    out.args.back().desc = "Controls overlapping contours are treated."
                      " The default 'ignore' treats overlapping contours as a single contour, regardless of"
                      " contour orientation. The option 'honour_opposite_orientations' makes overlapping contours"
                      " with opposite orientation cancel. Otherwise, orientation is ignored. The latter is useful"
                      " for Boolean structures where contour orientation is significant for interior contours (holes)."
                      " The option 'overlapping_contours_cancel' ignores orientation and cancels all contour overlap.";
    out.args.back().default_val = "ignore";
    out.args.back().expected = true;
    out.args.back().examples = { "ignore", "honour_opposite_orientations", 
                            "overlapping_contours_cancel", "honour_opps", "overlap_cancel" }; 
*/

    return out;
}

Drover MinkowskiSum3D(Drover DICOM_data,
                      const OperationArgPkg& OptArgs,
                      const std::map<std::string, std::string>&,
                      const std::string&){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto NormalizedROILabelRegex = OptArgs.getValueStr("NormalizedROILabelRegex").value();
    const auto ROILabelRegex = OptArgs.getValueStr("ROILabelRegex").value();

    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();
//    const auto ContourOverlapStr = OptArgs.getValueStr("ContourOverlap").value();
    const auto OpSelectionStr = OptArgs.getValueStr("Operation").value();
    const auto Distance = std::stod( OptArgs.getValueStr("Distance").value() );

    const std::string base_dir("/tmp/MinkowskiSum3D");
    const std::string NewROIName("New ROI");
    const std::string NewNormalizedROIName("New ROI");

    const long int MeshSubdivisions = 2;
    const long int MeshSimplificationEdgeCountLimit = 7500;
    //-----------------------------------------------------------------------------------------------------------------


    const auto regex_dilate_exact_surface     = Compile_Regex("dil?a?t?e?_?exa?c?t?_?surfa?c?e?"); // diexsurf
    const auto regex_dilate_exact_vertex      = Compile_Regex("dil?a?t?e?_?exa?c?t?_?verte?x?");   // diexvert
    const auto regex_dilate_inexact_isotropic = Compile_Regex("dil?a?t?e?_?ine?x?a?c?t?_?isot?r?o?p?i?c?"); //diiniso
    const auto regex_erode_inexact_isotropic  = Compile_Regex("ero?d?e?_?ine?x?a?c?t?_?isot?r?o?p?i?c?"); //eriniso
    const auto regex_shell_inexact_isotropic  = Compile_Regex("she?l?l?_?ine?x?a?c?t?_?isot?r?o?p?i?c?"); //shiniso

    if( !std::regex_match(OpSelectionStr, regex_dilate_exact_surface)
    &&  !std::regex_match(OpSelectionStr, regex_dilate_exact_vertex)
    &&  !std::regex_match(OpSelectionStr, regex_dilate_inexact_isotropic)
    &&  !std::regex_match(OpSelectionStr, regex_erode_inexact_isotropic)
    &&  !std::regex_match(OpSelectionStr, regex_shell_inexact_isotropic) ){
        throw std::invalid_argument("Operation selection is not valid. Cannot continue.");
    }

    //Stuff references to all contours into a list. Remember that you can still address specific contours through
    // the original holding containers (which are not modified here).
    auto cc_all = All_CCs( DICOM_data );
    auto cc_ROIs = Whitelist( cc_all, { { "ROIName", ROILabelRegex },
                                        { "NormalizedROIName", NormalizedROILabelRegex } } );
    if(cc_ROIs.empty()){
        throw std::invalid_argument("No contours selected. Cannot continue.");
    }

    auto common_metadata = contour_collection<double>().get_common_metadata(cc_ROIs, {});

    // Generate a polyhedron surface mesh iff necessary.
    dcma_surface_meshes::Polyhedron output_mesh;
    if( (std::regex_match(OpSelectionStr, regex_dilate_exact_vertex)) ){
        // Do nothing -- no surface is needed.

    }else if( (std::regex_match(OpSelectionStr, regex_dilate_exact_surface))
          ||  (std::regex_match(OpSelectionStr, regex_dilate_inexact_isotropic))
          ||  (std::regex_match(OpSelectionStr, regex_erode_inexact_isotropic))
          ||  (std::regex_match(OpSelectionStr, regex_shell_inexact_isotropic)) ){
        // Generate a surface and prepare it for a Minkowski operation.
        dcma_surface_meshes::Parameters meshing_params;
        output_mesh = dcma_surface_meshes::Estimate_Surface_Mesh( cc_ROIs, meshing_params );
        //output_mesh = dcma_surface_meshes::Estimate_Surface_Mesh_AdvancingFront( cc_ROIs, meshing_params );

        polyhedron_processing::Subdivide(output_mesh, MeshSubdivisions);
        polyhedron_processing::Simplify(output_mesh, MeshSimplificationEdgeCountLimit);
        polyhedron_processing::SaveAsOFF(output_mesh, base_dir + "_polyhedron.off");

    }else{
        throw std::invalid_argument("Operation not recognized");
    }


    // Operate on the mesh.
    if(std::regex_match(OpSelectionStr, regex_dilate_exact_surface)){
        const auto sphere_mesh = polyhedron_processing::Regular_Icosahedron(Distance);
        polyhedron_processing::Dilate(output_mesh,
                                      sphere_mesh); // Full 3D dilation/"offset."

    }else if(std::regex_match(OpSelectionStr, regex_dilate_exact_vertex)){
        const auto sphere_mesh = polyhedron_processing::Regular_Icosahedron(Distance);
        polyhedron_processing::Dilate(output_mesh, 
                                      cc_ROIs, 
                                      sphere_mesh); // Vertex-based dilation/"offset."

    }else if(std::regex_match(OpSelectionStr, regex_dilate_inexact_isotropic)){
        polyhedron_processing::Transform( output_mesh, 
                                          Distance,
                                          polyhedron_processing::TransformOp::Dilate );

    }else if(std::regex_match(OpSelectionStr, regex_erode_inexact_isotropic)){
        polyhedron_processing::Transform( output_mesh, 
                                          Distance,
                                          polyhedron_processing::TransformOp::Erode );

    }else if(std::regex_match(OpSelectionStr, regex_shell_inexact_isotropic)){
        polyhedron_processing::Transform( output_mesh, 
                                          Distance,
                                          polyhedron_processing::TransformOp::Shell );

    }else{
        throw std::invalid_argument("Operation not recognized");
    }


    //-----------------------------------------------------------------------------------------------------------------

    auto IAs_all = All_IAs( DICOM_data );
    auto IAs = Whitelist( IAs_all, ImageSelectionStr );
    for(auto & iap_it : IAs){

        // Sample new contours on the selected images.
        //
        // Note: We use image planes because the original contour planes will not generally encompass the new extent of
        //       the ROI.
        std::list<plane<double>> img_planes;
        for(const auto &animg : (*iap_it)->imagecoll.images){
            img_planes.emplace_back( animg.image_plane() );
        }

        // Perform the slicing.
        auto cc = polyhedron_processing::Slice_Polyhedron(output_mesh, img_planes);

        // If there were any contours generated, inject them into the Drover object.
        if(!cc.contours.empty()){
            for(auto &c : cc.contours){
                if(c.points.size() >= 3){
                    c.Reorient_Counter_Clockwise();
                    c.closed = true;
                    c.metadata = common_metadata;
                    c.metadata["ROIName"] = NewROIName;
                    c.metadata["NormalizedROIName"] = NewNormalizedROIName;
                }
            }

            if(DICOM_data.contour_data == nullptr){
                std::unique_ptr<Contour_Data> output (new Contour_Data());
                DICOM_data.contour_data = std::move(output);
            }
            DICOM_data.contour_data->ccs.emplace_back(cc);
        }
    }

    return DICOM_data;
}
