//DICOMautomaton_Dispatcher.cc - A part of DICOMautomaton 2016. Written by hal clark.
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

#include <boost/filesystem.hpp>
#include <cstdlib>            //Needed for exit() calls.
#include <utility>            //Needed for std::pair.

#include "Boost_Serialization_File_Loader.h"
#include "DICOM_File_Loader.h"
#include "FITS_File_Loader.h"
#include "Operation_Dispatcher.h"
#include "PACS_Loader.h"
#include "Structs.h"
#include "YgorArguments.h"    //Needed for ArgumentHandler class.
#include "YgorFilesDirs.h"    //Needed for Does_File_Exist_And_Can_Be_Read(...), etc..
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorString.h"       //Needed for GetFirstRegex(...)


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

    //A explicit declaration that the user will generate data in an operation.
    bool GeneratingVirtualData = false;

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
    std::string db_connection_params("dbname=pacs user=hal host=localhost port=5432");

    //----------------------------------------------- Data: File Loading ---------------------------------------------
    // The following objects are only relevant for the various file loaders. They will be passed through the loaders
    // (e.g., DICOM file, Boost.Serialization archive, Protobuf file, etc.) until successfully loaded.

    //List of filenames or directories to parse and load.
    std::list<std::string> StandaloneFilesDirs;  // Used to defer filesystem checking.
    std::list<boost::filesystem::path> StandaloneFilesDirsReachable;


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
                       { "-f common.sql -f seriesA.sql -n -f seriesB.sql -o View",
                         "Load two distinct groups of data. The second group does not 'see' the"
                         " file 'common.sql' side effects -- the queries are totally separate." },
                       { "fileA fileB -s fileC adir/ -m PatientID=XYZ003 -o ComputeXYZ -o View",
                         "Load standalone files and all files in specified directory. Inform"
                         " the analysis 'ComputeXYZ' of the patient's ID, launch the analyses." }
                     };
    arger.description = "A program for launching DICOMautomaton analyses.";

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

        auto known_ops = Known_Operations();
        for(auto &anop : known_ops){
            std::cout << "Operation: '" << anop.first << "'" << std::endl;

            auto optdocs = anop.second.first();

            std::cout << "\tDescription:" << std::endl;
            for(auto &e : Reflow_Line_to_Fit_Width_Left_Just(optdocs.desc,70)){
                std::cout << "\t\t" << e << std::endl;
            }
            std::cout << std::endl;

            std::cout << "\tNotes:" << std::endl;
            for(auto &n : optdocs.notes){
                for(auto &e : Reflow_Line_to_Fit_Width_Left_Just(n,70)){
                    std::cout << "\t\t" << e << std::endl;
                }
                std::cout << std::endl;
            }
            std::cout << std::endl;

            std::cout << "\tParameters:" << std::endl;
            if(optdocs.args.empty()){
                std::cout << "\tNo registered options." << std::endl;
            }else{
                for(auto &a : optdocs.args){
                    std::cout << "\t  '" << a.name << "'" << std::endl;
                    for(const auto &aline : Reflow_Line_to_Fit_Width_Left_Just(a.desc,70)){
                        std::cout << "\t\t" << aline << std::endl;
                    }
                    std::cout << "\t    Default: \"" << a.default_val << "\"" << std::endl;
                    std::cout << "\t    Examples: " << std::endl;
                    for(auto &e : a.examples){
                        std::cout << "\t\t\t\"" << e << "\"" << std::endl;
                    }
                    std::cout << std::endl;
                }
            }
            std::cout << std::endl;
        }
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
          Operations.emplace_back(optarg);
        }catch(const std::exception &e){
          FUNCERR("Unable to parse operation: " << e.what());
        }
        return;
      })
    );

    arger.push_back( ygor_arg_handlr_t(400, 'x', "disregard", true, "SFML_Viewer",
      "Ignore the associated argument; essentially a 'no-op.' This option simplifies tweaking scripts.",
      [&](const std::string &) -> void {
        return;
      })
    );

    arger.Launch(argc, argv);

    //============================================== Input Verification ==============================================

    //Remove empty groups of query files. Probably not needed, as it ought to get caught at the DB query stage.
    for(auto l_it = GroupedFilterQueryFiles.begin(); l_it != GroupedFilterQueryFiles.end();  ){
        if(l_it->empty()){
            l_it = GroupedFilterQueryFiles.erase(l_it);
        }else{
            ++l_it;
        }
    }

    //Convert directories to filenames.
// TODO.


    //Remove non-existent filenames and directories.
    {
        boost::filesystem::path PathShuttle;
        for(const auto &auri : StandaloneFilesDirs){
            bool wasOK = false;
            try{
                PathShuttle = boost::filesystem::canonical(auri);
                wasOK = boost::filesystem::exists(PathShuttle);
            }catch(const boost::filesystem::filesystem_error &){ }

            if(wasOK){
                StandaloneFilesDirsReachable.emplace_back(auri);
            }else{
                FUNCWARN("Unable to resolve file or directory '" << auri << "'. Ignoring it");
            }
        }
    }

    //Try find a lexicon file if none were provided.
    if(FilenameLex.empty()){
        std::list<std::string> trial = { 
                "20150925_SGF_and_SGFQ_tags.lexicon",
                "Lexicons/20150925_SGF_and_SGFQ_tags.lexicon",
                "/usr/share/explicator/lexicons/20150925_20150925_SGF_and_SGFQ_tags.lexicon",
                "/usr/share/explicator/lexicons/20130319_SGF_filter_data_deciphered5.lexicon",
                "/usr/share/explicator/lexicons/20121030_SGF_filter_data_deciphered4.lexicon" };
        for(const auto & f : trial) if(Does_File_Exist_And_Can_Be_Read(f)){
            FilenameLex = f;
            FUNCINFO("No lexicon was explicitly provided. Using file '" << FilenameLex << "' as lexicon");
            break;
        }
    }

    
    
    //A likely lexicon file should always be provided.
    if(FilenameLex.empty()) FUNCERR("Lexicon not located. Please provide one or see program help for more info");


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

    //PACS db loading.
    if(!GroupedFilterQueryFiles.empty()){
        if(!Load_From_PACS_DB( DICOM_data, InvocationMetadata, FilenameLex, 
                               db_connection_params, GroupedFilterQueryFiles )){
            FUNCERR("Unable to load files from the PACS db. Cannot continue");
        }
    }

    //Standalone file loading.

    //Standalone file loading: Boost.Serialization archives.
    if(!StandaloneFilesDirsReachable.empty()
    && !Load_From_Boost_Serialization_Files( DICOM_data, InvocationMetadata, FilenameLex,
                                             StandaloneFilesDirsReachable )){
        FUNCERR("Failed to load Boost.Serialization archive");
    }

    //Standalone file loading: DICOM files.
    if(!StandaloneFilesDirsReachable.empty()
    && !Load_From_DICOM_Files( DICOM_data, InvocationMetadata, FilenameLex,
                               StandaloneFilesDirsReachable )){
        FUNCERR("Failed to load DICOM file");
    }

    //Standalone file loading: FITS files.
    if(!StandaloneFilesDirsReachable.empty()
    && !Load_From_FITS_Files( DICOM_data, InvocationMetadata, FilenameLex,
                               StandaloneFilesDirsReachable )){
        FUNCERR("Failed to load FITS file");
    }

    //Other loaders ...


    //If any standalone files remain, they cannot be loaded.
    if(!StandaloneFilesDirsReachable.empty()){
        FUNCERR("Unable to load file " << StandaloneFilesDirsReachable.front() << ". Refusing to continue");
    }

    //============================================= Dispatch to Analyses =============================================

    if(!Operation_Dispatcher( DICOM_data, InvocationMetadata, FilenameLex,
                              Operations )){
        FUNCERR("Analysis failed. Cannot continue");
    }

    return 0;
}
