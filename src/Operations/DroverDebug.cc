//DroverDebug.cc - A part of DICOMautomaton 2017. Written by hal clark.

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
#include <regex>

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
#include "../YgorImages_Functors/Compute/AccumulatePixelDistributions.h"
#include "../YgorImages_Functors/Compute/GenerateSurfaceMask.h"

#include "DroverDebug.h"



std::list<OperationArgDoc> OpArgDocDroverDebug(void){
    std::list<OperationArgDoc> out;

    // This operation reports basic information on the state of the main Drover class.
    // It can be used to report on the state of the data, which can be useful for debugging.

    return out;
}



Drover DroverDebug(Drover DICOM_data, 
                   OperationArgPkg /*OptArgs*/, 
                   std::map<std::string,std::string> /*InvocationMetadata*/, 
                   std::string /*FilenameLex*/ ){

    //Dose data.
    {
        FUNCINFO("There are " <<
                 DICOM_data.dose_data.size() <<
                 " Dose_Arrays loaded");

        size_t d_arr = 0;
        for(auto &dap : DICOM_data.dose_data){
            FUNCINFO("  Dose_Array " <<
                     d_arr++ <<
                     " has " <<
                     dap->imagecoll.images.size() <<
                     " image slices");
        }
    }

    //Image data.
    {
        FUNCINFO("There are " <<
                 DICOM_data.image_data.size() <<
                 " Image_Arrays loaded");

        size_t i_arr = 0;
        for(auto &iap : DICOM_data.image_data){
            FUNCINFO("  Image_Array " <<
                     i_arr++ <<
                     " has " <<
                     iap->imagecoll.images.size() <<
                     " image slices");
            size_t i_num = 0;
            for(auto &img : iap->imagecoll.images){
                FUNCINFO("    Image " <<
                         i_num++ <<
                         " has pxl_dx, pxl_dy, pxl_dz = " <<
                         img.pxl_dx << ", " <<
                         img.pxl_dy << ", " <<
                         img.pxl_dz);
                FUNCINFO("    Image " <<
                         (i_num-1) <<
                         " has anchor, offset = " <<
                         img.anchor << ", " <<
                         img.offset);
                FUNCINFO("    Image " <<
                         (i_num-1) <<
                         " has row_unit, col_unit = " <<
                         img.row_unit << ", " <<
                         img.col_unit);
            }
        }
    }

    //Contour data.
    do{
        if(DICOM_data.contour_data == nullptr){
            FUNCINFO("There are 0 Contour_Data loaded");
            break;
        }

        FUNCINFO("There are " <<
                 DICOM_data.contour_data->ccs.size() <<
                 " Contour_Data loaded");

        size_t c_dat = 0;
        for(auto & cc : DICOM_data.contour_data->ccs){
            FUNCINFO("  Contour_Data " <<
                     c_dat++ <<
                     " has " <<
                     cc.contours.size() <<
                     " contours");
            size_t c_num = 0;
            for(auto & c : cc.contours){
                FUNCINFO("    contour " <<
                         c_num++ <<
                         " has " <<
                         c.points.size() <<
                         " vertices");
                if(!c.points.empty()){
                    FUNCINFO("      contour " <<
                         (c_num-1) <<
                         " has average point " <<
                         c.Average_Point());
                }
            }
        }
    }while(false);

    return std::move(DICOM_data);
}
