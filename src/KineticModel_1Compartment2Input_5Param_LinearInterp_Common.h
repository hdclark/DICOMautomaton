//KineticModel_1Compartment2Input_5Param_LinearInterp_Common.h.

#pragma once

#include <boost/serialization/nvp.hpp>
#include <boost/serialization/shared_ptr.hpp>
#include <limits>
#include <memory>

#include "YgorMathIOBoostSerialization.h"

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


namespace boost {
namespace serialization {

template<typename Archive>
void serialize(Archive &a, 
               KineticModel_1Compartment2Input_5Param_LinearInterp_Parameters &p, 
               const unsigned int /*version*/ ){
    a & boost::serialization::make_nvp("cAIF",  p.cAIF)

      & boost::serialization::make_nvp("cVIF",  p.cVIF)

      & boost::serialization::make_nvp("cROI",  p.cROI)

      & boost::serialization::make_nvp("FittingPerformed", p.FittingPerformed)
      & boost::serialization::make_nvp("FittingSuccess",   p.FittingSuccess)

      & boost::serialization::make_nvp("RSS",   p.RSS)

      & boost::serialization::make_nvp("k1A",   p.k1A)
      & boost::serialization::make_nvp("tauA",  p.tauA)
      & boost::serialization::make_nvp("k1V",   p.k1V)
      & boost::serialization::make_nvp("tauV",  p.tauV)
      & boost::serialization::make_nvp("k2",    p.k2);
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
 
