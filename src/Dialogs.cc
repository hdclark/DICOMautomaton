//Dialogs.cc - A part of DICOMautomaton 2021. Written by hal clark.

#include <string>
#include <vector>
#include <initializer_list>
#include <functional>
#include <chrono>
#include <thread>

#include "Dialogs.h"

// This header interacts negatively with other headers (SDL and ASIO), so only include at the end.
#include "pfd20211102/portable-file-dialogs.h"

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
                            const std::string &root,
                            const std::vector<std::string> &filters ){
    if(!pfd::settings::available()){
        throw std::runtime_error("No dialog options available");
    }

    this->dialog = std::make_unique<pfd::open_file>(title, root, filters, pfd::opt::multiselect);

    // Shouldn't be needed, but ensure a minimal amount of time elapses before the dialog can be queried.
    this->t_launched = std::chrono::steady_clock::now();
}

select_files::~select_files(){
    this->terminate();
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


