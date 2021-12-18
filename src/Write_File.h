//Write_File.h - A part of DICOMautomaton 2018. Written by hal clark.

#pragma once

#include <boost/interprocess/creation_tags.hpp>
#include <boost/interprocess/sync/named_mutex.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>

#include <functional>
#include <memory>
#include <string>
#include <filesystem>


// A lock that can be used to coordinate separate processes running on the same computer.
// Mostly used for synchronizing appends to a file.
struct interprocess_lock {
    boost::interprocess::named_mutex mutex;
    boost::interprocess::scoped_lock<boost::interprocess::named_mutex> lock;
    std::string name;

    interprocess_lock(const std::string& name);
    ~interprocess_lock();
};

std::unique_ptr<interprocess_lock>
Make_File_Lock(const std::string& name);

// Generate a filename in a system-specific temporary directory.
std::filesystem::path
Generate_Unique_tmp_Filename(const std::string &basename,
                             const std::string &suffix);

// This routine will write text to a file, protecting the write with a semaphore from concurrrent processes.
// The filename is claimed after the semaphore is acquired to avoid a race condition.
void Append_File( const std::function<std::string(void)>& gen_file_name,
                  const std::string& mutex_name,
                  const std::string& iff_newfile,
                  const std::string& body );
