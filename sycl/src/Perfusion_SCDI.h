
#pragma once

#include <exception>
#include <functional>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <string>    
#include <vector>

#include <cstdlib>            //Needed for exit() calls.
#include <utility>            //Needed for std::pair.

#include "YgorMath.h"         //Needed for samples_1D.

void
Launch_SCDI( samples_1D<double> &,
             samples_1D<double> &, 
             std::vector<samples_1D<double>> & );

void
MultiplyVectorByScalar(std::vector<float> &v, float k);

