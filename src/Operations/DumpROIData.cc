//DumpROIData.cc - A part of DICOMautomaton 2015, 2016. Written by hal clark.

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

#include "DumpROIData.h"


std::list<OperationArgDoc> OpArgDocDumpROIData(void){
    return std::list<OperationArgDoc>();
}

Drover DumpROIData(Drover DICOM_data, OperationArgPkg /*OptArgs*/, std::map<std::string,std::string> /*InvocationMetadata*/, std::string FilenameLex){

    //Simply dump ROI contour information to stdout.
    typedef std::tuple<std::string,std::string,std::string> key_t; //PatientID, ROIName, NormalizedROIName.

    //Individual contour information.
    std::map<key_t,int> ContourCounts;
    std::map<key_t,double> MinimumSeparation; // Almost always the 'thickness' of contours.
    std::map<key_t,double> SlabVolume;
    std::map<key_t,double> TotalPerimeter; // Total perimeter of all contours.

    std::map<key_t,double> RowLinearMin; // Extreme linear width ("caliper width").
    std::map<key_t,double> RowLinearMax;
    std::map<key_t,double> ColLinearMin;
    std::map<key_t,double> ColLinearMax;
    std::map<key_t,double> OrthoLinearMin; // To the center; does not include contour thickness. (Added later.)
    std::map<key_t,double> OrthoLinearMax;

    const vec3<double> row_unit(1.0,0.0,0.0); //Assumption!
    const vec3<double> col_unit(0.0,1.0,0.0); //Assumption!
    const auto ortho_unit = row_unit.Cross(col_unit).unit();

    const bool PlanarContourAssumption = true;
    if(DICOM_data.contour_data != nullptr){
        for(auto & cc : DICOM_data.contour_data->ccs){
            for(auto & c : cc.contours){
                key_t key = std::tie(c.metadata["PatientID"], c.metadata["ROIName"], c.metadata["NormalizedROIName"]);
                const auto min_sep = c.GetMetadataValueAs<double>("MinimumSeparation").value();
                ContourCounts[key] += 1;
                MinimumSeparation[key] = min_sep;
                SlabVolume[key] += std::abs( c.Get_Signed_Area(PlanarContourAssumption) * min_sep );
                TotalPerimeter[key] += std::abs( c.Perimeter() );

                //Find the axes-aligned extrema. 
                {

                    //Verify the row and unit direction assumptions are at least reasonable.
                    const auto est_normal = c.Estimate_Planar_Normal();
                    if( std::abs(est_normal.Dot(ortho_unit)) < 0.95 ){
                        RowLinearMin[key] = std::numeric_limits<double>::quiet_NaN();
                        RowLinearMax[key] = std::numeric_limits<double>::quiet_NaN();
                        ColLinearMin[key] = std::numeric_limits<double>::quiet_NaN();
                        ColLinearMax[key] = std::numeric_limits<double>::quiet_NaN();
                        OrthoLinearMin[key] = std::numeric_limits<double>::quiet_NaN();
                        OrthoLinearMax[key] = std::numeric_limits<double>::quiet_NaN();

                    }else{
                        //Find the min and max projection along each unit.
                        std::vector<double> row_proj;
                        std::vector<double> col_proj;
                        std::vector<double> ortho_proj;
                        for(const auto &p : c.points){
                            row_proj.push_back( row_unit.Dot(p) );
                            col_proj.push_back( col_unit.Dot(p) );
                            ortho_proj.push_back( ortho_unit.Dot(p) );
                        }
                        decltype(row_proj)::iterator row_min_it;
                        decltype(row_proj)::iterator row_max_it;
                        decltype(col_proj)::iterator col_min_it;
                        decltype(col_proj)::iterator col_max_it;
                        decltype(col_proj)::iterator ortho_min_it;
                        decltype(col_proj)::iterator ortho_max_it;
                        std::tie(row_min_it, row_max_it) = std::minmax_element(std::begin(row_proj), std::end(row_proj));
                        std::tie(col_min_it, col_max_it) = std::minmax_element(std::begin(col_proj), std::end(col_proj));
                        std::tie(ortho_min_it, ortho_max_it) = std::minmax_element(std::begin(ortho_proj), std::end(ortho_proj));

                        //Only update if the caliper width is larger. (The default will be zero.)
                        if(RowLinearMin.count(key) == 0){  
                            RowLinearMin[key] = *row_min_it;
                        }else{
                            if(*row_min_it < RowLinearMin[key]) RowLinearMin[key] = *row_min_it;
                        }
                        if(RowLinearMax.count(key) == 0){  
                            RowLinearMax[key] = *row_max_it;
                        }else{
                            if(*row_max_it > RowLinearMax[key]) RowLinearMax[key] = *row_max_it;
                        }

                        if(ColLinearMin.count(key) == 0){  
                            ColLinearMin[key] = *col_min_it;
                        }else{
                            if(*col_min_it < ColLinearMin[key]) ColLinearMin[key] = *col_min_it;
                        }
                        if(ColLinearMax.count(key) == 0){  
                            ColLinearMax[key] = *col_max_it;
                        }else{
                            if(*col_max_it > ColLinearMax[key]) ColLinearMax[key] = *col_max_it;
                        }

                        if(OrthoLinearMin.count(key) == 0){  
                            OrthoLinearMin[key] = *ortho_min_it;
                        }else{
                            if(*ortho_min_it < OrthoLinearMin[key]) OrthoLinearMin[key] = *ortho_min_it;
                        }
                        if(OrthoLinearMax.count(key) == 0){  
                            OrthoLinearMax[key] = *ortho_max_it;
                        }else{
                            if(*ortho_max_it > OrthoLinearMax[key]) OrthoLinearMax[key] = *ortho_max_it;
                        }

                    }
                }

            }
        }
    }

    std::cout << "==== Raw labels, normalized ROIName, contour counts, and slab volume ====" << std::endl;
    for(auto & ContourCount : ContourCounts){
        const auto thekey = ContourCount.first;
        std::cout << "DumpROIData:\t"
                  << "PatientID='" << std::get<0>(thekey) << "'\t"
                  << "ROIName='" << std::get<1>(thekey) << "'\t"
                  << "NormalizedROIName='" << std::get<2>(thekey) << "'\t"
                  << "ContourCount=" << ContourCount.second << "\t"
                  << "MinimumSeparation=" << MinimumSeparation[thekey] << "\t"
                  << "SlabVolume=" << SlabVolume[thekey] << "\t"
                  << "TotalPerimeter=" << TotalPerimeter[thekey] << "\t"
                  << "RowLinearDimension=" << (RowLinearMax[thekey] - RowLinearMin[thekey]) << "\t"
                  << "ColLinearDimension=" << (ColLinearMax[thekey] - ColLinearMin[thekey]) << "\t"
                  << "OrthoLinearDimension=" << (OrthoLinearMax[thekey] - OrthoLinearMin[thekey] + MinimumSeparation[thekey]) << "\t"
                  << std::endl;
    }
    std::cout << std::endl;



    std::cout << "==== Explictor best-guesses ====" << std::endl;
    Explicator X(FilenameLex);
    for(auto & ContourCount : ContourCounts){
        //Simply dump the suspected mapping.
        //std::cout << "PatientID='" << std::get<0>(ContourCount.first) << "'\t"
        //          << "ROIName='" << std::get<1>(ContourCount.first) << "'\t"
        //          << "NormalizedROIName='" << std::get<2>(ContourCount.first) << "'\t"
        //          << "Contours='" << ContourCount.second << "'"
        //          << std::endl;

        //Print out the best few guesses for each raw contour name.
        const auto ROIName = std::get<1>(ContourCount.first);
        X(ROIName);
        std::unique_ptr<std::map<std::string,float>> res(std::move(X.Get_Last_Results()));
        std::vector<std::pair<std::string,float>> ordered_res(res->begin(), res->end());
        std::sort(ordered_res.begin(), ordered_res.end(), 
                  [](const std::pair<std::string,float> &L, const std::pair<std::string,float> &R) -> bool {
                      return L.second > R.second;
                  });
        if(ordered_res.size() != 1) for(auto & ordered_re : ordered_res){
            std::cout << ordered_re.first << " : " << ROIName; // << std::endl;
            //std::cout << " QQQ " << ordered_res[i].second << std::endl;
            std::cout << std::endl;
            //if(i > 1) break;
        }
    }
    std::cout << std::endl;
    
    return std::move(DICOM_data);
}
