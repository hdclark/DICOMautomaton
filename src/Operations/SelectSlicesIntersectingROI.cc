//SelectSlicesIntersectingROI.cc - A part of DICOMautomaton 2016. Written by hal clark.

#include <experimental/optional>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <regex>
#include <string>    

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "SelectSlicesIntersectingROI.h"
#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.



OperationDoc OpArgDocSelectSlicesIntersectingROI(void){
    OperationDoc out;
    out.name = "SelectSlicesIntersectingROI";

    out.desc = 
        "This operation applies a whitelist to the most-recently loaded images. Images must 'slice' through one of the"
        " described ROIs in order to make the whitelist."
        " This operation is typically used to reduce long computations by trimming the field of view of extraneous image"
        " slices.";


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



Drover SelectSlicesIntersectingROI(Drover DICOM_data, OperationArgPkg OptArgs, std::map<std::string,std::string> /*InvocationMetadata*/, std::string /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ROILabelRegex = OptArgs.getValueStr("ROILabelRegex").value();
    const auto NormalizedROILabelRegex = OptArgs.getValueStr("NormalizedROILabelRegex").value();
    //-----------------------------------------------------------------------------------------------------------------

    //Stuff references to all contours into a list. Remember that you can still address specific contours through
    // the original holding containers (which are not modified here).
    auto cc_all = All_CCs( DICOM_data );
    auto cc_ROIs = Whitelist( cc_all, { { "ROIName", ROILabelRegex },
                                        { "NormalizedROIName", NormalizedROILabelRegex } } );


    //Generate a closure that discards images not encompassing any ROI contours.
    const auto retain_encompassing_imgs = [&cc_ROIs](const planar_image<float, double> &animg) -> bool {
                //Retain the image IFF it intersects one of the contours.
                for(const auto &cc_ref : cc_ROIs){
                    for(const auto &acontour : cc_ref.get().contours){
                        if(animg.encompasses_contour_of_points(acontour)) return true;
                    }
                }
                return false;
    };

    //Cycle over all images and dose arrays, trimming spurious images.
    for(auto &img_arr : DICOM_data.image_data){
        img_arr->imagecoll.Retain_Images_Satisfying( retain_encompassing_imgs );
    }

    return DICOM_data;
}
