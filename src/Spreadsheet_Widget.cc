//Spreadsheet_Widget.cc -- a custom ImGui spreadsheet widget for tables::table2.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <sstream>
#include <utility>
#include <vector>

#include "imgui20210904/imgui.h"
#include "imgui20210904/imgui_internal.h"

#include <SDL.h>

#include "YgorLog.h"
#include "YgorMisc.h"

#include "Tables.h"
#include "String_Parsing.h"
#include "Spreadsheet_Widget.h"


Spreadsheet_Widget::Spreadsheet_Widget() = default;

void Spreadsheet_Widget::reset(){
    active_cell_ = {};
    selection_.clear();
    editing_cell_ = {};
    editing_first_frame_ = 0;
    edit_buf_ = {};
    edit_original_ = {};
    pending_chars_.clear();
    focus_cell_ = {};
    resize_to_default_ = true;
    resize_to_fit_ = false;
    undo_stack_.clear();
    redo_stack_.clear();
}

void Spreadsheet_Widget::request_resize_to_default(){
    resize_to_default_ = true;
}

void Spreadsheet_Widget::request_resize_to_fit(){
    resize_to_fit_ = true;
}

std::optional<Spreadsheet_Widget::cell_bounds_t>
Spreadsheet_Widget::get_selection_bounds() const {
    if(selection_.empty()) return {};

    const auto seed = *(std::begin(selection_));
    tables::cell_coord_t row_bounds = { seed.first, seed.first };
    tables::cell_coord_t col_bounds = { seed.second, seed.second };
    for(const auto& c : selection_){
        if(c.first  < row_bounds.first)  row_bounds.first  = c.first;
        if(row_bounds.second < c.first)  row_bounds.second = c.first;
        if(c.second < col_bounds.first)  col_bounds.first  = c.second;
        if(col_bounds.second < c.second) col_bounds.second = c.second;
    }
    return std::make_pair(row_bounds, col_bounds);
}

void Spreadsheet_Widget::commit_edit(tables::table2 &table){
    if(!editing_cell_) return;

    const auto cell = editing_cell_.value();
    const auto new_val = table.value(cell.first, cell.second);

    // Record an undo entry only if the value actually changed.
    if(new_val != edit_original_){
        undo_action_t action;
        undo_entry_t entry;
        entry.cell = cell;
        entry.old_value = edit_original_;
        entry.new_value = new_val;
        action.entries.push_back(entry);
        record_undo(std::move(action));
    }

    editing_cell_ = {};
    editing_first_frame_ = 0;
    pending_chars_.clear();
}

void Spreadsheet_Widget::record_undo(undo_action_t action){
    if(action.entries.empty()) return;
    undo_stack_.push_back(std::move(action));
    redo_stack_.clear();
    while(undo_stack_.size() > max_undo_depth_){
        undo_stack_.erase(undo_stack_.begin());
    }
}

void Spreadsheet_Widget::perform_undo(tables::table2 &table){
    if(undo_stack_.empty()) return;
    auto action = std::move(undo_stack_.back());
    undo_stack_.pop_back();

    undo_action_t redo_action;
    for(auto it = action.entries.rbegin(); it != action.entries.rend(); ++it){
        const auto &e = *it;
        undo_entry_t re;
        re.cell = e.cell;
        re.new_value = e.old_value;
        re.old_value = table.value(e.cell.first, e.cell.second);

        if(e.old_value){
            table.inject(e.cell.first, e.cell.second, e.old_value.value());
        }else{
            table.remove(e.cell.first, e.cell.second);
        }
        redo_action.entries.push_back(re);
    }
    redo_stack_.push_back(std::move(redo_action));
}

void Spreadsheet_Widget::perform_redo(tables::table2 &table){
    if(redo_stack_.empty()) return;
    auto action = std::move(redo_stack_.back());
    redo_stack_.pop_back();

    undo_action_t undo_action;
    for(auto it = action.entries.rbegin(); it != action.entries.rend(); ++it){
        const auto &e = *it;
        undo_entry_t ue;
        ue.cell = e.cell;
        ue.new_value = e.old_value;
        ue.old_value = table.value(e.cell.first, e.cell.second);

        if(e.old_value){
            table.inject(e.cell.first, e.cell.second, e.old_value.value());
        }else{
            table.remove(e.cell.first, e.cell.second);
        }
        undo_action.entries.push_back(ue);
    }
    undo_stack_.push_back(std::move(undo_action));
}


void Spreadsheet_Widget::render(tables::table2 &table, const display_config_t &config){
    ImGuiIO& io = ImGui::GetIO();

    // Measure user input.
    const bool window_is_focused = ImGui::IsWindowFocused( ImGuiFocusedFlags_RootAndChildWindows );

    const bool pressing_shift     = io.KeyShift;
    const bool pressing_ctrl      = io.KeyCtrl;
    const bool pressing_tab       = ImGui::IsKeyPressed(SDL_SCANCODE_TAB);
    const bool pressing_enter     = ImGui::IsKeyPressed(SDL_SCANCODE_RETURN);
    const bool pressing_delete    = ImGui::IsKeyPressed(SDL_SCANCODE_DELETE);
    const bool pressing_backspace = ImGui::IsKeyPressed(SDL_SCANCODE_BACKSPACE);
    const bool pressing_c         = ImGui::IsKeyPressed(SDL_SCANCODE_C);
    const bool pressing_x         = ImGui::IsKeyPressed(SDL_SCANCODE_X);
    const bool pressing_v         = ImGui::IsKeyPressed(SDL_SCANCODE_V);
    const bool pressing_z         = ImGui::IsKeyPressed(SDL_SCANCODE_Z);
    const bool pressing_y         = ImGui::IsKeyPressed(SDL_SCANCODE_Y);

    const bool pressing_up        = ImGui::IsKeyPressed(SDL_SCANCODE_UP);
    const bool pressing_down      = ImGui::IsKeyPressed(SDL_SCANCODE_DOWN);
    const bool pressing_left      = ImGui::IsKeyPressed(SDL_SCANCODE_LEFT);
    const bool pressing_right     = ImGui::IsKeyPressed(SDL_SCANCODE_RIGHT);

    std::string typed_text;
    {
        for(int i = 0; i < io.InputQueueCharacters.Size; ++i){
            // Note: ImGui encodes chars as ImWchar. We accept printable ASCII and Latin-1 (up to 255).
            // Code points beyond this range are replaced with '?'.
            const auto c_wchar = io.InputQueueCharacters[i];
            const char c = (' ' <= c_wchar && c_wchar <= 255) ? static_cast<char>(c_wchar) : '?';
            typed_text.push_back( c );
        }
    }

    // Compute table bounds.
    const auto [tbl_min_col, tbl_max_col] = table.standard_min_max_col();
    const auto [tbl_min_row, tbl_max_row] = table.standard_min_max_row();

    // ImGui currently has a 64-column limit, so truncate extra columns.
    auto l_min_col = tbl_min_col;
    auto l_max_col = tbl_max_col;
    auto l_min_row = tbl_min_row;
    auto l_max_row = tbl_max_row;

    l_min_col = std::clamp<int64_t>(l_min_col, tbl_min_col, tbl_max_col);
    l_max_col = std::clamp<int64_t>(l_max_col, l_min_col, std::min<int64_t>(tbl_max_col, l_min_col + 63));
    l_min_row = std::clamp<int64_t>(l_min_row, tbl_min_row, tbl_max_row);
    l_max_row = std::clamp<int64_t>(l_max_row, l_min_row, std::min<int64_t>(tbl_max_row, l_min_row + 19'999));

    if( (l_min_col != tbl_min_col)
    ||  (l_max_col != tbl_max_col)
    ||  (l_min_row != tbl_min_row)
    ||  (l_max_row != tbl_max_row) ){
        ImGui::Text("Warning: this table is truncated for display purposes");
        ImGui::Separator();
    }

    // Validate state against current table bounds.
    // The table may have changed since the last frame.
    if(active_cell_){
        const auto [r, c] = active_cell_.value();
        if(r < l_min_row || r > l_max_row || c < l_min_col || c > l_max_col){
            active_cell_ = {};
        }
    }
    if(editing_cell_){
        const auto [r, c] = editing_cell_.value();
        if(r < l_min_row || r > l_max_row || c < l_min_col || c > l_max_col){
            editing_cell_ = {};
            editing_first_frame_ = 0;
            pending_chars_.clear();
        }
    }
    {
        auto it = selection_.begin();
        while(it != selection_.end()){
            const auto [r, c] = *it;
            if(r < l_min_row || r > l_max_row || c < l_min_col || c > l_max_col){
                it = selection_.erase(it);
            }else{
                ++it;
            }
        }
    }

    const float TEXT_BASE_WIDTH = ImGui::CalcTextSize("A").x;
    const float TEXT_BASE_HEIGHT = ImGui::CalcTextSize("A").y;

    if( (tbl_min_col >= tbl_max_col)
    ||  (tbl_min_row >= tbl_max_row)
    ||  (l_min_col >= l_max_col)
    ||  (l_min_row >= l_max_row) ){
        return;
    }

    if( !ImGui::BeginTable("Table display",
                           static_cast<int>(l_max_col - l_min_col + 1),
                           ImGuiTableFlags_Borders
                             | ImGuiTableFlags_NoSavedSettings
                             | ImGuiTableFlags_ScrollX
                             | ImGuiTableFlags_ScrollY
                             | ImGuiTableFlags_RowBg
                             | ImGuiTableFlags_BordersV
                             | ImGuiTableFlags_BordersInnerV
                             | ImGuiTableFlags_BordersOuterV
                             | ImGuiTableFlags_SizingFixedFit
                             | ImGuiTableFlags_Hideable
                             | ImGuiTableFlags_Reorderable
                             | ImGuiTableFlags_Resizable )){
        return;
    }

    // Number the columns.
    const float default_col_width = 70.0f;
    const float min_col_width = TEXT_BASE_WIDTH * 3.0f;
    {
        for(int64_t c = l_min_col; c <= l_max_col; ++c){
            std::stringstream ss;
            ss << std::setw(3) << std::to_string(c);
            ImGui::TableSetupColumn(ss.str().c_str(),
                                    ImGuiTableColumnFlags_WidthFixed,
                                    default_col_width);
        }
    }
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableHeadersRow();

    std::array<char, 2048> buf;
    string_to_array(buf, "");

    // Resize column widths.
    if(false){
    }else if(resize_to_default_){
        for(int64_t col = l_min_col; col <= l_max_col; ++col){
            ImGui::TableSetColumnWidth(static_cast<int>(col - l_min_col), default_col_width);
        }
        resize_to_default_ = false;

    }else if(resize_to_fit_){
        std::map<int64_t, float> col_width;
        tables::visitor_func_t f_size = [&](int64_t row, int64_t col, std::string& v) -> tables::action {
            if( (l_min_col <= col) && (col <= l_max_col)
            &&  (l_min_row <= row) && (row <= l_max_row) ){
                const auto w = ImGui::CalcTextSize(v.c_str()).x + TEXT_BASE_WIDTH * 2.0f;
                col_width[col] = std::max<float>(col_width[col], min_col_width);
                col_width[col] = std::max<float>(col_width[col], w);
            }
            return tables::action::automatic;
        };
        table.visit_standard_block(f_size);

        for(int64_t col = l_min_col; col <= l_max_col; ++col){
            ImGui::TableSetColumnWidth(static_cast<int>(col - l_min_col), col_width[col]);
        }
        resize_to_fit_ = false;
    }

    // Eliminate dead zones in the grid where the mouse cannot click.
    const auto cell_padding = ImGui::GetStyle().CellPadding;

    // Hide the default keyboard navigation.
    const ImVec4 hidden_colour(1.0f, 1.0f, 1.0f, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_NavHighlight, hidden_colour);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, hidden_colour);
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, hidden_colour);

    // Visit each cell and render the contents.
    tables::visitor_func_t f = [&](int64_t row, int64_t col, std::string& v) -> tables::action {
        if( (l_min_col <= col) && (col <= l_max_col)
        &&  (l_min_row <= row) && (row <= l_max_row) ){
            ImGui::TableNextColumn();
            string_to_array(buf, v);
            const bool buf_holds_full_v = ((v.size() + 1U) < buf.size());

            // This ID ensures the table can grow with cells retaining their ID's. With the
            // display limits of 20,000 rows and 64 columns, the max value is
            // 19,999 + 63 * 100,000 = 6,319,999, which fits safely in int32_t.
            int cell_ID = static_cast<int>((row - l_min_row) + (col - l_min_col) * 100'000);
            ImGui::PushID(cell_ID);

            const auto available_space = ImGui::GetContentRegionAvail();
            std::optional<ImVec2> cell_actual_pos_min;
            std::optional<ImVec2> cell_actual_pos_max;

            const auto cell_rc = std::make_pair(row, col);
            const bool is_active = active_cell_ && (active_cell_.value() == cell_rc);
            const bool is_group_selected = (selection_.count(cell_rc) != 0UL);
            const bool is_editing = editing_cell_ ? (editing_cell_.value() == cell_rc) : false;
            bool key_changed = false;

            if(is_editing){
                // Inject pending characters from first-keypress capture.
                if(!pending_chars_.empty()){
                    ImVector<ImWchar> old_q;
                    old_q.swap(io.InputQueueCharacters);
                    for(const char c : pending_chars_){
                        io.AddInputCharacter(static_cast<unsigned int>(static_cast<unsigned char>(c)));
                    }
                    for(int i = 0; i < old_q.Size; ++i){
                        io.InputQueueCharacters.push_back(old_q[i]);
                    }
                    pending_chars_.clear();
                }

                // Draw editable text.
                if( 0L < editing_first_frame_ ){
                    ImGui::SetKeyboardFocusHere();
                }
                ImGui::SetNextItemWidth( available_space.x );
                key_changed = ImGui::InputText("##datum", buf.data(), buf.size() - 1);

                // Check if still editing (focus + active). If not, stop editing in the next frame.
                const bool still_editing = !ImGui::IsItemDeactivated();

                if( is_active
                &&  ImGui::IsItemVisible() ){
                    cell_actual_pos_min = ImGui::GetItemRectMin();
                    cell_actual_pos_max = ImGui::GetItemRectMax();
                }

                if(false){
                }else if( 0L < editing_first_frame_ ){
                    // Debounce: skip navigation keypresses on the first frame(s) of editing,
                    // because these keypresses may have triggered the navigation to this cell.
                    --editing_first_frame_;

                }else if( pressing_tab && pressing_shift ){
                    commit_edit(table);
                    const auto next = std::make_pair(row, col - 1L);
                    editing_cell_ = next;
                    editing_first_frame_ = 2L;
                    active_cell_ = next;
                    selection_.clear();
                    selection_.insert(next);

                }else if( pressing_tab ){
                    commit_edit(table);
                    const auto next = std::make_pair(row, col + 1L);
                    editing_cell_ = next;
                    editing_first_frame_ = 2L;
                    active_cell_ = next;
                    selection_.clear();
                    selection_.insert(next);

                }else if( pressing_enter && pressing_shift ){
                    commit_edit(table);
                    const auto next = std::make_pair(row - 1L, col);
                    editing_cell_ = next;
                    editing_first_frame_ = 2L;
                    active_cell_ = next;
                    selection_.clear();
                    selection_.insert(next);

                }else if( pressing_enter ){
                    commit_edit(table);
                    const auto next = std::make_pair(row + 1L, col);
                    editing_cell_ = next;
                    editing_first_frame_ = 2L;
                    active_cell_ = next;
                    selection_.clear();
                    selection_.insert(next);

                }else if(!still_editing){
                    commit_edit(table);
                }

            }else{
                // Draw selectable text.
                const ImGuiSelectableFlags selectable_flags = ImGuiSelectableFlags_None;
                const ImVec2 selectable_size( 0.0f, TEXT_BASE_HEIGHT + cell_padding.y);

                if(ImGui::Selectable(buf.data(), is_active, selectable_flags, selectable_size)){
                    active_cell_ = cell_rc;

                    if(false){
                    }else if(pressing_shift){
                        // Rectangular selection.
                        selection_.insert(cell_rc);
                        const auto bounds_opt = get_selection_bounds();
                        if(bounds_opt){
                            const auto [row_bounds, col_bounds] = bounds_opt.value();
                            for(int64_t r = row_bounds.first; r <= row_bounds.second; ++r){
                                for(int64_t c_i = col_bounds.first; c_i <= col_bounds.second; ++c_i){
                                    selection_.insert( std::make_pair(r, c_i) );
                                }
                            }
                        }

                    }else if( pressing_ctrl ){
                        // Toggle selection for one cell.
                        if(is_group_selected){
                            selection_.erase(cell_rc);
                        }else{
                            selection_.insert(cell_rc);
                        }

                    }else{
                        // Exclusive selection of one cell.
                        selection_.clear();
                        selection_.insert(cell_rc);
                    }
                }

                // Move scroll to the cell iff directed.
                if( focus_cell_
                &&  (focus_cell_.value() == cell_rc) ){
                    ImGui::SetScrollHereX();
                    ImGui::SetScrollHereY();
                    focus_cell_ = {};
                }

                // Set bounding box coordinates for the cell.
                if( is_active
                &&  ImGui::IsItemVisible() ){
                    cell_actual_pos_min = ImGui::GetItemRectMin();
                    cell_actual_pos_max = ImGui::GetItemRectMax();
                }

                // Check if text is hovered, active, and the mouse was double-clicked.
                bool is_double_clicked = false;
                for(int i = 0; i < IM_ARRAYSIZE(io.MouseDown); i++){
                    if(ImGui::IsMouseDoubleClicked(i)){
                        is_double_clicked = true;
                    }
                }
                const bool enter_edit_mouse =   ImGui::IsItemActive()
                                             && ImGui::IsItemHovered()
                                             && ImGui::IsItemVisible()
                                             && ImGui::IsItemClicked()
                                             && is_double_clicked;
                const bool enter_edit_keybd =   window_is_focused
                                             && ImGui::IsItemVisible()
                                             && is_active
                                             && !typed_text.empty()
                                             && !pressing_ctrl;
                const bool enter_edit_enter =   window_is_focused
                                             && ImGui::IsItemVisible()
                                             && is_active
                                             && pressing_enter
                                             && !pressing_ctrl
                                             && !pressing_shift;

                if( enter_edit_mouse || enter_edit_enter ){
                    // Double-click or Enter: enter edit mode with existing content.
                    edit_original_ = table.value(cell_rc.row, cell_rc.col);
                    editing_cell_ = cell_rc;
                    editing_first_frame_ = 1L;
                    selection_.erase(cell_rc);
                }else if( enter_edit_keybd ){
                    // Keyboard typing: enter edit mode with fresh content.
                    edit_original_ = table.value(cell_rc.row, cell_rc.col);
                    v = "";   // Clear cell so InputText starts with empty buffer.
                    pending_chars_ = typed_text;
                    editing_cell_ = cell_rc;
                    editing_first_frame_ = 1L;
                    selection_.erase(cell_rc);
                }
            }

            // Colourize if keywords are present.
            if(config.use_keyword_highlighting){
                for(const auto &kv : config.colours){
                    if(v == kv.first){
                        ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, ImGui::GetColorU32(kv.second));
                        break;
                    }
                }
            }

            // Colourize selected cells.
            if(is_group_selected){
                ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, ImGui::GetColorU32(config.selected_colour));
            }

            // Draw yellow border on the active cell.
            if( is_active
            &&  cell_actual_pos_min
            &&  cell_actual_pos_max ){
                auto drawlist = ImGui::GetWindowDrawList();
                drawlist->AddRect(cell_actual_pos_min.value(),
                                  cell_actual_pos_max.value(),
                                  IM_COL32(255, 255, 0, 255));
            }

            // Draw thin yellow border on other selected cells (not the active cell).
            if( !is_active
            &&  is_group_selected
            &&  ImGui::IsItemVisible() ){
                auto drawlist = ImGui::GetWindowDrawList();
                drawlist->AddRect(ImGui::GetItemRectMin(),
                                  ImGui::GetItemRectMax(),
                                  IM_COL32(255, 255, 0, 128));
            }

            ImGui::PopID();

            if( key_changed
            &&  buf_holds_full_v ) array_to_string(v, buf);
        }
        return tables::action::automatic;
    };
    table.visit_standard_block(f);

    ImGui::PopStyleColor(); // ImGuiCol_NavHighlight
    ImGui::PopStyleColor(); // ImGuiCol_HeaderHovered
    ImGui::PopStyleColor(); // ImGuiCol_HeaderActive


    // Helper function for jump navigation selection.
    const auto insert_cells_between = [&](const tables::cell_coord_t &A,
                                          const tables::cell_coord_t &B) -> void {
        const auto min_r = std::min(A.first, B.first);
        const auto max_r = std::max(A.first, B.first);
        const auto min_c = std::min(A.second, B.second);
        const auto max_c = std::max(A.second, B.second);

        for(auto r = min_r; r <= max_r; ++r){
            for(auto c = min_c; c <= max_c; ++c){
                selection_.insert( std::make_pair(r, c) );
            }
        }
        return;
    };

    // Check for keyboard actions.
    if( window_is_focused
    &&  !editing_cell_ ){

        // Undo.
        if( pressing_ctrl
        &&  pressing_z
        &&  !pressing_shift ){
            perform_undo(table);

        // Redo.
        }else if( pressing_ctrl
              &&  pressing_y ){
            perform_redo(table);

        // Delete the selection.
        }else if( (pressing_delete || pressing_backspace)
              &&  !selection_.empty() ){
            undo_action_t action;
            for(const auto& c : selection_){
                const auto [r, col] = c;
                const auto old_val = table.value(r, col);
                if(old_val){
                    undo_entry_t entry;
                    entry.cell = c;
                    entry.old_value = old_val;
                    entry.new_value = {};
                    action.entries.push_back(entry);
                }
                table.remove(r, col);
            }
            record_undo(std::move(action));

        // Copy the selection.
        }else if( pressing_ctrl
              &&  pressing_c
              &&  !selection_.empty() ){
            const auto bounds_opt = get_selection_bounds();
            if(bounds_opt){
                const auto [row_bounds, col_bounds] = bounds_opt.value();
                std::ostringstream os;
                table.write_csv(os, '\t', row_bounds, col_bounds);
                const auto selection_csv = os.str();
                ImGui::SetClipboardText(selection_csv.c_str());
                YLOGINFO("Copied rectangular selection to clipboard");
            }

        // Cut the selection.
        }else if( pressing_ctrl
              &&  pressing_x
              &&  !selection_.empty() ){
            const auto bounds_opt = get_selection_bounds();
            if(bounds_opt){
                const auto [row_bounds, col_bounds] = bounds_opt.value();
                std::ostringstream os;
                table.write_csv(os, '\t', row_bounds, col_bounds);
                const auto selection_csv = os.str();
                ImGui::SetClipboardText(selection_csv.c_str());

                undo_action_t action;
                for(const auto& c : selection_){
                    const auto [r, col] = c;
                    const auto old_val = table.value(r, col);
                    if(old_val){
                        undo_entry_t entry;
                        entry.cell = c;
                        entry.old_value = old_val;
                        entry.new_value = {};
                        action.entries.push_back(entry);
                    }
                    table.remove(r, col);
                }
                record_undo(std::move(action));
                YLOGINFO("Cut rectangular selection to clipboard");
            }

        // Paste the selection.
        }else if( pressing_ctrl
              &&  pressing_v
              &&  active_cell_ ){
            char *c_txt = SDL_GetClipboardText();
            const std::string txt(c_txt);
            SDL_free(c_txt);
            try{
                tables::table2 t;
                std::stringstream ss(txt);
                t.read_csv( ss );
                const auto mmr = t.min_max_row();
                const auto mmc = t.min_max_col();
                const auto [row_offset, col_offset] = active_cell_.value();

                undo_action_t action;
                tables::visitor_func_t l_f = [&](int64_t r, int64_t col, std::string& val) -> tables::action {
                    const auto dest_r = r - mmr.first + row_offset;
                    const auto dest_c = col - mmc.first + col_offset;

                    undo_entry_t entry;
                    entry.cell = std::make_pair(dest_r, dest_c);
                    entry.old_value = table.value(dest_r, dest_c);
                    entry.new_value = val;
                    action.entries.push_back(entry);

                    table.inject(dest_r, dest_c, val);
                    return tables::action::automatic;
                };
                t.visit_block(mmr, mmc, l_f);
                record_undo(std::move(action));
                YLOGINFO("Pasted rectangular region from clipboard");

            }catch(const std::exception &e){
                YLOGWARN("Unable to parse tabular data from clipboard: " << e.what());
            }

        // Jump navigation over multiple cells, optionally adding to the selection.
        }else if( pressing_ctrl
              &&  active_cell_
              &&  pressing_up ){
            const auto inc = std::make_pair<int64_t,int64_t>(-1L, 0L);
            const auto jump = table.jump_navigate(active_cell_.value(), inc);
            if( pressing_shift ) insert_cells_between(active_cell_.value(), jump);
            else{ selection_.clear(); selection_.insert(jump); }
            active_cell_ = jump;
            focus_cell_ = active_cell_;
        }else if( pressing_ctrl
              &&  active_cell_
              &&  pressing_down ){
            const auto inc = std::make_pair<int64_t,int64_t>(1L, 0L);
            const auto jump = table.jump_navigate(active_cell_.value(), inc);
            if( pressing_shift ) insert_cells_between(active_cell_.value(), jump);
            else{ selection_.clear(); selection_.insert(jump); }
            active_cell_ = jump;
            focus_cell_ = active_cell_;
        }else if( pressing_ctrl
              &&  active_cell_
              &&  pressing_left ){
            const auto inc = std::make_pair<int64_t,int64_t>(0L, -1L);
            const auto jump = table.jump_navigate(active_cell_.value(), inc);
            if( pressing_shift ) insert_cells_between(active_cell_.value(), jump);
            else{ selection_.clear(); selection_.insert(jump); }
            active_cell_ = jump;
            focus_cell_ = active_cell_;
        }else if( pressing_ctrl
              &&  active_cell_
              &&  pressing_right ){
            const auto inc = std::make_pair<int64_t,int64_t>(0L, 1L);
            const auto jump = table.jump_navigate(active_cell_.value(), inc);
            if( pressing_shift ) insert_cells_between(active_cell_.value(), jump);
            else{ selection_.clear(); selection_.insert(jump); }
            active_cell_ = jump;
            focus_cell_ = active_cell_;

        // Navigate the selected cell one cell over, optionally extending the selection.
        }else if( active_cell_
              &&  pressing_up ){
            const auto [r, c] = active_cell_.value();
            const auto jump = std::make_pair(std::clamp(r - 1L, l_min_row, l_max_row), c);
            if( pressing_shift ){
                selection_.insert(active_cell_.value());
                selection_.insert(jump);
            }else{
                selection_.clear();
                selection_.insert(jump);
            }
            active_cell_ = jump;
            focus_cell_ = active_cell_;
        }else if( active_cell_
              &&  pressing_down ){
            const auto [r, c] = active_cell_.value();
            const auto jump = std::make_pair(std::clamp(r + 1L, l_min_row, l_max_row), c);
            if( pressing_shift ){
                selection_.insert(active_cell_.value());
                selection_.insert(jump);
            }else{
                selection_.clear();
                selection_.insert(jump);
            }
            active_cell_ = jump;
            focus_cell_ = active_cell_;
        }else if( active_cell_
              &&  pressing_left ){
            const auto [r, c] = active_cell_.value();
            const auto jump = std::make_pair(r, std::clamp(c - 1L, l_min_col, l_max_col));
            if( pressing_shift ){
                selection_.insert(active_cell_.value());
                selection_.insert(jump);
            }else{
                selection_.clear();
                selection_.insert(jump);
            }
            active_cell_ = jump;
            focus_cell_ = active_cell_;
        }else if( active_cell_
              &&  pressing_right ){
            const auto [r, c] = active_cell_.value();
            const auto jump = std::make_pair(r, std::clamp(c + 1L, l_min_col, l_max_col));
            if( pressing_shift ){
                selection_.insert(active_cell_.value());
                selection_.insert(jump);
            }else{
                selection_.clear();
                selection_.insert(jump);
            }
            active_cell_ = jump;
            focus_cell_ = active_cell_;

        // Tab/Enter navigation without editing.
        }else if( active_cell_
              &&  pressing_tab
              &&  pressing_shift ){
            const auto [r, c] = active_cell_.value();
            active_cell_ = std::make_pair(r, c - 1L);
            selection_.clear();
            selection_.insert(active_cell_.value());
            focus_cell_ = active_cell_;
        }else if( active_cell_
              &&  pressing_tab ){
            const auto [r, c] = active_cell_.value();
            active_cell_ = std::make_pair(r, c + 1L);
            selection_.clear();
            selection_.insert(active_cell_.value());
            focus_cell_ = active_cell_;
        }
    }

    ImGui::EndTable();
}

