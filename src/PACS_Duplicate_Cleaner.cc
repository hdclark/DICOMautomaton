//PACS_Duplicate_Cleaner.h - DICOMautomaton 2017. Written by hal clark.
//
//This program de-duplicates DICOM files that are already in the PACS DB, deleting them.
//
// Note: A full, exact byte-wise comparison is not performed. Rather, the DICOM tags that
//       are required to be unique are compared against the DB. If a match is found the file
//       is deleted. Be careful not to run this on the PACS DB itself, since the DB files 
//       will be deleted! (This is not checked because the PACS DB might be mounted in some
//       exotic way that will confuse such efforts, such as sshfs.)
//
// Note: The file is NOT ingressed if it is not yet in the PACS DB.
//

#include <exception>
#include <functional>
#include <iostream>
#include <list>
#include <map>
#include <pqxx/pqxx>            //PostgreSQL C++ interface.
#include <string>
#include <tuple>

#include "Imebra_Shim.h"        //Wrapper for Imebra library. Black-boxed to speed up compilation.
#include "YgorArguments.h"
#include "YgorFilesDirs.h"
#include "YgorMisc.h"           //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.

int main(int argc, char **argv){
    //std::string db_params("dbname=pacs user=hal host=localhost port=63443");
    std::string db_params("dbname=pacs user=hal host=localhost");
    const std::string DICOMFileSystemStoreBase("/home/pacs_store");
    std::string DICOMFile;  //The filename to use.
    bool dryrun = false;    //Do not actually insert the file into the db, just test for errors.
    bool verbose = false;   //Print extra information. Normally successful info is suppresed.

    //---------------------------------------------------------------------------------------------------------
    //------------------------------------------ Argument Handling --------------------------------------------
    //---------------------------------------------------------------------------------------------------------
    class ArgumentHandler arger;
    const std::string progname(argv[0]);
    //----
    arger.description = "Given a DICOM file, check if it is in the PACS DB. If so, delete the file."
                        " Note that a full, byte-by-byte comparison is NOT performed -- rather only the top-level"
                        " DICOM unique identifiers are (currently) compared. No other metadata is considered."
                        " So this program is not suitable if DICOM files have been modified without re-assigning"
                        " unique identifiers! (Which is non-standard behaviour.) Note that if an /exact/ comparison"
                        " is desired, using a traditional file de-duplicator will work.";

    arger.examples = { { " -f '/path/to/a/dicom/file.dcm'" ,
                         "Check if 'file.dcm' is already in the PACS DB. If so, delete it ('file.dcm')." },
                       { " -f '/path/to/a/dicom/file.dcm' -n " ,
                         "Check if 'file.dcm' is already in the PACS DB, but do not delete anything." }
    };
    //----

    arger.default_callback = [](int, const std::string &optarg) -> void {
        FUNCERR("Unrecognized option with argument: '" << optarg << "'");
    };
    arger.optionless_callback = [&](const std::string &optarg) -> void {
        if(!DICOMFile.empty()){
            FUNCERR("This program can only handle a single file at a time."
                    " Earlier file: '" << DICOMFile << "'. This file: '" << optarg << "'");
        }
        DICOMFile = optarg;
        return;
    };
    //----

    arger.push_back( std::make_tuple(1, 'v', "verbose", false, "",
                                     "Print extra information.",
                                     [&](const std::string &optarg) -> void {
        verbose = true;
        return;
    }));
    arger.push_back( std::make_tuple(2, 'f', "dicom-file", true, "afile.dcm",
                                     "(req'd) The DICOM file to use.",
                                     [&](const std::string &optarg) -> void {
        if(!DICOMFile.empty()){
            FUNCERR("This program can only handle a single file at a time."
                    " Earlier file: '" << DICOMFile << "'. This file: '" << optarg << "'");
        }
        DICOMFile = optarg;
        return;
    }));
    arger.push_back( std::make_tuple(2, 'n', "dry-run", false, "",
                                     "Do not delete anything -- just report if a file is present in the PACS DB.",
                                     [&](const std::string &optarg) -> void {
        dryrun = true;
        return;
    }));

    arger.Launch(argc, argv);

    //---------------------------------------------------------------------------------------------------------
    //--------------------------------------- Requirement Verification ----------------------------------------
    //---------------------------------------------------------------------------------------------------------
    if(DICOMFile.empty()) FUNCERR("Cannot read DICOM file '" << DICOMFile << "'. Cannot continue");

    //---------------------------------------------------------------------------------------------------------
    //----------------------------------------- Data Loading & Prep -------------------------------------------
    //---------------------------------------------------------------------------------------------------------
    //Process the file.
    auto mmap = get_metadata_top_level_tags(DICOMFile);

    //Basic information check.
    {
        const auto PatientID         = mmap["PatientID"];
        const auto StudyInstanceUID  = mmap["StudyInstanceUID"];
        const auto StudyDate         = mmap["StudyDate"];
        const auto StudyTime         = mmap["StudyTime"];
        const auto SeriesInstanceUID = mmap["SeriesInstanceUID"];
        const auto SeriesNumber      = mmap["SeriesNumber"];
        const auto SOPInstanceUID    = mmap["SOPInstanceUID"];

        if(PatientID.empty() || StudyInstanceUID.empty()  || StudyDate.empty()    || StudyTime.empty() 
        || SeriesInstanceUID.empty() || SeriesNumber.empty() || SOPInstanceUID.empty() ){
            FUNCERR("File '" << DICOMFile << "' is absent, missing information, or not a DICOM file.");
        }
    }

    //---------------------------------------------------------------------------------------------------------
    //------------------------------------------- Database Querying -------------------------------------------
    //---------------------------------------------------------------------------------------------------------
    //Query the DB using the uniquely-identifying DICOM information.

    try{
        pqxx::connection c(db_params);
        pqxx::work txn(c);
        std::stringstream tb1, tb2;
        pqxx::result r;

        //----------------------------- Determine if a record already exists ----------------------------------

        tb1.str(""); //Clear stringstream.
        tb2.str(""); //Clear stringstream.
        tb1 << "SELECT StoreFullPathName FROM metadata WHERE ( ";
        tb1 << "       ( PatientID         = " << txn.quote(mmap["PatientID"])         << " ) ";
        tb1 << "   AND ( StudyInstanceUID  = " << txn.quote(mmap["StudyInstanceUID"])  << " ) ";
        tb1 << "   AND ( SeriesInstanceUID = " << txn.quote(mmap["SeriesInstanceUID"]) << " ) ";
        tb1 << "   AND ( SOPInstanceUID    = " << txn.quote(mmap["SOPInstanceUID"])    << " ) ";
        tb1 << " );";

        r = txn.exec(tb1.str());
        if(r.empty()){
            if(verbose) FUNCINFO("File '" << DICOMFile << "' is NOT in the DB");
            return 0;
        }

        //---------------------------------- Ensure existing file is accessible ----------------------------------
        //Also ensure that the basic DICOM unique identifiers match the DB record.

        if(r.size() != 1){
            FUNCERR("Multiple StoreFullPathName found for file '" << DICOMFile << "'. There should be 0 or 1");
        }
        const auto StoreFullPathName = (r[0]["StoreFullPathName"].is_null()) ? "" :
                                                                               r[0]["StoreFullPathName"].as<std::string>();

        auto pmmap = get_metadata_top_level_tags(StoreFullPathName);
        if((mmap["PatientID"]         != pmmap["PatientID"])
        || (mmap["StudyInstanceUID"]  != pmmap["StudyInstanceUID"])
        || (mmap["SeriesInstanceUID"] != pmmap["SeriesInstanceUID"])
        || (mmap["SOPInstanceUID"]    != pmmap["SOPInstanceUID"]) ){
            FUNCERR("PACS DB file '" << StoreFullPathName << "' does not match the DB record! Aborting");
        }

        //---------------------------------- Ensure existing file is accessible ----------------------------------

        if(dryrun){
            FUNCINFO("File '" << DICOMFile << "' is a duplicate (not removed due to dry-run)");
        }else{
            //Remove the file.
            if(RemoveFile(DICOMFile)){
                if(verbose) FUNCINFO("Deleted file '" << DICOMFile << "' which duplicated PACS DB file '" << StoreFullPathName << "'");
            }else{
                FUNCERR("Unable to delete file '" << DICOMFile << "' which duplicates PACS DB file '" << StoreFullPathName << "'");
            }
        }

    }catch(const std::exception &e){
        FUNCERR("Unable to query database:\n" << e.what() << "\nCannot continue");
    }

    return 0;
}
