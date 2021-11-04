//Dialogs.h -- dialogs for user interaction.

#pragma once

#include <string>
#include <vector>
#include <initializer_list>
#include <functional>
#include <chrono>

namespace pfd {
    class open_file;
}


class select_files {
    private:
        std::unique_ptr<pfd::open_file> dialog;
        std::chrono::time_point<std::chrono::steady_clock> t_launched;

    public:
        select_files( const std::string &title,
                      const std::string &root = "",
                      const std::vector<std::string> &filters
                          = std::vector<std::string>{ std::string("All Files"), std::string("*") } );

        ~select_files();

        // Synchronously block while attempting to close the dialog.
        void terminate();

        // Asynchronously check if the result is ready.
        bool is_ready() const;

        // Synchronously block until the result is ready.
        //
        // Note that calling this will terminate and invalidate the dialog. It can only be called once.
        std::vector<std::string> get_selection();

};


