//DetectShapes3D.cc - A part of DICOMautomaton 2019. Written by hal clark.

#include <any>
#include <optional>
#include <functional>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>    

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../YgorImages_Functors/Compute/Detect_Geometry_Clustered_RANSAC.h"
#include "DetectShapes3D.h"
#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.



OperationDoc OpArgDocDetectShapes3D(){
    OperationDoc out;
    out.name = "DetectShapes3D";
    out.desc = "This operation attempts to detect shapes in image volumes.";

    out.args.emplace_back();
    out.args.back() = IAWhitelistOpArgDoc();
    out.args.back().name = "ImageSelection";
    out.args.back().default_val = "all";
    
    return out;
}



bool DetectShapes3D(Drover &DICOM_data,
                      const OperationArgPkg& OptArgs,
                      const std::map<std::string, std::string>&
                      /*InvocationMetadata*/,
                      const std::string& /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();

    //-----------------------------------------------------------------------------------------------------------------
    {
        auto IAs_all = All_IAs( DICOM_data );
        auto IAs = Whitelist( IAs_all, ImageSelectionStr );
        for(auto & iap_it : IAs){
            DetectGeometryClusteredRANSACUserData ud;
            ud.inc_lower_threshold = 0.1; // Ignore voxels with values < 0.

            if(!(*iap_it)->imagecoll.Compute_Images( ComputeDetectGeometryClusteredRANSAC, { }, { }, &ud )){
                throw std::runtime_error("Unable to perform shape detection.");
            }
        }
    }

    return true;
}
