//GenerateVirtualDataPerfusionV1.cc - A part of DICOMautomaton 2016. Written by hal clark.

#include <iostream>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <string>    
#include <vector>
#include <set> 
#include <map>
#include <unordered_map>
#include <list>
#include <functional>
#include <thread>
#include <array>
#include <mutex>
#include <limits>
#include <cmath>

#include <getopt.h>           //Needed for 'getopts' argument parsing.
#include <cstdlib>            //Needed for exit() calls.
#include <utility>            //Needed for std::pair.
#include <algorithm>
#include <experimental/optional>

#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>

#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMathPlottingGnuplot.h" //Needed for YgorMathPlottingGnuplot::*.
#include "YgorMathChebyshev.h" //Needed for cheby_approx class.
#include "YgorStats.h"        //Needed for Stats:: namespace.
#include "YgorFilesDirs.h"    //Needed for Does_File_Exist_And_Can_Be_Read(...), etc..
#include "YgorContainers.h"   //Needed for bimap class.
#include "YgorPerformance.h"  //Needed for YgorPerformance_dt_from_last().
#include "YgorAlgorithms.h"   //Needed for For_Each_In_Parallel<..>(...)
#include "YgorArguments.h"    //Needed for ArgumentHandler class.
#include "YgorString.h"       //Needed for GetFirstRegex(...)
#include "YgorImages.h"
#include "YgorImagesIO.h"
#include "YgorImagesPlotting.h"

#include "Explicator.h"       //Needed for Explicator class.

#include "../Structs.h"

#include "../Imebra_Shim.h"

#include "../YgorImages_Functors/Grouping/Misc_Functors.h"

#include "../YgorImages_Functors/Processing/DCEMRI_AUC_Map.h"
#include "../YgorImages_Functors/Processing/DCEMRI_S0_Map.h"
#include "../YgorImages_Functors/Processing/DCEMRI_T1_Map.h"
#include "../YgorImages_Functors/Processing/Highlight_ROI_Voxels.h"
#include "../YgorImages_Functors/Processing/Kitchen_Sink_Analysis.h"
#include "../YgorImages_Functors/Processing/IVIMMRI_ADC_Map.h"
#include "../YgorImages_Functors/Processing/Time_Course_Slope_Map.h"
#include "../YgorImages_Functors/Processing/CT_Perfusion_Clip_Search.h"
#include "../YgorImages_Functors/Processing/CT_Perf_Pixel_Filter.h"
#include "../YgorImages_Functors/Processing/CT_Convert_NaNs_to_Air.h"
#include "../YgorImages_Functors/Processing/Min_Pixel_Value.h"
#include "../YgorImages_Functors/Processing/Max_Pixel_Value.h"
#include "../YgorImages_Functors/Processing/CT_Reasonable_HU_Window.h"
#include "../YgorImages_Functors/Processing/Slope_Difference.h"
#include "../YgorImages_Functors/Processing/Centralized_Moments.h"
#include "../YgorImages_Functors/Processing/Logarithmic_Pixel_Scale.h"
#include "../YgorImages_Functors/Processing/Per_ROI_Time_Courses.h"
#include "../YgorImages_Functors/Processing/DBSCAN_Time_Courses.h"
#include "../YgorImages_Functors/Processing/In_Image_Plane_Bilinear_Supersample.h"
#include "../YgorImages_Functors/Processing/In_Image_Plane_Bicubic_Supersample.h"
#include "../YgorImages_Functors/Processing/In_Image_Plane_Pixel_Decimate.h"
#include "../YgorImages_Functors/Processing/Cross_Second_Derivative.h"
#include "../YgorImages_Functors/Processing/Liver_Pharmacokinetic_Model_5Param_Linear.h"
#include "../YgorImages_Functors/Processing/Liver_Pharmacokinetic_Model_5Param_Cheby.h"
#include "../YgorImages_Functors/Processing/Orthogonal_Slices.h"

#include "../YgorImages_Functors/Transform/DCEMRI_C_Map.h"
#include "../YgorImages_Functors/Transform/DCEMRI_Signal_Difference_C.h"
#include "../YgorImages_Functors/Transform/CT_Perfusion_Signal_Diff.h"
#include "../YgorImages_Functors/Transform/DCEMRI_S0_Map_v2.h"
#include "../YgorImages_Functors/Transform/DCEMRI_T1_Map_v2.h"
#include "../YgorImages_Functors/Transform/Pixel_Value_Histogram.h"
#include "../YgorImages_Functors/Transform/Subtract_Spatially_Overlapping_Images.h"

#include "../YgorImages_Functors/Compute/Per_ROI_Time_Courses.h"
#include "../YgorImages_Functors/Compute/Contour_Similarity.h"

#include "GenerateVirtualDataPerfusionV1.h"


std::list<OperationArgDoc> OpArgDocGenerateVirtualDataPerfusionV1(void){
    return std::list<OperationArgDoc>();
}

Drover GenerateVirtualDataPerfusionV1(Drover DICOM_data, OperationArgPkg OptArgs, std::map<std::string,std::string> /*InvocationMetadata*/, std::string /*FilenameLex*/){

    //This operation generates data suitable for testing perfusion modeling operations. There are no specific checks in
    // this code. Another operation performs the actual validation. You might be able to manually verify if the perfusion
    // model admits a simple solution.

    typedef decltype(DICOM_data.image_data) loaded_imgs_storage_t;
    std::list<loaded_imgs_storage_t> loaded_imgs_storage;
    std::shared_ptr<Contour_Data> loaded_contour_data_storage = std::make_shared<Contour_Data>();


    // The test images are divided into sections. Some sections are for testing purposes, and others provide fake data
    // for the perfusion models (i.e., AIF and VIF).
    long int Rows = 50;
    long int Columns = 50;
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

        out->filename = OriginFilename;

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
                    if(isininc(0,row,9)){
                        OutgoingPixelValue = static_cast<float>(col);

                    //Strip 2: constant spatially, linear-changing temporally.
                    }else if(isininc(10,row,19)){
                        OutgoingPixelValue = static_cast<float>(t);

                    //Strip 3: constant spatially, square-changing temporally.
                    }else if(isininc(20,row,29)){
                        OutgoingPixelValue = static_cast<float>(t*t / 250.0);

                    //Strip 4: constant spatially, Gaussian temporally.
                    }else if(isininc(30,row,39)){
                        const double sigma  = 20.0; //seconds.
                        const double centre = 50.0; //seconds.
                        const auto exparg = -1.0 * std::pow(t-centre, 2.0) / std::pow(sigma, 2.0);
                        OutgoingPixelValue = static_cast<float>( std::exp(exparg) );

                    //Strip 5: AIF and VIF; constant spatially, Gaussian temporally.
                    }else if(isininc(40,row,49)){
                        //The AIF.
                        if(col < 25){
                            const double sigma  = 15.0; //seconds.
                            const double centre = 30.0; //seconds.
                            const auto exparg = -1.0 * std::pow(t-centre, 2.0) / std::pow(sigma, 2.0);
                            OutgoingPixelValue = static_cast<float>( std::exp(exparg) );

                        //The VIF.
                        }else{
                            const double sigma  = 50.0; //seconds.
                            const double centre = 70.0; //seconds.
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
    // Also load dose data into the fray.
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
                shtl.points.push_back(animg.get().position(/*row=*/40, /*column=*/ 0 ));
                shtl.points.push_back(animg.get().position(/*row=*/49, /*column=*/ 0 ));
                shtl.points.push_back(animg.get().position(/*row=*/49, /*column=*/24 ));
                shtl.points.push_back(animg.get().position(/*row=*/40, /*column=*/24 ));
                shtl.Reorient_Counter_Clockwise();
                shtl.metadata = animgcoll.get().get_common_metadata({});
                shtl.metadata["ROINumber"] = std::to_string(ROINumber);
                shtl.metadata["ROIName"] = ROIName;
                shtl.metadata["MinimumSeparation"] = animg.get().metadata["ImageThickness"];
                cc.contours.push_back(std::move(shtl));
            }
            output->ccs.push_back( contours_with_meta() );
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
                shtl.points.push_back(animg.get().position(/*row=*/40, /*column=*/25 ));
                shtl.points.push_back(animg.get().position(/*row=*/49, /*column=*/25 ));
                shtl.points.push_back(animg.get().position(/*row=*/49, /*column=*/49 ));
                shtl.points.push_back(animg.get().position(/*row=*/40, /*column=*/49 ));
                shtl.Reorient_Counter_Clockwise();
                shtl.metadata = animgcoll.get().get_common_metadata({});
                shtl.metadata["ROINumber"] = std::to_string(ROINumber);
                shtl.metadata["ROIName"] = ROIName;
                shtl.metadata["MinimumSeparation"] = animg.get().metadata["ImageThickness"];
                cc.contours.push_back(std::move(shtl));
            }
            output->ccs.push_back( contours_with_meta() );
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
                shtl.points.push_back(animg.get().position(/*row=*/49, /*column=*/ 0 ));
                shtl.points.push_back(animg.get().position(/*row=*/49, /*column=*/49 ));
                shtl.points.push_back(animg.get().position(/*row=*/ 0, /*column=*/49 ));
                shtl.Reorient_Counter_Clockwise();
                shtl.metadata = animgcoll.get().get_common_metadata({});
                shtl.metadata["ROINumber"] = std::to_string(ROINumber);
                shtl.metadata["ROIName"] = ROIName;
                shtl.metadata["MinimumSeparation"] = animg.get().metadata["ImageThickness"];
                cc.contours.push_back(std::move(shtl));
            }
            output->ccs.push_back( contours_with_meta() );
            output->ccs.back() = cc;
            output->ccs.back().Raw_ROI_name = ROIName; 
            output->ccs.back().ROI_number = ROINumber;
        }


        DICOM_data.contour_data = std::move(output);
    }

    return std::move(DICOM_data);
}
