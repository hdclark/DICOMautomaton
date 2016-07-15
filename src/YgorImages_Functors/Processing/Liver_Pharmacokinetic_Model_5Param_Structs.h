//Liver_Pharmacokinetic_Model_5Param_Structs.h.
#pragma once

#include <list>
#include <functional>
#include <limits>
#include <map>
#include <cmath>
#include <tuple>
#include <regex>

#include <experimental/any>

#include "YgorMisc.h"
#include "YgorMath.h"
#include "YgorMathChebyshev.h"
#include "YgorImages.h"


// --------- Common Structs ----------
struct KineticModel_PixelSelectionCriteria {
    std::map<std::string,std::regex> metadata_criteria;
    long int row = -1;
    long int column = -1;

};


// ---------- One Compartment, Dual Input model; 5 model parameters; linear interpolation method ----------
struct KineticModel_Liver_1C2I_5Param_LinearUserData {

    double ContrastInjectionLeadTime;
    std::map<std::string, samples_1D<double>> time_courses;

    std::list<KineticModel_PixelSelectionCriteria> pixels_to_plot;

    std::regex TargetROIs;
};


// ---------- One Compartment, Dual Input model; 5 model parameters; Chebyshev polynomial method ----------
struct KineticModel_Liver_1C2I_5Param_ChebyUserData {

    double ContrastInjectionLeadTime;
    std::map<std::string, cheby_approx<double>> time_courses;
    std::map<std::string, cheby_approx<double>> time_course_derivatives;

    std::list<KineticModel_PixelSelectionCriteria> pixels_to_plot;

    std::regex TargetROIs;
};


