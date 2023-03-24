
//CopyContours.cc - A part of DICOMautomaton 2021. Written by hal clark.

#include <list>
#include <map>
#include <string>

#include "../Structs.h"
#include "YgorLog.h"

#include "CreateCustomContour.h"

OperationDoc OpArgDocCreateCustomContour(){
    OperationDoc out;
    out.name = "CreateCustomContour";

    out.desc = 
        "This operation creates a new contour from a list of x, y, and z coordinates.";

    out.args.emplace_back();
    out.args.back().name = "ROILabel";
    out.args.back().desc = "A label to attach to the copied ROI contours.";
    out.args.back().default_val = "unspecified";
    out.args.back().expected = true;

    out.args.emplace_back();
    out.args.back().name = "XValues";
    out.args.back().desc = "List of X coordinates of the points in the contour";
    out.args.back().default_val = "0";
    out.args.back().expected = true;

    out.args.emplace_back();
    out.args.back().name = "YValues";
    out.args.back().desc = "List of Y coordinates of the points in the contour";
    out.args.back().default_val = "0";
    out.args.back().expected = true;

    out.args.emplace_back();
    out.args.back().name = "ZValues";
    out.args.back().desc = "List of Z coordinates of the points in the contour";
    out.args.back().default_val = "0";
    out.args.back().expected = true;
    return out;
}

bool CreateCustomContour(Drover &DICOM_data,
                    const OperationArgPkg& OptArgs,
                    std::map<std::string, std::string>& /*InvocationMetadata*/,
                    const std::string& FilenameLex){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ROILabel = OptArgs.getValueStr("ROILabel").value();
    const auto XValues = OptArgs.getValueStr("XValues").value();
    const auto YValues = OptArgs.getValueStr("YValues").value();
    const auto ZValues = OptArgs.getValueStr("ZValues").value();
    //-----------------------------------------------------------------------------------------------------------------

    //Stuff references to all contours into a list. Remember that you can still address specific contours through
    // the original holding containers (which are not modified here).
    YLOGINFO("The X values are " << XValues);
    YLOGINFO("The Y values are " << YValues);
    YLOGINFO("The Z values are " << YValues);
    return true;
}
