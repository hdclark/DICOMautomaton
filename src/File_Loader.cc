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
#include <initializer_list>

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
#include "OFF_File_Loader.h"
#include "STL_File_Loader.h"
#include "OBJ_File_Loader.h"
#include "PLY_File_Loader.h"
#include "3ddose_File_Loader.h"
#include "Line_Sample_File_Loader.h"
#include "TAR_File_Loader.h"
#include "DVH_File_Loader.h"

using loader_func_t = std::function<bool(std::list<boost::filesystem::path>&)>;
struct file_loader_t {
    std::list<std::string> exts;
    float priority;
    loader_func_t f;
};

// This routine loads files. In order for it to return true, all files need to be successfully read.
// If a file cannot be read, all others are tried before returning false.
bool
Load_Files( Drover &DICOM_data,
            const std::map<std::string,std::string> &InvocationMetadata,
            const std::string &FilenameLex,
            std::list<boost::filesystem::path> &Paths ){

    // Generate a priority list of file loaders.
    // Note that some file loaders are extremely generous in what they accept, so feeding them generic files could
    // result in false-positives and invalid data. The following default order was determined heuristically.
    const auto get_default_loaders = [&](){
        std::list<file_loader_t> loaders;

        //Standalone file loading: TAR files.
        loaders.emplace_back(file_loader_t{{".tar"}, 1.0, [&](std::list<boost::filesystem::path> &p) -> bool {
            if(!p.empty()
            && !Load_From_TAR_Files( DICOM_data, InvocationMetadata, FilenameLex, p )){
                FUNCWARN("Failed to load TAR file");
                return false;
            }
            return true;
        }});

        //Standalone file loading: Boost.Serialization archives.
        loaders.emplace_back(file_loader_t{{".gz", ".tar", ".tar.gz", ".tgz", ".xml", ".xml.gz", ".txt", ".txt.gz"}, 2.0, [&](std::list<boost::filesystem::path> &p) -> bool {
            if(!p.empty()
            && !Load_From_Boost_Serialization_Files( DICOM_data, InvocationMetadata, FilenameLex, p )){
                FUNCWARN("Failed to load Boost.Serialization archive");
                return false;
            }
            return true;
        }});

        //Standalone file loading: DICOM files.
        loaders.emplace_back(file_loader_t{{".dcm", ""}, 3.0, [&](std::list<boost::filesystem::path> &p) -> bool {
            if(!p.empty()
            && !Load_From_DICOM_Files( DICOM_data, InvocationMetadata, FilenameLex, p )){
                FUNCWARN("Failed to load DICOM file");
                return false;
            }
            return true;
        }});

        //Standalone file loading: (ASCII or binary) PLY (mesh or point cloud) files.
        loaders.emplace_back(file_loader_t{{".ply"}, 4.0, [&](std::list<boost::filesystem::path> &p) -> bool {
            if(!p.empty()
            && !Load_From_PLY_Files( DICOM_data, InvocationMetadata, FilenameLex, p )){
                FUNCWARN("Failed to load ASCII/binary PLY mesh or point cloud file");
                return false;
            }
            return true;
        }});

        //Standalone file loading: ASCII STL mesh files.
        //
        // Note: should preceed 'tabular DVH' line sample files.
        loaders.emplace_back(file_loader_t{{".stl"}, 5.0, [&](std::list<boost::filesystem::path> &p) -> bool {
            if(!p.empty()
            && !Load_Mesh_From_ASCII_STL_Files( DICOM_data, InvocationMetadata, FilenameLex, p )){
                FUNCWARN("Failed to load ASCII STL mesh file");
                return false;
            }
            return true;
        }});

        //Standalone file loading: binary STL mesh files.
        loaders.emplace_back(file_loader_t{{".stl"}, 6.0, [&](std::list<boost::filesystem::path> &p) -> bool {
            if(!p.empty()
            && !Load_Mesh_From_Binary_STL_Files( DICOM_data, InvocationMetadata, FilenameLex, p )){
                FUNCWARN("Failed to load binary STL mesh file");
                return false;
            }
            return true;
        }});

        //Standalone file loading: 'tabular DVH' line sample files.
        loaders.emplace_back(file_loader_t{{".dvh", ".txt", ".dat"}, 7.0, [&](std::list<boost::filesystem::path> &p) -> bool {
            if(!p.empty()
            && !Load_From_DVH_Files( DICOM_data, InvocationMetadata, FilenameLex, p )){
                FUNCWARN("Failed to load DVH file");
                return false;
            }
            return true;
        }});

        //Standalone file loading: FITS files.
        loaders.emplace_back(file_loader_t{{".fit", ".fits"}, 8.0, [&](std::list<boost::filesystem::path> &p) -> bool {
            if(!p.empty()
            && !Load_From_FITS_Files( DICOM_data, InvocationMetadata, FilenameLex, p )){
                FUNCWARN("Failed to load FITS file");
                return false;
            }
            return true;
        }});

        //Standalone file loading: DOSXYZnrc 3ddose files.
        loaders.emplace_back(file_loader_t{{".3ddose"}, 9.0, [&](std::list<boost::filesystem::path> &p) -> bool {
            if(!p.empty()
            && !Load_From_3ddose_Files( DICOM_data, InvocationMetadata, FilenameLex, p )){
                FUNCWARN("Failed to load 3ddose file");
                return false;
            }
            return true;
        }});

        //Standalone file loading: OFF point cloud files.
        //
        // Note: should preceed the OFF mesh loader.
        loaders.emplace_back(file_loader_t{{".off"}, 10.0, [&](std::list<boost::filesystem::path> &p) -> bool {
            if(!p.empty()
            && !Load_Points_From_OFF_Files( DICOM_data, InvocationMetadata, FilenameLex, p )){
                FUNCWARN("Failed to load OFF point cloud file");
                return false;
            }
            return true;
        }});

        //Standalone file loading: OFF mesh files.
        loaders.emplace_back(file_loader_t{{".off"}, 11.0, [&](std::list<boost::filesystem::path> &p) -> bool {
            if(!p.empty()
            && !Load_Mesh_From_OFF_Files( DICOM_data, InvocationMetadata, FilenameLex, p )){
                FUNCWARN("Failed to load OFF mesh file");
                return false;
            }
            return true;
        }});

        //Standalone file loading: OBJ point cloud files.
        //
        // Note: should preceed the OBJ mesh loader.
        loaders.emplace_back(file_loader_t{{".obj"}, 12.0, [&](std::list<boost::filesystem::path> &p) -> bool {
            if(!p.empty()
            && !Load_Points_From_OBJ_Files( DICOM_data, InvocationMetadata, FilenameLex, p )){
                FUNCWARN("Failed to load OBJ point cloud file");
                return false;
            }
            return true;
        }});

        //Standalone file loading: OBJ mesh files.
        loaders.emplace_back(file_loader_t{{".obj"}, 13.0, [&](std::list<boost::filesystem::path> &p) -> bool {
            if(!p.empty()
            && !Load_Mesh_From_OBJ_Files( DICOM_data, InvocationMetadata, FilenameLex, p )){
                FUNCWARN("Failed to load OBJ mesh file");
                return false;
            }
            return true;
        }});

        //Standalone file loading: XYZ point cloud files.
        //
        // Note: XYZ can be confused with many other formats, so it should be near the end.
        loaders.emplace_back(file_loader_t{{".xyz", ".txt"}, 14.0, [&](std::list<boost::filesystem::path> &p) -> bool {
            if(!p.empty()
            && !Load_From_XYZ_Files( DICOM_data, InvocationMetadata, FilenameLex, p )){
                FUNCWARN("Failed to load XYZ file");
                return false;
            }
            return true;
        }});

        //Standalone file loading: line sample files.
        //
        // Note: this file can be confused with many other formats, so it should be near the end.
        loaders.emplace_back(file_loader_t{{".lsamp", ".lsamps", ".txt"}, 15.0, [&](std::list<boost::filesystem::path> &p) -> bool {
            if(!p.empty()
            && !Load_From_Line_Sample_Files( DICOM_data, InvocationMetadata, FilenameLex, p )){
                FUNCWARN("Failed to load line sample file");
                return false;
            }
            return true;
        }});

        return loaders;
    };


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

    // Partition the paths by file extension.
    icase_map_t<std::list<boost::filesystem::path>> extensions(icase_str_lt_lambda);
    for(auto &p : Paths){
        const auto ext = p.extension().string();
        extensions[ext].push_back(p);
    }
    Paths.clear();

    for(auto & ep : extensions){
        const auto ext = ep.first;
        auto &&l_Paths = ep.second;
//FUNCINFO("ext = " << ext);
        
        // Boost the priority of any loaders whose extensions match this bunch of files.
        auto loaders = get_default_loaders();
        for(auto &l : loaders){
            if(std::any_of( std::begin(l.exts),
                            std::end(l.exts),
                            [ext](const std::string &l_ext){ return icase_str_eq_lambda(ext, l_ext); })){
            
                l.priority -= 100.0;
            }
        }

        // For select extensions, exclude all other loaders that are extremely likely to be irrelevant.
        if( icase_str_eq_lambda(ext, ".dcm")
        ||  icase_str_eq_lambda(ext, "")
        ||  icase_str_eq_lambda(ext, ".tar")
        ||  icase_str_eq_lambda(ext, ".3ddose")
        ||  icase_str_eq_lambda(ext, ".stl")
        ||  icase_str_eq_lambda(ext, ".obj")
        ||  icase_str_eq_lambda(ext, ".off")
        ||  icase_str_eq_lambda(ext, ".lsamps") ){
            std::remove_if( std::begin(loaders), std::end(loaders),
                            [&](const file_loader_t &l){
                                return std::none_of( std::begin(l.exts),
                                                     std::end(l.exts),
                                                     [&](const std::string &l_ext){ return icase_str_eq_lambda(ext, l_ext); });
                            });
        }

        // Re-sort using the altered priorities.
        loaders.sort( [](const file_loader_t &l, const file_loader_t &r){
            return (l.priority < r.priority);
        });

        // Attempt to load the files.
        for(const auto &l : loaders){
//FUNCINFO("Trying loader ext[0] = " << l.exts.front() << " with priority = " << l.priority);
            if(!l_Paths.empty() && !l.f(l_Paths)){
                return false;
            }
        }

        // Return any remaining files to the user's container.
        Paths.splice( std::end(Paths), l_Paths );
    }

    return (Paths.empty() && !contained_unresolvable);
}

