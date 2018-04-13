//DumpROIDoseInfo.cc - A part of DICOMautomaton 2016. Written by hal clark.

#include <experimental/optional>
#include <fstream>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>    
#include <utility>            //Needed for std::pair.

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "DumpROIDoseInfo.h"
#include "Explicator.h"       //Needed for Explicator class.


OperationDoc OpArgDocDumpROIDoseInfo(void){
    OperationDoc out;
    out.name = "DumpROIDoseInfo";
    out.desc = "";

    out.notes.emplace_back("");


    out.args.emplace_back();
    out.args.back().name = "ROILabelRegex";
    out.args.back().desc = "A regex matching ROI labels/names to consider. The default will match"
                      " all available ROIs. Be aware that input spaces are trimmed to a single space."
                      " If your ROI name has more than two sequential spaces, use regex to avoid them."
                      " All ROIs have to match the single regex, so use the 'or' token if needed."
                      " Regex is case insensitive and uses grep syntax.";
    out.args.back().default_val = ".*";
    out.args.back().expected = true;
    out.args.back().examples = { ".*", ".*body.*", "body", "Gross_Liver", 
                            R"***(.*parotid.*|.*sub.*mand.*)***", 
                            R"***(left_parotid|right_parotid|eyes)***" };


    return out;
}

Drover DumpROIDoseInfo(Drover DICOM_data, OperationArgPkg OptArgs, std::map<std::string,std::string> /*InvocationMetadata*/, std::string FilenameLex){

    //This operation computes mean voxel doses with the given ROIs.
    // 

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ROILabelRegex = OptArgs.getValueStr("ROILabelRegex").value();
    //-----------------------------------------------------------------------------------------------------------------
    const auto theregex = std::regex(ROILabelRegex, std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);

    Explicator X(FilenameLex);

    //Using the "old" method. Specific ROIs cannot be individually selected.
    //
    // NOTE: Ensure doses have been loaded into Dose_Arrays and not into Image_Arrays. Do this by altering the
    //       loader. You should also be able to simply cast Image_Arrays to Dose_Arrays for purposes of this routine.
    //

    if(!DICOM_data.Meld(false)) throw std::runtime_error("Unable to meld dose, image, and contour data");
    auto bdm = DICOM_data.Bounded_Dose_Means();

    for(auto & apair : bdm){
        auto cwm_it = apair.first;
        auto mean_dose = apair.second;

        auto raw_roi_name = cwm_it->Raw_ROI_name;
        auto norm_roi_name = X(raw_roi_name);

        std::cout << "Mean dose: " << mean_dose << " for raw-named ROI '" << raw_roi_name << "'" << std::endl;
        std::cout << "Mean dose: " << mean_dose << " for normalize-named ROI '" << norm_roi_name << "'" << std::endl;
    }


    //Using the "new" method of YgorImaging functors.

/*
    //Stuff references to all contours into a list. Remember that you can still address specific contours through
    // the original holding containers (which are not modified here).
    std::list<std::reference_wrapper<contour_collection<double>>> cc_all;
    for(auto & cc : DICOM_data.contour_data->ccs){
        auto base_ptr = reinterpret_cast<contour_collection<double> *>(&cc);
        cc_all.push_back( std::ref(*base_ptr) );
    }

    //Whitelist contours using the provided regex.
    auto cc_ROIs = cc_all;
    cc_ROIs.remove_if([=](std::reference_wrapper<contour_collection<double>> cc) -> bool {
                   const auto ROINameOpt = cc.get().contours.front().GetMetadataValueAs<std::string>("ROIName");
                   const auto ROIName = ROINameOpt.value();
                   return !(std::regex_match(ROIName,theregex));
    });


    //Gather all contours for each volume of interest.
    typedef std::tuple<std::string,std::string,std::string> key_t; //PatientID, ROIName, NormalizedROIName.
    typedef std::list<std::reference_wrapper< contour_of_points<double>>> val_t; 
*/



    return DICOM_data;
}
