//EQDConversion.h.
#pragma once


#include <cmath>
#include <any>
#include <functional>
#include <limits>
#include <list>
#include <map>

#include "YgorImages.h"
#include "YgorMath.h"
#include "YgorMisc.h"

template <class T> class contour_collection;


struct EQDConversionUserData {

    // -----------------------------
    // The type of model to use.
    enum class
    Model {
        SimpleLinearQuadratic,  // EQD based on a simplistic linear-quadratic model. No repopulation effects accounted for.

        PinnedLinearQuadratic,  // EQD based on a simplistic linear-quadratic model. No repopulation effects accounted for.
                                // The prescription dose is scaled to give 2 Gy/f; the effective number of fractions
                                // is extracted from the prescription dose EQD and used for all voxels. This eliminates
                                // the need to specify that every voxel will get 2 Gy/f.
    } model = Model::SimpleLinearQuadratic;

    // -----------------------------
    // SimpleLinearQuadratic parameters.
    double NumberOfFractions = -1.0;

    double AlphaBetaRatioNormal = 3.0; // alpha/beta for non-tumourous tissues.
    double AlphaBetaRatioTumour = 10.0; // alpha/beta for tumourous tissues.

    double TargetDosePerFraction = 2.0; // Should be 2.0 for EQD2.

    // -----------------------------
    // PinnedLinearQuadratic parameters.
    double PrescriptionDose = -1.0;

};


bool EQDConversion(planar_image_collection<float,double>::images_list_it_t first_img_it,
                    std::list<planar_image_collection<float,double>::images_list_it_t> ,
                    std::list<std::reference_wrapper<planar_image_collection<float,double>>>,
                    std::list<std::reference_wrapper<contour_collection<double>>>, 
                    std::any );

