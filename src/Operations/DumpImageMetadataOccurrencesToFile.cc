//DumpImageMetadataOccurrencesToFile.cc - A part of DICOMautomaton 2015, 2016. Written by hal clark.

#include <cstdlib>            //Needed for exit() calls.
#include <fstream>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <string>    
#include <utility>            //Needed for std::pair.

#include <boost/interprocess/creation_tags.hpp>
#include <boost/interprocess/sync/named_mutex.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "DumpImageMetadataOccurrencesToFile.h"
#include "YgorFilesDirs.h"    //Needed for Does_File_Exist_And_Can_Be_Read(...), etc..
#include "YgorImages.h"
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorString.h"       //Needed for GetFirstRegex(...)


OperationDoc OpArgDocDumpImageMetadataOccurrencesToFile(){
    OperationDoc out;
    out.name = "DumpImageMetadataOccurrencesToFile";

    out.desc = 
        "Dump all the metadata elements, but group like-items together and also print the occurence number.";

    out.args.emplace_back();
    out.args.back() = IAWhitelistOpArgDoc();
    out.args.back().name = "ImageSelection";
    out.args.back().default_val = "last";


    out.args.emplace_back();
    out.args.back().name = "FileName";
    out.args.back().desc = "A filename (or full path) in which to append metadata reported by this routine."
                           " The format is tab-separated values (TSV)."
                           " Leave empty to dump to generate a unique temporary file.";
    out.args.back().default_val = "";
    out.args.back().expected = true;
    out.args.back().examples = { "", "/tmp/somefile", "localfile.tsv", "derivative_data.tsv" };
    out.args.back().mimetype = "text/tsv";


    out.args.emplace_back();
    out.args.back().name = "UserComment";
    out.args.back().desc = "A string that will be inserted into the output file which will simplify merging output"
                           " with differing parameters, from different sources, or using sub-selections of the data."
                           " If left empty, the column will be empty in the output.";
    out.args.back().default_val = "";
    out.args.back().expected = true;
    out.args.back().examples = { "", "Using XYZ", "Patient treatment plan C" };


    return out;
}

bool DumpImageMetadataOccurrencesToFile(Drover &DICOM_data,
                                          const OperationArgPkg& OptArgs,
                                          const std::map<std::string, std::string>& /*InvocationMetadata*/,
                                          const std::string& /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();

    auto FileName = OptArgs.getValueStr("FileName").value();

    const auto UserComment = OptArgs.getValueStr("UserComment");
    //-----------------------------------------------------------------------------------------------------------------

    //A superset of all metadata names and also an occurence tally.
    std::map<std::string, std::map<std::string,long int>> sset; // [metadata_name][metadata_value] = count.

    auto IAs_all = All_IAs( DICOM_data );
    auto IAs = Whitelist( IAs_all, ImageSelectionStr );
    for(auto & iap_it : IAs){

        for(const auto &animg : (*iap_it)->imagecoll.images){
            for(const auto &md_pair : animg.metadata){
                ++(sset[md_pair.first][md_pair.second]);
            }
        }
    }

    //Report the findings. 
    FUNCINFO("Attempting to claim a mutex");
    try{
        //File-based locking is used so this program can be run over many patients concurrently.
        //
        //Try open a named mutex. Probably created in /dev/shm/ if you need to clear it manually...
        boost::interprocess::named_mutex mutex(boost::interprocess::open_or_create,
                                               "dicomautomaton_operation_dumpimagemetadataoccurrencestofile_mutex");
        boost::interprocess::scoped_lock<boost::interprocess::named_mutex> lock(mutex);

        if(FileName.empty()){
            FileName = Get_Unique_Sequential_Filename("/tmp/dicomautomaton_dumpimagemetadataoccurrencestofile_", 6, ".tsv");
        }
        const auto FirstWrite = !Does_File_Exist_And_Can_Be_Read(FileName);
        std::fstream FO(FileName, std::fstream::out | std::fstream::app);
        if(!FO){
            throw std::runtime_error("Unable to open file for reporting results. Cannot continue.");
        }

        if(FirstWrite){ // Write a TSV header.
            FO << "UserComment\t"
               //<< "PatientID\t"
               << "MetadataKey\t"
               << "MetadataValue\t"
               << "Occurrence"
               << std::endl;
        }

        for(const auto &sspair : sset){
            const auto key = sspair.first;
            for(const auto &vc_pair : sspair.second){
                const auto value = vc_pair.first;
                const auto count = vc_pair.second;

                FO << UserComment.value_or("") << "\t"
                   //<< patient_ID           << "\t"
                   << key                  << "\t"
                   << value                << "\t"
                   << count                << std::endl;
            }
        }
        FO.flush();
        FO.close();

    }catch(const std::exception &e){
        FUNCERR("Unable to write to output file: '" << e.what() << "'");
    }

    return true;
}
