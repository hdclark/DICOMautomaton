//Polyominoes.cc - A part of DICOMautomaton 2023. Written by hal clark.

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

#include "Polyominoes.h"


OperationDoc OpArgDocPolyominoes(){
    OperationDoc out;
    out.name = "Polyominoes";

    out.desc = 
        "This operation implements a 2D inventory management survival-horror game using discretized affine"
        " transformations on polyominoes.";
    
    out.notes.emplace_back(
        "This operation will perform a single iteration of a polyomino game."
        " Invoke multiple times to play a complete game."
    );
    
    out.args.emplace_back();
    out.args.back() = IAWhitelistOpArgDoc();
    out.args.back().name = "ImageSelection";
    out.args.back().default_val = "last";


    out.args.emplace_back();
    out.args.back().name = "Channel";
    out.args.back().desc = "The image channel to operated on (zero-based)."
                           " Negative values will cause all channels to be operated on.";
    out.args.back().default_val = "0";
    out.args.back().expected = true;
    out.args.back().examples = { "-1",
                                 "0",
                                 "1" };

    out.args.emplace_back();
    out.args.back().name = "Family";
    out.args.back().desc = "The family from which to randomly draw new ominoes from."
                           "\n\n"
                           "'0' draws ominoes from all available families."
                           "\n\n"
                           "'1' draws only from the monomino family, which contains only a single, trivial omino."
                           "\n\n"
                           "'2' draws only from the domino family, which contains a single omino."
                           "\n\n"
                           "'3' draws only from the tromino family, which contains two one-sided ominoes."
                           "\n\n"
                           "'4' draws only from the tetromino family, which contains seven one-sided ominoes."
                           "\n\n"
                           "'5' draws only from the pentomino family, which contains eighteen one-sided ominoes.";
    out.args.back().default_val = "0";
    out.args.back().expected = true;
    out.args.back().examples = { "0", "1", "2", "3", "4", "5" };
    out.args.back().samples = OpArgSamples::Exhaustive;

    out.args.emplace_back();
    out.args.back().name = "Action";
    out.args.back().desc = "Controls how the moving polyomino (if any are present) is manipulated."
                           "\n\n"
                           " The 'none' action causes the moving polyomino to drop down one row, otherwise"
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
                                 "translate-down",
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

bool Polyominoes(Drover &DICOM_data,
                        const OperationArgPkg& OptArgs,
                        std::map<std::string, std::string>& /*InvocationMetadata*/,
                        const std::string& /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();

    const auto ActionStr = OptArgs.getValueStr("Action").value();
    const auto Channel = std::stol( OptArgs.getValueStr("Channel").value() );
    const auto Family = std::stol( OptArgs.getValueStr("Family").value() );

    const auto Low = std::stod( OptArgs.getValueStr("Low").value() );
    const auto High = std::stod( OptArgs.getValueStr("High").value() );

    //-----------------------------------------------------------------------------------------------------------------
    const auto Threshold = (High * 0.5) + (Low * 0.5);

    const auto regex_none      = Compile_Regex("^no?n?e?$");
    const auto regex_clockwise = Compile_Regex("^ro?t?a?t?e?[-_]?clo?c?k?w?i?s?e?$");
    const auto regex_cntrclock = Compile_Regex("^ro?t?a?t?e?[-_]?[ca][on][ut]?[ni]?t?e?r?[-_]?c?l?o?c?k?w?i?s?e$");
    const auto regex_shift_l   = Compile_Regex("^[ts][rh]?[ai]?[nf]?[st]?l?a?t?e?[-_]?le?f?t?$");
    const auto regex_shift_r   = Compile_Regex("^[ts][rh]?[ai]?[nf]?[st]?l?a?t?e?[-_]?ri?g?h?t?$");
    const auto regex_shift_d   = Compile_Regex("^[ts][rh]?[ai]?[nf]?[st]?l?a?t?e?[-_]?do?w?n?$");
    const auto regex_drop      = Compile_Regex("^dr?o?p?$");

    const bool action_none      = std::regex_match(ActionStr, regex_none);
    const bool action_clockwise = std::regex_match(ActionStr, regex_clockwise);
    const bool action_cntrclock = std::regex_match(ActionStr, regex_cntrclock);
    const bool action_shift_l   = std::regex_match(ActionStr, regex_shift_l);
    const bool action_shift_r   = std::regex_match(ActionStr, regex_shift_r);
    const bool action_shift_d   = std::regex_match(ActionStr, regex_shift_d);
    const bool action_drop      = std::regex_match(ActionStr, regex_drop);

    const std::string moving_poly_pos_row_str = "MovingPolyominoPositionRow";
    const std::string moving_poly_pos_col_str = "MovingPolyominoPositionColumn";
    const std::string moving_poly_family_str  = "MovingPolyominoFamily";
    const std::string moving_poly_shape_str   = "MovingPolyominoShape";
    const std::string moving_poly_orien_str   = "MovingPolyominoOrientation";
    const std::string omino_score_str         = "PolyominoesScore";
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


    // omino family (size=5); omino shape (size=1-7); distinct orientation (size=1-4); coordinates (size=1-4).
    std::array< std::vector<std::vector<std::vector<coord_t>>>, 5> valid_ominoes {{

    {{
        // Monomonoes:
        //
        // This is just a single block.
        {
           { { 0, 0 } }
        }
    }},

    {{
        // Dominoes:
        //
        // One domino, rotation is symmetrical.
        // Rotational centre cell marked with 'x'.
        //
        //   o--> +col direction
        //   | 
        //   v +row direction
        //
        {  // 1.    ▣
           //   x▣  x
           //      
           //
           { { 0, 0 }, { 0, 1 } },
           //
           { { -1, 0 },
             {  0, 0 } }
        }
    }},

    {{
        // Trominoes:
        //
        // Rotational centre cell marked with 'x'.
        //
        //   o--> +col direction
        //   | 
        //   v +row direction
        //
        {  // 1.     ▣
           //   ▣x▣  x
           //        ▣
           //
           { { 0, -1 }, { 0, 0 }, { 0, 1 } },
           //
           { { -1, 0 },
             {  0, 0 },
             {  1, 0 } }
        },
        {  // 2.         ▣  ▣  
           //   x▣  ▣x  ▣x  x▣
           //   ▣    ▣
           //
           { { 0, 0 },   { 0, 1 },
             { 1, 0 } },
           //
           { { 0, -1 },  { 0, 0 },
                         { 1, 0 } },
           //
                       { { -1, 0 },
             { 0, -1 },  {  0, 0 } },
           //
           { { -1, 0 },
             {  0, 0 }, { 0, 1 } },
        }
    }},

    {{
        // Tetrominoes:
        // 
        // Several tetrominoes.
        // Rotational centre cell marked with 'x'.
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
        {  // 1.
           //   x▣
           //   ▣▣
           //
           { { 0, 0 }, { 0, 1 },
             { 1, 0 }, { 1, 1 } }
        },
        {  // 2.      ▣
           //   ▣x▣▣  x
           //         ▣
           //         ▣
           //
           { { 0, -1 }, { 0, 0 }, { 0, 1 }, { 0, 2 } },
           //
           { { -1, 0 },
             {  0, 0 },
             {  1, 0 },
             {  2, 0 } }
        },
        {  // 3.      ▣
           //   ▣x   ▣x
           //    ▣▣  ▣ 
           //
           { { 0, -1 }, { 0, 0 },
                        { 1, 0 }, { 1, 1 } },
           //
                       { { -1, 0 },
             {  0, -1 }, {  0, 0 },
             {  1, -1 } }
        },
        {  // 4.     ▣
           //    x▣  x▣
           //   ▣▣    ▣
           //
                      { { 0, 0 }, { 0, 1 },
             { 1, -1 }, { 1, 0 } },
           //
           { { -1,  0 },
             {  0,  0 }, {  0,  1 },
                         {  1,  1 } }
        },
        {  // 5. ▣        ▣▣    ▣
           //    x   ▣x▣   x  ▣x▣
           //    ▣▣  ▣     ▣
           //
           { { -1, 0 },
             {  0, 0 },
             {  1, 0 }, { 1, 1 } },
           //
           { { 0, -1 }, { 0, 0 }, { 0, 1 },
             { 1, -1 } },
           //
           { { -1, -1 }, { -1, 0 },
                         {  0, 0 },
                         {  1, 0 } },
           //
                                { { -1, 1 },
             { 0, -1 }, { 0, 0 }, {  0, 1 } }
        },
        {  // 6.  ▣  ▣    ▣▣
           //     x  ▣x▣  x   ▣x▣
           //    ▣▣       ▣     ▣
           //
                      { { -1, 0 },
                        {  0, 0 },
             { 1, -1 }, {  1, 0 } },
           //
           { { -1, -1 },
             {  0, -1 }, { 0, 0 }, { 0, 1 } },
           //
           { { -1, 0 }, { -1, 1 },
             {  0, 0 },
             {  1, 0 } },
           //
           { { 0, -1 }, { 0, 0 }, { 0, 1 },
                                  { 1, 1 } }
        },
        {  // 7. ▣         ▣   ▣
           //    x▣  ▣x▣  ▣x  ▣x▣
           //    ▣    ▣    ▣
           //
           { { -1, 0 },
             {  0, 0 }, { 0, 1 },
             {  1, 0 } },
           //
           { { 0, -1 }, { 0, 0 }, { 0, 1 },
                        { 1, 0 } },
           //
                      { { -1, 0 },
             { 0, -1 }, {  0, 0 },
                        {  1, 0 } },
           //
                      { { -1, 0 },
             { 0, -1 }, {  0, 0 }, { 0, 1 } }
        }
    }},
    {{
        // Pentominoes:
        // 
        // There are 18 distinct pentominoes when they cannot be flipped.
        //
        // These were generated by manually labeling the first orientation, and then rotating to
        // generate the other orientations. Here is the vim search-replace for clockwise rotation:
        //   :'<,'>s/[{][ ]*\([012-]*\), *\([012-]*\) *[}]/{ \2, -\1 }/g
        //   :'<,'>s/[-][-]//g
        //   :'<,'>s/[-]0,/0,/g
        //
        // Rotational centre cell marked with 'x'.
        //
        //   o> +col direction
        //   | 
        //   v +row direction
        //
        {  // 1.  o
           //    oxo
           //     o
           //
                       { { -1, 0 },
             {  0, -1 }, {  0, 0 }, {  0, 1 },
                         {  1, 0 } }
        },
        {  // 2.
           //   ooxoo
           //
           { { 0, -2 }, { 0, -1 }, { 0, 0 }, { 0, 1 }, { 0, 2 } },
           //
           { { -2, 0 }, { -1, 0 }, { 0, 0 }, { 1, 0 }, { 2, 0 } }
        },
        {  // 3.
           //     oo
           //    ox
           //     o
           //
           { { -1,  0 }, { -1, 1 },
             {  0, -1 }, {  0, 0 },
                         {  1, 0 } },
           //
           { { 0, 1 }, { 1, 1 }, { -1, -0 }, { 0, -0 }, { 0, -1 } },
           //
           { { 1, -0 }, { 1, -1 }, { 0, 1 }, { 0, -0 }, { -1, -0 } },
           //
           { { 0, -1 }, { -1, -1 }, { 1, 0 }, { 0, 0 }, { 0, 1 } }
        },
        {  // 4.
           //    oo
           //     xo
           //     o
           //
           { { -1, -1 }, { -1, 0 },
                         {  0, 0 }, {  0, 1 },
                         {  1, 0 } },
           //
           { { -1, 1 }, { 0, 1 }, { 0, -0 }, { 1, -0 }, { 0, -1 } },
           //
           { { 1, 1 }, { 1, -0 }, { 0, -0 }, { 0, -1 }, { -1, -0 } },
           //
           { { 1, -1 }, { 0, -1 }, { 0, 0 }, { -1, 0 }, { 0, 1 } }
        },
        {  // 5.  o
           //     o
           //     x
           //    oo
           //
                       { { -2, 0 },
                         { -1, 0 },
                         {  0, 0 },
             {  1, -1 }, {  1, 0 } },
           //
           { { 0, 2 }, { 0, 1 }, { 0, -0 }, { -1, -1 }, { 0, -1 } },
           //
           { { 2, -0 }, { 1, -0 }, { 0, -0 }, { -1, 1 }, { -1, -0 } },
           //
           { { 0, -2 }, { 0, -1 }, { 0, 0 }, { 1, 1 }, { 0, 1 } }
        },
        {  // 6.  o
           //     o
           //     x
           //     oo
           //
                       { { -2, 0 },
                         { -1, 0 },
                         {  0, 0 },
                         {  1, 0 }, { 1, 1 } },
           //
           { { 0, 2 }, { 0, 1 }, { 0, -0 }, { 0, -1 }, { 1, -1 } },
           //
           { { 2, -0 }, { 1, -0 }, { 0, -0 }, { -1, -0 }, { -1, -1 } },
           //
           { { 0, -2 }, { 0, -1 }, { 0, 0 }, { 0, 1 }, { -1, 1 } }
        },
        {  // 7.
           //    oo
           //    ox
           //     o
           //
           { { -1, -1 }, { -1, 0 },
             {  0, -1 }, {  0, 0 },
                         {  1, 0 } },
           //
           { { -1, 1 }, { 0, 1 }, { -1, -0 }, { 0, -0 }, { 0, -1 } },
           //
           { { 1, 1 }, { 1, -0 }, { 0, 1 }, { 0, -0 }, { -1, -0 } },
           //
           { { 1, -1 }, { 0, -1 }, { 1, 0 }, { 0, 0 }, { 0, 1 } }
        },
        {  // 8.
           //    oo
           //    xo
           //    o
           //
           { { -1, 0 }, { -1, 1 },
             {  0, 0 }, {  0, 1 },
             {  1, 0 } },
           //
           { { 0, 1 }, { 1, 1 }, { 0, -0 }, { 1, -0 }, { 0, -1 } },
           //
           { { 1, -0 }, { 1, -1 }, { 0, -0 }, { 0, -1 }, { -1, -0 } },
           //
           { { 0, -1 }, { -1, -1 }, { 0, 0 }, { -1, 0 }, { 0, 1 } }
        },
        {  // 9.  o
           //     o
           //    ox
           //    o
           //
                       { { -2, 0 },
                         { -1, 0 },
             {  0, -1 }, {  0, 0 },
             {  1, -1 } },
           //
           { { 0, 2 }, { 0, 1 }, { -1, -0 }, { 0, -0 }, { -1, -1 } },
           //
           { { 2, -0 }, { 1, -0 }, { 0, 1 }, { 0, -0 }, { -1, 1 } },
           //
           { { 0, -2 }, { 0, -1 }, { 1, 0 }, { 0, 0 }, { 1, 1 } }
        },
        {  // 10. o
           //     o
           //     xo
           //      o
           //
           { { -2, 0 },
             { -1, 0 },
             {  0, 0 }, {  0, 1 },
                        {  1, 1 } },
           //
           { { 0, 2 }, { 0, 1 }, { 0, -0 }, { 1, -0 }, { 1, -1 } },
           //
           { { 2, -0 }, { 1, -0 }, { 0, -0 }, { 0, -1 }, { -1, -1 } },
           //
           { { 0, -2 }, { 0, -1 }, { 0, 0 }, { -1, 0 }, { -1, 1 } }
        },
        {  // 11.
           //   ooo
           //    x
           //    o
           //
           { { -1, -1 }, { -1, 0 }, { -1, 1 },
                         {  0, 0 },
                         {  1, 0 } },
           //
           { { -1, 1 }, { 0, 1 }, { 1, 1 }, { 0, -0 }, { 0, -1 } },
           //
           { { 1, 1 }, { 1, -0 }, { 1, -1 }, { 0, -0 }, { -1, -0 } },
           //
           { { 1, -1 }, { 0, -1 }, { -1, -1 }, { 0, 0 }, { 0, 1 } }
        },
        {  // 12.
           //    o o
           //    oxo
           //
           { { -1, -1 },            { -1, 1 },
             {  0, -1 }, {  0, 0 }, {  0, 1 } },
           //
           { { -1, 1 }, { 1, 1 }, { -1, -0 }, { 0, -0 }, { 1, -0 } },
           //
           { { 1, 1 }, { 1, -1 }, { 0, 1 }, { 0, -0 }, { 0, -1 } },
           //
           { { 1, -1 }, { -1, -1 }, { 1, 0 }, { 0, 0 }, { -1, 0 } }
        },
        {  // 13.  o
           //      o
           //    oox
           //
                                   { { -2, 0 },
                                     { -1, 0 },
             {  0, -2 }, {  0, -1 }, {  0, 0 } },
           //
           { { 0, 2 }, { 0, 1 }, { -2, -0 }, { -1, -0 }, { 0, -0 } },
           //
           { { 2, -0 }, { 1, -0 }, { 0, 2 }, { 0, 1 }, { 0, -0 } },
           //
           { { 0, -2 }, { 0, -1 }, { 2, 0 }, { 1, 0 }, { 0, 0 } }
        },
        {  // 14. o
           //    xo
           //   oo
           //
                                  { { -1, 1 },
                         {  0, 0 }, {  0, 1 },
             {  1, -1 }, {  1, 0 } },
           //
           { { 1, 1 }, { 0, -0 }, { 1, -0 }, { -1, -1 }, { 0, -1 } },
           //
           { { 1, -1 }, { 0, -0 }, { 0, -1 }, { -1, 1 }, { -1, -0 } },
           //
           { { -1, -1 }, { 0, 0 }, { -1, 0 }, { 1, 1 }, { 0, 1 } }
        },
        {  // 15.  o
           //     ox
           //      o
           //      o
           //
                       { { -1, 0 },
             {  0, -1 }, {  0, 0 },
                         {  1, 0 },
                         {  2, 0 } },
           //
           { { 0, 1 }, { -1, -0 }, { 0, -0 }, { 0, -1 }, { 0, -2 } },
           //
           { { 1, -0 }, { 0, 1 }, { 0, -0 }, { -1, -0 }, { -2, -0 } },
           //
           { { 0, -1 }, { 1, 0 }, { 0, 0 }, { 0, 1 }, { 0, 2 } }
        },
        {  // 16. o
           //     xo
           //     o
           //     o
           //
           { { -1, 0 },
             {  0, 0 }, {  0, 1 },
             {  1, 0 },
             {  2, 0 } },
           //
           { { 0, 1 }, { 0, -0 }, { 1, -0 }, { 0, -1 }, { 0, -2 } },
           //
           { { 1, -0 }, { 0, -0 }, { 0, -1 }, { -1, -0 }, { -2, -0 } },
           //
           { { 0, -1 }, { 0, 0 }, { -1, 0 }, { 0, 1 }, { 0, 2 } }
        },
        {  // 17.  oo
           //      x
           //     oo
           //
                       { { -1, 0 }, { -1, 1 },
                         {  0, 0 },
             {  1, -1 }, {  1, 0 } },
           //
           { { 0, 1 }, { 1, 1 }, { 0, -0 }, { -1, -1 }, { 0, -1 } },
           //
           { { 1, -0 }, { 1, -1 }, { 0, -0 }, { -1, 1 }, { -1, -0 } },
           //
           { { 0, -1 }, { -1, -1 }, { 0, 0 }, { 1, 1 }, { 0, 1 } }
        },
        {  // 18.  oo
           //       x
           //       oo
           //
           { { -1, -1 }, { -1, 0 },
                         {  0, 0 },
                         {  1, 0 }, {  1, 1 } },
           //
           { { -1, 1 }, { 0, 1 }, { 0, -0 }, { 0, -1 }, { 1, -1 } },
           //
           { { 1, 1 }, { 1, -0 }, { 0, -0 }, { -1, -0 }, { -1, -1 } },
           //
           { { 1, -1 }, { 0, -1 }, { 0, 0 }, { 0, 1 }, { -1, 1 } }
        }
    }}
    }};

    if( (valid_ominoes.size() != 4UL)

    ||  (valid_ominoes.at(0).size() != 1UL) // Monominoes.
    ||  (valid_ominoes.at(0).at(0).size() != 1UL)

    ||  (valid_ominoes.at(1).size() != 1UL) // Dominoes.
    ||  (valid_ominoes.at(1).at(0).size() != 2UL)

    ||  (valid_ominoes.at(2).size() != 2UL) // Trominoes.
    ||  (valid_ominoes.at(2).at(0).size() != 2UL)
    ||  (valid_ominoes.at(2).at(1).size() != 4UL)

    ||  (valid_ominoes.at(3).size() != 7UL) // Tetrominoes.
    ||  (valid_ominoes.at(3).at(0).size() != 1UL)
    ||  (valid_ominoes.at(3).at(1).size() != 2UL)
    ||  (valid_ominoes.at(3).at(2).size() != 2UL)
    ||  (valid_ominoes.at(3).at(3).size() != 2UL)
    ||  (valid_ominoes.at(3).at(4).size() != 4UL)
    ||  (valid_ominoes.at(3).at(5).size() != 4UL)
    ||  (valid_ominoes.at(3).at(6).size() != 4UL)

    ||  (valid_ominoes.at(4).size() != 18UL) // Pentominoes.
    ||  (valid_ominoes.at(4).at(0).size() != 1UL)
    ||  (valid_ominoes.at(4).at(1).size() != 2UL)
    ||  (valid_ominoes.at(4).at(2).size() != 4UL)
    ||  (valid_ominoes.at(4).at(3).size() != 4UL)
    ||  (valid_ominoes.at(4).at(4).size() != 4UL)
    ||  (valid_ominoes.at(4).at(5).size() != 4UL)
    ||  (valid_ominoes.at(4).at(6).size() != 4UL)
    ||  (valid_ominoes.at(4).at(7).size() != 4UL)
    ||  (valid_ominoes.at(4).at(8).size() != 4UL)
    ||  (valid_ominoes.at(4).at(9).size() != 4UL)
    ||  (valid_ominoes.at(4).at(10).size() != 4UL)
    ||  (valid_ominoes.at(4).at(11).size() != 4UL)
    ||  (valid_ominoes.at(4).at(12).size() != 4UL)
    ||  (valid_ominoes.at(4).at(13).size() != 4UL)
    ||  (valid_ominoes.at(4).at(14).size() != 4UL)
    ||  (valid_ominoes.at(4).at(15).size() != 4UL)
    ||  (valid_ominoes.at(4).at(16).size() != 4UL)
    ||  (valid_ominoes.at(4).at(17).size() != 4UL)
    ||  (valid_ominoes.at(4).at(18).size() != 4UL) ){
        throw std::logic_error("Unexpected omino storage");
    }

    // Confirm poly shape and orientation are plausible.
    const auto poly_desc_bounded = [&valid_ominoes]( long int poly_family,
                                                     long int poly_shape,
                                                     long int poly_orien ) -> bool {
        return (0L <= poly_family)
            && (0L <= poly_shape)
            && (0L <= poly_orien)
            && (poly_family < static_cast<long int>(valid_ominoes.size()))
            && (poly_shape < static_cast<long int>(valid_ominoes.at(poly_family).size()))
            && (poly_orien < static_cast<long int>(valid_ominoes.at(poly_family).at(poly_shape).size()));
    };

    // Cell value primitives.
    const auto cell_is_active = [Threshold]( const img_t& img,
                                             long int row,
                                             long int col,
                                             long int chn ) -> bool {
        return (Threshold < img.value(row, col, chn));
    };

    const auto make_cell_active = [Threshold,
                                   High,
                                   &valid_ominoes]( img_t& img,
                                                    long int row,
                                                    long int col,
                                                    long int chn,
                                                    long int poly_family,
                                                    long int poly_shape ){

        const auto N_polys = static_cast<float>(valid_ominoes.at(poly_family).size());
        const auto dval = (High - Threshold) / (N_polys + 1.0);
        const auto val = Threshold + dval + (dval * static_cast<float>(poly_shape));
        img.reference(row, col, chn) = val;
        return;
    };

    const auto make_cell_inactive = [Low]( img_t& img,
                                           long int row,
                                           long int col,
                                           long int chn ){
        img.reference(row, col, chn) = Low;
        return;
    };

    // Determines the lowest-valued row. Useful for placing new polys (need to figure out offset).
    const auto min_row_coord = []( const std::vector<coord_t> &c ) -> long int {
        const auto it = std::min_element( std::begin(c), std::end(c),
                                          [](const coord_t &l, const coord_t &r){
                                              return (l.at(0) < r.at(0));
                                          } );
        return it->at(0);
    };

    // Convert from position coordinates, the poly family, shape, and orientation to absolute image pixel coordinates
    // using an image.
    const auto resolve_abs_coords = [&valid_ominoes]( const img_t& img,
                                                      long int poly_family,
                                                      long int poly_shape,
                                                      long int poly_orien,
                                                      long int poly_pos_row,
                                                      long int poly_pos_col ) -> std::vector<coord_t> {
        auto l_coords = valid_ominoes.at(poly_family).at(poly_shape).at(poly_orien);
        for(auto &c : l_coords){
            c.at(0) += poly_pos_row; // Shift from relative to get absolute.
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

    const auto coords_all_active = [&cell_is_active]( const img_t& img,
                                                      long int chn,
                                                      const std::vector<coord_t> &abs_coords ) -> bool {
        for(const auto &c : abs_coords){
            if(!cell_is_active(img, c.at(0), c.at(1), chn)){
                return false;
            }
        }
        return true;
    };

    const auto coords_all_inactive = [&cell_is_active]( const img_t& img,
                                                        long int chn,
                                                        const std::vector<coord_t> &abs_coords ) -> bool {
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
                                                             long int poly_family,
                                                             long int poly_shape,
                                                             const std::vector<coord_t> &abs_coords ){
        for(const auto &c : abs_coords){
            make_cell_active(img, c.at(0), c.at(1), chn, poly_family, poly_shape);
        }
        return;
    };

    const auto make_all_coords_inactive = [&make_cell_inactive]( img_t& img,
                                                                 long int chn,
                                                                 const std::vector<coord_t> &abs_coords ){
        for(const auto &c : abs_coords){
            make_cell_inactive(img, c.at(0), c.at(1), chn);
        }
        return;
    };

    // Implement a change in the moving poly from one placement to another.
    const auto implement_poly_move = [&]( img_t& img,
                                          long int chn,

                                          long int curr_poly_family,
                                          long int curr_poly_shape,
                                          long int curr_poly_orien,
                                          long int curr_poly_pos_row,
                                          long int curr_poly_pos_col,

                                          long int next_poly_family,
                                          long int next_poly_shape,
                                          long int next_poly_orien,
                                          long int next_poly_pos_row,
                                          long int next_poly_pos_col ) -> bool {

        // Confirm current placement.
        const auto l_abs_coords = resolve_abs_coords(img, curr_poly_family,
                                                          curr_poly_shape,
                                                          curr_poly_orien,
                                                          curr_poly_pos_row,
                                                          curr_poly_pos_col);
        if(!coords_all_active(img, chn, l_abs_coords)){
            throw std::logic_error("Moving omino placement inconsistent, unable to continue");
        }

        // Evaluate whether proposed placment is acceptable.
        const auto l_next_abs_coords = resolve_abs_coords(img, next_poly_family,
                                                               next_poly_shape,
                                                               next_poly_orien,
                                                               next_poly_pos_row,
                                                               next_poly_pos_col);
        if(!abs_coords_valid(img, l_next_abs_coords)){
            return false;
        }

        make_all_coords_inactive(img, chn, l_abs_coords);
        if(!coords_all_inactive(img, chn, l_next_abs_coords)){
            make_all_coords_active(img, chn, curr_poly_family, curr_poly_shape, l_abs_coords);
            return false;
        }

        img.metadata[moving_poly_pos_row_str] = std::to_string(next_poly_pos_row);
        img.metadata[moving_poly_pos_col_str] = std::to_string(next_poly_pos_col);
        img.metadata[moving_poly_family_str]  = std::to_string(next_poly_family);
        img.metadata[moving_poly_shape_str]   = std::to_string(next_poly_shape);
        img.metadata[moving_poly_orien_str]   = std::to_string(next_poly_orien);
        make_all_coords_active(img, chn, next_poly_family, next_poly_shape, l_next_abs_coords);
        return true;
    };


    for(auto & iap_it : IAs){
        for(auto& img : (*iap_it)->imagecoll.images){
            for(const auto chn : select_channels(img, Channel)){

                // Look for metadata to indicate where/which the current moving poly is.
                //
                // Note: depending on the 'rules', it might be impossible to differentiate the moving poly from
                // stationary cells without this metadata.
                auto moving_poly_pos_row = img.GetMetadataValueAs<long int>(moving_poly_pos_row_str);
                auto moving_poly_pos_col = img.GetMetadataValueAs<long int>(moving_poly_pos_col_str);
                auto moving_poly_family  = img.GetMetadataValueAs<long int>(moving_poly_family_str);
                auto moving_poly_shape   = img.GetMetadataValueAs<long int>(moving_poly_shape_str);
                auto moving_poly_orien   = img.GetMetadataValueAs<long int>(moving_poly_orien_str);

                // If there is a poly in the metadata, merely confirm the metadata is accurate.
                if( moving_poly_pos_row
                &&  moving_poly_pos_col
                &&  moving_poly_family
                &&  moving_poly_shape
                &&  moving_poly_orien ){

                    // Check family, shape, and orientation are valid.
                    if( !poly_desc_bounded( moving_poly_family.value(),
                                            moving_poly_shape.value(),
                                            moving_poly_orien.value() ) ){
                        // This might be due to a alteration of the poly data, but more likely due to a metadata error.
                        throw std::runtime_error("Moving omino placement not understood, unable to continue");
                    }
                    const auto l_abs_coords = resolve_abs_coords(img, moving_poly_family.value(),
                                                                      moving_poly_shape.value(),
                                                                      moving_poly_orien.value(),
                                                                      moving_poly_pos_row.value(),
                                                                      moving_poly_pos_col.value());
                    if( !abs_coords_valid(img, l_abs_coords)
                    ||  !coords_all_active(img, chn, l_abs_coords) ){
                        throw std::runtime_error("Moving omino placement is not accurate, unable to continue");
                    }

                // Otherwise, try to insert a new poly.
                }else if( !moving_poly_pos_row
                      ||  !moving_poly_pos_col
                      ||  !moving_poly_family
                      ||  !moving_poly_shape
                      ||  !moving_poly_orien ){
                    std::random_device rdev;
                    std::mt19937 re( rdev() );

                    // Create a new omino randomly, drawing from the specified family and shape uniformly.
                    // Do not consider orientation, since some shapes will be over-represented;
                    // ominoes can always be rotated by the user, but cannot be transformed/transmuted.
                    std::vector< std::pair<long int, long int> > poss;
                    const auto N_families = static_cast<long int>(valid_ominoes.size());
                    for(long int family = 0L; family < N_families; ++family){
                        if( (Family <= 0L) 
                        ||  (4L < Family)
                        ||  (family == Family) ){
                            const auto N_shapes = static_cast<long int>(valid_ominoes.at(family).size());
                            for(long int shape = 0L; shape < N_shapes; ++shape){
                                poss.emplace_back( family, shape );
                            }
                        }
                    }
                    if(poss.empty()){
                        throw std::runtime_error("No valid ominoes to draw from, unable to continue");
                    }
                    const auto index = std::uniform_int_distribution<long int>(0UL, poss.size()-1UL)(re);
                    const auto l_family = poss.at(index).first;
                    const auto l_shape = poss.at(index).second;
                    const auto l_orien = std::uniform_int_distribution<long int>(0L, valid_ominoes.at(l_family).at(l_shape).size()-1L)(re);
                    const auto row_offset = min_row_coord(valid_ominoes.at(l_family).at(l_shape).at(l_orien)) * -1L;
                    const auto l_pos_row = row_offset;
                    const auto l_pos_col = (img.columns / 2L) - 1L;

                    // Check if the poly can be placed.
                    // If not possible due to a collision where the piece will be placed, the game concludes.
                    const auto l_abs_coords = resolve_abs_coords(img, l_family, l_shape, l_orien, l_pos_row, l_pos_col);
                    if(!abs_coords_valid(img, l_abs_coords)){
                        throw std::runtime_error("Unable to create omino, image is too small");
                    }
                    if(!coords_all_inactive(img, chn, l_abs_coords)){
                        throw std::runtime_error("Unable to place new omino, unable to continue");
                    }

                    // Perform the insert.
                    img.metadata[moving_poly_pos_row_str] = std::to_string(l_pos_row);
                    img.metadata[moving_poly_pos_col_str] = std::to_string(l_pos_col);
                    img.metadata[moving_poly_family_str]  = std::to_string(l_family);
                    img.metadata[moving_poly_shape_str]   = std::to_string(l_shape);
                    img.metadata[moving_poly_orien_str]   = std::to_string(l_orien);
                    make_all_coords_active(img, chn, l_family, l_shape, l_abs_coords);
                    continue;
                }

                //
                // At this point, there is a valid moving poly!
                //


                // Check for complete rows.
                //
                // Note: do not consider rows with the moving poly. We have to wait until it stops moving before it will
                // contribute to a completed row.
                {
                    const auto l_abs_coords = resolve_abs_coords(img, moving_poly_family.value(),
                                                                      moving_poly_shape.value(),
                                                                      moving_poly_orien.value(),
                                                                      moving_poly_pos_row.value(),
                                                                      moving_poly_pos_col.value());
                    if( !abs_coords_valid(img, l_abs_coords)
                    ||  !coords_all_active(img, chn, l_abs_coords) ){
                        throw std::runtime_error("Moving omino placement is not accurate, unable to continue");
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

                    make_all_coords_active(img, chn, moving_poly_family.value(), moving_poly_shape.value(), l_abs_coords);
                    if(found_complete_row){
                        auto score = img.GetMetadataValueAs<long int>(omino_score_str);
                        score = score.value_or(0L) + 1L;
                        img.metadata[omino_score_str] = std::to_string(score.value());
                        continue;
                    }
                }

                // Otherwise, attempt to implement the proposed action (or a single downward move if no action
                // selected).
                //
                // In this implementation, we permit blocks to make an arbitrary number of actions before dropping down.
                // In most other implementations there is a fixed amount of time to make actions before the poly drops,
                // but practically any number of actions can be performed within that time.
                if(action_none){
                    const bool moved = implement_poly_move(img, chn,
                                                          // From:
                                                          moving_poly_family.value(),
                                                          moving_poly_shape.value(),
                                                          moving_poly_orien.value(),
                                                          moving_poly_pos_row.value(),
                                                          moving_poly_pos_col.value(),
                                                          // To:
                                                          moving_poly_family.value(),
                                                          moving_poly_shape.value(),
                                                          moving_poly_orien.value(),
                                                          moving_poly_pos_row.value() + 1L,
                                                          moving_poly_pos_col.value() );

                    // If the default action move fails, the block must be at the bottom. So freeze the moving poly.
                    // The next iteration will create a new moving poly, so no need to do so here.
                    if(!moved){
                        img.metadata.erase(moving_poly_pos_row_str);
                        img.metadata.erase(moving_poly_pos_col_str);
                        img.metadata.erase(moving_poly_family_str);
                        img.metadata.erase(moving_poly_shape_str);
                        img.metadata.erase(moving_poly_orien_str);
                        continue;
                    }

                }else if(action_clockwise){
                    const auto N_oriens = static_cast<long int>(valid_ominoes.at(moving_poly_family.value())
                                                                             .at(moving_poly_shape.value())
                                                                             .size());
                    const auto new_orien = (moving_poly_orien.value() + 1L) % N_oriens;
                    implement_poly_move(img, chn,
                                        // From:
                                        moving_poly_family.value(),
                                        moving_poly_shape.value(),
                                        moving_poly_orien.value(),
                                        moving_poly_pos_row.value(),
                                        moving_poly_pos_col.value(),
                                        // To:
                                        moving_poly_family.value(),
                                        moving_poly_shape.value(),
                                        new_orien,
                                        moving_poly_pos_row.value(),
                                        moving_poly_pos_col.value() );

                }else if(action_cntrclock){
                    const auto N_oriens = static_cast<long int>(valid_ominoes.at(moving_poly_family.value())
                                                                             .at(moving_poly_shape.value())
                                                                             .size());
                    const auto new_orien = (moving_poly_orien.value() + N_oriens - 1L) % N_oriens;
                    implement_poly_move(img, chn,
                                        // From:
                                        moving_poly_family.value(),
                                        moving_poly_shape.value(),
                                        moving_poly_orien.value(),
                                        moving_poly_pos_row.value(),
                                        moving_poly_pos_col.value(),
                                        // To:
                                        moving_poly_family.value(),
                                        moving_poly_shape.value(),
                                        new_orien,
                                        moving_poly_pos_row.value(),
                                        moving_poly_pos_col.value() );

                }else if(action_shift_l){
                    implement_poly_move(img, chn,
                                        // From:
                                        moving_poly_family.value(),
                                        moving_poly_shape.value(),
                                        moving_poly_orien.value(),
                                        moving_poly_pos_row.value(),
                                        moving_poly_pos_col.value(),
                                        // To:
                                        moving_poly_family.value(),
                                        moving_poly_shape.value(),
                                        moving_poly_orien.value(),
                                        moving_poly_pos_row.value(),
                                        moving_poly_pos_col.value() - 1L );

                }else if(action_shift_r){
                    implement_poly_move(img, chn,
                                        // From:
                                        moving_poly_family.value(),
                                        moving_poly_shape.value(),
                                        moving_poly_orien.value(),
                                        moving_poly_pos_row.value(),
                                        moving_poly_pos_col.value(),
                                        // To:
                                        moving_poly_family.value(),
                                        moving_poly_shape.value(),
                                        moving_poly_orien.value(),
                                        moving_poly_pos_row.value(),
                                        moving_poly_pos_col.value() + 1L );

                }else if(action_shift_d){
                    implement_poly_move(img, chn,
                                        // From:
                                        moving_poly_family.value(),
                                        moving_poly_shape.value(),
                                        moving_poly_orien.value(),
                                        moving_poly_pos_row.value(),
                                        moving_poly_pos_col.value(),
                                        // To:
                                        moving_poly_family.value(),
                                        moving_poly_shape.value(),
                                        moving_poly_orien.value(),
                                        moving_poly_pos_row.value() + 1L,
                                        moving_poly_pos_col.value() );

                }else if(action_drop){
                    // Drop the poly until it collides with something. We accept the shortest unimpeded drop.
                    for(auto i = 0L; i < img.rows; ++i){
                        const bool moved = implement_poly_move(img, chn,
                                                               // From:
                                                               moving_poly_family.value(),
                                                               moving_poly_shape.value(),
                                                               moving_poly_orien.value(),
                                                               moving_poly_pos_row.value(),
                                                               moving_poly_pos_col.value(),
                                                               // To:
                                                               moving_poly_family.value(),
                                                               moving_poly_shape.value(),
                                                               moving_poly_orien.value(),
                                                               moving_poly_pos_row.value() + 1L,
                                                               moving_poly_pos_col.value() );
                        if(moved){
                            moving_poly_pos_row = moving_poly_pos_row.value() + 1L;
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
