//XYZ_File_Loader.cc - A part of DICOMautomaton 2019. Written by hal clark.
//
// This program loads point cloud data from XYZ files.
//

#include <cmath>
#include <cstdint>
#include <exception>
#include <fstream>
#include <list>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>    

#include <boost/filesystem.hpp>
#include <cstdlib>            //Needed for exit() calls.

#include "Structs.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorString.h"       //Needed for SplitStringToVector, Canonicalize_String2, SplitVector functions.


bool Load_From_XYZ_Files( Drover &DICOM_data,
                          std::map<std::string,std::string> & /* InvocationMetadata */,
                          std::string &,
                          std::list<boost::filesystem::path> &Filenames ){

    //This routine will attempt to load XYZ-format files. Files that are not successfully loaded
    // are not consumed so that they can be passed on to the next loading stage as needed. 
    //
    // The following file describes the core format and will be correctly read by this routine:
    //  _________________________________________________________________________
    //  |# This is a comment. It should be ignored.                             |
    //  |# The next line is intentionally blank. It should be ignored too.      |
    //  |                                                                       |
    //  |1.0 1.0 1.0                                                            |
    //  | 2.0 2.0 2.0                                                           |
    //  |3,3,3                                                                  |
    //  |                                                                       |
    //  |4;4 4                                                                  |
    //  |5.0E-4 nan inf                                                         |
    //  |                                                                       |
    //  |6.0,6.0,6.0 # This is also a comment and should be ignored.            |
    //  |_______________________________________________________________________|
    //
    // Only ASCII format is accepted. Multiple separators are accepted, and whitespace is generally not significant
    // (except if used as a separator between numbers). Only lines with 3 scalars are accepted as valid points.
    // Reading metadata encoded into comments (as is done for FITS files) is not supported.
    //
    // The accepted format is variable, and it is hard to decide whether a given file is definitively in XYZ format. The
    // threshold to decide is whether any single line contains a point that can be successfully read. If this happens,
    // the file is considered to be in XYZ format. Therefore, it is best to attempt loading other, more strucured
    // formats if uncertain about the file type ahead of time.
    //
    // Note: This routine returns false only iff a file is suspected of being suited for this loader, but could not be
    //       loaded (e.g., the file seems appropriate, but a parsing failure was encountered).
    //
    if(Filenames.empty()) return true;

    DICOM_data.point_data.emplace_back( std::make_shared<Point_Cloud>() );

    size_t i = 0;
    const size_t N = Filenames.size();

    long int point_count = 0;

    auto bfit = Filenames.begin();
    while(bfit != Filenames.end()){
        FUNCINFO("Parsing file #" << i+1 << "/" << N << " = " << 100*(i+1)/N << "%");
        ++i;
        const auto Filename = bfit->string();

        try{
            //////////////////////////////////////////////////////////////
            // Attempt to load the file.
            std::fstream FI(Filename.c_str(), std::ios::in);

            if(!FI.good()){
                throw std::runtime_error("Unable to read file.");
            }

            std::string aline;
            while(!FI.eof()){
                std::getline(FI, aline);
                if(FI.eof()) break;

                // Strip away comments.
                const auto l_c_v = SplitStringToVector(aline, '#', 'd');
                if(l_c_v.size() == 0){ // Empty line.
                    continue;
                }
                //// Handle comment lines.
                //if(l_c_v.size() == 2){
                //    const auto comment = l_c_v[1];
                //}
                aline = l_c_v.front(); // Retain only non-comments from this point.

                aline = Canonicalize_String2(aline, CANONICALIZE::TRIM);
                if(aline.empty()) continue;

                // Split the line assuming the the separator could be anything common.
                std::vector<std::string> xyz = { aline };
                xyz = SplitVector(xyz, ' ', 'd');
                xyz = SplitVector(xyz, ',', 'd');
                xyz = SplitVector(xyz, ';', 'd');
                xyz = SplitVector(xyz, '\t', 'd');
                if(xyz.size() != 3){
                    // If no points have thusfar been identified, assume it is not actually an XYZ file and ignore the
                    // line altogether.
                    if(point_count == 0){
                        continue;

                    // But if points have been identified, the file may be damaged or invalid. Indicate the read
                    // failure.
                    }else{
                        return false;
                    }
                }

                vec3<double> shtl;
                shtl.x = std::stod(xyz.at(0));
                shtl.y = std::stod(xyz.at(1));
                shtl.z = std::stod(xyz.at(2));
                DICOM_data.point_data.back()->points.emplace_back( std::make_pair(shtl, point_count) );
                ++point_count;
            }
            FI.close();
            //////////////////////////////////////////////////////////////

            // Reject the file if no points were successfully read from it.
            if(point_count == 0){
                throw std::runtime_error("Unable to read file.");
            }

            FUNCINFO("Loaded XYZ file with " 
                     << point_count
                     << " points");
            bfit = Filenames.erase( bfit ); 
            continue;
        }catch(const std::exception &e){
            FUNCINFO("Unable to load as XYZ file");
        };

        //Skip the file. It might be destined for some other loader.
        ++bfit;
    }
            
    //If nothing was loaded, do not post-process.
    const size_t N2 = Filenames.size();
    if(N == N2){
        DICOM_data.point_data.pop_back();
        return true;
    }

    return true;
}
