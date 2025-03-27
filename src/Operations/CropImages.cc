//CropImages.cc - A part of DICOMautomaton 2018. Written by hal clark.

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
#include "../YgorImages_Functors/Compute/CropToROIs.h"
#include "CropImages.h"
#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.



OperationDoc OpArgDocCropImages(){
    OperationDoc out;
    out.name = "CropImages";
    out.tags.emplace_back("category: image processing");
    out.desc = "This operation crops image slices in either pixel or DICOM coordinate spaces.";

    out.args.emplace_back();
    out.args.back() = IAWhitelistOpArgDoc();
    out.args.back().name = "ImageSelection";
    out.args.back().default_val = "all";
    

    out.args.emplace_back();
    out.args.back().name = "RowsL";
    out.args.back().desc = "The number of rows to remove, starting with the first row. Can be absolute (px), percentage (%), or"
                      " distance in terms of the DICOM coordinate system. Note the DICOM coordinate system can be flipped, so"
                      " the first row can be either on the top or bottom of the image.";
    out.args.back().default_val = "0px";
    out.args.back().expected = true;
    out.args.back().examples = { "0px", "10px", "100px", "15%", "15.75%", "123.45" };


    out.args.emplace_back();
    out.args.back().name = "RowsH";
    out.args.back().desc = "The number of rows to remove, starting with the last row. Can be absolute (px), percentage (%), or"
                      " distance in terms of the DICOM coordinate system. Note the DICOM coordinate system can be flipped, so"
                      " the first row can be either on the top or bottom of the image.";
    out.args.back().default_val = "0px";
    out.args.back().expected = true;
    out.args.back().examples = { "0px", "10px", "100px", "15%", "15.75%", "123.45" };


    out.args.emplace_back();
    out.args.back().name = "ColumnsL";
    out.args.back().desc = "The number of columns to remove, starting with the first column. Can be absolute (px), percentage (%), or"
                      " distance in terms of the DICOM coordinate system. Note the DICOM coordinate system can be flipped, so"
                      " the first column can be either on the top or bottom of the image.";
    out.args.back().default_val = "0px";
    out.args.back().expected = true;
    out.args.back().examples = { "0px", "10px", "100px", "15%", "15.75%", "123.45" };


    out.args.emplace_back();
    out.args.back().name = "ColumnsH";
    out.args.back().desc = "The number of columns to remove, starting with the last column. Can be absolute (px), percentage (%), or"
                      " distance in terms of the DICOM coordinate system. Note the DICOM coordinate system can be flipped, so"
                      " the first column can be either on the top or bottom of the image.";
    out.args.back().default_val = "0px";
    out.args.back().expected = true;
    out.args.back().examples = { "0px", "10px", "100px", "15%", "15.75%", "123.45" };


    out.args.emplace_back();
    out.args.back().name = "DICOMMargin";
    out.args.back().desc = "The amount of margin (in the DICOM coordinate system) to spare from cropping.";
    out.args.back().default_val = "0.0";
    out.args.back().expected = true;
    out.args.back().examples = { "0.1", "2.0", "-0.5", "20.0" };

    return out;
}



bool CropImages(Drover &DICOM_data,
                  const OperationArgPkg& OptArgs,
                  std::map<std::string, std::string>& /*InvocationMetadata*/,
                  const std::string& /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto RowsL_str = OptArgs.getValueStr("RowsL").value();
    const auto RowsH_str = OptArgs.getValueStr("RowsH").value();
    const auto ColsL_str = OptArgs.getValueStr("ColumnsL").value();
    const auto ColsH_str = OptArgs.getValueStr("ColumnsH").value();

    const auto DICOMMargin = std::stod(OptArgs.getValueStr("DICOMMargin").value());
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();

    //-----------------------------------------------------------------------------------------------------------------
    const auto regex_is_pixel = Compile_Regex("px$");
    const auto regex_is_percent = Compile_Regex("[%]$");

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
        auto IAs_all = All_IAs( DICOM_data );
        auto IAs = Whitelist( IAs_all, ImageSelectionStr );
        for(auto & iap_it : IAs){
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

                if(RowsL_is_pixel){          dRowL = Ucol * Dcol * RowsL;
                }else if(RowsL_is_percent){  dRowL = Ucol * Dcol * (rows-1) * RowsL / 100.0;
                }else{                       dRowL = Ucol * RowsL;
                }

                if(ColsL_is_pixel){          dColL = Urow * Drow * ColsL; 
                }else if(ColsL_is_percent){  dColL = Urow * Drow * (cols-1) * ColsL / 100.0;
                }else{                       dColL = Urow * ColsL; 
                }

                if(RowsH_is_pixel){          dRowH = Ucol * -Dcol * RowsH;
                }else if(RowsH_is_percent){  dRowH = Ucol * -Dcol * (rows-1) * RowsH / 100.0;
                }else{                       dRowH = Ucol * -RowsH;
                }

                if(ColsH_is_pixel){          dColH = Urow * -Drow * ColsH;
                }else if(ColsH_is_percent){  dColH = Urow * -Drow * (cols-1) * ColsH / 100.0;
                }else{                       dColH = Urow * -ColsH;
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
        }
    }
    std::list<std::reference_wrapper<contour_collection<double>>> cc_ROIs;
    cc_ROIs.emplace_back( std::ref(cc) );

    // Perform the crop.
    {
        auto IAs_all = All_IAs( DICOM_data );
        auto IAs = Whitelist( IAs_all, ImageSelectionStr );
        for(auto & iap_it : IAs){
            CropToROIsUserData ud;
            ud.row_margin = DICOMMargin;
            ud.col_margin = DICOMMargin;
            ud.ort_margin = DICOMMargin;

            if(!(*iap_it)->imagecoll.Compute_Images( ComputeCropToROIs, { },
                                                     cc_ROIs, &ud )){
                throw std::runtime_error("Unable to perform crop.");
            }
        }
    }

    return true;
}
