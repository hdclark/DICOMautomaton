//ExportSurfaceMeshes.cc - A part of DICOMautomaton 2020. Written by hal clark.

#include <algorithm>
#include <optional>
#include <fstream>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <regex>
#include <set> 
#include <stdexcept>
#include <string>    
#include <utility>            //Needed for std::pair.
#include <vector>

#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorLog.h"
#include "YgorStats.h"        //Needed for Stats:: namespace.
#include "YgorString.h"       //Needed for GetFirstRegex(...)
#include "YgorMathIOOFF.h"
#include "YgorFilesDirs.h"

#include "Explicator.h"       //Needed for Explicator class.

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../Thread_Pool.h"

#include "ExportSurfaceMeshes.h"
#include "ExportSurfaceMeshesPLY.h"

OperationDoc OpArgDocExportSurfaceMeshes(){
    return OpArgDocExportSurfaceMeshesPLY();
}

bool ExportSurfaceMeshes(Drover &DICOM_data,
                           const OperationArgPkg& OptArgs,
                           std::map<std::string, std::string>& InvocationMetadata,
                           const std::string& FilenameLex){
    return ExportSurfaceMeshesPLY(DICOM_data, OptArgs, InvocationMetadata, FilenameLex);
}

