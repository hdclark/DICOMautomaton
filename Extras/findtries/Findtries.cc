#include <iostream>
#include <iomanip>
#include <string>
#include <fstream>
#include <sstream>
#include <map>
#include <vector>
#include <algorithm>

//Takes an input file full of text and prints out a list of all tries present.
using namespace std;

//This is the length of each sequence of letters. A traditional 'trie' would be 3.
#define TRIE_NUM 20

#ifndef isininc
//Inclusive_in_range()      isininc( 0, 10, 100) == true
#define isininc( A, x, B ) (((x) >= (A)) && ((x) <= (B)))
#endif

/*
//Template stolen from Stack Overflow. Exists to be passed to sort() to sort
// by second element of a pair, instead of first (the default.) 
//
//It was not needed this time, but I want to keep it here for future reference.

template<class T1, class T2, class Pred = std::less<T> >
struct sort_pair_second {
    bool operator()(const std::pair<T1,T2>&left, const std::pair<T1,T2>&right) {
        Pred p;
        return p(left.second, right.second);
    }
};
*/


int main(int argc, char *argv[]){
    if( argc != 3 ){
        cout << "Simple program to comb a text file for X # of adjacent characters. Spits out occurences and strings to file." << endl;
        cout << argv[0] << " <input> <output> " << endl;
        return -1;
    }

    std::fstream FI;
    FI.open(argv[1], std::ifstream::in);
    if(FI.fail()){
        cout << "Input file not found. " << endl;
        return -1;
    }

    std::fstream FO;
    FO.open(argv[2], std::ofstream::out);

    map<string,int> totlcnt;
    map<string,int>::iterator it;
    long int entire = 0;

    do{
        std::string rawstring;
        getline(FI,rawstring);
        std::stringstream inss(rawstring);
        while(inss.good()){
            bool wasgood = inss.good();
            string theword;
            inss >> theword;

            for(unsigned int i=0; i<theword.length();++i){
                unsigned int ascii_code = static_cast<unsigned int>(theword[i]);
                if( isininc(65, ascii_code, 90) ){ //This is what we want: Uppercase.
                    //Nothing here...
                }else if( isininc(97, ascii_code, 122)){ //Lowercase
                    theword[i] = static_cast<unsigned char>( ascii_code - 32 );
                }
            }

            if(theword.length() > (TRIE_NUM-1)){
                for(int i=TRIE_NUM;i<=theword.size();++i){
                    const string thesubstr = theword.substr((i-TRIE_NUM),TRIE_NUM);

                    //We are only interested in ASCII data. Just kill the conditional otherwise.
                    bool isallascii = true;
                    for(int j=0; j<TRIE_NUM; ++j){
                        if( !(isininc(65,static_cast<unsigned int>(thesubstr[j]),90) || isininc(97,static_cast<unsigned int>(thesubstr[j]),122)) ) 
                            isallascii = false;
                    }

                    if( isallascii ){
                        //Fill a map with all the info. This might get memorily costly :).
                        it = totlcnt.find(thesubstr);
                        ++totlcnt[ thesubstr ];
                        ++entire;
                    }//if( isallascii )
                }//for(int i=TRIE_NUM;i<=theword.size();++i)
            }//if(theword.length() > (TRIE_NUM-1))
        }//while(inss.good())
    }while(!FI.eof());

    //Since maps are always auto-sorted, we can make the inverse map to sort. By occurence.
    vector< pair<int,string> > converse;
    for( it = totlcnt.begin() ; it != totlcnt.end(); it++ ){
        pair<int,string> temp((*it).second,(*it).first);
        converse.push_back(temp);
        //Remove it from the map. We no longer need it.
        totlcnt.erase(it);
    }
    sort( converse.begin(), converse.end() ); //Default sort compares the first element.

    //Now we spit out the info.
    for( int i=0; i<converse.size(); ++i){
        // <occurence count>     <string>
         //FO << converse[i].first << " " << converse[i].second << endl;
        // <occurence count>/<total number found>   <string>
         //FO << converse[i].first << "/" << entire << " " << converse[i].second << endl;
        // <(occurence count) / (total number found)>     <string>
         FO << setiosflags(ios::fixed) << setprecision(20) << setw(30) << static_cast<float>(converse[i].first)/static_cast<float>(entire) << " " << converse[i].second << endl;
    }

    FO.close();
    FI.close();
    return 0;
}
