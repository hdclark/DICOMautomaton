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
#include "../Regex_Selectors.h"
#include "../Surface_Meshes.h"

#include "MinkowskiSum3D.h"


OperationDoc OpArgDocMinkowskiSum3D(void){
    OperationDoc out;
    out.name = "MinkowskiSum3D";

    out.desc = 
        "This operation computes a Minkowski sum of a 3D surface mesh generated from the selected ROIs with a sphere."
        " The effect is that a margin is added or subtracted to the ROIs, causing them to 'grow' outward or 'shrink'"
        " inward.";

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
    out.args.back().name = "ImageSelection";
    out.args.back().desc = "Images to sample the new contours on. Note that the image planes need not match"
                           " the original since a full 3D mesh surface is generated."
                           " Either 'none', 'last', or 'all'.";
    out.args.back().default_val = "last";
    out.args.back().expected = true;
    out.args.back().examples = { "none", "last", "all" };

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

Drover MinkowskiSum3D(Drover DICOM_data, OperationArgPkg OptArgs, std::map<std::string,std::string>, std::string ){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto NormalizedROILabelRegex = OptArgs.getValueStr("NormalizedROILabelRegex").value();
    const auto ROILabelRegex = OptArgs.getValueStr("ROILabelRegex").value();

    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();
//    const auto ContourOverlapStr = OptArgs.getValueStr("ContourOverlap").value();

    const std::string base_dir("/tmp/MinkowskiSum3D");
    const std::string NewROIName("New ROI");
    const std::string NewNormalizedROIName("New ROI");

    const long int MeshSubdivisions = 2;
    const long int MeshSimplificationEdgeCountLimit = 7500;
    //-----------------------------------------------------------------------------------------------------------------


    const auto regex_none = std::regex("no?n?e?$", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto regex_last = std::regex("la?s?t?$", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto regex_all  = std::regex("al?l?$",   std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);

//    const auto regex_ignore = std::regex("^ig?n?o?r?e?$", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
//    const auto regex_honopps = std::regex("^ho?n?o?u?r?_?o?p?p?o?s?i?t?e?_?o?r?i?e?n?t?a?t?i?o?n?s?$", 
//                                          std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
//    const auto regex_cancel = std::regex("^ov?e?r?l?a?p?p?i?n?g?_?c?o?n?t?o?u?r?s?_?c?a?n?c?e?l?s?$",
//                                          std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);

    if( !std::regex_match(ImageSelectionStr, regex_none)
    &&  !std::regex_match(ImageSelectionStr, regex_last)
    &&  !std::regex_match(ImageSelectionStr, regex_all) ){
        throw std::invalid_argument("Image selection is not valid. Cannot continue.");
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

    // Generate a polyhedron surface mesh.
    contour_surface_meshes::Parameters meshing_params;

    auto output_mesh = contour_surface_meshes::Estimate_Surface_Mesh( cc_ROIs, meshing_params );
    //auto output_mesh = contour_surface_meshes::Estimate_Surface_Mesh_AdvancingFront( cc_ROIs, meshing_params );

    polyhedron_processing::Subdivide(output_mesh, MeshSubdivisions);
    polyhedron_processing::Simplify(output_mesh, MeshSimplificationEdgeCountLimit);
    polyhedron_processing::SaveAsOFF(output_mesh, base_dir + "_polyhedron.off");

    // Dilate the mesh.
    const auto sphere_mesh = polyhedron_processing::Regular_Icosahedron();
    polyhedron_processing::Dilate(output_mesh, sphere_mesh); // Full 3D dilation/"offset."
    //polyhedron_processing::Dilate(output_mesh, cc_ROIs, sphere_mesh); // Vertex-based dilation/"offset."


    //-----------------------------------------------------------------------------------------------------------------

    auto iap_it = DICOM_data.image_data.begin();
    if(false){
    }else if(std::regex_match(ImageSelectionStr, regex_none)){
        iap_it = DICOM_data.image_data.end();
    }else if(std::regex_match(ImageSelectionStr, regex_last)){
        if(!DICOM_data.image_data.empty()) iap_it = std::prev(DICOM_data.image_data.end());
    }
    while(iap_it != DICOM_data.image_data.end()){

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

        ++iap_it;
    }


    return DICOM_data;
}
