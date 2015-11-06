#include <iostream>
#include <iomanip>
#include <fstream>
#include <map>

#include <YgorMisc.h>

union small {
    uint16_t i;
    unsigned char c[2];
};

union large {
    uint32_t i;
    unsigned char c[4];
    small s[2];
};

bool Is_Common_ASCII( const unsigned char &in ){
    return isininc((unsigned char)(32),in,(unsigned char)(126));
}

bool operator==( const large &L, const large &R ){
    return L.i == R.i;
}

bool operator==( const small &L, const small &R ){
    return L.i == R.i;
}


std::ostream & operator<<( std::ostream &out, const large &in ){
    out << in.i << " (" << (unsigned int)(in.c[0]) << "," << (unsigned int)(in.c[1]) << "," << (unsigned int)(in.c[2]) << "," << (unsigned int)(in.c[3]) << ")";
    return out;
}

std::ostream & operator<<( std::ostream &out, const small &in ){
    out << in.i << " (" << (unsigned int)(in.c[0]) << "," << (unsigned int)(in.c[1]) << ")";
    return out;
}


//Special items.
small UL;



void Parse_Binary_File(const std::string &filename_in, const std::string &filename_out, bool rep1, bool rep2, bool rep3, bool rep4){
    //Load the file into memory.
    std::ifstream in(filename_in.c_str(), std::ios::in | std::ios::binary | std::ios::ate);
    if(!in.is_open()){
         FUNCERR("Unable to load file");
    }

    std::ifstream::pos_type size = in.tellg();  //Grab the size of the binary file.
    unsigned char *mem = new unsigned char [size];   //Allocate space for the entire block to be pulled into memory.
    in.seekg(0, std::ios::beg);                 //Seek back to the beginning.
    in.read((char *)(mem), size);                    //Read the entire file in (in one go.)
    in.close();

    std::cout << "File (size = " << size << ") has been read into memory." << std::endl;

    //Dump the output to another file (In case we decide to directly output the uncommon ASCII chars..)
    std::ofstream out(filename_out.c_str(), std::ios::out );

    //-----------------------------------------------parsing the data-----------------------------------------------------
    std::basic_string<unsigned char> buff; //Stores temporary things.
    large shuttle;


    //First, we continually read until we have read 4 consecutive non-\0 characters. Check that they are 'DICM'. 
    auto space = mem+4, dummy = space;
    large target;     target.c[0] = 'D';     target.c[1] = 'I';     target.c[2] = 'C';     target.c[3] = 'M';
    for( ; space != (mem+size); space++){
        dummy = space - 4;
        shuttle.c[0] = *(dummy++);
        shuttle.c[1] = *(dummy++);
        shuttle.c[2] = *(dummy++);
        shuttle.c[3] = *(dummy); 

        if( (shuttle.c[0] != '\0') && (shuttle.c[1] != '\0') && (shuttle.c[2] != '\0') && (shuttle.c[3] != '\0') ){
            if(shuttle.i = target.i){
                //Found it!
                break;
            }else{
                FUNCWARN("Unable to find 'DICM' in the header.");
                return;
            }
        }
    }

    //Now we have bounds for the effective length of the data within the file.
    const unsigned char *begin = space;
    const unsigned char *end   = mem + size;

    //We do a read-interpret-read_data loop until we are at the end.
    auto i = begin;
    while(i < end){
        //Clear our storage from last time.
        buff.clear();
 
        //Read in four bytes. This is the identifier for whatever we are about to read.
        shuttle.c[0] = *(i++);  shuttle.c[1] = *(i++); shuttle.c[2] = *(i++);  shuttle.c[3] = *(i++);
        const large ident = shuttle;

        //Read in the next four bytes. Do so two at a time. These vary in interpretation.
        shuttle.c[0] = *(i++);  shuttle.c[1] = *(i++); shuttle.c[2] = *(i++);  shuttle.c[3] = *(i++);
        const small first = shuttle.s[0];
        const small secnd = shuttle.s[1];

        out << "ident = " << ident << ",  first = " << first << ",  secnd = " << secnd;

        //We now determine whether there is any data associated with this item or not. Some items *appear* to have data associated
        // with them, but in fact do not. Such an item is the 'UL' item (File Meta Information Group Length.)
        long int amount = 0; 

        //  If the first two characters were 'Common ASCII' characters, we have to deal with possibly-different syntax. 
        if( Is_Common_ASCII(first.c[0]) && Is_Common_ASCII(first.c[1]) ){
            //Now the next two bits denote the amount of data to read. If they are zero, then the next 4 bytes denote the amount of data (maybe it is too much for two bytes??)
            long int maybe_amount = static_cast<long int>(secnd.i);

            if(maybe_amount == 0){
                shuttle.c[0] = *(i++);  shuttle.c[1] = *(i++); shuttle.c[2] = *(i++);  shuttle.c[3] = *(i++);
                maybe_amount = static_cast<long int>(shuttle.i);
            }
            amount = maybe_amount;

            //Handle special cases.
//            if( first == UL ) amount = 0;
 
            // ...


//   What about the case when first and second are non-empty, but first does NOT contain ASCII ?  How to handle that?

        //Otherwise the entire 4 bytes denote the number of bytes to read in.
        }else{
            amount = static_cast<long int>(shuttle.i);
        }


        //If there is data to be read in, do so.
        for(long int j=0; j<amount; ++j){
            buff.push_back( *(i++) );
        }

        

        //Print the data.
        if(amount != 0){
            out << ",  data = \"";
            for(size_t x=0; x<buff.size(); ++x) out << (char)(buff[x]);
            out << "\"";
        }
        out << std::endl;
        
        


    }

   
    //---------------------------------------------------------------------------------------------------------------------
    out.close(); 
    delete[] mem;
    return;
}

int main(int argc, char **argv){
    //Set special items.
    UL.c[0] = (char)('U');   UL.c[1] = (char)('L');




    Parse_Binary_File("RS1.dcm", "RS1_parsed.out", true, true, true, true);
    Parse_Binary_File("CT3.dcm", "CT3_parsed.out", true, true, true, true);


    
    
    return 0;
}
