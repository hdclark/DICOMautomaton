
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
    YLOGINFO("The Z values are " << ZValues);

    contour_of_points<double> cop;
    contour_collection<double> cc;

    cop.closed = true;

    std::vector<double> x_values = ValueStringToDoubleList(XValues);
    std::vector<double> y_values = ValueStringToDoubleList(YValues);
    std::vector<double> z_values = ValueStringToDoubleList(ZValues);

    if(x_values.size()!=y_values.size()||y_values.size()!=z_values.size()) {
        YLOGWARN("Each list must have the same number of numbers");
        return true;
    }

    if(x_values.size() < 3) {
        YLOGWARN("We require more than 3 points to be in a contour");
        return true;
    }

    for(int i = 0; i < x_values.size(); i++) {
        const vec3<double> p(static_cast<double>(x_values[i]), 
                            static_cast<double>(y_values[i]),
                            static_cast<double>(z_values[i]));
        cop.points.emplace_back(p);
    }
    cop.metadata["ROILabel"] = ROILabel;

    cc.contours.emplace_back(cop);

    DICOM_data.Ensure_Contour_Data_Allocated();
    DICOM_data.contour_data->ccs.emplace_back(cc);
    return true;
}

std::vector<double> ValueStringToDoubleList(std::string input) {
    std::vector<double> values;
    size_t pos = 0;
    std::string delimiter = " ";
    std::string token;
    while ((pos = input.find(delimiter)) != std::string::npos) {
        token = input.substr(0, pos);
        try {
            values.emplace_back(stod(token));
            std::string error_message = "Invalid Input";
        } catch(std::string error_message) {
            YLOGWARN(error_message);
            return std::vector<double>();
        }
        input.erase(0, pos + delimiter.length());
    }
    return values;
}

