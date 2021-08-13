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

#include <filesystem>
#include <cstdlib>            //Needed for exit() calls.

#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMathIOXYZ.h"
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorString.h"       //Needed for SplitStringToVector, Canonicalize_String2, SplitVector functions.

#include "Structs.h"
#include "Imebra_Shim.h"

bool Load_From_XYZ_Files( Drover &DICOM_data,
                          const std::map<std::string,std::string> & /* InvocationMetadata */,
                          const std::string &,
                          std::list<std::filesystem::path> &Filenames ){

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

    size_t i = 0;
    const size_t N = Filenames.size();

    auto bfit = Filenames.begin();
    while(bfit != Filenames.end()){
        FUNCINFO("Parsing file #" << i+1 << "/" << N << " = " << 100*(i+1)/N << "%");
        ++i;
        const auto Filename = bfit->string();

        DICOM_data.point_data.emplace_back( std::make_shared<Point_Cloud>() );

        try{
            //////////////////////////////////////////////////////////////
            // Attempt to load the file.
            std::ifstream FI(Filename.c_str(), std::ios::in);
            if(!ReadPointSetFromXYZ(DICOM_data.point_data.back()->pset, FI)){
                throw std::runtime_error("Unable to read point cloud from file.");
            }
            FI.close();
            //////////////////////////////////////////////////////////////

            // Reject the file if the point cloud is not valid.
            const auto N_points = DICOM_data.point_data.back()->pset.points.size();
            if( N_points == 0 ){
                throw std::runtime_error("Unable to read point cloud from file.");
            }

            // Supply generic minimal metadata iff it is needed.
            std::map<std::string, std::string> generic_metadata;

            generic_metadata["Filename"] = Filename; 

            generic_metadata["PatientID"] = "unspecified";
            generic_metadata["StudyInstanceUID"] = Generate_Random_UID(60);
            generic_metadata["SeriesInstanceUID"] = Generate_Random_UID(60);
            generic_metadata["FrameOfReferenceUID"] = Generate_Random_UID(60);
            generic_metadata["SOPInstanceUID"] = Generate_Random_UID(60);
            generic_metadata["Modality"] = "PointCloud";

            generic_metadata["PointName"] = "unspecified"; 
            generic_metadata["NormalizedPointName"] = "unspecified"; 

            generic_metadata["ROIName"] = "unspecified"; 
            generic_metadata["NormalizedROIName"] = "unspecified"; 
            DICOM_data.point_data.back()->pset.metadata.merge(generic_metadata);

            FUNCINFO("Loaded point cloud with " << N_points << " points");
            bfit = Filenames.erase( bfit ); 
            continue;
        }catch(const std::exception &e){
            FUNCINFO("Unable to load as XYZ point cloud file");
            DICOM_data.point_data.pop_back();
        };

        //Skip the file. It might be destined for some other loader.
        ++bfit;
    }

    return true;
}

