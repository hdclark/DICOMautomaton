//DICOMautomaton_Dump.cc -- A simple DICOM tag value extractor.
// This little program prints a tag's value formatted as a string.


#include <iostream>
//#include <vector>
#include <string>
//#include <utility>        //Needed for std::pair.
//#include <memory>         //Needed for std::unique_ptr.
//#include <algorithm>      //Needed for std::sort.
//#include <list>
//#include <tuple>
//#include <map>

//#pragma GCC diagnostic push
//#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
//#pragma GCC diagnostic ignored "-Wunused-but-set-parameter"
//#include "imebra.h"
//#pragma GCC diagnostic pop

//#include "YgorMisc.h"       //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
//#include "YgorMath.h"       //Needed for 'vec3' class.
//#include "YgorString.h"     //Needed for Canonicalize_String2().

//#include "Structs.h"
#include "Imebra_Shim.h"
bool VERBOSE = false;  //Provides additional information.

int main(int argc, char **argv){


    if(argc != 4){
        FUNCERR("Usage: " << argv[0] << " <filename>  <tag_l>  <tag_u> \n"
                " For example, " << argv[0] << " /tmp/test.dcm 0x0008 0x0060\n"
                " Warning: this is a simplistic, one-off program!");
    }

    try{

        const std::string Filename(argv[1]);

        //Convert tag inputs to numbers.
        unsigned int U = std::stoul(argv[2], nullptr, 16);    
        unsigned int L = std::stoul(argv[3], nullptr, 16);    

        std::cout << get_tag_as_string(Filename, U, L) << std::endl;

        return 0;
    }catch(const std::exception &e){
        std::cerr << e.what() << std::endl;
    }
    return 1;
}

