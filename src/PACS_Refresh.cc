//PACS_Refresh.cc - Re-run over all records, trying to fill any NULLs encountered.
//
// This program is designed to update the database whenever the table structure has
// been tweaked.
//

#ifdef DCMA_USE_POSTGRES
#else
    #error "Attempted to compile without PostgreSQL support, which is required."
#endif

#include <cmath>
#include <exception>
#include <fstream>
#include <functional>
#include <list>
#include <map>
#include <pqxx/pqxx>         //PostgreSQL C++ interface.
#include <string>    

#include "Imebra_Shim.h"     //Wrapper for Imebra library. Black-boxed to speed up compilation.
#include "YgorArguments.h"   //Needed for ArgumentHandler class.
#include "YgorMisc.h"        //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorLog.h"
#include "YgorString.h"      //Needed for GetFirstRegex(...)

int main(int argc, char* argv[]){
//---------------------------------------------------------------------------------------------------------------------
//------------------------------------------- Instances used throughout -----------------------------------------------
//---------------------------------------------------------------------------------------------------------------------
    //std::string db_params("dbname=pacs user=hal host=localhost port=63443");
    std::string db_params("dbname=pacs user=hal host=localhost port=5432");

    long int NumberOfDaysRecent = 7; //Only update records imported within the specified days.

//---------------------------------------------------------------------------------------------------------------------
//------------------------------------------------ Option parsing -----------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------


    class ArgumentHandler arger;
    const std::string progname(argv[0]);
    arger.examples = { { "--help" , "Show the help screen and some info about the program." } };
    arger.description = "A program for trying to replace database NULLs, if possible.";

    arger.default_callback = [](int, const std::string &optarg) -> void {
      YLOGERR("Unrecognized option with argument: '" << optarg << "'");
      return; 
    };
    arger.optionless_callback = [](const std::string &optarg) -> void {
      YLOGERR("What do you want me to do with the option '" << optarg << "' ?");
      return; 
    };

    arger.push_back( ygor_arg_handlr_t(1, 'd', "days-back", true, Xtostring(NumberOfDaysRecent), 
      "The number of days back for which the import was considered 'recent'. (Only recent records are updated.)",
      [&](const std::string &optarg) -> void {
        if(!Is_String_An_X<long int>(optarg)) YLOGERR("'" << optarg << "' is not a valid number of days");
        NumberOfDaysRecent = fabs(stringtoX<long int>(optarg));
        return;
      })
    );

    arger.Launch(argc, argv);

//---------------------------------------------------------------------------------------------------------------------
//------------------------------------------------ Input Verification ---------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------

//---------------------------------------------------------------------------------------------------------------------
//----------------------------------------------- Filename Testing ----------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------

//---------------------------------------------------------------------------------------------------------------------
//----------------------------------------------- Database Initiation -------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------
 
    try{
        pqxx::connection c(db_params);
        pqxx::work txn(c);
        std::stringstream tb1, tb2;

        //-------------------------------------------------------------------------------------------------------------
        //Select records from the system pacs database.
        
        pqxx::result r1;
        {
          std::stringstream ss;
          ss << "SELECT * FROM metadata "
             << "WHERE (metadata.ImportTimepoint > (now() - INTERVAL '" << NumberOfDaysRecent << " days'));"; 
          r1 = txn.exec(ss.str());
        }
        if(r1.empty()) YLOGERR("Database table 'metadata' contains no records. Nothing to do");
        YLOGINFO("Found " << r1.size() << " records to inspect");

        //-------------------------------------------------------------------------------------------------------------
        //Process each record, parsing the file, saving metadata, and walking over the columns to see if they are null.
        for(pqxx::result::size_type i = 0; i != r1.size(); ++i){
            //Get the returned pacsid.
            const auto pacsid = r1[i]["pacsid"].as<long int>();

            //Get the store filename and check if the file has been validated.
            const auto storefullpathname = r1[i]["StoreFullPathName"].as<std::string>();

            // TODO: if checksum non-NULL, verify it is correct. Fail with lots of info if not correct!
            //       if checksum is NULL, compute it and update the db.

            //Print to screen what the original filename was, so the user can inspect what files are being processed.
            YLOGINFO("About to parse file with pacsid = " << pacsid << " at location '" << storefullpathname << "'");
            YLOGINFO("Completion: " << i << "/" << r1.size() << " == " << static_cast<double>(10000*i/r1.size())/100.0 << "%");

            //Harvest the metadata of interest.
            auto mmap = get_metadata_top_level_tags(storefullpathname);

            //Generate some conveniences for later.
            pqxx::result r3;
            const auto update_boilerplate = [&](const std::string &colname, const std::string &value) -> std::string {
                std::stringstream ss;
                ss << " UPDATE metadata ";
                ss << " SET (" << colname << ") ";
                ss << " = (COALESCE(" << colname << "," << value << ")) ";  //Avoid changing if something is already present.
                ss << " WHERE pacsid = " << pacsid << " ";
                ss << " RETURNING pacsid; ";
                return ss.str();
            };
            const auto check_update_ok = [&](const std::string &colname) -> void {
                if((r3.size() != 1) || (r3[0]["pacsid"].as<long int>() != pacsid)){
                    YLOGERR("Update of column name '" << colname << "' failed. Refusing to continue");
                }
                return;
            };
            const auto null_if_empty_str = [](const std::string &value) -> std::string {
                std::stringstream ss;
                ss << " NULLIF( " << value << " ,'') ";
                return ss.str();
            };
            const auto null_if_given_str = [](const std::string &value, const std::string &given) -> std::string {
                std::stringstream ss;
                ss << " NULLIF( " << value << " , " << given << " ) ";
                return ss.str();
            };
            const auto cast_to_bigint = [](const std::string &value) -> std::string {
                std::stringstream ss;
                ss << " CAST( " << value << " AS BIGINT) ";
                return ss.str();
            };
            const auto cast_to_int = [](const std::string &value) -> std::string {
                std::stringstream ss;
                ss << " CAST( " << value << " AS INT) ";
                return ss.str();
            };
            const auto cast_to_real = [](const std::string &value) -> std::string {
                std::stringstream ss;
                ss << " CAST( " << value << " AS REAL) ";
                return ss.str();
            };
            const auto cast_to_double = [](const std::string &value) -> std::string {
                std::stringstream ss;
                ss << " CAST( " << value << " AS DOUBLE PRECISION) ";
                return ss.str();
            };
            const auto cast_to_date = [](const std::string &value) -> std::string {
                std::stringstream ss;
                ss << " CAST( " << value << " AS DATE) ";
                return ss.str();
            };
            const auto cast_to_time = [](const std::string &value) -> std::string {
                std::stringstream ss;
                ss << " CAST( " << value << " AS TIME) ";
                return ss.str();
            };
            const auto dicom_string_to_real_array = [&](const std::string &values) -> std::string {
                auto tokens = SplitStringToVector(values, '\\', 'd');
                bool first = true;
                std::stringstream ss;
                ss << " CAST( ARRAY[ ";
                for(auto &x : tokens){
                    if(!first) ss << " , ";
                    first = false;
                    ss << cast_to_real( null_if_empty_str( txn.quote(x) ) );
                }
                ss << " ] AS REAL[]) ";
                return ss.str();
            };
            const auto dicom_string_to_double_array = [&](const std::string &values) -> std::string {
                auto tokens = SplitStringToVector(values, '\\', 'd');
                bool first = true;
                std::stringstream ss;
                ss << " CAST( ARRAY[ ";
                for(auto &x : tokens){
                    if(!first) ss << " , ";
                    first = false;
                    ss << cast_to_real( null_if_empty_str( txn.quote(x) ) );
                }
                ss << " ] AS DOUBLE PRECISION[]) ";
                return ss.str();
            };
            const auto dicom_string_to_int_array = [&](const std::string &values) -> std::string {
                auto tokens = SplitStringToVector(values, '\\', 'd');
                bool first = true;
                std::stringstream ss;
                ss << " CAST( ARRAY[ ";
                for(auto &x : tokens){
                    if(!first) ss << " , ";
                    first = false;
                    ss << cast_to_int( cast_to_real( null_if_empty_str( txn.quote(x) ) ) );
                }
                ss << " ] AS INT[]) ";
                return ss.str();
            };
            std::string colname, value;

            //---------------------------------------------------------------------------------------------------------
            //Push the data to the database.

            //skipping "pacsid". (If it is NULL we could not have found it!)

            //DICOM logical hierarchy fields.
            colname = "PatientID";
            value   = null_if_empty_str( txn.quote(mmap[colname]) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "StudyInstanceUID";
            value   = null_if_empty_str( txn.quote(mmap[colname]) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "SeriesInstanceUID";
            value   = null_if_empty_str( txn.quote(mmap[colname]) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "SOPInstanceUID";
            value   = null_if_empty_str( txn.quote(mmap[colname]) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            //DICOM data collection, additional or fallback linkage metadata.
            colname = "InstanceNumber";
            value   = cast_to_bigint( null_if_empty_str( txn.quote(mmap[colname]) ) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "InstanceCreationDate";
            value   = cast_to_date( null_if_given_str( null_if_empty_str( txn.quote(mmap[colname]) ), txn.quote("0000-00-00")) );
            //value   = cast_to_date( null_if_empty_str( txn.quote(mmap[colname]) ) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "InstanceCreationTime";
            value   = cast_to_time( null_if_empty_str( txn.quote(mmap[colname]) ) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "StudyDate";
            value   = cast_to_date( null_if_given_str( null_if_empty_str( txn.quote(mmap[colname]) ), txn.quote("0000-00-00")) );
            //value   = cast_to_date( null_if_empty_str( txn.quote(mmap[colname]) ) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "StudyTime";
            value   = cast_to_time( null_if_empty_str( txn.quote(mmap[colname]) ) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "StudyID";
            value   = null_if_empty_str( txn.quote(mmap[colname]) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "StudyDescription";
            value   = null_if_empty_str( txn.quote(mmap[colname]) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "SeriesDate";
            value   = cast_to_date( null_if_given_str( null_if_empty_str( txn.quote(mmap[colname]) ), txn.quote("0000-00-00")) );
            //value   = cast_to_date( null_if_empty_str( txn.quote(mmap[colname]) ) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "SeriesTime";
            value   = cast_to_time( null_if_empty_str( txn.quote(mmap[colname]) ) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "SeriesNumber";
            value   = cast_to_bigint( cast_to_real( null_if_empty_str( txn.quote(mmap[colname]) ) ) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "SeriesDescription";
            value   = null_if_empty_str( txn.quote(mmap[colname]) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "AcquisitionDate";
            value   = cast_to_date( null_if_given_str( null_if_empty_str( txn.quote(mmap[colname]) ), txn.quote("0000-00-00")) );
            //value   = cast_to_date( null_if_empty_str( txn.quote(mmap[colname]) ) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "AcquisitionTime";
            value   = cast_to_time( null_if_empty_str( txn.quote(mmap[colname]) ) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "AcquisitionNumber";
            value   = cast_to_bigint( cast_to_real( null_if_empty_str( txn.quote(mmap[colname]) ) ) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "ContentDate";
            value   = cast_to_date( null_if_given_str( null_if_empty_str( txn.quote(mmap[colname]) ), txn.quote("0000-00-00")) );
            //value   = cast_to_date( null_if_empty_str( txn.quote(mmap[colname]) ) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "ContentTime";
            value   = cast_to_time( null_if_empty_str( txn.quote(mmap[colname]) ) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "BodyPartExamined";
            value   = null_if_empty_str( txn.quote(mmap[colname]) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "ScanningSequence";
            value   = null_if_empty_str( txn.quote(mmap[colname]) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "SequenceVariant";
            value   = null_if_empty_str( txn.quote(mmap[colname]) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "ScanOptions";
            value   = null_if_empty_str( txn.quote(mmap[colname]) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "MRAcquisitionType";
            value   = null_if_empty_str( txn.quote(mmap[colname]) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            //DICOM image, dose map specifications and metadata.
            colname = "SliceThickness";
            value   = cast_to_real( null_if_empty_str( txn.quote(mmap[colname]) ) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "SliceNumber";
            value   = cast_to_bigint( cast_to_real( null_if_empty_str( txn.quote(mmap[colname]) ) ) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "SliceLocation";
            value   = cast_to_real( null_if_given_str( null_if_empty_str( txn.quote(mmap[colname]) ) , txn.quote("0") ) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "ImageIndex";
            value   = cast_to_bigint( cast_to_real( null_if_empty_str( txn.quote(mmap[colname]) ) ) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "SpacingBetweenSlices";
            value   = cast_to_real( null_if_empty_str( txn.quote(mmap[colname]) ) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "ImagePositionPatient";
            value   = dicom_string_to_real_array( mmap[colname] );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "ImageOrientationPatient";
            value   = dicom_string_to_real_array( mmap[colname] );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "FrameOfReferenceUID";
            value   = null_if_empty_str( txn.quote(mmap[colname]) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "PositionReferenceIndicator";
            value   = null_if_empty_str( txn.quote(mmap[colname]) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "SamplesPerPixel";
            value   = cast_to_int( cast_to_real( null_if_empty_str( txn.quote(mmap[colname]) ) ) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "PhotometricInterpretation";
            value   = null_if_empty_str( txn.quote(mmap[colname]) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "NumberofFrames";
            value   = cast_to_int( cast_to_real( null_if_empty_str( txn.quote(mmap[colname]) ) ) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "FrameIncrementPointer";
            value   = dicom_string_to_int_array( mmap[colname] );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);


            colname = "Rows";
            value   = cast_to_int( cast_to_real( null_if_empty_str( txn.quote(mmap[colname]) ) ) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "Columns";
            value   = cast_to_int( cast_to_real( null_if_empty_str( txn.quote(mmap[colname]) ) ) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "PixelSpacing";
            value   = dicom_string_to_real_array( mmap[colname] );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "BitsAllocated";
            value   = cast_to_int( cast_to_real( null_if_empty_str( txn.quote(mmap[colname]) ) ) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "BitsStored";
            value   = cast_to_int( cast_to_real( null_if_empty_str( txn.quote(mmap[colname]) ) ) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "HighBit";
            value   = cast_to_int( cast_to_real( null_if_empty_str( txn.quote(mmap[colname]) ) ) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "PixelRepresentation";
            value   = cast_to_int( cast_to_real( null_if_empty_str( txn.quote(mmap[colname]) ) ) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "DoseUnits";
            value   = null_if_empty_str( txn.quote(mmap[colname]) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "DoseType";
            value   = null_if_empty_str( txn.quote(mmap[colname]) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "DoseSummationType";
            value   = null_if_empty_str( txn.quote(mmap[colname]) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "DoseGridScaling";
            value   = cast_to_real( null_if_empty_str( txn.quote(mmap[colname]) ) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "GridFrameOffsetVector";
            value   = dicom_string_to_real_array( mmap[colname] );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "TemporalPositionIdentifier";
            value   = cast_to_int( cast_to_real( null_if_empty_str( txn.quote(mmap[colname]) ) ) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "NumberofTemporalPositions";
            value   = cast_to_int( cast_to_real( null_if_empty_str( txn.quote(mmap[colname]) ) ) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "TemporalResolution";
            value   = cast_to_real( null_if_empty_str( txn.quote(mmap[colname]) ) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);


            colname = "TemporalPositionIndex";
            value   = cast_to_int( cast_to_real( null_if_empty_str( txn.quote(mmap[colname]) ) ) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "FrameReferenceTime"; //Not a true time. Integer number of msec.
            value   = cast_to_bigint( cast_to_real( null_if_given_str( null_if_empty_str( txn.quote(mmap[colname]) ), txn.quote("0") ) ) );
            //value   = cast_to_bigint( cast_to_real( null_if_empty_str( txn.quote(mmap[colname]) ) ) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "FrameTime";
            value   = cast_to_bigint( cast_to_real( null_if_empty_str( txn.quote(mmap[colname]) ) ) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "TriggerTime";
            //value   = cast_to_bigint( cast_to_real( null_if_given_str( null_if_empty_str( txn.quote(mmap[colname]) ), txn.quote("0") ) ) );
            value   = cast_to_bigint( cast_to_real( null_if_empty_str( txn.quote(mmap[colname]) ) ) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "TriggerTimeOffset";
            value   = cast_to_bigint( cast_to_real( null_if_empty_str( txn.quote(mmap[colname]) ) ) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "PerformedProcedureStepStartDate";
            value   = cast_to_date( null_if_given_str( null_if_empty_str( txn.quote(mmap[colname]) ), txn.quote("0000-00-00")) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "PerformedProcedureStepStartTime";
            value   = cast_to_time( null_if_empty_str( txn.quote(mmap[colname]) ) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "PerformedProcedureStepEndDate";
            value   = cast_to_date( null_if_given_str( null_if_empty_str( txn.quote(mmap[colname]) ), txn.quote("0000-00-00")) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "PerformedProcedureStepEndTime";
            value   = cast_to_time( null_if_empty_str( txn.quote(mmap[colname]) ) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "Exposure";
            value   = cast_to_real( null_if_empty_str( txn.quote(mmap[colname]) ) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "ExposureTime";
            value   = cast_to_real( null_if_empty_str( txn.quote(mmap[colname]) ) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "ExposureInMicroAmpereSeconds";
            value   = cast_to_real( null_if_empty_str( txn.quote(mmap[colname]) ) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "XRayTubeCurrent";
            value   = cast_to_real( null_if_empty_str( txn.quote(mmap[colname]) ) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "RepetitionTime";
            value   = cast_to_double( null_if_empty_str( txn.quote(mmap[colname]) ) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "EchoTime";
            value   = cast_to_real( null_if_empty_str( txn.quote(mmap[colname]) ) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "NumberofAverages";
            value   = cast_to_double( null_if_empty_str( txn.quote(mmap[colname]) ) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "ImagingFrequency";
            value   = cast_to_double( null_if_empty_str( txn.quote(mmap[colname]) ) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "ImagedNucleus";
            value   = null_if_empty_str( txn.quote(mmap[colname]) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "EchoNumbers";
            value   = cast_to_double( null_if_empty_str( txn.quote(mmap[colname]) ) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "MagneticFieldStrength";
            value   = cast_to_real( null_if_empty_str( txn.quote(mmap[colname]) ) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "NumberofPhaseEncodingSteps";
            value   = cast_to_double( null_if_empty_str( txn.quote(mmap[colname]) ) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);


            colname = "EchoTrainLength";
            value   = cast_to_double( null_if_empty_str( txn.quote(mmap[colname]) ) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "PercentSampling";
            value   = cast_to_double( null_if_empty_str( txn.quote(mmap[colname]) ) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "PercentPhaseFieldofView";
            value   = cast_to_double( null_if_empty_str( txn.quote(mmap[colname]) ) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "PixelBandwidth";
            value   = cast_to_real( null_if_empty_str( txn.quote(mmap[colname]) ) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "DeviceSerialNumber";
            value   = null_if_empty_str( txn.quote(mmap[colname]) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "ProtocolName";
            value   = null_if_empty_str( txn.quote(mmap[colname]) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "ReceiveCoilName";
            value   = null_if_empty_str( txn.quote(mmap[colname]) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "TransmitCoilName";
            value   = null_if_empty_str( txn.quote(mmap[colname]) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "InplanePhaseEncodingDirection";
            value   = null_if_empty_str( txn.quote(mmap[colname]) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "FlipAngle";
            value   = cast_to_real( null_if_empty_str( txn.quote(mmap[colname]) ) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "SAR";
            value   = cast_to_double( null_if_empty_str( txn.quote(mmap[colname]) ) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "dB_dt";
            value   = cast_to_double( null_if_empty_str( txn.quote(mmap[colname]) ) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "PatientPosition";
            value   = null_if_empty_str( txn.quote(mmap[colname]) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "AcquisitionDuration";
            value   = cast_to_double( null_if_empty_str( txn.quote(mmap[colname]) ) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "Diffusion_bValue";
            value   = cast_to_double( null_if_empty_str( txn.quote(mmap[colname]) ) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "DiffusionGradientOrientation";
            value   = dicom_string_to_double_array( mmap[colname] );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "DiffusionDirection";
            value   = null_if_empty_str( txn.quote(mmap[colname]) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "WindowCenter";
            value   = cast_to_double( null_if_empty_str( txn.quote(mmap[colname]) ) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "WindowWidth";
            value   = cast_to_double( null_if_empty_str( txn.quote(mmap[colname]) ) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "RescaleIntercept";
            value   = cast_to_double( null_if_empty_str( txn.quote(mmap[colname]) ) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "RescaleSlope";
            value   = cast_to_double( null_if_empty_str( txn.quote(mmap[colname]) ) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "RescaleType";
            value   = null_if_empty_str( txn.quote(mmap[colname]) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            //DICOM radiotherapy plan metadata.
            colname = "RTPlanLabel";
            value   = null_if_empty_str( txn.quote(mmap[colname]) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "RTPlanName";
            value   = null_if_empty_str( txn.quote(mmap[colname]) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "RTPlanDescription";
            value   = null_if_empty_str( txn.quote(mmap[colname]) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "RTPlanDate";
            value   = cast_to_date( null_if_given_str( null_if_empty_str( txn.quote(mmap[colname]) ), txn.quote("0000-00-00")) );
            //value   = cast_to_date( null_if_empty_str( txn.quote(mmap[colname]) ) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "RTPlanTime";
            value   = cast_to_time( null_if_empty_str( txn.quote(mmap[colname]) ) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "RTPlanGeometry";
            value   = null_if_empty_str( txn.quote(mmap[colname]) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            //DICOM patient, physician, operator metadata.
            colname = "PatientsName";
            value   = null_if_empty_str( txn.quote(mmap[colname]) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "PatientsBirthDate";
            value   = cast_to_date( null_if_given_str( null_if_empty_str( txn.quote(mmap[colname]) ), txn.quote("0000-00-00")) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "PatientsGender";
            value   = null_if_empty_str( txn.quote(mmap[colname]) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "PatientsWeight";
            value   = cast_to_real( null_if_empty_str( txn.quote(mmap[colname]) ) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "OperatorsName";
            value   = null_if_empty_str( txn.quote(mmap[colname]) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "ReferringPhysicianName";
            value   = null_if_empty_str( txn.quote(mmap[colname]) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            //DICOM categorical fields.
            colname = "SOPClassUID";
            value   = null_if_empty_str( txn.quote(mmap[colname]) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "Modality";
            value   = null_if_empty_str( txn.quote(mmap[colname]) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            //DICOM machine/device, institution fields.
            colname = "Manufacturer";
            value   = null_if_empty_str( txn.quote(mmap[colname]) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "StationName";
            value   = null_if_empty_str( txn.quote(mmap[colname]) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "ManufacturersModelName";
            value   = null_if_empty_str( txn.quote(mmap[colname]) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "SoftwareVersions";
            value   = null_if_empty_str( txn.quote(mmap[colname]) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "InstitutionName";
            value   = null_if_empty_str( txn.quote(mmap[colname]) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            colname = "InstitutionalDepartmentName";
            value   = null_if_empty_str( txn.quote(mmap[colname]) );
            r3 = txn.exec(update_boilerplate(colname, value));            check_update_ok(colname);

            //Non-DICOM metadata fields.
            // - skipping "Project"
            // - skipping "Comments"
            // - skipping "FullPathName"
            // - skipping "gdcmdump"
            // - skipping "ImportTimepoint"

            //---------------------------------------------------------------------------------------------------------
        }

        //-------------------------------------------------------------------------------------------------------------
        //Finish the transaction and drop the connection.
        txn.commit();

    }catch(const std::exception &e){
        YLOGERR("Unable to push to database: " << e.what());
    }

//---------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------- Cleanup --------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------
    //Attempt to delete the DICOMTempFile if it exists.
    // Probably safest NOT to attempt to delete it until I have a safer interface (std::filesystem).
    return 0;
}
