//DumpAnEncompassedPoint.cc - A part of DICOMautomaton 2015, 2016. Written by hal clark.

#include <fstream>
#include <list>
#include <map>
#include <memory>
#include <string>    

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "DumpAnEncompassedPoint.h"
#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.


OperationDoc OpArgDocDumpAnEncompassedPoint(void){
    OperationDoc out;
    out.name = "DumpAnEncompassedPoint";
    out.desc = "";

    out.notes.emplace_back("");
    return out;
}

Drover DumpAnEncompassedPoint(Drover DICOM_data, OperationArgPkg /*OptArgs*/, std::map<std::string,std::string> /*InvocationMetadata*/, std::string /*FilenameLex*/){
    //Grab an arbitrary point from one of the images. Find all other images which encompass the point.
    const auto apoint = DICOM_data.image_data.front()->imagecoll.images.front().center();
    auto encompassing_images = DICOM_data.image_data.front()->imagecoll.get_images_which_encompass_point(apoint);

    FUNCINFO("Found " << encompassing_images.size() << " images which encompass the point " << apoint);

    return DICOM_data;
}
