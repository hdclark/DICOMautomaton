//Tetrominoes.cc - A part of DICOMautomaton 2023. Written by hal clark.

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
#include <random>

#include "YgorImages.h"
#include "YgorString.h"       //Needed for GetFirstRegex(...)
#include "YgorStats.h"       //Needed for Stats:: namespace.

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../YgorImages_Functors/ConvenienceRoutines.h"
#include "../YgorImages_Functors/Grouping/Misc_Functors.h"
#include "../YgorImages_Functors/Compute/Volumetric_Neighbourhood_Sampler.h"

#include "Tetrominoes.h"


OperationDoc OpArgDocTetrominoes(){
    OperationDoc out;
    out.name = "Tetrominoes";

    out.desc = 
        "This operation implements a 2D inventory management survival-horror game using discretized affine"
        " transformations on tetrominoes.";
    
    out.notes.emplace_back(
        "This operation will perform a single iteration of a tetromino game."
        " Invoke multiple times to play a complete game."
    );
    
    out.args.emplace_back();
    out.args.back() = IAWhitelistOpArgDoc();
    out.args.back().name = "ImageSelection";
    out.args.back().default_val = "last";


    out.args.emplace_back();
    out.args.back().name = "Channel";
    out.args.back().desc = "The channel to operated on (zero-based)."
                           " Negative values will cause all channels to be operated on.";
    out.args.back().default_val = "0";
    out.args.back().expected = true;
    out.args.back().examples = { "-1",
                                 "0",
                                 "1" };

    out.args.emplace_back();
    out.args.back().name = "Action";
    out.args.back().desc = "Controls how the moving tetromino (if any are present) is manipulated."
                           "\n\n"
                           " The 'none' action causes the moving tetromino to drop down one row, otherwise"
                           " any number of other actions can be taken to defer this movement."
                           " For consitency with other implementations, the 'none' action should be performed"
                           " repeatedly approximately every second. Other actions should be performed in the"
                           " interim time between the 'none' action."
                           "\n\n"
                           " Note: actions that are not possible are ignored but still defer the 'none' action"
                           " movement.";
    out.args.back().default_val = "none";
    out.args.back().expected = true;
    out.args.back().examples = { "none",
                                 "rotate-clockwise",
                                 "rotate-counterclockwise",
                                 "translate-left",
                                 "translate-right",
                                 "drop" };
    out.args.back().samples = OpArgSamples::Exhaustive;

    out.args.emplace_back();
    out.args.back().name = "Low";
    out.args.back().desc = "The voxel value that represents 'inactive' cells. Since cells are either 'active' or"
                           " 'inactive', the value halfway between the low and high values is used as the threshold.";
    out.args.back().default_val = "0.0";
    out.args.back().expected = true;
    out.args.back().examples = { "0.0",
                                 "-1.23",
                                 "10.0" };

    out.args.emplace_back();
    out.args.back().name = "High";
    out.args.back().desc = "The voxel value that represents 'active' cells. Since cells are either 'active' or"
                           " 'inactive', the value halfway between the low and high values is used as the threshold.";
    out.args.back().default_val = "1.0";
    out.args.back().expected = true;
    out.args.back().examples = { "1.5",
                                 "-0.23",
                                 "255.0" };

    return out;
}

bool Tetrominoes(Drover &DICOM_data,
                        const OperationArgPkg& OptArgs,
                        std::map<std::string, std::string>& /*InvocationMetadata*/,
                        const std::string& /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();

    const auto ActionStr = OptArgs.getValueStr("Action").value();
    const auto Channel = std::stol( OptArgs.getValueStr("Channel").value() );

    const auto Low = std::stod( OptArgs.getValueStr("Low").value() );
    const auto High = std::stod( OptArgs.getValueStr("High").value() );

    //-----------------------------------------------------------------------------------------------------------------
    const auto Threshold = (High * 0.5) + (Low * 0.5);

    const auto regex_none      = Compile_Regex("^no?n?e?$");
    const auto regex_clockwise = Compile_Regex("^ro?t?a?t?e?[-_]?clo?c?k?w?i?s?e?$");
    const auto regex_cntrclock = Compile_Regex("^ro?t?a?t?e?[-_]?[ca][on][ut]?[ni]?t?e?r?[-_]?c?l?o?c?k?w?i?s?e$");
    const auto regex_shift_l   = Compile_Regex("^[ts][rh]?[ai]?[nf]?[st]?l?a?t?e?[-_]?le?f?t?$");
    const auto regex_shift_r   = Compile_Regex("^[ts][rh]?[ai]?[nf]?[st]?l?a?t?e?[-_]?ri?g?h?t?$");
    const auto regex_drop      = Compile_Regex("^dr?o?p?$");

    const bool action_none      = std::regex_match(ActionStr, regex_none);
    const bool action_clockwise = std::regex_match(ActionStr, regex_clockwise);
    const bool action_cntrclock = std::regex_match(ActionStr, regex_cntrclock);
    const bool action_shift_l   = std::regex_match(ActionStr, regex_shift_l);
    const bool action_shift_r   = std::regex_match(ActionStr, regex_shift_r);
    const bool action_drop      = std::regex_match(ActionStr, regex_drop);

    const std::string moving_tet_pos_row_str = "MovingTetrominoPositionRow";
    const std::string moving_tet_pos_col_str = "MovingTetrominoPositionColumn";
    const std::string moving_tet_shape_str   = "MovingTetrominoShape";
    const std::string moving_tet_orien_str   = "MovingTetrominoOrientation";
    const std::string tetromino_score_str    = "TetrominoScore";
    //-----------------------------------------------------------------------------------------------------------------

    auto IAs_all = All_IAs( DICOM_data );
    auto IAs = Whitelist( IAs_all, ImageSelectionStr );

    using img_t = planar_image<float, double>;
    using coord_t = std::array<long int, 2>;

    const auto select_channels = [](const img_t& img, long int x){
        // Given a selection request, return the intersection of requested and available channels.
        // Negative selection implies selecting all available channels.
        // Positive selection implies selecting the single channel (zero-based).
        // Throws when requesting a channel that does not exist.
        std::set<long int> channels;
        if(x < 0){
            for(long int i = 0; i < img.channels; ++i) channels.insert(i);
        }else if(x < img.channels){
            channels.insert(x);
        }else{
            throw std::runtime_error("Channel selection not present in image.");
        }
        return channels;
    };


    // List of all tetrominoes and their permitted orientations.
    //
    // 1.       2.              3.              4.
    //   ▢▢▢▢     ▢▢▢▢▢▢  ▢▢▢     ▢▢▢▢▢  ▢▢▢▢     ▢▢▢▢▢  ▢▢▢▢ 
    //   ▢▣▣▢     ▢▣▣▣▣▢  ▢▣▢     ▢▣▣▢▢  ▢▢▣▢     ▢▢▣▣▢  ▢▣▢▢ 
    //   ▢▣▣▢     ▢▢▢▢▢▢  ▢▣▢     ▢▢▣▣▢  ▢▣▣▢     ▢▣▣▢▢  ▢▣▣▢ 
    //   ▢▢▢▢             ▢▣▢     ▢▢▢▢▢  ▢▣▢▢     ▢▢▢▢▢  ▢▢▣▢ 
    //                    ▢▣▢            ▢▢▢▢            ▢▢▢▢ 
    //                    ▢▢▢
    //
    // 5.                           6.
    //   ▢▢▢▢  ▢▢▢▢▢  ▢▢▢▢  ▢▢▢▢▢     ▢▢▢▢  ▢▢▢▢▢  ▢▢▢▢  ▢▢▢▢▢
    //   ▢▣▢▢  ▢▣▣▣▢  ▢▣▣▢  ▢▢▢▣▢     ▢▢▣▢  ▢▣▢▢▢  ▢▣▣▢  ▢▣▣▣▢
    //   ▢▣▢▢  ▢▣▢▢▢  ▢▢▣▢  ▢▣▣▣▢     ▢▢▣▢  ▢▣▣▣▢  ▢▣▢▢  ▢▢▢▣▢
    //   ▢▣▣▢  ▢▢▢▢▢  ▢▢▣▢  ▢▢▢▢▢     ▢▣▣▢  ▢▢▢▢▢  ▢▣▢▢  ▢▢▢▢▢
    //   ▢▢▢▢         ▢▢▢▢            ▢▢▢▢         ▢▢▢▢       
    //
    // 7.                        
    //   ▢▢▢▢  ▢▢▢▢▢  ▢▢▢▢  ▢▢▢▢▢
    //   ▢▣▢▢  ▢▣▣▣▢  ▢▢▣▢  ▢▢▣▢▢
    //   ▢▣▣▢  ▢▢▣▢▢  ▢▣▣▢  ▢▣▣▣▢
    //   ▢▣▢▢  ▢▢▢▢▢  ▢▢▣▢  ▢▢▢▢▢
    //   ▢▢▢▢         ▢▢▢▢       
    // 
    // Raw tetrominoes with rotational centre cell marked with 'x'.
    //
    //   o--> +col direction
    //   | 
    //   v +row direction
    //
    //     1.     2.         3.          4.
    //       x▣     ▣x▣▣  ▣    ▣x    ▣       x▣  ▣  
    //       ▣▣           x     ▣▣  ▣x      ▣▣   x▣ 
    //                    ▣         ▣             ▣ 
    //                    ▣                         
    //     5.                  6.
    //       ▣   ▣x▣  ▣▣    ▣     ▣  ▣    ▣▣  ▣x▣
    //       x   ▣     x  ▣x▣     x  ▣x▣  x     ▣
    //       ▣▣        ▣         ▣▣       ▣      
    //                                           
    //     7.
    //        ▣         ▣   ▣
    //        x▣  ▣x▣  ▣x  ▣x▣
    //        ▣    ▣    ▣
    // 
    // tet shape (size=7); distinct orientation (size=1-4); coordinates (size=4).
    //std::vector< std::vector< std::vector<coord_t> > > valid_tets {
    std::array< std::vector< std::array<coord_t, 4> >, 7> valid_tets {{
        {  // 1.
           //   x▣
           //   ▣▣
           //
           {{ { 0, 0 }, { 0, 1 },
              { 1, 0 }, { 1, 1 } }}
        },
        {  // 2.      ▣
           //   ▣x▣▣  x
           //         ▣
           //         ▣
           //
           {{ { 0, -1 }, { 0, 0 }, { 0, 1 }, { 0, 2 } }},
           //
           {{ { -1, 0 },
             {  0, 0 },
             {  1, 0 },
             {  2, 0 } }}
        },
        {  // 3.      ▣
           //   ▣x   ▣x
           //    ▣▣  ▣ 
           //
           {{ { 0, -1 }, { 0, 0 },
                         { 1, 0 }, { 1, 1 } }},
           //
                       {{ { -1, 0 },
              {  0, -1 }, {  0, 0 },
              {  1, -1 } }}
        },
        {  // 4.     ▣
           //    x▣  x▣
           //   ▣▣    ▣
           //
                       {{ { 0, 0 }, { 0, 1 },
               { 1, -1 }, { 1, 0 } }},
           //
           {{ { -1,  0 },
              {  0,  0 }, {  0,  1 },
                          {  1,  1 } }}
        },
        {  // 5. ▣        ▣▣    ▣
           //    x   ▣x▣   x  ▣x▣
           //    ▣▣  ▣     ▣
           //
           {{ { -1, 0 },
              {  0, 0 },
              {  1, 0 }, { 1, 1 } }},
           //
           {{ { 0, -1 }, { 0, 0 }, { 0, 1 },
              { 1, -1 } }},
           //
           {{ { -1, -1 }, { -1, 0 },
                          {  0, 0 },
                          {  1, 0 } }},
           //
                                {{ { -1, 1 },
              { 0, -1 }, { 0, 0 }, {  0, 1 } }}
        },
        {  // 6.  ▣  ▣    ▣▣
           //     x  ▣x▣  x   ▣x▣
           //    ▣▣       ▣     ▣
           //
                      {{ { -1, 0 },
                         {  0, 0 },
              { 1, -1 }, {  1, 0 } }},
           //
           {{ { -1, -1 },
              {  0, -1 }, { 0, 0 }, { 0, 1 } }},
           //
           {{ { -1, 0 }, { -1, 1 },
              {  0, 0 },
              {  1, 0 } }},
           //
           {{ { 0, -1 }, { 0, 0 }, { 0, 1 },
                                   { 1, 1 } }}
        },
        {  // 7. ▣         ▣   ▣
           //    x▣  ▣x▣  ▣x  ▣x▣
           //    ▣    ▣    ▣
           //
           {{ { -1, 0 },
              {  0, 0 }, { 0, 1 },
              {  1, 0 } }},
           //
           {{ { 0, -1 }, { 0, 0 }, { 0, 1 },
                         { 1, 0 } }},
           //
                      {{ { -1, 0 },
              { 0, -1 }, {  0, 0 },
                         {  1, 0 } }},
           //
                      {{ { -1, 0 },
              { 0, -1 }, {  0, 0 }, { 0, 1 } }}
        }
    }};

    if( (valid_tets.size() != 7UL)
    ||  (valid_tets.at(0).size() != 1UL)
    ||  (valid_tets.at(1).size() != 2UL)
    ||  (valid_tets.at(2).size() != 2UL)
    ||  (valid_tets.at(3).size() != 2UL)
    ||  (valid_tets.at(4).size() != 4UL)
    ||  (valid_tets.at(5).size() != 4UL)
    ||  (valid_tets.at(6).size() != 4UL) ){
        throw std::logic_error("Unexpected tetromino arrangement");
    }

    // Confirm tet shape and orientation are plausible.
    const auto tet_desc_bounded = [&valid_tets]( long int tet_shape,
                                                 long int tet_orien ) -> bool {
        return (0L <= tet_shape)
            && (0L <= tet_orien)
            && (tet_shape < static_cast<long int>(valid_tets.size()))
            && (tet_orien < static_cast<long int>(valid_tets.at(tet_shape).size()));
    };

    // Cell value primitives.
    const auto cell_is_active = [Threshold]( const img_t& img,
                                             long int row,
                                             long int col,
                                             long int chn ) -> bool {
        return (Threshold < img.value(row, col, chn));
    };

    const auto make_cell_active = [High]( img_t& img,
                                          long int row,
                                          long int col,
                                          long int chn ){
        img.reference(row, col, chn) = High;
        return;
    };

    const auto make_cell_inactive = [Low]( img_t& img,
                                           long int row,
                                           long int col,
                                           long int chn ){
        img.reference(row, col, chn) = Low;
        return;
    };

    // Determines the lowest-valued row. Useful for placing new tets (need to figure out offset).
    const auto min_row_coord = [&valid_tets]( const std::array<coord_t,4> &c ) -> long int {
        return std::min( std::min(c[0][0],c[1][0]), std::min(c[2][0],c[3][0]) ); 
    };

    // Convert from position coordinates, the tet shape, and the tet orientation to absolute image pixel coordinates
    // using an image.
    const auto resolve_abs_coords = [&valid_tets]( const img_t& img,
                                                   long int tet_shape,
                                                   long int tet_orien,
                                                   long int tet_pos_row,
                                                   long int tet_pos_col ) -> std::array<coord_t,4> {
        auto l_coords = valid_tets.at(tet_shape).at(tet_orien);
        for(auto &c : l_coords){
            c.at(0) += tet_pos_row; // Shift from relative to get absolute.
            c.at(1) += tet_pos_col;
        }
        return l_coords;
    };

    // Boolean tests for absolute coordinates.
    const auto abs_coords_valid = []( const img_t& img,
                                      const std::array<coord_t,4> &abs_coords ) -> bool {
        for(const auto &c : abs_coords){
            if( (c.at(0) < 0L) || (img.rows <= c.at(0))
            ||  (c.at(1) < 0L) || (img.columns <= c.at(1)) ){
                return false;
            }
        }
        return true;
    };

    const auto coords_all_active = [&cell_is_active]( const img_t& img,
                                                      long int chn,
                                                      const std::array<coord_t,4> &abs_coords ) -> bool {
        for(const auto &c : abs_coords){
            if(!cell_is_active(img, c.at(0), c.at(1), chn)){
                return false;
            }
        }
        return true;
    };

    const auto coords_all_inactive = [&cell_is_active]( const img_t& img,
                                                        long int chn,
                                                        const std::array<coord_t,4> &abs_coords ) -> bool {
        for(const auto &c : abs_coords){
            if(cell_is_active(img, c.at(0), c.at(1), chn)){
                return false;
            }
        }
        return true;
    };

    // Coordinate writers.
    const auto make_all_coords_active = [&make_cell_active]( img_t& img,
                                                             long int chn,
                                                             const std::array<coord_t,4> &abs_coords ){
        for(const auto &c : abs_coords){
            make_cell_active(img, c.at(0), c.at(1), chn);
        }
        return;
    };

    const auto make_all_coords_inactive = [&make_cell_inactive]( img_t& img,
                                                                 long int chn,
                                                                 const std::array<coord_t,4> &abs_coords ){
        for(const auto &c : abs_coords){
            make_cell_inactive(img, c.at(0), c.at(1), chn);
        }
        return;
    };

    // Implement a change in the moving tet from one placement to another.
    const auto implement_tet_move = [&]( img_t& img,
                                         long int chn,

                                         long int curr_tet_shape,
                                         long int curr_tet_orien,
                                         long int curr_tet_pos_row,
                                         long int curr_tet_pos_col,

                                         long int next_tet_shape,
                                         long int next_tet_orien,
                                         long int next_tet_pos_row,
                                         long int next_tet_pos_col ) -> bool {

        // Confirm current placement.
        const auto l_abs_coords = resolve_abs_coords(img, curr_tet_shape,
                                                          curr_tet_orien,
                                                          curr_tet_pos_row,
                                                          curr_tet_pos_col);
        if(!coords_all_active(img, chn, l_abs_coords)){
            throw std::logic_error("Moving tetromino placement inconsistent, unable to continue");
        }

        // Evaluate whether proposed placment is acceptable.
        const auto l_next_abs_coords = resolve_abs_coords(img, next_tet_shape,
                                                               next_tet_orien,
                                                               next_tet_pos_row,
                                                               next_tet_pos_col);

        if(!abs_coords_valid(img, l_next_abs_coords)){
            return false;
        }

        make_all_coords_inactive(img, chn, l_abs_coords);
        if(!coords_all_inactive(img, chn, l_next_abs_coords)){
            make_all_coords_active(img, chn, l_abs_coords);
            return false;
        }

        img.metadata[moving_tet_pos_row_str] = std::to_string(next_tet_pos_row);
        img.metadata[moving_tet_pos_col_str] = std::to_string(next_tet_pos_col);
        img.metadata[moving_tet_shape_str]   = std::to_string(next_tet_shape);
        img.metadata[moving_tet_orien_str]   = std::to_string(next_tet_orien);
        make_all_coords_active(img, chn, l_next_abs_coords);
        return true;
    };


    for(auto & iap_it : IAs){
        for(auto& img : (*iap_it)->imagecoll.images){
            for(const auto chn : select_channels(img, Channel)){

                // Look for metadata to indicate where/which the current moving tet is.
                //
                // Note: depending on the 'rules', it might be impossible to differentiate the moving tet from
                // stationary cells without this metadata.
                auto moving_tet_pos_row = img.GetMetadataValueAs<long int>(moving_tet_pos_row_str);
                auto moving_tet_pos_col = img.GetMetadataValueAs<long int>(moving_tet_pos_col_str);
                auto moving_tet_shape   = img.GetMetadataValueAs<long int>(moving_tet_shape_str);
                auto moving_tet_orien   = img.GetMetadataValueAs<long int>(moving_tet_orien_str);

                // If there is a tet in the metadata, merely confirm the metadata is accurate.
                if( moving_tet_pos_row
                &&  moving_tet_pos_col
                &&  moving_tet_shape
                &&  moving_tet_orien ){

                    // Check shape and orientation are valid.
                    if( !tet_desc_bounded( moving_tet_shape.value(),
                                           moving_tet_orien.value() ) ){
                        // This might be due to a alteration of the tet data, but more likely due to a metadata error.
                        throw std::runtime_error("Moving tetromino placement not understood, unable to continue");
                    }
                    const auto l_abs_coords = resolve_abs_coords(img, moving_tet_shape.value(),
                                                                      moving_tet_orien.value(),
                                                                      moving_tet_pos_row.value(),
                                                                      moving_tet_pos_col.value());
                    if( !abs_coords_valid(img, l_abs_coords)
                    ||  !coords_all_active(img, chn, l_abs_coords) ){
                        throw std::runtime_error("Moving tetromino placement is not accurate, unable to continue");
                    }

                // Otherwise, try to insert a new tet.
                }else if( !moving_tet_pos_row
                      ||  !moving_tet_pos_col
                      ||  !moving_tet_shape
                      ||  !moving_tet_orien ){
                    std::random_device rdev;
                    std::mt19937 re( rdev() );

                    const auto l_shape = std::uniform_int_distribution<long int>(0L, valid_tets.size()-1L)(re);
                    const auto l_orien = std::uniform_int_distribution<long int>(0L, valid_tets.at(l_shape).size()-1L)(re);
                    const auto row_offset = min_row_coord(valid_tets.at(l_shape).at(l_orien)) * -1L;
                    const auto l_pos_row = row_offset;
                    const auto l_pos_col = (img.columns / 2L) - 1L;

                    // Check if the tet can be placed.
                    // If not possible due to a collision where the piece will be placed, the game concludes.
                    const auto l_abs_coords = resolve_abs_coords(img, l_shape, l_orien, l_pos_row, l_pos_col);
                    if(!abs_coords_valid(img, l_abs_coords)){
                        throw std::runtime_error("Unable to create tetromino, image is too small");
                    }
                    if(!coords_all_inactive(img, chn, l_abs_coords)){
                        throw std::runtime_error("Unable to place new tetromino, unable to continue");
                    }

                    // Perform the insert.
                    img.metadata[moving_tet_pos_row_str] = std::to_string(l_pos_row);
                    img.metadata[moving_tet_pos_col_str] = std::to_string(l_pos_col);
                    img.metadata[moving_tet_shape_str]   = std::to_string(l_shape);
                    img.metadata[moving_tet_orien_str]   = std::to_string(l_orien);
                    make_all_coords_active(img, chn, l_abs_coords);
                    continue;
                }

                //
                // At this point, there is a valid moving tet!
                //


                // Check for complete rows.
                //
                // Note: do not consider rows with the moving tet. We have to wait until it stops moving before it will
                // contribute to a completed row.
                {
                    const auto l_abs_coords = resolve_abs_coords(img, moving_tet_shape.value(),
                                                                      moving_tet_orien.value(),
                                                                      moving_tet_pos_row.value(),
                                                                      moving_tet_pos_col.value());
                    if( !abs_coords_valid(img, l_abs_coords)
                    ||  !coords_all_active(img, chn, l_abs_coords) ){
                        throw std::runtime_error("Moving tetromino placement is not accurate, unable to continue");
                    }
                    make_all_coords_inactive(img, chn, l_abs_coords);

                    // Search for completed rows.
                    std::vector<long int> count(img.rows, 0L);
                    for(auto r = 0L; r < img.rows; ++r){
                        for(auto c = 0L; c < img.columns; ++c){
                            count[r] += cell_is_active(img, r, c, chn) ? 1L : 0L;
                        }
                    }
                    bool found_complete_row = false;
                    for(auto r = (img.rows-1L); r >= 0L; --r){
                        // Check if the row is full.
                        if( count.at(r) == img.columns ){
                            // Shift all rows above (i.e., with smaller row number) down by one row. The top row assumes
                            // the inactive cell number.
                            for(auto l_r = r; l_r >= 0L; --l_r){
                                for(auto l_c = 0L; l_c < img.columns; ++l_c){
                                    const auto old_r = l_r - 1L;
                                    img.reference( l_r, l_c, chn ) = (0L <= old_r) ? img.value(old_r, l_c, chn) : Low;
                                }
                            }
                            found_complete_row = true;
                            break; // Only process one row at a time to simplify this logic.
                        }
                    }

                    make_all_coords_active(img, chn, l_abs_coords);
                    if(found_complete_row){
                        auto score = img.GetMetadataValueAs<long int>(tetromino_score_str);
                        score = score.value_or(0L) + 1L;
                        img.metadata[tetromino_score_str] = std::to_string(score.value());
                        continue;
                    }
                }

                // Otherwise, attempt to implement the proposed action (or a single downward move if no action
                // selected).
                //
                // In this implementation, we permit blocks to make an arbitrary number of actions before dropping down.
                // In most other implementations there is a fixed amount of time to make actions before the tet drops,
                // but practically any number of actions can be performed within that time.
                if(action_none){
                    const bool moved = implement_tet_move(img, chn,
                                                          // From:
                                                          moving_tet_shape.value(),
                                                          moving_tet_orien.value(),
                                                          moving_tet_pos_row.value(),
                                                          moving_tet_pos_col.value(),
                                                          // To:
                                                          moving_tet_shape.value(),
                                                          moving_tet_orien.value(),
                                                          moving_tet_pos_row.value() + 1L,
                                                          moving_tet_pos_col.value() );

                    // If the default action move fails, the block must be at the bottom. So freeze the moving tet.
                    // The next iteration will create a new moving tet, so no need to do so here.
                    if(!moved){
                        img.metadata.erase(moving_tet_pos_row_str);
                        img.metadata.erase(moving_tet_pos_col_str);
                        img.metadata.erase(moving_tet_shape_str);
                        img.metadata.erase(moving_tet_orien_str);
                        continue;
                    }

                }else if(action_clockwise){
                    const auto N_oriens = static_cast<long int>(valid_tets.at(moving_tet_shape.value()).size());
                    const auto new_orien = (moving_tet_orien.value() + 1L) % N_oriens;
                    implement_tet_move(img, chn,
                                       // From:
                                       moving_tet_shape.value(),
                                       moving_tet_orien.value(),
                                       moving_tet_pos_row.value(),
                                       moving_tet_pos_col.value(),
                                       // To:
                                       moving_tet_shape.value(),
                                       new_orien,
                                       moving_tet_pos_row.value(),
                                       moving_tet_pos_col.value() );

                }else if(action_cntrclock){
                    const auto N_oriens = static_cast<long int>(valid_tets.at(moving_tet_shape.value()).size());
                    const auto new_orien = (moving_tet_orien.value() + N_oriens - 1L) % N_oriens;
                    implement_tet_move(img, chn,
                                       // From:
                                       moving_tet_shape.value(),
                                       moving_tet_orien.value(),
                                       moving_tet_pos_row.value(),
                                       moving_tet_pos_col.value(),
                                       // To:
                                       moving_tet_shape.value(),
                                       new_orien,
                                       moving_tet_pos_row.value(),
                                       moving_tet_pos_col.value() );

                }else if(action_shift_l){
                    implement_tet_move(img, chn,
                                       // From:
                                       moving_tet_shape.value(),
                                       moving_tet_orien.value(),
                                       moving_tet_pos_row.value(),
                                       moving_tet_pos_col.value(),
                                       // To:
                                       moving_tet_shape.value(),
                                       moving_tet_orien.value(),
                                       moving_tet_pos_row.value(),
                                       moving_tet_pos_col.value() - 1L );

                }else if(action_shift_r){
                    implement_tet_move(img, chn,
                                       // From:
                                       moving_tet_shape.value(),
                                       moving_tet_orien.value(),
                                       moving_tet_pos_row.value(),
                                       moving_tet_pos_col.value(),
                                       // To:
                                       moving_tet_shape.value(),
                                       moving_tet_orien.value(),
                                       moving_tet_pos_row.value(),
                                       moving_tet_pos_col.value() + 1L );

                }else if(action_drop){
                    // Drop the tet until it collides with something. We accept the shortest unimpeded drop.
                    for(auto i = 0L; i < img.rows; ++i){
                        const bool moved = implement_tet_move(img, chn,
                                                              // From:
                                                              moving_tet_shape.value(),
                                                              moving_tet_orien.value(),
                                                              moving_tet_pos_row.value(),
                                                              moving_tet_pos_col.value(),
                                                              // To:
                                                              moving_tet_shape.value(),
                                                              moving_tet_orien.value(),
                                                              moving_tet_pos_row.value() + 1L,
                                                              moving_tet_pos_col.value() );
                        if(moved){
                            moving_tet_pos_row = moving_tet_pos_row.value() + 1L;
                        }else{
                            break;
                        }
                    }
                }else{
                    throw std::runtime_error("Unknown action, unable to continue");
                }
            }
        }
    }

    return true;
}
