
//CopyContours.cc - A part of DICOMautomaton 2021. Written by hal clark.

#include <list>
#include <map>

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
    out.args.back().desc = "List of contours separated by | character, where each countour is a list of doubles separated by spaces. Contains x-values";
    out.args.back().default_val = "0";
    out.args.back().expected = true;

    out.args.emplace_back();
    out.args.back().name = "YValues";
    out.args.back().desc = "List of contours separated by | character, where each countour is a list of doubles separated by spaces. Contains y-values";
    out.args.back().default_val = "0";
    out.args.back().expected = true;

    out.args.emplace_back();
    out.args.back().name = "ZValues";
    out.args.back().desc = "List of contours separated by | character, where each countour is a list of doubles separated by spaces. Contains z-values";
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

    contour_collection<double> cc;

    std::vector<std::string> x_contours = SplitString(XValues, "|");
    std::vector<std::string> y_contours = SplitString(YValues, "|");
    std::vector<std::string> z_contours = SplitString(ZValues, "|");
    

    if(x_contours.size()!=y_contours.size()||y_contours.size()!=z_contours.size()) {
        YLOGWARN("Ensure same number of contours for each dimension");
        return true;
    }

    if(x_contours.size() < 1) {
        YLOGWARN("We require that there is at least one contour.");
        return true;
    }

    DICOM_data.Ensure_Contour_Data_Allocated();
    
    std::vector<std::vector<double>> x_cops;
    std::vector<std::vector<double>> y_cops;
    std::vector<std::vector<double>> z_cops;

    for(int i = 0; i < x_contours.size(); i++) {
        std::vector<std::string> x_cop_string = SplitString(x_contours[i], " ");
        std::vector<std::string> y_cop_string = SplitString(y_contours[i], " ");
        std::vector<std::string> z_cop_string = SplitString(z_contours[i], " ");
        if(x_cop_string.size()!=y_cop_string.size()||y_cop_string.size()!=z_cop_string.size()) {
            YLOGWARN("Ensure each contour has the same number of points for each dimension");
        }
        if(x_cop_string.size() < 3) {
            YLOGWARN("Each contour must have at least 3 points");
        }
        std::vector<double> x_cop;
        std::vector<double> y_cop;
        std::vector<double> z_cop;
        for(int j = 0; j < x_cop_string.size(); j++) {
            try {
                x_cop.emplace_back(stod(x_cop_string[j]));
                y_cop.emplace_back(stod(y_cop_string[j]));
                z_cop.emplace_back(stod(z_cop_string[j]));
                std::string error_message = "Invalid Input";
            } catch(std::string error_message) {
                return true;
            }
        }
        x_cops.emplace_back(x_cop);
        y_cops.emplace_back(y_cop);
        z_cops.emplace_back(z_cop);
    }

    for(int i = 0; i < x_cops.size(); i++) {
        contour_of_points<double> cop;
        for(int j = 0; j < x_cops[i].size(); j++) {
            const vec3<double> p(static_cast<double>(x_cops[i][j]),
                                static_cast<double>(y_cops[i][j]),
                                static_cast<double>(z_cops[i][j]));
            cop.points.emplace_back(p);
        }
        cop.closed = true;
        cop.metadata["ROILabel"] = ROILabel;
        cc.contours.emplace_back(cop);
    }

    DICOM_data.contour_data->ccs.emplace_back(cc);
    return true;
}

std::vector<std::string> SplitString(std::string input, std::string delimiter) {
    std::vector<std::string> values;
    size_t pos = 0;
    std::string token;
    while ((pos = input.find(delimiter)) != std::string::npos) {
        token = input.substr(0, pos);
        try {
            values.emplace_back(token);
            std::string error_message = "Invalid Input";
        } catch(std::string error_message) {
            return std::vector<std::string>();
        }
        input.erase(0, pos + delimiter.length());
    }
    values.emplace_back(input);
    return values;
}