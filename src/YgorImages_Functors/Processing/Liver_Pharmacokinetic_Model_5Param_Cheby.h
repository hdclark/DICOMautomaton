//Liver_Pharmacokinetic_Model_5Param_Cheby.h.
#pragma once

#include <list>
#include <functional>
#include <limits>
#include <map>
#include <set>
#include <cmath>
#include <tuple>
#include <regex>

#include <experimental/any>

#include "YgorMisc.h"
#include "YgorMath.h"
#include "YgorMathChebyshev.h"
#include "YgorImages.h"


struct LiverPharmacoModel5ParamChebyUserDataPixelSelectionCriteria {
    std::map<std::string,std::regex> metadata_criteria;
    long int row = -1;
    long int column = -1;

};

struct LiverPharmacoModel5ParamChebyUserData {

    double ContrastInjectionLeadTime;
    std::map<std::string, cheby_approx<double>> time_courses;
    std::map<std::string, cheby_approx<double>> time_course_derivatives;

    std::list<LiverPharmacoModel5ParamChebyUserDataPixelSelectionCriteria> pixels_to_plot;

    std::regex TargetROIs;
};

bool 
LiverPharmacoModel5ParamCheby(planar_image_collection<float,double>::images_list_it_t first_img_it,
                        std::list<planar_image_collection<float,double>::images_list_it_t> selected_img_its,
                        std::list<std::reference_wrapper<planar_image_collection<float,double>>> outgoing_maps,
                        std::list<std::reference_wrapper<contour_collection<double>>> ccsl, 
                        std::experimental::any ud);

