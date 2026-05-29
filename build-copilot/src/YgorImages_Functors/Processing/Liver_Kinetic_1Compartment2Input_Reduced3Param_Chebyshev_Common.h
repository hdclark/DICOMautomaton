//Liver_Kinetic_1Compartment2Input_Reduced3Param_Chebyshev_Common.h.
#pragma once

#ifdef DCMA_USE_GNU_GSL

#include <list>
#include <functional>
#include <limits>
#include <map>
#include <cmath>
#include <tuple>
#include <regex>

#include "Liver_Kinetic_Common.h"

struct KineticModel_Liver_1C2I_Reduced3Param_Chebyshev_UserData {

    double ContrastInjectionLeadTime;
    std::map<std::string, cheby_approx<double>> time_courses;
    std::map<std::string, cheby_approx<double>> time_course_derivatives;

    std::list<KineticModel_PixelSelectionCriteria> pixels_to_plot;

    std::regex TargetROIs;

    size_t ExpApproxTrunc;
    double MultiplicationCoeffTrunc;
};


#endif // DCMA_USE_GNU_GSL

