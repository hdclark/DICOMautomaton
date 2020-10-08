//BEDConversion.h.
#pragma once


#include <any>
#include <functional>
#include <list>

#include "YgorImages.h"

template <class T> class contour_collection;


struct BEDConversionUserData {

    // -----------------------------
    // The type of model to use.
    enum class
    Model {

        // BED based on a simplistic linear-quadratic model. No repopulation effects accounted for.
        BEDSimpleLinearQuadratic,

        // EQDx based on a simplistic linear-quadratic model. No repopulation effects accounted for.
        EQDXSimpleLinearQuadratic,

        // EQDx based on a simplistic linear-quadratic model. No repopulation effects accounted for.
        // The prescription dose is scaled to give x dose per fraction; the effective number of fractions
        // is extracted from the prescription dose EQDx and used for all voxels. This eliminates
        // the need to specify that every voxel will get x dose per fraction.
        EQDXPinnedLinearQuadratic,

    } model = Model::EQDXSimpleLinearQuadratic;

    // -----------------------------
    // SimpleLinearQuadratic parameters.
    double NumberOfFractions = -1.0; // i.e., as actually delivered.

    double AlphaBetaRatioLate = 3.0; // alpha/beta for late-responding (i.e., normal, non-tumourous) tissues.
    double AlphaBetaRatioEarly = 10.0; // alpha/beta for early-responding (i.e., tumourous and some normal) tissues.

    double TargetDosePerFraction = 2.0; // = the 'x' in EQDx. Should be 2.0 for EQD2.

    // -----------------------------
    // PinnedLinearQuadratic parameters.
    double PrescriptionDose = -1.0;

};


bool BEDConversion(planar_image_collection<float,double>::images_list_it_t first_img_it,
                   std::list<planar_image_collection<float,double>::images_list_it_t> ,
                   std::list<std::reference_wrapper<planar_image_collection<float,double>>>,
                   std::list<std::reference_wrapper<contour_collection<double>>>, 
                   std::any );

