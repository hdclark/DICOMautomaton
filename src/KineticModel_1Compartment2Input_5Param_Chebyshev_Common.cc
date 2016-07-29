//KineticModel_1Compartment2Input_5Param_Chebyshev_Common.cc.

#include <list>
#include <functional>
#include <limits>
#include <cmath>

#include "YgorMisc.h"
#include "YgorMath.h"
#include "YgorMathChebyshev.h"
#include "YgorMathChebyshevFunctions.h"
#include "YgorStats.h"       //Needed for Stats:: namespace.

#include "KineticModel_1Compartment2Input_5Param_Chebyshev_Common.h"

void
Evaluate_Model( const KineticModel_1Compartment2Input_5Param_Chebyshev_Parameters &state,
                const double t,
                KineticModel_1Compartment2Input_5Param_Chebyshev_Results &res){
                
    // Chebyshev polynomial approximation method. 
    // 
    // This function computes the predicted contrast enhancement of a kinetic
    // liver perfusion model at the ROI sample t_i's. Gradients are able to be
    // computed using this method, so they are also computed.

    const double k1A  = state.k1A;
    const double tauA = state.tauA;
    const double k1V  = state.k1V;
    const double tauV = state.tauV;
    const double k2   = state.k2;

    const size_t exp_approx_N = state.ExpApproxTrunc;
    const double mult_trunc = state.MultiplicationCoeffTrunc;

    double int_AIF_exp      = std::numeric_limits<double>::quiet_NaN();
    double int_VIF_exp      = std::numeric_limits<double>::quiet_NaN();
    double int_AIF_exp_tau  = std::numeric_limits<double>::quiet_NaN();
    double int_VIF_exp_tau  = std::numeric_limits<double>::quiet_NaN();
    double int_dAIF_exp     = std::numeric_limits<double>::quiet_NaN();
    double int_dVIF_exp     = std::numeric_limits<double>::quiet_NaN();

    //AIF integral(s).
    {
        const double A = k2;
        const double B = k2 * (tauA - t);
        const double C = 1.0;
        const double taumin = -tauA;
        const double taumax = t - tauA;
        double expmin, expmax;
        std::tie(expmin,expmax) = state.cAIF->Get_Domain();
    
        cheby_approx<double> exp_kern = Chebyshev_Basis_Approx_Exp_Analytic1(exp_approx_N,expmin,expmax, A,B,C);
        cheby_approx<double> integrand;
        cheby_approx<double> integral;

        //Evaluate the model.
        integrand = exp_kern.Fast_Approx_Multiply(*(state.cAIF),mult_trunc);
        integral = integrand.Chebyshev_Integral();
        int_AIF_exp = (integral.Sample(taumax) - integral.Sample(taumin));

        //Compute things for gradients.
        {
            //Evaluate $\partial_{k2}$ part of gradient.
            integrand = integrand.Fast_Approx_Multiply(Chebyshev_Basis_Exact_Linear(expmin,expmax,1.0,tauA-t),mult_trunc);
            integral = integrand.Chebyshev_Integral();
            int_AIF_exp_tau = (integral.Sample(taumax) - integral.Sample(taumin));

            //Evaluate $\partial_{tauA}$ part of gradient.
            integrand = exp_kern.Fast_Approx_Multiply(*(state.dcAIF),mult_trunc);
            integral = integrand.Chebyshev_Integral();
            int_dAIF_exp = (integral.Sample(taumax) - integral.Sample(taumin));
        }
    }
     
    //VIF integral(s).
    {
        const double A = k2;
        const double B = k2 * (tauV - t);
        const double C = 1.0;
        const double taumin = -tauV;
        const double taumax = t - tauV;
        double expmin, expmax;
        std::tie(expmin,expmax) = state.cVIF->Get_Domain();
 
        cheby_approx<double> exp_kern = Chebyshev_Basis_Approx_Exp_Analytic1(exp_approx_N,expmin,expmax, A,B,C);
        cheby_approx<double> integrand;
        cheby_approx<double> integral;

        //Evaluate the model.
        integrand = exp_kern.Fast_Approx_Multiply(*state.cVIF,mult_trunc);
        integral = integrand.Chebyshev_Integral();
        int_VIF_exp = (integral.Sample(taumax) - integral.Sample(taumin));

        //Compute things for gradients.
        {
            //Evaluate $\partial_{k2}$ part of gradient.
            integrand = integrand.Fast_Approx_Multiply(Chebyshev_Basis_Exact_Linear(expmin,expmax,1.0,tauV-t),mult_trunc);
            integral = integrand.Chebyshev_Integral();
            int_VIF_exp_tau = (integral.Sample(taumax) - integral.Sample(taumin));

            //Evaluate $\partial_{tauV}$ part of gradient.
            integrand = exp_kern.Fast_Approx_Multiply((*state.dcVIF),mult_trunc);
            integral = integrand.Chebyshev_Integral();
            int_dVIF_exp = (integral.Sample(taumax) - integral.Sample(taumin));
        }
    }

    //Evaluate the model's integral. This is the model's predicted contrast enhancement.
    res.I = (k1A * int_AIF_exp) + (k1V * int_VIF_exp);

    //Work out gradient information, if desired.
    {
        res.d_I_d_k1A  = ( int_AIF_exp );  // $\partial_{k1A}$
        res.d_I_d_tauA = ( -k1A * int_dAIF_exp ); // $\partial_{tauA}$
        res.d_I_d_k1V  = ( int_VIF_exp ); // $\partial_{k1V}$
        res.d_I_d_tauV = ( -k1V * int_dVIF_exp ); // $\partial_{tauV}$
        res.d_I_d_k2   = ( (k1A * int_AIF_exp_tau) + (k1V * int_VIF_exp_tau)  ); // $\partial_{k2}$
    }

    return;
}

