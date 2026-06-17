// EstimateEquivalentSquare.h.

#pragma once

#include <map>
#include <string>

#include "../Structs.h"


double EstimateEquivalentSquareForContours(const contour_collection<double> &cc);

OperationDoc OpArgDocEstimateEquivalentSquare();

bool EstimateEquivalentSquare(Drover &DICOM_data,
                              const OperationArgPkg& /*OptArgs*/,
                              std::map<std::string, std::string>& /*InvocationMetadata*/,
                              const std::string& /*FilenameLex*/);
