//DumpAnEncompassedPoint.cc - A part of DICOMautomaton 2015, 2016. Written by hal clark.

#include <fstream>
#include <list>
#include <map>
#include <memory>
#include <string>    
#include <filesystem>

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "DumpAnEncompassedPoint.h"
#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorLog.h"


OperationDoc OpArgDocDumpAnEncompassedPoint(){
    OperationDoc out;
    out.name = "DumpAnEncompassedPoint";
    out.tags.emplace_back("category: meta");
    out.tags.emplace_back("category: needs refresh");

    out.desc = 
        "This operation estimates the number of spatially-overlapping images. It finds an arbitrary point within an"
        " arbitrary image, and then finds all other images which encompass the point.";

    return out;
}

bool DumpAnEncompassedPoint(Drover &DICOM_data,
                              const OperationArgPkg& /*OptArgs*/,
                              std::map<std::string, std::string>& /*InvocationMetadata*/,
                              const std::string& /*FilenameLex*/){
    const auto apoint = DICOM_data.image_data.front()->imagecoll.images.front().center();
    auto encompassing_images = DICOM_data.image_data.front()->imagecoll.get_images_which_encompass_point(apoint);

    YLOGINFO("Found " << encompassing_images.size() << " images which encompass the point " << apoint);

    return true;
}
