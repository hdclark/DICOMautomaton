//PolyominoesAI.cc - A part of DICOMautomaton 2025. Written by hal clark.

#include <any>
#include <optional>
#include <functional>
#include <iterator>
#include <list>
#include <map>
#include <algorithm>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>    
#include <cstdint>
#include <vector>
#include <limits>
#include <cmath>

#include "YgorImages.h"
#include "YgorString.h"       //Needed for GetFirstRegex(...)

#include "../Structs.h"
#include "../Regex_Selectors.h"

#include "PolyominoesAI.h"


OperationDoc OpArgDocPolyominoesAI(){
    OperationDoc out;
    out.name = "PolyominoesAI";

    out.tags.emplace_back("category: image processing");
    out.tags.emplace_back("category: simulation");

    out.desc = 
        "This operation provides an AI player for the Polyominoes game."
        " It analyzes the current game state and determines the optimal placement for the"
        " currently moving polyomino using heuristics."
        " The recommended action sequence is stored in image metadata for human interpretation,"
        " and can optionally be applied automatically.";
    
    out.notes.emplace_back(
        "This operation should be invoked after a polyomino game has been initialized with the Polyominoes operation."
        " It reads the current game state from image metadata and evaluates possible placements."
    );
    out.notes.emplace_back(
        "The AI uses a heuristic-based approach that considers: aggregate height of the board,"
        " number of complete lines, number of holes (empty cells covered by filled cells),"
        " and bumpiness (variation in column heights). These heuristics are weighted to find optimal placements."
    );
    out.notes.emplace_back(
        "The recommended action sequence is stored in the 'PolyominoesAIRecommendedActions' metadata field"
        " as a human-readable comma-separated list (e.g., 'rotate-clockwise,translate-left,translate-left,drop')."
    );
    
    out.args.emplace_back();
    out.args.back() = IAWhitelistOpArgDoc();
    out.args.back().name = "ImageSelection";
    out.args.back().default_val = "last";

    out.args.emplace_back();
    out.args.back().name = "Channel";
    out.args.back().desc = "The image channel to analyze (zero-based)."
                           " Negative values will cause all channels to be operated on.";
    out.args.back().default_val = "0";
    out.args.back().expected = true;
    out.args.back().examples = { "-1", "0", "1" };

    out.args.emplace_back();
    out.args.back().name = "Low";
    out.args.back().desc = "The voxel value that represents 'inactive' cells (same as Polyominoes operation).";
    out.args.back().default_val = "0.0";
    out.args.back().expected = true;
    out.args.back().examples = { "0.0", "-1.23", "10.0" };

    out.args.emplace_back();
    out.args.back().name = "High";
    out.args.back().desc = "The voxel value that represents 'active' cells (same as Polyominoes operation).";
    out.args.back().default_val = "1.0";
    out.args.back().expected = true;
    out.args.back().examples = { "1.5", "-0.23", "255.0" };

    out.args.emplace_back();
    out.args.back().name = "WeightAggregateHeight";
    out.args.back().desc = "Heuristic weight for the aggregate height of all columns."
                           " Higher (more positive) weights penalize tall stacks."
                           " Typical value: -0.5 to -1.0.";
    out.args.back().default_val = "-0.51";
    out.args.back().expected = true;
    out.args.back().examples = { "-0.5", "-0.51", "-1.0" };

    out.args.emplace_back();
    out.args.back().name = "WeightCompleteLines";
    out.args.back().desc = "Heuristic weight for the number of complete lines that would be cleared."
                           " Higher (more positive) weights reward line completions."
                           " Typical value: 0.5 to 1.0.";
    out.args.back().default_val = "0.76";
    out.args.back().expected = true;
    out.args.back().examples = { "0.5", "0.76", "1.0" };

    out.args.emplace_back();
    out.args.back().name = "WeightHoles";
    out.args.back().desc = "Heuristic weight for the number of holes (empty cells below filled cells)."
                           " Lower (more negative) weights penalize holes."
                           " Typical value: -0.3 to -1.0.";
    out.args.back().default_val = "-0.36";
    out.args.back().expected = true;
    out.args.back().examples = { "-0.3", "-0.36", "-0.5" };

    out.args.emplace_back();
    out.args.back().name = "WeightBumpiness";
    out.args.back().desc = "Heuristic weight for bumpiness (sum of absolute differences between adjacent column heights)."
                           " Lower (more negative) weights reward smoother surfaces."
                           " Typical value: -0.1 to -0.5.";
    out.args.back().default_val = "-0.18";
    out.args.back().expected = true;
    out.args.back().examples = { "-0.1", "-0.18", "-0.5" };

    return out;
}

bool PolyominoesAI(Drover &DICOM_data,
                   const OperationArgPkg& OptArgs,
                   std::map<std::string, std::string>& /*InvocationMetadata*/,
                   const std::string& /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();
    const auto Channel = std::stol( OptArgs.getValueStr("Channel").value() );

    const auto Low = std::stod( OptArgs.getValueStr("Low").value() );
    const auto High = std::stod( OptArgs.getValueStr("High").value() );

    const auto WeightAggregateHeight = std::stod( OptArgs.getValueStr("WeightAggregateHeight").value() );
    const auto WeightCompleteLines   = std::stod( OptArgs.getValueStr("WeightCompleteLines").value() );
    const auto WeightHoles           = std::stod( OptArgs.getValueStr("WeightHoles").value() );
    const auto WeightBumpiness       = std::stod( OptArgs.getValueStr("WeightBumpiness").value() );

    //-----------------------------------------------------------------------------------------------------------------
    const auto Threshold = (High * 0.5) + (Low * 0.5);

    const std::string moving_poly_pos_row_str = "MovingPolyominoPositionRow";
    const std::string moving_poly_pos_col_str = "MovingPolyominoPositionColumn";
    const std::string moving_poly_family_str  = "MovingPolyominoFamily";
    const std::string moving_poly_shape_str   = "MovingPolyominoShape";
    const std::string moving_poly_orien_str   = "MovingPolyominoOrientation";
    const std::string ai_recommended_str      = "PolyominoesAIRecommendedActions";
    const std::string ai_best_col_str         = "PolyominoesAIBestColumn";
    const std::string ai_best_orien_str       = "PolyominoesAIBestOrientation";
    const std::string ai_best_score_str       = "PolyominoesAIBestScore";

    //-----------------------------------------------------------------------------------------------------------------

    auto IAs_all = All_IAs( DICOM_data );
    auto IAs = Whitelist( IAs_all, ImageSelectionStr );

    using img_t = planar_image<float, double>;
    using coord_t = std::array<int64_t, 2>;

    const auto select_channels = [](const img_t& img, int64_t x){
        std::set<int64_t> channels;
        if(x < 0){
            for(int64_t i = 0; i < img.channels; ++i) channels.insert(i);
        }else if(x < img.channels){
            channels.insert(x);
        }else{
            throw std::runtime_error("Channel selection not present in image.");
        }
        return channels;
    };

    // Polyomino definitions - must match Polyominoes.cc exactly.
    // omino family (size=5); omino shape (size varies); distinct orientation (size varies); coordinates.
    std::array< std::vector<std::vector<std::vector<coord_t>>>, 5> valid_ominoes {{

    {{
        // Monomonoes:
        {
           { { 0, 0 } }
        }
    }},

    {{
        // Dominoes:
        {
           { { 0, 0 }, { 0, 1 } },
           { { -1, 0 },
             {  0, 0 } }
        }
    }},

    {{
        // Trominoes:
        {
           { { 0, -1 }, { 0, 0 }, { 0, 1 } },
           { { -1, 0 },
             {  0, 0 },
             {  1, 0 } }
        },
        {
           { { 0, 0 },   { 0, 1 },
             { 1, 0 } },
           { { 0, -1 },  { 0, 0 },
                         { 1, 0 } },
                       { { -1, 0 },
             { 0, -1 },  {  0, 0 } },
           { { -1, 0 },
             {  0, 0 }, { 0, 1 } },
        }
    }},

    {{
        // Tetrominoes:
        {  // 1. O-piece
           { { 0, 0 }, { 0, 1 },
             { 1, 0 }, { 1, 1 } }
        },
        {  // 2. I-piece
           { { 0, -1 }, { 0, 0 }, { 0, 1 }, { 0, 2 } },
           { { -1, 0 },
             {  0, 0 },
             {  1, 0 },
             {  2, 0 } }
        },
        {  // 3. S-piece
           { { 0, -1 }, { 0, 0 },
                        { 1, 0 }, { 1, 1 } },
                       { { -1, 0 },
             {  0, -1 }, {  0, 0 },
             {  1, -1 } }
        },
        {  // 4. Z-piece
                      { { 0, 0 }, { 0, 1 },
             { 1, -1 }, { 1, 0 } },
           { { -1,  0 },
             {  0,  0 }, {  0,  1 },
                         {  1,  1 } }
        },
        {  // 5. L-piece
           { { -1, 0 },
             {  0, 0 },
             {  1, 0 }, { 1, 1 } },
           { { 0, -1 }, { 0, 0 }, { 0, 1 },
             { 1, -1 } },
           { { -1, -1 }, { -1, 0 },
                         {  0, 0 },
                         {  1, 0 } },
                                { { -1, 1 },
             { 0, -1 }, { 0, 0 }, {  0, 1 } }
        },
        {  // 6. J-piece
                      { { -1, 0 },
                        {  0, 0 },
             { 1, -1 }, {  1, 0 } },
           { { -1, -1 },
             {  0, -1 }, { 0, 0 }, { 0, 1 } },
           { { -1, 0 }, { -1, 1 },
             {  0, 0 },
             {  1, 0 } },
           { { 0, -1 }, { 0, 0 }, { 0, 1 },
                                  { 1, 1 } }
        },
        {  // 7. T-piece
           { { -1, 0 },
             {  0, 0 }, { 0, 1 },
             {  1, 0 } },
           { { 0, -1 }, { 0, 0 }, { 0, 1 },
                        { 1, 0 } },
                      { { -1, 0 },
             { 0, -1 }, {  0, 0 },
                        {  1, 0 } },
                      { { -1, 0 },
             { 0, -1 }, {  0, 0 }, { 0, 1 } }
        }
    }},
    {{
        // Pentominoes (18 shapes):
        {  // 1. Plus/X-piece
                       { { -1, 0 },
             {  0, -1 }, {  0, 0 }, {  0, 1 },
                         {  1, 0 } }
        },
        {  // 2. I-piece
           { { 0, -2 }, { 0, -1 }, { 0, 0 }, { 0, 1 }, { 0, 2 } },
           { { -2, 0 }, { -1, 0 }, { 0, 0 }, { 1, 0 }, { 2, 0 } }
        },
        {  // 3. F-piece (variant 1)
           { { -1,  0 }, { -1, 1 },
             {  0, -1 }, {  0, 0 },
                         {  1, 0 } },
           { { 0, 1 }, { 1, 1 }, { -1, 0 }, { 0, 0 }, { 0, -1 } },
           { { 1, 0 }, { 1, -1 }, { 0, 1 }, { 0, 0 }, { -1, 0 } },
           { { 0, -1 }, { -1, -1 }, { 1, 0 }, { 0, 0 }, { 0, 1 } }
        },
        {  // 4. F-piece (variant 2)
           { { -1, -1 }, { -1, 0 },
                         {  0, 0 }, {  0, 1 },
                         {  1, 0 } },
           { { -1, 1 }, { 0, 1 }, { 0, 0 }, { 1, 0 }, { 0, -1 } },
           { { 1, 1 }, { 1, 0 }, { 0, 0 }, { 0, -1 }, { -1, 0 } },
           { { 1, -1 }, { 0, -1 }, { 0, 0 }, { -1, 0 }, { 0, 1 } }
        },
        {  // 5. L-piece (variant 1)
                       { { -2, 0 },
                         { -1, 0 },
                         {  0, 0 },
             {  1, -1 }, {  1, 0 } },
           { { 0, 2 }, { 0, 1 }, { 0, 0 }, { -1, -1 }, { 0, -1 } },
           { { 2, 0 }, { 1, 0 }, { 0, 0 }, { -1, 1 }, { -1, 0 } },
           { { 0, -2 }, { 0, -1 }, { 0, 0 }, { 1, 1 }, { 0, 1 } }
        },
        {  // 6. L-piece (variant 2)
                       { { -2, 0 },
                         { -1, 0 },
                         {  0, 0 },
                         {  1, 0 }, { 1, 1 } },
           { { 0, 2 }, { 0, 1 }, { 0, 0 }, { 0, -1 }, { 1, -1 } },
           { { 2, 0 }, { 1, 0 }, { 0, 0 }, { -1, 0 }, { -1, -1 } },
           { { 0, -2 }, { 0, -1 }, { 0, 0 }, { 0, 1 }, { -1, 1 } }
        },
        {  // 7. P-piece (variant 1)
           { { -1, -1 }, { -1, 0 },
             {  0, -1 }, {  0, 0 },
                         {  1, 0 } },
           { { -1, 1 }, { 0, 1 }, { -1, 0 }, { 0, 0 }, { 0, -1 } },
           { { 1, 1 }, { 1, 0 }, { 0, 1 }, { 0, 0 }, { -1, 0 } },
           { { 1, -1 }, { 0, -1 }, { 1, 0 }, { 0, 0 }, { 0, 1 } }
        },
        {  // 8. P-piece (variant 2)
           { { -1, 0 }, { -1, 1 },
             {  0, 0 }, {  0, 1 },
             {  1, 0 } },
           { { 0, 1 }, { 1, 1 }, { 0, 0 }, { 1, 0 }, { 0, -1 } },
           { { 1, 0 }, { 1, -1 }, { 0, 0 }, { 0, -1 }, { -1, 0 } },
           { { 0, -1 }, { -1, -1 }, { 0, 0 }, { -1, 0 }, { 0, 1 } }
        },
        {  // 9. N-piece (variant 1)
                       { { -2, 0 },
                         { -1, 0 },
             {  0, -1 }, {  0, 0 },
             {  1, -1 } },
           { { 0, 2 }, { 0, 1 }, { -1, 0 }, { 0, 0 }, { -1, -1 } },
           { { 2, 0 }, { 1, 0 }, { 0, 1 }, { 0, 0 }, { -1, 1 } },
           { { 0, -2 }, { 0, -1 }, { 1, 0 }, { 0, 0 }, { 1, 1 } }
        },
        {  // 10. N-piece (variant 2)
           { { -2, 0 },
             { -1, 0 },
             {  0, 0 }, {  0, 1 },
                        {  1, 1 } },
           { { 0, 2 }, { 0, 1 }, { 0, 0 }, { 1, 0 }, { 1, -1 } },
           { { 2, 0 }, { 1, 0 }, { 0, 0 }, { 0, -1 }, { -1, -1 } },
           { { 0, -2 }, { 0, -1 }, { 0, 0 }, { -1, 0 }, { -1, 1 } }
        },
        {  // 11. Y-piece
           { { -1, -1 }, { -1, 0 }, { -1, 1 },
                         {  0, 0 },
                         {  1, 0 } },
           { { -1, 1 }, { 0, 1 }, { 1, 1 }, { 0, 0 }, { 0, -1 } },
           { { 1, 1 }, { 1, 0 }, { 1, -1 }, { 0, 0 }, { -1, 0 } },
           { { 1, -1 }, { 0, -1 }, { -1, -1 }, { 0, 0 }, { 0, 1 } }
        },
        {  // 12. U-piece
           { { -1, -1 },            { -1, 1 },
             {  0, -1 }, {  0, 0 }, {  0, 1 } },
           { { -1, 1 }, { 1, 1 }, { -1, 0 }, { 0, 0 }, { 1, 0 } },
           { { 1, 1 }, { 1, -1 }, { 0, 1 }, { 0, 0 }, { 0, -1 } },
           { { 1, -1 }, { -1, -1 }, { 1, 0 }, { 0, 0 }, { -1, 0 } }
        },
        {  // 13. V-piece
                                   { { -2, 0 },
                                     { -1, 0 },
             {  0, -2 }, {  0, -1 }, {  0, 0 } },
           { { 0, 2 }, { 0, 1 }, { -2, 0 }, { -1, 0 }, { 0, 0 } },
           { { 2, 0 }, { 1, 0 }, { 0, 2 }, { 0, 1 }, { 0, 0 } },
           { { 0, -2 }, { 0, -1 }, { 2, 0 }, { 1, 0 }, { 0, 0 } }
        },
        {  // 14. W-piece
                                  { { -1, 1 },
                         {  0, 0 }, {  0, 1 },
             {  1, -1 }, {  1, 0 } },
           { { 1, 1 }, { 0, 0 }, { 1, 0 }, { -1, -1 }, { 0, -1 } },
           { { 1, -1 }, { 0, 0 }, { 0, -1 }, { -1, 1 }, { -1, 0 } },
           { { -1, -1 }, { 0, 0 }, { -1, 0 }, { 1, 1 }, { 0, 1 } }
        },
        {  // 15. Z-piece (variant 1)
                       { { -1, 0 },
             {  0, -1 }, {  0, 0 },
                         {  1, 0 },
                         {  2, 0 } },
           { { 0, 1 }, { -1, 0 }, { 0, 0 }, { 0, -1 }, { 0, -2 } },
           { { 1, 0 }, { 0, 1 }, { 0, 0 }, { -1, 0 }, { -2, 0 } },
           { { 0, -1 }, { 1, 0 }, { 0, 0 }, { 0, 1 }, { 0, 2 } }
        },
        {  // 16. Z-piece (variant 2)
           { { -1, 0 },
             {  0, 0 }, {  0, 1 },
             {  1, 0 },
             {  2, 0 } },
           { { 0, 1 }, { 0, 0 }, { 1, 0 }, { 0, -1 }, { 0, -2 } },
           { { 1, 0 }, { 0, 0 }, { 0, -1 }, { -1, 0 }, { -2, 0 } },
           { { 0, -1 }, { 0, 0 }, { -1, 0 }, { 0, 1 }, { 0, 2 } }
        },
        {  // 17. S-piece (variant 1)
                       { { -1, 0 }, { -1, 1 },
                         {  0, 0 },
             {  1, -1 }, {  1, 0 } },
           { { 0, 1 }, { 1, 1 }, { 0, 0 }, { -1, -1 }, { 0, -1 } },
           { { 1, 0 }, { 1, -1 }, { 0, 0 }, { -1, 1 }, { -1, 0 } },
           { { 0, -1 }, { -1, -1 }, { 0, 0 }, { 1, 1 }, { 0, 1 } }
        },
        {  // 18. S-piece (variant 2)
           { { -1, -1 }, { -1, 0 },
                         {  0, 0 },
                         {  1, 0 }, {  1, 1 } },
           { { -1, 1 }, { 0, 1 }, { 0, 0 }, { 0, -1 }, { 1, -1 } },
           { { 1, 1 }, { 1, 0 }, { 0, 0 }, { -1, 0 }, { -1, -1 } },
           { { 1, -1 }, { 0, -1 }, { 0, 0 }, { 0, 1 }, { -1, 1 } }
        }
    }}
    }};

    // Cell value primitives.
    const auto cell_is_active = [Threshold]( const img_t& img,
                                             int64_t row,
                                             int64_t col,
                                             int64_t chn ) -> bool {
        return (Threshold < img.value(row, col, chn));
    };

    // Confirm poly shape and orientation are plausible.
    const auto poly_desc_bounded = [&valid_ominoes]( int64_t poly_family,
                                                     int64_t poly_shape,
                                                     int64_t poly_orien ) -> bool {
        return (0L <= poly_family)
            && (0L <= poly_shape)
            && (0L <= poly_orien)
            && (poly_family < static_cast<int64_t>(valid_ominoes.size()))
            && (poly_shape < static_cast<int64_t>(valid_ominoes.at(poly_family).size()))
            && (poly_orien < static_cast<int64_t>(valid_ominoes.at(poly_family).at(poly_shape).size()));
    };

    // Convert from position coordinates, the poly family, shape, and orientation to absolute image pixel coordinates.
    const auto resolve_abs_coords = [&valid_ominoes]( const img_t& /*img*/,
                                                      int64_t poly_family,
                                                      int64_t poly_shape,
                                                      int64_t poly_orien,
                                                      int64_t poly_pos_row,
                                                      int64_t poly_pos_col ) -> std::vector<coord_t> {
        auto l_coords = valid_ominoes.at(poly_family).at(poly_shape).at(poly_orien);
        for(auto &c : l_coords){
            c.at(0) += poly_pos_row;
            c.at(1) += poly_pos_col;
        }
        return l_coords;
    };

    // Boolean tests for absolute coordinates.
    const auto abs_coords_valid = []( const img_t& img,
                                      const std::vector<coord_t> &abs_coords ) -> bool {
        for(const auto &c : abs_coords){
            if( (c.at(0) < 0L) || (img.rows <= c.at(0))
            ||  (c.at(1) < 0L) || (img.columns <= c.at(1)) ){
                return false;
            }
        }
        return true;
    };

    // Structure to hold a candidate placement.
    struct Placement {
        int64_t target_col;
        int64_t target_orien;
        double score;
        std::string action_sequence;
    };

    // Heuristic evaluation function for a board state.
    // Returns a score where higher is better.
    const auto evaluate_board = [&cell_is_active,
                                 WeightAggregateHeight,
                                 WeightCompleteLines,
                                 WeightHoles,
                                 WeightBumpiness]( const img_t& img,
                                                   int64_t chn,
                                                   const std::vector<coord_t>& piece_coords ) -> double {
        // Create a temporary board state with the piece placed.
        std::vector<std::vector<bool>> board(img.rows, std::vector<bool>(img.columns, false));
        for(int64_t r = 0; r < img.rows; ++r){
            for(int64_t c = 0; c < img.columns; ++c){
                board[r][c] = cell_is_active(img, r, c, chn);
            }
        }
        // Add the piece to the board.
        for(const auto& coord : piece_coords){
            if(coord.at(0) >= 0 && coord.at(0) < img.rows &&
               coord.at(1) >= 0 && coord.at(1) < img.columns){
                board[coord.at(0)][coord.at(1)] = true;
            }
        }

        // Calculate column heights (from top).
        std::vector<int64_t> column_heights(img.columns, 0);
        for(int64_t c = 0; c < img.columns; ++c){
            for(int64_t r = 0; r < img.rows; ++r){
                if(board[r][c]){
                    column_heights[c] = img.rows - r;
                    break;
                }
            }
        }

        // 1. Aggregate height.
        int64_t aggregate_height = 0;
        for(int64_t c = 0; c < img.columns; ++c){
            aggregate_height += column_heights[c];
        }

        // 2. Complete lines.
        int64_t complete_lines = 0;
        for(int64_t r = 0; r < img.rows; ++r){
            bool complete = true;
            for(int64_t c = 0; c < img.columns; ++c){
                if(!board[r][c]){
                    complete = false;
                    break;
                }
            }
            if(complete) ++complete_lines;
        }

        // 3. Holes (empty cells below filled cells).
        int64_t holes = 0;
        for(int64_t c = 0; c < img.columns; ++c){
            bool found_block = false;
            for(int64_t r = 0; r < img.rows; ++r){
                if(board[r][c]){
                    found_block = true;
                }else if(found_block){
                    ++holes;
                }
            }
        }

        // 4. Bumpiness (sum of absolute differences between adjacent column heights).
        int64_t bumpiness = 0;
        for(int64_t c = 0; c < img.columns - 1; ++c){
            bumpiness += std::abs(column_heights[c] - column_heights[c + 1]);
        }

        // Calculate weighted score.
        const double score = WeightAggregateHeight * static_cast<double>(aggregate_height)
                           + WeightCompleteLines * static_cast<double>(complete_lines)
                           + WeightHoles * static_cast<double>(holes)
                           + WeightBumpiness * static_cast<double>(bumpiness);
        return score;
    };

    // Simulate dropping a piece at a given column and orientation.
    // Returns the final row position and the corresponding coordinates after dropping as far as possible.
    const auto simulate_drop = [&valid_ominoes,
                                &resolve_abs_coords,
                                &abs_coords_valid,
                                &cell_is_active]( const img_t& img,
                                                  int64_t chn,
                                                  int64_t poly_family,
                                                  int64_t poly_shape,
                                                  int64_t poly_orien,
                                                  int64_t target_col,
                                                  const std::vector<coord_t>& current_piece_coords ) -> std::pair<int64_t, std::vector<coord_t>> {
        // Start from the top row.
        // Find the minimum row offset for this orientation to determine starting row.
        const auto& orien_coords = valid_ominoes.at(poly_family).at(poly_shape).at(poly_orien);
        int64_t min_row_offset = 0;
        for(const auto& c : orien_coords){
            if(c.at(0) < min_row_offset) min_row_offset = c.at(0);
        }
        int64_t start_row = -min_row_offset;

        // Drop until collision.
        int64_t final_row = start_row;
        for(int64_t row = start_row; row < img.rows; ++row){
            auto test_coords = resolve_abs_coords(img, poly_family, poly_shape, poly_orien, row, target_col);
            
            // Check if all coords are valid.
            if(!abs_coords_valid(img, test_coords)){
                break;
            }
            
            // Check if all coords are empty (not colliding with existing pieces).
            bool can_place = true;
            for(const auto& tc : test_coords){
                // Skip if this coord is part of the current piece.
                bool is_current = false;
                for(const auto& cc : current_piece_coords){
                    if(tc.at(0) == cc.at(0) && tc.at(1) == cc.at(1)){
                        is_current = true;
                        break;
                    }
                }
                if(!is_current && cell_is_active(img, tc.at(0), tc.at(1), chn)){
                    can_place = false;
                    break;
                }
            }
            
            if(!can_place){
                break;
            }
            final_row = row;
        }

        auto final_coords = resolve_abs_coords(img, poly_family, poly_shape, poly_orien, final_row, target_col);

        // Ensure we do not return a colliding placement when no valid drop row exists.
        // If any of the final coordinates overlap active cells, mark the placement as invalid
        // by moving the coordinates out-of-bounds so that downstream bounds checks reject it.
        bool placement_collides = false;
        for(const auto& c : final_coords){
            const auto row = c.at(0);
            const auto col = c.at(1);
            if(cell_is_active(img, chn, row, col)){
                placement_collides = true;
                break;
            }
        }
        if(placement_collides){
            final_row = -1;
            for(auto& c : final_coords){
                c.at(0) = -1; // row out-of-bounds; caller's abs_coords_valid should reject
            }
        }
        return {final_row, final_coords};
    };

    // Generate action sequence to move from current state to target state.
    const auto generate_action_sequence = [&valid_ominoes]( int64_t current_col,
                                                             int64_t current_orien,
                                                             int64_t target_col,
                                                             int64_t target_orien,
                                                             int64_t poly_family,
                                                             int64_t poly_shape ) -> std::string {
        std::string actions;
        const int64_t n_oriens = static_cast<int64_t>(valid_ominoes.at(poly_family).at(poly_shape).size());
        
        // Calculate rotation actions (prefer shortest path).
        int64_t clockwise_rotations = (target_orien - current_orien + n_oriens) % n_oriens;
        int64_t counter_rotations = (current_orien - target_orien + n_oriens) % n_oriens;
        
        if(clockwise_rotations <= counter_rotations){
            for(int64_t i = 0; i < clockwise_rotations; ++i){
                if(!actions.empty()) actions += ",";
                actions += "rotate-clockwise";
            }
        }else{
            for(int64_t i = 0; i < counter_rotations; ++i){
                if(!actions.empty()) actions += ",";
                actions += "rotate-counter-clockwise";
            }
        }
        
        // Calculate translation actions.
        int64_t col_diff = target_col - current_col;
        if(col_diff < 0){
            for(int64_t i = 0; i < -col_diff; ++i){
                if(!actions.empty()) actions += ",";
                actions += "translate-left";
            }
        }else if(col_diff > 0){
            for(int64_t i = 0; i < col_diff; ++i){
                if(!actions.empty()) actions += ",";
                actions += "translate-right";
            }
        }
        
        // Add drop action.
        if(!actions.empty()) actions += ",";
        actions += "drop";
        
        return actions;
    };


    for(auto & iap_it : IAs){
        for(auto& img : (*iap_it)->imagecoll.images){
            for(const auto chn : select_channels(img, Channel)){

                // Read current moving piece state from metadata.
                auto moving_poly_pos_row = img.GetMetadataValueAs<int64_t>(moving_poly_pos_row_str);
                auto moving_poly_pos_col = img.GetMetadataValueAs<int64_t>(moving_poly_pos_col_str);
                auto moving_poly_family  = img.GetMetadataValueAs<int64_t>(moving_poly_family_str);
                auto moving_poly_shape   = img.GetMetadataValueAs<int64_t>(moving_poly_shape_str);
                auto moving_poly_orien   = img.GetMetadataValueAs<int64_t>(moving_poly_orien_str);

                // Check if there is a moving piece.
                if( !moving_poly_pos_row
                ||  !moving_poly_pos_col
                ||  !moving_poly_family
                ||  !moving_poly_shape
                ||  !moving_poly_orien ){
                    // No moving piece - nothing to do.
                    img.metadata[ai_recommended_str] = "none";
                    continue;
                }

                // Validate the piece descriptor.
                if( !poly_desc_bounded( moving_poly_family.value(),
                                        moving_poly_shape.value(),
                                        moving_poly_orien.value() ) ){
                    throw std::runtime_error("Moving omino descriptor invalid, unable to continue");
                }

                // Get current piece coordinates.
                const auto current_coords = resolve_abs_coords(img, moving_poly_family.value(),
                                                                    moving_poly_shape.value(),
                                                                    moving_poly_orien.value(),
                                                                    moving_poly_pos_row.value(),
                                                                    moving_poly_pos_col.value());

                // Find best placement by evaluating all possible orientations and columns.
                Placement best_placement;
                best_placement.score = -std::numeric_limits<double>::infinity();
                best_placement.target_col = moving_poly_pos_col.value();
                best_placement.target_orien = moving_poly_orien.value();
                best_placement.action_sequence = "drop";

                const int64_t n_orientations = static_cast<int64_t>(
                    valid_ominoes.at(moving_poly_family.value()).at(moving_poly_shape.value()).size());

                for(int64_t orien = 0; orien < n_orientations; ++orien){
                    // Determine column range for this orientation.
                    const auto& orien_coords = valid_ominoes.at(moving_poly_family.value())
                                                            .at(moving_poly_shape.value())
                                                            .at(orien);
                    int64_t min_col_offset = 0;
                    int64_t max_col_offset = 0;
                    for(const auto& c : orien_coords){
                        if(c.at(1) < min_col_offset) min_col_offset = c.at(1);
                        if(c.at(1) > max_col_offset) max_col_offset = c.at(1);
                    }
                    
                    int64_t min_col = -min_col_offset;
                    int64_t max_col = img.columns - 1 - max_col_offset;

                    for(int64_t col = min_col; col <= max_col; ++col){
                        // Simulate dropping at this position.
                        auto [final_row, final_coords] = simulate_drop(img, chn,
                                                                       moving_poly_family.value(),
                                                                       moving_poly_shape.value(),
                                                                       orien,
                                                                       col,
                                                                       current_coords);
                        
                        // Validate the placement.
                        if(!abs_coords_valid(img, final_coords)){
                            continue;
                        }

                        // Evaluate the board state with this placement.
                        double score = evaluate_board(img, chn, final_coords);

                        if(score > best_placement.score){
                            best_placement.score = score;
                            best_placement.target_col = col;
                            best_placement.target_orien = orien;
                            best_placement.action_sequence = generate_action_sequence(
                                moving_poly_pos_col.value(),
                                moving_poly_orien.value(),
                                col,
                                orien,
                                moving_poly_family.value(),
                                moving_poly_shape.value());
                        }
                    }
                }

                // Store recommended actions in metadata.
                img.metadata[ai_recommended_str] = best_placement.action_sequence;
                img.metadata[ai_best_col_str] = std::to_string(best_placement.target_col);
                img.metadata[ai_best_orien_str] = std::to_string(best_placement.target_orien);
                img.metadata[ai_best_score_str] = std::to_string(best_placement.score);
            }
        }
    }

    return true;
}
