//GenerateVirtualDataDoseStairsV1.cc - A part of DICOMautomaton 2017. Written by hal clark.

#include <algorithm>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>    
#include <cstdint>

#include "Explicator.h"       //Needed for Explicator class.

#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorString.h"       //Needed for GetFirstRegex(...)

#include "../Imebra_Shim.h"
#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "GenerateVirtualDataDoseStairsV1.h"


OperationDoc OpArgDocGenerateVirtualDataDoseStairsV1(){
    OperationDoc out;
    out.name = "GenerateVirtualDataDoseStairsV1";

    out.tags.emplace_back("category: image processing");
    out.tags.emplace_back("category: radiation dose");
    out.tags.emplace_back("category: virtual phantom");

    out.desc = 
        "This operation generates a dosimetric stairway. It can be used for testing how dosimetric data is transformed.";

    return out;
}

bool GenerateVirtualDataDoseStairsV1(Drover &DICOM_data,
                                       const OperationArgPkg&,
                                       std::map<std::string, std::string>& /*InvocationMetadata*/,
                                       const std::string& FilenameLex){

    Explicator X(FilenameLex);

    using loaded_imgs_storage_t = decltype(DICOM_data.image_data);
    std::list<loaded_imgs_storage_t> loaded_imgs_storage;
    //std::shared_ptr<Contour_Data> loaded_contour_data_storage = std::make_shared<Contour_Data>();

    const float Dmax = 70.0; //Gray.

    // The test images are divided into sections.
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
    const std::string ContentDate = "20160706";
    const std::string ContentTime = "123056";

    // Other metadata.
    //const double RescaleSlope = 1.0;
    //const double RescaleIntercept = 0.0;
    const std::string OriginFilename = "/dev/null";
    const std::string PatientID = "VirtualDataPatientVersion1";
    const std::string StudyInstanceUID = Generate_Random_UID(31);
    const std::string SeriesInstanceUID = Generate_Random_UID(31);
    const std::string FrameOfReferenceUID = Generate_Random_UID(60);
    const std::string SOPClassUID = "1.2.840.10008.5.1.4.1.1.481.2"; // RT Dose IOD.
    const std::string Modality = "RTDOSE";


    // --- The virtual 'signal' image series ---
    loaded_imgs_storage.emplace_back();
    SliceNumber = 1;
    for(int64_t time_index = 0; time_index < 1; ++time_index, ++SliceNumber){
        const std::string SOPInstanceUID = Generate_Random_UID(60);

        auto out = std::make_unique<Image_Array>();
        out->imagecoll.images.emplace_back();

        out->imagecoll.images.back().metadata["Filename"] = OriginFilename;

        out->imagecoll.images.back().metadata["PatientID"] = PatientID;
        out->imagecoll.images.back().metadata["StudyInstanceUID"] = StudyInstanceUID;
        out->imagecoll.images.back().metadata["SeriesInstanceUID"] = SeriesInstanceUID;
        out->imagecoll.images.back().metadata["SOPInstanceUID"] = SOPInstanceUID;
        out->imagecoll.images.back().metadata["SOPClassUID"] = SOPClassUID;

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
        out->imagecoll.images.back().metadata["PixelSpacing"] = std::to_string(ImagePixeldy) + "\\" + std::to_string(ImagePixeldx);
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
                    auto OutgoingPixelValue = static_cast<float>(col + row * Columns + chnl * Columns * Rows); 
                    OutgoingPixelValue *= Dmax / (Rows*Columns*Channels - 1); // Rescale to [0,Dmax].
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
    int64_t ROINumberNidus = 1;
    {
        const std::string ROIName = "Body";
        const int64_t ROINumber = ROINumberNidus++;

        auto output = std::make_unique<Contour_Data>();

        std::list<std::reference_wrapper<planar_image<float,double>>> imgs;
        for(auto &animg : DICOM_data.image_data.back()->imagecoll.images){
            imgs.emplace_back( std::ref(animg) );
        }

        Encircle_Images_with_Contours_Opts opts;
        opts.inclusivity = Encircle_Images_with_Contours_Opts::Inclusivity::Centre;
        opts.contouroverlap = Encircle_Images_with_Contours_Opts::ContourOverlap::Allow;
        
        std::map<std::string,std::string> metadata;
        metadata = DICOM_data.image_data.back()->imagecoll.get_common_metadata({});
        metadata["ROIName"] = ROIName;
        metadata["NormalizedROIName"] = X(ROIName);
        metadata["MinimumSeparation"] = metadata["SliceThickness"];
        metadata["ROINumber"] = std::to_string(ROINumber);

        auto cc = Encircle_Images_with_Contours(imgs, opts, metadata);

        output->ccs.emplace_back( );
        output->ccs.back() = cc;

        DICOM_data.contour_data = std::move(output);
    }

    return true;
}
