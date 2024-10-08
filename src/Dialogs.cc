//Dialogs.cc - A part of DICOMautomaton 2021, 2024. Written by hal clark.

#include <string>
#include <vector>
#include <initializer_list>
#include <functional>
#include <chrono>
#include <thread>
#include <filesystem>

#include "Dialogs.h"

// This header interacts negatively with other headers (SDL and ASIO), so only include at the end.
#include "pfd20211102/portable-file-dialogs.h"


// ================================================================================================
// Open files.

// Note: Here is a sample program showing how this class should work.
//
//    #include <string>
//    #include <vector>
//    #include <iostream>
//    
//    #include "Dialogs.h"
//    
//    int main(){
//        std::vector<std::string> filters { std::string("All"), std::string("*") };
//        select_files sf("title", "", filters);
//        while( !sf.is_ready() ){ }
//        for(const auto &f : sf.get_selection()) std::cout << "Selected file '" << f << "'" << std::endl;
//        return 0;
//    }

select_files::select_files( const std::string &title,
                            const std::filesystem::path &root,
                            const std::vector<std::string> &filters ){
    if(!pfd::settings::available()){
        throw std::runtime_error("No dialog options available");
    }

    this->dialog = std::make_unique<pfd::open_file>(title, root.string(), filters, pfd::opt::multiselect);

    // Shouldn't be needed, but ensure a minimal amount of time elapses before the dialog can be queried.
    this->t_launched = std::chrono::steady_clock::now();
}

select_files::~select_files(){
    this->terminate();
}

void
select_files::set_user_data(const std::any &ud){
    this->user_data = ud;
}

std::any
select_files::get_user_data() const {
    return this->user_data;
}

void
select_files::terminate(){
    //if(this->dialog && !this->is_ready()){
    //    this->dialog->kill();
    //}
    this->dialog = nullptr;
}

bool
select_files::is_ready() const {
    if(!this->dialog){
        throw std::runtime_error("Dialog not initialized");
    }

    // Ensure a minimum amount of time has elapsed before actually querying the dialog.
    const auto t_now = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(t_now - t_launched).count();

    return (500 <= elapsed) && this->dialog->ready(10);
}

std::vector<std::string>
select_files::get_selection(){
    if(!this->dialog){
        throw std::runtime_error("Dialog not initialized");
    }

    // Poll until the input is available.
    while(!this->is_ready()){
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
    const auto res = this->dialog->result();
    this->terminate();
    return res;
}


// ================================================================================================
// Select a filename.
select_filename::select_filename( const std::string &title,
                                  const std::filesystem::path &root,
                                  const std::vector<std::string> &filters ){
    if(!pfd::settings::available()){
        throw std::runtime_error("No dialog options available");
    }

    this->dialog = std::make_unique<pfd::save_file>(title, root.string(), filters, pfd::opt::force_overwrite);

    // Shouldn't be needed, but ensure a minimal amount of time elapses before the dialog can be queried.
    this->t_launched = std::chrono::steady_clock::now();
}

select_filename::~select_filename(){
    this->terminate();
}

void
select_filename::set_user_data(const std::any &ud){
    this->user_data = ud;
}

std::any
select_filename::get_user_data() const {
    return this->user_data;
}

void
select_filename::terminate(){
    //if(this->dialog && !this->is_ready()){
    //    this->dialog->kill();
    //}
    this->dialog = nullptr;
}

bool
select_filename::is_ready() const {
    if(!this->dialog){
        throw std::runtime_error("Dialog not initialized");
    }

    // Ensure a minimum amount of time has elapsed before actually querying the dialog.
    const auto t_now = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(t_now - t_launched).count();

    return (500 <= elapsed) && this->dialog->ready(10);
}

std::string
select_filename::get_selection(){
    if(!this->dialog){
        throw std::runtime_error("Dialog not initialized");
    }

    // Poll until the input is available.
    while(!this->is_ready()){
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
    const auto res = this->dialog->result();
    this->terminate();
    return res;
}



// ================================================================================================
// Select a directory.
select_directory::select_directory( const std::string &title,
                                  const std::filesystem::path &root ){
    if(!pfd::settings::available()){
        throw std::runtime_error("No dialog options available");
    }

    this->dialog = std::make_unique<pfd::select_folder>(title, root.string(), pfd::opt::none);

    // Shouldn't be needed, but ensure a minimal amount of time elapses before the dialog can be queried.
    this->t_launched = std::chrono::steady_clock::now();
}

select_directory::~select_directory(){
    this->terminate();
}

void
select_directory::set_user_data(const std::any &ud){
    this->user_data = ud;
}

std::any
select_directory::get_user_data() const {
    return this->user_data;
}

void
select_directory::terminate(){
    //if(this->dialog && !this->is_ready()){
    //    this->dialog->kill();
    //}
    this->dialog = nullptr;
}

bool
select_directory::is_ready() const {
    if(!this->dialog){
        throw std::runtime_error("Dialog not initialized");
    }

    // Ensure a minimum amount of time has elapsed before actually querying the dialog.
    const auto t_now = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(t_now - t_launched).count();

    return (500 <= elapsed) && this->dialog->ready(10);
}

std::string
select_directory::get_selection(){
    if(!this->dialog){
        throw std::runtime_error("Dialog not initialized");
    }

    // Poll until the input is available.
    while(!this->is_ready()){
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
    const auto res = this->dialog->result();
    this->terminate();
    return res;
}


