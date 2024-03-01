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
#include "YgorLog.h"
#include "YgorString.h"       //Needed for GetFirstRegex(...)



OperationDoc OpArgDocPlotPerROITimeCourses(){
    OperationDoc out;
    out.name = "PlotPerROITimeCourses";
    out.desc = "Interactively plot time courses for the specified ROI(s).";


    out.args.emplace_back();
    out.args.back() = RCWhitelistOpArgDoc();
    out.args.back().name = "ROILabelRegex";
    out.args.back().default_val = ".*";

    out.args.emplace_back();
    out.args.back() = CCWhitelistOpArgDoc();
    out.args.back().name = "ROISelection";
    out.args.back().default_val = "all";

    out.args.emplace_back();
    out.args.back() = NCWhitelistOpArgDoc();
    out.args.back().name = "NormalizedROILabelRegex";
    out.args.back().default_val = ".*";

    return out;
}



bool PlotPerROITimeCourses(Drover &DICOM_data,
                             const OperationArgPkg& OptArgs,
                             std::map<std::string, std::string>& /*InvocationMetadata*/,
                             const std::string& /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ROILabelRegex = OptArgs.getValueStr("ROILabelRegex").value();
    const auto ROISelection = OptArgs.getValueStr("ROISelection").value();
    const auto NormalizedROILabelRegex = OptArgs.getValueStr("NormalizedROILabelRegex").value();
    //-----------------------------------------------------------------------------------------------------------------
    auto img_arr = DICOM_data.image_data.back();

    auto cc_all = All_CCs( DICOM_data );
    auto cc_ROIs = Whitelist( cc_all, ROILabelRegex, NormalizedROILabelRegex, ROISelection );


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
            YLOGWARN("Unable to plot time courses: " << e.what());
        }
    }

    return true;
}
