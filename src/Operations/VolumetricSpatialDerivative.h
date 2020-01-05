// VolumetricSpatialDerivative.h.

#pragma once

#include <getopt.h> //Needed for 'getopts' argument parsing.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>  //Needed for exit() calls.
#include <optional>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <list>
#include <map>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>  //Needed for std::pair.
#include <vector>

#include "YgorAlgorithms.h"          //Needed for For_Each_In_Parallel<..>(...)
#include "YgorArguments.h"           //Needed for ArgumentHandler class.
#include "YgorContainers.h"          //Needed for bimap class.
#include "YgorFilesDirs.h"           //Needed for Does_File_Exist_And_Can_Be_Read(...), etc..
#include "YgorImages.h"
#include "YgorImagesIO.h"
#include "YgorImagesPlotting.h"
#include "YgorMath.h"                //Needed for vec3 class.
#include "YgorMathChebyshev.h"       //Needed for cheby_approx class.
#include "YgorMathPlottingGnuplot.h" //Needed for YgorMathPlottingGnuplot::*.
#include "YgorMisc.h"                //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorPerformance.h"         //Needed for YgorPerformance_dt_from_last().
#include "YgorStats.h"               //Needed for Stats:: namespace.
#include "YgorString.h"              //Needed for GetFirstRegex(...)

#include "Explicator.h" //Needed for Explicator class.

#include "../Structs.h"


OperationDoc OpArgDocVolumetricSpatialDerivative(void);

Drover VolumetricSpatialDerivative(Drover DICOM_data, OperationArgPkg /*OptArgs*/,
                         std::map<std::string, std::string> /*InvocationMetadata*/,
                         std::string /*FilenameLex*/);
