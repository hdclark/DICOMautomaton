//Common_Plotting.h.

#pragma once

#include <map>
#include <string>

#include "YgorMath.h"                //Needed for samples_1D class.
#include "YgorMathChebyshev.h"       //Needed for cheby_approx class.

template <class T> class cheby_approx;
template <class T> class samples_1D;


void 
PlotTimeCourses(std::string title,
                         const std::map<std::string, samples_1D<double>> &s1D_time_courses,
                         const std::map<std::string, cheby_approx<double>> &cheby_time_courses,
                         std::string xlabel = "Time (s)",
                         std::string ylabel = "Pixel Intensity",
                         long int cheby_samples = 250);

