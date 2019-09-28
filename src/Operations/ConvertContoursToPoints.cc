//ConvertContoursToPoints.cc - A part of DICOMautomaton 2019. Written by hal clark.

#include <asio.hpp>
#include <algorithm>
#include <experimental/optional>
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

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../Thread_Pool.h"
#include "ConvertContoursToPoints.h"
#include "Explicator.h"       //Needed for Explicator class.
#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorStats.h"        //Needed for Stats:: namespace.
#include "YgorString.h"       //Needed for GetFirstRegex(...)
#include "../Surface_Meshes.h"



OperationDoc OpArgDocConvertContoursToPoints(void){
    OperationDoc out;
    out.name = "ConvertContoursToPoints";

    out.desc = 
        "This operation extracts vertices from the selected contours and converts them into a point cloud."
        " Contours are not modified.";
        
    out.notes.emplace_back(
        "Existing point clouds are ignored and unaltered."
    );
        

    out.args.emplace_back();
    out.args.back().name = "Label";
    out.args.back().desc = "A label to attach to the point cloud.";
    out.args.back().default_val = "unspecified";
    out.args.back().expected = true;
    out.args.back().examples = { "unspecified", "POIs", "peaks", "above_zero", "below_5.3" };


    out.args.emplace_back();
    out.args.back().name = "NormalizedROILabelRegex";
    out.args.back().desc = "A regex matching ROI labels/names to consider. The default will match"
                           " all available ROIs. Be aware that input spaces are trimmed to a single space."
                           " If your ROI name has more than two sequential spaces, use regex to avoid them."
                           " All ROIs have to match the single regex, so use the 'or' token if needed."
                           " Regex is case insensitive and uses extended POSIX syntax.";
    out.args.back().default_val = ".*";
    out.args.back().expected = true;
    out.args.back().examples = { ".*", ".*Body.*", "Body", "Gross_Liver",
                            R"***(.*Left.*Parotid.*|.*Right.*Parotid.*|.*Eye.*)***",
                            R"***(Left Parotid|Right Parotid)***" };

    out.args.emplace_back();
    out.args.back().name = "ROILabelRegex";
    out.args.back().desc = "A regex matching ROI labels/names to consider. The default will match"
                           " all available ROIs. Be aware that input spaces are trimmed to a single space."
                           " If your ROI name has more than two sequential spaces, use regex to avoid them."
                           " All ROIs have to match the single regex, so use the 'or' token if needed."
                           " Regex is case insensitive and uses extended POSIX syntax.";
    out.args.back().default_val = ".*";
    out.args.back().expected = true;
    out.args.back().examples = { ".*", ".*body.*", "body", "Gross_Liver",
                                 R"***(.*left.*parotid.*|.*right.*parotid.*|.*eyes.*)***",
                                 R"***(left_parotid|right_parotid)***" };

    return out;
}



Drover ConvertContoursToPoints(Drover DICOM_data, OperationArgPkg OptArgs, std::map<std::string,std::string> /*InvocationMetadata*/, std::string FilenameLex){

    Explicator X(FilenameLex);

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto LabelStr = OptArgs.getValueStr("Label").value();

    const auto NormalizedROILabelRegex = OptArgs.getValueStr("NormalizedROILabelRegex").value();
    const auto ROILabelRegex = OptArgs.getValueStr("ROILabelRegex").value();

    //-----------------------------------------------------------------------------------------------------------------

    //Stuff references to all contours into a list. Remember that you can still address specific contours through
    // the original holding containers (which are not modified here).
    auto cc_all = All_CCs( DICOM_data );
    auto cc_ROIs = Whitelist( cc_all, { { "ROIName", ROILabelRegex },
                                        { "NormalizedROIName", NormalizedROILabelRegex } } );
    if(cc_ROIs.empty()){
        throw std::invalid_argument("No contours selected. Cannot continue.");
    }

    //Construct a destination for the point clouds.
    DICOM_data.point_data.emplace_back( std::make_unique<Point_Cloud>() );

    for(auto & cc_refw : cc_ROIs){
        for(const auto & c : cc_refw.get().contours){
            for(const auto & v : c.points){
                DICOM_data.point_data.back()->pset.points.emplace_back( v );
            }
        }
    }

////////////// TODO ///////////////

    // Determine the common set of contour metadata and assign it to the point data.
    {
        auto cm = contour_collection<double>().get_common_metadata(cc_ROIs, {});
        DICOM_data.point_data.back()->pset.metadata = cm;
    }

    DICOM_data.point_data.back()->pset.metadata["Label"] = LabelStr;
    DICOM_data.point_data.back()->pset.metadata["Description"] = "Point cloud derived from planar contours.";

    return DICOM_data;
}
