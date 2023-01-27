//Boost_Serialization_Archive_Converter.cc - A part of DICOMautomaton 2016. Written by hal clark.
//
// This program can be used to convert Boost.Serialization archives between a variety of formats.
//

#include <boost/algorithm/string/predicate.hpp>
#include <filesystem>
#include <fstream>
#include <functional>
#include <list>
#include <map>
#include <stdexcept>
#include <string>    
#include <utility>

#include "Structs.h"
#include "YgorArguments.h"    //Needed for ArgumentHandler class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorLog.h"
//#include "YgorMath.h"         //Needed for vec3 class.
//#include "YgorImagesIO.h"

#include "Common_Boost_Serialization.h"







int main(int argc, char* argv[]){


    //Default output conversion type.
    std::string ConvertTo("gzip-txt");

    //We cannot assume this is any specific object. It could be a Drover, a Contour_Data, etc..
    std::filesystem::path FilenameIn;

    //The file extension is completely ignored at the moment.
    std::filesystem::path FilenameOut;


    //Drover conversion routines.
    using drover_serial_func_t = std::function<bool (const Drover &, std::filesystem::path)>;
    std::map<std::string, drover_serial_func_t> drover_serial_func_name_mapping;

    drover_serial_func_name_mapping["gzip-binary"] = Common_Boost_Serialize_Drover_to_Gzip_Binary;
    drover_serial_func_name_mapping["gzip-txt"] = Common_Boost_Serialize_Drover_to_Gzip_Simple_Text;
    drover_serial_func_name_mapping["gzip-xml"] = Common_Boost_Serialize_Drover_to_Gzip_XML;

    drover_serial_func_name_mapping["binary"] = Common_Boost_Serialize_Drover_to_Binary;
    drover_serial_func_name_mapping["txt"] = Common_Boost_Serialize_Drover_to_Simple_Text;
    drover_serial_func_name_mapping["xml"] = Common_Boost_Serialize_Drover_to_XML;

    Drover DICOM_data;

    

    //================================================ Argument Parsing ==============================================

    class ArgumentHandler arger;
    const std::string progname(argv[0]);
    arger.examples = { { "--help", 
                         "Show the help screen and some info about the program." },
                       { "-i file.binary.gz -o file.xml -t 'XML'",
                         "Convert to a text XML file." },
                       { "-i file.binary.gz -o file.xml.gz -t 'gzip-xml'",
                         "Convert to a gzipped text XML file." },
                       { "-i file.binary -o file.xml -t 'XML'",
                         "Convert to a text XML file." },
                       { "-i file.xml.gz -o file.txt -t 'txt'",
                         "Convert to a simple text file." },
                       { "-i file.txt -o file.txt.gz -t 'gzip-txt'",
                         "Convert to a gzipped simple text file. (Same as simply `gzip file.txt`)" },
                       { "-i file.xml.gz -o file.bin -t 'binary'",
                         "Convert to a binary file." },
                       { "-i file.xml.gz -o file.bin.gz -t 'gzip-binary'",
                         "Convert to a gzipped binary file." }
                     };
    arger.description = "A program for converting Boost.Serialization archives types which DICOMautomaton can read.";

    arger.default_callback = [](int, const std::string &optarg) -> void {
      YLOGERR("Unrecognized option with argument: '" << optarg << "'");
      return; 
    };
    arger.optionless_callback = [](const std::string &optarg) -> void {
      YLOGERR("Unrecognized option with argument: '" << optarg << "'");
      return; 
    };

    arger.push_back( ygor_arg_handlr_t(0, 'i', "input", true, "file.bin.gz",
      "Input filename.",
      [&](const std::string &optarg) -> void {
        FilenameIn = optarg;
        return;
      })
    );
 
    arger.push_back( ygor_arg_handlr_t(1, 'o', "output", true, "file.XYZ",
      "Output filename.",
      [&](const std::string &optarg) -> void {
        FilenameOut = optarg;
        return;
      })
    );

    arger.push_back( ygor_arg_handlr_t(2, 't', "output-type", true, ConvertTo,
      "The format to convert to. Supported: gzip-binary, gzip-txt, gzip-xml, binary, txt, xml.",
      [&](const std::string &optarg) -> void {
        ConvertTo = optarg;
        return;
      })
    );

    arger.Launch(argc, argv);


    //============================================== Input Verification ==============================================

    

    try{
        bool wasOK;
        FilenameIn = std::filesystem::canonical(FilenameIn);
        wasOK = std::filesystem::exists(FilenameIn);
        if(!wasOK) throw std::runtime_error("File does not exist or is not reachable.");
    }catch(const std::filesystem::filesystem_error &e){ 
        YLOGERR("Unable to open input file: " << e.what());
        return 1;
    }

    try{
        bool wasOK;
        FilenameOut = std::filesystem::canonical(FilenameOut);
        wasOK = std::filesystem::exists(FilenameOut);
        if(wasOK){
            YLOGERR("Specified output file " << FilenameOut << " exists. Refusing to overwrite");
            return 1;
        }
    }catch(const std::filesystem::filesystem_error &){ }


    //Parse into a Drover.
    Drover A;
    if(!Common_Boost_Deserialize_Drover(A,FilenameIn)){
        YLOGERR("Unable to parse input file");
    }

    //If it was a Drover, we made it this far. Write in the desired format.
    for(const auto &op_func : drover_serial_func_name_mapping){
        if(boost::iequals(op_func.first,ConvertTo)){
            if(!op_func.second(A,FilenameOut)){
                YLOGERR("Unable to write output file");
            }else{
                YLOGINFO("Success");
                return 0;
            }
        }
    }

    YLOGERR("Conversion failed: output format not recognized");
    return 1;
}
