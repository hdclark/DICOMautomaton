// MinkowskiSum3D.h.

#pragma once

/*
#include <iostream>
#include <sstream>
#include <fstream>
#include <iomanip>
*/
#include <string>
/*
#include <vector>
#include <set>
#include <map>
#include <unordered_map>
#include <list>
#include <functional>
#include <thread>
#include <array>
#include <mutex>
#include <limits>
#include <cmath>


#include <getopt.h> //Needed for 'getopts' argument parsing.
#include <cstdlib>  //Needed for exit() calls.
#include <utility>  //Needed for std::pair.
#include <algorithm>
#include <experimental/optional>

#include "YgorMisc.h"                //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorMath.h"                //Needed for vec3 class.
#include "YgorMathPlottingGnuplot.h" //Needed for YgorMathPlottingGnuplot::*.
#include "YgorMathChebyshev.h"       //Needed for cheby_approx class.
#include "YgorStats.h"               //Needed for Stats:: namespace.
#include "YgorFilesDirs.h"           //Needed for Does_File_Exist_And_Can_Be_Read(...), etc..
#include "YgorContainers.h"          //Needed for bimap class.
#include "YgorPerformance.h"         //Needed for YgorPerformance_dt_from_last().
#include "YgorAlgorithms.h"          //Needed for For_Each_In_Parallel<..>(...)
#include "YgorArguments.h"           //Needed for ArgumentHandler class.
#include "YgorString.h"              //Needed for GetFirstRegex(...)
#include "YgorImages.h"
#include "YgorImagesIO.h"
#include "YgorImagesPlotting.h"
*/

#include "../Structs.h"


OperationDoc OpArgDocMinkowskiSum3D(void);

Drover
MinkowskiSum3D(Drover DICOM_data, OperationArgPkg /*OptArgs*/, std::map<std::string, std::string> /*InvocationMetadata*/, std::string /*FilenameLex*/);

