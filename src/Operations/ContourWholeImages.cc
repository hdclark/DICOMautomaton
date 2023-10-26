//ContourWholeImages.cc - A part of DICOMautomaton 2017, 2023. Written by hal clark.

#include <algorithm>
#include <optional>
#include <functional>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>    
#include <cstdint>

#include "Explicator.h"       //Needed for Explicator class.

#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.

#include "../Structs.h"
#include "../Metadata.h"
#include "../Regex_Selectors.h"

#include "ContourWholeImages.h"



OperationDoc OpArgDocContourWholeImages(){
    OperationDoc out;
    out.name = "ContourWholeImages";

    out.desc = 
        "This operation constructs contours for an ROI that encompasses voxels of the specified images."
        " It is useful for operations that operate on ROIs whenever you want to compute something over the whole image."
        " This routine avoids having to manually contour.";
        
    out.notes.emplace_back(
        "This routine will attempt to avoid repeat contours. Generated contours are tested for intersection with an"
        " image before the image is processed."
    );
    out.notes.emplace_back(
        "Existing contours are ignored and unaltered."
    );

    out.args.emplace_back();
    out.args.back().name = "ROILabel";
    out.args.back().desc = "A label to attach to the ROI contours.";
    out.args.back().default_val = "everything";
    out.args.back().expected = true;
    out.args.back().examples = { "everything", "whole_images", "unspecified" };

    
    out.args.emplace_back();
    out.args.back() = IAWhitelistOpArgDoc();
    out.args.back().name = "ImageSelection";
    out.args.back().default_val = "last";


    out.args.emplace_back();
    out.args.back().name = "EncircleMethod";
    out.args.back().desc = "The method used to generate the ROI contours. Options include 'whole' and 'FOV'."
                           "\n\n"
                           "The default option, 'whole', makes contours that encircle all voxels."
                           " Contours are set slightly inside the outer boundary so they can be easily visualized by"
                           " overlaying on an image. All voxel centres will be within the ROI contours."
                           "\n\n"
                           "Option 'FOV' uses image metadata (if available) to only encircle image voxels"
                           " which are within the scanned field of view. In practice, this will be a large circle"
                           " centred on the middle of an image.";
    out.args.back().default_val = "whole";
    out.args.back().expected = true;
    out.args.back().examples = { "whole", "FOV" };

    return out;
}



bool ContourWholeImages(Drover &DICOM_data,
                          const OperationArgPkg& OptArgs,
                          std::map<std::string, std::string>& /*InvocationMetadata*/,
                          const std::string& FilenameLex){

    Explicator X(FilenameLex);

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ROILabel = OptArgs.getValueStr("ROILabel").value();
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();
    const auto MethodStr = OptArgs.getValueStr("EncircleMethod").value();

    //-----------------------------------------------------------------------------------------------------------------
    const auto regex_whole = Compile_Regex("^wh?o?l?e?$");
    const auto regex_fov = Compile_Regex("^fi?e?l?d?[-_]?o?f?[-_]?v?i?e?w?$");

    const auto NormalizedROILabel = X(ROILabel);
    const int64_t ROINumber = 10001; // TODO: find highest existing and ++ it.
    DICOM_data.Ensure_Contour_Data_Allocated();

    auto IAs_all = All_IAs( DICOM_data );
    auto IAs = Whitelist( IAs_all, ImageSelectionStr );
    for(auto & iap_it : IAs){
        //Prepare contour metadata using image metadata.
        //
        //Note: We currently attach *all* common image data to each contour. Should we filter some (most) out?
        std::map<std::string,std::string> metadata;
        metadata = DICOM_data.image_data.back()->imagecoll.get_common_metadata({});
        metadata["ROIName"] = ROILabel;
        metadata["NormalizedROIName"] = NormalizedROILabel;
        metadata["MinimumSeparation"] = metadata["SliceThickness"]; // TODO: is there a routine to do this? (YES: Unique_Contour_Planes()...)
        metadata["ROINumber"] = std::to_string(ROINumber);
        metadata["Description"] = "Whole-Image Contour";

        contour_collection<double> cc;

        if(false){
        }else if(std::regex_match(MethodStr, regex_whole)){
            std::list<std::reference_wrapper<planar_image<float,double>>> imgs;
            for(auto &animg : (*iap_it)->imagecoll.images){
                imgs.emplace_back( std::ref(animg) );
            }

            Encircle_Images_with_Contours_Opts opts;
            opts.inclusivity = Encircle_Images_with_Contours_Opts::Inclusivity::Centre;
            opts.contouroverlap = Encircle_Images_with_Contours_Opts::ContourOverlap::Disallow;

            cc = Encircle_Images_with_Contours(imgs, opts, metadata);

        }else if(std::regex_match(MethodStr, regex_fov)){
            for(auto &animg : (*iap_it)->imagecoll.images){
                const auto centre = animg.center();

                // Identify a suitable FOV diameter.
                auto diam_opt = get_as<double>(animg.metadata, "ReconstructionDiameter");
                if(!diam_opt){
                    YLOGWARN("FOV metadata is not available; resorting to default geometric diameter");
                    const auto corners = animg.corners2D();
                    for(const auto &c : corners){
                        const auto diag = c - centre;
                        const auto w = 2.0 * std::abs(diag.Dot(animg.row_unit));
                        const auto h = 2.0 * std::abs(diag.Dot(animg.col_unit));
                        if( !diam_opt ) diam_opt = w;
                        if( diam_opt.value() < w ) diam_opt = w;
                        if( diam_opt.value() < h ) diam_opt = h;
                    }
                }

                // Create the contour.
                cc.contours.emplace_back();
                cc.contours.back().metadata = metadata;
                cc.contours.back().closed = true;
                const auto pi = std::acos(-1.0);
                const auto r = diam_opt.value() / 2.0;
                const auto ortho_unit = animg.col_unit.Cross( animg.row_unit );

                // Select the number of vertices so each vertex is separated at most by a context-relevant spacing.
                // In this case, we use the smallest in-plane voxel dimension as a relative scale.
                //
                // Option #1: direct maximum distance between vertices. 
                // This provides only an indirect measure of the error.
                // Note the factor of 2 which is arbitrary and used to reduce the number of vertices.
                //const double target_vert_spacing = std::min(animg.pxl_dx, animg.pxl_dy) * 2.0; // <-- arb. x2 here.
                //auto N_verts = static_cast<int64_t>( std::ceil(2.0 * pi * r / target_vert_spacing) );
                // 
                // Option #2: limit the deviation between a right-triangle and an arc with the same radius.
                // This provides a more direct way to gauge the error introduced by approximating the circular ROI.
                // Note the factor of 50 which implies the worst deviation from a perfect circle will be 1/50 the
                // smallest voxel dimension.
                const double max_ortho_discrepancy = std::min(animg.pxl_dx, animg.pxl_dy) / 50.0; // <-- arb. 50x here.
                auto N_verts = static_cast<int64_t>( std::ceil(pi / std::acos(1.0 - max_ortho_discrepancy / r) ) );
                N_verts = std::clamp<int64_t>(N_verts, 20L, 50'000L);

                for(size_t i = 0UL; i < N_verts; ++i){
                    const auto angle = 2.0 * pi * static_cast<double>(i) / static_cast<double>(N_verts);
                    const auto v = (animg.row_unit * diam_opt.value() * 0.5).rotate_around_unit(ortho_unit, angle);
                    cc.contours.back().points.emplace_back(centre + v);
                }
            }

        }else{
            throw std::invalid_argument("EncircleMethod argument '"_s + MethodStr + "' is not valid");
        }

        // Construct a destination for the ROI contours.
        DICOM_data.Ensure_Contour_Data_Allocated();
        DICOM_data.contour_data->ccs.emplace_back();

        DICOM_data.contour_data->ccs.back().contours.splice(
             DICOM_data.contour_data->ccs.back().contours.end(),
             cc.contours);
    }

    return true;
}
