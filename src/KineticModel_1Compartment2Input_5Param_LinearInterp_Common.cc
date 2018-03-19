//KineticModel_1Compartment2Input_5Param_LinearInterp_Common.cc.

#include <array>

#include "KineticModel_1Compartment2Input_5Param_LinearInterp_Common.h"
#include "YgorMath.h"

void
Evaluate_Model( const KineticModel_1Compartment2Input_5Param_LinearInterp_Parameters &state,
                const double t,
                KineticModel_1Compartment2Input_5Param_LinearInterp_Results &res){
                
    // Direct linear interpolation approach.
    // 
    // This function computes the predicted contrast enhancement of a kinetic
    // liver perfusion model at the ROI sample t_i's. Gradients are not able to
    // be computed using this method because they are discontinuous.

    const double k1A  = state.k1A;
    const double tauA = state.tauA;
    const double k1V  = state.k1V;
    const double tauV = state.tauV;
    const double k2   = state.k2;


    //-------------------------------------------------------------------------------------------------------------------------
    //First, the arterial contribution. This involves an integral over the AIF.
    // Compute: \int_{tau=0}^{tau=t} k1A * AIF(tau - tauA) * exp((k2)*(tau-t)) dtau 
    //          = k1A \int_{tau=-tauA}^{tau=(t-tauA)} AIF(tau) * exp((k2)*(tau-(t-tauA))) dtau.
    // The integration coordinate is transformed to make it suit the integration-over-kernel-... implementation. 
    //
    //double Integratedk1ACA = k1A*state.cAIF->Integrate_Over_Kernel_exp(0.0-tauA, t-tauA, {k2,0.0}, {-(t-tauA),0.0})[0];
    const double int_AIF_exp = state.cAIF->Integrate_Over_Kernel_exp(0.0-tauA, t-tauA, {k2,0.0}, {-(t-tauA),0.0})[0];

    //-------------------------------------------------------------------------------------------------------------------------
    //The venous contribution is identical, but all the fitting parameters are different and AIF -> VIF.
    //
    //double Integratedk1VCV = k1V*state.cVIF->Integrate_Over_Kernel_exp(0.0-tauV, t-tauV, {k2,0.0}, {-(t-tauV),0.0})[0];
    const double int_VIF_exp = state.cVIF->Integrate_Over_Kernel_exp(0.0-tauV, t-tauV, {k2,0.0}, {-(t-tauV),0.0})[0];

    //Evaluate the model's integral. This is the model's predicted contrast enhancement.
    res.I = (k1A * int_AIF_exp) + (k1V * int_VIF_exp);

    return;
}

