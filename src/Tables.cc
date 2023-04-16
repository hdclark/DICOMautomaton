
#include <set>
#include <string>
#include <iostream>
#include <limits>
#include <utility>
#include <istream>
#include <ostream>
#include <iomanip>
#include <cstdint>

#include "YgorString.h"
#include "YgorMisc.h"
#include "YgorLog.h"

#include "Tables.h"

//#ifndef DCMA_TABLE_DISABLE_ALL_SPECIALIZATIONS
//    #define DCMA_TABLE_DISABLE_ALL_SPECIALIZATIONS
//#endif

namespace tables {

// cell class.

template <class T>
cell<T>::cell() : row(-1), col(-1) {}
#ifndef DCMA_TABLE_DISABLE_ALL_SPECIALIZATIONS
    template cell<double     >::cell();
    template cell<int64_t    >::cell();
    template cell<std::string>::cell();
#endif

template <class T>
cell<T>::cell(int64_t r, int64_t c, const T& v) : row(r), col(c), val(v) {}
#ifndef DCMA_TABLE_DISABLE_ALL_SPECIALIZATIONS
    template cell<double     >::cell(int64_t, int64_t, const double     &);
    template cell<int64_t    >::cell(int64_t, int64_t, const int64_t    &);
    template cell<std::string>::cell(int64_t, int64_t, const std::string&);
#endif

template <class T>
bool
cell<T>::operator==(const cell& rhs) const {
    return (this->row == rhs.row) && (this->col == rhs.col);
}
#ifndef DCMA_TABLE_DISABLE_ALL_SPECIALIZATIONS
    template bool cell<double     >::operator==(const cell&) const;
    template bool cell<int64_t    >::operator==(const cell&) const;
    template bool cell<std::string>::operator==(const cell&) const;
#endif

template <class T>
bool
cell<T>::operator!=(const cell& rhs) const {
    return !(*this == rhs);
}
#ifndef DCMA_TABLE_DISABLE_ALL_SPECIALIZATIONS
    template bool cell<double     >::operator!=(const cell&) const;
    template bool cell<int64_t    >::operator!=(const cell&) const;
    template bool cell<std::string>::operator!=(const cell&) const;
#endif

template <class T>
bool
cell<T>::operator<(const cell& rhs) const {
    // Make the primary sorting axis the row number.
    // This makes it easier to append new rows, which is expected to happen more
    // frequently than appending new columns.
    return (this->row == rhs.row) ? (this->col < rhs.col)
                                  : (this->row < rhs.row);
}
#ifndef DCMA_TABLE_DISABLE_ALL_SPECIALIZATIONS
    template bool cell<double     >::operator<(const cell&) const;
    template bool cell<int64_t    >::operator<(const cell&) const;
    template bool cell<std::string>::operator<(const cell&) const;
#endif

template <class T>
int64_t
cell<T>::get_row() const {
    return this->row;
}
#ifndef DCMA_TABLE_DISABLE_ALL_SPECIALIZATIONS
    template int64_t cell<double     >::get_row() const;
    template int64_t cell<int64_t    >::get_row() const;
    template int64_t cell<std::string>::get_row() const;
#endif

template <class T>
int64_t
cell<T>::get_col() const {
    return this->col;
}
#ifndef DCMA_TABLE_DISABLE_ALL_SPECIALIZATIONS
    template int64_t cell<double     >::get_col() const;
    template int64_t cell<int64_t    >::get_col() const;
    template int64_t cell<std::string>::get_col() const;
#endif


// table2 class.

table2::table2(){};

std::pair<int64_t, int64_t>
table2::min_max_row() const {
    int64_t min = std::numeric_limits<int64_t>::max();
    int64_t max = std::numeric_limits<int64_t>::lowest();
    if(const auto it = std::rbegin(this->data); it != std::rend(this->data)){
        max = it->get_row();
    }
    if(const auto it = std::begin(this->data); it != std::end(this->data)){
        min = it->get_row();
    }
    if(max < min){
        throw std::runtime_error("No data available, min and max rows are not defined");
    }
    return {min, max};
}

std::pair<int64_t, int64_t>
table2::min_max_col() const {
    int64_t min = std::numeric_limits<int64_t>::max();
    int64_t max = std::numeric_limits<int64_t>::lowest();

    for(const auto& c : this->data){
        const auto col = c.get_col();
        max = std::max(max, col);
        min = std::min(min, col);
    }
    if(max < min){
        throw std::runtime_error("No data available, min and max columns are not defined");
    }
    return {min, max};
}

std::pair<int64_t, int64_t>
table2::standard_min_max_row() const {
    const int64_t zero = 0;
    const int64_t ten = 10;
    if(this->data.empty()){
        return { zero, ten };
    }
    auto [min_row, max_row] = this->min_max_row();
    return { std::min( zero, min_row ), std::max( ten, max_row + 5 ) };
}

std::pair<int64_t, int64_t>
table2::standard_min_max_col() const {
    const int64_t zero = 0;
    const int64_t five = 5;
    if(this->data.empty()){
        return { zero, five };
    }
    auto [min_col, max_col] = this->min_max_col();
    return { std::min( zero, min_col ), std::max( five, max_col + 2 ) };
}

std::optional<std::string>
table2::value(int64_t row, int64_t col) const {
    std::optional<std::string> out;
    auto it = this->data.find( cell<std::string>(row, col, "") );
    if(it != std::end(this->data)){
        out = it->val;
    }
    return out;
}

std::optional<std::reference_wrapper<std::string>>
table2::value_ref(int64_t row, int64_t col){
    std::optional<std::reference_wrapper<std::string>> out;
    auto it = this->data.find( cell<std::string>(row, col, "") );
    if(it != std::end(this->data)){
        // NOTE: std::set does not care about the cell's content, but doesn't know this.
        // So we have to cast away the const on the value. Do NOT cast away the const
        // on the row or column numbers!
        out = std::ref(const_cast<std::string&>(it->val));
    }
    return out;
}

int64_t
table2::next_empty_row() const {
    int64_t out = 0;
    if(const auto it = std::rbegin(this->data); it != std::rend(this->data)){
        out = it->get_row() + 1;
    }
    return out;
}

int64_t
table2::next_empty_col() const {
    int64_t out = 0;
    for(const auto& c : this->data){
        out = std::max(out, c.get_col() + 1);
    }
    return out;
}

void
table2::inject(int64_t row, int64_t col, const std::string& val){
    cell<std::string> n(row, col, val);
    auto it = this->data.find( n );
    if(it != std::end(this->data)){
        (const_cast<cell<std::string>&>(*it)).val = val;
    }else{
        this->data.insert( n );
    }
    return;
}

void
table2::remove(int64_t row, int64_t col){
    cell<std::string> n(row, col, "");
    this->data.erase(n);
}

void
table2::visit_block( const std::pair<int64_t, int64_t>& row_bounds,
                     const std::pair<int64_t, int64_t>& col_bounds,
                     const visitor_func_t& f ){
    if(!f){
        throw std::invalid_argument("Invalid user functor");
    }
    for(int64_t row = row_bounds.first; row <= row_bounds.second; ++row){
        for(int64_t col = col_bounds.first; col <= col_bounds.second; ++col){
            cell<std::string> shtl(row, col, "");
            cell<std::string>* cell_ptr = &shtl;
            
            auto it = this->data.find( shtl );
            const bool cell_already_present = (it != std::end(this->data));
            if(cell_already_present){
                cell_ptr = &(const_cast<cell<std::string>&>(*it));
            }

            const auto res = f(row, col, cell_ptr->val);

            if(cell_already_present){
                if(false){
                }else if(res == action::remove){
                    this->data.erase(it);
                }else if(res == action::automatic){
                    if(cell_ptr->val.empty()){
                        this->data.erase(it);
                    }
                }

            }else{
                if(false){
                }else if(res == action::add){
                    this->data.insert(*cell_ptr);
                }else if(res == action::automatic){
                    if(!(cell_ptr->val.empty())){
                        this->data.insert(*cell_ptr);
                    }
                }
            }
        }
    }
    return;
}

void
table2::visit_standard_block( const visitor_func_t& f ){
    this->visit_block( this->standard_min_max_row(),
                       this->standard_min_max_col(),
                       f );
    return;
}

void
table2::read_csv( std::istream &is ){
    this->data.clear();
    this->metadata.clear();

    const std::string quotes = "\"";  // Characters that open a quote at beginning of line only.
    const std::string escs   = "\\";  // The escape character(s) inside quotes.
    const std::string pseps  = "\t";  // 'Priority' separation characters. If detected, these take priority over others.
    std::string seps   = ",";   // The characters that separate cells.

    std::stringstream ss;

    // --- Automatic separator detection ---
    const int64_t autodetect_separator_rows = 10;

    // Buffer the input stream into a stringstream, checking for the presence of priority separators.
    bool use_pseps = false;
    for(int64_t i = 0; i < autodetect_separator_rows; ++i){
        std::string line;
        if(std::getline(is, line)){
            // Avoid newline at end, which can appear as an extra empty row later.
            ss << ((i==0) ? "" : "\n") << line;

            const bool has_pseps = (line.find_first_of(pseps) != std::string::npos);
            if(has_pseps){
                use_pseps = true;
                break;
            }
        }
    }
    if(use_pseps){
        seps = pseps;
        YLOGINFO("Detected alternative separators, switching acceptable separators");
    }

    // -------------------------------------

    const auto clean_string = [](const std::string &in){
        return Canonicalize_String2(in, CANONICALIZE::TRIM_ENDS);
    };

    int64_t row_num = -1;
    std::string line;
    while(std::getline(ss, line) || std::getline(is, line)){
        ++row_num;
        bool inside_quote;
        std::string cell;

        int64_t col_num = 0;
        auto c_it = std::begin(line);
        const auto end = std::end(line);
        for( ; c_it != end; ++c_it){
            const bool is_quote = (quotes.find_first_of(*c_it) != std::string::npos);
            const bool is_print = std::isprint(static_cast<unsigned char>(*c_it));
            auto c_next_it = std::next(c_it);

            if(inside_quote){
                const bool is_esc = (escs.find_first_of(*c_it) != std::string::npos);

                if(is_quote){
                    // Close the quote.
                    inside_quote = false;

                }else if(is_esc){
                    // Implement escape of next character.
                    if(c_next_it == end){
                        throw std::runtime_error("Nothing to escape (row "_s + std::to_string(row_num) + ")");
                    }
                    cell.push_back( *c_next_it );
                    ++c_it;
                    ++c_next_it;

                }else if(is_print){
                    cell.push_back( *c_it );
                }
                
            }else{
                const bool is_sep = (seps.find_first_of(*c_it) != std::string::npos);

                if(is_quote){
                    // Open the quote.
                    inside_quote = true;

                }else if( is_sep ){
                    // Push cell into table.
                    cell = clean_string(cell);
                    if(!cell.empty()) this->inject(row_num, col_num, cell);
                    ++col_num;
                    cell.clear();

                }else if(is_print){
                    cell.push_back( *c_it );
                }
            }

            if( c_next_it == end ){
                // Push cell into table.
                cell = clean_string(cell);
                if(!cell.empty()) this->inject(row_num, col_num, cell);
                ++col_num;
                cell.clear();
            }
        }

        // Add any outstanding contents as a cell.
        cell = clean_string(cell);
        if( !cell.empty()
        ||  inside_quote ){
            throw std::invalid_argument("Unable to parse row "_s + std::to_string(row_num));
        }
    }

    if(this->data.empty()){
        throw std::runtime_error("Unable to extract any data from file");
    }
    return;
}

void
table2::write_csv( std::ostream &os ) const {
    const auto [row_min, row_max] = this->standard_min_max_row();
    const auto [col_min, col_max] = this->standard_min_max_col();
    const char quote = '"';
    const char esc = '\\';
    const char sep = ',';

    for(int64_t row = row_min; row <= row_max; ++row){
        for(int64_t col = col_min; col <= col_max; ++col){
            const auto val = this->value(row, col).value_or("");
            if(!val.empty()) os << std::quoted(val, quote, esc);
            os << sep;
        }
        os << "\n";
    }
    os.flush();
    if(!os){
        throw std::runtime_error("Unable to write file");
    }
    return;
}


} // namespace tables


/*

// Sample usage

int main(){

    tables::table2 t;
    t.data.emplace(12, 23, "test cell 1");
    t.data.emplace(123, 234, "test cell 2");

    const auto [min_row, max_row] = t.min_max_row();
    const auto [min_col, max_col] = t.min_max_row();

    std::cout << "Current min/max row: " << min_row << "/" << max_row << std::endl;
    std::cout << "Current min/max col: " << min_col << "/" << max_col << std::endl;

    std::cout << "Next empty row: " << t.next_empty_row() << std::endl;
    std::cout << "Next empty col: " << t.next_empty_col() << std::endl;

    std::cout << "Is (1, 2) present? " << !!t.value(1, 2) << std::endl;
    std::cout << "Is (12, 23) present? " << !!t.value(12, 23) << std::endl;
    std::cout << "Value of cell (12, 23): '" << t.value(12, 23).value().get() << "'" << std::endl;

    std::cout << "Number of cells prior to visitation: " << t.data.size() << std::endl;

    tables::visitor_func_t f_1 = [](int64_t row, int64_t col, std::string& v) -> tables::action {
        if(!v.empty()){
            std::cout << "Visiting non-empty cell " << row << ", " << col << " containing '" << v << "'" << std::endl;
            v += " (visited)";
        }
        if( (row == 50) && (col == 75) ){
            v += "(visitor added this cell)";
        }
        if( (row == 123) && (col == 234) ){
            v.clear(); // This cell will now be removed via the automatic action.
        }
        return tables::action::automatic; // Retain only non-empty cells.
    };

    t.visit_standard_block(f_1);
    std::cout << "Number of cells after visitation (automatic): " << t.data.size() << std::endl;

    tables::visitor_func_t f_2 = [](int64_t row, int64_t col, std::string& v) -> tables::action {
        return tables::action::add; // Add all cells, even if empty.
    };

    t.visit_standard_block(f_2);
    std::cout << "Number of cells after visitation (add): " << t.data.size() << std::endl;


    tables::visitor_func_t f_3 = [](int64_t row, int64_t col, std::string& v) -> tables::action {
        return tables::action::remove; // Remove all cells, even if not empty.
    };

    t.visit_standard_block(f_3);
    std::cout << "Number of cells after visitation (remove): " << t.data.size() << std::endl;

    return 0;
}
*/
