//DumpPixelValuesOverTimeForAnEncompassedPoint.cc - A part of DICOMautomaton 2015, 2016. Written by hal clark.

#include <fstream>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <string>    
#include <utility>            //Needed for std::pair.

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "DumpPixelValuesOverTimeForAnEncompassedPoint.h"
#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.


OperationDoc OpArgDocDumpPixelValuesOverTimeForAnEncompassedPoint(){
    OperationDoc out;
    out.name = "DumpPixelValuesOverTimeForAnEncompassedPoint";

    out.desc = 
        "Output the pixel values over time for a generic point."
        " Currently the point is arbitrarily taken to tbe the centre of the first image."
        " This is useful for quickly and programmatically inspecting trends, but the"
        " SFML_Viewer operation is better for interactive exploration.";

    return out;
}

bool DumpPixelValuesOverTimeForAnEncompassedPoint(Drover &DICOM_data,
                                                    const OperationArgPkg& /*OptArgs*/,
                                                    std::map<std::string, std::string>& /*InvocationMetadata*/,
                                                    const std::string& /*FilenameLex*/){

    const auto apoint = DICOM_data.image_data.front()->imagecoll.images.front().center();
    auto encompassing_images = DICOM_data.image_data.front()->imagecoll.get_images_which_encompass_point(apoint);
    const int channel = 0;

    std::cout << "time\t";
    std::cout << "pixel intensity\t";
    std::cout << "modality\t";
    std::cout << "image center\t";
    std::cout << "image volume" << std::endl;
    for(const auto &img_it : encompassing_images){
        std::cout << img_it->metadata.find("FrameReferenceTime")->second << "\t";
        std::cout << img_it->value(apoint, channel) << "\t";
        std::cout << img_it->metadata.find("Modality")->second << "\t";
        std::cout << img_it->center() << "\t";
        std::cout << (img_it->rows * img_it->columns * img_it->pxl_dx * img_it->pxl_dy * img_it->pxl_dz) << std::endl;
    }

    return true;
}
