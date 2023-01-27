//ExportPointClouds.cc - A part of DICOMautomaton 2019. Written by hal clark.

#include <algorithm>
#include <optional>
#include <fstream>
#include <fstream>
#include <iostream>
#include <iomanip>
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
#include "YgorMathIOXYZ.h"
#include "YgorFilesDirs.h"

#include "Explicator.h"       //Needed for Explicator class.

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../Thread_Pool.h"

#include "ExportPointClouds.h"

OperationDoc OpArgDocExportPointClouds(){
    OperationDoc out;
    out.name = "ExportPointClouds";

    out.desc = 
        "This operation writes point clouds to file.";


    out.args.emplace_back();
    out.args.back() = PCWhitelistOpArgDoc();
    out.args.back().name = "PointSelection";
    out.args.back().default_val = "last";
   

    out.args.emplace_back();
    out.args.back().name = "FilenameBase";
    out.args.back().desc = "The base filename that line samples will be written to."
                           " The file format is 'XYZ' -- a 3-column text file containing vector coordinates of the points."
                           " Metadata is excluded."
                           //" Metadata is included, but will be base64 encoded if any non-printable"
                           //" characters are detected. If no name is given, the default will be used."
                           " A '_', a sequentially-increasing number, and the '.xyz' file suffix are"
                           " appended after the base filename.";
    out.args.back().default_val = "/tmp/dcma_exportpointclouds";
    out.args.back().expected = true;
    out.args.back().examples = { "point_cloud", 
                                 "../somedir/data", 
                                 "/path/to/some/points" };
    out.args.back().mimetype = "text/plain";

    return out;
}



bool ExportPointClouds(Drover &DICOM_data,
                         const OperationArgPkg& OptArgs,
                         std::map<std::string, std::string>& /*InvocationMetadata*/,
                         const std::string& /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto PointSelectionStr = OptArgs.getValueStr("PointSelection").value();
    const auto FilenameBaseStr = OptArgs.getValueStr("FilenameBase").value();

    //-----------------------------------------------------------------------------------------------------------------

    auto PCs_all = All_PCs( DICOM_data );
    const auto PCs = Whitelist( PCs_all, PointSelectionStr );
    for(auto & pcp_it : PCs){

        // Determine which filename to use.
        const auto FN = Get_Unique_Sequential_Filename(FilenameBaseStr + "_", 6, ".xyz");

        std::fstream FO(FN, std::fstream::out);
        if(!WritePointSetToXYZ((*pcp_it)->pset, FO) || !FO){
            throw std::runtime_error("Unable to write point cloud. Cannot continue.");
        }

        YLOGINFO("Point cloud written to '" << FN << "'");
    }

    return true;
}
