//Liver_Kinetic_Common.h.
#pragma once

#ifdef DCMA_USE_GNU_GSL

#include <map>
#include <cmath>
#include <cstdint>


struct KineticModel_PixelSelectionCriteria {
    std::map<std::string,std::regex> metadata_criteria;
    int64_t row = -1;
    int64_t column = -1;

};

#endif // DCMA_USE_GNU_GSL

