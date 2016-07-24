//Liver_Kinetic_Common.h.
#pragma once

#include <map>
#include <cmath>


struct KineticModel_PixelSelectionCriteria {
    std::map<std::string,std::regex> metadata_criteria;
    long int row = -1;
    long int column = -1;

};

