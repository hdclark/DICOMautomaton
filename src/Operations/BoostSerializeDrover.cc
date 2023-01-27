//BoostSerializeDrover.cc - A part of DICOMautomaton 2016. Written by hal clark.

#include <filesystem>
#include <optional>
#include <list>
#include <map>
#include <memory>
#include <ostream>
#include <stdexcept>
#include <string>    

#include "../Common_Boost_Serialization.h"
#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorLog.h"


OperationDoc OpArgDocBoost_Serialize_Drover(){
    OperationDoc out;
    out.name = "Boost_Serialize_Drover";
    out.desc = 
        "This operation exports all loaded state to a serialized format that can be loaded again later."
        " Is is especially useful for suspending long-running operations with intermittant interactive sub-operations.";


    out.args.emplace_back();
    out.args.back().name = "Filename";
    out.args.back().desc = "The filename (or full path name) to which the serialized data should be written."
                           " The file format is gzipped XML, which should be portable across most CPUs.";
    out.args.back().default_val = "/tmp/boost_serialized_drover.xml.gz";
    out.args.back().expected = true;
    out.args.back().examples = { "/tmp/out.xml.gz", 
                            "./out.xml.gz",
                            "out.xml.gz" };
    out.args.back().mimetype = "application/octet-stream";


    out.args.emplace_back();
    out.args.back().name = "Components";
    out.args.back().desc = "Which components to include in the output."
                           " Currently, any combination of (all images), (all contours), (all point clouds), "
                           " (all surface meshes), and (all treatment plans) can be selected."
                           " Note that RTDOSEs are treated as images.";
    out.args.back().default_val = "images+contours+pointclouds+surfacemeshes+rtplans";
    out.args.back().expected = true;
    out.args.back().examples = { "images",
                                 "images+pointclouds",
                                 "images+pointclouds+surfacemeshes",
                                 "pointclouds+surfacemeshes",
                                 "rtplans+images+contours",
                                 "contours+images+pointclouds" };

    return out;
}

bool Boost_Serialize_Drover(Drover &DICOM_data,
                              const OperationArgPkg& OptArgs,
                              std::map<std::string, std::string>& /*InvocationMetadata*/,
                              const std::string& /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    auto FilenameStr = OptArgs.getValueStr("Filename").value();
    auto ComponentsStr = OptArgs.getValueStr("Components").value();

    //-----------------------------------------------------------------------------------------------------------------

    const auto regex_images   = Compile_Regex(".*ima?ge?s?.*");
    const auto regex_contours = Compile_Regex(".*cont?o?u?r?s?.*");
    const auto regex_pclouds  = Compile_Regex(".*po?i?n?t?.?clo?u?d?s?.*");
    const auto regex_smeshes  = Compile_Regex(".*su?r?f?a?c?e?.?mes?h?e?s?.*");
    const auto regex_rtplans  = Compile_Regex(".*r?t?.?pla?n?s?.*");

    const bool include_images   = std::regex_match(ComponentsStr, regex_images);
    const bool include_contours = std::regex_match(ComponentsStr, regex_contours);
    const bool include_pclouds  = std::regex_match(ComponentsStr, regex_pclouds);
    const bool include_smeshes  = std::regex_match(ComponentsStr, regex_smeshes);
    const bool include_rtplans  = std::regex_match(ComponentsStr, regex_rtplans);

    const std::filesystem::path apath(FilenameStr);

    // Figure out what needs to be serialized.
    //
    // Note: The Drover class holds everything as shared_ptrs or containers of shared_ptrs, so these copies are
    // superficial.
    Drover d;
    if(include_images){
        d.image_data = DICOM_data.image_data;
    }
    if(include_contours){
        d.contour_data = DICOM_data.contour_data;
    }
    if(include_pclouds){
        d.point_data = DICOM_data.point_data;
    }
    if(include_smeshes){
        d.smesh_data = DICOM_data.smesh_data;
    }
    if(include_rtplans){
        d.rtplan_data = DICOM_data.rtplan_data;
    }

    const auto res = Common_Boost_Serialize_Drover(d, apath);
    if(res){
        YLOGINFO("Dumped serialization to file " << apath);
    }else{
        throw std::runtime_error("Unable dump serialization to file " + apath.string());
    }

    return true;
}
