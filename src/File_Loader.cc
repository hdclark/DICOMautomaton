//File_Loader.cc - A part of DICOMautomaton 2019. Written by hal clark.

#include <exception>
//#include <functional>
#include <iostream>
#include <list>
#include <map>
//#include <memory>
#include <string>    
#include <vector>
//#include <cfenv>              //Needed for std::feclearexcept(FE_ALL_EXCEPT).

#include <boost/filesystem.hpp>
//#include <cstdlib>            //Needed for exit() calls.
//#include <utility>            //Needed for std::pair.

#include "YgorFilesDirs.h"    //Needed for Does_File_Exist_And_Can_Be_Read(...), etc..
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorString.h"       //Needed for GetFirstRegex(...)

#include "Structs.h"

#include "Boost_Serialization_File_Loader.h"
#include "DICOM_File_Loader.h"
#include "FITS_File_Loader.h"
#include "XYZ_File_Loader.h"
#include "OFF_Mesh_File_Loader.h"
#include "STL_Mesh_File_Loader.h"
#include "OBJ_Mesh_File_Loader.h"
#include "3ddose_File_Loader.h"
#include "Line_Sample_File_Loader.h"



// This routine loads files. In order for it to return true, all files need to be successfully read.
// If a file cannot be read, all others are tried before returning false.
bool
Load_Files( Drover &DICOM_data,
            std::map<std::string,std::string> &InvocationMetadata,
            std::string &FilenameLex,
            std::list<boost::filesystem::path> &Paths ){

    //Convert directories to filenames.
    // TODO.

    //Remove non-existent filenames and directories.
    bool contained_unresolvable = false;
    {
        std::list<boost::filesystem::path> CPaths;
        for(const auto &apath : Paths){
            bool wasOK = false;
            try{
                wasOK = boost::filesystem::exists(apath);
            }catch(const boost::filesystem::filesystem_error &){ }

            if(wasOK){
                CPaths.emplace_back(apath);
            }else{
                FUNCWARN("Unable to resolve file or directory '" << apath << "'");
                contained_unresolvable = true;
            }
        }
        Paths = CPaths;
    }

    //Standalone file loading: Boost.Serialization archives.
    if(!Paths.empty()
    && !Load_From_Boost_Serialization_Files( DICOM_data, InvocationMetadata, FilenameLex, Paths )){
        FUNCWARN("Failed to load Boost.Serialization archive");
        return false;
    }

    //Standalone file loading: DICOM files.
    if(!Paths.empty()
    && !Load_From_DICOM_Files( DICOM_data, InvocationMetadata, FilenameLex, Paths )){
        FUNCWARN("Failed to load DICOM file");
        return false;
    }

    //Standalone file loading: FITS files.
    if(!Paths.empty()
    && !Load_From_FITS_Files( DICOM_data, InvocationMetadata, FilenameLex, Paths )){
        FUNCWARN("Failed to load FITS file");
        return false;
    }

    //Standalone file loading: DOSXYZnrc 3ddose files.
    if(!Paths.empty()
    && !Load_From_3ddose_Files( DICOM_data, InvocationMetadata, FilenameLex, Paths )){
        FUNCWARN("Failed to load 3ddose file");
        return false;
    }

    //Standalone file loading: OFF mesh files.
    if(!Paths.empty()
    && !Load_Mesh_From_OFF_Files( DICOM_data, InvocationMetadata, FilenameLex, Paths )){
        FUNCWARN("Failed to load OFF mesh file");
        return false;
    }

    //Standalone file loading: OBJ files.
    if(!Paths.empty()
    && !Load_Mesh_From_OBJ_Files( DICOM_data, InvocationMetadata, FilenameLex, Paths )){
        FUNCWARN("Failed to load OBJ mesh file");
        return false;
    }

    //Standalone file loading: STL mesh files (both ASCII and binary).
    if(!Paths.empty()
    && !Load_Mesh_From_STL_Files( DICOM_data, InvocationMetadata, FilenameLex, Paths )){
        FUNCWARN("Failed to load STL mesh file");
        return false;
    }

    //Standalone file loading: XYZ point cloud files.
    if(!Paths.empty()
    && !Load_From_XYZ_Files( DICOM_data, InvocationMetadata, FilenameLex, Paths )){
        FUNCWARN("Failed to load XYZ file");
        return false;
    }

    //Standalone file loading: line sample files.
    if(!Paths.empty()
    && !Load_From_Line_Sample_Files( DICOM_data, InvocationMetadata, FilenameLex, Paths )){
        FUNCWARN("Failed to load line sample file");
        return false;
    }

    return (Paths.empty() && !contained_unresolvable);
}

