//BED_Conversion.cc - A part of DICOMautomaton 2017. Written by hal clark.

#include <cmath>
#include <stdexcept>

#include "BED_Conversion.h"

//struct BEDabr {
//    double val = std::numeric_limits<double>::quiet_NaN(); 
//    double abr = std::numeric_limits<double>::quiet_NaN(); // Alpha/beta used to compute the BED.
//};

BEDabr
operator-(BEDabr A){
    A.val *= -1.0;
    return A;
}

BEDabr
operator+(BEDabr A, BEDabr B){
    if(false){
    }else if( !std::isfinite(A.abr) ){ //|| (A.abr < 0) ){
        throw std::runtime_error("Cannot sum BED's -- LHS has invalid alpha/beta.");
    }else if( !std::isfinite(B.abr) ){ //|| (B.abr < 0) ){
        throw std::runtime_error("Cannot sum BED's -- LHS has invalid alpha/beta.");
    }else if( A.abr != B.abr ){
        throw std::runtime_error("Cannot sum BED's -- they have different alpha/beta.");
    }
    A.val += B.val;
    return A;
}

BEDabr
operator-(BEDabr A, BEDabr B){
    return (A + (-B));
}

BEDabr
operator*(BEDabr A, double B){
    A.val *= B;
    return A;
}

BEDabr
operator/(BEDabr A, double B){
    A.val /= B;
    return A;
}

double
operator/(BEDabr A, BEDabr B){
    if(false){
    }else if( !std::isfinite(A.abr) ){ //|| (A.abr < 0) ){
        throw std::runtime_error("Cannot divide BED's -- LHS has invalid alpha/beta.");
    }else if( !std::isfinite(B.abr) ){ //|| (B.abr < 0) ){
        throw std::runtime_error("Cannot divide BED's -- LHS has invalid alpha/beta.");
    }else if( A.abr != B.abr ){
        throw std::runtime_error("Cannot divide BED's -- they have different alpha/beta.");
    }
    return A.val / B.val;
}



BEDabr 
BEDabr_from_scalar_abr(double BED, double abr){
    BEDabr B;
    B.val = BED;
    B.abr = abr;
    return B;
}

BEDabr 
BEDabr_from_n_d_abr(double n, double d, double abr){
    // n   -- Number of fractions (nominally integer, but can be analytically extended to be non-integer too).
    // d   -- Dose per fraction (generally in Gy, but can be something else if you are consistent).
    // abr -- Alpha/beta for the tissue of interest (generally in Gy, but can be something else if you are consistent).
    //        (Typical values are 3 Gy for normal tissues, 10 Gy for tumours.)
    //
    // returns: BEDabr. Remember that you cannot combine BED's if they have different alpha/beta!
    //          To help enforce this, the alpha/beta specified is 'carried around' by this struct.
    //
    // Note: This simplisitic BED calculation ignores repopulation effects. These effects can be significant in certain 
    //       tissues and tumour sites. If the treatment schedule is short enough, the approximation is usually valid.
    BEDabr B;
    B.val = n*d*(1.0 + d/abr);
    B.abr = abr;
    return B;
}

BEDabr 
BEDabr_from_n_D_abr(double n, double D, double abr){
    // n   -- Number of fractions (nominally integer, but can be analytically extended to be non-integer too).
    // D   -- Total dose delivered via the specified fractionation (generally in Gy, but can be something else if you are consistent).
    // abr -- Alpha/beta for the tissue of interest (generally in Gy, but can be something else if you are consistent).
    //        (Typical values are 3 Gy for normal tissues, 10 Gy for tumours.)
    //
    // returns: BEDabr. Remember that you cannot combine BED's if they have different alpha/beta!
    //          To help enforce this, the alpha/beta specified is 'carried around' by this struct.
    //
    // Note: This simplisitic BED calculation ignores repopulation effects. These effects can be significant in certain 
    //       tissues and tumour sites. If the treatment schedule is short enough, the approximation is usually valid.
    BEDabr B;
    B.val = D*(1.0 + (D/n)/abr);
    B.abr = abr;
    return B;
}

double
n_from_d_BEDabr(double d, BEDabr B){
    // d   -- Dose per fraction (generally in Gy, but can be something else if you are consistent).
    //
    // returns: The number of fractions needed to achieve the given B assuming d dose per fraction.
    //
    // Note: This routine is typically used to convert functions with signatures like F(n,BED) to G(d,BED).
    const double numer = B.val;
    const double denom = d * (1.0 + d/B.abr);
    return numer/denom;
}

double
D_from_n_BEDabr(double n, BEDabr B){
    // n   -- Number of fractions (nominally integer, but can be analytically extended to be non-integer too).
    //
    // returns: Dose in whatever units B is in (usually Gy).
    //
    // Note: This simplisitic BED calculation ignores repopulation effects. These effects can be significant in certain 
    //       tissues and tumour sites. If the treatment schedule is short enough, the approximation is usually valid.
    const double nabr = n*B.abr;
    return 0.5*nabr*( std::sqrt(1.0 + 4.0*B.val/nabr) - 1.0 );
}

double
D_from_d_BEDabr(double d, BEDabr B){
    // d   -- Dose per fraction (generally in Gy, but can be something else if you are consistent).
    //
    // returns: Dose in whatever units B and d are in (usually Gy).
    //
    // Note: This simplisitic BED calculation ignores repopulation effects. These effects can be significant in certain 
    //       tissues and tumour sites. If the treatment schedule is short enough, the approximation is usually valid.
    const double n = n_from_d_BEDabr(d, B);
    return D_from_n_BEDabr( n, B );
}


