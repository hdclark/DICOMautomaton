//ContourViaGeometry.cc - A part of DICOMautomaton 2019. Written by hal clark.

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
#include "YgorStats.h"        //Needed for Stats:: namespace.
#include "YgorString.h"       //Needed for GetFirstRegex(...)

#include "Explicator.h"       //Needed for Explicator class.

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../Thread_Pool.h"
#include "../Insert_Contours.h"

#include "ContourViaGeometry.h"


OperationDoc OpArgDocContourViaGeometry(){
    OperationDoc out;
    out.name = "ContourViaGeometry";

    out.desc = 
        "This operation constructs ROI contours using geometrical primitives.";
        
    out.notes.emplace_back(
        "This routine requires an image array onto which the contours will be written."
    );

    out.notes.emplace_back(
        "This routine expects images to be non-overlapping. In other words, if images overlap then the contours"
        " generated may also overlap. This is probably not what you want (but there is nothing intrinsically wrong"
        " with presenting this routine with multiple images if you intentionally want overlapping contours)."
    );
        
    out.notes.emplace_back(
        "Existing contours are ignored and unaltered."
    );
        
    out.notes.emplace_back(
        "Small and degenerate contours produced by this routine are suppressed."
        " If a specific number of contours must be generated, provide a slightly larger radius to compensate"
        " for the degenerate cases at the extrema."
    );
        

    out.args.emplace_back();
    out.args.back().name = "ROILabel";
    out.args.back().desc = "A label to attach to the ROI contours.";
    out.args.back().default_val = "unspecified";
    out.args.back().expected = true;
    out.args.back().examples = { "unspecified", "body", "air", "bone", "invalid", "above_zero", "below_5.3" };


    out.args.emplace_back();
    out.args.back() = IAWhitelistOpArgDoc();
    out.args.back().name = "ImageSelection";
    out.args.back().default_val = "last";
   

    out.args.emplace_back();
    out.args.back().name = "Shapes";
    out.args.back().desc = "This parameter is used to specify the shapes to consider."
                           " There is currently a single supported shape: sphere."
                           " However, it is likely that more shapes will be accepted in the future."
                           " Spheres have two configurable parameters: centre and radius."
                           " A sphere with centre (1.0,2.0,3.0) and radius 12.3 can be specified as"
                           " 'sphere(1.0, 2.0, 3.0,  12.3)'.";
    out.args.back().default_val = "";
    out.args.back().expected = true;
    out.args.back().examples = { "sphere(-1.0, 2.0, 3.0,  12.3)" };

    return out;
}



Drover ContourViaGeometry(Drover DICOM_data,
                          const OperationArgPkg& OptArgs,
                          const std::map<std::string, std::string>&
                          /*InvocationMetadata*/,
                          const std::string& FilenameLex){
    Explicator X(FilenameLex);

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ROILabel = OptArgs.getValueStr("ROILabel").value();
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();
    const auto ShapesStr = OptArgs.getValueStr("Shapes").value();

    //-----------------------------------------------------------------------------------------------------------------
    const std::string ROIColour = "black";

    const auto NormalizedROILabel = X(ROILabel);

    // Parse ShapesOpt into a list of (varyable) geometry? For now simply parse into a single sphere.
    const auto sphere_regex = Compile_Regex("^sp?h?e?r?e?.*$");

    std::list<sphere<double>> spheres;

    if(std::regex_match(ShapesStr, sphere_regex)){
        auto split = SplitStringToVector(ShapesStr, '(', 'd');
        split = SplitVector(split, ')', 'd');
        split = SplitVector(split, ',', 'd');

        std::vector<double> numbers;
        for(const auto &w : split){
           try{
               const auto x = std::stod(w);
               numbers.emplace_back(x);
           }catch(const std::exception &){ }
        }
        if(numbers.size() != 4){
            throw std::invalid_argument("Unable to parse sphere shape parameters. Cannot continue.");
        }

        const vec3<double> centre( numbers.at(0),
                                   numbers.at(1),
                                   numbers.at(2) );
        const double radius = numbers.at(3);
        spheres.emplace_back( centre, radius );

    }else{
        throw std::invalid_argument("Shape not understood. Refusing to continue.");
    }


    //Construct a destination for the ROI contours.
    DICOM_data.Ensure_Contour_Data_Allocated();
    DICOM_data.contour_data->ccs.emplace_back();

    const double MinimumSeparation = 1.0; // TODO: is there a routine to do this? (YES: Unique_Contour_Planes()...)
    std::list<contour_of_points<double>> copl;

    //Iterate over each requested image_array. Each image is processed independently, so a thread pool is used.
    auto IAs_all = All_IAs( DICOM_data );
    auto IAs = Whitelist( IAs_all, ImageSelectionStr );
    for(auto & iap_it : IAs){
        for(const auto &animg : (*iap_it)->imagecoll.images){

            auto contour_metadata = animg.metadata;
            contour_metadata["ROIName"] = ROILabel;
            contour_metadata["NormalizedROIName"] = NormalizedROILabel;
            contour_metadata["ROINumber"] = std::to_string(10000); // TODO: find highest existing and ++ it.
            contour_metadata["MinimumSeparation"] = std::to_string(MinimumSeparation);
            contour_metadata["OutlineColour"] = ROIColour;

            const auto img_plane = animg.image_plane();
            for(const auto &asphere : spheres){
                const auto radius = asphere.r_0;
                const auto centre = asphere.C_0;

                const auto plane_centre_sphere_dist = std::abs( img_plane.Get_Signed_Distance_To_Point(centre) );
                if(plane_centre_sphere_dist > radius) continue;

                // rho is radius of the circle projected onto this image.
                const auto rho = radius * std::sqrt( 1.0 - std::pow(plane_centre_sphere_dist / radius, 2.0) );
                const auto proj_centre = img_plane.Project_Onto_Plane_Orthogonally(centre);

                if(rho < 0.05) continue; // Skip small contours, which can be problematic.

                // ensure vertex sampling is sufficient.
                const auto min_vert_sep = 1.0; // DICOM units (mm).
                const auto pi = std::acos(-1.0);
                const auto arc_length = 2.0 * pi * rho;
                auto num_verts = static_cast<long int>( std::ceil(arc_length / min_vert_sep) );
                num_verts = (num_verts < 3) ? 3 : num_verts; 

                try{
                    Inject_Point_Contour(animg,
                                         proj_centre,
                                         DICOM_data.contour_data->ccs.back(),
                                         contour_metadata,
                                         rho,
                                         num_verts );
                }catch(const std::exception &){};
            }
        }
    }

    return DICOM_data;
}

