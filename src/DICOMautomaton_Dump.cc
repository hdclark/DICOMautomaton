//DICOMautomaton_Dump.cc -- A simple DICOM tag value extractor.
// This little program prints a tag's value formatted as a string.

#include <YgorMisc.h>
#include <exception>
#include <iostream>
#include <string>

#include "Imebra_Shim.h"

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

