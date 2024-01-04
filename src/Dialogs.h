//Dialogs.h -- dialogs for user interaction.

#pragma once

#include <string>
#include <vector>
#include <initializer_list>
#include <functional>
#include <chrono>
#include <filesystem>
#include <any>

namespace pfd {
    class open_file;
    class save_file;
}


class select_files {
    private:
        std::unique_ptr<pfd::open_file> dialog;
        std::chrono::time_point<std::chrono::steady_clock> t_launched;
        std::any user_data;

    public:
        select_files( const std::string &title,
                      const std::filesystem::path &root = std::filesystem::path(),
                      const std::vector<std::string> &filters
                          = std::vector<std::string>{ std::string("All Files"), std::string("*") } );

        ~select_files();

        // User data access.
        void set_user_data(const std::any &);
        std::any get_user_data() const;

        // Synchronously block while attempting to close the dialog.
        void terminate();

        // Asynchronously check if the result is ready.
        bool is_ready() const;

        // Synchronously block until the result is ready.
        //
        // Note that calling this will terminate and invalidate the dialog. It can only be called once.
        std::vector<std::string> get_selection();

};


class select_filename {
    private:
        std::unique_ptr<pfd::save_file> dialog;
        std::chrono::time_point<std::chrono::steady_clock> t_launched;
        std::any user_data;

    public:
        select_filename( const std::string &title,
                         const std::filesystem::path &root = std::filesystem::path(),
                         const std::vector<std::string> &filters
                             = std::vector<std::string>{ std::string("All Files"), std::string("*") } );

        ~select_filename();

        // User data access.
        void set_user_data(const std::any &);
        std::any get_user_data() const;

        // Synchronously block while attempting to close the dialog.
        void terminate();

        // Asynchronously check if the result is ready.
        bool is_ready() const;

        // Synchronously block until the result is ready.
        //
        // Note that calling this will terminate and invalidate the dialog. It can only be called once.
        std::string get_selection();

};


