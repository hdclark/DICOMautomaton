//Liver_Kinetic_1Compartment2Input_5Param_LinearInterp_Common.h.
#pragma once

#include <list>
#include <functional>
#include <limits>
#include <map>
#include <cmath>

#include "Liver_Kinetic_Common.h"

struct KineticModel_Liver_1C2I_5Param_LinearInterp_UserData {

    double ContrastInjectionLeadTime;
    std::map<std::string, samples_1D<double>> time_courses;

    std::list<KineticModel_PixelSelectionCriteria> pixels_to_plot;

    std::regex TargetROIs;
};

