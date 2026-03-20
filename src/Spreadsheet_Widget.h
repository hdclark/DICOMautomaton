//Spreadsheet_Widget.h -- a custom ImGui spreadsheet widget for tables::table2.

#pragma once

#include <array>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <cstdint>

#include "Tables.h"

#include "imgui20210904/imgui.h"


class Spreadsheet_Widget {
    public:
        // Display configuration passed in by the caller each frame.
        struct display_config_t {
            bool use_keyword_highlighting = true;
            std::map<std::string, ImVec4> colours;
            ImVec4 selected_colour = ImVec4(0.260f, 0.590f, 0.980f, 0.50f);
        };

        Spreadsheet_Widget();

        // Render the spreadsheet grid for the given table.
        // Call once per frame while the table should be displayed.
        // The caller must hold any necessary locks.
        void render(tables::table2 &table, const display_config_t &config);

        // Reset all selection and editing state (e.g., when switching tables).
        void reset();

        // Request column resizing on the next render.
        void request_resize_to_default();
        void request_resize_to_fit();

    private:
        // Cell selection.
        std::optional<tables::cell_coord_t> active_cell_;
        std::set<tables::cell_coord_t> selection_;

        // Cell editing.
        std::optional<tables::cell_coord_t> editing_cell_;
        int64_t editing_first_frame_ = 0;
        std::optional<std::string> edit_original_;  // value when editing began; nullopt if cell did not exist.

        // Characters pending injection (for first-keypress handling).
        std::string pending_chars_;

        // Scroll/focus request.
        std::optional<tables::cell_coord_t> focus_cell_;

        // Column sizing.
        bool resize_to_default_ = true;
        bool resize_to_fit_ = false;

        // Undo/redo.
        struct undo_entry_t {
            tables::cell_coord_t cell;
            std::optional<std::string> old_value; // nullopt = cell did not exist.
            std::optional<std::string> new_value; // nullopt = cell was removed.
        };
        struct undo_action_t {
            std::vector<undo_entry_t> entries;
        };
        std::vector<undo_action_t> undo_stack_;
        std::vector<undo_action_t> redo_stack_;
        static constexpr size_t max_undo_depth_ = 200;

        // Helpers.
        using cell_bounds_t = std::pair<tables::cell_coord_t, tables::cell_coord_t>;
        std::optional<cell_bounds_t> get_selection_bounds() const;

        void commit_edit(tables::table2 &table);
        void perform_undo(tables::table2 &table);
        void perform_redo(tables::table2 &table);
        void record_undo(undo_action_t action);
};

