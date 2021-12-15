//PlotPerROITimeCourses.cc - A part of DICOMautomaton 2015, 2016. Written by hal clark.

#include <exception>
#include <any>
#include <optional>
#include <fstream>
#include <functional>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <regex>
#include <string>    
#include <utility>            //Needed for std::pair.
#include <vector>

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../YgorImages_Functors/Compute/Per_ROI_Time_Courses.h"
#include "PlotPerROITimeCourses.h"
#include "YgorFilesDirs.h"    //Needed for Does_File_Exist_And_Can_Be_Read(...), etc..
#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMathPlottingGnuplot.h" //Needed for YgorMathPlottingGnuplot::*.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorString.h"       //Needed for GetFirstRegex(...)



OperationDoc OpArgDocPlotPerROITimeCourses(){
    OperationDoc out;
    out.name = "PlotPerROITimeCourses";
    out.desc = "Interactively plot time courses for the specified ROI(s).";


    out.args.emplace_back();
    out.args.back() = RCWhitelistOpArgDoc();
    out.args.back().name = "ROILabelRegex";
    out.args.back().default_val = ".*";

    return out;
}



bool PlotPerROITimeCourses(Drover &DICOM_data,
                             const OperationArgPkg& OptArgs,
                             std::map<std::string, std::string>& /*InvocationMetadata*/,
                             const std::string& /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ROILabelRegex = OptArgs.getValueStr("ROILabelRegex").value();
    //-----------------------------------------------------------------------------------------------------------------
    const auto theregex = Compile_Regex(ROILabelRegex);

    auto img_arr = DICOM_data.image_data.back();


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
                   const auto& ROIName = ROINameOpt.value();
                   return !(std::regex_match(ROIName,theregex));
    });


    //Compute some aggregate C(t) curves from the available ROIs.
    ComputePerROITimeCoursesUserData ud; // User Data.
    if(!img_arr->imagecoll.Compute_Images( ComputePerROICourses,   //Non-modifying function, can use in-place.
                                           { },
                                           cc_ROIs,
                                           &ud )){
        throw std::runtime_error("Unable to compute per-ROI time courses");
    }
    //For perfusion purposes, Scale down the ROIs per-atomos (i.e., per-voxel).
    for(auto & tcs : ud.time_courses){
        const auto lROIname = tcs.first;
        const auto lVoxelCount = ud.voxel_count[lROIname];
        tcs.second = tcs.second.Multiply_With(1.0/static_cast<double>(lVoxelCount));
    }


    //Plot the ROIs we computed.
    if(true){
        //NOTE: This routine is spotty. It doesn't always work, and seems to have a hard time opening a 
        // display window when a large data set is loaded. Files therefore get written for backup access.
        std::cout << "Producing " << ud.time_courses.size() << " time courses:" << std::endl;
        std::vector<YgorMathPlottingGnuplot::Shuttle<samples_1D<double>>> shuttle;
        for(auto & tcs : ud.time_courses){
            const auto lROIname = tcs.first;
            const auto lTimeCourse = tcs.second;
            shuttle.emplace_back(lTimeCourse, lROIname + " - Voxel Averaged");
            const auto lFileName = Get_Unique_Sequential_Filename("/tmp/roi_time_course_",4,".txt");
            lTimeCourse.Write_To_File(lFileName); 
            AppendStringToFile("# Time course for ROI '"_s + lROIname + "'.\n", lFileName);
            std::cout << "\tTime course for ROI '" << lROIname << "' written to '" << lFileName << "'." << std::endl;
        }
        try{
            YgorMathPlottingGnuplot::Plot<double>(shuttle, "ROI Time Courses", "Time (s)", "Pixel Intensity");
        }catch(const std::exception &e){
            FUNCWARN("Unable to plot time courses: " << e.what());
        }
    }

    return true;
}
