//DumpVoxelDoseInfo.cc - A part of DICOMautomaton 2016. Written by hal clark.

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
#include "../Dose_Meld.h"

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
#include "../YgorImages_Functors/Processing/ImagePartialDerivative.h"
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

#include "DumpVoxelDoseInfo.h"


std::list<OperationArgDoc> OpArgDocDumpVoxelDoseInfo(void){
    std::list<OperationArgDoc> out;

    return out;
}

Drover DumpVoxelDoseInfo(Drover DICOM_data, OperationArgPkg OptArgs, std::map<std::string,std::string> /*InvocationMetadata*/, std::string FilenameLex){

    //This operation locates the minimum and maximum dose voxel values. It is useful for estimating prescription doses.
    // 
    // Note: This implementation makes use of the 'old' way as implemented in the Drover class via melding dose. Please
    //       verify it works (or re-write using the new methods) before using for anything important.
    //

    double themin = std::numeric_limits<double>::max();
    double themax = std::numeric_limits<double>::min();

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

    return std::move(DICOM_data);
}
