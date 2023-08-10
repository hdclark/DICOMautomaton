//Table.h -- a minimal 2D spreadsheet / 'stringly-typed' sparse matrix class.

#pragma once

#include <set>
#include <map>
#include <string>
#include <optional>
#include <functional>
#include <utility>
#include <istream>
#include <ostream>

namespace tables {

template<class T>
class cell {
    private:
        int64_t row;
        int64_t col;

    public:
        T val;

        cell();
        cell(int64_t r, int64_t c, const T& v);

        int64_t get_row() const;
        int64_t get_col() const;

        bool operator==(const cell &) const;
        bool operator!=(const cell &) const;
        bool operator<(const cell &) const; 
};

// Controls how cells are treated during iteration.
enum class action {
    automatic, // automatically prune empty cells, add non-empty cells.
    remove,
    add,
};

using visitor_func_t = std::function< action (int64_t r, int64_t c, std::string& v)>;

struct table2 {
    //std::set< std::variant<cell<std::string>,
    //                       cell<double>,
    //                       cell<int64_t>>> data;
    std::set< cell<std::string> > data;

    std::map<std::string, std::string> metadata;

    // Constructor.
    table2();

    // These functions return the (inclusive) bounds of the content currently in the table.
    std::pair<int64_t, int64_t> min_max_row() const;
    std::pair<int64_t, int64_t> min_max_col() const;

    // These functions return the (inclusive) bounds spanning
    // [r,c] = [min(0,min_row), min(0,min_col)] to [max(10,max_row+5), max(5,max_col+2)].
    // which helps when the table needs to expand to the bottom-right.
    std::pair<int64_t, int64_t> standard_min_max_row() const;
    std::pair<int64_t, int64_t> standard_min_max_col() const;

    // These functions locate the next empty row or column to the bottom/right of all existing rows/columns.
    // Note that holes in the interior are ignored. They are useful for appending data.
    int64_t next_empty_row() const;
    int64_t next_empty_col() const;

    // Overwrite existing or insert new cell.
    void inject(int64_t row, int64_t col, const std::string& val);

    // Remove existing cell, if present.
    void remove(int64_t row, int64_t col);

    // Const value extraction.
    std::optional<std::string> value(int64_t row, int64_t col) const;

    // Optional is disengaged if cell does not exist.
    std::optional<std::reference_wrapper<std::string>> value_ref(int64_t row, int64_t col);

    // Visits every cell within the bounds (inclusive), even if not active.
    // Whether the cell should be engaged or disengaged after iteration is controlled by the user functor.
    void visit_block( const std::pair<int64_t, int64_t>& row_bounds,
                      const std::pair<int64_t, int64_t>& col_bounds,
                      const visitor_func_t& f );
    
    // Same as previous, but visits the 'standard' block (see above).
    void visit_standard_block( const visitor_func_t& f );

    // Read from a stream.
    //
    // Purges any existing cells and metadata, and does not read metadata from the file.
    // Also accepts TSV files (auto-detects tabs in the first few lines).
    // Throws on error or if nothing was read. Should work equally well with binary and text mode streams.
    void read_csv( std::istream &is );

    // Write to a stream.
    //
    // Quotes cells for maxmimum portability. Best to use with binary streams to avoid platform-specific line endings.
    // Throws on error. Disregards all metadata. Defaults to 'standard' bounds (see above).
    void write_csv( std::ostream &os,
                    char separator = ',', // Also accepts tabs.
                    std::optional<std::pair<int64_t, int64_t>> row_bounds = {},
                    std::optional<std::pair<int64_t, int64_t>> col_bounds = {} ) const;

};

} // namespace tables

