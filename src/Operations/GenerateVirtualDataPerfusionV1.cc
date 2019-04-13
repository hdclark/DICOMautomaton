//GenerateVirtualDataPerfusionV1.cc - A part of DICOMautomaton 2016. Written by hal clark.

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>    

#include "../Imebra_Shim.h"
#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "GenerateVirtualDataPerfusionV1.h"
#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorString.h"       //Needed for GetFirstRegex(...)


OperationDoc OpArgDocGenerateVirtualDataPerfusionV1(void){
    OperationDoc out;
    out.name = "GenerateVirtualDataPerfusionV1";

    out.desc = 
        "This operation generates data suitable for testing perfusion modeling operations. There are no specific checks in"
        " this code. Another operation performs the actual validation. You might be able to manually verify if the perfusion"
        " model admits a simple solution.";

    return out;
}

Drover GenerateVirtualDataPerfusionV1(Drover DICOM_data, OperationArgPkg , std::map<std::string,std::string>, std::string){

    using loaded_imgs_storage_t = decltype(DICOM_data.image_data);
    std::list<loaded_imgs_storage_t> loaded_imgs_storage;
    std::shared_ptr<Contour_Data> loaded_contour_data_storage = std::make_shared<Contour_Data>();


    // The test images are divided into sections. Some sections are for testing purposes, and others provide fake data
    // for the perfusion models (i.e., AIF and VIF).
    long int Rows = 20;
    long int Columns = 20;
    long int Channels = 1;

    const double SliceThickness = 1.0;
    const double SliceLocation  = 1.0;
    const double SpacingBetweenSlices = 1.0;
    const vec3<double> ImageAnchor(0.0, 0.0, 0.0);
    const vec3<double> ImagePosition(100.0,100.0,100.0);
    const vec3<double> ImageOrientationColumn = vec3<double>(1.0,0.0,0.0).unit();
    const vec3<double> ImageOrientationRow = vec3<double>(0.0,1.0,0.0).unit();
    const double ImagePixeldy = 1.0; //Spacing between adjacent rows.
    const double ImagePixeldx = 1.0; //Spacing between adjacent columns.
    const double ImageThickness = 1.0;

    long int InstanceNumber = 1; //Gets bumped for each image.
    long int SliceNumber    = 1; //Gets bumped at each temporal bump.
    long int ImageIndex     = 1; //For PET series. Not sure when to bump...
    const long int AcquisitionNumber = 1;

    // Temporal metadata.
    long int TemporalPositionIdentifier = 1;
    long int TemporalPositionIndex      = 1;
    long int NumberOfTemporalPositions  = 40;
    long int FrameTime = 1;
    double dt = 2.5;
    const std::string ContentDate = "20160706";
    const std::string ContentTime = "123056";

    // Other metadata.
    const double RescaleSlope = 1.0;
    const double RescaleIntercept = 0.0;
    const std::string OriginFilename = "/dev/null";
    const std::string PatientID = "VirtualDataPatientVersion1";
    const std::string StudyInstanceUID = PatientID + "_Study1";
    const std::string SeriesInstanceUID = StudyInstanceUID + "_Series1";
    const std::string SOPInstanceUID = Generate_Random_String_of_Length(6);
    const std::string FrameofReferenceUID = PatientID;
    const std::string Modality = "CT";


    // --- The virtual 'signal' image series ---
    loaded_imgs_storage.emplace_back();
    SliceNumber = 1;
    for(long int time_index = 0; time_index < NumberOfTemporalPositions; ++time_index, ++SliceNumber){
        const double t = dt * time_index;
        long int FrameReferenceTime = t * 1000.0;

        std::unique_ptr<Image_Array> out(new Image_Array());
        out->imagecoll.images.emplace_back();

        out->imagecoll.images.back().metadata["Filename"] = OriginFilename;

        out->imagecoll.images.back().metadata["PatientID"] = PatientID;
        out->imagecoll.images.back().metadata["StudyInstanceUID"] = StudyInstanceUID;
        out->imagecoll.images.back().metadata["SeriesInstanceUID"] = SeriesInstanceUID;
        out->imagecoll.images.back().metadata["SOPInstanceUID"] = SOPInstanceUID;

        out->imagecoll.images.back().metadata["dt"] = std::to_string(t);
        out->imagecoll.images.back().metadata["Rows"] = std::to_string(Rows);
        out->imagecoll.images.back().metadata["Columns"] = std::to_string(Columns);
        out->imagecoll.images.back().metadata["SliceThickness"] = std::to_string(SliceThickness);
        out->imagecoll.images.back().metadata["SliceNumber"] = std::to_string(SliceNumber);
        out->imagecoll.images.back().metadata["SliceLocation"] = std::to_string(SliceLocation);
        out->imagecoll.images.back().metadata["ImageIndex"] = std::to_string(ImageIndex);
        out->imagecoll.images.back().metadata["SpacingBetweenSlices"] = std::to_string(SpacingBetweenSlices);
        out->imagecoll.images.back().metadata["ImagePositionPatient"] = std::to_string(ImagePosition.x) + "\\"
                                                                      + std::to_string(ImagePosition.y) + "\\"
                                                                      + std::to_string(ImagePosition.z);
        out->imagecoll.images.back().metadata["ImageOrientationPatient"] = std::to_string(ImagePosition.x) + "\\"
                                                                         + std::to_string(ImagePosition.y) + "\\"
                                                                         + std::to_string(ImagePosition.z);
        out->imagecoll.images.back().metadata["PixelSpacing"] = std::to_string(ImagePixeldx) + "\\" + std::to_string(ImagePixeldy);
        out->imagecoll.images.back().metadata["FrameofReferenceUID"] = FrameofReferenceUID;

        out->imagecoll.images.back().metadata["StudyTime"] = ContentTime;
        out->imagecoll.images.back().metadata["SeriesTime"] = ContentTime;
        out->imagecoll.images.back().metadata["AcquisitionTime"] = ContentTime;
        out->imagecoll.images.back().metadata["ContentTime"] = ContentTime;

        out->imagecoll.images.back().metadata["StudyDate"] = ContentDate;
        out->imagecoll.images.back().metadata["SeriesDate"] = ContentDate;
        out->imagecoll.images.back().metadata["AcquisitionDate"] = ContentDate;
        out->imagecoll.images.back().metadata["ContentDate"] = ContentDate;

        out->imagecoll.images.back().metadata["Modality"] = Modality;

        // ---

        out->imagecoll.images.back().init_orientation(ImageOrientationRow,ImageOrientationColumn);
        out->imagecoll.images.back().init_buffer(Rows, Columns, Channels);
        out->imagecoll.images.back().init_spatial(ImagePixeldx,ImagePixeldx,ImageThickness, ImageAnchor, ImagePosition);

        for(long int row = 0; row < Rows; ++row){
            for(long int col = 0; col < Columns; ++col){
                for(long int chnl = 0; chnl < Channels; ++chnl){

                    float OutgoingPixelValue = std::numeric_limits<float>::quiet_NaN();

                    //Strip 1: linear-changing spatially, constant temporally.
                    if(isininc(0,row,3)){
                        OutgoingPixelValue = static_cast<float>(col);

                    //Strip 2: constant spatially, linear-changing temporally.
                    }else if(isininc(4,row,7)){
                        OutgoingPixelValue = static_cast<float>(t);

                    //Strip 3: constant spatially, square-changing temporally.
                    }else if(isininc(8,row,11)){
                        OutgoingPixelValue = static_cast<float>(t*t / 250.0);

                    //Strip 4: constant spatially, Gaussian temporally.
                    }else if(isininc(12,row,15)){
                        const double sigma  = 20.0; //seconds.
                        const double centre = 50.0; //seconds.
                        const auto exparg = -1.0 * std::pow(t-centre, 2.0) / std::pow(sigma, 2.0);
                        OutgoingPixelValue = static_cast<float>( std::exp(exparg) );

                    //Strip 5: AIF and VIF; constant spatially, Gaussian temporally.
                    }else if(isininc(16,row,19)){
                        //The AIF.
                        if(col < 10){
                            const double sigma  = 10.0; //seconds.
                            const double centre = 25.0; //seconds.
                            const auto exparg = -1.0 * std::pow(t-centre, 2.0) / std::pow(sigma, 2.0);
                            OutgoingPixelValue = static_cast<float>( std::exp(exparg) );

                        //The VIF.
                        }else{
                            const double sigma  = 10.0; //seconds.
                            const double centre = 45.0; //seconds.
                            const auto exparg = -1.0 * std::pow(t-centre, 2.0) / std::pow(sigma, 2.0);
                            OutgoingPixelValue = static_cast<float>( std::exp(exparg) );
                        }

                    }else{
                        throw std::runtime_error("Image dimensions have been changed without changing the pixel definitions.");
                    }

                    out->imagecoll.images.back().reference(row,col,chnl) = OutgoingPixelValue;
                } //Loop over channels.
            } //Loop over columns.
        } //Loop over rows.

        loaded_imgs_storage.back().push_back( std::move( out ) );
    }


    //Collate each group of images into a single set, if possible. Also stuff the correct contour data in the same set.
    for(auto &loaded_img_set : loaded_imgs_storage){
        if(loaded_img_set.empty()) continue;

        auto collated_imgs = Collate_Image_Arrays(loaded_img_set);
        if(!collated_imgs){
            throw std::runtime_error("Unable to collate images. Virtual data should never cause this error.");
        }

        DICOM_data.image_data.emplace_back(std::move(collated_imgs));
    }


    //Create contours.
    {
        std::unique_ptr<Contour_Data> output (new Contour_Data());

        //Get an image to base contours on. (This just make it slightly easier to specify contours.)
        auto animgcoll = std::ref(DICOM_data.image_data.back()->imagecoll);
        auto animg = std::ref(DICOM_data.image_data.back()->imagecoll.images.front());

        long int ROINumberNidus = 1;

        //AIF.
        {
            const std::string ROIName = "Abdominal_Aorta";
            const auto ROINumber = ROINumberNidus++;

            contour_collection<double> cc;
            {
                contour_of_points<double> shtl;
                shtl.closed = true;
                shtl.points.push_back(animg.get().position(/*row=*/16, /*column=*/0 ));
                shtl.points.push_back(animg.get().position(/*row=*/19, /*column=*/0 ));
                shtl.points.push_back(animg.get().position(/*row=*/19, /*column=*/9 ));
                shtl.points.push_back(animg.get().position(/*row=*/16, /*column=*/9 ));
                shtl.Reorient_Counter_Clockwise();
                shtl.metadata = animgcoll.get().get_common_metadata({});
                shtl.metadata["ROINumber"] = std::to_string(ROINumber);
                shtl.metadata["ROIName"] = ROIName;
                shtl.metadata["MinimumSeparation"] = animg.get().metadata["ImageThickness"];
                cc.contours.push_back(std::move(shtl));
            }
            output->ccs.emplace_back( );
            output->ccs.back() = cc;
            output->ccs.back().Raw_ROI_name = ROIName; 
            output->ccs.back().ROI_number = ROINumber;
        }


        //VIF.
        {
            const std::string ROIName = "Hepatic_Portal_Vein";
            const auto ROINumber = ROINumberNidus++;

            contour_collection<double> cc;
            {
                contour_of_points<double> shtl;
                shtl.closed = true;
                shtl.points.push_back(animg.get().position(/*row=*/16, /*column=*/10 ));
                shtl.points.push_back(animg.get().position(/*row=*/19, /*column=*/10 ));
                shtl.points.push_back(animg.get().position(/*row=*/19, /*column=*/19 ));
                shtl.points.push_back(animg.get().position(/*row=*/16, /*column=*/19 ));
                shtl.Reorient_Counter_Clockwise();
                shtl.metadata = animgcoll.get().get_common_metadata({});
                shtl.metadata["ROINumber"] = std::to_string(ROINumber);
                shtl.metadata["ROIName"] = ROIName;
                shtl.metadata["MinimumSeparation"] = animg.get().metadata["ImageThickness"];
                cc.contours.push_back(std::move(shtl));
            }
            output->ccs.emplace_back( );
            output->ccs.back() = cc;
            output->ccs.back().Raw_ROI_name = ROIName; 
            output->ccs.back().ROI_number = ROINumber;
        }


        //Body.
        {
            const std::string ROIName = "Body";
            const auto ROINumber = ROINumberNidus++;

            contour_collection<double> cc;
            {
                contour_of_points<double> shtl;
                shtl.closed = true;
                shtl.points.push_back(animg.get().position(/*row=*/ 0, /*column=*/ 0 ));
                shtl.points.push_back(animg.get().position(/*row=*/19, /*column=*/ 0 ));
                shtl.points.push_back(animg.get().position(/*row=*/19, /*column=*/19 ));
                shtl.points.push_back(animg.get().position(/*row=*/ 0, /*column=*/19 ));
                shtl.Reorient_Counter_Clockwise();
                shtl.metadata = animgcoll.get().get_common_metadata({});
                shtl.metadata["ROINumber"] = std::to_string(ROINumber);
                shtl.metadata["ROIName"] = ROIName;
                shtl.metadata["MinimumSeparation"] = animg.get().metadata["ImageThickness"];
                cc.contours.push_back(std::move(shtl));
            }
            output->ccs.emplace_back( );
            output->ccs.back() = cc;
            output->ccs.back().Raw_ROI_name = ROIName; 
            output->ccs.back().ROI_number = ROINumber;
        }


        DICOM_data.contour_data = std::move(output);
    }

    return DICOM_data;
}
