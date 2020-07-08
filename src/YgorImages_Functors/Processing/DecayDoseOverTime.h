//DecayDoseOverTime.h.
#pragma once


#include <cmath>
#include <any>
#include <functional>
#include <limits>
#include <list>
#include <map>

#include "YgorImages.h"

template <class T> class contour_collection;

typedef enum { // Controls how dose is decayed (i.e., selects the model).
    Halve,                 // Ad-hoc model applicable for all tissues (but defensible for none).
    Jones_and_Grant_2014   // Applicable for CNS tissues.

} DecayDoseOverTimeMethod;


struct DecayDoseOverTimeUserData {
    // Note: default parameters are given so some results can be pre-computed without risk of
    //       generating NaN/Inf signals even when a model is not in use. Provided values are
    //       "typical".


    //General parameters.
    long int channel = -1; // Which channel to consider. Use '-1' for all channels.

    // Model selection.
    DecayDoseOverTimeMethod model = DecayDoseOverTimeMethod::Halve;

    // 'Halve' model parameters.
    // ... no parameters ...


    // 'Jones_and_Grant_2014' model parameters.
    double Course1DosePerFraction = 2.0;     // Note `course 1' refers to a historical treatment
    double Course1NumberOfFractions = 35.0;  // that corresponds to the user-provided dose data.

    double ToleranceTotalDose = 50.0;         // Note this refers to a hypothetical lifetime dose
    double ToleranceNumberOfFractions = 35.0; // limit (used to generate a lifetime 'tolerance' 
                                              // BED). 

    double TemporalGapMonths = 12.0; // Note Jones and Grant recommend clamping to [0y:3y].
                                     // This should be enforced in the calling code.
                                     // Also note that 1y = 12mo exactly, so 1mo = 30.4375d. 

    double AlphaBetaRatio = 2.0; // Note Jones and Grant recommend 2 Gy rather than 3 Gy 
                                 // to be more conservative.

    bool UseMoreConservativeRecovery = true; // Note Jones and Grant provided two equations,
                                             // one is claimed to be more conservative. So
                                             // it should preferably be used.

};


bool DecayDoseOverTime(planar_image_collection<float,double>::images_list_it_t first_img_it,
                    std::list<planar_image_collection<float,double>::images_list_it_t> ,
                    std::list<std::reference_wrapper<planar_image_collection<float,double>>>,
                    std::list<std::reference_wrapper<contour_collection<double>>>, 
                    std::any );

