//AutoCropImages.cc - A part of DICOMautomaton 2018. Written by hal clark.

#include <cmath>
#include <exception>
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
#include <vector>

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../YgorImages_Functors/Compute/CropToROIs.h"
#include "AutoCropImages.h"
#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorString.h"       //Needed for GetFirstRegex(...)



OperationDoc OpArgDocAutoCropImages(void){
    OperationDoc out;
    out.name = "AutoCropImages";

    out.desc = 
        "This operation crops image slices using image-specific metadata embedded within the image.";

    out.args.emplace_back();
    out.args.back() = IAWhitelistOpArgDoc();
    out.args.back().name = "ImageSelection";
    out.args.back().default_val = "all";
    

    out.args.emplace_back();
    out.args.back().name = "DICOMMargin";
    out.args.back().desc = "The amount of margin (in the DICOM coordinate system) to spare from cropping.";
    out.args.back().default_val = "0.0";
    out.args.back().expected = true;
    out.args.back().examples = { "0.1", "2.0", "-0.5", "20.0" };


    out.args.emplace_back();
    out.args.back().name = "RTIMAGE";
    out.args.back().desc = "If true, attempt to crop the image using information embedded in an RTIMAGE."
                      " This option cannot be used with the other options.";
    out.args.back().default_val = "true";
    out.args.back().expected = true;
    out.args.back().examples = { "true", "false" };


    return out;
}



Drover AutoCropImages(Drover DICOM_data, OperationArgPkg OptArgs, std::map<std::string,std::string> /*InvocationMetadata*/, std::string /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();

    const auto DICOMMargin = std::stod(OptArgs.getValueStr("DICOMMargin").value());

    const auto RTIMAGE_str = OptArgs.getValueStr("RTIMAGE").value();

    //-----------------------------------------------------------------------------------------------------------------
    const auto regex_true = std::regex("^tr?u?e?$", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);

    const auto RTIMAGE = std::regex_match(RTIMAGE_str, regex_true);


    auto IAs_all = All_IAs( DICOM_data );
    auto IAs = Whitelist( IAs_all, ImageSelectionStr );
    for(auto & iap_it : IAs){
        decltype((*iap_it)->imagecoll) cropped_imagecoll; // Cropped images get spliced into here temporarily.

        while( !( (*iap_it)->imagecoll.images.empty()) ){
            auto img_it = (*iap_it)->imagecoll.images.begin();

            auto img_plane = img_it->image_plane();

            // Create a contour collection for the relevant images.
            contour_collection<double> cc;  // These are used only for purposes of cropping.


            const auto rows = img_it->rows;
            const auto cols = img_it->columns;

            if( (rows <= 0) || (cols <= 0) ){
                throw std::invalid_argument("Passed an image with no spatial extent. Cannot continue.");
            }

            const auto Urow = img_it->row_unit;
            const auto Drow = img_it->pxl_dx;
            const auto Ucol = img_it->col_unit;
            const auto Dcol = img_it->pxl_dy;

            //Override crop specifications using metadata from the image.
            if(RTIMAGE){
                {
                    auto Modality     = img_it->GetMetadataValueAs<std::string>("Modality").value_or("");
                    auto RTImagePlane = img_it->GetMetadataValueAs<std::string>("RTImagePlane").value_or("");
                    if((Modality != "RTIMAGE") || (RTImagePlane != "NORMAL")){
                        throw std::domain_error("This routine can only handle RTIMAGES with RTImagePlane=NORMAL.");
                    }
                }

                const auto RTImageSID = std::stod( img_it->GetMetadataValueAs<std::string>("RTImageSID").value_or("1000.0") );
                const auto SAD = std::stod( img_it->GetMetadataValueAs<std::string>("RadiationMachineSAD").value_or("1000.0") );
                const auto SADToSID = RTImageSID / SAD; // Factor for scaling distance on SAD plane to distance on image panel plane.

                auto LeafJawPositions0 = img_it->GetMetadataValueAs<std::string>("ExposureSequence/BeamLimitingDeviceSequence#0/LeafJawPositions");
                auto LeafJawPositions1 = img_it->GetMetadataValueAs<std::string>("ExposureSequence/BeamLimitingDeviceSequence#1/LeafJawPositions");
                auto RTBeamLimitingDeviceType0 = img_it->GetMetadataValueAs<std::string>("ExposureSequence/BeamLimitingDeviceSequence#0/RTBeamLimitingDeviceType");
                auto RTBeamLimitingDeviceType1 = img_it->GetMetadataValueAs<std::string>("ExposureSequence/BeamLimitingDeviceSequence#1/RTBeamLimitingDeviceType");
                auto BeamLimitingDeviceAngle = img_it->GetMetadataValueAs<std::string>("BeamLimitingDeviceAngle");
                if(!LeafJawPositions0 || !LeafJawPositions1 || !RTBeamLimitingDeviceType0 || !RTBeamLimitingDeviceType1 || !BeamLimitingDeviceAngle){
                    throw std::domain_error("Unable to perform RTIMAGE auto-crop: lacking geometry data.");
                }

                //Figure out which jaw corresponds to which set of coordinates.
                decltype(LeafJawPositions0) x_jaws;
                decltype(LeafJawPositions1) y_jaws;
                const auto regex_x = std::regex(".*x.*", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
                const auto regex_y = std::regex(".*y.*", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);

                if(false){
                }else if( std::regex_match(RTBeamLimitingDeviceType0.value(), regex_x)
                       && std::regex_match(RTBeamLimitingDeviceType1.value(), regex_y) ){
                    x_jaws = LeafJawPositions0;
                    y_jaws = LeafJawPositions1;
                }else if( std::regex_match(RTBeamLimitingDeviceType1.value(), regex_x)
                       && std::regex_match(RTBeamLimitingDeviceType0.value(), regex_y) ){
                    x_jaws = LeafJawPositions1;
                    y_jaws = LeafJawPositions0;
                }else{
                    throw std::domain_error("Unable to perform RTIMAGE auto-crop: unknown or missing geometry specification.");
                }

                try{
                    auto x_coords = SplitStringToVector(x_jaws.value(), R"***(\\)***"_s, 'd');
                    auto y_coords = SplitStringToVector(y_jaws.value(), R"***(\\)***"_s, 'd');

                    auto rot_ang = std::stod( BeamLimitingDeviceAngle.value() ) * M_PI/180.0; // in radians.

                    
                    const auto x_lower = std::stod(x_coords.at(0)) * SADToSID;
                    const auto x_upper = std::stod(x_coords.at(1)) * SADToSID;
                    const auto y_lower = std::stod(y_coords.at(0)) * SADToSID;
                    const auto y_upper = std::stod(y_coords.at(1)) * SADToSID;

                    auto A = (vec2<double>(x_upper, y_upper)).rotate_around_z(rot_ang);
                    auto B = (vec2<double>(x_lower, y_lower)).rotate_around_z(rot_ang);

                    auto C = (vec2<double>(x_upper, y_lower)).rotate_around_z(rot_ang);
                    auto D = (vec2<double>(x_lower, y_upper)).rotate_around_z(rot_ang);

                    cc.contours.emplace_back();
                    cc.contours.back().closed = true;
                    cc.contours.back().points.emplace_back( Ucol * A.x + Urow * A.y );
                    cc.contours.back().points.emplace_back( Ucol * C.x + Urow * C.y );
                    cc.contours.back().points.emplace_back( Ucol * B.x + Urow * B.y );
                    cc.contours.back().points.emplace_back( Ucol * D.x + Urow * D.y );
                    cc.contours.back() = cc.contours.back().Project_Onto_Plane_Orthogonally(img_plane); //Ensure orthogonal positioning.

/*
                    // Save the contour for later viewing.
                    if(DICOM_data.contour_data == nullptr){
                        std::unique_ptr<Contour_Data> output (new Contour_Data());
                        DICOM_data.contour_data = std::move(output);
                    }
                    DICOM_data.contour_data->ccs.emplace_back();
                    DICOM_data.contour_data->ccs.back().Raw_ROI_name = "AutoCrop";
                    DICOM_data.contour_data->ccs.back().ROI_number = 10000; // TODO: find highest existing and ++ it.
                    DICOM_data.contour_data->ccs.back().Minimum_Separation = 1.0;
                    DICOM_data.contour_data->ccs.back().contours.emplace_back( cc.contours.back() );
*/

                }catch(const std::exception &e){
                    throw std::domain_error("Unable to perform RTIMAGE auto-crop: coordinate transforms: "_s + e.what());
                }
            }

            std::list<std::reference_wrapper<contour_collection<double>>> cc_ROIs;
            cc_ROIs.emplace_back( std::ref(cc) );

            //Pack the image into a shuttle by itself.
            decltype((*iap_it)->imagecoll) shtl;
            shtl.images.splice(shtl.images.end(), (*iap_it)->imagecoll.images, img_it);

            //Perform the crop.
            CropToROIsUserData ud;
            ud.row_margin = DICOMMargin;
            ud.col_margin = DICOMMargin;
            ud.ort_margin = DICOMMargin;

            if(!shtl.Compute_Images( ComputeCropToROIs, { },
                                     cc_ROIs, &ud )){
                throw std::runtime_error("Unable to perform crop.");
            }

            //Move the cropped image into a buffer for later insertion.
            if(!shtl.images.empty()){
                cropped_imagecoll.images.splice( cropped_imagecoll.images.end(), shtl.images );
            }
        }

        //Return the cropped images to the image collection.
        (*iap_it)->imagecoll.images.splice( (*iap_it)->imagecoll.images.end(), 
                                            cropped_imagecoll.images );
    }

    return DICOM_data;
}
