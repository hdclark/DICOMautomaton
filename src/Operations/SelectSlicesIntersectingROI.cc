//SelectSlicesIntersectingROI.cc - A part of DICOMautomaton 2016. Written by hal clark.

#include <optional>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <string>    

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "SelectSlicesIntersectingROI.h"
#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.



OperationDoc OpArgDocSelectSlicesIntersectingROI(){
    OperationDoc out;
    out.name = "SelectSlicesIntersectingROI";

    out.desc = 
        "This operation applies a whitelist to the most-recently loaded images. Images must 'slice' through one of the"
        " described ROIs in order to make the whitelist."
        " This operation is typically used to reduce long computations by trimming the field of view of extraneous image"
        " slices.";


    out.args.emplace_back();
    out.args.back() = NCWhitelistOpArgDoc();
    out.args.back().name = "NormalizedROILabelRegex";
    out.args.back().default_val = ".*";

    out.args.emplace_back();
    out.args.back() = RCWhitelistOpArgDoc();
    out.args.back().name = "ROILabelRegex";
    out.args.back().default_val = ".*";

    return out;
}



Drover SelectSlicesIntersectingROI(Drover DICOM_data,
                                   const OperationArgPkg& OptArgs,
                                   const std::map<std::string, std::string>& /*InvocationMetadata*/,
                                   const std::string& /*FilenameLex*/){

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
