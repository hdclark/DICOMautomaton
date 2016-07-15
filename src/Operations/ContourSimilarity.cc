//ContourSimilarity.cc - A part of DICOMautomaton 2015, 2016. Written by hal clark.

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
#include "../YgorImages_Functors/Processing/Liver_Pharmacokinetic_Model_5Param_Structs.h"
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

#include "ContourSimilarity.h"


std::list<OperationArgDoc> OpArgDocContourSimilarity(void){
    return std::list<OperationArgDoc>();
}

Drover ContourSimilarity(Drover DICOM_data, OperationArgPkg /*OptArgs*/, std::map<std::string,std::string> /*InvocationMetadata*/, std::string /*FilenameLex*/){

    //Get handles for each of the original image arrays so we can easily refer to them later.
    std::vector<std::shared_ptr<Image_Array>> orig_img_arrays;
    for(auto & img_arr : DICOM_data.image_data) orig_img_arrays.push_back(img_arr);

    //We require exactly one image volume. Use only the first image.
    if(orig_img_arrays.empty()) FUNCERR("This routine requires at least one imaging volume");
    orig_img_arrays.resize(1);

    //Package the ROIs of interest into two contour_collections to compare.
    contour_collection<double> cc_H; //Haley.
    contour_collection<double> cc_J; //Joel.
    contour_collection<double> cc_E; //Existing (i.e., therapist-contoured + oncologist inspected treatment contours).

    {
        for(auto & cc : DICOM_data.contour_data->ccs){
            for(auto & c : cc.contours){
                const auto name = c.metadata["ROIName"]; //c.metadata["NormalizedROIName"].
                const auto iccrH = GetFirstRegex(name, "(ICCR2016_Haley)");
                const auto iccrJ = GetFirstRegex(name, "(ICCR2016_Joel)");
                const auto eye  = GetFirstRegex(name, "([eE][yY][eE])");
                const auto orbit = GetFirstRegex(name, "([oO][rR][bB][iI][tT])");
                if(!iccrH.empty()){
                    cc_H.contours.push_back(c);
                }else if(!iccrJ.empty()){
                    cc_J.contours.push_back(c);
                }else if(!eye.empty() || !orbit.empty()){
                    FUNCWARN("Assuming contour '" << name << "' refers to eye(s)");
                    cc_E.contours.push_back(c);
                }
            }
        }

        if(cc_E.contours.empty()){
            FUNCWARN("Unable to find 'eyes' contour among:");
            for(auto & cc : DICOM_data.contour_data->ccs){
                std::cout << cc.contours.front().metadata["ROIName"] << std::endl;
            }
            throw std::domain_error("No 'eyes' contour available");
        }

    }

    //Compute similarity of the two contour_collections.
    ComputeContourSimilarityUserData ud;
    if(true){
        for(auto & img_arr : orig_img_arrays){
            ud.Clear();
            if(!img_arr->imagecoll.Compute_Images( ComputeContourSimilarity, { },
                                                   { std::ref(cc_H), std::ref(cc_E) }, &ud )){
                FUNCERR("Unable to compute Dice similarity");
            }
            std::cout << "Dice coefficient (H,E) = " << ud.Dice_Coefficient() << std::endl;
            std::cout << "Jaccard coefficient (H,E) = " << ud.Jaccard_Coefficient() << std::endl;

            ud.Clear();
            if(!img_arr->imagecoll.Compute_Images( ComputeContourSimilarity, { },
                                                   { std::ref(cc_J), std::ref(cc_E) }, &ud )){
                FUNCERR("Unable to compute Dice similarity");
            }
            std::cout << "Dice coefficient (J,E) = " << ud.Dice_Coefficient() << std::endl;
            std::cout << "Jaccard coefficient (J,E) = " << ud.Jaccard_Coefficient() << std::endl;

            ud.Clear();
            if(!img_arr->imagecoll.Compute_Images( ComputeContourSimilarity, { },
                                                   { std::ref(cc_H), std::ref(cc_J) }, &ud )){
                FUNCERR("Unable to compute Dice similarity");
            }
            std::cout << "Dice coefficient (H,J) = " << ud.Dice_Coefficient() << std::endl;
            std::cout << "Jaccard coefficient (H,J) = " << ud.Jaccard_Coefficient() << std::endl;
        }
    } 


    return std::move(DICOM_data);
}
