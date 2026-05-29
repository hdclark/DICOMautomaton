//DICOMautomaton_Dump.cc -- A simple DICOM tag value extractor.
// This little program prints a tag's value formatted as a string.

#include <YgorMisc.h>
#include "YgorLog.h"
#include <exception>
#include <iostream>
#include <string>

#include "Imebra_Shim.h"

int main(int argc, char **argv){
    if(argc != 4){
        YLOGERR(argv[0] << " -- a simple DICOM tag value extractor.\n"
                " Usage: " << argv[0] << " <filename> <tag_u> <tag_l>\n"
                " For example, " << argv[0] << " /tmp/file.dcm 0x0008 0x0060\n");
    }

    try{
        const std::string Filename(argv[1]);

        //Convert tag inputs to numbers.
        unsigned int U = std::stoul(argv[2], nullptr, 16);    
        unsigned int L = std::stoul(argv[3], nullptr, 16);    

        const auto val = get_tag_as_string(Filename, U, L);

        // Write the full contents to stdout, including any null bytes.
        std::cout.write(val.data(), val.size());
        std::cout << std::endl;

        return 0;
    }catch(const std::exception &e){
        std::cerr << e.what() << std::endl;
    }
    return 1;
}

