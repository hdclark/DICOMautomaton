//File_Loader.cc - A part of DICOMautomaton 2019, 2021. Written by hal clark.

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

#include <filesystem>
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
#include "XIM_File_Loader.h"
#include "OFF_File_Loader.h"
#include "STL_File_Loader.h"
#include "OBJ_File_Loader.h"
#include "PLY_File_Loader.h"
#include "3ddose_File_Loader.h"
#include "Line_Sample_File_Loader.h"
#include "Transformation_File_Loader.h"
#include "TAR_File_Loader.h"
#include "DVH_File_Loader.h"
#include "CSV_File_Loader.h"
#include "Script_Loader.h"

using loader_func_t = std::function<bool(std::list<std::filesystem::path>&)>;
struct file_loader_t {
    std::list<std::string> exts;
    long int priority;
    loader_func_t f;
};

// This routine loads files. In order for it to return true, all files need to be successfully read.
// If a file cannot be read, all others are tried before returning false.
bool
Load_Files( Drover &DICOM_data,
            std::map<std::string,std::string> &InvocationMetadata,
            const std::string &FilenameLex,
            std::list<OperationArgPkg> &Operations,
            std::list<std::filesystem::path> &Paths ){

    // Generate a priority list of file loaders.
    // Note that some file loaders are extremely generous in what they accept, so feeding them generic files could
    // result in false-positives and invalid data. The following default order was determined heuristically.
    const auto get_default_loaders = [&](){
        std::list<file_loader_t> loaders;

        long int priority = 0;

        //Standalone file loading: TAR files.
        loaders.emplace_back(file_loader_t{{".tar", ".gz", ".tar.gz", ".tgz"}, ++priority, [&](std::list<std::filesystem::path> &p) -> bool {
            if(!p.empty()
            && !Load_From_TAR_Files( DICOM_data, InvocationMetadata, FilenameLex, Operations, p )){
                FUNCWARN("Failed to load TAR file");
                return false;
            }
            return true;
        }});

        //Standalone file loading: Boost.Serialization archives.
        loaders.emplace_back(file_loader_t{{".gz", ".tar", ".tar.gz", ".tgz", ".xml", ".xml.gz", ".txt", ".txt.gz"}, ++priority, [&](std::list<std::filesystem::path> &p) -> bool {
            if(!p.empty()
            && !Load_From_Boost_Serialization_Files( DICOM_data, InvocationMetadata, FilenameLex, p )){
                FUNCWARN("Failed to load Boost.Serialization archive");
                return false;
            }
            return true;
        }});

        //Standalone file loading: DICOM files.
        loaders.emplace_back(file_loader_t{{".dcm"}, ++priority, [&](std::list<std::filesystem::path> &p) -> bool {
            if(!p.empty()
            && !Load_From_DICOM_Files( DICOM_data, InvocationMetadata, FilenameLex, p )){
                FUNCWARN("Failed to load DICOM file");
                return false;
            }
            return true;
        }});

        //Standalone file loading: XIM files.
        loaders.emplace_back(file_loader_t{{".xim"}, ++priority, [&](std::list<std::filesystem::path> &p) -> bool {
            if(!p.empty()
            && !Load_From_XIM_Files( DICOM_data, InvocationMetadata, FilenameLex, p )){
                FUNCWARN("Failed to load XIM file");
                return false;
            }
            return true;
        }});

        //Standalone file loading: (ASCII or binary) PLY (mesh or point cloud) files.
        loaders.emplace_back(file_loader_t{{".ply"}, ++priority, [&](std::list<std::filesystem::path> &p) -> bool {
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
        loaders.emplace_back(file_loader_t{{".stl"}, ++priority, [&](std::list<std::filesystem::path> &p) -> bool {
            if(!p.empty()
            && !Load_Mesh_From_ASCII_STL_Files( DICOM_data, InvocationMetadata, FilenameLex, p )){
                FUNCWARN("Failed to load ASCII STL mesh file");
                return false;
            }
            return true;
        }});

        //Standalone file loading: binary STL mesh files.
        loaders.emplace_back(file_loader_t{{".stl"}, ++priority, [&](std::list<std::filesystem::path> &p) -> bool {
            if(!p.empty()
            && !Load_Mesh_From_Binary_STL_Files( DICOM_data, InvocationMetadata, FilenameLex, p )){
                FUNCWARN("Failed to load binary STL mesh file");
                return false;
            }
            return true;
        }});

        //Standalone file loading: 'tabular DVH' line sample files.
        loaders.emplace_back(file_loader_t{{".dvh", ".txt", ".dat"}, ++priority, [&](std::list<std::filesystem::path> &p) -> bool {
            if(!p.empty()
            && !Load_From_DVH_Files( DICOM_data, InvocationMetadata, FilenameLex, p )){
                FUNCWARN("Failed to load DVH file");
                return false;
            }
            return true;
        }});

        //Standalone file loading: script files.
        loaders.emplace_back(file_loader_t{{".dcma", ".dsc", ".dscr", ".scr", ".txt"}, ++priority, [&](std::list<std::filesystem::path> &p) -> bool {
            if(!p.empty()
            && !Load_From_Script_Files( Operations, p )){
                FUNCWARN("Failed to load script file");
                return false;
            }
            return true;
        }});

        //Standalone file loading: FITS files.
        loaders.emplace_back(file_loader_t{{".fit", ".fits"}, ++priority, [&](std::list<std::filesystem::path> &p) -> bool {
            if(!p.empty()
            && !Load_From_FITS_Files( DICOM_data, InvocationMetadata, FilenameLex, p )){
                FUNCWARN("Failed to load FITS file");
                return false;
            }
            return true;
        }});

        //Standalone file loading: DOSXYZnrc 3ddose files.
        loaders.emplace_back(file_loader_t{{".3ddose"}, ++priority, [&](std::list<std::filesystem::path> &p) -> bool {
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
        loaders.emplace_back(file_loader_t{{".off"}, ++priority, [&](std::list<std::filesystem::path> &p) -> bool {
            if(!p.empty()
            && !Load_Points_From_OFF_Files( DICOM_data, InvocationMetadata, FilenameLex, p )){
                FUNCWARN("Failed to load OFF point cloud file");
                return false;
            }
            return true;
        }});

        //Standalone file loading: OFF mesh files.
        loaders.emplace_back(file_loader_t{{".off"}, ++priority, [&](std::list<std::filesystem::path> &p) -> bool {
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
        loaders.emplace_back(file_loader_t{{".obj"}, ++priority, [&](std::list<std::filesystem::path> &p) -> bool {
            if(!p.empty()
            && !Load_Points_From_OBJ_Files( DICOM_data, InvocationMetadata, FilenameLex, p )){
                FUNCWARN("Failed to load OBJ point cloud file");
                return false;
            }
            return true;
        }});

        //Standalone file loading: OBJ mesh files.
        loaders.emplace_back(file_loader_t{{".obj"}, ++priority, [&](std::list<std::filesystem::path> &p) -> bool {
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
        loaders.emplace_back(file_loader_t{{".xyz", ".txt"}, ++priority, [&](std::list<std::filesystem::path> &p) -> bool {
            if(!p.empty()
            && !Load_From_XYZ_Files( DICOM_data, InvocationMetadata, FilenameLex, p )){
                FUNCWARN("Failed to load XYZ file");
                return false;
            }
            return true;
        }});

        //Standalone file loading: transformation files.
        //
        // Note: this file can be confused with many other formats, so it should be near the end.
        loaders.emplace_back(file_loader_t{{".trans", ".txt"}, ++priority, [&](std::list<std::filesystem::path> &p) -> bool {
            if(!p.empty()
            && !Load_Transforms_From_Files( DICOM_data, InvocationMetadata, FilenameLex, p )){
                FUNCWARN("Failed to load transformation file");
                return false;
            }
            return true;
        }});

        //Standalone file loading: line sample files.
        //
        // Note: this file can be confused with many other formats, so it should be near the end.
        loaders.emplace_back(file_loader_t{{".lsamp", ".lsamps", ".txt"}, ++priority, [&](std::list<std::filesystem::path> &p) -> bool {
            if(!p.empty()
            && !Load_From_Line_Sample_Files( DICOM_data, InvocationMetadata, FilenameLex, p )){
                FUNCWARN("Failed to load line sample file");
                return false;
            }
            return true;
        }});

        //Standalone file loading: CSV files.
        //
        // Note: this file can be confused with many other formats, so it should be near the end.
        loaders.emplace_back(file_loader_t{{".csv", ".tsv"}, ++priority, [&](std::list<std::filesystem::path> &p) -> bool {
            if(!p.empty()
            && !Load_From_CSV_Files( DICOM_data, InvocationMetadata, FilenameLex, p )){
                FUNCWARN("Failed to load CSV/TSV file");
                return false;
            }
            return true;
        }});


        return loaders;
    };

    const auto has_recognized_extension = [=](const std::filesystem::path &p) -> bool {
        const auto loaders = get_default_loaders();
        const auto ext = p.extension().string();
        const auto recognized = std::any_of( std::begin(loaders), std::end(loaders),
                                             [ext](const file_loader_t &l){
            return std::any_of( std::begin(l.exts),
                                std::end(l.exts),
                                [ext](const std::string &l_ext){ return icase_str_eq(ext, l_ext); });
        });
        return recognized;
    };

    // Convert directories to filenames and remove non-existent filenames and directories.
    bool contained_unresolvable = false;
    {
        auto loaders = get_default_loaders();
        std::list<std::filesystem::path> recursed_Paths;
        std::list<std::filesystem::path> l_Paths;
        while(!recursed_Paths.empty() || !Paths.empty()){
            const auto is_orig = !Paths.empty();
            auto p = (is_orig) ? Paths.front() : recursed_Paths.front();
            if(is_orig){
                Paths.pop_front();
            }else{
                recursed_Paths.pop_front();
            }

            try{
                // Resolve the file to an absolute path if possible. This is useful for resolving symbolic links.
                // Note that this won't be possible for Windows UNC paths like '\\server\share_name\path\to\a\file.dcm',
                // so we have to confirm whether the absolute path can still be accessed.
                const auto l_p = std::filesystem::absolute(p);
                if( std::filesystem::exists(l_p) ) p = l_p;

                if(! std::filesystem::exists(p) ){
                    throw std::runtime_error("Unable to resolve file or directory "_s + p.string() + "'");
                }

                if( std::filesystem::is_directory(p) ){
                    for(const auto &rp : std::filesystem::directory_iterator(p)){
                        recursed_Paths.push_back(rp);
                    }
                }else{
                    // Only include files with recognized file extensions.
                    if( is_orig 
                    ||  has_recognized_extension(p)){
                        l_Paths.push_back(p);
                    }else{
                        FUNCWARN("Ignoring file '" << p.string() << "' because extension is not recognized. Specify explicitly to attempt loading");
                    }
                }
            }catch(const std::filesystem::filesystem_error &e){
                FUNCWARN(e.what());
                contained_unresolvable = true;
            }
        }
        Paths = l_Paths;
    }

    // Partition the paths by file extension.
    icase_map_t<std::list<std::filesystem::path>> extensions(icase_str_lt);
    for(auto &p : Paths){
        const auto ext = p.extension().string();
        extensions[ext].push_back(p);
    }
    Paths.clear();

    for(auto &ep : extensions){
        const auto ext = ep.first;
        auto &&l_Paths = ep.second;
        
        // Warn if the file extension is not recognized.
        for(const auto &p : l_Paths){
            if(!has_recognized_extension(p)){
                FUNCWARN("Unrecognized file extension '" << ext << "'. Attempting to load because it was explicitly specified");
            }
        }
                                                  
        // Boost the priority of any loaders whose extensions match this bunch of files.
        auto loaders = get_default_loaders();
        for(auto &l : loaders){
            if(std::any_of( std::begin(l.exts),
                            std::end(l.exts),
                            [ext](const std::string &l_ext){ return icase_str_eq(ext, l_ext); })){
            
                l.priority -= 1000;
            }
        }

        // For select 'unique' extensions, exclude all other loaders that are likely to be irrelevant.
        if( !ext.empty()
        &&  (   icase_str_eq(ext, ".dcm")
             || icase_str_eq(ext, ".xim")
             || icase_str_eq(ext, ".tar")
             || icase_str_eq(ext, ".tgz")
             || icase_str_eq(ext, ".gz")
             || icase_str_eq(ext, ".tar.gz")
             || icase_str_eq(ext, ".3ddose")
             || icase_str_eq(ext, ".stl")
             || icase_str_eq(ext, ".obj")
             || icase_str_eq(ext, ".off")
             || icase_str_eq(ext, ".ply")
             || icase_str_eq(ext, ".xyz")
             || icase_str_eq(ext, ".scr")
             || icase_str_eq(ext, ".dscr")
             || icase_str_eq(ext, ".csv")
             || icase_str_eq(ext, ".tsv")
             || icase_str_eq(ext, ".lsamps") ) ){
            loaders.remove_if( [ext](const file_loader_t &l){
                                    return std::none_of( std::begin(l.exts),
                                                         std::end(l.exts),
                                                         [&](const std::string &l_ext){ return icase_str_eq(ext, l_ext); });
                                } );
        }

        // Re-sort using the altered priorities.
        loaders.sort( [](const file_loader_t &l, const file_loader_t &r){
            return (l.priority < r.priority);
        });

        // Attempt to load the files.
        for(const auto &l : loaders){
            if(l_Paths.empty()) break;
            std::stringstream ss;
            for(const auto &e : l.exts) ss << (ss.str().empty() ? "" : ", ") << "'" << e << "'";
            FUNCINFO("Trying loader for extensions: " << ss.str() << " for file(s) with extension '" << ext << "'");
            if(!l_Paths.empty() && !l.f(l_Paths)){
                return false;
            }
        }

        // Return any remaining files to the user's container.
        Paths.splice( std::end(Paths), l_Paths );
    }

    if(!Paths.empty()){
        for(const auto &p : Paths) FUNCWARN("Unloaded file: '" << p.string() << "'");
    }

    return (Paths.empty() && !contained_unresolvable);
}

