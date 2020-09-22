//GenerateVirtualDataImageSphereV1.cc - A part of DICOMautomaton 2019. Written by hal clark.

#include <algorithm>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>    

#include "../Imebra_Shim.h"
#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "Explicator.h"       //Needed for Explicator class.
#include "GenerateVirtualDataImageSphereV1.h"
#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorString.h"       //Needed for GetFirstRegex(...)


OperationDoc OpArgDocGenerateVirtualDataImageSphereV1(){
    OperationDoc out;
    out.name = "GenerateVirtualDataImageSphereV1";
    out.desc = 
        "This operation generates a bitmap image of a sphere."
        " It can be used for testing how images are quantified or transformed.";

    return out;
}

Drover GenerateVirtualDataImageSphereV1(Drover DICOM_data,
                                        const OperationArgPkg&,
                                        const std::map<std::string, std::string>&,
                                        const std::string& FilenameLex){

    Explicator X(FilenameLex);

    using loaded_imgs_storage_t = decltype(DICOM_data.image_data);
    std::list<loaded_imgs_storage_t> loaded_imgs_storage;
    //std::shared_ptr<Contour_Data> loaded_contour_data_storage = std::make_shared<Contour_Data>();

    // The test images are divided into sections.
    const long int Images   = 100;
    const long int Rows     = 100;
    const long int Columns  = 100;
    const long int Channels = 1;

    const double SpacingBetweenSlices = 1.0;
    const vec3<double> ImageAnchor(0.0, 0.0, 0.0);
    vec3<double> ImagePosition(100.0,100.0,100.0);
    const vec3<double> ImageOrientationColumn = vec3<double>(1.0,0.0,0.0).unit();
    const vec3<double> ImageOrientationRow    = vec3<double>(0.0,1.0,0.0).unit();
    const vec3<double> ImageOrientationOrtho  = ImageOrientationColumn.Cross( ImageOrientationRow ).unit();
    const double ImagePixeldy = 1.0; //Spacing between adjacent rows.
    const double ImagePixeldx = 1.0; //Spacing between adjacent columns.
    const double SliceThickness = 1.0;

    const vec3<double> SphereCentre(150.0, 150.0, 150.0);
    const double SphereRadius = 25.0;

    long int InstanceNumber = 1; //Gets bumped for each image.
    const long int AcquisitionNumber = 1;

    // Temporal metadata.
    const std::string ContentDate = "20190226";
    const std::string ContentTime = "195741";

    // Other metadata.
    const double RescaleSlope = 1.0;
    const double RescaleIntercept = 0.0;
    const std::string OriginFilename = "/dev/null";
    const std::string PatientID = "VirtualDataImageSphereVersion1";
    const std::string StudyInstanceUID = Generate_Random_UID(60);
    const std::string SeriesInstanceUID = Generate_Random_UID(60);
    const std::string FrameOfReferenceUID = Generate_Random_UID(60);
    const std::string Modality = "CT";

    // --- The virtual 'signal' image series ---
    loaded_imgs_storage.emplace_back();
    for(long int img_index = 0; img_index < Images; ++img_index){
        const std::string SOPInstanceUID = Generate_Random_UID(60);

        auto out = std::make_unique<Image_Array>();
        out->imagecoll.images.emplace_back();

        out->imagecoll.images.back().metadata["Filename"] = OriginFilename;

        out->imagecoll.images.back().metadata["PatientID"] = PatientID;
        out->imagecoll.images.back().metadata["StudyInstanceUID"] = StudyInstanceUID;
        out->imagecoll.images.back().metadata["SeriesInstanceUID"] = SeriesInstanceUID;
        out->imagecoll.images.back().metadata["SOPInstanceUID"] = SOPInstanceUID;

        out->imagecoll.images.back().metadata["Rows"] = std::to_string(Rows);
        out->imagecoll.images.back().metadata["Columns"] = std::to_string(Columns);
        out->imagecoll.images.back().metadata["SliceThickness"] = std::to_string(SliceThickness);
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

        out->imagecoll.images.back().init_orientation(ImageOrientationRow, ImageOrientationColumn);
        out->imagecoll.images.back().init_buffer(Rows, Columns, Channels);
        out->imagecoll.images.back().init_spatial(ImagePixeldx, ImagePixeldy, SliceThickness, ImageAnchor, ImagePosition);

        for(long int row = 0; row < Rows; ++row){
            for(long int col = 0; col < Columns; ++col){
                const auto R = out->imagecoll.images.back().position(row,col);
                const auto dist = R.distance(SphereCentre);
                for(long int chnl = 0; chnl < Channels; ++chnl){
                    const auto val = (dist < SphereRadius) ? 1.0 : 0.0;
                    out->imagecoll.images.back().reference(row,col,chnl) = val;
                } //Loop over channels.
            } //Loop over columns.
        } //Loop over rows.

        ImagePosition += ImageOrientationOrtho * SpacingBetweenSlices;
        ++InstanceNumber;

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

    //Create an empty contour set.
    DICOM_data.Ensure_Contour_Data_Allocated();

    return DICOM_data;
}
