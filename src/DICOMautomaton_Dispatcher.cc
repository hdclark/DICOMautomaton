//DICOMautomaton_Dispatcher.cc - A part of DICOMautomaton 2021. Written by hal clark.
//
// This program provides a standard entry-point into some DICOMautomaton analysis routines.
//

#include <exception>
#include <functional>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <string>    
#include <vector>
//#include <cfenv>              //Needed for std::feclearexcept(FE_ALL_EXCEPT).

#include <filesystem>
#include <cstdlib>            //Needed for exit() calls.
#include <utility>            //Needed for std::pair.

#include "YgorArguments.h"    //Needed for ArgumentHandler class.
#include "YgorFilesDirs.h"    //Needed for Does_File_Exist_And_Can_Be_Read(...), etc..
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorString.h"       //Needed for GetFirstRegex(...)

#include "Structs.h"

#include "Documentation.h"
#include "PACS_Loader.h"
#include "File_Loader.h"
#include "Lexicon_Loader.h"

#include "Operation_Dispatcher.h"
#include "DCMA_Version.h"

//extern const std::string DCMA_VERSION_STR;

int main(int argc, char* argv[]){
    //This is the main entry-point into various routines. All major options are set here, via command line arguments.
    // Depending on the arguments received, data is loaded through a variety of loaders and sent on to an analysis
    // dispatcher.
    //
    // Because the loader and analysis stages are separate, and separate from each other, this code should be amenable
    // to both direct use and remote use via some RPC mechanism.
    //

    //------------------------------------------------- Data: General ------------------------------------------------
    // The following objects should remain available for the analysis dispatcher and for some analysis routines (where
    // appopriate).

    //The main storage place and manager class for loaded image sets, contours, dose matrices, etc..
    Drover DICOM_data;

    //Lexicon filename, for the Explicator class. This is used in select cases for string translation.
    std::string FilenameLex;

    //User-defined tags which are used for helping to keep track of information not present (or easily available) in the
    // loaded DICOM data. Things like volunteer tracking numbers, information from imaging/scanning sessions, etc..
    std::map<std::string,std::string> InvocationMetadata;

    //Operations to perform on the data.
    std::list<OperationArgPkg> Operations;
    long int OperationDepth = 0;

    //A explicit declaration that the user will generate data in an operation.
    bool GeneratingVirtualData = false;

    //A Boolean guard variable to ensure loose parameters are only added to valid, active operations.
    bool MostRecentOperationActive = false;


    //------------------------------------------------- Data: Database -----------------------------------------------
    // The following objects are only relevant for the PACS database loader.

    //These are the means of file input from the database. Each distinct set can be composed of many files which are 
    // executed sequentially in the order provided. Each distinct set can thus create state on the database which can
    // be accessed by later scripts in the set. This facility is provided in case the user needs to run common setup
    // scripts (e.g., to create temporary views, pre-deal with NULLs, setup temporary functions, etc..)
    //
    // Each set is executed separately, and each set produces one distinct image collection. In this way, several image
    // series can be loaded into memory for processing or viewing. 
    std::list<std::list<std::string>> GroupedFilterQueryFiles;
    GroupedFilterQueryFiles.emplace_back();

    //PostgreSQL db connection settings.
    //std::string db_connection_params("dbname=pacs user=hal host=localhost port=5432");
    std::string db_connection_params("host=localhost ...");

    //----------------------------------------------- Data: File Loading ---------------------------------------------
    // The following objects are only relevant for the various file loaders. They will be passed through the loaders
    // (e.g., DICOM file, Boost.Serialization archive, Protobuf file, etc.) until successfully loaded.

    //List of filenames or directories to parse and load.
    std::list<std::string> StandaloneFilesDirs;  // Used to defer filesystem checking.
    std::list<std::filesystem::path> StandaloneFilesDirsReachable;


    //================================================ Argument Parsing ==============================================

    for(auto i = 0; i < argc; ++i) InvocationMetadata["Invocation"] += std::string(argv[i]) + " ";

    class ArgumentHandler arger;
    const std::string progname(argv[0]);
    arger.examples = { { "--help", 
                         "Show the help screen and some info about the program." },
                       { "-f create_temp_view.sql -f select_records_from_temp_view.sql -o ComputeSomething",
                         "Load a SQL common file that creates a SQL view, issue a query involving the view"
                         " which returns some DICOM file(s). Perform analysis 'ComputeSomething' with the"
                         " files." },
                       { "-f common.sql -f seriesA.sql -n -f seriesB.sql -o OperationXYZ",
                         "Load two distinct groups of data. The second group does not 'see' the"
                         " file 'common.sql' side effects -- the queries are totally separate." },
                       { "fileA fileB -s fileC adir/ -m PatientID=XYZ003 -o ComputeXYZ",
                         "Load standalone files and all files in specified directory. Inform"
                         " the analysis 'ComputeXYZ' of the patient's ID, launch the analyses." },
                       { "file.dcm -o ComputeX:abc=123 -x ComputeY -p def=456 -o ComputeZ -p ghi=678 -z ghi=789",
                         "Load file 'file.dcm', perform 'ComputeX' using abc=123, do *not* perform"
                         " 'ComputeY', and perform 'ComputeZ' using ghi=678 (not ghi=789)." }
                     };
    arger.description = "A program for launching DICOMautomaton analyses. Version:"_s + DCMA_VERSION_STR;

    arger.default_callback = [](int, const std::string &optarg) -> void {
      FUNCERR("Unrecognized option with argument: '" << optarg << "'");
      return; 
    };
    arger.optionless_callback = [&](const std::string &optarg) -> void {
      StandaloneFilesDirs.push_back( optarg );
      return; 
    };

    arger.push_back( ygor_arg_handlr_t(0, 'u', "detailed-usage", false, "",
      "Print detailed information about operation arguments and quit.",
      [&](const std::string &) -> void {
        Emit_Documentation(std::cout);
        std::exit(0);
        return;
      })
    );
 
    arger.push_back( ygor_arg_handlr_t(0, 'r', "version", false, "",
      "Print the version and quit.",
      [&](const std::string &) -> void {
        std::cout << "DICOMautomaton version: " << DCMA_VERSION_STR << std::endl;
        std::exit(0);
        return;
      })
    );
 
    arger.push_back( ygor_arg_handlr_t(100, 'l', "lexicon", true, "<best guess>",
      "Lexicon file for normalizing ROI contour names.",
      [&](const std::string &optarg) -> void {
        FilenameLex = optarg;
        return;
      })
    );
 
#ifdef DCMA_USE_POSTGRES
    arger.push_back( ygor_arg_handlr_t(210, 'd', "database-parameters", true, db_connection_params,
      "PostgreSQL database connection settings to use for PACS database.",
      [&](const std::string &optarg) -> void {
        db_connection_params = optarg;
        return;
      })
    );

    arger.push_back( ygor_arg_handlr_t(211, 'f', "filter-query-file", true, "/tmp/query.sql",
      "Query file(s) to use for filtering which DICOM files should be used for analysis."
      " Files are loaded sequentially and should ultimately return full metadata records.",
      [&](const std::string &optarg) -> void {
        GroupedFilterQueryFiles.back().push_back(optarg);
        return;
      })
    );

    arger.push_back( ygor_arg_handlr_t(212, 'n', "next-group", false, "", 
      "Signifies the beginning of a new (separate from the last) group of filter scripts.",
      [&](const std::string &) -> void {
        GroupedFilterQueryFiles.emplace_back();
        return;
      })
    );
#endif // DCMA_USE_POSTGRES

    arger.push_back( ygor_arg_handlr_t(220, 's', "standalone", true, "/path/to/dir/or/file",
      "Specify stand-alone files or directories to load. (This is the default for argument-less"
      " options.)",
      [&](const std::string &optarg) -> void {
        StandaloneFilesDirs.push_back( optarg );
        return;
      })
    );

    arger.push_back( ygor_arg_handlr_t(230, 'v', "virtual-data", false, "",
      "Inform the loaders that virtual data will be generated. Use with care, because this"
      " option causes checks to be skipped that could break assumptions in some operations.",
      [&](const std::string &) -> void {
        GeneratingVirtualData = true;
        return;
      })
    );

    arger.push_back( ygor_arg_handlr_t(300, 'm', "metadata", true, "'Volunteer=01'",
      "Metadata key-value pairs which are tacked onto results destined for a database. "
      "If there is an conflicting key-value pair, the values are concatenated.",
      [&](const std::string &optarg) -> void {
        auto tokens = SplitStringToVector(optarg, '=', 'd');
        if(tokens.size() != 2) FUNCERR("Metadata format not recognized: '" << optarg << "'. Use 'A=B'");
        InvocationMetadata[tokens.front()] += tokens.back();
        return;
      })
    );

    arger.push_back( ygor_arg_handlr_t(400, 'o', "operation", true, "SFML_Viewer",
      "An operation to perform on the fully loaded data. Some operations can be chained, some"
      " may necessarily terminate computation. See '-u' for detailed operation information.",
      [&](const std::string &optarg) -> void {
        try{
          if(OperationDepth == 0){
            Operations.emplace_back(optarg);
          }else{
            if(Operations.empty()) throw std::invalid_argument("No parent node found");
            OperationArgPkg *o = &( Operations.back() );
            for(long int i = 1; i < OperationDepth; ++i){
              o = o->lastChild();
              if(o == nullptr) throw std::invalid_argument("No child node found");
            }
            o->makeChild(optarg);
          }
          MostRecentOperationActive = true;
        }catch(const std::exception &e){
          FUNCERR("Unable to parse operation: " << e.what());
        }
        return;
      })
    );

    arger.push_back( ygor_arg_handlr_t(400, 'x', "disregard", true, "SFML_Viewer",
      "Ignore the following operation and all following parameters; essentially a 'no-op.'"
      " This option simplifies tweaking a workflow.",
      [&](const std::string &) -> void {
        MostRecentOperationActive = false;
        return;
      })
    );

    arger.push_back( ygor_arg_handlr_t(500, 'p', "parameter", true, "ABC=xyz",
      "A parameter to apply to the previous operation. This option is convenient when the number or length"
      " or parameters supplied to an operation is large.",
      [&](const std::string &optarg) -> void {
        try{
          if(MostRecentOperationActive){
            if(Operations.empty()) throw std::invalid_argument("No parent node found");
            OperationArgPkg *o = &( Operations.back() );

            for(long int i = 0; i < OperationDepth; ++i){
              o = o->lastChild();
              if(o == nullptr) throw std::invalid_argument("No child node found");
            }

            if(!(o->insert(optarg))){
              throw std::invalid_argument("Parameter insertion failed (is it duplicated?");
            }
          }
        }catch(const std::exception &e){
          FUNCERR("Unable to append parameter: " << e.what());
        }
        return;
      })
    );

    arger.push_back( ygor_arg_handlr_t(500, 'z', "ignore", true, "ABC=xyz",
      "Ignore the following parameter, but still perform the operation without it."
      " This option simplifies tweaking a workflow.",
      [&](const std::string &) -> void {
        return;
      })
    );

    arger.push_back( ygor_arg_handlr_t(600, '(', "start-children", false, "",
      "Descend scope by one level. Operations in this scope will be appended as children to the previous operation."
      " A valid node must preceed this option."
      " Note that this option may require escaping like '-\\('.",
      [&](const std::string &) -> void {
        if(Operations.empty()){
            FUNCERR("This option can only be specified after a valid operation");
        }
        ++OperationDepth;
        MostRecentOperationActive = false;
        return;
      })
    );

    arger.push_back( ygor_arg_handlr_t(600, ')', "stop-children", false, "",
      "Ascend scope by one level."
      " Note that this option may require escaping like '-\\)'.",
      [&](const std::string &) -> void {
        --OperationDepth;
        MostRecentOperationActive = false;
        if(OperationDepth < 0){
            FUNCERR("Mismatched scope modifiers '(' or ')' detected");
        }
        return;
      })
    );

    arger.Launch(argc, argv);

    //============================================== Input Verification ==============================================

    if(OperationDepth != 0){
        FUNCERR("Mismatched scope modifiers '(' or ')' detected");
    }

#ifdef DCMA_USE_POSTGRES
    //Remove empty groups of query files. Probably not needed, as it ought to get caught at the DB query stage.
    for(auto l_it = GroupedFilterQueryFiles.begin(); l_it != GroupedFilterQueryFiles.end();  ){
        if(l_it->empty()){
            l_it = GroupedFilterQueryFiles.erase(l_it);
        }else{
            ++l_it;
        }
    }
#endif // DCMA_USE_POSTGRES

    for(const auto &auri : StandaloneFilesDirs){
        StandaloneFilesDirsReachable.emplace_back(auri);
    }

    //Try find a lexicon file if none were provided.
    if(FilenameLex.empty()){
        FilenameLex = Locate_Lexicon_File();
        if(FilenameLex.empty()){
            FUNCINFO("No lexicon was explicitly provided. Using located file '" << FilenameLex << "' as lexicon");
        }
    }
    if(FilenameLex.empty()){
        FUNCINFO("No lexicon provided or located. Attempting to write a default lexicon");
        FilenameLex = Create_Default_Lexicon_File();
        FUNCINFO("Using file '" << FilenameLex << "' as lexicon");
    }

    //We require at least one SQL file for PACS db loading, one file/directory name for standalone file loading..
    if( GroupedFilterQueryFiles.empty()    
    &&  StandaloneFilesDirsReachable.empty()
    &&  !GeneratingVirtualData ){

        FUNCERR("No query files provided. Cannot proceed");

// TODO: Special case: Launch RPC server to wait for data if no files or SQL files provided?


    //If DB or standalone loading, we require at least one action.
    }else if(Operations.empty()){
        FUNCWARN("No operations specified: defaulting to operation 'SFML_Viewer'");
        Operations.emplace_back("SFML_Viewer");
    }


    //================================================= Data Loading =================================================

#ifdef DCMA_USE_POSTGRES
    //PACS db loading.
    if(!GroupedFilterQueryFiles.empty()){
        if(!Load_From_PACS_DB( DICOM_data, InvocationMetadata, FilenameLex, 
                               db_connection_params, GroupedFilterQueryFiles )){
            FUNCERR("Unable to load files from the PACS db. Cannot continue");
        }
    }
#endif // DCMA_USE_POSTGRES

    //Standalone file loading.
    if(!Load_Files(DICOM_data, InvocationMetadata, FilenameLex, StandaloneFilesDirsReachable)){
#ifdef DCMA_FUZZ_TESTING
        // If file loading failed, then the loader successfully rejected bad data. Terminate to indicate this success.
        return 0;
#else
        //FUNCERR("Unable to load file " << StandaloneFilesDirsReachable.front() << ". Refusing to continue");
        FUNCERR("File loading unsuccessful. Refusing to continue"); // TODO: provide better diagnostic here.
#endif // DCMA_FUZZ_TESTING
    }

    //============================================= Dispatch to Analyses =============================================

    if(!Operation_Dispatcher(DICOM_data, InvocationMetadata, FilenameLex, Operations)){
        FUNCERR("Analysis failed. Cannot continue");
    }

    return 0;
}
