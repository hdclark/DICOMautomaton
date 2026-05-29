//KineticModel_1Compartment2Input_Reduced3Param_Chebyshev_FreeformOptimization.h.

#pragma once


#include "KineticModel_1Compartment2Input_Reduced3Param_Chebyshev_Common.h"


// This routine fits a pharmacokinetic model to the observed liver perfusion data using a Chebyshev polynomial
// approximation scheme.
//
// The 'dimensionalty reduction' approach is used, so while this routine estimates all 5 model parameters (k1A, tauA,
// k1V, tauV, k2), only (tauA, tauV, k2) are actually fitted numerically. Estimates for (k1A, k1V) are derived from the
// fitted (tauA, tauV, k2) using a scheme that maximizes the objective function.
//
struct KineticModel_1Compartment2Input_Reduced3Param_Chebyshev_Parameters
Optimize_FreeformOptimization_Reduced3Param(KineticModel_1Compartment2Input_Reduced3Param_Chebyshev_Parameters state);

