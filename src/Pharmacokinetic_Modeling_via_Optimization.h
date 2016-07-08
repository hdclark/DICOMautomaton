//Pharmacokinetic_Modeling_via_Optimization.h.

#pragma once

#include <limits>
#include <memory>

//#include "YgorMisc.h"
#include <YgorMath.h>
#include "YgorMath.h"
#include "YgorMathChebyshev.h"
//#include "YgorMathChebyshevFunctions.h"


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
struct Pharmacokinetic_Parameters_5Param_Chebyshev_Optimization {
 
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

//This struct is only needed if you want to evaluate the gradients of the model at
// some specific time.
struct Pharmacokinetic_Parameters_5Param_Chebyshev_Optimization_Results {

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
chebyshev_5param_model_optimization( const Pharmacokinetic_Parameters_5Param_Chebyshev_Optimization &state,
                        const double t,
                        Pharmacokinetic_Parameters_5Param_Chebyshev_Optimization_Results &res);
 

// This routine fits a pharmacokinetic model to the observed liver perfusion data using a 
// Chebyshev polynomial approximation scheme.
//
// This routine fits all 5 model free parameters (k1A, tauA, k1V, tauV, k2) numerically.
//
struct Pharmacokinetic_Parameters_5Param_Chebyshev_Optimization
Pharmacokinetic_Model_5Param_Chebyshev_Optimization(Pharmacokinetic_Parameters_5Param_Chebyshev_Optimization state);


// This routine fits a pharmacokinetic model to the observed liver perfusion data using a 
// Chebyshev polynomial approximation scheme.
//
// This routine fits only 3 model free parameters (k1A, k1V, k2) numerically. The neglected
// parameters (tauA, tauV) are kept at 0.0.
//
struct Pharmacokinetic_Parameters_5Param_Chebyshev_Optimization
Pharmacokinetic_Model_3Param_Chebyshev_Optimization(Pharmacokinetic_Parameters_5Param_Chebyshev_Optimization state);


