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

    const auto N_faces = this->N * this->N * 6;
    this->faces.resize( N_faces ); 

    for(int64_t f = 0; f < 6; ++f){
        for(int64_t x = 0; x < N; ++x){
            for(int64_t y = 0; y < N; ++y){
                const auto c = std::make_tuple(f, x, y);
                const auto index = this->index(c);
                this->assert_index_valid(index);
                this->get_face(index).colour = f;
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
    const auto N_faces = this->N * this->N * 6;
    return isininc(0, index, N_faces - 1);
}

rc_game_t::coords_t rc_game_t::coords(int64_t index) const {
    this->assert_index_valid(index); 

    const ldiv_t q = ::ldiv(index,6);
    const ldiv_t z = ::ldiv(q.quot,this->N);
    const int64_t f = q.rem;
    const int64_t y = z.rem;
    const int64_t x = z.quot;

    const auto t = std::make_tuple(f, x, y);
    if( (this->index(t) != index)
    ||  !this->confirm_index_valid(index) ){
        throw std::logic_error("Indexing scheme mismatch.");
    }
    return t;
}

const rc_face_t& rc_game_t::get_const_face(int64_t index) const {
    this->assert_index_valid(index); 
    return this->faces.at(index);
}

rc_face_t& rc_game_t::get_face(int64_t index){
    this->assert_index_valid(index); 
    return this->faces.at(index);
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

    // Determine if the neighbour is on the current face. If so, return it and the same direction.
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

