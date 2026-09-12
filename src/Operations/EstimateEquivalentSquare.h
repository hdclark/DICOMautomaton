// EstimateEquivalentSquare.h.

#pragma once

#include <map>
#include <string>

#include "../Structs.h"


enum class EquivalentSquareHoleDetection {
    SignedContourOrientation,
    SimpleOverlap,
};

bool ContourHasSelfIntersection(const contour_of_points<double> &c);
bool ContourCollectionsHaveIntersections(const contour_collection<double> &cc);
bool ContourCollectionIsSimpleNested(const contour_collection<double> &cc);

double EstimateEquivalentSquareForContours(
    const contour_collection<double> &cc,
    EquivalentSquareHoleDetection hole_detection = EquivalentSquareHoleDetection::SignedContourOrientation);

OperationDoc OpArgDocEstimateEquivalentSquare();

bool EstimateEquivalentSquare(Drover &DICOM_data,
                              const OperationArgPkg& /*OptArgs*/,
                              std::map<std::string, std::string>& /*InvocationMetadata*/,
                              const std::string& /*FilenameLex*/);
