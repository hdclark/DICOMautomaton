//Common_Plotting.h.

#pragma once

#include <cstdint>
#include <map>
#include <string>


template <class T> class cheby_approx;
template <class T> class samples_1D;


void 
PlotTimeCourses(const std::string& title,
                         const std::map<std::string, samples_1D<double>> &s1D_time_courses,
                         const std::map<std::string, cheby_approx<double>> &cheby_time_courses,
                         const std::string& xlabel = "Time (s)",
                         const std::string& ylabel = "Pixel Intensity",
                         int64_t cheby_samples = 250);

