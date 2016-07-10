//Common_Plotting.h.

#pragma once

#include <string>
#include <map>

#include "YgorMath.h"                //Needed for samples_1D class.
#include "YgorMathChebyshev.h"       //Needed for cheby_approx class.


void 
PlotTimeCourses(std::string title,
                         const std::map<std::string, samples_1D<double>> &s1D_time_courses,
                         const std::map<std::string, cheby_approx<double>> &cheby_time_courses,
                         std::string xlabel = "Time (s)",
                         std::string ylabel = "Pixel Intensity",
                         long int cheby_samples = 250);

