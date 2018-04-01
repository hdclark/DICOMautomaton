//DumpVoxelDoseInfo.cc - A part of DICOMautomaton 2016. Written by hal clark.

#include <fstream>
#include <iostream>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <string>    

#include "../Dose_Meld.h"
#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "DumpVoxelDoseInfo.h"
#include "YgorImages.h"
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.


std::list<OperationArgDoc> OpArgDocDumpVoxelDoseInfo(void){
    std::list<OperationArgDoc> out;

    return out;
}

Drover DumpVoxelDoseInfo(Drover DICOM_data, OperationArgPkg , std::map<std::string,std::string>, std::string ){

    //This operation locates the minimum and maximum dose voxel values. It is useful for estimating prescription doses.
    // 
    // Note: This implementation makes use of the 'old' way as implemented in the Drover class via melding dose. Please
    //       verify it works (or re-write using the new methods) before using for anything important.
    //

    double themin = std::numeric_limits<double>::infinity();
    double themax = -(std::numeric_limits<double>::infinity());

    {
        std::list<std::shared_ptr<Dose_Array>> dose_data_to_use(DICOM_data.dose_data);
        if(DICOM_data.dose_data.size() > 1){
            dose_data_to_use = Meld_Dose_Data(DICOM_data.dose_data);
            if(dose_data_to_use.size() != 1){
                FUNCERR("This routine cannot handle multiple dose data which cannot be melded. This has " << dose_data_to_use.size());
            }
        }

        //Loop over the attached dose datasets (NOT the dose slices!). It is implied that we have to sum up doses 
        // from each attached data in order to find the total (actual) dose.
        //
        //NOTE: Should I get rid of the idea of cycling through multiple dose data? The only way we get here is if the data
        // cannot be melded... This is probably not a worthwhile feature to keep in this code.
        for(auto & dd_it : dose_data_to_use){

            //Loop through all dose frames (slices).
            for(auto i_it = dd_it->imagecoll.images.begin(); i_it != dd_it->imagecoll.images.end(); ++i_it){
                //Note: i_it is something like std::list<planar_image<T,R>>::iterator.
                //  
                //Now cycle through every pixel in the plane.
                for(long int i=0; i<i_it->rows; ++i)  for(long int j=0; j<i_it->columns; ++j){
                    //Greyscale or R channel. We assume the channels satisfy: R = G = B.
                    const auto pointval = i_it->value(i,j,0); 
                    const auto pointdose = static_cast<double>(dd_it->grid_scale * pointval); 

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
