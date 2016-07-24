//DecimatePixels.cc - A part of DICOMautomaton 2015, 2016. Written by hal clark.

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

#include "DecimatePixels.h"


std::list<OperationArgDoc> OpArgDocDecimatePixels(void){
    std::list<OperationArgDoc> out;

    out.emplace_back();
    out.back().name = "OutSizeR";
    out.back().desc = "The number of pixels along the row unit vector to group into an outgoing pixel."
                      " Must be a multiplicative factor of the incoming image's row count."
                      " No decimation occurs if either this or 'OutSizeC' is zero or negative.";
    out.back().default_val = "8";
    out.back().expected = true;
    out.back().examples = { "0", "2", "4", "8", "16", "32", "64", "128", "256", "512" };

    out.emplace_back();
    out.back().name = "OutSizeC";
    out.back().desc = "The number of pixels along the column unit vector to group into an outgoing pixel."
                      " Must be a multiplicative factor of the incoming image's column count."
                      " No decimation occurs if either this or 'OutSizeR' is zero or negative.";
    out.back().default_val = "8";
    out.back().expected = true;
    out.back().examples = { "0", "2", "4", "8", "16", "32", "64", "128", "256", "512" };

    return out;
}

Drover DecimatePixels(Drover DICOM_data, OperationArgPkg OptArgs, std::map<std::string,std::string> /*InvocationMetadata*/, std::string /*FilenameLex*/){

    //This operation spatially aggregates blocks of pixels, thereby decimating them and making the images consume
    // far less memory. The precise size reduction and spatial aggregate can be set in the source. 

    //---------------------------------------------- User Parameters --------------------------------------------------
    const long int DecimateR = std::stol( OptArgs.getValueStr("OutSizeR").value() );
    const long int DecimateC = std::stol( OptArgs.getValueStr("OutSizeC").value() );
    //-----------------------------------------------------------------------------------------------------------------

    //Decimate the number of pixels for modeling purposes.
    if((DecimateR > 0) && (DecimateC > 0)){
        auto DecimateRC = std::bind(InImagePlanePixelDecimate, 
                                    std::placeholders::_1, std::placeholders::_2, 
                                    std::placeholders::_3, std::placeholders::_4,
                                    DecimateR, DecimateC,
                                    std::experimental::any());


        for(auto & img_arr : DICOM_data.image_data){
            if(!img_arr->imagecoll.Process_Images_Parallel( GroupIndividualImages,
                                                   DecimateRC,
                                                   {}, {} )){
                FUNCERR("Unable to decimate pixels");
            }
        }
    }

    return std::move(DICOM_data);
}
