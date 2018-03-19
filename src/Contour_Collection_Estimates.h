//Contour_Collection_Estimates.h.

#pragma once

#include <functional>
#include <limits>
#include <list>

#include "YgorMath.h"
#include "YgorMisc.h"

template <class T> class contour_collection;


double Estimate_Contour_Separation_Multi( std::list<std::reference_wrapper<contour_collection<double>>> ccl );

