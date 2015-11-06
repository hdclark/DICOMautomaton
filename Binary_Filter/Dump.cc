#include <iostream>
#include <iomanip>
#include <fstream>
#include <map>

#include <YgorMisc.h>


bool Is_Common_ASCII( const unsigned char &in ){
    return isininc((unsigned char)(32),in,(unsigned char)(126));
}


void Dump_Binary_File(const std::string &filename_in, const std::string &filename_out, bool rep1, bool rep2, bool rep3, bool rep4){
    //Load the file into memory.
    std::ifstream in(filename_in.c_str(), std::ios::in | std::ios::binary | std::ios::ate);
    if(!in.is_open()){
         FUNCERR("Unable to load file");
    }

    std::ifstream::pos_type size = in.tellg();  //Grab the size of the binary file.
    unsigned char *memblock = new unsigned char [size];   //Allocate space for the entire block to be pulled into memory.
    in.seekg(0, std::ios::beg);                 //Seek back to the beginning.
    in.read((char *)(memblock), size);                    //Read the entire file in (in one go.)
    in.close();

    std::cout << "File (size = " << size << ") has been read into memory." << std::endl;

    //Dump the output to another file (In case we decide to directly output the uncommon ASCII chars..)
    std::ofstream out(filename_out.c_str(), std::ios::out );

/*
    if(rep1){
        //Output everything except '\0'. Instead of printing them, break on them.
        for(size_t i = 0; i < size; ++i){
            if(memblock[i] == '\0'){
                out << std::endl;
            }else{
                out << memblock[i];
            }
        }
    }
*/

/*
if(rep2){
    //Only print out characters which are common punctuation or alphanumeric. Print a newline when a switch occurs.
    //Print the decimal value of the non-common ascii chars in brackets beside the chars.
    char last = '\0';
    for(size_t i = 0; i < size; ++i){
        if(Is_Common_ASCII(memblock[i]) != Is_Common_ASCII(last)){
            out << std::endl;
        }

        if(Is_Common_ASCII(memblock[i])){
            out << memblock[i];
        }else{
            out << memblock[i] << "(" << (unsigned int)(memblock[i]) << ")";
        }

        last = memblock[i];
    }
}
*/


    //Cycle through the buffer once. Catch switches from common-ascii to non-common, and store the snippets. Count the 
    // number of times each snippet occurs in the file. Re-cycle through and preceed each snippet with the number of 
    // times the snippet has occurred in the file.
    std::map<std::string,long int> snippets;

    unsigned char last = '\0';
    std::string current_snippet;
    for(size_t i = 0; i < size; ++i){
 
        //If a switch has occured.
        if( (current_snippet.size() != 0) 
                     && ((Is_Common_ASCII(memblock[i]) != Is_Common_ASCII(last)) || ((i+1)==size)) ){
            snippets[ current_snippet ] += 1;
            current_snippet.clear();
        }

        current_snippet.push_back( memblock[i] );
        last = memblock[i];
    }



    last = '\0';
    current_snippet.clear();
    for(size_t i = 0; i < size; ++i){

        //If a switch has occured or we are out of data.
        if( (current_snippet.size() != 0)
                     && ((Is_Common_ASCII(memblock[i]) != Is_Common_ASCII(last)) || ((i+1)==size)) ){

            if(rep1){
            //Print out the occurence rate.
                out << "FREQ ";
                out << std::setw(4);
                out << std::setfill(' ');
                out << snippets[current_snippet];
                out << "x: ";
            }

            if(rep2){
                //Print out a unique ID so we can quickly and easily identify like-portions of the output.
                auto it = snippets.find(current_snippet);
                if(it != snippets.end() ){
                    out << "ID_";
                    out << std::setw(4);
                    out << std::setfill('_');
                    out << std::distance(snippets.begin(), it);
                }
                out << ": ";
            }    

            if(rep3){
                //Prints the size of the data.
                out << "size: ";
                out << std::setw(4);
                out << std::setfill(' ');
                out << current_snippet.size();
                out << " ";
            }    

            if(rep4){
                //Prints out whether the data was common ASCII or not.
                if( Is_Common_ASCII(current_snippet[0]) ){
                    out << "STR: ";
                }else{
                    out << "BIN: ";
                }
            }

            //If the snippet is common ASCII, we simply dump it to screen. 
            if( Is_Common_ASCII(current_snippet[0]) ){
                out << current_snippet << std::endl;

            //Otherwise, we convert the characters to hex so that we won't have any troubles viewing the output.
            }else{
                for(size_t j=0; j<current_snippet.size(); j++){
//                    out << std::setfill('0') << std::setw(2) << std::hex << (unsigned int)((unsigned char)(current_snippet[j])) << " ";
                    out << std::setfill('0') << std::setw(3) << std::dec << (unsigned int)((unsigned char)(current_snippet[j])) << " ";

                }
                out << std::dec << std::endl;

            }

            current_snippet.clear();
        }

        current_snippet.push_back( memblock[i] );
        last = memblock[i];
    }

    out.close();
    
    delete[] memblock;
    return;
}

int main(int argc, char **argv){

    Dump_Binary_File("RS1.dcm", "RS1_entire.out", true, true, true, true);
    Dump_Binary_File("RS2.dcm", "RS2_entire.out", true, true, true, true);

    Dump_Binary_File("CT3.dcm", "CT3_entire.out", true, true, true, true);


    Dump_Binary_File("RS1.dcm", "RS1_meld.out", false, false, false, false);
    Dump_Binary_File("RS2.dcm", "RS2_meld.out", false, false, false, false);


    
    
    return 0;
}
