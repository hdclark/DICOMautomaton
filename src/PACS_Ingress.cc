//PACS_Ingress.h - DICOMautomaton 2015. Written by hal clark.
//
//This program is suitable for importing individual DICOM files into a PACS-like database.
// The modality and linkage is ignored for the purposes of ingress. Files can be properly
// linked, queried, and further examined after they have been imported.
//
// Note that, because this program essentially just distills files down to a collection of
// DICOM key-values, routines are tightly coupled with the DICOM parser. 
//

#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <utility>        //Needed for std::pair.
#include <memory>         //Needed for std::unique_ptr.
#include <algorithm>      //Needed for std::sort.
#include <list>
#include <tuple>

#include "Structs.h"
#include "Imebra_Shim.h"     //Wrapper for Imebra library. Black-boxed to speed up compilation.

#include <pqxx/pqxx> //PostgreSQL C++ interface.

#include "YgorMisc.h"           //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorString.h"         //Needed for stringtoX(), X_to_string().
#include "YgorTime.h"           //Needed for class time_mark.
#include "YgorArguments.h"
#include "YgorFilesDirs.h"

int main(int argc, char **argv){
    //std::string db_params("dbname=pacs user=hal host=localhost port=63443");
    std::string db_params("dbname=pacs user=hal host=localhost");
    const std::string DICOMFileSystemStoreBase("/home/pacs_file_store");
    std::string DICOMFile;  //The filename to use.
    std::string Project;    //Human-readable project of data origin. MSc, PhD, Special_Project_...
    std::string Comments;   //Human-readable general comments.
    std::string GDCMDump;   //Text of executing `gdcmdump` if available.

    //---------------------------------------------------------------------------------------------------------
    //------------------------------------------ Argument Handling --------------------------------------------
    //---------------------------------------------------------------------------------------------------------
    class ArgumentHandler arger;
    const std::string progname(argv[0]);
    //----
    arger.description = "Given a DICOM file and some additional metadata, insert the data    "
                        " into the PACs system database. The file itself will be copied into "
                        " the database and various bits of data will be deciphered.          ";

    arger.examples = { { "" ,
                         "Insert the file '/tmp/a' into the database." } 
    };
    //----

    arger.default_callback = [](int, const std::string &optarg) -> void {
        FUNCERR("Unrecognized option with argument: '" << optarg << "'");
    };
    arger.optionless_callback = [&](const std::string &optarg) -> void {
        if(!DICOMFile.empty()) FUNCERR("This program can only handle a single file at a time");
        DICOMFile = optarg;
        return;
    };
    //----

    arger.push_back( std::make_tuple(1, 'f', "dicom-file", true, "/tmp/a",
                                     "(req'd) The DICOM file to use.",
                                     [&](const std::string &optarg) -> void {
        if(!DICOMFile.empty()) FUNCERR("This program can only handle a single file at a time");
        DICOMFile = optarg;
        return;
    }));
    arger.push_back( std::make_tuple(2, 'p', "project", true, "MSc",
                                     "(req'd) Human-readable project of data origin.",
                                     [&](const std::string &optarg) -> void {
        Project = optarg;
        return;
    }));
    arger.push_back( std::make_tuple(2, 'c', "comments", true, "'First images collected in this project. Ended up not using.'",
                                     "(req'd) Human-readable project of data origin.",
                                     [&](const std::string &optarg) -> void {
        Comments = optarg;
        return;
    }));
    arger.push_back( std::make_tuple(1, 'g', "gdcmdump-file", true, "/tmp/a.dcm.gdcmdump",
                                     "File containing output from `gdcmdump`.",
                                     [&](const std::string &optarg) -> void {
        if(!Does_File_Exist_And_Can_Be_Read(optarg)) FUNCERR("Cannot read file '" << optarg << "'");
        GDCMDump = LoadFileToString(optarg);
        return;
    }));

    arger.Launch(argc, argv);

    //---------------------------------------------------------------------------------------------------------
    //--------------------------------------- Requirement Verification ----------------------------------------
    //---------------------------------------------------------------------------------------------------------
    if(DICOMFile.empty()) FUNCERR("Cannot read DICOM file '" << DICOMFile << "'. Cannot continue");
    if(Project.empty())   FUNCERR("The 'project' string is mandatory. Cannot continue");
    if(Comments.empty())  FUNCERR("The 'comments' string is mandatory. Cannot continue");
    if(GDCMDump.empty())  FUNCERR("The 'gdcmdump' string is strongly suggested. Refusing to continue");

    //---------------------------------------------------------------------------------------------------------
    //----------------------------------------- Data Loading & Prep -------------------------------------------
    //---------------------------------------------------------------------------------------------------------
    //Process the file.
    auto mmap = get_metadata_top_level_tags(DICOMFile);

    //Figure out a reasonable place to keep the file in the filesystem store. It isn't so important except to
    // be reasonably human-readable, fairly balanced in the filesystem, and not already present.
    const auto StudyInstanceUID  = mmap["StudyInstanceUID"];
    const auto StudyDate         = mmap["StudyDate"];
    const auto StudyTime         = mmap["StudyTime"];
    const auto SeriesInstanceUID = mmap["SeriesInstanceUID"];
    const auto SeriesNumber      = mmap["SeriesNumber"];
    const auto SOPInstanceUID    = mmap["SOPInstanceUID"];

    if(StudyInstanceUID.empty()  || StudyDate.empty()    || StudyTime.empty() 
    || SeriesInstanceUID.empty() || SeriesNumber.empty() || SOPInstanceUID.empty() ){
        FUNCERR("File is '" << DICOMFile << "' missing information and cannot be imported into the database");
    }

    const auto TopDirName = Detox_String(StudyDate) + "-"_s
                          + Detox_String(StudyTime) + "_"_s
                          + Detox_String(StudyInstanceUID);

    const auto MidDirName = Detox_String(SeriesNumber) + "-"_s
                          + Detox_String(SeriesInstanceUID);

    const auto NewFullDir = DICOMFileSystemStoreBase + "/"_s
                          + TopDirName + "/"_s
                          + MidDirName + "/";    //Not the full path, just the complete directory.

    const auto NewFileName = Detox_String(SOPInstanceUID) + ".dcm";
    const auto StoreFullPathName = NewFullDir + NewFileName;

    const auto NewGDCMDumpFileName = Detox_String(SOPInstanceUID) + ".gdcmdump";
    const auto StoreGDCMDumpFileName = NewFullDir + NewGDCMDumpFileName;

    //Ensure the destination location can be created and the file copied.
    if(!Does_Dir_Exist_And_Can_Be_Read(NewFullDir) && !Create_Dir_and_Necessary_Parents(NewFullDir)){
        FUNCERR("Unable to create directory '" << NewFullDir << "'. Cannot continue");
    }

    //if(!TouchFile(StoreFullPathName)){
    //    FUNCERR("Unable to touch file '" << StoreFullPathName << "'. Cannot continue");
    //}

    //Copy the file.
    if(!CopyFile(DICOMFile, StoreFullPathName)){
        FUNCERR("Unable to copy file '" << DICOMFile << "' to filesystem store destination '" << StoreFullPathName << "'");
    }

    //Write the GDCMDump file into the store.
    if(!WriteStringToFile(GDCMDump, StoreGDCMDumpFileName)){
        FUNCERR("Unable to write GDCMDump file '" << StoreGDCMDumpFileName << "' into the filesystem store");
    }

    //Set the permissions ...
    // ... TODO ...

    //---------------------------------------------------------------------------------------------------------
    //----------------------------------------- Database Registration -----------------------------------------
    //---------------------------------------------------------------------------------------------------------
    //Now we convert the data into a format the database can use to verify/cast into the proper format. In some
    // cases we cast to a REAL before an INT. This is because I've encountered INT fields printed in strings or
    // reported by Imebra as '16.0000' which PostgreSQL doesn't like. Casting to REAL and then INT is a 
    // logical workaround.

    try{
        pqxx::connection c(db_params);
        pqxx::work txn(c);
        std::stringstream tb1, tb2;

        //------------------------------------- Claim a new pacsid --------------------------------------------
        //Don't worry about iterating the nidus unnecessarily. There is plenty of room to skip ids, and we can
        // always squash holes at a later time (as required).
        tb1.str(""); //Clear stringstream.
        tb2.str(""); //Clear stringstream.
        tb1 << "INSERT INTO pacsid_nidus                          ";
        tb1 << "    (pacsid) VALUES (nextval('pacsid_nidus_seq')) ";
        tb1 << "RETURNING pacsid;                                 ";

        pqxx::result r = txn.exec(tb1.str());
        if(r.affected_rows() != 1) FUNCERR("Unable to create new pacsid. Cannot continue");
        const auto pacsid = r[0]["pacsid"].as<long int>(); 

        //------------------------------- Push the metadata to the database -----------------------------------
        tb1.str(""); //Clear stringstream.
        tb2.str(""); //Clear stringstream.
        tb1 << "INSERT INTO metadata ( ";

        tb1 << "    pacsid,                      ";        tb2 << pacsid << ",";

        //DICOM logical hierarchy fields.
        tb1 << "    PatientID,                   ";        tb2 << "NULLIF(" << txn.quote(mmap["PatientID"]) << ",''),";
        tb1 << "    StudyInstanceUID,            ";        tb2 << "NULLIF(" << txn.quote(mmap["StudyInstanceUID"]) << ",''),";
        tb1 << "    SeriesInstanceUID,           ";        tb2 << "NULLIF(" << txn.quote(mmap["SeriesInstanceUID"]) << ",''),";
        tb1 << "    SOPInstanceUID,              ";        tb2 << "NULLIF(" << txn.quote(mmap["SOPInstanceUID"]) << ",''),";

/*
        tb1 << "    StudyDate,                   ";        tb2 << "CAST( NULLIF(" << txn.quote(mmap["StudyDate"]) << ",'') AS DATE),";
        tb1 << "    StudyTime,                   ";        tb2 << "CAST( NULLIF(" << txn.quote(mmap["StudyTime"]) << ",'') AS TIME),";
        tb1 << "    SeriesNumber,                ";        tb2 << "CAST( CAST( NULLIF(" << txn.quote(mmap["SeriesNumber"]) << ",'') AS REAL) AS BIGINT),";
*/

        //Non-DICOM metadata fields.
        tb1 << "    Project,                     ";        tb2 << "NULLIF(" << txn.quote(Project) << ",''),";
        tb1 << "    Comments,                    ";        tb2 << "NULLIF(" << txn.quote(Comments) << ",''),";
        tb1 << "    FullPathName,                ";        tb2 << "NULLIF(" << txn.quote(Fully_Expand_Filename(DICOMFile)) << ",''),";
//        tb1 << "    gdcmdump,                    ";        tb2 << "NULLIF(" << txn.quote(GDCMDump) << ",''),";
        tb1 << "    ImportTimepoint,             ";        tb2 << "now(),";
        tb1 << "    StoreFullPathName            ";        tb2 << txn.quote(StoreFullPathName);

        tb1 << ") VALUES (" << tb2.str() << ");";

        r = txn.exec(tb1.str());
        if(r.affected_rows() != 1){

            //Remove filesystem store copied files! TODO FIXME.
            //Remove directory ... IFF nothing else is in it... TODO FIXME.
            // ...

            FUNCERR("Number of affected rows was " << r.affected_rows() << " != 1");
        }else{
            FUNCINFO("Success! PACS id=" << pacsid << " and StoreFullPathName='" << StoreFullPathName << "'");
        }
        txn.commit(); 

    }catch(const std::exception &e){
        FUNCERR("Unable to push to database:\n" << e.what() << "\n" << "Cannot continue");
    }

    return 0;
}
