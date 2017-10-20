//BED_Conversion.h.

#include <limits>

// These functions assist in converting between dose and BED 'spaces'.

#pragma once

struct BEDabr { // Biologically effective dose assuming a specific alpha/beta ratio.
    double val = std::numeric_limits<double>::quiet_NaN(); 
    double abr = std::numeric_limits<double>::quiet_NaN(); // Alpha/beta used to compute the BED.
};


BEDabr
operator-(BEDabr A);

BEDabr
operator+(BEDabr A, BEDabr B);
    
BEDabr
operator-(BEDabr A, BEDabr B);

BEDabr
operator*(BEDabr A, double B);

BEDabr
operator/(BEDabr A, double B);

double
operator/(BEDabr A, BEDabr B);



BEDabr 
BEDabr_from_scalar(double B, double abr);

BEDabr 
BEDabr_from_n_d_abr(double n, double d, double abr);

BEDabr 
BEDabr_from_n_D_abr(double n, double D, double abr);

double
n_from_d_BEDabr(double d, BEDabr B);

double
D_from_n_BEDabr(double n, BEDabr B);

double
D_from_d_BEDabr(double d, BEDabr B);


