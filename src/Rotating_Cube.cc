//Rotating_Cube.cc - A part of DICOMautomaton 2025. Written by hal clark.
#pragma once

#include <cstdint>
#include <vector>
#include <optional>
#include <tuple>
#include <map>
#include <algorithm>
#include <stdexcept>
#include <limits>
#include <cmath>

#include "YgorMisc.h"
#include "YgorLog.h"

#include "Rotating_Cube.h"

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

std::tuple<rc_game_t::coords_t, rc_direction>
rc_game_t::get_neighbour_cell(std::tuple<rc_game_t::coords_t, rc_direction> x) const {


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
        const auto half_N = (static_cast<double>(N) - 1) * 0.5;
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

void rc_game_t::move(std::tuple<rc_game_t::coords_t, rc_direction> x){

    // Implements a move by rotating a portion of the cube.
    //
    // There are two distinct kinds of rotations possible:
    //
    // (1) face rotations, which involves spinning N*(N+4) cells around an axis intersecting the centre of the cube by
    // 90 degrees. Note that this type of move impacts one adjacent face.
    //
    // (2) cell shifts, which involves spinning N*4 cells around an axis intersecting the centre of the cube by 90
    // degrees. This type only impacts a few cells. Note that the cells cannot be directly adjacent to the edge of any
    // faces, otherwise the adjacent face will need to rotate as well (i.e., then it will be a type (1) move).

    const auto orig_coords = std::get<0>(x);
    const auto orig_dir = std::get<1>(x);
    const auto [orig_face, orig_cell_x, orig_cell_y] = orig_coords;
    const auto orig_index = this->index(orig_coords);


    // Type (1) moves:

    // TODO.

    // Type (2) moves:
    for(int64_t i = 0; i < this->N; ++i){
        auto curr_index = orig_index;
        auto curr_dir = orig_dir;

        if( false ){
        }else if( (orig_dir == rc_direction::left)
              ||  (orig_dir == rc_direction::right)
              ||  (orig_dir == rc_direction::up)
              ||  (orig_dir == rc_direction::down) ){
            // Do nothing.
        }else{
            throw std::logic_error("Unsupported move direction");
        }

        std::map<int64_t, rc_cell_t> new_cells;

        while(true){
            auto curr_coords = this->coords(curr_index);
            auto curr_cell = this->get_const_cell(curr_index);

            const auto next_x = this->get_neighbour_cell( std::make_tuple(curr_coords, curr_dir ));

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


