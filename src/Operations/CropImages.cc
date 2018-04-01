//CropImages.cc - A part of DICOMautomaton 2018. Written by hal clark.

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
#include "../YgorImages_Functors/Compute/CropToROIs.h"
#include "CropImages.h"
#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.



std::list<OperationArgDoc> OpArgDocCropImages(void){
    std::list<OperationArgDoc> out;

    // This operation crops image slices.
    //

    out.emplace_back();
    out.back().name = "ImageSelection";
    out.back().desc = "Images to operate on. Either 'none', 'last', 'first', or 'all'.";
    out.back().default_val = "all";
    out.back().expected = true;
    out.back().examples = { "none", "last", "first", "all" };
    

    out.emplace_back();
    out.back().name = "RowsL";
    out.back().desc = "The number of rows to remove, starting with the first row. Can be absolute (px), percentage (%), or"
                      " distance in terms of the DICOM coordinate system. Note the DICOM coordinate system can be flipped, so"
                      " the first row can be either on the top or bottom of the image.";
    out.back().default_val = "0px";
    out.back().expected = true;
    out.back().examples = { "0px", "10px", "100px", "15%", "15.75%", "123.45" };


    out.emplace_back();
    out.back().name = "RowsH";
    out.back().desc = "The number of rows to remove, starting with the last row. Can be absolute (px), percentage (%), or"
                      " distance in terms of the DICOM coordinate system. Note the DICOM coordinate system can be flipped, so"
                      " the first row can be either on the top or bottom of the image.";
    out.back().default_val = "0px";
    out.back().expected = true;
    out.back().examples = { "0px", "10px", "100px", "15%", "15.75%", "123.45" };


    out.emplace_back();
    out.back().name = "ColumnsL";
    out.back().desc = "The number of columns to remove, starting with the first column. Can be absolute (px), percentage (%), or"
                      " distance in terms of the DICOM coordinate system. Note the DICOM coordinate system can be flipped, so"
                      " the first column can be either on the top or bottom of the image.";
    out.back().default_val = "0px";
    out.back().expected = true;
    out.back().examples = { "0px", "10px", "100px", "15%", "15.75%", "123.45" };


    out.emplace_back();
    out.back().name = "ColumnsH";
    out.back().desc = "The number of columns to remove, starting with the last column. Can be absolute (px), percentage (%), or"
                      " distance in terms of the DICOM coordinate system. Note the DICOM coordinate system can be flipped, so"
                      " the first column can be either on the top or bottom of the image.";
    out.back().default_val = "0px";
    out.back().expected = true;
    out.back().examples = { "0px", "10px", "100px", "15%", "15.75%", "123.45" };


    out.emplace_back();
    out.back().name = "DICOMMargin";
    out.back().desc = "The amount of margin (in the DICOM coordinate system) to spare from cropping.";
    out.back().default_val = "0.0";
    out.back().expected = true;
    out.back().examples = { "0.1", "2.0", "-0.5", "20.0" };

    return out;
}



Drover CropImages(Drover DICOM_data, OperationArgPkg OptArgs, std::map<std::string,std::string> /*InvocationMetadata*/, std::string /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto RowsL_str = OptArgs.getValueStr("RowsL").value();
    const auto RowsH_str = OptArgs.getValueStr("RowsH").value();
    const auto ColsL_str = OptArgs.getValueStr("ColumnsL").value();
    const auto ColsH_str = OptArgs.getValueStr("ColumnsH").value();

    const auto DICOMMargin = std::stod(OptArgs.getValueStr("DICOMMargin").value());
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();

    //-----------------------------------------------------------------------------------------------------------------
    const auto regex_none  = std::regex("^no?n?e?$", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto regex_first = std::regex("^fi?r?s?t?$", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto regex_last  = std::regex("^la?s?t?$", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto regex_all   = std::regex("^al?l?$",   std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);

    const auto regex_is_pixel = std::regex("px$", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto regex_is_percent = std::regex("[%]$", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);

    if( !std::regex_match(ImageSelectionStr, regex_none)
    &&  !std::regex_match(ImageSelectionStr, regex_first)
    &&  !std::regex_match(ImageSelectionStr, regex_last)
    &&  !std::regex_match(ImageSelectionStr, regex_all) ){
        throw std::invalid_argument("Image selection is not valid. Cannot continue.");
    }

    const auto RowsL = std::stod( RowsL_str );
    const auto RowsH = std::stod( RowsH_str );
    const auto ColsL = std::stod( ColsL_str );
    const auto ColsH = std::stod( ColsH_str );

    const auto RowsL_is_pixel = std::regex_match(RowsL_str, regex_is_pixel);
    const auto RowsH_is_pixel = std::regex_match(RowsH_str, regex_is_pixel);
    const auto ColsL_is_pixel = std::regex_match(ColsL_str, regex_is_pixel);
    const auto ColsH_is_pixel = std::regex_match(ColsH_str, regex_is_pixel);

    const auto RowsL_is_percent = std::regex_match(RowsL_str, regex_is_percent);
    const auto RowsH_is_percent = std::regex_match(RowsH_str, regex_is_percent);
    const auto ColsL_is_percent = std::regex_match(ColsL_str, regex_is_percent);
    const auto ColsH_is_percent = std::regex_match(ColsH_str, regex_is_percent);

    // Create a contour collection for the relevant images.
    contour_collection<double> cc;  // These are used only for purposes of cropping.
    {
        auto iap_it = DICOM_data.image_data.begin();
        if(false){
        }else if(std::regex_match(ImageSelectionStr, regex_none)){ iap_it = DICOM_data.image_data.end();
        }else if(std::regex_match(ImageSelectionStr, regex_last)){
            if(!DICOM_data.image_data.empty()) iap_it = std::prev(DICOM_data.image_data.end());
        }
        while(iap_it != DICOM_data.image_data.end()){
            for(auto &animg : (*iap_it)->imagecoll.images){

                const auto rows = animg.rows;
                const auto cols = animg.columns;

                if( (rows <= 0) || (cols <= 0) ){
                    throw std::invalid_argument("Passed an image with no spatial extent. Cannot continue.");
                }

                const auto Urow = animg.row_unit;
                const auto Drow = animg.pxl_dx;
                const auto Ucol = animg.col_unit;
                const auto Dcol = animg.pxl_dy;


                vec3<double> dRowL;
                vec3<double> dRowH;
                vec3<double> dColL;
                vec3<double> dColH;

                if(false){
                }else if(RowsL_is_pixel  ){  dRowL = Urow * Drow * RowsL;
                }else if(RowsL_is_percent){  dRowL = Urow * Drow * (rows-1) * RowsL / 100.0;
                }else{                       dRowL = Urow * RowsL;
                }

                if(false){
                }else if(ColsL_is_pixel  ){  dColL = Ucol * Dcol * ColsL; 
                }else if(ColsL_is_percent){  dColL = Ucol * Dcol * (cols-1) * ColsL / 100.0;
                }else{                       dColL = Ucol * ColsL; 
                }

                if(false){
                }else if(RowsH_is_pixel  ){  dRowH = Urow * -Drow * RowsH;
                }else if(RowsH_is_percent){  dRowH = Urow * -Drow * (rows-1) * RowsH / 100.0;
                }else{                       dRowH = Urow * -RowsH;
                }

                if(false){
                }else if(ColsH_is_pixel  ){  dColH = Ucol * -Dcol * ColsH;
                }else if(ColsH_is_percent){  dColH = Ucol * -Dcol * (cols-1) * ColsH / 100.0;
                }else{                       dColH = Ucol * -ColsH;
                }

                Encircle_Images_with_Contours_Opts opts;
                opts.inclusivity = Encircle_Images_with_Contours_Opts::Inclusivity::Centre;
                opts.contouroverlap = Encircle_Images_with_Contours_Opts::ContourOverlap::Disallow;

                std::list<std::reference_wrapper<planar_image<float,double>>> imgs;
                imgs.emplace_back( std::ref(animg) );

                auto metadata = animg.metadata;
                auto cc_new = Encircle_Images_with_Contours(imgs, opts, metadata, dRowL, dRowH, dColL, dColH);

                cc.contours.splice( cc.contours.end(), cc_new.contours );
            }
            ++iap_it;
            if(std::regex_match(ImageSelectionStr, regex_first)) break;
        }
    }
    std::list<std::reference_wrapper<contour_collection<double>>> cc_ROIs;
    cc_ROIs.emplace_back( std::ref(cc) );

    // Perform the crop.
    {
        auto iap_it = DICOM_data.image_data.begin();
        if(false){
        }else if(std::regex_match(ImageSelectionStr, regex_none)){ iap_it = DICOM_data.image_data.end();
        }else if(std::regex_match(ImageSelectionStr, regex_last)){
            if(!DICOM_data.image_data.empty()) iap_it = std::prev(DICOM_data.image_data.end());
        }
        while(iap_it != DICOM_data.image_data.end()){
            CropToROIsUserData ud;
            ud.row_margin = DICOMMargin;
            ud.col_margin = DICOMMargin;
            ud.ort_margin = DICOMMargin;

            if(!(*iap_it)->imagecoll.Compute_Images( ComputeCropToROIs, { },
                                                     cc_ROIs, &ud )){
                throw std::runtime_error("Unable to perform crop.");
            }
            ++iap_it;
            if(std::regex_match(ImageSelectionStr, regex_first)) break;
        }
    }

    return DICOM_data;
}
