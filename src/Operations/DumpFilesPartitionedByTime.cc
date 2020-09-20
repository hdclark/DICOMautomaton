//DumpFilesPartitionedByTime.cc - A part of DICOMautomaton 2015, 2016. Written by hal clark.

#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <string>    
#include <utility>            //Needed for std::pair.

#include "../Structs.h"
#include "DumpFilesPartitionedByTime.h"
#include "YgorImages.h"
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.


OperationDoc OpArgDocDumpFilesPartitionedByTime(){
    OperationDoc out;
    out.name = "DumpFilesPartitionedByTime";
       
    out.desc = 
        " This operation prints PACS filenames along with the associated time. It is more focused than the metadata "
        " dumpers above. This data can be used for many things, such as image viewers which are not DICOM-aware or"
        " deformable registration on time series data.";

    return out;
}

Drover DumpFilesPartitionedByTime(Drover DICOM_data,
                                  const OperationArgPkg& /*OptArgs*/,
                                  const std::map<std::string, std::string>& /*InvocationMetadata*/,
                                  const std::string& /*FilenameLex*/){

    std::multimap<std::string,std::string> partitions;
    for(auto &img_arr : DICOM_data.image_data){
        for(auto &img : img_arr->imagecoll.images){
            if(!img.MetadataKeyPresent("dt")){
                FUNCWARN("Time key is not present for file '" << img.metadata["StoreFullPathName"] << "'. Omitting it");
                continue;
            }
            partitions.insert( std::make_pair( img.metadata["dt"],
                                               img.metadata["StoreFullPathName"] ) );
        }
    } 
    for(const auto &apair : partitions){
        std::cout << apair.first << " " << apair.second << std::endl;
    } 

    return DICOM_data;
}
