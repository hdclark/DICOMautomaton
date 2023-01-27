//Write_File.cc - A part of DICOMautomaton 2018. Written by hal clark.

#include <boost/interprocess/creation_tags.hpp>
#include <boost/interprocess/sync/named_mutex.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>

#include <fstream>
#include <stdexcept>
#include <string>    
#include <memory>
#include <filesystem>

#include "YgorFilesDirs.h"
#include "YgorMisc.h"
#include "YgorLog.h"

#include "Write_File.h"

interprocess_lock::interprocess_lock(const std::string& n) : mutex(boost::interprocess::open_or_create, n.c_str()), name(n) {
    YLOGINFO("Attempting to lock named mutex '" << this->name << "'");
    boost::interprocess::scoped_lock<boost::interprocess::named_mutex> lock(this->mutex);
    this->lock.swap(lock);
}

interprocess_lock::~interprocess_lock(){
    YLOGINFO("Released lock on named mutex '" << this->name << "'");
}

std::unique_ptr<interprocess_lock>
Make_File_Lock(const std::string& name){
    return std::make_unique<interprocess_lock>(name);
}


std::filesystem::path
Generate_Unique_tmp_Filename(const std::string &basename,
                             const std::string &suffix){
    const auto basepath = std::filesystem::temp_directory_path() / basename;
    const auto fullpath = Get_Unique_Sequential_Filename(basepath.string(), 6, suffix);
    return fullpath;
}


void Append_File( const std::function<std::string(void)>& gen_file_name,
                  const std::string& mutex_name,
                  const std::string& iff_newfile,
                  const std::string& body ){
                 
    //File-based locking is used so this program can be run over many patients concurrently.
    // Try open a named mutex. Probably created in /dev/shm/ if you need to clear it manually...
    YLOGINFO("About to claim mutex '" << mutex_name << "'");
    boost::interprocess::named_mutex mutex(boost::interprocess::open_or_create, mutex_name.c_str());
    boost::interprocess::scoped_lock<boost::interprocess::named_mutex> lock(mutex);

    const auto file_name = gen_file_name();

    const auto FirstWrite = !Does_File_Exist_And_Can_Be_Read(file_name);
    std::fstream FO(file_name, std::fstream::out | std::fstream::app);
    if(!FO){
        throw std::runtime_error("Unable to open file for writing. Cannot continue.");
    }
    if(FirstWrite){ 
        // Write a header or notice if the file is new.
        FO << iff_newfile;
    }
    FO << body;
    FO.flush();
    FO.close();

    const std::string msg = (FirstWrite ? "Wrote to new file" : "Appended to existing file");
    YLOGINFO(msg << " '" << file_name << "'");

    return;
}
