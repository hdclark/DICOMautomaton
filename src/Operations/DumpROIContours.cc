//DumpROIContours.cc - A part of DICOMautomaton 2016. Written by hal clark.

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

#include <boost/interprocess/sync/named_mutex.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>

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
#include "../YgorImages_Functors/Compute/AccumulatePixelDistributions.h"

#include "DumpROIContours.h"



std::list<OperationArgDoc> OpArgDocDumpROIContours(void){
    std::list<OperationArgDoc> out;

    // This operation exports contours in 'tabular' format.

    out.emplace_back();
    out.back().name = "DumpFileName";
    out.back().desc = "A filename (or full path) in which to (over)write with contour data."
                      " File format is Wavefront obj."
                      " Leave empty to dump to generate a unique temporary file.";
    out.back().default_val = "";
    out.back().expected = true;
    out.back().examples = { "", "/tmp/somefile.obj", "localfile.obj", "derivative_data.obj" };

    out.emplace_back();
    out.back().name = "MTLFileName";
    out.back().desc = "A filename (or full path) in which to (over)write a Wavefront material library file."
                      " This file is used to colour the contours to help differentiate them."
                      " Leave empty to dump to generate a unique temporary file.";
    out.back().default_val = "";
    out.back().expected = true;
    out.back().examples = { "", "/tmp/materials.mtl", "localfile.mtl", "somefile.mtl" };

    out.emplace_back();
    out.back().name = "NormalizedROILabelRegex";
    out.back().desc = "A regex matching ROI labels/names to consider. The default will match"
                      " all available ROIs. Be aware that input spaces are trimmed to a single space."
                      " If your ROI name has more than two sequential spaces, use regex to avoid them."
                      " All ROIs have to match the single regex, so use the 'or' token if needed."
                      " Regex is case insensitive and uses extended POSIX syntax.";
    out.back().default_val = ".*";
    out.back().expected = true;
    out.back().examples = { ".*", ".*Body.*", "Body", "Gross_Liver",
                            R"***(.*Left.*Parotid.*|.*Right.*Parotid.*|.*Eye.*)***",
                            R"***(Left Parotid|Right Parotid)***" };

    out.emplace_back();
    out.back().name = "ROILabelRegex";
    out.back().desc = "A regex matching ROI labels/names to consider. The default will match"
                      " all available ROIs. Be aware that input spaces are trimmed to a single space."
                      " If your ROI name has more than two sequential spaces, use regex to avoid them."
                      " All ROIs have to match the single regex, so use the 'or' token if needed."
                      " Regex is case insensitive and uses extended POSIX syntax.";
    out.back().default_val = ".*";
    out.back().expected = true;
    out.back().examples = { ".*", ".*body.*", "body", "Gross_Liver",
                            R"***(.*left.*parotid.*|.*right.*parotid.*|.*eyes.*)***",
                            R"***(left_parotid|right_parotid)***" };

    return out;
}



Drover DumpROIContours(Drover DICOM_data, OperationArgPkg OptArgs, std::map<std::string,std::string> /*InvocationMetadata*/, std::string FilenameLex){

    //---------------------------------------------- User Parameters --------------------------------------------------
    auto DumpFileName = OptArgs.getValueStr("DumpFileName").value();
    auto MTLFileName = OptArgs.getValueStr("MTLFileName").value();
    const auto ROILabelRegex = OptArgs.getValueStr("ROILabelRegex").value();
    const auto NormalizedROILabelRegex = OptArgs.getValueStr("NormalizedROILabelRegex").value();

    //-----------------------------------------------------------------------------------------------------------------
    const auto theregex = std::regex(ROILabelRegex, std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto thenormalizedregex = std::regex(NormalizedROILabelRegex, std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);

    if(DumpFileName.empty()){
        DumpFileName = Get_Unique_Sequential_Filename("/tmp/dicomautomaton_dumproicontours_", 6, ".obj");
    }
    if(MTLFileName.empty()){
        MTLFileName = Get_Unique_Sequential_Filename("/tmp/dicomautomaton_dumproicontours_", 6, ".mtl");
    }

    //Stuff references to all contours into a list. Remember that you can still address specific contours through
    // the original holding containers (which are not modified here).
    std::list<std::reference_wrapper<contour_collection<double>>> cc_all;
    for(auto & cc : DICOM_data.contour_data->ccs){
        auto base_ptr = reinterpret_cast<contour_collection<double> *>(&cc);
        cc_all.push_back( std::ref(*base_ptr) );
    }

    //Whitelist contours using the provided regex.
    auto cc_ROIs = cc_all;
    cc_ROIs.remove_if([=](std::reference_wrapper<contour_collection<double>> cc) -> bool {
                   const auto ROINameOpt = cc.get().contours.front().GetMetadataValueAs<std::string>("ROIName");
                   const auto ROIName = ROINameOpt.value();
                   return !(std::regex_match(ROIName,theregex));
    });
    cc_ROIs.remove_if([=](std::reference_wrapper<contour_collection<double>> cc) -> bool {
                   const auto ROINameOpt = cc.get().contours.front().GetMetadataValueAs<std::string>("NormalizedROIName");
                   const auto ROIName = ROINameOpt.value();
                   return !(std::regex_match(ROIName,thenormalizedregex));
    });

    if(cc_ROIs.empty()){
        throw std::invalid_argument("No contours selected. Cannot continue.");
    }


    //Generate a Wavefront materials file to colour the contours differently.
    std::vector<std::string> mats; // List of materials defined.
    {
        std::ofstream FO(MTLFileName, std::fstream::out);


        const auto write_mat = [&](const std::string &name,
                                   const vec3<double> &c_amb,
                                   const vec3<double> &c_dif,
                                   const vec3<double> &c_spc,
                                   const double s_exp,
                                   const double trans) -> void {

                mats.push_back(name);
                FO << "newmtl " << name << std::endl;
                FO << "Ka " << c_amb.x << " " << c_amb.y << " " << c_amb.z << std::endl; //Ambient colour.
                FO << "Kd " << c_dif.x << " " << c_dif.y << " " << c_dif.z << std::endl; //Diffuse colour.
                FO << "Ks " << c_spc.x << " " << c_spc.y << " " << c_spc.z << std::endl; //Specular colour.
                FO << "Ns " << s_exp << std::endl; //Specular exponent.
                FO << "d " << trans << std::endl; //Transparency ("dissolved"); d=1 is fully opaque.
                FO << "illum 2" << std::endl; //Illumination model (2 = "Colour on, Ambient on").
                FO << std::endl;
        };

        //write_mat("Red", vec3<double>(R, G, B), vec3<double>(R, G, B), vec3<double>(R, G, B), 10.0, 0.9);
        
        //Basic, core colours.
        //write_mat("Red",     vec3<double>(1.0, 0.0, 0.0), vec3<double>(1.0, 0.0, 0.0), vec3<double>(1.0, 0.0, 0.0), 10.0, 0.9);
        //write_mat("Green",   vec3<double>(0.0, 1.0, 0.0), vec3<double>(0.0, 1.0, 0.0), vec3<double>(0.0, 1.0, 0.0), 10.0, 0.9);
        //write_mat("Blue",    vec3<double>(0.0, 0.0, 1.0), vec3<double>(0.0, 0.0, 1.0), vec3<double>(0.0, 0.0, 1.0), 10.0, 0.9);


        // From 'http://stackoverflow.com/questions/470690/how-to-automatically-generate-n-distinct-colors' on 20160919
        // which originate from: Kelly, Kenneth L. "Twenty-two colors of maximum contrast." Color Engineering 3.26 (1965): 26-27.

        write_mat("vivid_yellow", vec3<double>(1.000,0.702,0.000), vec3<double>(1.000,0.702,0.000), vec3<double>(1.000,0.702,0.000), 10.0, 0.9);
        write_mat("strong_purple", vec3<double>(0.502,0.243,0.459), vec3<double>(0.502,0.243,0.459), vec3<double>(0.502,0.243,0.459), 10.0, 0.9);
        write_mat("vivid_orange", vec3<double>(1.000,0.408,0.000), vec3<double>(1.000,0.408,0.000), vec3<double>(1.000,0.408,0.000), 10.0, 0.9);
        write_mat("very_light_blue", vec3<double>(0.651,0.741,0.843), vec3<double>(0.651,0.741,0.843), vec3<double>(0.651,0.741,0.843), 10.0, 0.9);
        write_mat("vivid_red", vec3<double>(0.757,0.000,0.125), vec3<double>(0.757,0.000,0.125), vec3<double>(0.757,0.000,0.125), 10.0, 0.9);
        write_mat("grayish_yellow", vec3<double>(0.808,0.635,0.384), vec3<double>(0.808,0.635,0.384), vec3<double>(0.808,0.635,0.384), 10.0, 0.9);
        write_mat("medium_gray", vec3<double>(0.506,0.439,0.400), vec3<double>(0.506,0.439,0.400), vec3<double>(0.506,0.439,0.400), 10.0, 0.9);
        write_mat("vivid_green", vec3<double>(0.000,0.490,0.204), vec3<double>(0.000,0.490,0.204), vec3<double>(0.000,0.490,0.204), 10.0, 0.9);
        write_mat("strong_purplish_pink", vec3<double>(0.965,0.463,0.557), vec3<double>(0.965,0.463,0.557), vec3<double>(0.965,0.463,0.557), 10.0, 0.9);
        write_mat("strong_blue", vec3<double>(0.000,0.325,0.541), vec3<double>(0.000,0.325,0.541), vec3<double>(0.000,0.325,0.541), 10.0, 0.9);
        write_mat("strong_yellowish_pink", vec3<double>(1.000,0.478,0.361), vec3<double>(1.000,0.478,0.361), vec3<double>(1.000,0.478,0.361), 10.0, 0.9);
        write_mat("strong_violet", vec3<double>(0.325,0.216,0.478), vec3<double>(0.325,0.216,0.478), vec3<double>(0.325,0.216,0.478), 10.0, 0.9);
        write_mat("vivid_orange_yellow", vec3<double>(1.000,0.557,0.000), vec3<double>(1.000,0.557,0.000), vec3<double>(1.000,0.557,0.000), 10.0, 0.9);
        write_mat("strong_purplish_red", vec3<double>(0.702,0.157,0.318), vec3<double>(0.702,0.157,0.318), vec3<double>(0.702,0.157,0.318), 10.0, 0.9);
        write_mat("vivid_greenish_yellow", vec3<double>(0.957,0.784,0.000), vec3<double>(0.957,0.784,0.000), vec3<double>(0.957,0.784,0.000), 10.0, 0.9);
        write_mat("strong_reddish_brown", vec3<double>(0.498,0.094,0.051), vec3<double>(0.498,0.094,0.051), vec3<double>(0.498,0.094,0.051), 10.0, 0.9);
        write_mat("vivid_yellowish_green", vec3<double>(0.576,0.667,0.000), vec3<double>(0.576,0.667,0.000), vec3<double>(0.576,0.667,0.000), 10.0, 0.9);
        write_mat("deep_yellowish_brown", vec3<double>(0.349,0.200,0.082), vec3<double>(0.349,0.200,0.082), vec3<double>(0.349,0.200,0.082), 10.0, 0.9);
        write_mat("vivid_reddish_orange", vec3<double>(0.945,0.227,0.075), vec3<double>(0.945,0.227,0.075), vec3<double>(0.945,0.227,0.075), 10.0, 0.9);
        write_mat("dark_olive_green", vec3<double>(0.137,0.173,0.086), vec3<double>(0.137,0.173,0.086), vec3<double>(0.137,0.173,0.086), 10.0, 0.9);

        FO.flush();
    }

    //Dump the data in a structured ASCII Wavefront OBJ format using native polygons.
    //
    // NOTE: This routine creates a single polygon for each contour. Some programs might not be able to handle this,
    //       and may require triangles or quads at most.
    {
        std::ofstream FO(DumpFileName, std::fstream::out);

        //Reference the MTL file, but use relative paths to make moving files around easier without having to modify them.
        //FO << "mtllib " << MTLFileName << std::endl;
        FO << "mtllib " << SplitStringToVector(MTLFileName, '/', 'd').back() << std::endl;
        FO << std::endl;
 
        long int gvc = 0; // Global vertex count. Used to track vert number because they have whole-file scope.
        long int family = 0;
        for(auto &cc_ref : cc_ROIs){
            long int contour = 0;
            for(auto &c : cc_ref.get().contours){
                const auto N = c.points.size();
                if(N < 3) continue;

                //Generate an object name.
                FO << "o Contour_" << family << "_" << contour++ << std::endl;
                FO << std::endl;

                //Add useful comments, such as ROIName.
                FO << "# Metadata: ROIName = " << c.metadata["ROIName"] << std::endl;
                FO << "# Metadata: NormalizedROIName = " << c.metadata["NormalizedROIName"] << std::endl;

                //Choose a face colour.
                //
                // Note: Simply add more colours above if you need more colours here.
                // Note: The obj format does not support per-vertex colours.
                // Note: The usmtl statement should be before the vertices because some loaders (e.g., Meshlab)
                //       apply the material to vertices instead of faces.
                //
                FO << "usemtl " << mats[ family % mats.size() ] << std::endl;

                //Print the vertices.
                const auto defaultprecision = FO.precision();
                FO << std::setprecision(std::numeric_limits<long double>::digits10 + 1); 
                for(const auto &p : c.points){
                    FO << "v " << p.x << " " << p.y << " " << p.z << std::endl;
                }
                FO << std::endl;
                FO << std::setprecision(defaultprecision);

                //Print the face linkages. 
                //
                // Note: The obj format starts at 1, not 0. Polygons are implicitly closed and do not need to include a
                //       duplicate vertex.
                // 
                FO << "f";
                for(long int i = 1; i <= N; ++i) FO << " " << (gvc+i);
                //if(c.closed) FO << " " << (gvc+1);
                FO << std::endl; 
                FO << std::endl; 
                gvc += N;
            }
            ++family;
        }

        FO.flush();
    }

    return std::move(DICOM_data);
}
