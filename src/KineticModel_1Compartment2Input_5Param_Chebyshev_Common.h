//KineticModel_1Compartment2Input_5Param_Chebyshev_Common.h.

#pragma once

#include <boost/serialization/nvp.hpp>
#include <boost/serialization/version.hpp>
#include <cstddef>
#include <limits>
#include <memory>

#include "YgorMathChebyshevIOBoostSerialization.h"
#include "YgorMathIOBoostSerialization.h"

template <class T> class cheby_approx;
template <class T> class samples_1D;


// Shuttle struct for passing around the state needed to perform a pharmacokinetic modeling fit.
// The design of passing around a struct of pointers and parameters was settled on because this
// approach:
//
//   1. Requires little copying of large time courses (AIF, VIF, and ROI) is done over the lifetime
//      of the modeling process.
//
//   2. Keeps the state needed to perform the modeling process (1) alive as long as needed, and
//      (2) alive and handy (e.g., exposed to the invoker after the modeling process) in case we 
//      want to attempt to re-fit afterward.
//
//   3. Can be used by the caller and internally without marshalling or even pointer alterations.
//
//   4. Since the same interface is used to retrieve fitted values and specify initial estimates,
//      iterative modeling is very easy to accomplish.
//
//   5. It is made to operate with std::future's return method of passing out a task's return 
//      value using std::move(). If function parameters were directly used, some state would be
//      lost when the future returned.
//
//   6. It is easily serialized and a copy can be kept with the parameter maps, ensuring you have
//      all necessary information to reconstruct the model afterward.
//
struct KineticModel_1Compartment2Input_5Param_Chebyshev_Parameters {
 
    // Experimental observations.
    std::shared_ptr<cheby_approx<double>> cAIF;
    std::shared_ptr<cheby_approx<double>> dcAIF;

    std::shared_ptr<cheby_approx<double>> cVIF;
    std::shared_ptr<cheby_approx<double>> dcVIF;

    std::shared_ptr<samples_1D<double>>   cROI;

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

    // Computation adjustments.
    
    // Exponential coefficient truncation point.
    //   3 usually works (roughly). 5 is probably OK. 10 should suffice. 
    //   20 could be overkill. Depends on params, though.
    size_t ExpApproxTrunc = 10;                                            

    // Only retain MultiplicationCoeffTrunc*max(N,M) coefficients for faster (approximate) Chebyshev multiplication.
    //   If +inf, then use regular (full) multiplication.
    double MultiplicationCoeffTrunc = std::numeric_limits<double>::infinity();

};


BOOST_CLASS_VERSION(KineticModel_1Compartment2Input_5Param_Chebyshev_Parameters, 1)

namespace boost {
namespace serialization {

template<typename Archive>
void serialize(Archive &a, 
               KineticModel_1Compartment2Input_5Param_Chebyshev_Parameters &p, 
               const unsigned int version ){
    a & boost::serialization::make_nvp("cAIF",  p.cAIF)
      & boost::serialization::make_nvp("dcAIF", p.dcAIF)

      & boost::serialization::make_nvp("cVIF",  p.cVIF)
      & boost::serialization::make_nvp("dcVIF", p.dcVIF)

      & boost::serialization::make_nvp("cROI",  p.cROI)

      & boost::serialization::make_nvp("FittingPerformed", p.FittingPerformed)
      & boost::serialization::make_nvp("FittingSuccess",   p.FittingSuccess)

      & boost::serialization::make_nvp("RSS",   p.RSS)

      & boost::serialization::make_nvp("k1A",   p.k1A)
      & boost::serialization::make_nvp("tauA",  p.tauA)
      & boost::serialization::make_nvp("k1V",   p.k1V)
      & boost::serialization::make_nvp("tauV",  p.tauV)
      & boost::serialization::make_nvp("k2",    p.k2);

    if(version >= 1){
        a & boost::serialization::make_nvp("ExpApproxTrunc", p.ExpApproxTrunc)
          & boost::serialization::make_nvp("MultiplicationCoeffTrunc", p.MultiplicationCoeffTrunc);
    }

    return;
}

}
}


//This struct is returned when evaluating the model. Jacobian matrix elements are also returned to give some indication
// of the objective function topology at the given time with the optimized parameter values.
struct KineticModel_1Compartment2Input_5Param_Chebyshev_Results {

    // Evaluated model value.
    double I = std::numeric_limits<double>::quiet_NaN();

    // Model gradients along the parameter axes. (Note: model parameter gradients = Jacobian matrix elements, *not*
    // objective function gradients.)
    double d_I_d_k1A  = std::numeric_limits<double>::quiet_NaN();
    double d_I_d_tauA = std::numeric_limits<double>::quiet_NaN();
    double d_I_d_k1V  = std::numeric_limits<double>::quiet_NaN();
    double d_I_d_tauV = std::numeric_limits<double>::quiet_NaN();
    double d_I_d_k2   = std::numeric_limits<double>::quiet_NaN();

}; 


//Means for evaluating the model at a given time with the supplied parameters.
void
Evaluate_Model( const KineticModel_1Compartment2Input_5Param_Chebyshev_Parameters &state,
                const double t,
                KineticModel_1Compartment2Input_5Param_Chebyshev_Results &res);
 
