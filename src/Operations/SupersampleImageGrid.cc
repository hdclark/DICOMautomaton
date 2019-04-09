//SupersampleImageGrid.cc - A part of DICOMautomaton 2016. Written by hal clark.

#include <experimental/any>
#include <experimental/optional>
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



OperationDoc OpArgDocSupersampleImageGrid(void){
    OperationDoc out;
    out.name = "SupersampleImageGrid";

    out.desc = 
        "This operation scales supersamples images so they have more rows and/or columns, but the whole image keeps its"
        " shape and spatial extent. This operation is typically used for zooming into images or trying to ensure a"
        " sufficient number of voxels are within small contours.";
        
    out.notes.emplace_back(
        "Be aware that specifying large multipliers (or even small multipliers on large images) will consume much"
        " memory. It is best to pre-crop images to a region of interest if possible."
    );

    out.args.emplace_back();
    out.args.back().name = "ColumnScaleFactor";
    out.args.back().desc = "A positive integer specifying how many columns will be in the new images."
                           " The number is relative to the incoming image column count. Specifying '1' will"
                           " result in nothing happening. Specifying '8' will result in 8x as many columns.";
    out.args.back().default_val = "2";
    out.args.back().expected = true;
    out.args.back().examples = { "1", "2", "3", "8" };

    out.args.emplace_back();
    out.args.back().name = "DoseImageSelection";
    out.args.back().desc = "Dose images to operate on. Either 'none', 'last', or 'all'.";
    out.args.back().default_val = "none";
    out.args.back().expected = true;
    out.args.back().examples = { "none", "last", "all" };
    
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
    out.args.back().name = "SliceScaleFactor";
    out.args.back().desc = "A positive integer specifying how many image slices will be in the new images."
                           " The number is relative to the incoming image slice count. Specifying '1' will"
                           " result in nothing happening. Specifying '8' will result in 8x as many slices."
                           " Note that slice supersampling always happens *after* in-plane supersampling.";
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

    return out;
}


Drover SupersampleImageGrid(Drover DICOM_data, OperationArgPkg OptArgs, std::map<std::string,std::string> /*InvocationMetadata*/, std::string /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ColumnScaleFactor = std::stol(OptArgs.getValueStr("ColumnScaleFactor").value());
    const auto DoseImageSelectionStr = OptArgs.getValueStr("DoseImageSelection").value();
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();
    const auto RowScaleFactor = std::stol(OptArgs.getValueStr("RowScaleFactor").value());
    const auto SliceScaleFactor = std::stol(OptArgs.getValueStr("SliceScaleFactor").value());
    const auto SamplingMethodStr = OptArgs.getValueStr("SamplingMethod").value();

    //-----------------------------------------------------------------------------------------------------------------
    const auto regex_none = Compile_Regex("no?n?e?$");
    const auto regex_last = Compile_Regex("la?s?t?$");
    const auto regex_all  = Compile_Regex("al?l?$");

    const auto inplane_bilin = Compile_Regex("inp?l?a?n?e?-?b?i?line?a?r?");
    const auto inplane_bicub = Compile_Regex("inp?l?a?n?e?-?b?i?cubi?c?");
    const auto trilin = Compile_Regex("tr?i?l?i?n?e?a?r?");

    if( !std::regex_match(DoseImageSelectionStr, regex_none)
    &&  !std::regex_match(DoseImageSelectionStr, regex_last)
    &&  !std::regex_match(DoseImageSelectionStr, regex_all) ){
        throw std::invalid_argument("Dose Image selection is not valid. Cannot continue.");
    }

    auto IAs_all = All_IAs( DICOM_data );
    auto IAs = Whitelist( IAs_all, ImageSelectionStr );
    
    if(false){
    }else if( std::regex_match(SamplingMethodStr, inplane_bilin)
          ||  std::regex_match(SamplingMethodStr, trilin) ){
        //Image data.
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

        //Dose data.
        auto dap_it = DICOM_data.dose_data.begin();
        if(false){
        }else if(std::regex_match(DoseImageSelectionStr, regex_none)){ dap_it = DICOM_data.dose_data.end();
        }else if(std::regex_match(DoseImageSelectionStr, regex_last)){
            if(!DICOM_data.dose_data.empty()) dap_it = std::prev(DICOM_data.dose_data.end());
        }
        while(dap_it != DICOM_data.dose_data.end()){
            InImagePlaneBilinearSupersampleUserData bilin_ud;
            bilin_ud.RowScaleFactor = RowScaleFactor; 
            bilin_ud.ColumnScaleFactor = ColumnScaleFactor; 
            if(!(*dap_it)->imagecoll.Process_Images_Parallel( GroupIndividualImages,
                                                              InImagePlaneBilinearSupersample,
                                                              {}, {}, &bilin_ud )){
                throw std::runtime_error("Unable to bilinearly supersample dose images. Cannot continue.");
            }
            ++dap_it;
        }

    }else if(std::regex_match(SamplingMethodStr, inplane_bicub)){
        //Image data.
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

        //Dose data.
        auto dap_it = DICOM_data.dose_data.begin();
        if(false){
        }else if(std::regex_match(DoseImageSelectionStr, regex_none)){ dap_it = DICOM_data.dose_data.end();
        }else if(std::regex_match(DoseImageSelectionStr, regex_last)){
            if(!DICOM_data.dose_data.empty()) dap_it = std::prev(DICOM_data.dose_data.end());
        }
        while(dap_it != DICOM_data.dose_data.end()){
            InImagePlaneBicubicSupersampleUserData bicub_ud;
            bicub_ud.RowScaleFactor = RowScaleFactor; 
            bicub_ud.ColumnScaleFactor = ColumnScaleFactor; 
            if(!(*dap_it)->imagecoll.Process_Images_Parallel( GroupIndividualImages,
                                                              InImagePlaneBicubicSupersample,
                                                              {}, {}, &bicub_ud )){
                throw std::runtime_error("Unable to bicubically supersample dose images. Cannot continue.");
            }
            ++dap_it;
        }

    }else{
        throw std::invalid_argument("Invalid sampling method specified. Cannot continue");
    }

    if(false){
    }else if( std::regex_match(SamplingMethodStr, trilin) ){

        for(auto & iap_it : IAs){
            if((*iap_it)->imagecoll.images.empty()){
                FUNCWARN("Skipping empty image array");
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
                const auto img_plane = img.image_plane();
                //const auto dR = (R_0 - img_plane.R_0).Dot(N_0);
                const auto dR = (img.position(0,0) - R_0).Dot(N_0);
                const auto up = dR + 0.5 * img.pxl_dz;
                const auto lo = dR - 0.5 * img.pxl_dz;
                if(!std::isfinite(upper_extent) || (upper_extent < up)) upper_extent = up;
                if(!std::isfinite(lower_extent) || (lo < lower_extent)) lower_extent = lo;
            }

            const auto N_old = (*iap_it)->imagecoll.images.size();
            const auto N_new = N_old * SliceScaleFactor;
            const auto pxl_dz = std::abs(upper_extent - lower_extent) / static_cast<double>(N_new);

            for(long int i = 0; i < N_new; ++i){
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
            }

            // Interpolate the slices using the original slices as a reference.
            std::list<std::reference_wrapper<planar_image_collection<float, double>>> IARL = { std::ref( (*iap_it)->imagecoll ) };
            ComputeInterpolateImageSlicesUserData ud;
            ud.channel = -1; // Operate on all channels to maintain consistency with in-plane only methods.

            if(!edit_imagecoll.Compute_Images( ComputeInterpolateImageSlices, 
                                               IARL, {}, &ud )){
                throw std::runtime_error("Unable to interpolate image slices.");
            }

            // Trim metadata.

            // ... 

            // Inject the data.
            DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>() );
            DICOM_data.image_data.back()->imagecoll.images.splice(
                DICOM_data.image_data.back()->imagecoll.images.end(),
                edit_imagecoll.images );
        }
    }

    return DICOM_data;
}
