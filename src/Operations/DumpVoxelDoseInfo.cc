//DumpVoxelDoseInfo.cc - A part of DICOMautomaton 2016. Written by hal clark.

#include <iostream>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <string>    

#include "../Dose_Meld.h"
#include "../Structs.h"
#include "DumpVoxelDoseInfo.h"
#include "YgorImages.h"
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.


OperationDoc OpArgDocDumpVoxelDoseInfo(){
    OperationDoc out;
    out.name = "DumpVoxelDoseInfo";

    out.desc = 
        "This operation locates the minimum and maximum dose voxel values. It is useful for estimating prescription doses.";
        
    out.notes.emplace_back(
        "This implementation makes use of a primitive way of estimating dose. Please"
        " verify it works (or re-write using the new methods) before using for anything important."
    );

    return out;
}

Drover DumpVoxelDoseInfo(Drover DICOM_data, OperationArgPkg , std::map<std::string,std::string>, std::string ){

    double themin = std::numeric_limits<double>::infinity();
    double themax = -(std::numeric_limits<double>::infinity());

    {
        auto d = Isolate_Dose_Data(DICOM_data);
        std::list<std::shared_ptr<Image_Array>> dose_data_to_use(d.image_data);
        if(d.image_data.size() > 1){
            dose_data_to_use = Meld_Image_Data(d.image_data);
            if(dose_data_to_use.size() != 1){
                FUNCERR("This routine cannot handle multiple dose data which cannot be melded. This has " << dose_data_to_use.size());
            }
        }

        //Loop over the attached dose datasets (NOT the dose slices!). It is implied that we have to sum up doses 
        // from each attached data in order to find the total (actual) dose.
        //
        //NOTE: Should I get rid of the idea of cycling through multiple dose data? The only way we get here is if the data
        // cannot be melded... This is probably not a worthwhile feature to keep in this code.
        for(auto & dd : dose_data_to_use){

            //Loop through all dose frames (slices).
            for(auto & image : dd->imagecoll.images){
                //Now cycle through every pixel in the plane.
                for(long int i=0; i<image.rows; ++i)  for(long int j=0; j<image.columns; ++j){
                    //Greyscale or R channel. We assume the channels satisfy: R = G = B.
                    const auto pointval = image.value(i,j,0); 
                    const auto pointdose = static_cast<double>(pointval); 

                    if(pointdose < themin) themin = pointdose;
                    if(pointdose > themax) themax = pointdose;
                }
            }

        }//Loop over the distinct dose file data.
    }

    std::cout << "Min dose: " << themin << " Gy" << std::endl;
    std::cout << "Max dose: " << themax << " Gy" << std::endl;

    return DICOM_data;
}
