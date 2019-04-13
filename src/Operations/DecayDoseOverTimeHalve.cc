//DecayDoseOverTimeHalve.cc - A part of DICOMautomaton 2017. Written by hal clark.

#include <experimental/any>
#include <experimental/optional>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>    

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../YgorImages_Functors/Grouping/Misc_Functors.h"
#include "../YgorImages_Functors/Processing/DecayDoseOverTime.h"
#include "DecayDoseOverTimeHalve.h"
#include "Explicator.h"       //Needed for Explicator class.
#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.



OperationDoc OpArgDocDecayDoseOverTimeHalve(void){
    OperationDoc out;
    out.name = "DecayDoseOverTimeHalve";

    out.desc = 
        "This operation transforms a dose map (assumed to be delivered some distant time in the past) to simulate 'decay'"
        " or 'evaporation' or 'forgivance' of radiation dose by simply halving the value. This model is only appropriate "
        " at long time-scales, but there is no cut-off or threshold to denote what is sufficiently 'long'. So use at "
        " your own risk. As a rule of thumb, do not use this routine if fewer than 2-3y have elapsed.";
        
    out.notes.emplace_back(
        "This routine will combine spatially-overlapping images by summing voxel intensities. So if you have a time"
        " course it may be more sensible to aggregate images in some way (e.g., spatial averaging) prior to calling"
        " this routine."
    );

    out.notes.emplace_back(
        "Since this routine is meant to be applied multiple times in succession for different ROIs (which possibly"
        " overlap), all images are imbued with a second channel that is treated as a mask. Mask channels are"
        " permanently attached so that multiple passes will not erroneously decay dose. If this will be problematic,"
        " the extra column should be trimmed immediately after calling this routine."
    );


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



Drover DecayDoseOverTimeHalve(Drover DICOM_data, OperationArgPkg OptArgs, std::map<std::string,std::string> /*InvocationMetadata*/, std::string FilenameLex){

    DecayDoseOverTimeUserData ud;
    ud.model = DecayDoseOverTimeMethod::Halve;
    ud.channel = 0; // A second channel (added on-the-fly) will be used to store a modification mask.

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ROILabelRegex = OptArgs.getValueStr("ROILabelRegex").value();
    const auto NormalizedROILabelRegex = OptArgs.getValueStr("NormalizedROILabelRegex").value();

    //-----------------------------------------------------------------------------------------------------------------

    Explicator X(FilenameLex);

    //Merge the image arrays if necessary.
    if(DICOM_data.image_data.empty()){
        throw std::invalid_argument("This routine requires at least one image array. Cannot continue");
    }

    auto img_arr_ptr = DICOM_data.image_data.front();
    if(img_arr_ptr == nullptr){
        throw std::runtime_error("Encountered a nullptr when expecting a valid Image_Array ptr.");
    }else if(img_arr_ptr->imagecoll.images.empty()){
        throw std::runtime_error("Encountered a Image_Array with valid images -- no images found.");
    }

    //Stuff references to all contours into a list. Remember that you can still address specific contours through
    // the original holding containers (which are not modified here).
    auto cc_all = All_CCs( DICOM_data );
    auto cc_ROIs = Whitelist( cc_all, { { "ROIName", ROILabelRegex },
                                        { "NormalizedROIName", NormalizedROILabelRegex } } );

    if(cc_ROIs.empty()){
        throw std::invalid_argument("No contours selected. Cannot continue.");
    }

    // Perform the dose modification.
    if(!img_arr_ptr->imagecoll.Process_Images_Parallel( GroupIndividualImages,
                                                        DecayDoseOverTime,
                                                        {}, cc_ROIs, &ud )){
        throw std::runtime_error("Unable to decay dose (via halving).");
    }

    return DICOM_data;
}
