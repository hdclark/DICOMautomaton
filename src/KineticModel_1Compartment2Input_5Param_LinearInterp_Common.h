//KineticModel_1Compartment2Input_5Param_LinearInterp_Common.h.

#pragma once

#include <limits>
#include <memory>

#include "YgorMathIOSerialization.h"

template <class T> class samples_1D;



// Shuttle struct for passing around the state needed to perform a pharmacokinetic modeling fit.
struct KineticModel_1Compartment2Input_5Param_LinearInterp_Parameters {
 
    // Experimental observations.
    std::shared_ptr<samples_1D<double>> cAIF;

    std::shared_ptr<samples_1D<double>> cVIF;

    std::shared_ptr<samples_1D<double>> cROI;

    // Indicators for various things.
    bool FittingPerformed = false;
    bool FittingSuccess   = false;

    // Fitting quantities (IFF available).
    double RSS  = std::numeric_limits<double>::quiet_NaN(); // Residual sum of squares.

    // 5-parameter liver CT perfusion parameters.
    double k1A  = std::numeric_limits<double>::quiet_NaN();
    double tauA = std::numeric_limits<double>::quiet_NaN();
    double k1V  = std::numeric_limits<double>::quiet_NaN();
    double tauV = std::numeric_limits<double>::quiet_NaN();
    double k2   = std::numeric_limits<double>::quiet_NaN();

};


namespace ygor {
namespace serialization {

#ifndef DCMA_YGOR_SHARED_PTR_SERIALIZATION
#define DCMA_YGOR_SHARED_PTR_SERIALIZATION
template<typename T>
void serialize(xml_oarchive &a, std::shared_ptr<T> &p){
    auto has_value = static_cast<bool>(p);
    a & make_nvp("has_value", has_value);
    if(has_value){
        a & make_nvp("value", *p);
    }
    return;
}

template<typename T>
void serialize(xml_iarchive &a, std::shared_ptr<T> &p){
    bool has_value = false;
    a & make_nvp("has_value", has_value);
    if(has_value){
        p = std::make_shared<T>();
        a & make_nvp("value", *p);
    }else{
        p.reset();
    }
    return;
}
#endif // DCMA_YGOR_SHARED_PTR_SERIALIZATION

template<typename Archive>
void serialize(Archive &a, 
               KineticModel_1Compartment2Input_5Param_LinearInterp_Parameters &p){
    a & make_nvp("cAIF",  p.cAIF)

      & make_nvp("cVIF",  p.cVIF)

      & make_nvp("cROI",  p.cROI)

      & make_nvp("FittingPerformed", p.FittingPerformed)
      & make_nvp("FittingSuccess",   p.FittingSuccess)

      & make_nvp("RSS",   p.RSS)

      & make_nvp("k1A",   p.k1A)
      & make_nvp("tauA",  p.tauA)
      & make_nvp("k1V",   p.k1V)
      & make_nvp("tauV",  p.tauV)
      & make_nvp("k2",    p.k2);
    return;
}

}
}


//This struct is returned when evaluating the model.
struct KineticModel_1Compartment2Input_5Param_LinearInterp_Results {

    // Evaluated model value.
    double I = std::numeric_limits<double>::quiet_NaN();

}; 


//Means for evaluating the model at a given time with the supplied parameters.
void
Evaluate_Model( const KineticModel_1Compartment2Input_5Param_LinearInterp_Parameters &state,
                const double t,
                KineticModel_1Compartment2Input_5Param_LinearInterp_Results &res);
 
