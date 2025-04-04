//DumpVoxelDoseInfo.cc - A part of DICOMautomaton 2016. Written by hal clark.

#include <iostream>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <string>    
#include <cstdint>

#include "YgorImages.h"
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorLog.h"

#include "../Dose_Meld.h"
#include "../Structs.h"
#include "DumpVoxelDoseInfo.h"


OperationDoc OpArgDocDumpVoxelDoseInfo(){
    OperationDoc out;
    out.name = "DumpVoxelDoseInfo";

    out.tags.emplace_back("category: image processing");
    out.tags.emplace_back("category: radiation dose");

    out.desc = 
        "This operation locates the minimum and maximum dose voxel values. It is useful for estimating prescription doses.";
        
    out.notes.emplace_back(
        "This implementation makes use of a primitive way of estimating dose. Please"
        " verify it works (or re-write using the new methods) before using for anything important."
    );

    return out;
}

bool DumpVoxelDoseInfo(Drover &DICOM_data,
                         const OperationArgPkg&,
                         std::map<std::string, std::string>& /*InvocationMetadata*/,
                         const std::string&){

    double themin = std::numeric_limits<double>::infinity();
    double themax = -(std::numeric_limits<double>::infinity());

    {
        auto d = Isolate_Dose_Data(DICOM_data);
        std::list<std::shared_ptr<Image_Array>> dose_data_to_use(d.image_data);
        if(d.image_data.size() > 1){
            dose_data_to_use = Meld_Image_Data(d.image_data);
            if(dose_data_to_use.size() != 1){
                throw std::runtime_error("This routine cannot handle multiple dose data which cannot be melded.");
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
                for(int64_t i=0; i<image.rows; ++i)  for(int64_t j=0; j<image.columns; ++j){
                    //Greyscale or R channel. We assume the channels satisfy: R = G = B.
                    const auto pointval = image.value(i,j,0); 
                    const auto pointdose = static_cast<double>(pointval); 

                    if(pointdose < themin) themin = pointdose;
                    if(pointdose > themax) themax = pointdose;
                }
            }

        }//Loop over the distinct dose file data.
    }

    {
        std::lock_guard<std::mutex> lock(ygor::g_term_sync);
        std::cout << "Min dose: " << themin << " Gy" << std::endl;
        std::cout << "Max dose: " << themax << " Gy" << std::endl;
    }

    return true;
}
