//Pharmacokinetic_Modeling_via_Least_Squares.h.

#pragma once

#include <limits>
#include <memory>

//#include "YgorMisc.h"
#include "YgorMath.h"
#include "YgorMathChebyshev.h"
//#include "YgorMathChebyshevFunctions.h"

#include "YgorMathIOBoostSerialization.h"
#include "YgorMathChebyshevIOBoostSerialization.h"

#include <boost/serialization/nvp.hpp>

//#include <boost/serialization/string.hpp>
//#include <boost/serialization/array.hpp>
//#include <boost/serialization/list.hpp>
//#include <boost/serialization/vector.hpp>
//#include <boost/serialization/map.hpp>
#include <boost/serialization/shared_ptr.hpp>



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
struct Pharmacokinetic_Parameters_5Param_Chebyshev_Least_Squares {
 
    // Experimental observations.
    std::shared_ptr<cheby_approx<double>> cAIF;
    std::shared_ptr<cheby_approx<double>> dcAIF;

    std::shared_ptr<cheby_approx<double>> cVIF;
    std::shared_ptr<cheby_approx<double>> dcVIF;

    std::shared_ptr<samples_1D<double>> cROI;


    // Indicators for various things.
    bool FittingPerformed = false;
    bool FittingSuccess = false;

    // Fitting quantities.
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

//Struct: Pharmacokinetic_Parameters_5Param_Chebyshev_Least_Squares.
template<typename Archive>
void serialize(Archive &a, Pharmacokinetic_Parameters_5Param_Chebyshev_Least_Squares &p, const unsigned int /*version*/){
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
    return;
}

}
}




//This struct is only needed if you want to evaluate the gradients of the model at
// some specific time.
struct Pharmacokinetic_Parameters_5Param_Chebyshev_Least_Squares_Results {

    // Evaluated model value.
    double I = std::numeric_limits<double>::quiet_NaN();

    // Model gradients along the parameter axes. (Note: model gradients, 
    // *not* objective function gradients.)
    double d_I_d_k1A  = std::numeric_limits<double>::quiet_NaN();
    double d_I_d_tauA = std::numeric_limits<double>::quiet_NaN();
    double d_I_d_k1V  = std::numeric_limits<double>::quiet_NaN();
    double d_I_d_tauV = std::numeric_limits<double>::quiet_NaN();
    double d_I_d_k2   = std::numeric_limits<double>::quiet_NaN();

}; 


// This routine can be used to evaluate a model (using the parameters in 'state') at given time.
void
chebyshev_5param_model_least_squares( const Pharmacokinetic_Parameters_5Param_Chebyshev_Least_Squares &state,
                        const double t,
                        Pharmacokinetic_Parameters_5Param_Chebyshev_Least_Squares_Results &res);
 

// This routine fits a pharmacokinetic model to the observed liver perfusion data using a 
// Chebyshev polynomial approximation scheme.
//
// This routine fits all 5 model free parameters (k1A, tauA, k1V, tauV, k2) numerically.
//
struct Pharmacokinetic_Parameters_5Param_Chebyshev_Least_Squares
Pharmacokinetic_Model_5Param_Chebyshev_Least_Squares(Pharmacokinetic_Parameters_5Param_Chebyshev_Least_Squares state);


// This routine fits a pharmacokinetic model to the observed liver perfusion data using a 
// Chebyshev polynomial approximation scheme.
//
// This routine fits only 3 model free parameters (k1A, k1V, k2) numerically. The neglected
// parameters (tauA, tauV) are kept at 0.0.
//
struct Pharmacokinetic_Parameters_5Param_Chebyshev_Least_Squares
Pharmacokinetic_Model_3Param_Chebyshev_Least_Squares(Pharmacokinetic_Parameters_5Param_Chebyshev_Least_Squares state);


