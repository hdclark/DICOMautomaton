//DumpAllOrderedImageMetadataToFile.cc - A part of DICOMautomaton 2015, 2016. Written by hal clark.

#include <fstream>
#include <list>
#include <map>
#include <memory>
#include <set> 
#include <string>    
#include <utility>            //Needed for std::pair.
#include <filesystem>

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "DumpAllOrderedImageMetadataToFile.h"
#include "YgorFilesDirs.h"    //Needed for Does_File_Exist_And_Can_Be_Read(...), etc..
#include "YgorImages.h"
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.


OperationDoc OpArgDocDumpAllOrderedImageMetadataToFile(){
    OperationDoc out;
    out.name = "DumpAllOrderedImageMetadataToFile";
    out.desc = 
        "Dump exactly what order the data will be in for the following analysis.";

    return out;
}

bool DumpAllOrderedImageMetadataToFile(Drover &DICOM_data,
                                         const OperationArgPkg& /*OptArgs*/,
                                         std::map<std::string, std::string>& /*InvocationMetadata*/,
                                         const std::string& /*FilenameLex*/){

    const auto Dump_All_Ordered_Image_Metadata_To_File = 
        [](const decltype(DICOM_data.image_data.front()->imagecoll.images) &images, const std::string &dumpfile) -> void {

            //Get a superset of all metadata names.
            std::set<std::string> sset;
            for(const auto &img : images){
                for(const auto &md_pair : img.metadata){
                    sset.insert(md_pair.first);
                }
            }

            //Cycle through the images and print available tags.
            std::stringstream df;
            for(const auto &akey : sset) df << akey << "\t";
            df << std::endl;
            for(const auto &img : images){
                for(const auto &akey : sset) df << img.metadata.find(akey)->second << "\t";
                df << std::endl;
            }
            if(!OverwriteStringToFile(df.str(),dumpfile)) throw std::runtime_error("Unable to dump ordered image metadata to file");
            return;
        };

    Dump_All_Ordered_Image_Metadata_To_File(DICOM_data.image_data.front()->imagecoll.images, "/tmp/ordered_image_metadata.tsv");

    return true;
}
