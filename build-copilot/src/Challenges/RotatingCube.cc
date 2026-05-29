// RotatingCubeGame.cc - A part of DICOMautomaton 2026. Written by hal clark.

#include <vector>
#include <random>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <tuple>
#include <map>
#include <stdexcept>
#include <limits>

#include "YgorMath.h"
#include "YgorMisc.h"
#include "YgorLog.h"

#include <SDL.h>
#include "../imgui20210904/imgui.h"

#include "RotatingCube.h"


// ------------------------------------------------------------------------------------
// Representation.
// ------------------------------------------------------------------------------------

rc_game_t::rc_game_t(){
    //int64_t N = 3;
    this->reset(this->N);
}

void rc_game_t::reset(int64_t l_N){
    this->N = l_N;

    const auto N_cells = this->N * this->N * 6;
    this->cells.resize( N_cells ); 

    for(int64_t f = 0; f < 6; ++f){
        for(int64_t x = 0; x < N; ++x){
            for(int64_t y = 0; y < N; ++y){
                const auto c = std::make_tuple(f, x, y);
                const auto index = this->index(c);
                this->assert_index_valid(index);
                this->get_cell(index).colour = f;
            }
        }
    }
    return;
}

int64_t rc_game_t::index(const rc_game_t::coords_t &c) const {
    int64_t out = -1;
    const auto [f, x, y] = c;
    if( isininc(0, f, 5)
    &&  isininc(0, x, this->N - 1)
    &&  isininc(0, y, this->N - 1) ){
        out = f * (this->N * this->N) + (y * this->N) + x;
    }
    return out;
}

void rc_game_t::assert_index_valid(int64_t index) const {
    if( ! this->confirm_index_valid(index) ){
        throw std::logic_error("Index is not valid");
    }
    return;
}

bool rc_game_t::confirm_index_valid(int64_t index) const {
    const auto N_cells = this->N * this->N * 6;
    return isininc(0, index, N_cells - 1);
}

rc_game_t::coords_t rc_game_t::coords(int64_t index) const {
    this->assert_index_valid(index); 

    const ldiv_t q = ::ldiv(index,this->N*this->N);
    const ldiv_t z = ::ldiv(q.rem,this->N);
    const int64_t f = q.quot;
    const int64_t y = z.quot;
    const int64_t x = z.rem;

    const auto t = std::make_tuple(f, x, y);
    if( (this->index(t) != index)
    ||  !this->confirm_index_valid(index) ){
        throw std::logic_error("Indexing scheme mismatch.");
    }
    return t;
}

const rc_cell_t& rc_game_t::get_const_cell(int64_t index) const {
    this->assert_index_valid(index); 
    return this->cells.at(index);
}

rc_cell_t& rc_game_t::get_cell(int64_t index){
    this->assert_index_valid(index); 
    return this->cells.at(index);
}

int64_t rc_game_t::get_N() const {
    return this->N;
}

std::array<float,4> rc_game_t::colour_to_rgba(int64_t colour) const {
    std::array<float,4> out {0.0f, 0.0f, 0.0f, 1.0f};

    const auto n = (colour % 6);
    if(false){
    }else if(n == 0){
        // Cadmium Orange and Cadmium Yellow (Close to Hansa Yellow)
        out = {1.0, 0.7, 0.0, 1.0};
    }else if(n == 1){
        // Cadmium Red
        out = {1.0, 0.153, 0.008, 1.0};
    }else if(n == 2){
        // Quinacridone Magenta
        out = {0.502, 0.008, 0.18, 1.0};
    }else if(n == 3){
        // Cobalt Blue
        out = {0.0, 0.129, 0.522, 1.0};
    }else if(n == 4){
        // Permanent Green
        out = {0.027, 0.427, 0.086, 1.0};
    }else if(n == 5){
        // Burnt Sienna
        out = {0.482, 0.282, 0.0, 1.0};
    }

    return out;
}

std::tuple<int64_t, rc_direction>
rc_game_t::get_neighbour_face(int64_t face, rc_direction dir) const {
    // Given a face and a direction, return the adjacent neighbouring face and how the direction should be interpretted
    // relative to the new face (so calling this routine 4x will traverse the cube and will return you back to the face
    // and direction you started with).
    //
    // Note that this function encodes the connectivity of the faces. There is some asymmetry in the directionality
    // in order to simplify the layout when projected onto a flat surface:
    //
    //  Face 2D layout and adjacency:                         Cell layout in a face: (x,y)
    //
    //            ---------
    //            |       |                                     ___________________
    //            |   4   |                                     |     |     |     |
    //            |       |                                     | 2,0 | 2,1 | 2,2 |
    //    ---------------------------------                     |_____|_____|_____|
    //    |       |       |       |       |                     |     |     |     |
    //    |   0   |   1   |   2   |   3   |                     | 1,0 | 1,1 | 1,2 |
    //    |       |       |       |       |                     |_____|_____|_____|
    //    ---------------------------------                     |     |     |     |
    //                    |       |                             | 0,0 | 1,0 | 2,0 |
    //                    |   5   |     .                       |_____|_____|_____|     .
    //                    |       |    /|\ Up                                          /|\ y
    //                    ---------     |                                               |
    //                                  |______\ Right                                  |______\ x
    //                                         /                                               /
    //

    // TODO: encode this as an array at compile-time. (Risk of array bounds issue with enum class??)

    std::map< std::pair<int64_t, rc_direction>, std::pair<int64_t, rc_direction> > adj;

    adj[ std::make_pair(0, rc_direction::left)  ] = std::make_pair(3, rc_direction::left);
    adj[ std::make_pair(0, rc_direction::right) ] = std::make_pair(1, rc_direction::right);
    adj[ std::make_pair(0, rc_direction::up)    ] = std::make_pair(4, rc_direction::right);
    adj[ std::make_pair(0, rc_direction::down)  ] = std::make_pair(5, rc_direction::up);

    adj[ std::make_pair(1, rc_direction::left)  ] = std::make_pair(0, rc_direction::left);
    adj[ std::make_pair(1, rc_direction::right) ] = std::make_pair(2, rc_direction::right);
    adj[ std::make_pair(1, rc_direction::up)    ] = std::make_pair(4, rc_direction::up);
    adj[ std::make_pair(1, rc_direction::down)  ] = std::make_pair(5, rc_direction::right);

    adj[ std::make_pair(2, rc_direction::left)  ] = std::make_pair(1, rc_direction::left);
    adj[ std::make_pair(2, rc_direction::right) ] = std::make_pair(3, rc_direction::right);
    adj[ std::make_pair(2, rc_direction::up)    ] = std::make_pair(4, rc_direction::left);
    adj[ std::make_pair(2, rc_direction::down)  ] = std::make_pair(5, rc_direction::down);

    adj[ std::make_pair(3, rc_direction::left)  ] = std::make_pair(2, rc_direction::left);
    adj[ std::make_pair(3, rc_direction::right) ] = std::make_pair(0, rc_direction::right);
    adj[ std::make_pair(3, rc_direction::up)    ] = std::make_pair(4, rc_direction::down);
    adj[ std::make_pair(3, rc_direction::down)  ] = std::make_pair(5, rc_direction::left);

    adj[ std::make_pair(4, rc_direction::left)  ] = std::make_pair(0, rc_direction::down);
    adj[ std::make_pair(4, rc_direction::right) ] = std::make_pair(2, rc_direction::down);
    adj[ std::make_pair(4, rc_direction::up)    ] = std::make_pair(3, rc_direction::down);
    adj[ std::make_pair(4, rc_direction::down)  ] = std::make_pair(1, rc_direction::down);

    adj[ std::make_pair(5, rc_direction::left)  ] = std::make_pair(1, rc_direction::up);
    adj[ std::make_pair(5, rc_direction::right) ] = std::make_pair(3, rc_direction::up);
    adj[ std::make_pair(5, rc_direction::up)    ] = std::make_pair(2, rc_direction::up);
    adj[ std::make_pair(5, rc_direction::down)  ] = std::make_pair(0, rc_direction::up);

    return adj[ std::make_pair(face, dir) ];
}

rc_game_t::move_t
rc_game_t::get_neighbour_cell(rc_game_t::move_t x) const {


    // Get current face.
    const auto & curr_coords = std::get<0>(x);
    const auto & curr_dir = std::get<1>(x);
    const auto & [curr_face, curr_cell_x, curr_cell_y] = curr_coords;

    rc_game_t::coords_t new_coords = curr_coords;
    rc_direction new_dir = curr_dir;
    auto & [new_face, new_cell_x, new_cell_y] = new_coords;

    // Determine if the directly adjacent neighbour is on the current face. If so, return it and the same direction.
    const auto N = this->get_N();
    if(false){
    }else if( curr_dir == rc_direction::highest ){
        throw std::logic_error("Invalid direction");

    }else if( (curr_dir == rc_direction::rotate_left )
          ||  (curr_dir == rc_direction::rotate_left ) ){
        throw std::invalid_argument("Invalid direction: unable to accomodate rotations");

    }else if( (curr_dir == rc_direction::left) && (0L < curr_cell_x) ){
        new_cell_x -= 1L;

    }else if( (curr_dir == rc_direction::right) && ((1L + curr_cell_x) < N) ){
        new_cell_x += 1L;

    }else if( (curr_dir == rc_direction::down) && (0L < curr_cell_y) ){
        new_cell_y -= 1L;

    }else if( (curr_dir == rc_direction::up) && ((1L + curr_cell_y) < N) ){
        new_cell_y += 1L;

    // Otherwise we need to wrap around the cube to an adjacent face.
    }else{
        // Find the adjacent face and direction.
        const auto [adj_face, adj_dir] = this-> get_neighbour_face(curr_face, curr_dir);

        new_face = adj_face;
        new_dir = adj_dir;

        // First, wrap the relevant coordinate based on the movement direction.
        if(false){
        }else if( curr_dir == rc_direction::right ){
            new_cell_x = 0L;
        }else if( curr_dir == rc_direction::left ){
            new_cell_x = N-1L;

        }else if( curr_dir == rc_direction::up ){
            new_cell_y = 0L;
        }else if( curr_dir == rc_direction::down ){
            new_cell_y = N-1L;
        }

        // Then rotate the x and y coordinates according to the relative change in direction.
        //
        // We simplify by indicating the number of 90 degree increments required to accomplish the rotation.
        const auto get_angle = [&](rc_direction d){
            int64_t angle = 0L;
            if(false){
            }else if(d == rc_direction::right){
                angle = 0;
            }else if(d == rc_direction::up){
                angle = 1;
            }else if(d == rc_direction::left){
                angle = 2;
            }else if(d == rc_direction::down){
                angle = 3;
            }
            return angle;
        };

        auto N_rots_needed = get_angle(adj_dir) - get_angle(curr_dir);
        if(N_rots_needed < 0L){
            N_rots_needed += 4L;
        }

        // Perform the rotations.
        const auto half_N = (static_cast<double>(this->N - 1L)) * 0.5;
        while(N_rots_needed > 0){
            // Shift the coordinates to the virtual centre of the face.
            const double orig_x = static_cast<double>(new_cell_x) - half_N;
            const double orig_y = static_cast<double>(new_cell_y) - half_N;

            // Apply a 90 degree rotation.
            const double rot_x = -orig_y;
            const double rot_y = orig_x;

            // Shift back to the original coordinates (bottom-left origin).
            new_cell_x = static_cast<int64_t>( std::round(rot_x + half_N) );
            new_cell_y = static_cast<int64_t>( std::round(rot_y + half_N) );

            --N_rots_needed;
        }
    }

    return {new_coords, new_dir};
}

void rc_game_t::move(rc_game_t::move_t x){

    // Triage a requested move, breaking it down into separate shifts and face rotations.
    const auto orig_coords = std::get<0>(x);
    const auto orig_dir = std::get<1>(x);
    const auto [orig_face, orig_cell_x, orig_cell_y] = orig_coords;
    const auto orig_index = this->index(orig_coords);

    std::optional< rc_game_t::move_t > move_shift;
    std::optional< rc_game_t::move_t > move_face_rot;

    if( false ){
    }else if( (orig_dir == rc_direction::rotate_left)
          ||  (orig_dir == rc_direction::rotate_right) ){
        move_face_rot = x;
        move_shift = {};

        // A face rotation always necessitates a shift of the cells along the face's perimeter.

        // Move along neighbour cells until a cell from an adjacent face is identified.
        auto adj_cell_coords = orig_coords;
        auto [adj_face, adj_cell_x, adj_cell_y] = adj_cell_coords;
        auto adj_cell_dir = rc_direction::up; // Use up as a probe direction.
        while(adj_face == orig_face){
            std::tie(adj_cell_coords, adj_cell_dir) = this->get_neighbour_cell( rc_game_t::move_t{adj_cell_coords, adj_cell_dir} );
            std::tie(adj_face, adj_cell_x, adj_cell_y) = adj_cell_coords;
        }

        // Translate the face's rotation into a shift direction.
        int64_t N_rotations_needed = (orig_dir == rc_direction::rotate_left) ? 1L : 3L;
        while(N_rotations_needed > 0L){
            if(false){
            }else if(adj_cell_dir == rc_direction::left ){
                adj_cell_dir = rc_direction::down;
            }else if(adj_cell_dir == rc_direction::down ){
                adj_cell_dir = rc_direction::right;
            }else if(adj_cell_dir == rc_direction::right ){
                adj_cell_dir = rc_direction::up;
            }else if(adj_cell_dir == rc_direction::up ){
                adj_cell_dir = rc_direction::left;
            }

            --N_rotations_needed;
        }

        move_shift = rc_game_t::move_t{adj_cell_coords, adj_cell_dir};

    }else if( (orig_dir == rc_direction::left)
          ||  (orig_dir == rc_direction::right)
          ||  (orig_dir == rc_direction::up)
          ||  (orig_dir == rc_direction::down) ){
        move_shift = x;
        move_face_rot = {};

        // Check if the shift necessitates a face rotation. This is only the case if the cell is adjacent to the edge of
        // a face AND the direction of travel is parallel to the edge.
        std::vector<rc_direction> adj_dirs;
        if(false){
        }else if( orig_dir == rc_direction::left ){
            adj_dirs.emplace_back(rc_direction::up);
            adj_dirs.emplace_back(rc_direction::down);
        }else if( orig_dir == rc_direction::right ){
            adj_dirs.emplace_back(rc_direction::up);
            adj_dirs.emplace_back(rc_direction::down);

        }else if( orig_dir == rc_direction::up ){
            adj_dirs.emplace_back(rc_direction::left);
            adj_dirs.emplace_back(rc_direction::right);
        }else if( orig_dir == rc_direction::down ){
            adj_dirs.emplace_back(rc_direction::left);
            adj_dirs.emplace_back(rc_direction::right);
        }

        for(const auto & adj_dir : adj_dirs){
            const auto [adj_cell_coords, adj_cell_dir] = this->get_neighbour_cell( rc_game_t::move_t{orig_coords, adj_dir} );
            const auto [adj_face, adj_cell_x, adj_cell_y] = adj_cell_coords;
            if(adj_face != orig_face){
                rc_direction rot_dir = rc_direction::highest;
                if(false){
                }else if(adj_dir == rc_direction::left){
                    rot_dir = (orig_dir == rc_direction::up) ? rc_direction::rotate_left : rc_direction::rotate_right;
                }else if(adj_dir == rc_direction::right){
                    rot_dir = (orig_dir == rc_direction::up) ? rc_direction::rotate_right : rc_direction::rotate_left;
                }else if(adj_dir == rc_direction::up){
                    rot_dir = (orig_dir == rc_direction::left) ? rc_direction::rotate_right : rc_direction::rotate_left;
                }else if(adj_dir == rc_direction::down){
                    rot_dir = (orig_dir == rc_direction::left) ? rc_direction::rotate_left : rc_direction::rotate_right;
                }
                move_face_rot = rc_game_t::move_t{adj_cell_coords, rot_dir};
                break;
            }
        }

    }else{
        throw std::invalid_argument("Unsupported move direction");
    }

    // Perform the necessary moves.
    //
    // Moves should not conflict or interfere with one another, so the order is irrelevant.
    if(move_face_rot){
        this->implement_primitive_face_rotate(move_face_rot.value());
    }
    if(move_shift){
        this->implement_primitive_shift(move_shift.value());
    }

    return;
}


void rc_game_t::implement_primitive_shift(rc_game_t::move_t x){
    // Implement circular cell shifts, which involves spinning N*4 cells around an axis intersecting the centre of the
    // cube by 90 degrees.
    //
    // Note that this type of move also necessitates a rotation primitive when the cells are directly adjacent to the
    // edge of a face, but this rotation is not performed here.
    const auto orig_coords = std::get<0>(x);
    const auto orig_dir = std::get<1>(x);
    const auto [orig_face, orig_cell_x, orig_cell_y] = orig_coords;
    const auto orig_index = this->index(orig_coords);

    if( false ){
    }else if( (orig_dir == rc_direction::left)
          ||  (orig_dir == rc_direction::right)
          ||  (orig_dir == rc_direction::up)
          ||  (orig_dir == rc_direction::down) ){
        // Do nothing.
    }else{
        throw std::invalid_argument("Unsupported move direction");
    }

    for(int64_t i = 0; i < this->N; ++i){
        auto curr_index = orig_index;
        auto curr_dir = orig_dir;

        std::map<int64_t, rc_cell_t> new_cells;

        while(true){
            auto curr_coords = this->coords(curr_index);
            auto curr_cell = this->get_const_cell(curr_index);

            const auto next_x = this->get_neighbour_cell( rc_game_t::move_t{curr_coords, curr_dir} );

            const auto next_coords = std::get<0>(next_x);
            const auto next_dir = std::get<1>(next_x);
            const auto [next_face, next_cell_x, next_cell_y] = next_coords;
            const auto next_index = this->index(next_coords);

            // Insert the mapping.
            new_cells[ next_index ] = curr_cell;

            // Check if we've wrapped around the cube yet.
            if(next_index == orig_index){
                break;
            }else{
                curr_index = next_index;
                curr_dir = next_dir;
            }
        }

        // Implement the moves.
        for(const auto &c : new_cells){
            const auto& index = c.first;
            const auto& cell = c.second;
            this->get_cell(index) = cell;
        }
    }

    return;
}

void rc_game_t::implement_primitive_face_rotate(rc_game_t::move_t x){
    // Implement face rotations, which involves spinning the N*N cells of a face around the centre of the face.
    // 
    // Note that this type of move also necessitates a shift primitive, which is not performed here.
    const auto orig_coords = std::get<0>(x);
    const auto orig_dir = std::get<1>(x);
    const auto [orig_face, orig_cell_x, orig_cell_y] = orig_coords;
    const auto orig_index = this->index(orig_coords);

    if( false ){
    }else if( (orig_dir == rc_direction::rotate_left)
          ||  (orig_dir == rc_direction::rotate_right) ){
        // Do nothing.
    }else{
        throw std::logic_error("Unsupported move direction");
    }

    const int64_t N_rots_needed = ( orig_dir == rc_direction::rotate_left ) ? 1L : 3L;

    std::map<int64_t, rc_cell_t> new_cells;

    for(int64_t i = 0L; i < this->N; ++i){
        for(int64_t j = 0L; j < this->N; ++j){
            auto new_cell_x = i;
            auto new_cell_y = j;

            // Perform the rotations.
            const auto half_N = (static_cast<double>(this->N - 1L)) * 0.5;
            auto l_N_rots_needed = N_rots_needed;
            while(l_N_rots_needed > 0){
                // Shift the coordinates to the virtual centre of the face.
                const double shifted_x = static_cast<double>(new_cell_x) - half_N;
                const double shifted_y = static_cast<double>(new_cell_y) - half_N;

                // Apply a 90 degree rotation.
                const double rot_x = -shifted_y;
                const double rot_y = shifted_x;

                // Shift back to the original coordinates (bottom-left origin).
                new_cell_x = static_cast<int64_t>( std::round(rot_x + half_N) );
                new_cell_y = static_cast<int64_t>( std::round(rot_y + half_N) );

                --l_N_rots_needed;
            }

            const coords_t curr_cell_coords = { orig_face, i, j };
            const auto curr_cell_index = this->index(curr_cell_coords);

            const coords_t new_cell_coords = { orig_face, new_cell_x, new_cell_y };
            const auto new_cell_index = this->index(new_cell_coords);

            new_cells[ new_cell_index ] = this->get_const_cell(curr_cell_index);
        }
    }

    // Implement the moves.
    for(const auto &c : new_cells){
        const auto& index = c.first;
        const auto& cell = c.second;
        this->get_cell(index) = cell;
    }

    return;
}

std::vector<rc_game_t::move_t>
rc_game_t::generate_random_moves(int64_t n) const {
    std::vector<rc_game_t::move_t> out;

    std::array<rc_direction,6> dirs { rc_direction::left,
                                      rc_direction::right,
                                      rc_direction::up,
                                      rc_direction::down,
                                      rc_direction::rotate_left,
                                      rc_direction::rotate_right, };

    // Create random distributions for sampled quantities.
    //
    // Note: endpoints are inclusive.
    std::uniform_int_distribution<int64_t> rd_dirs(0L, dirs.size()-1L);
    std::uniform_int_distribution<int64_t> rd_face(0L, 5L);
    std::uniform_int_distribution<int64_t> rd_cell_x(0L, this->N-1L);
    std::uniform_int_distribution<int64_t> rd_cell_y(0L, this->N-1L);

    std::random_device rdev;
    std::mt19937 re( rdev() ); //Random engine.

    for(int64_t i = 0; i < n; ++i){
        out.emplace_back(move_t{ coords_t{ rd_face(re), rd_cell_x(re), rd_cell_y(re)},
                                 dirs.at( rd_dirs(re) ) });
    }
    return out;
}

// ------------------------------------------------------------------------------------
// Game built around representation.
// ------------------------------------------------------------------------------------

RotatingCubeGame::RotatingCubeGame(){
    Reset();
}

void RotatingCubeGame::Reset(){
    rc_game.reset(rc_game_size);

    rc_game_colour_map.clear();

    rc_game_move_history.clear();
    rc_game_move_history_current = 0L;

    // Reset the update time.
    const auto t_now = std::chrono::steady_clock::now();
    t_cube_updated = t_now;

    return;
}

void RotatingCubeGame::append_history(rc_game_t::move_t x){

    // Trim any moves after the current history, if any are present.
    {
        decltype(rc_game_move_history) l_history;
        auto n = rc_game_move_history_current;
        for(const auto& move : rc_game_move_history){
            --n;
            if(n < 0L) break;
            l_history.emplace_back(move);
        }
        rc_game_move_history = l_history;
    }

    rc_game_move_history.emplace_back(x);
    const auto n = static_cast<int64_t>(rc_game_move_history.size());
    rc_game_move_history_current = n;
    return;
}

void RotatingCubeGame::jump_to_history(int64_t n){
    const auto l_N = static_cast<int64_t>(rc_game_move_history.size());
    if( !isininc(0L, n, l_N) ){
        throw std::invalid_argument("Requested history is not available");
    }

    rc_game.reset(rc_game_size);
    rc_game_move_history_current = n;
    for(const auto& move : rc_game_move_history){
        --n;
        if(n < 0L) break;
        rc_game.move(move);
    }
    return;
}


bool RotatingCubeGame::Display(bool &enabled){
    if( !enabled ) return true;

    const auto win_width  = static_cast<int>(700);
    const auto win_height = static_cast<int>(500);
    auto flags = ImGuiWindowFlags_AlwaysAutoResize
               | ImGuiWindowFlags_NoScrollWithMouse
               | ImGuiWindowFlags_NoNavInputs
               | ImGuiWindowFlags_NoScrollbar ;
    ImGui::SetNextWindowSize(ImVec2(win_width, win_height), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(100, 100), ImGuiCond_FirstUseEver);
    ImGui::Begin("Cube", &enabled, flags );

    const auto f = ImGui::IsWindowFocused();

    // Reset the game before any game state is used.
    if( f && ImGui::IsKeyPressed(SDL_SCANCODE_R) ){
        Reset();
    }

    double t_updated_diff = 0.0;
    double t_diff_decay_factor = 0.0;
    const auto update_t_diff = [&](){
        const auto t_now = std::chrono::steady_clock::now();
        t_updated_diff = std::chrono::duration_cast<std::chrono::milliseconds>(t_now - t_cube_updated).count();
        t_diff_decay_factor = 1.0 - (std::clamp<double>(t_updated_diff, 0.0, rc_game_anim_dt) / rc_game_anim_dt);
        return;
    };
    update_t_diff();

    const auto propagate_t_diff = [&](){
        const auto t_now = std::chrono::steady_clock::now();
        t_cube_updated = t_now;
        update_t_diff();
        return;
    };

    if(f){
        ImGuiIO &io = ImGui::GetIO();
        const bool hotkey_ctrl_z = io.KeyCtrl && ImGui::IsKeyPressed(SDL_SCANCODE_Z);
        const bool hotkey_ctrl_y = io.KeyCtrl && ImGui::IsKeyPressed(SDL_SCANCODE_Y);

        const auto l_N = static_cast<int64_t>(rc_game_move_history.size());
        if(0L < l_N){
            const auto l_n = rc_game_move_history_current;
            if(false){
            }else if( hotkey_ctrl_z
                  &&  (0L < l_n) ){
                jump_to_history( l_n - 1L );
                propagate_t_diff();
            }else if( hotkey_ctrl_y
                  &&  (l_n < l_N) ){
                jump_to_history( l_n + 1L );
                propagate_t_diff();
            }
        }
    }

    const auto reset = ImGui::Button("Reset");
    if(reset){
        Reset();
    }
    ImGui::SameLine();
    {
        auto l_N = static_cast<int>(rc_game.get_N());
        ImGui::SliderInt("Size", &l_N, 1, 10);
        if(l_N != rc_game_size){
            rc_game_size = l_N;
            Reset();
        }
    }
    for(const int64_t n : { 3L, 4L, 5L, 7L, 10L}){
        ImGui::SameLine();
        std::stringstream ss;
        ss << "Scramble (" << n << ")";
        const auto scramble = ImGui::Button(ss.str().c_str());
        if(scramble){
            const auto moves = rc_game.generate_random_moves(n);
            for(const auto& move : moves){
                rc_game.move(move);
                append_history( move );
                propagate_t_diff();
            }
        }
    }
    if( !rc_game_move_history.empty() ){
        ImGui::SameLine();

        const auto l_orig = rc_game_move_history_current;
        imgui_slider_int_wrapper<int64_t>("History", rc_game_move_history_current,
                                          0L, static_cast<int64_t>(rc_game_move_history.size()),
            [&](void){
                jump_to_history( rc_game_move_history_current );
                propagate_t_diff();
                return;
            });
    }
    ImGui::Separator();

    const int64_t rc_game_box_width = 1200;
    const int64_t rc_game_box_height = 800;


    // Lay out the faces and cells on a grid.
    const auto rc_game_N = rc_game.get_N();
    
    const int64_t cell_count_height = rc_game_N * 3 + 2;
    const int64_t cell_count_width = rc_game_N * 4 + 2;

    const int64_t cell_height = static_cast<int64_t>(std::floor(1.0f * rc_game_box_height / cell_count_height));
    const int64_t cell_width  = static_cast<int64_t>(std::floor(1.0f * rc_game_box_width  / cell_count_width ));


    ImVec2 curr_screen_pos = ImGui::GetCursorScreenPos();
    ImVec2 curr_window_pos = ImGui::GetCursorPos();
    //ImVec2 window_extent = ImGui::GetContentRegionAvail();
    ImDrawList *window_draw_list = ImGui::GetWindowDrawList();

    {
        const auto c = ImColor(0.7f, 0.7f, 0.8f, 1.0f);
        window_draw_list->AddRect(curr_screen_pos, ImVec2( curr_screen_pos.x + rc_game_box_width, 
                                                           curr_screen_pos.y + rc_game_box_height ), c);
    }

    const auto block_dims = ImVec2( cell_width, cell_height );

    // Use a placeholder object to determine which drag-and-drop payload is available, if any.
    ImGui::Dummy(ImVec2( rc_game_box_width, rc_game_box_height));
    std::optional<int64_t> drag_and_drop_index;
    if( ImGui::BeginDragDropTarget() ){
        int64_t l_index = -10;
        if(const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("rc_game_index", ImGuiDragDropFlags_AcceptPeekOnly); payload != nullptr){
            if(payload->DataSize != sizeof(int64_t)){
                throw std::logic_error("Drag-and-drop payload is not expected size, refusing to continue");
            }
            drag_and_drop_index = *static_cast<const int64_t*>(payload->Data);
        }

        ImGui::EndDragDropTarget();
    }
    ImGui::SetCursorPos(curr_window_pos);

    std::optional< std::tuple<int64_t, int64_t, rc_game_t::coords_t> > drag_and_drop_grid_coords;



    // Walk over the grid.
    for(int64_t i = 1; (i+1) < cell_count_width; ++i){
        for(int64_t j = 1; (j+1) < cell_count_height; ++j){
            ImVec2 cell_pos_window( curr_window_pos.x + (cell_width  * i),
                                    curr_window_pos.y + (cell_height * j) );
            ImVec2 cell_pos_screen( curr_screen_pos.x + (cell_width  * i),
                                    curr_screen_pos.y + (cell_height * j) );

            rc_game_t::coords_t c { -1, -1, -1 };

            if(false){
            }else if( isininc( rc_game_N*0 + 1, i, rc_game_N*1) 
                  &&  isininc( rc_game_N*1 + 1, j, rc_game_N*2) ){
                std::get<0>(c) = 0; // Face number.
                std::get<1>(c) = i - 1 - (rc_game_N*0); // x coordinate.
                std::get<2>(c) = j - 1 - (rc_game_N*1); // y coordinate.

            }else if( isininc( rc_game_N*1 + 1, i, rc_game_N*2) 
                  &&  isininc( rc_game_N*1 + 1, j, rc_game_N*2) ){
                std::get<0>(c) = 1; // Face number.
                std::get<1>(c) = i - 1 - (rc_game_N*1); // x coordinate.
                std::get<2>(c) = j - 1 - (rc_game_N*1); // y coordinate.

            }else if( isininc( rc_game_N*2 + 1, i, rc_game_N*3) 
                  &&  isininc( rc_game_N*1 + 1, j, rc_game_N*2) ){
                std::get<0>(c) = 2; // Face number.
                std::get<1>(c) = i - 1 - (rc_game_N*2); // x coordinate.
                std::get<2>(c) = j - 1 - (rc_game_N*1); // y coordinate.

            }else if( isininc( rc_game_N*3 + 1, i, rc_game_N*4) 
                  &&  isininc( rc_game_N*1 + 1, j, rc_game_N*2) ){
                std::get<0>(c) = 3; // Face number.
                std::get<1>(c) = i - 1 - (rc_game_N*3); // x coordinate.
                std::get<2>(c) = j - 1 - (rc_game_N*1); // y coordinate.

            }else if( isininc( rc_game_N*1 + 1, i, rc_game_N*2) 
                  &&  isininc( rc_game_N*0 + 1, j, rc_game_N*1) ){
                std::get<0>(c) = 4; // Face number.
                std::get<1>(c) = i - 1 - (rc_game_N*1); // x coordinate.
                std::get<2>(c) = j - 1 - (rc_game_N*0); // y coordinate.

            }else if( isininc( rc_game_N*2 + 1, i, rc_game_N*3) 
                  &&  isininc( rc_game_N*2 + 1, j, rc_game_N*3) ){
                std::get<0>(c) = 5; // Face number.
                std::get<1>(c) = i - 1 - (rc_game_N*2); // x coordinate.
                std::get<2>(c) = j - 1 - (rc_game_N*2); // y coordinate.

            }

            // Invert the y coordinate (map between screen space and the cell layout).
            std::get<2>(c) = (rc_game_N - 1) - std::get<2>(c);

            const auto index = rc_game.index(c);

            if(rc_game.confirm_index_valid(index)){
                // If this is the cell being dragged, save the coordinates for later.
                if( drag_and_drop_index
                &&  (index == drag_and_drop_index.value()) ){
                    drag_and_drop_grid_coords = std::make_tuple(i, j, c);
                }

                const auto l_colour_num = rc_game.get_const_cell(index).colour;
                auto l_colour = rc_game.colour_to_rgba(l_colour_num);

                // Animate the colour.
                const auto has_cached_colour = (rc_game_colour_map.count(index) != 0UL);
                if(!has_cached_colour){
                    // Insert the current colour into the cache.
                    rc_game_colour_map[index] = l_colour_num;
                }else{
                    // If the blend factor is sufficiently low, update the cache to terminate the animation.
                    if(t_diff_decay_factor < 0.01){
                        rc_game_colour_map[index] = l_colour_num;
                    }

                    // Retrieve the cached colour and interpolate between the colours.
                    const auto prev_colour_num = rc_game_colour_map[index];
                    const auto prev_colour = rc_game.colour_to_rgba(prev_colour_num);
                    for(auto i = 0UL; i < l_colour.size(); ++i){
                        l_colour.at(i) = std::clamp<float>(l_colour.at(i) + (prev_colour.at(i) - l_colour.at(i)) * t_diff_decay_factor, 0.0f, 1.0f);
                    }
                }
                const auto im_col = ImColor(l_colour.at(0), l_colour.at(1), l_colour.at(2), l_colour.at(3)).Value;

                // Check if this cell can accept an in-progress drag-and-drop index.
                bool drag_and_drop_active = !!drag_and_drop_index;

                const auto & [ cell_face, cell_x, cell_y ] = c;

                std::stringstream ss;
                ss << "##";
                ss << i << ", " << j << "\n"
                   << cell_face << ", "
                   << cell_x << ", "
                   << cell_y << "\n";

                ImGui::SetCursorPos(cell_pos_window);

                int styles_overridden = 0;
                {
                    auto im_colour_button = im_col;
                    auto im_colour_hovered = im_col;
                    auto im_colour_active = im_col;

                    im_colour_button.w *= 0.9;
                    im_colour_hovered.w *= 0.8;
                    im_colour_active.w *= 0.6;

                    if(drag_and_drop_active){
                        im_colour_button.w *= 0.75;
                        im_colour_hovered.w *= 0.75;
                        im_colour_active.w *= 0.75;
                    }

                    // Temporarily alter the appearance of buttons.
                    const auto& styles = ImGui::GetStyle();

                    ImGui::PushStyleColor(ImGuiCol_Button, im_colour_button);
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, im_colour_hovered);
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, im_colour_active);
                    styles_overridden += 3;
                }

                //ImGui::PushStyleColor(ImGuiCol_Button, im_col);

                // Draw the button.
                //
                // Note that if the text is not unique, then a unique ID needs to be provided.
                ImGui::Button(ss.str().c_str(), block_dims);

                //ImGui::PopStyleColor(1);
                if(0L < styles_overridden){
                    ImGui::PopStyleColor(styles_overridden);
                }

                // Make the cell draggable.
                if( ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)){
                    int64_t payload = index;
                    ImGui::SetDragDropPayload("rc_game_index", static_cast<void*>(&payload), sizeof(payload));

                    // Show a preview of the cube being dragged.
                    ImGui::Text("Cell");

                    ImGui::EndDragDropSource();
                }

                // Draw a border around the cell.
                const auto c_border = ImColor(1.0f, 1.0f, 1.0f, 0.60f);
                window_draw_list->AddRect(cell_pos_screen, ImVec2( cell_pos_screen.x + block_dims.x, 
                                                                   cell_pos_screen.y + block_dims.y ), c_border);
            }
        }
    }
    ImGui::SetCursorPos(curr_window_pos);



    // Walk over the grid.
    if(drag_and_drop_grid_coords){
        auto [i, j, c] = drag_and_drop_grid_coords.value();

        using pack_t = std::vector< std::tuple< int64_t, int64_t, rc_direction, std::string > >;
        for( auto [di, dj, dir, desc] : pack_t { { -1,  0, rc_direction::left,         std::string("left") },
                                                 {  1,  0, rc_direction::right,        std::string("right") },
                                                 {  0, -1, rc_direction::up,           std::string("up") },
                                                 {  0,  1, rc_direction::down,         std::string("down") },
                                                 { -1, -1, rc_direction::rotate_left,  std::string("rotate\nleft") },
                                                 {  1, -1, rc_direction::rotate_right, std::string("rotate\nright") } } ){
            ImVec2 cell_pos_screen( curr_screen_pos.x + (cell_width  * (i + di)),
                                    curr_screen_pos.y + (cell_height * (j + dj)) );
            ImVec2 cell_pos_window( curr_window_pos.x + (cell_width  * (i + di)),
                                    curr_window_pos.y + (cell_height * (j + dj)) );

            std::stringstream ss;
            if( false ){
            }else if( (dir == rc_direction::left)
                  ||  (dir == rc_direction::right)
                  ||  (dir == rc_direction::up)
                  ||  (dir == rc_direction::down) ){
                const auto [adj_c, adj_dir] = rc_game.get_neighbour_cell({c, dir});
                const auto [adj_f, adj_x, adj_y] = adj_c;
                ss << "##" << desc << "\n" << adj_f << "," << adj_x << "," << adj_y;
            }else{
                ss << "##" << desc;
            }

            ImGui::SetCursorPos(cell_pos_window);
            ImGui::Button(ss.str().c_str(), block_dims);

            // Accept a cell dragged here.
            if( ImGui::BeginDragDropTarget() ){
                if(const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("rc_game_index"); payload != nullptr){
                    if(payload->DataSize != sizeof(int64_t)){
                        throw std::logic_error("Drag-and-drop payload is not expected size, refusing to continue");
                    }
                    const int64_t payload_index = *static_cast<const int64_t*>(payload->Data);
                    const int64_t l_index = rc_game.index(c);
                    if(l_index != payload_index){
                        throw std::logic_error("Drag-and-drop inconsistency, unable to continue");
                    }

                    // Implement the move.
                    const auto l_move = rc_game_t::move_t{c, dir};
                    rc_game.move(l_move);
                    append_history( l_move );
                    propagate_t_diff();
                }
                ImGui::EndDragDropTarget();
            }

            // Show an indicator of what the drop buttons will do.
            const auto c_border = ImColor(0.8f, 0.8f, 0.8f, 1.0f);

            std::vector<vec2<float>> verts;

            if( (dir == rc_direction::left)
            ||  (dir == rc_direction::right)
            ||  (dir == rc_direction::up)
            ||  (dir == rc_direction::down) ){
                // Left arrow.        x       y
                verts.emplace_back( -0.50,  0.00 );
                verts.emplace_back( -0.10, -0.35 );
                verts.emplace_back( -0.10, -0.20 );
                verts.emplace_back(  0.50, -0.20 );
                verts.emplace_back(  0.50,  0.20 );
                verts.emplace_back( -0.10,  0.20 );
                verts.emplace_back( -0.10,  0.35 );
                verts.emplace_back( -0.50,  0.00 );

            }else{
                // Left rotation symbol.
                verts.emplace_back( -0.40,  0.40 );
                verts.emplace_back( -0.40, -0.05 );
                verts.emplace_back( -0.25,  0.10 );
                verts.emplace_back(  0.00, -0.05 );
                verts.emplace_back( -0.25, -0.35 );
                verts.emplace_back(  0.10, -0.45 );
                verts.emplace_back(  0.40,  0.00 );
                verts.emplace_back( -0.05,  0.30 );
                verts.emplace_back(  0.05,  0.40 );
                verts.emplace_back( -0.40,  0.40 );
            }

            const auto pi = std::acos(-1.0);
            for(auto & v : verts){
                if(false){
                }else if(dir == rc_direction::left){
                    // Do nothing.
                }else if(dir == rc_direction::right){
                    v = v.rotate_around_z(pi);
                }else if(dir == rc_direction::up){
                    v = v.rotate_around_z(pi * 1.5);
                }else if(dir == rc_direction::down){
                    v = v.rotate_around_z(pi * 0.5);

                }else if(dir == rc_direction::rotate_left){
                    // Do nothing.
                }else if(dir == rc_direction::rotate_right){
                    // Mirror.
                    v.x *= -1.0;
                }
            }

            window_draw_list->PathClear();
            for(auto & v : verts){
                // Scale and rotate if needed.
                ImVec2 im_v;
                im_v.x = cell_pos_screen.x + block_dims.x * 0.5 + v.x * (block_dims.x * 0.45);
                im_v.y = cell_pos_screen.y + block_dims.y * 0.5 - v.y * (block_dims.y * 0.45);
                window_draw_list->PathLineTo( im_v );
            }
            float thickness = 1.5f;
            bool closed = false;
            window_draw_list->PathStroke(c_border, closed, thickness);
        }

    }
    ImGui::SetCursorPos(curr_window_pos);
    ImGui::Dummy(ImVec2(rc_game_box_width, rc_game_box_height));
    ImGui::End();

    return true;
}

