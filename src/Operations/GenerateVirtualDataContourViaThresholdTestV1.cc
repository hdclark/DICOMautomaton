//GenerateVirtualDataContourViaThresholdTestV1.cc - A part of DICOMautomaton 2016. Written by hal clark.

#include <algorithm>
#include <list>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>    
#include <cstdint>

#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorLog.h"
#include "YgorString.h"       //Needed for GetFirstRegex(...)

#include "../Imebra_Shim.h"
#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "GenerateVirtualDataContourViaThresholdTestV1.h"


OperationDoc OpArgDocGenerateVirtualDataContourViaThresholdTestV1(){
    OperationDoc out;
    out.name = "GenerateVirtualDataContourViaThresholdTestV1";

    out.desc = 
        "This operation generates data suitable for testing the ContourViaThreshold operation.";

    return out;
}

bool GenerateVirtualDataContourViaThresholdTestV1(Drover &DICOM_data,
                                                    const OperationArgPkg&,
                                                    std::map<std::string, std::string>& /*InvocationMetadata*/,
                                                    const std::string&){

    using loaded_imgs_storage_t = decltype(DICOM_data.image_data);
    std::list<loaded_imgs_storage_t> loaded_imgs_storage;
    std::shared_ptr<Contour_Data> loaded_contour_data_storage = std::make_shared<Contour_Data>();

    int64_t Rows = 20;
    int64_t Columns = 20;
    int64_t Channels = 1;

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

    //int64_t InstanceNumber = 1; //Gets bumped for each image.
    int64_t SliceNumber    = 1; //Gets bumped at each temporal bump.
    int64_t ImageIndex     = 1; //For PET series. Not sure when to bump...
    //const int64_t AcquisitionNumber = 1;

    // Temporal metadata.
    //int64_t TemporalPositionIdentifier = 1;
    //int64_t TemporalPositionIndex      = 1;
    int64_t NumberOfTemporalPositions  = 1;
    //int64_t FrameTime = 1;
    double dt = 1.0;
    const std::string ContentDate = "20170208";
    const std::string ContentTime = "150443";

    // Other metadata.
    //const double RescaleSlope = 1.0;
    //const double RescaleIntercept = 0.0;
    const std::string OriginFilename = "/dev/null";
    const std::string PatientID = "VirtualDataContourViaThresholdTestingVersion1";
    const std::string StudyInstanceUID = Generate_Random_UID(60);
    const std::string SeriesInstanceUID = Generate_Random_UID(60);
    const std::string FrameOfReferenceUID = Generate_Random_UID(60);
    const std::string Modality = "CT";


    // --- The virtual 'signal' image series ---
    loaded_imgs_storage.emplace_back();
    SliceNumber = 1;
    for(int64_t time_index = 0; time_index < NumberOfTemporalPositions; ++time_index, ++SliceNumber){
        const double t = dt * time_index;
        //int64_t FrameReferenceTime = t * 1000.0;

        const std::string SOPInstanceUID = Generate_Random_UID(60);

        auto out = std::make_unique<Image_Array>();
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
        out->imagecoll.images.back().metadata["ImageOrientationPatient"] = std::to_string(ImageOrientationRow.x) + "\\"
                                                                         + std::to_string(ImageOrientationRow.y) + "\\"
                                                                         + std::to_string(ImageOrientationRow.z) + "\\"
                                                                         + std::to_string(ImageOrientationColumn.x) + "\\"
                                                                         + std::to_string(ImageOrientationColumn.y) + "\\"
                                                                         + std::to_string(ImageOrientationColumn.z);
        out->imagecoll.images.back().metadata["PixelSpacing"] = std::to_string(ImagePixeldx) + "\\" + std::to_string(ImagePixeldy);
        out->imagecoll.images.back().metadata["FrameOfReferenceUID"] = FrameOfReferenceUID;

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
        out->imagecoll.images.back().init_spatial(ImagePixeldx,ImagePixeldy,ImageThickness, ImageAnchor, ImagePosition);

        for(int64_t row = 0; row < Rows; ++row){
            for(int64_t col = 0; col < Columns; ++col){
                for(int64_t chnl = 0; chnl < Channels; ++chnl){

                    float OutgoingPixelValue = 0.0;

                    //Top-left: solid square.
                    if(isininc(2,row,8) && isininc(2,col,8)){
                        OutgoingPixelValue = 1.0;
                    }

                    //Top-right: square with hole.
                    if(isininc(2,row,8) && isininc(12,col,18)){
                        OutgoingPixelValue = 1.0;
                    }
                    if(isininc(4,row,6) && isininc(14,col,16)){
                        OutgoingPixelValue = 0.0;
                    }

                    //Bottom-left: pinch.
                    if(isininc(12,row,14) && isininc(2,col,4)){
                        OutgoingPixelValue = 1.0;
                    }
                    if(isininc(15,row,17) && isininc(5,col,7)){
                        OutgoingPixelValue = 1.0;
                    }

                    //Bottom-right: pinch.
                    if(isininc(15,row,17) && isininc(12,col,14)){
                        OutgoingPixelValue = 1.0;
                    }
                    if(isininc(12,row,14) && isininc(15,col,17)){
                        OutgoingPixelValue = 1.0;
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

    return true;
}
