//Common_Plotting.cc - A part of DICOMautomaton 2016. Written by hal clark.

#include <unistd.h>           //fork().
#include <cstdlib>            //exit(), EXIT_SUCCESS.
#include <exception>
#include <fstream>
#include <map>
#include <string>    
#include <utility>            //Needed for std::pair.
#include <vector>

#include "Common_Plotting.h"
#include "YgorFilesDirs.h"    //Needed for Does_File_Exist_And_Can_Be_Read(...), etc..
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMathChebyshev.h" //Needed for cheby_approx class.
#include "YgorMathPlottingGnuplot.h" //Needed for YgorMathPlottingGnuplot::*.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorString.h"       //Needed for GetFirstRegex(...)


void 
PlotTimeCourses(const std::string& title,
                         const std::map<std::string, samples_1D<double>> &s1D_time_courses,
                         const std::map<std::string, cheby_approx<double>> &cheby_time_courses,
                         const std::string& xlabel,
                         const std::string& ylabel,
                         long int cheby_samples){
    // NOTE: This routine is spotty. It doesn't always work, and seems to have a hard time opening a display window when a
    //       large data set is loaded. Files therefore get written for backup access.
    //
    // NOTE: This routine does not persist after the parent terminates. It could be made to by dealing with signalling.
    //       A better approach would be sending data to a dedicated server over the net. Better for headless operations,
    //       better for managing the plots and data, better for archiving, etc..

#if !defined(_WIN32) && !defined(_WIN64)
    auto pid = fork();
    if(pid == 0){ //Child process.
#endif
        //Package the data into a shuttle and write the to file.
        std::vector<YgorMathPlottingGnuplot::Shuttle<samples_1D<double>>> shuttle;
        for(auto & tcs : s1D_time_courses){
            const auto lROIname = tcs.first;
            const auto lTimeCourse = tcs.second;
            shuttle.emplace_back(lTimeCourse, lROIname);
            const auto lFileName = Get_Unique_Sequential_Filename("/tmp/samples1D_time_course_",6,".txt");
            lTimeCourse.Write_To_File(lFileName);
            AppendStringToFile("# Time course for ROI '"_s + lROIname + "'.\n", lFileName);
            FUNCINFO("Time course for ROI '" << lROIname << "' written to '" << lFileName << "'");
        }
        for(auto & tcs : cheby_time_courses){
            const auto lROIname = tcs.first;
            const auto lTimeCourse = tcs.second;
            const auto domain = lTimeCourse.Get_Domain();
            const double dx = (domain.second - domain.first)/static_cast<double>(cheby_samples);

            samples_1D<double> lTimeCourseSamples1D;
            const bool inhibitsort = true;
            for(long int i = 0; i < cheby_samples; ++i){
                double t = domain.first + dx * static_cast<double>(i);
                lTimeCourseSamples1D.push_back(t, 0.0, lTimeCourse.Sample(t), 0.0, inhibitsort);
            }

            shuttle.emplace_back(lTimeCourseSamples1D, lROIname);
            const auto lFileName = Get_Unique_Sequential_Filename("/tmp/cheby_approx_time_course_",6,".txt");
            lTimeCourseSamples1D.Write_To_File(lFileName);
            AppendStringToFile("# Time course for ROI '"_s + lROIname + "'.\n", lFileName);
            FUNCINFO("Time course for ROI '" << lROIname << "' written to '" << lFileName << "'");
        }

        //Plot the data.
        long int max_attempts = 20;
        for(long int attempt = 1; attempt <= max_attempts; ++attempt){
            try{
                YgorMathPlottingGnuplot::Plot<double>(shuttle, title, xlabel, ylabel);
                break;
            }catch(const std::exception &e){
                FUNCWARN("Unable to plot time courses: '" << e.what() 
                         << "'. Attempt " << attempt << " of " << max_attempts << " ...");
            }
        }

#if !defined(_WIN32) && !defined(_WIN64)
        std::exit(EXIT_SUCCESS);
    }
#endif
    return;
}

