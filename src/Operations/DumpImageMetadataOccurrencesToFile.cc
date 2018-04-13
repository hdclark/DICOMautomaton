//DumpImageMetadataOccurrencesToFile.cc - A part of DICOMautomaton 2015, 2016. Written by hal clark.

#include <cstdlib>            //Needed for exit() calls.
#include <fstream>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <string>    
#include <utility>            //Needed for std::pair.

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "DumpImageMetadataOccurrencesToFile.h"
#include "YgorFilesDirs.h"    //Needed for Does_File_Exist_And_Can_Be_Read(...), etc..
#include "YgorImages.h"
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorString.h"       //Needed for GetFirstRegex(...)


OperationDoc OpArgDocDumpImageMetadataOccurrencesToFile(void){
    OperationDoc out;
    out.name = "DumpImageMetadataOccurrencesToFile";
    out.desc = "";

    out.notes.emplace_back("");
    return out;
}

Drover DumpImageMetadataOccurrencesToFile(Drover DICOM_data, OperationArgPkg /*OptArgs*/, std::map<std::string,std::string> /*InvocationMetadata*/, std::string /*FilenameLex*/){

    //Dump all the metadata elements, but group like-items together and also print the occurence number.
    const auto Dump_Image_Metadata_Occurrences_To_File =
        [](const std::list<planar_image<float,double>> &images, const std::string &dumpfile) -> void {

            //Get a superset of all metadata names. Also bump the count for each metadata item.
            std::map<std::string, std::map<std::string,long int>> sset;
            for(const auto &img : images){
                for(const auto &md_pair : img.metadata){
                    ++(sset[md_pair.first][md_pair.second]);
                }
            }

            //Get the maximum unique map length.
            size_t maxm = 0;
            for(const auto &sspair : sset) if(sspair.second.size() > maxm) maxm = sspair.second.size();
     
            //Cycle through the images and print available tags.
            std::stringstream df;
            for(const auto &sspair : sset) df << sspair.first << "\tcount\t";
            df << std::endl;

            for(size_t i = 0; i < maxm; ++i){
                for(const auto &sspair : sset){
                    if(i < sspair.second.size()){
                        const auto pit = std::next(sspair.second.begin(),i);
                        df << pit->first << "\t" << pit->second << "\t";
                    }else{
                        df << "\t\t";
                    }
                }
                df << std::endl;
            }
            if(!OverwriteStringToFile(df.str(),dumpfile)) FUNCERR("Unable to dump ordered image metadata to file");
            return;
        };

    size_t i = 0;
    for(auto & img_array : DICOM_data.image_data){
        std::string fname = "/tmp/petct_analysis_img_array_metadata_occurences_"_s + Xtostring(i) + ".tsv";
        Dump_Image_Metadata_Occurrences_To_File(img_array->imagecoll.images, fname);
        ++i;
    }

    return DICOM_data;
}
