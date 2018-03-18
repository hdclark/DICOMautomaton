//AccumulateRowsColumns.cc - A part of DICOMautomaton 2018. Written by hal clark.

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
#include "../YgorImages_Functors/ConvenienceRoutines.h"

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

#include "AccumulateRowsColumns.h"


std::list<OperationArgDoc> OpArgDocAccumulateRowsColumns(void){
    std::list<OperationArgDoc> out;

    // This operation generates row- and column-profiles of images in which the entire row or column has been summed
    // together. It is useful primarily for detection of axes-aligned edges or ridges.
    //
    // Note: it is often useful to pre-process inputs by computing an in-image-plane derivative, gradient magnitude, or
    //       similar (i.e., something to emphasize edges) before calling this routine. It is not necessary, however.
    //

    out.emplace_back();
    out.back().name = "ImageSelection";
    out.back().desc = "Images to operate on. Either 'none', 'last', 'first', or 'all'.";
    out.back().default_val = "last";
    out.back().expected = true;
    out.back().examples = { "none", "last", "first", "all" };
    
    return out;
}

Drover AccumulateRowsColumns(Drover DICOM_data, OperationArgPkg OptArgs, std::map<std::string,std::string> /*InvocationMetadata*/, std::string /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();

    //-----------------------------------------------------------------------------------------------------------------
    const auto regex_none  = std::regex("^no?n?e?$", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto regex_first = std::regex("^fi?r?s?t?$", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto regex_last  = std::regex("^la?s?t?$", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto regex_all   = std::regex("^al?l?$",   std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);

    if( !std::regex_match(ImageSelectionStr, regex_none)
    &&  !std::regex_match(ImageSelectionStr, regex_first)
    &&  !std::regex_match(ImageSelectionStr, regex_last)
    &&  !std::regex_match(ImageSelectionStr, regex_all) ){
        throw std::invalid_argument("Image selection is not valid. Cannot continue.");
    }


    auto iap_it = DICOM_data.image_data.begin();
    if(false){
    }else if(std::regex_match(ImageSelectionStr, regex_none)){ iap_it = DICOM_data.image_data.end();
    }else if(std::regex_match(ImageSelectionStr, regex_last)){
        if(!DICOM_data.image_data.empty()) iap_it = std::prev(DICOM_data.image_data.end());
    }
    while(iap_it != DICOM_data.image_data.end()){

        std::vector<YgorMathPlottingGnuplot::Shuttle<samples_1D<double>>> row_sums;
        std::vector<YgorMathPlottingGnuplot::Shuttle<samples_1D<double>>> col_sums;
        {
            decltype((*iap_it)->imagecoll.images) shtl;
            for(const auto & animg : (*iap_it)->imagecoll.images){
                std::vector<double> row_sum(animg.rows, 0.0);
                std::vector<double> col_sum(animg.columns, 0.0);

                //Sum pixel values row- and column-wise.
                for(auto row = 0; row < animg.rows; ++row){
                    for(auto col = 0; col < animg.columns; ++col){
                        for(auto chan = 0; chan < animg.channels; ++chan){
                            const auto val = animg.value(row, col, chan);
                            row_sum[row] += val;
                            col_sum[col] += val;
                        }//Loop over channels.
                    } //Loop over cols
                } //Loop over rows

                //Record the data in the form of comparative plots.
                {
                    samples_1D<double> row_profile;
                    samples_1D<double> col_profile;

                    for(auto row = 0; row < animg.rows; ++row){
                        const auto pos = animg.position(row,0).Dot(animg.row_unit); //Relative to DICOM origin.
                        row_profile.push_back({ pos, 0.0, row_sum[row], 0.0 });
                    }
                    for(auto col = 0; col < animg.columns; ++col){
                        const auto pos = animg.position(0,col).Dot(animg.col_unit); //Relative to DICOM origin.
                        col_profile.push_back({ pos, 0.0, col_sum[col], 0.0 });
                    }
                    const auto row_area = row_profile.Integrate_Over_Kernel_unit()[0];
                    const auto col_area = col_profile.Integrate_Over_Kernel_unit()[0];
                    row_profile = row_profile.Multiply_With(1.0/row_area);
                    col_profile = col_profile.Multiply_With(1.0/col_area);

                    row_sums.emplace_back(row_profile, "Row Profile");
                    col_sums.emplace_back(col_profile, "Column Profile");
                }

                // Produce some images for the user to inspect.
                planar_image<float,double> row_prof(animg);
                planar_image<float,double> col_prof(animg);
                Stats::Running_MinMax<float> minmax_row;
                Stats::Running_MinMax<float> minmax_col;

                for(auto row = 0; row < animg.rows; ++row){
                    for(auto col = 0; col < animg.columns; ++col){
                        for(auto chan = 0; chan < animg.channels; ++chan){
                            row_prof.reference(row, col, chan) = row_sum[row];
                            col_prof.reference(row, col, chan) = col_sum[col];

                            minmax_row.Digest( row_sum[row] );
                            minmax_col.Digest( col_sum[col] );
                        }//Loop over channels.
                    } //Loop over cols
                } //Loop over rows

                const std::string row_desc = "Row-wise pixel accumulation";
                const std::string col_desc = "Column-wise pixel accumulation";
                UpdateImageDescription( std::ref(row_prof), row_desc );
                UpdateImageDescription( std::ref(col_prof), col_desc );
                UpdateImageWindowCentreWidth( std::ref(row_prof), minmax_row );
                UpdateImageWindowCentreWidth( std::ref(col_prof), minmax_col );

                shtl.emplace_back( row_prof );
                shtl.emplace_back( col_prof );
            }

            (*iap_it)->imagecoll.images.splice( (*iap_it)->imagecoll.images.end(),
                                                std::move(shtl) );
        }

        // Display the row and column sum profiles for visual estimation of edge coincidence.
        {
            try{
                YgorMathPlottingGnuplot::Plot<double>(row_sums, "Row sums", "DICOM position", "Pixel intensity");
                YgorMathPlottingGnuplot::Plot<double>(col_sums, "Column sums", "DICOM position", "Pixel intensity");
            }catch(const std::exception &e){
                FUNCWARN("Failed to plot: " << e.what());
            }
        }

        // Loop control.
        ++iap_it;
        if(std::regex_match(ImageSelectionStr, regex_first)) break;
    }

    return DICOM_data;
}
