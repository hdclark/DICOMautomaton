//Automaton.cc - DICOMautomaton 2012,2013,2014. Written by hal clark.
//
// This program is a generic program used for performing non-graphical tasks with data from 
// DICOM-format files. Development is ongoing, as this is a sort of testbed for ideas
// for the DICOMautomaton family.
//
// The plan is ultimately to have some of this code live in a library somewhere. This may
// happen *after* tweaking has slowed a bit.
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
#include "YgorString.h"      //Needed for GetFirstRegex(...)
#include "YgorImages.h"
#include "YgorImagesPlotting.h"
#include "Explicator.h"      //Needed for Explicator class.

#include "YgorDICOMTools.h"  //Needed for Is_File_A_DICOM_File(...);


//Forward declarations.
//class Dose_Array;   //Defined in Structs.h.
//class Contour_Data; //Defined in Structs.h.
//class Drover;       //Defined in structs.h.

//Constants.
const std::string Version("0.1.1 - Beta. Use at your own risk!"); //This has not yet been bumped. Should I use it or drop it?

//Globals.
bool VERBOSE = false;  //Provides additional information.
bool QUIET   = false;  //Suppresses ALL information. Not recommended!


//This function takes queries of two types (sanitized and non-sanitized,) an explicator instance, and a bimap of contour mappings (string <-> int), 
// and outputs a list of contour ROI numbers. On failure, the output is simply left blank. 
//
//NOTE: Duplicates are handled and removed from the output.
//
std::set<long int> Queries_to_ROI_Numbers(const std::set<std::string> &QueryString, const std::set<std::string> &SanitizedQueryString, Explicator *X, bimap<std::string, long int> Contour_classifications){
    std::set<long int> out;

    //The query strings are assumed to EXACTLY exist in a file. They might come from a previous mapping, or maybe we have exact information on the naming
    // of some contours *somehow.*
    for(const auto & s_it : QueryString){
        if( Contour_classifications.find( s_it ) != Contour_classifications.end() ){
            out.insert( Contour_classifications[s_it] );
        }
    } 

    //The sanitized query strings are those strings which the (dirty) DICOM strings will collapse to. In other words, these strings are contained in the
    // lexicon of the explicator we are passed.
    if(SanitizedQueryString.size() != 0){
        if( X != nullptr ){
            for(const auto & s_it : SanitizedQueryString){
                //Cycle through each tag in the contour data until we find (the first) match. We assume it is correct and move to the next.
                for(const auto & Contour_classification : Contour_classifications){
                    if( (*X)(Contour_classification.first) == s_it ){
                        out.insert(Contour_classification.second);
                        break;
                    }
                }
            }
        }else{
            if(VERBOSE && !QUIET) FUNCINFO("No explicator was passed in, so we were unable to handle sanitized queries");
        }
    }
    return out;
}

//Given the queries, the contour info from the DICOM files, and an explicator, we return a set of the (sanitized) queries which
// exist in the file. Queries which do not correspond to those in the DICOM data are not returned.
std::set<std::string> Queries_to_Available_Sanitized_Queries( const std::set<std::string> &QueryString, const std::set<std::string> &SanitizedQueryString, Explicator *X, bimap<std::string, long int> Contour_classifications){
    std::set<std::string> out;

    //The query strings are assumed to EXACTLY exist in a file. They might come from a previous mapping, or maybe we have exact information on the naming
    // of some contours *somehow.*
    for(const auto & s_it : QueryString){
        if( Contour_classifications.find( s_it ) != Contour_classifications.end() ){
            out.insert( (*X)(s_it) );
        }
    }

    //The sanitized query strings are those strings which the (dirty) DICOM strings will collapse to. In other words, these strings are contained in the
    // lexicon of the explicator we are passed.
    if(SanitizedQueryString.size() != 0){
        if( X != nullptr ){
            for(const auto & s_it : SanitizedQueryString){
                //Cycle through each tag in the contour data until we find (the first) match. We assume it is correct and move to the next.
                for(const auto & Contour_classification : Contour_classifications){
                    if( (*X)(Contour_classification.first) == s_it ){
                        out.insert((*X)(Contour_classification.first));
                        break;
                    }
                }
            }
        }else{
            if(VERBOSE && !QUIET) FUNCINFO("No explicator was passed in, so we were unable to handle sanitized queries");
        }
    }
    return out;
}


int main(int argc, char* argv[]){
//---------------------------------------------------------------------------------------------------------------------
//------------------------------------------- Instances used throughout -----------------------------------------------
//---------------------------------------------------------------------------------------------------------------------
    //Support/logistical structures.
    std::list<std::string> Filenames_In;
    std::string FilenameOut, FilenameLex;
    std::set<std::string> QueryString, SanitizedQueryString;

    //std::string db_params("dbname=Saliva user=hal host=localhost port=63443");
    std::string db_params("dbname=Saliva user=hal host=localhost port=5432");

    //Data structures.
    bimap<std::string,long int> Contour_classifications;
    Drover DICOM_data;

//---------------------------------------------------------------------------------------------------------------------
//------------------------------------------------ Option parsing -----------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------
//These are fairly common options. Run the program with -h to see them formatted properly.
    int next_options;
    const char* const short_options    = "hVvi:o:l:s:q:Q"; //This is the list of short, single-letter options.
                                                           //The : denotes a value passed in with the option.
    //This is the list of long options. Columns:  Name, BOOL: takes_value?, NULL, Map to short options.
    const struct option long_options[] = { { "help",            0, nullptr, 'h' },
                                           { "version",         0, nullptr, 'V' },
                                           { "verbose",         0, nullptr, 'v' },
                                           { "in",              1, nullptr, 'i' },
                                           { "out",             1, nullptr, 'o' },
                                           { "lexicon",         1, nullptr, 'l' },
                                           { "sanitized-query", 1, nullptr, 's' },
                                           { "query",           1, nullptr, 'q' },
                                           { "quiet",           0, nullptr, 'Q' },
                                           { nullptr,              0, nullptr,  0  }  };

    do{
        next_options = getopt_long(argc, argv, short_options, long_options, nullptr);
        switch(next_options){
            case 'h': 
                std::cout << std::endl;
                std::cout << argv[0] << " version " << Version << std::endl;
                std::cout << std::endl;
                std::cout << "-- Info: " << std::endl;
                std::cout << std::endl;
                std::cout << "  This program allows for performing rapid, no-nonsense, no-GUI computations using DICOM files. As much as possible," << std::endl;
                std::cout << "  emphasis is placed on having the program \"do the right thing\", which refers to the attempts to deal with incomplete" << std::endl;
                std::cout << "  information (such as missing files, non-matching DICOM data sets, and the careful treatment of existing data.)" << std::endl;
                std::cout << std::endl;
                std::cout << "  This program is designed to accept an input structure name(s) (pre-sanitized or not) and some DICOM data, and then produce" << std::endl;
                std::cout << "  output for the structure(s). An example might be computation of a DVH for the left parotid." << std::endl;
                std::cout << std::endl;
                std::cout << "  In some ways this program is very forgiving of user behaviour, but in general it has very strictly-defined behaviour." << std::endl;
                std::cout << "  For example, input files can be either directories or files, and non-DICOM files will be automatically weeded-out." << std::endl;
                std::cout << "  However, it is intentionally difficult to accidentally overwrite existing data: if an output file already exists, " << std::endl;
                std::cout << "  will usually refuse to overwrite it. To be user-friendly, though, a non-existing filename will be chosen and the the" << std::endl;
                std::cout << "  user will be warned. This might occasionally be frustrating for the user, but is the \"safe\" thing to do in most cases." << std::endl;
                std::cout << std::endl;
                std::cout << "-- Command line switches: " << std::endl;
                std::cout << std::endl;
                std::cout << "----------------------------------------------------------------------------------------------------------" << std::endl;
                std::cout << "   Short              Long                         Default          Description" << std::endl;
                std::cout << "----------------------------------------------------------------------------------------------------------" << std::endl;
                std::cout << "   -h                 --help                                        Display this message and exit." << std::endl;
                std::cout << "   -V                 --version                                     Display program version and exit." << std::endl;
                std::cout << "   -v                 --verbose                     <false>         Spit out info about what the program is doing." << std::endl;
                std::cout << "   -Q                 --quiet                       <false>         Suppresses everything except errors. Not recommended." << std::endl;
                std::cout << "                                                                      Not all warnings can be suppressed, but they have specific syntax." << std::endl;
                std::cout << std::endl;
                std::cout << "   -i myfilename      --in myfilename               <none>          Incoming DICOM file names. (Required)" << std::endl;
                std::cout << "   -i mydirname       --in mydirname                <none>          Incoming folder filled with DICOM files. (Required)" << std::endl;
                std::cout << "   -o newfilename     --out newfilename             /tmp/<random>   Outgoing file name." << std::endl;
                std::cout << std::endl;
                std::cout << "   -l filename        --lexicon filename            <best guess>    Explicator lexicon file name." << std::endl;
                std::cout << "   -s \"roi name\"      --sanitized-query \"roi name\"  <none>          (Sanitized) ROI name query string. (Body, Left Parotid, etc..)" << std::endl;
                std::cout << "                                                                      Use this when you do not know the exact tag." << std::endl;
                std::cout << "   -q \"roi name\"      --query           \"roi name\"  <none>          (Exact) ROI name query string. (LPAR, Lpar075, GTV+0.5, etc..)" << std::endl;
                std::cout << "                                                                      Use this when you do know the exact tag." << std::endl;
                std::cout << std::endl;
                std::cout << "-- Examples: " << std::endl;
                std::cout << "  (See the source directory until development cools down a little.)" << std::endl;
                std::cout << std::endl;
                return 0;
                break;
            case 'V': 
                INFO( "Version: " + Version );
                return 0;
                break;
            case 'v':
                INFO("Verbosity enabled");
                VERBOSE = true;
                break;
            case 'Q':
                QUIET = true;
                break;
            case 'i':
                //If we have a filename, simply put it in the collection.
                if(Does_File_Exist_And_Can_Be_Read(optarg)){
                    Filenames_In.emplace_back(optarg);
                }else{
                    //If we have a directory, grab all the filenames and place them in the collection.
                    //We will assume it is a directory now and will sort out the cruft afterward.
                    auto names = Get_List_of_File_and_Dir_Names_in_Dir(optarg);
                    for(auto & name : names) Filenames_In.push_back((std::string(optarg) + "/") + name);
                }
                break;
            case 'l':
                FilenameLex = optarg;
                break;
            case 'o':
                FilenameOut = optarg;
                break;
            case 'q':
                QueryString.insert(optarg);
                break;
            case 's':
                SanitizedQueryString.insert(optarg);
                break;
        }
    }while(next_options != -1);
    //From the optind man page:
    //  "If the resulting value of optind is greater than argc, this indicates a missing option-argument,
    //         and getopt() shall return an error indication."
    for( ; optind < argc; ++optind){
        //We treat everything else as input files. this is OK (but not safe) because we will test each file's existence.
        //FUNCWARN("Treating argument '" << argv[optind] << "' as an input filename");

        //If we have a filename, simply put it in the collection.
        if(Does_File_Exist_And_Can_Be_Read(argv[optind])){
            Filenames_In.emplace_back(argv[optind]);
        }else{
            //If we have a directory, grab all the filenames and place them in the collection.
            //We will assume it is a directory now and will sort out the cruft afterward.
            auto names = Get_List_of_File_and_Dir_Names_in_Dir(argv[optind]);
            for(auto & name : names) Filenames_In.push_back((std::string(argv[optind]) + "/") + name);
        }
    }


//---------------------------------------------------------------------------------------------------------------------
//------------------------------------------------ Input Verification ---------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------
//Preliminary safety checks: were we given enough information to successfully return any relevant info? 
//Note that we will catch file-related errors in the next section.
    if(QueryString.empty() && SanitizedQueryString.empty()){
        if(!QUIET) FUNCINFO("No queries were provided");
    }



//---------------------------------------------------------------------------------------------------------------------
//----------------------------------------------- Filename Testing ----------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------
//We now test the filenames to see if the input and output files exist. 
//So we do not overwrite the output file, we will exit if the output file already exists.

    if(Filenames_In.empty()) FUNCERR("No input given. Provide filenames or run '" << argv[0] << " -h' for info");
    if(FilenameOut.empty()){ 
        FilenameOut = Get_Unique_Filename("/tmp/DICOMautomaton_automaton_out_-_", 10);
        FUNCINFO("No output filename given. Proceeding with generated filename '" << FilenameOut << "'");
    }
    if(FilenameLex.empty()){ 
        std::list<std::string> trial = { 
                "20150925_SGF_and_SGFQ_tags.lexicon",
                "Lexicons/20150925_SGF_and_SGFQ_tags.lexicon",
                "/usr/share/explicator/lexicons/20150925_20150925_SGF_and_SGFQ_tags.lexicon",
                "/usr/share/explicator/lexicons/20130319_SGF_filter_data_deciphered5.lexicon",
                "/usr/share/explicator/lexicons/20121030_SGF_filter_data_deciphered4.lexicon" };
        for(auto & it : trial) if(Does_File_Exist_And_Can_Be_Read(it)){
            FilenameLex = it;
            FUNCINFO("No lexicon was provided. Using file '" << FilenameLex << "' as lexicon");
            break;
        }
        if(FilenameLex.empty()) FUNCERR("Lexicon not located. Please provide one or see " << argv[0] << " -h' for more info");
    }
    for(auto & it : Filenames_In){
        if(!Does_File_Exist_And_Can_Be_Read(it)) FUNCERR("Input file '" << it << "' does not exist");
    }
    if(!Does_File_Exist_And_Can_Be_Read(FilenameLex)) FUNCERR("Lexicon file '" << FilenameLex << "' does not exist");
    if(Does_File_Exist_And_Can_Be_Read(FilenameOut)) FUNCERR("Output file '" << FilenameOut << "' already exists");

    //The filenames are now set and the files are ready to be safely read/written.

//---------------------------------------------------------------------------------------------------------------------
//------------------------------------------------- File Sorting ------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------
    //For each input file, we grab the modality and sort into filename vectors.
    std::list<std::string> Filenames_In_Struct;  //RTSTRUCT modality. (RS structure file - contour (1D) data.)
    std::list<std::string> Filenames_In_CT;      //Image modalities.  (CT/MR/US data file. 2D pixel data.)
    std::list<std::string> Filenames_In_Dose;    //RTDOSE   modality. (RD dose files. 3D pixel data.)

    for(auto & it : Filenames_In){
        //First we check if the file is a valid DICOM format. If it is not, we simply ignore it.
        // Imebra should produce an exception if it cannot read the file, but bools are easier to deal with.
        if(!Is_File_A_DICOM_File(it)){
            FUNCWARN("File '" << it << "' does not appear to be a valid DICOM file. Ignoring it");
            continue;
        }
        const auto mod = get_modality(it);
        if(mod == "RTSTRUCT"){     Filenames_In_Struct.push_back(it); //Contours.
        }else if(mod == "RTDOSE"){ Filenames_In_Dose.push_back(it);   //Dose data.
        }else if(mod == "CT"){     Filenames_In_CT.push_back(it);     //CT.
        }else if(mod == "OT"){     Filenames_In_CT.push_back(it);     //"Other."
        }else if(mod == "US"){     Filenames_In_CT.push_back(it);     //Ultrasound.
        }else if(mod == "MR"){     Filenames_In_CT.push_back(it);     //MRI.
        }else if(mod == "PT"){     Filenames_In_CT.push_back(it);     //PET.
        }else if(!QUIET){  FUNCWARN("Unrecognized modality '" << mod << "' in file '" << it << "'. Ignoring it");
        }
    }

    //Now we blow away the list to make sure we don't accidentally use it.
    Filenames_In.clear();


//---------------------------------------------------------------------------------------------------------------------
//---------------------------------- File Parsing / Data Loading  : Patient Info --------------------------------------
//---------------------------------------------------------------------------------------------------------------------
//Perform any analysis which doesn't require loading anything (major) into memory.
if(true){
    std::string PatientID;
    if(!Filenames_In_Struct.empty()){
        PatientID = get_patient_ID(Filenames_In_Struct.front());
    }else if(!Filenames_In_Dose.empty()){
        PatientID = get_patient_ID(Filenames_In_Dose.front());
    }else if(!Filenames_In_CT.empty()){
        PatientID = get_patient_ID(Filenames_In_CT.front());
    }
    FUNCINFO("The patient ID is: " << PatientID);
}


//---------------------------------------------------------------------------------------------------------------------
//------------------------------------ File Parsing / Data Loading  : Contours ----------------------------------------
//---------------------------------------------------------------------------------------------------------------------

    //Load contour data.
    if(!Filenames_In_Struct.empty()) Contour_classifications  = get_ROI_tags_and_numbers(Filenames_In_Struct.front());
    if(!Filenames_In_Struct.empty()) DICOM_data.contour_data  = get_Contour_Data(Filenames_In_Struct.front());

//---------------------------------------------------------------------------------------------------------------------
//-------------------------------------------- Processing : Contours --------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------


    //Dump the raw contour data, the raw contour name, and the (explicator-derived) suspected cleaned organ name. This
    // is used for generating lexicons for libdemarcator.
if(false){
        Explicator X(FilenameLex);

        //Cycle through the contour_collections, stringify the data, and append it to file.
        for(auto & cc : DICOM_data.contour_data->ccs){
            const std::string rawname(cc.Raw_ROI_name);
            const std::string clean(X(rawname));
            const std::string rawcc(cc.write_to_string());

            {
            std::stringstream out;
            out << rawcc << " : " << clean << std::endl;
            AppendStringToFile(out.str(), "/tmp/automaton_new_demarcator_lexicon");
            }

            {
            std::stringstream out;
            out << clean << " : " << rawname << std::endl;
            AppendStringToFile(out.str(), "/tmp/automaton_new_demarcator_justwords");
            }
        }
        //-------------------------------------------------------------------
        //Exit normally.
        return 0;
}

    //Attempt to locate the desired query(ies) in the file. NOTE: This is NOT the best way to translate all tags!
if(false){
    if(!QueryString.empty() || !SanitizedQueryString.empty()){
        //Convert our sanitized/unsanitized queries into contour structure numbers.
        Explicator X(FilenameLex);
        std::set<long int> roi_numbers = Queries_to_ROI_Numbers(QueryString, SanitizedQueryString, &X, Contour_classifications);
        if(roi_numbers.empty()) FUNCERR("Unable to find matches within the file");

        //Output the matches.
        for(long roi_number : roi_numbers){
            std::cout << "Found match: '" << Contour_classifications[roi_number] << "'" << std::endl;
        }

        //Exit normally.
        return 0;
    }
}

    //Compute some geometrical features for specified organ(s). 
    // - Written in 20150113 for side project with Eman, Cheryl, and Steven.
    // - Used on 20150119 for computing volumes for paper re-submission.
    // Note: this routine does not require any dose information!
if(false){
    FUNCINFO("Computing geometrical moments and returning");
    if(!DICOM_data.Has_Contour_Data()) FUNCERR("No useable contour data. Cannot continue");
 
    Drover specific_data;
    Explicator X(FilenameLex);

    //Check if we have been passed any query data. If so, then we select a subset of the contour data to output.
    if(!QueryString.empty() || !SanitizedQueryString.empty()){
        //Convert our sanitized/unsanitized queries into contour structure numbers.
        std::set<long int> roi_numbers = Queries_to_ROI_Numbers(QueryString, SanitizedQueryString, &X, Contour_classifications);
        if(roi_numbers.empty()) FUNCERR("No contour numbers could be generated from input queries. Maybe the contours don't exist?");

        //Select only the data of interest.
        specific_data = DICOM_data.Duplicate( DICOM_data.contour_data->Get_Contours_With_Numbers( roi_numbers ) );

    //Otherwise, continue with all available structures.
    }else{
        specific_data = DICOM_data;
    }

    //Initialize/register the data (to ensure it is possible to produce sensible results).
    specific_data.Meld(VERBOSE && !QUIET);

    //Compute features. Report them within the loop (for each structure).
    for(const auto &acc : specific_data.contour_data->ccs){
        std::stringstream column_headers;
        std::stringstream column_features;

        column_headers << "Anon ID,";
        column_features << get_patient_ID(Filenames_In_Struct.front()) << ","; 
        column_headers << "Written structure,";
        column_features << "\"" << acc.Raw_ROI_name << "\",";
        column_headers << "Suspected structure,";
        column_features << "\"" << X(acc.Raw_ROI_name) << "\",";
        column_headers << "Average point,";
        column_features << "\"" << acc.Average_Point() << "\",";
        column_headers << "Centroid,";
        column_features << "\"" << acc.Centroid() << "\",";
        column_headers << "Perimeter,";
        column_features << acc.Perimeter() << ",";
        column_headers << "Average Perimeter,";
        column_features << acc.Average_Perimeter() << ",";
        column_headers << "Longest Perimeter,";
        column_features << acc.Longest_Perimeter() << ",";
        column_headers << "Slab Volume [cm^3],";
        const bool IgnoreContourOrientation = true;
        column_features << (1E-3)*acc.Slab_Volume( acc.Minimum_Separation, IgnoreContourOrientation ) << ",";

        //Write the SGFID and features in CSV format. (To screen? To file? To db? ...)
        std::cout << column_headers.str() << std::endl;
        std::cout << column_features.str() << std::endl;
    }
    return 0;
}


    //Perform some generic operations on the Contour data if it exists. Note that if it does not exist, it may not be an error.
if(false){
    if(DICOM_data.Has_Contour_Data()){
      {
        //Take all the ROI (Contour) names from the files, perform a translation using the lexicon, dump the data as a mapping file.
        Explicator X(FilenameLex); 
        const std::string FilenameMapping = Get_Unique_Sequential_Filename(Filenames_In_Struct.front() + ".map");        
        std::fstream FO(FilenameMapping.c_str(), std::ios::out);
        if(!FO.good()){
            FUNCERR("Unable to open mapping file for output. Attempting to use output filename '" << FilenameMapping << "'");
        }else{
            for(auto it=Contour_classifications.begin(); it != Contour_classifications.end(); ++it){
                std::string mapping(X(it->first));
                if(VERBOSE && !QUIET) FUNCINFO("Mapping file entry: " << it->first << " : " << mapping );
                FO << mapping << " : " << it->first << std::endl;
            }
            FO.close();
        }
      }

        //Print out a list of (the unique) ROI name/number correspondance.
        std::map<long int, bool> displayed;
        for(auto cc_it = DICOM_data.contour_data->ccs.begin(); cc_it != DICOM_data.contour_data->ccs.end(); ++cc_it){
            //Check if it has come up before. Place it in the map if it has (The bool value is inconsequential here, but we are protected by odd ROI numbers, etc..)
            if(displayed.find(cc_it->ROI_number) == displayed.end()){
                if(!QUIET) FUNCINFO("Contour with ROI number " << cc_it->ROI_number << " is named '" << cc_it->Raw_ROI_name << "'");
                displayed[cc_it->ROI_number] = true;
            }
        }

        //Produce some segmented contours.
        std::unique_ptr<Contour_Data> Subsegmented_New__Style_Contour_Data;
        //Subsegmented_New__Style_Contour_Data = (DICOM_data.contour_data->Split_Per_Volume_Along_Coronal_Plane())->Split_Per_Volume_Along_Sagittal_Plane();
        //Subsegmented_New__Style_Contour_Data = DICOM_data.contour_data->Split_Per_Height_Along_Coronal_Plane();
        //Subsegmented_New__Style_Contour_Data = DICOM_data.contour_data->Split_Per_Contour_Along_Coronal_Plane();
        //Subsegmented_New__Style_Contour_Data = DICOM_data.contour_data->Split_Per_Contour_Along_Coronal_Plane()->Split_Per_Contour_Along_Sagittal_Plane()->Split_Per_Contour_Along_Sagittal_Plane()->Split_Per_Contour_Along_Sagittal_Plane();

        //Print out a list of (the unique) ROI name/number correspondances in the subsegmented contours.
        //for(auto c_it = Subsegmented_New__Style_Contour_Data->contours.begin(); c_it != Subsegmented_New__Style_Contour_Data->contours.end(); ++c_it ){
        //    FUNCINFO("Subsegmented contour with ROI number " << (*c_it).ROI_number << " is named \"" << (*c_it).Raw_ROI_name << "\".");
        //}

        //Plot all the contours found in the DICOM file (one plot.)
        //DICOM_data.contour_data->Plot();

        //Plot all the contours found in the DICOM file (individual plots.)
        //for( auto i : DICOM_data.contour_data->contours ){ //Contour iteration.
        //    i.Plot();
        //}

        //Print out all the (sub)segmented contours.
        //Subsegmented_New__Style_Contour_Data->Plot();

    }
}


//---------------------------------------------------------------------------------------------------------------------
//------------------------------------- File Parsing / Data Loading  : Images -----------------------------------------
//---------------------------------------------------------------------------------------------------------------------
    //Load dose and image data.
    if(!Filenames_In_Dose.empty()) DICOM_data.dose_data = Load_Dose_Arrays(Filenames_In_Dose);
    if(!Filenames_In_CT.empty()) DICOM_data.image_data  = Load_Image_Arrays(Filenames_In_CT);

//for(const auto &f : Filenames_In_CT) std::cout << "CT Filename: " << f << std::endl;

//---------------------------------------------------------------------------------------------------------------------
//--------------------------------------------- Processing : Images ---------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------
    //Meld the data. This gathers information from the various files and attempts to amalgamate it.
    if(!DICOM_data.Meld(VERBOSE && !QUIET) )  FUNCERR("Unable to meld data.");

    //Test the duplication mechanism by creating another drover. 
if(false){
    Drover Another(DICOM_data);
    if(!Another.Meld(VERBOSE && !QUIET)) FUNCERR("Unable to meld duplicated data. Duplication was likely incomplete");
    return 0;
}

    //Test image and contour data using the plotting members. Probably not useful.
if(false){
    //DICOM_data.image_data.front()->imagecoll.Plot_Outlines();
    //DICOM_data.contour_data->Plot();
    DICOM_data.Plot_Image_Outlines();
    return 0;
}

    //Plot dose and contour data using the plotting members. Probably not useful.
if(false){
    Plot_Outlines(DICOM_data.dose_data.front()->imagecoll);
    DICOM_data.contour_data->Plot();
    DICOM_data.Plot_Dose_And_Contours();
    return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//               I'm not sure at the moment how to reconcile multiple queries / mixing sanitized and 
//                        unsanitized queries in the DVH and Mean Dose routines...  
//    FIXME                                                                                                   FIXME
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    //-------------------------------------- Compute a DVH and output it to file. ------------------------------------------
if(false){
    FUNCINFO("Computing a DVH now");
//    if(DICOM_data.Has_Contour_Data() && DICOM_data.Has_Image_Data()){
    if(DICOM_data.Has_Contour_Data() && DICOM_data.Has_Dose_Data()){
        long int roi_number(-1);

        //If given a _clean_ string query, then we need to locate a dirty string (from the file) which translates to it.
        {
          Explicator X(FilenameLex);
          for(const auto & Contour_classification : Contour_classifications){
              if( X(Contour_classification.first) == "Left Parotid" ){
                  roi_number = Contour_classification.second;
                  break;
              }
          }
        }  

        //If given a _dirty_ string query, then we check if it exists in the file. No attempt is made to reconcile non-existant data!
        //if( the query provided is in Contour_classifications){
        //    roi_number = Contour_classifications[the_query];
        //}
        
        //Check if the desired string has been found.
        if(roi_number == -1) FUNCERR("Unable to find desired structure within DICOM file. Is the query malformed?");

        //Make a new Drover which contains ONLY the contour(s) we are interested in. Meld it so we can reconcile the data. 
        Drover specific_data = DICOM_data.Duplicate( DICOM_data.contour_data->Get_Contours_With_Number( roi_number ) );
        specific_data.Meld(VERBOSE && !QUIET);

        auto thedvh = specific_data.Get_DVH();
  
        std::fstream FO(FilenameOut.c_str(), std::ifstream::out);
        FO << "# DVH for structure \"" << Contour_classifications[roi_number] << "\"" << std::endl;
        for(auto & it : thedvh){
            FO << it.first << " " << it.second << std::endl;
        }
        FO.close();
    }
    return 0;
}


    //------------------------- Perfom some segmentation and then output a DVH to file FOR EACH subsegment. ----------------------------
if(false){
    FUNCINFO("Segmenting structure into N subsegments and computing N DVHs now");
    if(DICOM_data.Has_Contour_Data() && DICOM_data.Has_Dose_Data()){
        //Subsegment only specific data from user queries (or everything, if no query provided).
        Drover specific_data;

        //Check if we have been passed any query data. If so, then we select a subset of the contour data to output.
        if(!QueryString.empty() || !SanitizedQueryString.empty()){
            //Convert our sanitized/unsanitized queries into contour structure numbers.
            Explicator X(FilenameLex);
            std::set<long int> roi_numbers = Queries_to_ROI_Numbers(QueryString, SanitizedQueryString, &X, Contour_classifications);
            if(roi_numbers.empty()) FUNCERR("No contour numbers could be generated from input queries. Maybe the contours don't exist?");

            //Select only the data of interest.
            specific_data = DICOM_data.Duplicate( DICOM_data.contour_data->Get_Contours_With_Numbers( roi_numbers ) );

        //Otherwise, continue with all available structures.
        }else{
            specific_data = DICOM_data;
        }

        //Perform some (fixed) sub-segmentation recipe.
        specific_data = specific_data.Duplicate( specific_data.contour_data->Split_Per_Volume_Along_Coronal_Plane() );
        specific_data = specific_data.Duplicate( specific_data.contour_data->Split_Per_Volume_Along_Sagittal_Plane() );
        //specific_data = specific_data.Duplicate( specific_data.contour_data->Split_Per_Contour_Along_Coronal_Plane() );
        //specific_data = specific_data.Duplicate( specific_data.contour_data->Split_Per_Contour_Along_Sagittal_Plane() );
        // ...etc...

        //Now cycle over the subsegments, one at a time, and compute the DVH. We have to monkey around to
        // get individual contours. We try polling for them until we get nullptrs. When we start getting
        // nullptrs, we move onto the next contours_with_meta. When that starts gving us nullptrs, we are done.
        // Both steps are wrapped in a single selector call, so we check for a nullptr when we ask for the
        // first element of a std::list and get a nullptr (meaning we have exhausted all available contours).
        for(long int cc_selector = 0; ; ){
            long int c_selector = 0;
            for(;;){
                //"scwiupcd" = single contour wrapped in unique_ptr<contour_data>
                auto scwiupcd = specific_data.contour_data->Get_Single_Contour_Number(cc_selector, c_selector);
                if(scwiupcd == nullptr) break;
        
                FUNCINFO("Computing DVH for contours_with_meta #" << cc_selector << " , contour #" << c_selector);
        
                //"scd" = single contour Drover.
                Drover scd = specific_data.Duplicate( std::move(scwiupcd) );
                scd.Meld(VERBOSE && !QUIET);
                auto thedvh = scd.Get_DVH();

                const auto FilenameOutDVH = Get_Unique_Sequential_Filename(FilenameOut + "_contour_");
                std::fstream FO(FilenameOutDVH.c_str(), std::ifstream::out);
                FO << "# DVH for contours_with_meta #" << cc_selector << " , contour #" << c_selector << std::endl;
                for(auto & it : thedvh) FO << it.first << " " << it.second << std::endl;
                FO.close();
                FUNCINFO("Wrote DVH to '" << FilenameOutDVH << "'");

                ++c_selector;
            }

            //If no contour could be selected when c_selector == 0, then we have cycled through all available.
            if(c_selector == 0) break;
            ++cc_selector;
        }


        //Now do the same thing except produce one DVH for each contour_collection (ie. subsegment).
        for(long int cc_selector = 0; ; ){
            //"sccwiupcd" = single contour collection wrapped in unique_ptr<contour_data>
            auto sccwiupcd = specific_data.contour_data->Get_Contours_Number(cc_selector);
            if(sccwiupcd == nullptr) break;

            FUNCINFO("Computing DVH for contours_with_meta #" << cc_selector);

            //"sccd" = single contour collection Drover.
            Drover sccd = specific_data.Duplicate( std::move(sccwiupcd) );
            sccd.Meld(VERBOSE && !QUIET);
            auto thedvh = sccd.Get_DVH();

            const auto FilenameOutDVH = Get_Unique_Sequential_Filename(FilenameOut + "_ccollection_");
            std::fstream FO(FilenameOutDVH.c_str(), std::ifstream::out);
            FO << "# DVH for contours_with_meta #" << cc_selector << std::endl;
            for(auto & it : thedvh) FO << it.first << " " << it.second << std::endl;
            FO.close();
            FUNCINFO("Wrote DVH to '" << FilenameOutDVH << "'");

            ++cc_selector;
        }

    } 
    return 0;
}


    //------------------------------- Compute the mean dose for all structures and print to screen. ------------------------------------
if(false){
    if(DICOM_data.Has_Contour_Data() && DICOM_data.Has_Dose_Data()){
        Drover specific_data;
        specific_data = DICOM_data;
        specific_data.Meld(VERBOSE && !QUIET);
        auto mmmmdoses = specific_data.Bounded_Dose_Min_Mean_Median_Max();
        const auto p_anon_id = get_patient_ID(Filenames_In_Struct.front());
        for(auto & mmmmdose : mmmmdoses){
            const auto name    = mmmmdose.first->Raw_ROI_name;
            const auto mmmm    = mmmmdose.second;
            //const auto min     = std::get<0>(mmmm);
            const auto mean    = std::get<1>(mmmm);
            //const auto median  = std::get<2>(mmmm);
            //const auto max     = std::get<3>(mmmm);
            std::cout << "DUMP," << p_anon_id << "," << name << "," << mean << std::endl;
        }
    }
    return 0;
}

    //-------------------------------- Compute the mean dose for a structure and output it to file. ------------------------------------
if(false){
    if(DICOM_data.Has_Contour_Data() && DICOM_data.Has_Dose_Data()){
        Drover specific_data;
        Explicator X(FilenameLex);

        //Check if we have been passed any query data. If so, then we select a subset of the contour data to output.
        if(!QueryString.empty() || !SanitizedQueryString.empty()){
            //Convert our sanitized/unsanitized queries into contour structure numbers.
            std::set<long int> roi_numbers = Queries_to_ROI_Numbers(QueryString, SanitizedQueryString, &X, Contour_classifications);
            if(roi_numbers.empty()) FUNCERR("No contour numbers could be generated from input queries. Maybe the contours don't exist?");

            //Select only the data of interest.
            specific_data = DICOM_data.Duplicate( DICOM_data.contour_data->Get_Contours_With_Numbers( roi_numbers ) );

        //Otherwise, continue with all available structures.
        }else{
            specific_data = DICOM_data;
        }

        //Perform some (fixed) sub-segmentation recipe. To get whole organs, perform no sub-segmentation (i.e., comment all).
        //
        //Split into halves.

        //specific_data = specific_data.Duplicate( specific_data.contour_data->Split_Per_Volume_Along_Sagittal_Plane()->Reorder_LR_to_ML() ); //Left/Right. 

        //specific_data = specific_data.Duplicate( specific_data.contour_data->Split_Per_Volume_Along_Coronal_Plane() );  //Front/Back. 
        //specific_data = specific_data.Duplicate( specific_data.contour_data->Split_Per_Volume_Along_Transverse_Plane() ); //Top/Bottom. 


        //specific_data = specific_data.Duplicate( specific_data.contour_data->Split_Per_Contour_Along_Sagittal_Plane()->Reorder_LR_to_ML() ); //Left/Right.
        
        //specific_data = specific_data.Duplicate( specific_data.contour_data->Split_Per_Contour_Along_Coronal_Plane() );  //Front/Back. 
       
        // (splitting per-contour transversely is not possible. Each contour would lie in it's own plane!)

        //Split into quarters.
        //specific_data = specific_data.Duplicate( specific_data.contour_data->Split_Per_Volume_Along_Sagittal_Plane()->Reorder_LR_to_ML() );
        //specific_data = specific_data.Duplicate( specific_data.contour_data->Split_Per_Volume_Along_Sagittal_Plane()->Reorder_LR_to_ML() );

        //specific_data = specific_data.Duplicate( specific_data.contour_data->Split_Per_Volume_Along_Sagittal_Plane()->Reorder_LR_to_ML() );
        //specific_data = specific_data.Duplicate( specific_data.contour_data->Split_Per_Volume_Along_Coronal_Plane() );

        //specific_data = specific_data.Duplicate( specific_data.contour_data->Split_Per_Volume_Along_Sagittal_Plane()->Reorder_LR_to_ML() );
        //specific_data = specific_data.Duplicate( specific_data.contour_data->Split_Per_Volume_Along_Transverse_Plane() );

        //specific_data = specific_data.Duplicate( specific_data.contour_data->Split_Per_Volume_Along_Coronal_Plane() );
        //specific_data = specific_data.Duplicate( specific_data.contour_data->Split_Per_Volume_Along_Transverse_Plane() );

        // Per-contour Sag/Cor and Per-contour Tnv ? 

        //specific_data = specific_data.Duplicate( specific_data.contour_data->Split_Per_Contour_Along_Sagittal_Plane()->Reorder_LR_to_ML() );
        //specific_data = specific_data.Duplicate( specific_data.contour_data->Split_Per_Contour_Along_Coronal_Plane() );



        //Core and Peel.
        //specific_data = specific_data.Duplicate( specific_data.contour_data->Split_Core_and_Peel(0.7) );

        //Core and Peel with a planar split.
        //specific_data = specific_data.Duplicate( specific_data.contour_data->Split_Core_and_Peel(0.7) );
        //specific_data = specific_data.Duplicate( specific_data.contour_data->Raycast_Split_Per_Contour_Into_Lateral() );

        //Initialize/register the data (to ensure it is possible to produce sensible results).
        specific_data.Meld(VERBOSE && !QUIET);


        //Compute the min/mean/max dose for each contour_collection. This is probably of type map from cc_iter to std::tuple<double,double,double>.
        auto mmmmdoses = specific_data.Bounded_Dose_Min_Mean_Median_Max();

        //Compute the normalized statistical moments for each contour_collection. It is proably a map type from cc_iter to std::map<std::array<int,3>,double>.
        auto moments = specific_data.Bounded_Dose_Normalized_Cent_Moments();
 
        //Output the data to screen.
        std::cout << std::setw(25) << "Structure" << std::setw(15) << "min dose" << std::setw(15) << "mean dose" << std::setw(15) << "median dose" << std::setw(15) << "max dose" << "    " << "segmentation" << std::endl;
        std::cout << std::setw(25) << "---------" << std::setw(15) << "--------" << std::setw(15) << "---------" << std::setw(15) << "-----------" << std::setw(15) << "--------" << "    " << "------------" << std::endl;
        for(auto & mmmmdose : mmmmdoses){
            const auto name    = mmmmdose.first->Raw_ROI_name;
            const auto seghist = Segmentations_to_Words( mmmmdose.first->Segmentation_History );
            const auto mmmm    = mmmmdose.second;
            const auto min     = std::get<0>(mmmm);
            const auto mean    = std::get<1>(mmmm);
            const auto median  = std::get<2>(mmmm);
            const auto max     = std::get<3>(mmmm);
            std::cout << std::setw(25) << name << std::setw(15) << min << std::setw(15) << mean << std::setw(15) << median << std::setw(15) << max << "    " << seghist << std::endl;
        }
        std::cout << std::endl;
        std::cout << std::setw(25) << "Structure" << std::setw(15) << "p" << std::setw(15) << "q" << std::setw(15) << "r" << std::setw(15) << "moment" << "    " << "segmentation" << std::endl;
        std::cout << std::setw(25) << "---------" << std::setw(15) << "-" << std::setw(15) << "-" << std::setw(15) << "-" << std::setw(15) << "------" << "    " << "------------" << std::endl;
        for(auto & moment : moments){
            const auto name    = moment.first->Raw_ROI_name;
            const auto seghist = Segmentations_to_Words( moment.first->Segmentation_History );
            const auto moms    = moment.second;
            for(const auto & mom : moms){
                const auto p = mom.first[0], q = mom.first[1], r = mom.first[2];
                if((p+q+r) > 3) continue;
                const auto themoment = mom.second;
                std::cout << std::setw(25) << name << std::setw(15) << p << std::setw(15) << q << std::setw(15) << r << std::setw(15) << themoment << "    " << seghist << std::endl;
            }
        }

        //Exit normally.
        return 0;
    }
}

    //-------------------------------- Compute the mean dose for a structure and output it to a db. ------------------------------------
if(false){
    if(DICOM_data.Has_Contour_Data() && DICOM_data.Has_Dose_Data()){
        Drover specific_data;
        Explicator X(FilenameLex);

        std::string patientID = get_patient_ID(Filenames_In_Struct.front()); //"SGF[0-9]{1,3}"
        if(patientID == "SG34") patientID = "SGF34";  //Corrections for Vitali's typos.
        if(patientID == "SG64") patientID = "SGF64";

        //Check if we have been passed any query data. If so, then we select a subset of the contour data to output.
        if(!QueryString.empty() || !SanitizedQueryString.empty()){
            //Convert our sanitized/unsanitized queries into contour structure numbers.
            std::set<long int> roi_numbers = Queries_to_ROI_Numbers(QueryString, SanitizedQueryString, &X, Contour_classifications);
            if(roi_numbers.empty()) FUNCERR("No contour numbers could be generated from input queries. Maybe the contours don't exist?");

            //Select only the data of interest.
            specific_data = DICOM_data.Duplicate( DICOM_data.contour_data->Get_Contours_With_Numbers( roi_numbers ) );

        //Otherwise, continue with all available structures.
        }else{
            specific_data = DICOM_data;
        }

        //Perform some (fixed) sub-segmentation recipe. To get whole organs, perform no sub-segmentation (i.e., comment all).
        //
        //Split into halves.

        //specific_data = specific_data.Duplicate( specific_data.contour_data->Split_Per_Volume_Along_Sagittal_Plane()->Reorder_LR_to_ML() ); //Left/Right. 

        //specific_data = specific_data.Duplicate( specific_data.contour_data->Split_Per_Volume_Along_Coronal_Plane() );  //Front/Back. 
        //specific_data = specific_data.Duplicate( specific_data.contour_data->Split_Per_Volume_Along_Transverse_Plane() ); //Top/Bottom. 


        //specific_data = specific_data.Duplicate( specific_data.contour_data->Split_Per_Contour_Along_Sagittal_Plane()->Reorder_LR_to_ML() ); //Left/Right.
        
        //specific_data = specific_data.Duplicate( specific_data.contour_data->Split_Per_Contour_Along_Coronal_Plane() );  //Front/Back. 
       
        // (splitting per-contour transversely is not possible. Each contour would lie in it's own plane!)

        //Split into quarters.
        //specific_data = specific_data.Duplicate( specific_data.contour_data->Split_Per_Volume_Along_Sagittal_Plane()->Reorder_LR_to_ML() );
        //specific_data = specific_data.Duplicate( specific_data.contour_data->Split_Per_Volume_Along_Sagittal_Plane()->Reorder_LR_to_ML() );

        //specific_data = specific_data.Duplicate( specific_data.contour_data->Split_Per_Volume_Along_Sagittal_Plane()->Reorder_LR_to_ML() );
        //specific_data = specific_data.Duplicate( specific_data.contour_data->Split_Per_Volume_Along_Coronal_Plane() );

        //specific_data = specific_data.Duplicate( specific_data.contour_data->Split_Per_Volume_Along_Sagittal_Plane()->Reorder_LR_to_ML() );
        //specific_data = specific_data.Duplicate( specific_data.contour_data->Split_Per_Volume_Along_Transverse_Plane() );

        //specific_data = specific_data.Duplicate( specific_data.contour_data->Split_Per_Volume_Along_Coronal_Plane() );
        //specific_data = specific_data.Duplicate( specific_data.contour_data->Split_Per_Volume_Along_Transverse_Plane() );

        // Per-contour Sag/Cor and Per-contour Tnv ? 

        //specific_data = specific_data.Duplicate( specific_data.contour_data->Split_Per_Contour_Along_Sagittal_Plane()->Reorder_LR_to_ML() );
        //specific_data = specific_data.Duplicate( specific_data.contour_data->Split_Per_Contour_Along_Coronal_Plane() );

        //Core and Peel.
        specific_data = specific_data.Duplicate( specific_data.contour_data->Split_Core_and_Peel(0.7) );

        //Core and Peel with a planar split.
        //specific_data = specific_data.Duplicate( specific_data.contour_data->Split_Core_and_Peel(0.7) );
        //specific_data = specific_data.Duplicate( specific_data.contour_data->Raycast_Split_Per_Contour_Into_Lateral() );

        //Initialize/register the data (to ensure it is possible to produce sensible results).
        specific_data.Meld(VERBOSE && !QUIET);


        //Compute the min/mean/max dose for each contour_collection. This is probably of type map from cc_iter to std::tuple<double,double,double>.
        auto mmmmdoses = specific_data.Bounded_Dose_Min_Mean_Median_Max();

        //Compute the normalized statistical moments for each contour_collection. It is proably a map type from cc_iter to std::map<std::array<int,3>,double>.
        auto moments = specific_data.Bounded_Dose_Normalized_Cent_Moments();
 
        //Send to the database.
        try{
            pqxx::connection c(db_params);
            pqxx::work txn(c);
            pqxx::result r;
            std::stringstream query;

            std::map<std::string,size_t> ssnc; //Sub-segment number counter.
            for(auto & mmmmdose : mmmmdoses){
                const auto name      = mmmmdose.first->Raw_ROI_name; //Raw, dirty name ("l_par").
                const auto ssn       = Xtostring<size_t>(ssnc[name]); //Sub-segment number (for the given structure).
                ++(ssnc[name]);

                const auto cleanname = X(name);                    //Clean, sanitized name ("Left Parotid").
                const auto seghist   = Segmentations_to_Words( mmmmdose.first->Segmentation_History );
                const auto desc      = name + " = "_s + seghist;

                const auto mmmm    = mmmmdose.second;
                const auto min     = std::get<0>(mmmm);
                const auto mean    = std::get<1>(mmmm);
                const auto median  = std::get<2>(mmmm);
                const auto max     = std::get<3>(mmmm);
 
                //Determine the database anonid.
                query.str(""); //Clear stringstream.
                query << "SELECT anonid FROM sgf_identifiers WHERE sgfid = " << txn.quote(patientID) << ";";
                r = txn.exec(query.str());
                if(r.empty()) FUNCERR("Unable to determine database anonid corresponding to '" << patientID << "'");
                const auto anonid = r[0][0].as<long int>();

                //Update or create and populate the record.
                query.str(""); //Clear stringstream.
                query << "SELECT anonid FROM experimental_subseg_voxel_dose_stats";
                query << "  WHERE anonid = " << txn.quote(anonid);
                query << "  AND suspected_roi_name = " << txn.quote(cleanname);
                query << "  AND subseg_schedule = " << txn.quote(seghist) << ";";
                r = txn.exec(query.str());
                if(r.empty()){
                    //No existing record. Create and populate one.
                    query.str(""); //Clear stringstream.
                    query << "INSERT INTO experimental_subseg_voxel_dose_stats "; 
                    query << "  (anonid, suspected_roi_name, actual_roi_name, subseg_schedule, min, mean, median, max)";
                    query << "  VALUES (" << txn.quote(anonid);
                    query << "    , " << txn.quote(cleanname);
                    query << "    , " << txn.quote(name);
                    query << "    , " << txn.quote(seghist);
                    query << "    , " << txn.quote(min);
                    query << "    , " << txn.quote(mean);
                    query << "    , " << txn.quote(median);
                    query << "    , " << txn.quote(max);
                    query << "  ) RETURNING anonid;";
                    r = txn.exec(query.str());
                    if(r.empty()) FUNCERR("Unable to create record for '" << patientID << "'");
                }else{
                    //Existing record. Overwrite the statistics.
                    query.str(""); //Clear stringstream.
                    query << "UPDATE experimental_subseg_voxel_dose_stats SET ";
                    query << "  (actual_roi_name, min, mean, median, max)";
                    query << "  = (" << txn.quote(name);
                    query << "    , " << txn.quote(min);
                    query << "    , " << txn.quote(mean);
                    query << "    , " << txn.quote(median);
                    query << "    , " << txn.quote(max);
                    query << "  ) WHERE anonid = " << txn.quote(anonid);
                    query << "    AND suspected_roi_name = " << txn.quote(cleanname);
                    query << "    AND subseg_schedule = " << txn.quote(seghist);
                    query << "  RETURNING anonid ;";
                    r = txn.exec(query.str());
                    if(r.empty()) FUNCERR("Unable to create record for '" << patientID << "'");
                }
            }

            txn.commit(); //If this is not called, everything is rolled back!
        }catch(const std::exception &e){
            FUNCERR("Unable to push to database: " << e.what());
        }
/*
        //Same thing for the moments.
        ssnc.clear();
        for(auto m_it = moments.begin(); m_it != moments.end(); ++m_it){
            const auto name      = m_it->first->Raw_ROI_name;
            const auto ssn       = Xtostring<size_t>(ssnc[name]);
            const auto cleanname = X(name);

            for(auto mm_it = m_it->second.begin(); mm_it != m_it->second.end(); ++mm_it){
                const auto p = mm_it->first[0], q = mm_it->first[1], r = mm_it->first[2];
                if((p+q+r) > 3) continue;
                const auto pqr = Xtostring(p)+Xtostring(q)+Xtostring(r); //Three-digit moment 'key'
                const auto moment = mm_it->second;

                const auto col = CSVTools_Get_First_Col_With_Text_Or_Make_New(&thecsv, cleanname + " mu_"_s + pqr + " "_s + ssn);
                thecsv[row][col] = Xtostring(moment);
            }

            ++(ssnc[name]);
        }
*/
        return 0;
    }
}
//---------------------------------------------------------------------------------------------------------------------
//------------------------------------------- Launch Embedded Environment ---------------------------------------------
//---------------------------------------------------------------------------------------------------------------------
//Since many of the tasks we might want to perform could be better performed iteratively with feedback, embedding an 
// interpreter which can access the data will increase overall utility of the program.

// ... ChaiScript (header only)?    Lua ?   Python ?  Simple homebrew-type thing?   Readline ?
// ... some other IPC mechanism? (e.g., D-bus, kdbus)

//---------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------- Cleanup --------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------
//Try avoid doing anything that will require you to explicitly clean anything up here (ie. please use smart pointers).

    return 0;
}
