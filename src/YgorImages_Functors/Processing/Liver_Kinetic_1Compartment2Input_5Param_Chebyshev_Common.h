//Liver_Kinetic_1Compartment2Input_5Param_Chebyshev_Common.h.
#pragma once

#include <list>
#include <functional>
#include <limits>
#include <map>
#include <cmath>

#include "Liver_Kinetic_Common.h"

struct KineticModel_Liver_1C2I_5Param_Chebyshev_UserData {

    double ContrastInjectionLeadTime;
    std::map<std::string, cheby_approx<double>> time_courses;
    std::map<std::string, cheby_approx<double>> time_course_derivatives;

    std::list<KineticModel_PixelSelectionCriteria> pixels_to_plot;

    std::regex TargetROIs;

    size_t ExpApproxTrunc;
    double MultiplicationCoeffTrunc;
};

