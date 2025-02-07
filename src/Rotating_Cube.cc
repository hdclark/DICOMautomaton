//Rotating_Cube.cc - A part of DICOMautomaton 2025. Written by hal clark.
#pragma once

#include <cstdint>
#include <vector>
#include <optional>
#include <tuple>
#include <algorithm>
#include <stdexcept>
#include <limits>

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

