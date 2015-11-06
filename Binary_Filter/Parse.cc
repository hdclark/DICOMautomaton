#include <iostream>
#include <iomanip>
#include <fstream>
#include <map>
#include <memory>
#include <vector>

#include "YgorMisc.h"
#include "YgorFilesDirs.h"



std::basic_string<unsigned char>  Simple_DICOM_Header( void ){
    std::basic_string<unsigned char> out;
    for(size_t i=0; i<128; ++i) out += static_cast<unsigned char>('\0');

    out += static_cast<unsigned char>('D');
    out += static_cast<unsigned char>('I');
    out += static_cast<unsigned char>('C');
    out += static_cast<unsigned char>('M');
    return out;
}


union small {
    uint16_t i;
    unsigned char c[2];
};

union large {
    uint32_t i;
    unsigned char c[4];
    small s[2];
};

bool Is_Common_ASCII( const unsigned char &in ){     return isininc((unsigned char)(32),in,(unsigned char)(126)); }
bool operator==( const large &L, const large &R ){   return L.i == R.i;  }
bool operator==( const small &L, const small &R ){   return L.i == R.i;  }

std::ostream & operator<<( std::ostream &out, const large &in ){

    //Dump as a single four-byte integer.
    out << std::setfill(' ') << std::setw(10) << in.i;

    //Dump as 4 one-byte integers.
    out << " (";
    out << std::setfill('0') << std::setw(3) << (unsigned int)(in.c[0]) << ",";
    out << std::setfill('0') << std::setw(3) << (unsigned int)(in.c[1]) << ",";
    out << std::setfill('0') << std::setw(3) << (unsigned int)(in.c[2]) << ",";
    out << std::setfill('0') << std::setw(3) << (unsigned int)(in.c[3]) << ")";

    //Dump as 2 two-byte integers (in hex.)
    out << " (";
    out << std::setfill('0') << std::setw(4) << std::hex << in.s[0].i << ",";
    out << std::setfill('0') << std::setw(4) << std::hex << in.s[1].i << ")";

    out << std::dec;
    return out;
}

std::ostream & operator<<( std::ostream &out, const small &in ){

    //Dump as a single two-byte integer.
    out << std::setfill(' ') << std::setw(8) << in.i;

    //Dump as 2 one-byte integers.
    out  << " (";
    out << std::setfill('0') << std::setw(3) << (unsigned int)(in.c[0]) << ",";
    out << std::setfill('0') << std::setw(3) << (unsigned int)(in.c[1]) << ")";

    //Dump as a single two-byte integer (in hex.)
    out << " (";
    out << std::setfill('0') << std::setw(4) << std::hex << in.i << ")";

    out << std::dec;
    return out;
}

class piece {
    public:
        large A;
        large B;

        long int data_size;
        std::basic_string<unsigned char> data; 

        std::vector<piece> child;   //This is the data, delinearized into sequential items.
};

std::ostream & operator<<( std::ostream &out, const std::basic_string<unsigned char> &in ){
    for(size_t x=0; x<in.size(); ++x) out << (char)(in[x]);
    return out;
}

std::ostream & operator<<( std::ostream &out, const piece &in ){
    //Print the simple parts.
    out << "A = " << in.A << ",  B(first) = " << in.B.s[0] << ",  B(secnd) = " << in.B.s[1];

    //Print the data.
    if(in.data.size() != 0){
        out << ",  data = \"";
        out << in.data;
        out << "\"";
    }
    return out;
}

//This is a sort of heuristic function - it attempts to 'guess' whether or not the given B denotes (only) the size of a pieces' data.
// Some elements require an extra 4 bytes (a pseudo 'C' parameter) to denote the size of the accompanying data.
bool Does_A_B_Not_Denote_A_Size( const large &A, const large &B ){

    //Special cases with precedent, found via trial and error.
    // -> return "false" to force the "B" to be equal to the size of the data.
    // -> return "true"  to check (elsewhere) whether the last two bytes of B are the size, or the next four bytes are the size.

    if( A.i == static_cast<uint32_t>(65538)       ) return true;  

    if( A.i == static_cast<uint32_t>(5255174)     ) return false;    //Original: Found in RS files - I think these exclusively denote stringified contour data ( "1.23/2.34/-5.56/1.24/2.45/..." )  Typically from the 'BODY' contour.
    if( A.i == static_cast<uint32_t>(3758161918)  ) return false;    //Original: Found in RS files - I think these are 'elements with a length of data' or something.

    if( A.i == static_cast<uint32_t>(1081312)     ) return false;    //Found in CT files - Used to denote the image data, I think.

    if( A.i == static_cast<uint32_t>(1191942)     ) return false;    //Found in RS files - GDCMdump gives "RT Referenced Study Sequence"
    if( A.i == static_cast<uint32_t>(4206598)     ) return false;    //Found in RS files - GDCMdump gives "Contour Sequence"


if( A.i == static_cast<uint32_t>(1060870)     ) return false;    //Found in RS files - GDCMdump gives "Referenced Frame of Reference Sequence"
if( A.i == static_cast<uint32_t>(3747846)     ) return false;    //Found in RS files - GDCMdump gives "ROI Contour Sequence"
if( A.i == static_cast<uint32_t>(1454086)     ) return false;    //Found in RS files - GDCMdump gives "Contour Image Sequence"

//Untested - likely matches.
if( A.i == static_cast<uint32_t>(1323014)     ) return false;    //Found in RS files - 3006,0014 





    //General scheme: Do the first two bytes of B look like a two-char identifier? (ie. UL, RS, DE, etc..)
    if( Is_Common_ASCII(B.c[0])  &&  Is_Common_ASCII(B.c[1])  ){
        return true;
    }
    return false;
}

//This is a sort of heuristic function - it attempts to 'guess' whether or not the given B contains a two-byte 'data length' integer.
// This is a typical thing for a few elements near the header, but is rarely (if ever?) found in the body of a DICOM file. 
bool Do_Last_Two_Bytes_of_B_Denote_A_Size( const large &A, const large &B ){
    //These items have been found to have the last two bytes of B denote the size of the data.
    //
    //This is a whitelist.
    if( 
        (A.i == static_cast<uint32_t>(2))            //Original: Determined using RTSTRUCT files.
        ||
        (A.i == static_cast<uint32_t>(131074))       //Original: Determined using RTSTRUCT files.
        ||
        (A.i == static_cast<uint32_t>(196610))       //Original: Determined using RTSTRUCT files.
        ||
        (A.i == static_cast<uint32_t>(1048578))      //Original: Determined using RTSTRUCT files.
        ||
        (A.i == static_cast<uint32_t>(1179650))      //Original: Determined using RTSTRUCT files.
      ) return true;
    return false;
}

//This is a sort of heuristic function - it attempts to 'guess' whether or not the given A and B will be followed by a four byte 
// 'data length' integer. This is a fairly rare case, it seems.
bool Do_Next_Four_Bytes_Denote_A_Size( const large &A, const large &B ){
    //These items have been found to have the next four bytes denote the size of the data.
    //
    //This is a whitelist.
    if(  
        (A.i == static_cast<uint32_t>(65538))        //Original: Determined using RTSTRUCT files.
//||
//(A.i == static_cast<uint32_t>(1081312))

      ) return true;
    return false;
}

//This is a sort of heuristic function - it attempts to 'guess' whether or not the given element contains data which should be 
// expanded into children. Such data either can or cannot: some data is data like "a string" and some contains nested elements.
bool Can_This_Elements_Data_Be_Delineated( const piece &in ){
    if(
          //These are required to be true for any element.   
          //
          //NOTE: All other logic blocks must && with this one. 
            (
//                (in.data.size() != 0)     //This stops from attempting to expand elements with less than 8 bytes (ie. not enough room for an "A" and a "B"!)
//            &&
                (in.data.size() >= 8)     //This stops from attempting to expand elements with less than 8 bytes (ie. not enough room for an "A" and a "B"!)
            &&
                (in.child.size() == 0)    //This means it has already been delineated - we do not want to (possibly over-) write data in this case.
            )
          //These are basic filters. We can not whitelist, but we can override elements within this block.
          // For example, when A = 3758161918, the first char of the data is NOT common ASCII, but this 
          // criteria works well for most other blocks. 
          &&
            (
                !Is_Common_ASCII(in.data[0])
            ||
                ( in.A.i == static_cast<uint32_t>(3758161918) )
            )
          //Exceptions.These items *appear* to have valid data, but do not.
          //
          //NOTE: This is a blacklist!
          &&
            (
                ( in.A.i != static_cast<uint32_t>(65538) )         //Original: Found in RTSTRUCT file.
            &&
                ( in.A.i != static_cast<uint32_t>(2) )             //Original: Found in RTSTRUCT file.
            &&
                ( in.A.i != static_cast<uint32_t>(1081312) )       //Found in CT file -> holds image data.
            )
      ) return true;
    return false;
}


//Given iterators to the loaded file, we attempt to locate the 'DICM' where we expect it.
// If it is found, an iterator to the memory immediately after the 'DICM' is returned, otherwise
// we simply return end.
unsigned char * Validate_DICOM_Format(const unsigned char *begin, const unsigned char *end){
    //If the memory is too small, we couldn't possibly find it.
    if( std::distance(begin, end) < 4 ) return (unsigned char *)(end);

    large shuttle;

    //We continually read until we have read 4 consecutive non-\0 characters. Check that they are 'DICM'. 
    unsigned char *space = (unsigned char *)(begin)+4, *dummy = space;
    large target;     target.c[0] = 'D';     target.c[1] = 'I';     target.c[2] = 'C';     target.c[3] = 'M';
    for( ; space != end; space++){
        dummy = space - 4;
        shuttle.c[0] = *(dummy++);
        shuttle.c[1] = *(dummy++);
        shuttle.c[2] = *(dummy++);
        shuttle.c[3] = *(dummy);

        if( (shuttle.c[0] != '\0') && (shuttle.c[1] != '\0') && (shuttle.c[2] != '\0') && (shuttle.c[3] != '\0') ){
            if(shuttle.i == target.i){
                //Found it!
                return space;
            }else{
                //Reaching this point may not be considered a failure - there may be 'noise' prior to reaching the 'DICM', but this
                // assumes that there is nothing in the file (other that \0's) prior to the 'DICM'.
                FUNCWARN("Unable to find 'DICM' in the memory region supplied");
                return (unsigned char *)(end);
            }
        }
    }
    return (unsigned char *)(end);
}


//Breaks a piece of memory into a sequence of 'pieces' (which are specific to the DICOM RTSTRUCT format.)
// The region of memory is not directly passed in, but rather is passed in via ~iterators to the beginning 
// and end of the memory block.
//
//NOTE: The data passed to this routine *needs* to be free of the 'DICM' magic file number.
std::vector<piece> Parse_Binary_File(const unsigned char *begin, const unsigned char *end){
    std::vector<piece> out;

    std::basic_string<unsigned char> buff; //Stores temporary data.
    large shuttle;

    //We do a read-interpret-read_data loop until we are at the end.
    auto i = begin;
    while(i < end){
        piece outgoing;

        //Clear our storage from last time.
        buff.clear();
 
        //Read in four bytes. This is the identifier for whatever we are about to read.
        shuttle.c[0] = *(i++);  shuttle.c[1] = *(i++); shuttle.c[2] = *(i++);  shuttle.c[3] = *(i++);
        outgoing.A = shuttle;

        //Read in the next four bytes. Do so two at a time. These vary in interpretation.
        shuttle.c[0] = *(i++);  shuttle.c[1] = *(i++); shuttle.c[2] = *(i++);  shuttle.c[3] = *(i++);
        outgoing.B = shuttle;

        //We now determine whether there is any data associated with this item or not. Some items *appear* to have data associated
        // with them, but in fact do not. Such an item is the 'UL' item (File Meta Information Group Length.)
        long int amount = 0; 

        //This criteria is a very brittle part of this program. It tries to guess which data needs to be delineated and
        // broken into children. It uses heuristics which are built by patching previously encountered warnings and failures!
        //
        //There are three OBSERVED possibilities here:
        // 1) The size is all 4 bytes of B (most common, espescially after the first few items near the header (compatability mode or something?) )
        // 2) The last two bytes of B denote the size. I've only seen this for a handful of elements, so I've used a whitelist function for it.
        // 3) The next 4 bytes (not yet read) must contain the size. This is the most rare. As far as I know, only one element does this

        if( Does_A_B_Not_Denote_A_Size( outgoing.A, outgoing.B ) ){
            //Now there are TWO OBSERVED possibilities:
            // 2) the last two bytes of B denote the size.
            if( Do_Last_Two_Bytes_of_B_Denote_A_Size( outgoing.A, outgoing.B ) ){
                //Grab the size from the last two bytes.
                amount = static_cast<long int>(outgoing.B.s[1].i);

            // 3) the next 4 bytes (not yet read) must contain the size.
            }else if( Do_Next_Four_Bytes_Denote_A_Size( outgoing.A, outgoing.B ) ){
                //Read ahead 4 more bytes to get the size.
                shuttle.c[0] = *(i++);  shuttle.c[1] = *(i++); shuttle.c[2] = *(i++);  shuttle.c[3] = *(i++);
                amount = static_cast<long int>(shuttle.i);

            // The OTHER possibility - we encounter an element which may need to be whitelisted as either of the THREE above possibilities!
            }else{
                FUNCWARN("Encountered an item which we have not previously encountered: A = " << outgoing.A << " and B = " << outgoing.B << ".");
                FUNCWARN("  Please determine how to read the element and add it to the appropriate whitelist function.");
                FUNCWARN("  Guessing how the item should be treated. Search the source for tag [ WWWW1 ] for more info.");

                //NOTE: The below code is a poor attempt at GUESSING how the item works. Expect errors. It is not necessarily (likely) how the item should be treated.
                //Attempt to "guess" the second probability, because it seems like a more common channel.. The appropriate whitelist in this case is Do_Last_Two_Bytes_of_B_Denote_A_Size(...)
                FUNCWARN("  If no additional warnings/errors are encountered, consider adding this item to the appropriate whitelist (and then test it!)");
                amount = static_cast<long int>(outgoing.B.s[1].i);
            }

        //Otherwise the entire 4 bytes denote the number of bytes to read in.
        // This is case 1).
        }else{
            amount = static_cast<long int>(shuttle.i);
        }

        //Perform a sanity check - is there enough data left to make this data sane?
        // If there is not, it is *not* safe to continue processing data. We *have* to return (with a warning.)
        const long int total_remaining_space = static_cast<long int>( std::distance(i,end) );
        if( total_remaining_space < amount ){
            FUNCWARN("We have interpreted an instruction to read memory of capacity beyond what we have loaded into memory during parsing. This _may_ or _may not_ be an error");
            FUNCWARN("  NOTE: The heuristic we use to 'guess' the proper size to load in can get snagged on elements with a large size. Try whitelisting the A value in the various whitelists");
            FUNCWARN("  NOTE: This element had A = " << outgoing.A << " and B = " << outgoing.B << ". We have attempted to read " << amount << " bytes when there was  " << total_remaining_space << " space remaining");
            return out;
        }

        outgoing.data_size = amount;

        //If there is data to be read in, do so.
        for(long int j=0; j<amount; ++j){
            buff.push_back( *(i++) );
        }

        outgoing.data = buff;

        out.push_back(outgoing);
    }

    return out;
}


//This routine walks over a chunk of de-lineated data and de-lineates all children data streams.
// It is called recursively until all elements have been delineated.
void Delineate_Children(std::vector<piece> &in){
    for(size_t i=0; i<in.size(); ++i){
        //If there is data in the string buffer, and there is not already a child node, and the data *appears*
        // to be able to be expanded, then we attempt to do so.
        if( Can_This_Elements_Data_Be_Delineated( in[i] ) ){
            in[i].child = Parse_Binary_File(  in[i].data.c_str(), in[i].data.c_str() + in[i].data.size() );

            //Now call this routine with the recently created data.
            Delineate_Children( in[i].child );

        //Otherwise, if there is already a child, then check it.
        }else if( (in[i].child.size() != 0) ){
            Delineate_Children( in[i].child );

        //Otherwise, this element has no data to process, has no attached nodes, and is fully expanded.
        //Simply move on to the next element.
        }else{
            continue;
        }
    }
    return;
}


//Dumps the entire vector, recursively dumping all children when they are encountered. Prefixes the data
// with spaces to denote the depth of the node.
void Dump_Children(std::ostream & out, const std::vector<piece> &in, const std::string space = ""){
    for(size_t i=0; i<in.size(); ++i){
        //Print out the identity of this piece. If there is no child node, print the data.
        out << space;
        out << "A = " << in[i].A << " ";
        out << "B = " << in[i].B << " ";
        out << "S = " << std::setfill(' ') << std::setw(9) << in[i].data_size << " ";
        if(in[i].child.size() != 0){
            out << " [HAS_CHILD] " << std::endl;
            Dump_Children(out, in[i].child, (space + "    "));
        }else if(in[i].data.size() == 0){
            out << " [_NO_DATA_] " << std::endl;
        }else{
            out << " data = \"" << in[i].data  << "\"" << std::endl;
        }
    }
    return;
}

void Dump_Children(std::ostream & out, const std::vector<piece *> &in, const std::string space = ""){
    for(size_t i=0; i<in.size(); ++i) Dump_Children(out, { *(in[i]) } ); //Vector initializer to turn out of the element.
    return;
}


//Given a vector of nodes, we cycle through (recursively) and pick out the elements which match the key at a given depth.
// For instance, if we want to look for the (fictional) element   { 123, 456, 789 } then at depth 0 we look for all elements
// corresponding to 123, recurse and look for 456, and then recurse and look for 789. 
//
//NOTE: Only the final element(s) are returned. They are NOT embedded at the depth we find them.
//NOTE: Wildcards ("0") can be at any depth.
//NOTE: we do not make "in" const because we do not want const pointers to the data - we want it to be mutable!
void Get_Elements(std::vector<piece *> &out, std::vector<piece> &in, const std::vector<uint32_t> &key, const uint32_t depth = 0 ){
    for(size_t i=0; i<in.size(); ++i){

        //If we have found a match,
        if((in[i].A.i == key[depth]) || (key[depth] == 0)){

            //If this is the final element in the key, then we simply collect the piece and move on.
            if( (depth + 1) == key.size() ){
                out.push_back( &(in[i]) );

            //Otherwise, we have to see if the current element has any children nodes. If it does, recurse, otherwise continue.
            }else if( in[i].child.size() != 0 ){
                Get_Elements(out, in[i].child, key, depth+1);
            }

        //Otherwise, just carry on.
        }else{
            //Do nothing...
        }
    }
    return;
}


//This function strips out data from nodes which have children and resets the "data_size" member to -1.
void Prep_Children_For_Recompute_Children_Data_Size( std::vector<piece> &in ){
    for(size_t i=0; i<in.size(); ++i){
        //NOTE: Do NOT adjust the B parameter here - even if it contains the size info. This will be done when it is needed!

        //Reset the size of each node. This indicates that we need to recompute it.
        in[i].data_size = -1;

        //If there are children nodes, we clear the data and recurse.
        if( in[i].child.size() != 0 ){
            in[i].data.clear();
            Prep_Children_For_Recompute_Children_Data_Size( in[i].child );

        //If there are not children nodes, we (probably, might?) have data attached. We do nothing.
        }else{
            //Do nothing...
        }
    }
    return;
}


//This function is used for repacking purposes. It recurses through a vector to give the TOTAL size of all children (including the 
// A, B, and data parts of each!)
//
//NOTE: The unit of "size" is bytes.
//NOTE: This size includes the "A" and "B" parts of the children nodes.
//NOTE: The return value of this function will be the total length of the 
long int Recompute_Children_Data_Size( std::vector<piece> &in ){
    long int upward = 0;

    //Cycle through the vector.
    for(size_t i=0; i<in.size(); ++i){

        //Check if this node has children. If it does, we recurse and determine the size of all child elements.
        if( in[i].child.size() != 0 ){
            in[i].data_size = Recompute_Children_Data_Size( in[i].child );

        //If it does not, we update the local "data_size" element and add it to "upward" along with the additional 8 bytes for this nodes "A" and "B".
        }else{
            in[i].data_size = (long int)(in[i].data.size());
        }

        //Check if B includes the size of the element. If it does not, then we have to take into account the extra 4 bytes required to append the
        // size immediately after B (when we eventually write the data...)
        if( Does_A_B_Not_Denote_A_Size( in[i].A, in[i].B ) ){

            //If the last two bytes of B denote the size, we have only 8 bytes of data (no extra size 4 bytes.)
            if( Do_Last_Two_Bytes_of_B_Denote_A_Size( in[i].A, in[i].B ) ){
                upward += in[i].data_size + (long int)(2*sizeof(uint32_t));

            //Otherwise, if we require an extra four bytes then we have to add this to the size of this elements memory footprint. 
            }else if( Do_Next_Four_Bytes_Denote_A_Size( in[i].A, in[i].B ) ){
                upward += in[i].data_size + (long int)(2*sizeof(uint32_t)) + (long int)(1*sizeof(uint32_t));

            //This is the case where we do not know enough about the tags to tell either way (safely.) Issue a warning and pick a method to try it.
            }else{
                FUNCWARN("Attempting to determine the size of the memory layout of an element A = " << in[i].A << " and B = " << in[i].B << " which is unfamiliar (not on a whitelist.)");
                FUNCWARN("  Please determine how to read the element and add it to the appropriate whitelist function.");
                FUNCWARN("  Guessing how the item should be treated. Search the source for tag [ WWWW2 ] for more info.");

                //GUESSING that the last two bytes of B denote the size! See ~10 lines above. The appropriate whitelist is Do_Last_Two_Bytes_of_B_Denote_A_Size(...).
                FUNCWARN("  Guessing a default layout. If this works, please add it to the appropriate whitelist (and test it!)");
                upward += in[i].data_size + (long int)(2*sizeof(uint32_t));
            }

        //Otherwise, B denotes the size. We have a simple layout.
        }else{
            upward += in[i].data_size + (long int)(2*sizeof(uint32_t));
        }

    }
    return upward;

}


//This function will take a vector of nodes and children and will repack each node. 
//
//NOTE: In order to fully pack an entire vector, just create a phony node and attach the vector to be its' child.
//
void Repack_Nodes( const std::vector<piece> &in, std::basic_string<unsigned char> &out ){

    //Cycle through the vector.
    for(size_t i=0; i<in.size(); ++i){

        //Dump the "A" part.
        out.push_back( in[i].A.c[0] );
        out.push_back( in[i].A.c[1] );
        out.push_back( in[i].A.c[2] );
        out.push_back( in[i].A.c[3] );


        //Check if B includes the size of the element. If it does not, then we have to take into account the extra 4 bytes required to append the size.
        if( Does_A_B_Not_Denote_A_Size( in[i].A, in[i].B ) ){

            //If the last two bytes of B denote the size, then we need to copy part of B and write the size. This is the second-most common scenario, and
            // appears to only happen near the header.
            if( Do_Last_Two_Bytes_of_B_Denote_A_Size( in[i].A, in[i].B ) ){
                out.push_back( in[i].B.c[0] );
                out.push_back( in[i].B.c[1] );

                small temp;
                temp.i = static_cast<uint16_t>( in[i].data_size );
                out.push_back( temp.c[0] );
                out.push_back( temp.c[1] );


            //Otherwise, if we require an extra four bytes then we simply copy over the B and then write the size.
            // This appears to be very rare, and only happens near the header.
            }else if( Do_Next_Four_Bytes_Denote_A_Size( in[i].A, in[i].B ) ){
                out.push_back( in[i].B.c[0] );
                out.push_back( in[i].B.c[1] );
                out.push_back( in[i].B.c[2] );
                out.push_back( in[i].B.c[3] );

                large temp;
                temp.i = static_cast<uint32_t>(in[i].data_size);
                out.push_back( temp.c[0] );
                out.push_back( temp.c[1] );
                out.push_back( temp.c[2] );
                out.push_back( temp.c[3] );

            //This is the case where we do not know enough about the tags to tell either way (safely.) Issue a warning and pick a method to try it.
            }else{
                FUNCWARN("Attempting to flatten an element A = " << in[i].A << " and B = " << in[i].B << " which is unfamiliar (not on a whitelist.)");
                FUNCWARN("  Please determine how to read the element and add it to the appropriate whitelist function.");
                FUNCWARN("  Guessing how the item should be treated. Search the source for tag [ WWWW3 ] for more info.");

                //GUESSING that the last two bytes of B denote the size! See ~10 lines above. The appropriate whitelist is Do_Last_Two_Bytes_of_B_Denote_A_Size(...).
                FUNCWARN("  Guessing a default behaviour. If this works, please add it to the appropriate whitelist.");

                out.push_back( in[i].B.c[0] );
                out.push_back( in[i].B.c[1] );

                small temp;
                temp.i = static_cast<uint16_t>( in[i].data_size );
                out.push_back( temp.c[0] );
                out.push_back( temp.c[1] );

            }

        //Otherwise, B denotes (only) the size. This is the most common scenario (in the body of the file.)
        }else{
            large temp;
            temp.i = static_cast<uint32_t>(in[i].data_size);
            out.push_back( temp.c[0] );
            out.push_back( temp.c[1] );
            out.push_back( temp.c[2] );
            out.push_back( temp.c[3] );
        }


        //Now check if there are any children nodes and dump them.
        if( in[i].child.size() != 0 ){
            Repack_Nodes( in[i].child, out );
 
        //Otherwise, dump the payload string. NOTE: Some data elements are empty - this is OK.
        }else{
            out += in[i].data;
        }
    }
    return;
}


int main(int argc, char **argv){

    std::string filename_in = "RS1.dcm";

//filename_in = "OTHER_DATA/CT3.dcm";
//filename_in = "RS1.dcm_repacked.dcm";
//filename_in = "/home/hal/SGF/SGF2/RS.1.2.246.352.71.4.371336894.14697590.20080428144535.dcm";
//filename_in = "/home/hal/SGF/SGF2/RS.1.2.246.352.71.4.371336894.14697590.20080428144535.dcm_repacked.dcm";

    if(argc == 2) filename_in = argv[1];

    std::string filename_parsed       = filename_in + "_parsed.txt";
    std::string filename_fully_parsed = filename_in + "_fully_parsed.txt";
    std::string filename_repacked     = filename_in + "_repacked.dcm";

//Black-hole the output.
filename_parsed        = "/dev/null";
filename_fully_parsed  = "/dev/null";
filename_repacked      = "/dev/null";

    //------
    long int size = 0;


FUNCINFO("___________________________________________________________ Performing basic loading routines ____________________________________________________");

    FUNCINFO("About to load file " << filename_in );

    //Load the file.
//    std::unique_ptr<unsigned char> file_in_memory = Load_Binary_File(filename_in, &size);
    std::unique_ptr<unsigned char> file_in_memory = Load_Binary_File<unsigned char>(filename_in, &size);

    if(file_in_memory == nullptr) FUNCERR("Unable to load binary file");
    const unsigned char *begin = file_in_memory.get();
    const unsigned char *end = begin + size;

    FUNCINFO("The size of the file in flat memory is " << size );

    //Check the file for the existence of the 'DICM' signature. 
    const unsigned char *it = Validate_DICOM_Format(begin, end);
    if(it == nullptr) FUNCERR("Unexpected error - unable to validate file");
    if(it == end    ) FUNCERR("The file does not appear to be a valid DICOM file. Please double check it");

  
FUNCINFO("___________________________________________________________ Performing parsing routines __________________________________________________________");

    //De-lineate the data within the file. Note that the nodes output are NOT traversed and expanded.
    std::vector<piece> data = Parse_Binary_File(it, end);
    if(data.size() == 0) FUNCERR("No data was output: the file is either empty or there was an issue processing data");

    //Dump the non-fully-expanded data. Note that none of the children nodes have been expanded (if there are any..)
    std::ofstream out(filename_parsed.c_str(), std::ios::out );
    for(size_t i=0; i<data.size(); ++i){
        out << data[i] << std::endl;
    }
    out.close();
 
    //Expand all children nodes recursively.
    Delineate_Children(data);      //   <------ this call modifies the data vector (by appending new nodes.)

    //Now cycle through the data and print *only* data strings from those pieces which *do not* have a child.
    // All of this data will be (should be) fully delinearized.
    //Dump_Children(std::cout, data);
    std::ofstream fully_parsed(filename_fully_parsed.c_str(), std::ios::out );
    Dump_Children(fully_parsed, data);
    fully_parsed.close();


FUNCINFO("___________________________________________________________ Performing modification routines _____________________________________________________");

    //Select an element (or a nested element) to get the data from. Elements are returned as pointer to the in-place data. We can ask for references to however
    // high up the tree we wish.
    std::vector<piece *> selection;
//    Get_Elements(selection, data, {6291464});  //Should pull out the modality (RTSTRUCT, etc..)
    Get_Elements(selection, data, {2109446, 3758161918, 2502662});  //For RS files, should pick out the ROI contour names.
//    Get_Elements(selection, data, {2109446, 3758161918, 0});  //For RS files, will pick out the ROI contour names and all neighbouring at-depth data.
//    Get_Elements(selection, data, {3747846, 3758161918, 4206598, 3758161918, 1454086, 3758161918, 290783240 }); //For RS files, will pull out references to CT data near the contour data.


//THIS CODE WORKS FINE - WE JUST ENABLED "QUIET MODE"
/*
    //Print this data to screen.
    Dump_Children(std::cout, selection);


    //Modify this data in place. Note that we need not worry about the size of data - this is recomputed (recursively) as needed upon writing.
    for( auto i=selection.begin(); i!=selection.end(); ++i){
        if( (**i).data == (unsigned char*)("L PAROTID ") ){
            (**i).data = (unsigned char*)("VCC RULZ, VIC DRULZ");
            break;
        }
    }


    //Print out the data again to show the (possible) change which we have enacted.
    FUNCINFO("After attempting to modify the data, we now have..");
    Dump_Children(std::cout, selection);
*/
    //Do not use the vector filled by Get_Elements after this point - the data may get shuffled and reorganized.


FUNCINFO("___________________________________________________________ Performing writing routines __________________________________________________________");


    //Write the data to file. We make sure to re-compute the 'size' of objects, since any of them may have been altered in some way (or their children, or their
    // children's children, etc..)

    Prep_Children_For_Recompute_Children_Data_Size( data );
    Recompute_Children_Data_Size( data );

    std::basic_string<unsigned char> repacked;
    Repack_Nodes( data, repacked );

    //Append a (token) DICOM header so that the file will be recognized elsewhere.
    repacked = Simple_DICOM_Header() + repacked;
   
    FUNCINFO("The size of the repacked flat file is " << repacked.size());

    std::ofstream bin_out( filename_repacked.c_str(), std::ios::out | std::ios::binary );
    bin_out << repacked;
    bin_out.close();



    return 0;
}
