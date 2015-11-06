//PACS_Verify_Filesystem_Store.cc - Run over all records, ensuring the file is still present in
// the filesystem store.
//

#include <iostream>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <string>    
#include <vector>
#include <set> 
#include <map>
#include <list>
#include <functional>
#include <thread>
#include <array>
#include <mutex>
#include <future>            //Needed for std::async(...)

#include <getopt.h>          //Needed for 'getopts' argument parsing.
#include <cstdlib>           //Needed for exit() calls.
#include <utility>           //Needed for std::pair.

#include <pqxx/pqxx>         //PostgreSQL C++ interface.

#include "Structs.h"
#include "Imebra_Shim.h"     //Wrapper for Imebra library. Black-boxed to speed up compilation.
#include "YgorMisc.h"        //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorMath.h"        //Needed for vec3 class.
#include "YgorFilesDirs.h"   //Needed for Does_File_Exist_And_Can_Be_Read(...), etc..
#include "YgorContainers.h"  //Needed for bimap class.
#include "YgorPerformance.h" //Needed for YgorPerformance_dt_from_last().
#include "YgorAlgorithms.h"  //Needed for For_Each_In_Parallel<..>(...)
#include "YgorArguments.h"   //Needed for ArgumentHandler class.
#include "YgorString.h"      //Needed for GetFirstRegex(...)
#include "Explicator.h"      //Needed for Explicator class.
#include "Demarcator.h"      //Needed for Demarcator class.

//#include "Distinguisher.h"   //Needed for Distinguisher class.            //Needed?

#include "YgorDICOMTools.h"  //Needed for Is_File_A_DICOM_File(...);

//Globals. Libimebrashim expects these to be defined.
bool VERBOSE = true;  //Provides additional information.
bool QUIET   = false;  //Suppresses ALL information. Not recommended!


int main(int argc, char* argv[]){
//---------------------------------------------------------------------------------------------------------------------
//------------------------------------------- Instances used throughout -----------------------------------------------
//---------------------------------------------------------------------------------------------------------------------
    //std::string db_params("dbname=pacs user=hal host=localhost port=63443");
    std::string db_params("dbname=pacs user=hal host=localhost port=5432");

    std::string DICOMFileSystemStoreBase("/home/pacs_file_store/");


//---------------------------------------------------------------------------------------------------------------------
//------------------------------------------------ Option parsing -----------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------


    class ArgumentHandler arger;
    const std::string progname(argv[0]);
    arger.examples = { { "--help" , "Show the help screen and some info about the program." } };
    arger.description = "A program for trying to replace database NULLs, if possible.";

    arger.default_callback = [](int, const std::string &optarg) -> void {
      FUNCERR("Unrecognized option with argument: '" << optarg << "'");
      return; 
    };
    arger.optionless_callback = [](const std::string &optarg) -> void {
      FUNCERR("What do you want me to do with the option '" << optarg << "' ?");
      return; 
    };

    arger.push_back( ygor_arg_handlr_t(2, 'f', "filesystem-store-base", true, DICOMFileSystemStoreBase, 
      "The base directory to use as the filesystem store.",
      [&](const std::string &optarg) -> void {
        DICOMFileSystemStoreBase = optarg;
        return;
      })
    );

    arger.Launch(argc, argv);

//---------------------------------------------------------------------------------------------------------------------
//------------------------------------------------ Input Verification ---------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------
    if(DICOMFileSystemStoreBase.empty()){
        FUNCERR("Filesystem store base directory not provided. Cannot proceed");
    }

//---------------------------------------------------------------------------------------------------------------------
//----------------------------------------------- Database Initiation -------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------

    const auto limit = 1000;
    long int offset = 0;
    while(true){
        offset += limit;
        FUNCINFO("Bunch: limit = " << limit << " and offset = " << offset);

        try{
            pqxx::connection c(db_params);
            pqxx::work txn(c);
            std::stringstream tb1, tb2;
    
            //-------------------------------------------------------------------------------------------------------------
            //Select all records from the system pacs database.
            std::stringstream q1;
            q1 << "SELECT * FROM metadata ";
            //q1 << "SELECT pacsid, PatientID, StudyInstanceUID, SeriesInstanceUID, SOPInstanceUID, StudyDate, StudyTime, SeriesNumber, FullPathName FROM metadata ";
            q1 << "ORDER BY PatientID, StudyInstanceUID, SeriesInstanceUID, SOPInstanceUID ";
            q1 << "LIMIT " << limit << " OFFSET " << offset << " ;";
            pqxx::result r1 = txn.exec(q1.str());
            if(r1.empty()) FUNCERR("Query returned no records. Nothing to do");
            FUNCINFO("Found " << r1.size() << " records to verify");
    
            //-------------------------------------------------------------------------------------------------------------
            //Process each record, parsing the file, saving metadata, and walking over the columns to see if they are null.
            for(pqxx::result::size_type i = 0; i != r1.size(); ++i){
                //Get the returned pacsid.
                const auto pacsid = r1[i]["pacsid"].as<long int>();
                const auto StoreFullPathName = r1[i]["StoreFullPathName"].as<std::string>();
                
                //Compute the md5 and compare it with the db md5.


/* 
                //Grab the hierarchial elements needed to store the file.
                const auto StudyInstanceUID = r1[i]["StudyInstanceUID"].as<std::string>();
                const auto StudyDate = r1[i]["StudyDate"].as<std::string>();
                const auto StudyTime = r1[i]["StudyTime"].as<std::string>();
                const auto SeriesInstanceUID = r1[i]["SeriesInstanceUID"].as<std::string>();
                const auto SeriesNumber = r1[i]["SeriesNumber"].as<std::string>();
                const auto SOPInstanceUID = r1[i]["SOPInstanceUID"].as<std::string>();
    
                const auto TopDirName = Detox_String(StudyDate) + "-"_s 
                                      + Detox_String(StudyTime) + "_"_s
                                      + Detox_String(StudyInstanceUID);
    
                const auto MidDirName = Detox_String(SeriesNumber) + "-"_s
                                      + Detox_String(SeriesInstanceUID);
              
                const auto NewFullDir = DICOMFileSystemStoreBase + "/"_s  
                                       + TopDirName + "/"_s
                                       + MidDirName + "/";    //Not the full path, just the complete directory.
    
                const auto NewFileName = Detox_String(SOPInstanceUID) + ".dcm";
                const auto NewFullPath = NewFullDir + NewFileName;
    
    
                FUNCINFO("Destination for '" << r1[i]["FullPathName"].as<std::string>() << "' will be \n\t\t\t '" << NewFullPath << "'");
    
                if(!Does_Dir_Exist_And_Can_Be_Read(NewFullDir) && !Create_Dir_and_Necessary_Parents(NewFullDir)){
                    FUNCERR("Unable to create directory '" << NewFullDir << "'. Cannot continue");
                }
    
                //if(!TouchFile(NewFullPath)){
                //    FUNCERR("Unable to touch file '" << NewFullPath << "'. Cannot continue");
                //}
   
                //Compute an md5 checksum for the file.
                // ... do this later ...
    
                //Add the new full path location to the table.
                {
                  std::stringstream q;
                  q << " UPDATE metadata ";
                  q << " SET (StoreFullPathName) = (" << txn.quote(NewFullPath) << ") ";
                  q << " WHERE pacsid = " << pacsid << " ";
                  q << " RETURNING pacsid; ";
                  pqxx::result res = txn.exec(q.str());
                  if(res.size() != 1) FUNCERR("Database insert '\n" << q.str() << "\n' unsuccessful. Cannot continue");
                }

                //Remove the largeobject from the database.
                oidwrapper.remove(txn);
    
                //NULL the oid reference in the table so that we don't continue using it.
                { 
                  std::stringstream q;
                  q << " UPDATE rawfiles ";
                  q << " SET (fileoid) = (NULL) ";
                  q << " WHERE pacsid = " << pacsid << " ";
                  q << " RETURNING pacsid; ";
                  pqxx::result res = txn.exec(q.str());
                  if(res.size() != 1) FUNCERR("Database insert '\n" << q.str() << "\n' unsuccessful. Cannot continue");
                }
*/        
                //////////////////////////////////////////////////////////////////////
    
                //---------------------------------------------------------------------------------------------------------
            }
    
            //-------------------------------------------------------------------------------------------------------------
            //Finish the transaction and drop the connection.
            txn.commit();
    
        }catch(const std::exception &e){
            FUNCERR("Unable to push to database: " << e.what());
        }

    }//For loop.


//---------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------- Cleanup --------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------
    //Attempt to delete the DICOMFileSystemStoreBase if it exists.
    // Probably safest NOT to attempt to delete it until I have a safer interface (std::filesystem).
    return 0;
}
