//UBC3TMRI_DCE_Differences.cc - A part of DICOMautomaton 2015, 2016. Written by hal clark.

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

#include "UBC3TMRI_DCE_Differences.h"


Drover UBC3TMRI_DCE_Differences(Drover DICOM_data, std::map<std::string,std::string> /*InvocationMetadata*/, std::string /*FilenameLex*/){

    //This routine generates difference maps using both long DCE scans. Thus it takes up a LOT of memory! Try avoid 
    // unnecessary copies of large (temporally long) arrays.

    //Stuff references to all contours into a list. Remember that you can still address specific contours through
    // the original holding containers (which are not modified here).
    std::list<std::reference_wrapper<contour_collection<double>>> cc_all;
    for(auto & cc : DICOM_data.contour_data->ccs){
        auto base_ptr = reinterpret_cast<contour_collection<double> *>(&cc);
        cc_all.push_back( std::ref(*base_ptr) );
    }


    if(DICOM_data.image_data.size() != 2) FUNCERR("Expected two image arrays in a specific order. Cannot continue");  
 
    //Get named handles for each image array so we can easily refer to them later.
    std::shared_ptr<Image_Array> orig_unstim_long  = *std::next(DICOM_data.image_data.begin(),0); //full (long) DCE 01 scan (no stim).
    std::shared_ptr<Image_Array> orig_stim_long    = *std::next(DICOM_data.image_data.begin(),1); //full (long) DCE 02 scan (stim).
    DICOM_data.image_data.clear(); //Trying to conserve space.


    //Deep-copy, trim the post-contrast injection signal, and temporally-average the image arrays.
    auto PurgeAbove35Seconds = std::bind(PurgeAboveTemporalThreshold, std::placeholders::_1, 35.0);

    std::shared_ptr<Image_Array> tavgd_unstim_long = std::make_shared<Image_Array>( *orig_unstim_long );
    tavgd_unstim_long->imagecoll.Prune_Images_Satisfying(PurgeAbove35Seconds);
    if(!tavgd_unstim_long->imagecoll.Condense_Average_Images(GroupSpatiallyOverlappingImages)){
        FUNCERR("Cannot temporally average data set. Is it able to be averaged?");
    }

    std::shared_ptr<Image_Array> tavgd_stim_long = std::make_shared<Image_Array>( *orig_stim_long );
    tavgd_stim_long->imagecoll.Prune_Images_Satisfying(PurgeAbove35Seconds);
    if(!tavgd_stim_long->imagecoll.Condense_Average_Images(GroupSpatiallyOverlappingImages)){
        FUNCERR("Cannot temporally average data set. Is it able to be averaged?");
    }       


    //Deep-copy the original long image array and use the temporally-averaged, pre-contrast map to work out the poor-man's Gad C in each voxel.
    std::shared_ptr<Image_Array> unstim_C = std::make_shared<Image_Array>( *orig_unstim_long );
    if(!unstim_C->imagecoll.Transform_Images( DCEMRISigDiffC, { tavgd_unstim_long->imagecoll }, {}) ){
        FUNCERR("Unable to transform image array to make poor-man's C map");
    }
    orig_unstim_long.reset();
    //DICOM_data.image_data.emplace_back(unstim_C);
    
    std::shared_ptr<Image_Array> stim_C = std::make_shared<Image_Array>( *orig_stim_long );
    if(!stim_C->imagecoll.Transform_Images( DCEMRISigDiffC, { tavgd_stim_long->imagecoll }, {}) ){
        FUNCERR("Unable to transform image array to make poor-man's C map");
    }
    orig_stim_long.reset();
    //DICOM_data.image_data.emplace_back(stim_C);


    //Generate maps of the slope for the various time segments.
    auto TimeCourseSlopeDifferenceOverStim = std::bind(TimeCourseSlopeDifference, 
                                             std::placeholders::_1, std::placeholders::_2, 
                                             std::placeholders::_3, std::placeholders::_4,
                                             135.0, 300.0,
                                             300.0, std::numeric_limits<double>::max(),
                                             std::experimental::any() );


    std::shared_ptr<Image_Array> nostim_case = std::make_shared<Image_Array>(*unstim_C);
    if(!nostim_case->imagecoll.Process_Images( GroupSpatiallyOverlappingImages,
                                               TimeCourseSlopeDifferenceOverStim,
                                               {}, cc_all )){
        FUNCERR("Unable to compute time course slope map");
    }
    unstim_C.reset(); 

    std::shared_ptr<Image_Array> stim_case = std::make_shared<Image_Array>(*stim_C);
    if(!stim_case->imagecoll.Process_Images( GroupSpatiallyOverlappingImages,
                                             TimeCourseSlopeDifferenceOverStim,
                                             {}, cc_all )){
        FUNCERR("Unable to compute time course slope map");
    }
    stim_C.reset();

    DICOM_data.image_data.emplace_back(nostim_case);
    DICOM_data.image_data.emplace_back(stim_case);

    //Compute the difference of the images.
    std::shared_ptr<Image_Array> difference = std::make_shared<Image_Array>(*stim_case);
    {
      std::list<std::reference_wrapper<planar_image_collection<float,double>>> external_imgs;
      external_imgs.push_back( std::ref(nostim_case->imagecoll) );

      if(!difference->imagecoll.Transform_Images( SubtractSpatiallyOverlappingImages, std::move(external_imgs), {} )){
          FUNCERR("Unable to subtract the pixel maps");
      }
    }

    DICOM_data.image_data.emplace_back(difference);
    
    return std::move(DICOM_data);
}
