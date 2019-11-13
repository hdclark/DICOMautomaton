//KineticModel_1Compartment2Input_Reduced3Param_Chebyshev_FreeformOptimization.cc.
// It is tied to the L2-norm, but uses some simplifications to speed up evaluation of the gradient. These
// simplifications are most potent when the optimizer does not specifically assume a least-squares form. (Parts of the
// objective function gradient -- i.e., the Jacobian of the objective function $F$ -- cancel out, but wouldn't if a
// Jacobian of the *model*  -- $I$ -- was used.)

#ifdef DCMA_USE_GNU_GSL
#else
    #error "Attempting to compile this operation without GNU GSL, which is required."
#endif

#include <gsl/gsl_errno.h>
#include <gsl/gsl_multimin.h>
#include <gsl/gsl_vector_double.h>
#include <stddef.h>
#include <array>
#include <cmath>
#include <exception>
#include <limits>
#include <memory>
#include <tuple>
#include <vector>

#include "KineticModel_1Compartment2Input_Reduced3Param_Chebyshev_Common.h"
#include "KineticModel_1Compartment2Input_Reduced3Param_Chebyshev_FreeformOptimization.h"
#include "YgorMath.h"
#include "YgorMathChebyshev.h"
#include "YgorMathChebyshevFunctions.h"
#include "YgorStats.h"       //Needed for Stats:: namespace.


static
void
ComputeIntegralSummations(KineticModel_1Compartment2Input_Reduced3Param_Chebyshev_Parameters *state,
                          bool ComputeGradientToo){
    //This routine uses that {tauA, tauV, and k2} specified in the state struct to compute six integral summation
    // quantities. They are also used to compute $F$ (= the RSS) and optimal estimates for k1A and k1V, and the state is
    // updated with these quantities.

    const auto IndicateFailure = [&](void) -> void {
            //This closure attempts to signal that the current parameters are no good.
            state->RSS = std::numeric_limits<double>::infinity();
            state->k1A = std::numeric_limits<double>::quiet_NaN();
            state->k1V = std::numeric_limits<double>::quiet_NaN();
            if(ComputeGradientToo){
                state->dF_dtauA = 0.0;
                state->dF_dtauV = 0.0;
                state->dF_dk2   = 0.0;
            }
            return;
    };

    const double tauA = state->tauA;
    const double tauV = state->tauV;
    const double k2   = state->k2;

    if( !std::isfinite(tauA) || !std::isfinite(tauV) || !std::isfinite(k2) ){
        IndicateFailure();
        return;
    }
   
    //if( !isininc(-20.0, tauA, 20.0) || !isininc(-20.0, tauV, 20.0) || !isininc(0.0, k2, 1.0) ){
    //    IndicateFailure();
    //    return;
    //}

    // For evaluating the objective function $F$.
    state->S_IA_IV = 0.0;
    state->S_IA_R  = 0.0;
    state->S_IV_R  = 0.0;
    state->S_IA_IA = 0.0;
    state->S_IV_IV = 0.0;
    state->S_R_R   = 0.0;

    // For evaluating the gradient of $F$.
    state->S_R_dtauA_IA  = 0.0;
    state->S_IA_dtauA_IA = 0.0;
    state->S_IV_dtauA_IA = 0.0;

    state->S_R_dtauV_IV  = 0.0;
    state->S_IV_dtauV_IV = 0.0;
    state->S_IA_dtauV_IV = 0.0;

    state->S_R_dk2_IA    = 0.0;
    state->S_R_dk2_IV    = 0.0;
    state->S_IA_dk2_IA   = 0.0;
    state->S_IV_dk2_IV   = 0.0;
    state->S_IA_dk2_IV   = 0.0;
    state->S_IV_dk2_IA   = 0.0;

    const double AIF_at_neg_tauA = state->cAIF->Sample(-tauA);
    const double VIF_at_neg_tauV = state->cVIF->Sample(-tauV);

    const size_t exp_approx_N = 10; // 3 usually works (roughly). 5 is probably OK. 10 should suffice. 
                                    // 20 could be overkill. Depends on params, though.
    double exp_A_min, exp_A_max;
    double exp_V_min, exp_V_max;
    std::tie(exp_A_min,exp_A_max) = state->cAIF->Get_Domain();
    std::tie(exp_V_min,exp_V_max) = state->cVIF->Get_Domain();

    for(const auto &R : state->cROI->samples){
        const double ti = R[0];
        const double Ri = R[2];

        double IA       = std::numeric_limits<double>::quiet_NaN();
        double dtauA_IA = std::numeric_limits<double>::quiet_NaN(); //Partial derivative w.r.t. tauA.
        double dk2_IA   = std::numeric_limits<double>::quiet_NaN(); //Partial derivative w.r.t. k2.

        //AIF integral.
        try{   
            const double A = k2;
            const double B = k2 * (tauA - ti);
            const double C = 1.0; 
            const double taumin = -tauA;
            const double taumax = ti - tauA;
            
            const cheby_approx<double> exp_kern = Chebyshev_Basis_Approx_Exp_Analytic1(exp_approx_N,exp_A_min,exp_A_max, A,B,C);
            const cheby_approx<double> integrand = exp_kern * (*(state->cAIF));
            const cheby_approx<double> integral = integrand.Chebyshev_Integral();

            IA = (integral.Sample(taumax) - integral.Sample(taumin));

            if(ComputeGradientToo){
                dtauA_IA = AIF_at_neg_tauA * std::exp(k2 * ti) - state->cAIF->Sample(ti - tauA);

                const cheby_approx<double> t_integrand = integrand * Chebyshev_Basis_Exact_Linear(exp_A_min,exp_A_max,1.0,0.0);
                const cheby_approx<double> t_integral = t_integrand.Chebyshev_Integral();

                dk2_IA = -ti*IA + (t_integral.Sample(taumax) - t_integral.Sample(taumin));
            }
        }catch(const std::exception &e){
            IndicateFailure();
            return;
        }

        double IV       = std::numeric_limits<double>::quiet_NaN();
        double dtauV_IV = std::numeric_limits<double>::quiet_NaN(); //Partial derivative w.r.t. tauV.
        double dk2_IV   = std::numeric_limits<double>::quiet_NaN(); //Partial derivative w.r.t. k2.


        //VIF integral.
        try{   
            const double A = k2;
            const double B = k2 * (tauV - ti);
            const double C = 1.0; 
            const double taumin = -tauV;
            const double taumax = ti - tauV;
            
            const cheby_approx<double> exp_kern = Chebyshev_Basis_Approx_Exp_Analytic1(exp_approx_N,exp_V_min,exp_V_max, A,B,C);
            const cheby_approx<double> integrand = exp_kern * (*(state->cVIF));
            const cheby_approx<double> integral = integrand.Chebyshev_Integral();

            IV = (integral.Sample(taumax) - integral.Sample(taumin));

            if(ComputeGradientToo){
                dtauV_IV = VIF_at_neg_tauV * std::exp(k2 * ti) - state->cVIF->Sample(ti - tauV);

                const cheby_approx<double> t_integrand = integrand * Chebyshev_Basis_Exact_Linear(exp_V_min,exp_V_max,1.0,0.0);
                const cheby_approx<double> t_integral = t_integrand.Chebyshev_Integral();

                dk2_IV = -ti*IV + (t_integral.Sample(taumax) - t_integral.Sample(taumin));
            }
        }catch(const std::exception &e){
            IndicateFailure();
            return;
        }

        //Add to the summations.
        state->S_IA_IV +=  IA*IV;
        state->S_IA_R +=   IA*Ri;
        state->S_IV_R +=   IV*Ri;
        state->S_IA_IA +=  IA*IA;
        state->S_IV_IV +=  IV*IV;
        state->S_R_R +=    Ri*Ri;

        if(ComputeGradientToo){
            state->S_R_dtauA_IA +=   Ri * dtauA_IA;
            state->S_IA_dtauA_IA +=  IA * dtauA_IA;
            state->S_IV_dtauA_IA +=  IV * dtauA_IA;

            state->S_R_dtauV_IV +=   Ri * dtauV_IV;
            state->S_IV_dtauV_IV +=  IV * dtauV_IV;
            state->S_IA_dtauV_IV +=  IA * dtauV_IV;

            state->S_R_dk2_IA +=   Ri * dk2_IA;
            state->S_R_dk2_IV +=   Ri * dk2_IV;
            state->S_IA_dk2_IA +=  IA * dk2_IA;
            state->S_IV_dk2_IV +=  IV * dk2_IV;
            state->S_IA_dk2_IV +=  IA * dk2_IV;
            state->S_IV_dk2_IA +=  IV * dk2_IA;
        }
    }        

    const double common_den = Stats::Sum(std::vector<double>({ (state->S_IA_IV)*(state->S_IA_IV), 
                                                               -(state->S_IA_IA)*(state->S_IV_IV) }));
    const double k1A_num = Stats::Sum(std::vector<double>({ (state->S_IA_IV)*(state->S_IV_R), 
                                                            -(state->S_IA_R)*(state->S_IV_IV) }));
    const double k1V_num = Stats::Sum(std::vector<double>({ (state->S_IA_IV)*(state->S_IA_R), 
                                                            -(state->S_IV_R)*(state->S_IA_IA) }));
    state->k1A = k1A_num / common_den;
    state->k1V = k1V_num / common_den;

    if(!std::isfinite(state->k1A) || !std::isfinite(state->k1V)){
        IndicateFailure();
        return;
    }

    const double F = Stats::Sum(std::vector<double>({ 
                                  (state->S_R_R),
                                  (state->k1A)*(state->k1A)*(state->S_IA_IA),
                                  (state->k1V)*(state->k1V)*(state->S_IV_IV),
                                  2.0*(state->k1A)*(state->k1V)*(state->S_IA_IV),
                                  -2.0*(state->k1A)*(state->S_IA_R),
                                  -2.0*(state->k1V)*(state->S_IV_R) }));
    if(ComputeGradientToo){

        state->dF_dtauA = 2.0 * Stats::Sum( std::vector<double>({
                                  -(state->k1A)*(state->S_R_dtauA_IA),
                                  (state->k1A)*(state->k1A)*(state->S_IA_dtauA_IA),
                                  (state->k1V)*(state->k1A)*(state->S_IV_dtauA_IA) }) );

        state->dF_dtauV = 2.0 * Stats::Sum( std::vector<double>({
                                  -(state->k1V)*(state->S_R_dtauV_IV),
                                  (state->k1V)*(state->k1V)*(state->S_IV_dtauV_IV),
                                  (state->k1A)*(state->k1V)*(state->S_IA_dtauV_IV) }) );

        state->dF_dk2   = 2.0 * Stats::Sum( std::vector<double>({
                                  -(state->k1A)*(state->S_R_dk2_IA),
                                  -(state->k1V)*(state->S_R_dk2_IV),
                                  (state->k1A)*(state->k1A)*(state->S_IA_dk2_IA),
                                  (state->k1V)*(state->k1V)*(state->S_IV_dk2_IV),
                                  (state->k1A)*(state->k1V)*(state->S_IA_dk2_IV),
                                  (state->k1V)*(state->k1A)*(state->S_IV_dk2_IA) }) );


    }

    state->RSS = F;

//FUNCINFO("Evaluating at {tauA, tauV, k2} = {" << tauA << ", " << tauV << ", " << k2 << "} gives {k1A, k1V} = {" << state->k1A << ", " << state->k1V << "} and F = " << F);

//FUNCINFO("{dF_dtauA, dF_dtauV, dF_dk2} = " << state->dF_dtauA << ", " << state->dF_dtauV << ", " << state->dF_dk2);

    return;
}



static
double
F_only_Reduced3Param(const gsl_vector *model_params, void *voided_state){
    //This function computes the residual-sum-of-squares between the ROI time course and a kinetic liver
    // perfusion model at the ROI sample t_i's. The gradient is NOT computed.

    auto state = reinterpret_cast<KineticModel_1Compartment2Input_Reduced3Param_Chebyshev_Parameters*>(voided_state);

    state->tauA = gsl_vector_get(model_params, 0);
    state->tauV = gsl_vector_get(model_params, 1);
    state->k2   = gsl_vector_get(model_params, 2);
  
    const bool ComputeGradientToo = false;
    ComputeIntegralSummations(state, ComputeGradientToo);

    return state->RSS;
}

static
void 
dF_only_Reduced3Param(const gsl_vector *model_params, void *voided_state, gsl_vector *dF){
    //This function computes the residual-sum-of-squares between the ROI time course and a kinetic liver
    // perfusion model at the ROI sample t_i's. The gradient is also computed (it requires F to be computed).

    auto state = reinterpret_cast<KineticModel_1Compartment2Input_Reduced3Param_Chebyshev_Parameters*>(voided_state);

    state->tauA = gsl_vector_get(model_params, 0);
    state->tauV = gsl_vector_get(model_params, 1);
    state->k2   = gsl_vector_get(model_params, 2);
  
    const bool ComputeGradientToo = true;
    ComputeIntegralSummations(state, ComputeGradientToo);

    gsl_vector_set(dF, 0, state->dF_dtauA);
    gsl_vector_set(dF, 1, state->dF_dtauV);
    gsl_vector_set(dF, 2, state->dF_dk2);

    return;
}

static
void 
F_and_dF_Reduced3Param(const gsl_vector *model_params, void *voided_state, double *F, gsl_vector *dF){
    //This function computes the residual-sum-of-squares between the ROI time course and a kinetic liver
    // perfusion model at the ROI sample t_i's. The gradient is also computed.

    auto state = reinterpret_cast<KineticModel_1Compartment2Input_Reduced3Param_Chebyshev_Parameters*>(voided_state);

    state->tauA = gsl_vector_get(model_params, 0);
    state->tauV = gsl_vector_get(model_params, 1);
    state->k2   = gsl_vector_get(model_params, 2);
  
    const bool ComputeGradientToo = true;
    ComputeIntegralSummations(state, ComputeGradientToo);

    *F = state->RSS;
    gsl_vector_set(dF, 0, state->dF_dtauA);
    gsl_vector_set(dF, 1, state->dF_dtauV);
    gsl_vector_set(dF, 2, state->dF_dk2);

    return;
}


struct KineticModel_1Compartment2Input_Reduced3Param_Chebyshev_Parameters
Optimize_FreeformOptimization_Reduced3Param(KineticModel_1Compartment2Input_Reduced3Param_Chebyshev_Parameters state){

    state.FittingPerformed = false;
    state.FittingSuccess = false;

    const int dimen = 3;

    int status;

    //Fitting parameters: tauA, tauV, k2.
    // The following are arbitrarily chosen. They should be seeded from previous computations, or
    // at least be nominal values reported within the literature.
    gsl_vector *model_params = gsl_vector_alloc(dimen);


    //First-pass fit.
    {

        //If there were finite parameters provided, use them as the initial guesses.
        gsl_vector_set(model_params, 0, std::isfinite(state.tauA) ? state.tauA : 0.0000);
        gsl_vector_set(model_params, 1, std::isfinite(state.tauV) ? state.tauV : 0.0000);
        gsl_vector_set(model_params, 2, std::isfinite(state.k2)   ? state.k2   : 0.0518);

        const gsl_multimin_fdfminimizer_type *minimizer_t;
        gsl_multimin_fdfminimizer *minimizer;

        gsl_multimin_function_fdf func_wrapper;

        func_wrapper.n = dimen;

        func_wrapper.f = F_only_Reduced3Param;
        func_wrapper.df = dF_only_Reduced3Param;
        func_wrapper.fdf = F_and_dF_Reduced3Param;

        func_wrapper.params = reinterpret_cast<void*>(&state); // User parameters.

        //minimizer_t = gsl_multimin_fdfminimizer_conjugate_fr;
        minimizer_t = gsl_multimin_fdfminimizer_vector_bfgs2;
        minimizer = gsl_multimin_fdfminimizer_alloc(minimizer_t, dimen);

        const double alg_knob_step_size = 0.1; //Meaning depends on the algorithm being used.
        const double dFtol = 1.0E-4; //When F changes < this on successive iters, then exit.
        const double ddFtol = 1.0E-4; //When dF (the gradient) changes < this on successive iters, then exit.
        gsl_multimin_fdfminimizer_set(minimizer, &func_wrapper, model_params, alg_knob_step_size, dFtol);

        size_t iter = 0;
        do{
            iter++;
            status = gsl_multimin_fdfminimizer_iterate(minimizer);
            if(status != 0) break;
            status = gsl_multimin_test_gradient(minimizer->gradient, ddFtol);
        }while( (status == GSL_CONTINUE) && (iter < 500));

        state.tauA = gsl_vector_get(minimizer->x, 0);
        state.tauV = gsl_vector_get(minimizer->x, 1);
        state.k2   = gsl_vector_get(minimizer->x, 2);

        gsl_multimin_fdfminimizer_free(minimizer);
    }

    //Second-pass fit.
    if(false){
        gsl_vector_set(model_params, 0, std::isfinite(state.tauA) ? state.tauA : 0.0000);
        gsl_vector_set(model_params, 1, std::isfinite(state.tauV) ? state.tauV : 0.0000);
        gsl_vector_set(model_params, 2, std::isfinite(state.k2)   ? state.k2   : 0.0518);

        const gsl_multimin_fdfminimizer_type *minimizer_t;
        gsl_multimin_fdfminimizer *minimizer;

        gsl_multimin_function_fdf func_wrapper;

        func_wrapper.n = dimen;

        func_wrapper.f = F_only_Reduced3Param;
        func_wrapper.df = dF_only_Reduced3Param;
        func_wrapper.fdf = F_and_dF_Reduced3Param;

        func_wrapper.params = reinterpret_cast<void*>(&state); // User parameters.

        minimizer_t = gsl_multimin_fdfminimizer_conjugate_fr;
        //minimizer_t = gsl_multimin_fdfminimizer_vector_bfgs2;
        minimizer = gsl_multimin_fdfminimizer_alloc(minimizer_t, dimen);

        const double alg_knob_step_size = 0.1; //Meaning depends on the algorithm being used.
        const double dFtol = 1.0E-1; //When F changes < this on successive iters, then exit.
        const double ddFtol = 1.0E-1; //When dF (the gradient) changes < this on successive iters, then exit.
        gsl_multimin_fdfminimizer_set(minimizer, &func_wrapper, model_params, alg_knob_step_size, dFtol);

        size_t iter = 0;
        do{
            iter++;
            status = gsl_multimin_fdfminimizer_iterate(minimizer);
            if(status != 0) break;
            status = gsl_multimin_test_gradient(minimizer->gradient, ddFtol);
        }while( (status == GSL_CONTINUE) && (iter < 1000));

        state.tauA = gsl_vector_get(minimizer->x, 0);
        state.tauV = gsl_vector_get(minimizer->x, 1);
        state.k2   = gsl_vector_get(minimizer->x, 2);

        gsl_multimin_fdfminimizer_free(minimizer);
    }

    gsl_vector_free(model_params);

    //Compute k1A, k1V, and RSS(==F) with the best tauA, tauV, and k2.
    {
        const bool ComputeGradientToo = false;
        ComputeIntegralSummations(&state, ComputeGradientToo);
    }

    state.FittingPerformed = true;
    state.FittingSuccess = (status == GSL_SUCCESS);

    return state;
}

