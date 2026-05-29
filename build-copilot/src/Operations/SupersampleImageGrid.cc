//SupersampleImageGrid.cc - A part of DICOMautomaton 2016. Written by hal clark.

#include <any>
#include <optional>
#include <functional>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>    

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../YgorImages_Functors/Grouping/Misc_Functors.h"
#include "../YgorImages_Functors/Processing/In_Image_Plane_Bicubic_Supersample.h"
#include "../YgorImages_Functors/Processing/In_Image_Plane_Bilinear_Supersample.h"
#include "../YgorImages_Functors/Compute/Interpolate_Image_Slices.h"
#include "SupersampleImageGrid.h"
#include "YgorImages.h"



OperationDoc OpArgDocSupersampleImageGrid(){
    OperationDoc out;
    out.name = "SupersampleImageGrid";

    out.tags.emplace_back("category: image processing");

    out.desc = 
        "This operation supersamples (i.e., scales and resamples) whole image arrays so they have more rows and/or"
        " columns, but in a way that the supersampled image array retains the shape and spatial extent of the"
        " original image array."
        " This operation is typically used for 'zooming' into images, or dividing large voxels"
        " so that binarization using small contours has reduced spillover.";
        
    out.notes.emplace_back(
        "Be aware that specifying large multipliers (or even small multipliers on large images) could consume"
        " large amounts of memory. It is best to pre-crop images to a given region of interest if possible."
    );

    out.args.emplace_back();
    out.args.back() = IAWhitelistOpArgDoc();
    out.args.back().name = "ImageSelection";
    out.args.back().default_val = "last";
    
    out.args.emplace_back();
    out.args.back().name = "RowScaleFactor";
    out.args.back().desc = "A positive integer specifying how many rows will be in the new images."
                           " The number is relative to the incoming image row count. Specifying '1' will"
                           " result in nothing happening. Specifying '8' will result in 8x as many rows.";
    out.args.back().default_val = "2";
    out.args.back().expected = true;
    out.args.back().examples = { "1", "2", "3", "8" };

    out.args.emplace_back();
    out.args.back().name = "ColumnScaleFactor";
    out.args.back().desc = "A positive integer specifying how many columns will be in the new images."
                           " The number is relative to the incoming image column count. Specifying '1' will"
                           " result in nothing happening. Specifying '8' will result in 8x as many columns.";
    out.args.back().default_val = "2";
    out.args.back().expected = true;
    out.args.back().examples = { "1", "2", "3", "8" };

    out.args.emplace_back();
    out.args.back().name = "SliceScaleFactor";
    out.args.back().desc = "A positive integer specifying how many image slices will be in the new images."
                           " The number is relative to the incoming image slice count. Specifying '1' will"
                           " result in nothing happening. Specifying '8' will result in 8x as many slices."
                           " Note that slice supersampling always happens *after* in-plane supersampling."
                           " Also note that merely setting this factor will not enable 3D supersampling;"
                           " you also need to specify a 3D-aware SamplingMethod.";
    out.args.back().default_val = "2";
    out.args.back().expected = true;
    out.args.back().examples = { "1", "2", "3", "8" };

    out.args.emplace_back();
    out.args.back().name = "SamplingMethod";
    out.args.back().desc = "The supersampling method to use. Note: 'inplane-' methods only consider neighbours"
                           " in the plane of a single image -- neighbours in adjacent images are not considered"
                           " and the supersampled image will contain the same number of image slices as the"
                           " inputs.";
    out.args.back().default_val = "inplane-bilinear";
    out.args.back().expected = true;
    out.args.back().examples = { "inplane-bicubic",
                                 "inplane-bilinear",
                                 "trilinear" };
    out.args.back().samples = OpArgSamples::Exhaustive;

    return out;
}


bool SupersampleImageGrid(Drover &DICOM_data,
                            const OperationArgPkg& OptArgs,
                            std::map<std::string, std::string>& /*InvocationMetadata*/,
                            const std::string& /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();
    const auto RowScaleFactor = std::stol(OptArgs.getValueStr("RowScaleFactor").value());
    const auto ColumnScaleFactor = std::stol(OptArgs.getValueStr("ColumnScaleFactor").value());
    const auto SliceScaleFactor = std::stol(OptArgs.getValueStr("SliceScaleFactor").value());
    const auto SamplingMethodStr = OptArgs.getValueStr("SamplingMethod").value();

    //-----------------------------------------------------------------------------------------------------------------
    const auto regex_none = Compile_Regex("no?n?e?$");
    const auto regex_last = Compile_Regex("la?s?t?$");
    const auto regex_all  = Compile_Regex("al?l?$");

    const auto inplane_bilin = Compile_Regex("inp?l?a?n?e?-?b?i?line?a?r?");
    const auto inplane_bicub = Compile_Regex("inp?l?a?n?e?-?b?i?cubi?c?");
    const auto trilin = Compile_Regex("tr?i?l?i?n?e?a?r?");

    auto IAs_all = All_IAs( DICOM_data );
    auto IAs = Whitelist( IAs_all, ImageSelectionStr );
    
    if(false){
    }else if( ( std::regex_match(SamplingMethodStr, inplane_bilin) ||  std::regex_match(SamplingMethodStr, trilin) )
          &&  ( ( 1L < RowScaleFactor ) || ( 1L < ColumnScaleFactor ) ) ){
        for(auto & iap_it : IAs){
            InImagePlaneBilinearSupersampleUserData bilin_ud;
            bilin_ud.RowScaleFactor = RowScaleFactor; 
            bilin_ud.ColumnScaleFactor = ColumnScaleFactor; 
            if(!(*iap_it)->imagecoll.Process_Images_Parallel( GroupIndividualImages,
                                                              InImagePlaneBilinearSupersample,
                                                              {}, {}, &bilin_ud )){
                throw std::runtime_error("Unable to bilinearly supersample images. Cannot continue.");
            }
        }

    }else if( std::regex_match(SamplingMethodStr, inplane_bicub)
          &&  ( ( 1L < RowScaleFactor ) || ( 1L < ColumnScaleFactor ) ) ){
        for(auto & iap_it : IAs){
            InImagePlaneBicubicSupersampleUserData bicub_ud;
            bicub_ud.RowScaleFactor = RowScaleFactor; 
            bicub_ud.ColumnScaleFactor = ColumnScaleFactor; 
            if(!(*iap_it)->imagecoll.Process_Images_Parallel( GroupIndividualImages,
                                                              InImagePlaneBicubicSupersample,
                                                              {}, {}, &bicub_ud )){
                throw std::runtime_error("Unable to bicubically supersample images. Cannot continue.");
            }
        }

    }else{
        throw std::invalid_argument("Invalid sampling method specified. Cannot continue");
    }

    if( std::regex_match(SamplingMethodStr, trilin)
    &&  (1L < SliceScaleFactor) ){

        for(auto & iap_it : IAs){
            if((*iap_it)->imagecoll.images.empty()){
                YLOGWARN("Skipping empty image array");
                continue;
            }

            // Create placeholder image slices with the correct geometry. The values of these images will be overwritten.
            planar_image_collection<float, double> edit_imagecoll;

            const auto proto_plane = (*iap_it)->imagecoll.images.front().image_plane();
            //const auto R_0 = proto_plane.R_0;
            const auto R_0 = (*iap_it)->imagecoll.images.front().position(0,0);
            const auto N_0 = proto_plane.N_0;

            auto upper_extent = std::numeric_limits<double>::quiet_NaN();
            auto lower_extent = std::numeric_limits<double>::quiet_NaN();
            for(const auto &img : (*iap_it)->imagecoll.images){
                const auto dR = (img.position(0,0) - R_0).Dot(N_0);
                const auto up = dR + 0.5 * img.pxl_dz;
                const auto lo = dR - 0.5 * img.pxl_dz;
                if(!std::isfinite(upper_extent) || (upper_extent < up)) upper_extent = up;
                if(!std::isfinite(lower_extent) || (lo < lower_extent)) lower_extent = lo;
            }

            const auto N_old = (*iap_it)->imagecoll.images.size();
            const auto N_new = N_old * SliceScaleFactor;
            const auto pxl_dz = std::abs(upper_extent - lower_extent) / static_cast<double>(N_new);

            const auto common_metadata = (*iap_it)->imagecoll.get_common_metadata({});

            for(size_t i = 0; i < N_new; ++i){
                edit_imagecoll.images.push_back( (*iap_it)->imagecoll.images.front() );
                const auto offset = R_0
                                  - N_0 * std::abs(lower_extent)
                                  + N_0 * pxl_dz * 0.5
                                  + N_0 * pxl_dz * static_cast<double>(i); 
                edit_imagecoll.images.back().init_spatial(edit_imagecoll.images.back().pxl_dx,
                                                          edit_imagecoll.images.back().pxl_dy,
                                                          pxl_dz,
                                                          edit_imagecoll.images.back().anchor,
                                                          offset);
                edit_imagecoll.images.back().metadata = common_metadata;
                edit_imagecoll.images.back().metadata["SliceThickness"] = std::to_string(pxl_dz);
            }
            edit_imagecoll.images.reverse();


            // Interpolate the slices using the original slices as a reference.
            std::list<std::reference_wrapper<planar_image_collection<float, double>>> IARL = { std::ref( (*iap_it)->imagecoll ) };
            ComputeInterpolateImageSlicesUserData ud;
            ud.channel = -1; // Operate on all channels to maintain consistency with in-plane only methods.
            ud.description = "Supersampled "_s
                           + std::to_string(RowScaleFactor) + "x, "_s
                           + std::to_string(ColumnScaleFactor) + "x, "_s
                           + std::to_string(SliceScaleFactor) + "x"_s
                           + " with trilinear interpolation";

            if(!edit_imagecoll.Compute_Images( ComputeInterpolateImageSlices, 
                                               IARL, {}, &ud )){
                throw std::runtime_error("Unable to interpolate image slices.");
            }

            // Trim metadata.

            // ... 

            // Inject the data.
            //DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>() );
            //DICOM_data.image_data.back()->imagecoll.images.splice(
            //    DICOM_data.image_data.back()->imagecoll.images.end(),
            //    edit_imagecoll.images );

            (*iap_it)->imagecoll.images.clear();
            (*iap_it)->imagecoll.images.splice(
                (*iap_it)->imagecoll.images.end(),
                edit_imagecoll.images );
        }
    }

    return true;
}
