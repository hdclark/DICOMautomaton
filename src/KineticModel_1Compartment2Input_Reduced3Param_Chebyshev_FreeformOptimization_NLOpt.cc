//KineticModel_1Compartment2Input_Reduced3Param_Chebyshev_FreeformOptimization.cc.
// It is tied to the L2-norm, but uses some simplifications to speed up evaluation of the gradient. These
// simplifications are most potent when the optimizer does not specifically assume a least-squares form. (Parts of the
// objective function gradient -- i.e., the Jacobian of the objective function $F$ -- cancel out, but wouldn't if a
// Jacobian of the *model*  -- $I$ -- was used.)

#include <list>
#include <functional>
#include <limits>
#include <cmath>

#ifdef DCMA_USE_NLOPT
#include <nlopt.h>
#endif // DCMA_USE_NLOPT

#include <boost/date_time/posix_time/posix_time.hpp>

#include "YgorMisc.h"
#include "YgorMath.h"
#include "YgorMathChebyshev.h"
#include "YgorMathChebyshevFunctions.h"
#include "YgorStats.h"       //Needed for Stats:: namespace.

#include "KineticModel_1Compartment2Input_Reduced3Param_Chebyshev_Common.h"
#include "KineticModel_1Compartment2Input_Reduced3Param_Chebyshev_FreeformOptimization.h"


static
void
ComputeIntegralSummations(KineticModel_1Compartment2Input_Reduced3Param_Chebyshev_Parameters *state,
                          bool ComputeGradientToo){
    //This routine uses that {tauA, tauV, and k2} specified in the state struct to compute six integral summation
    // quantities. They are also used to compute $F$ (= the RSS) and optimal estimates for k1A and k1V, and the state is
    // updated with these quantities.

    const double tauA = state->tauA;
    const double tauV = state->tauV;
    const double k2   = state->k2;

    const auto N = state->cROI->samples.size();

    // For evaluating the objective function $F$.
    std::vector<double> S_IA_IV;
    std::vector<double> S_IA_R;
    std::vector<double> S_IV_R;
    std::vector<double> S_IA_IA;
    std::vector<double> S_IV_IV;
    std::vector<double> S_R_R;

    S_IA_IV.reserve(N);
    S_IA_R.reserve(N);
    S_IV_R.reserve(N);
    S_IA_IA.reserve(N);
    S_IV_IV.reserve(N);
    S_R_R.reserve(N);

    // For evaluating the gradient of $F$.
    std::vector<double> S_R_dtauA_IA;
    //std::vector<double> S_R_dtauA_IV;  // = 0
    std::vector<double> S_IA_dtauA_IA;
    //std::vector<double> S_IV_dtauA_IV; // = 0
    //std::vector<double> S_IA_dtauA_IV; // = 0
    std::vector<double> S_IV_dtauA_IA;

    //std::vector<double> S_R_dtauV_IA; // = 0
    std::vector<double> S_R_dtauV_IV;
    //std::vector<double> S_IA_dtauV_IA; // = 0
    std::vector<double> S_IV_dtauV_IV;
    std::vector<double> S_IA_dtauV_IV;
    //std::vector<double> S_IV_dtauV_IA; // = 0

    std::vector<double> S_R_dk2_IA;
    std::vector<double> S_R_dk2_IV;
    std::vector<double> S_IA_dk2_IA;
    std::vector<double> S_IV_dk2_IV;
    std::vector<double> S_IA_dk2_IV;
    std::vector<double> S_IV_dk2_IA;

    if(ComputeGradientToo){
        S_R_dtauA_IA.reserve(N);
        S_IA_dtauA_IA.reserve(N);
        S_IV_dtauA_IA.reserve(N);

        S_R_dtauV_IV.reserve(N);
        S_IV_dtauV_IV.reserve(N);
        S_IA_dtauV_IV.reserve(N);

        S_R_dk2_IA.reserve(N);
        S_R_dk2_IV.reserve(N);
        S_IA_dk2_IA.reserve(N);
        S_IV_dk2_IV.reserve(N);
        S_IA_dk2_IV.reserve(N);
        S_IV_dk2_IA.reserve(N);
    }

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
        {   
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
        }

        double IV       = std::numeric_limits<double>::quiet_NaN();
        double dtauV_IV = std::numeric_limits<double>::quiet_NaN(); //Partial derivative w.r.t. tauV.
        double dk2_IV   = std::numeric_limits<double>::quiet_NaN(); //Partial derivative w.r.t. k2.


        //VIF integral.
        {   
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
        }

        //Add to the summations.
        S_IA_IV.push_back( IA*IV );
        S_IA_R.push_back(  IA*Ri );
        S_IV_R.push_back(  IV*Ri );
        S_IA_IA.push_back( IA*IA );
        S_IV_IV.push_back( IV*IV );
        S_R_R.push_back(   Ri*Ri );

        if(ComputeGradientToo){
            S_R_dtauA_IA.push_back(  Ri * dtauA_IA );
            S_IA_dtauA_IA.push_back( IA * dtauA_IA );
            S_IV_dtauA_IA.push_back( IV * dtauA_IA );

            S_R_dtauV_IV.push_back(  Ri * dtauV_IV );
            S_IV_dtauV_IV.push_back( IV * dtauV_IV );
            S_IA_dtauV_IV.push_back( IA * dtauV_IV );

            S_R_dk2_IA.push_back(  Ri * dk2_IA );
            S_R_dk2_IV.push_back(  Ri * dk2_IV );
            S_IA_dk2_IA.push_back( IA * dk2_IA );
            S_IV_dk2_IV.push_back( IV * dk2_IV );
            S_IA_dk2_IV.push_back( IA * dk2_IV );
            S_IV_dk2_IA.push_back( IV * dk2_IA );
        }
    }        

    state->S_IA_IV = Stats::Sum(std::move(S_IA_IV));
    state->S_IA_R  = Stats::Sum(std::move(S_IA_R));
    state->S_IV_R  = Stats::Sum(std::move(S_IV_R));
    state->S_IA_IA = Stats::Sum(std::move(S_IA_IA));
    state->S_IV_IV = Stats::Sum(std::move(S_IV_IV));
    state->S_R_R   = Stats::Sum(std::move(S_R_R));

    const double common_den = Stats::Sum(std::vector<double>({ (state->S_IA_IV)*(state->S_IA_IV), 
                                                               -(state->S_IA_IA)*(state->S_IV_IV) }));
    const double k1A_num = Stats::Sum(std::vector<double>({ (state->S_IA_IV)*(state->S_IV_R), 
                                                            -(state->S_IA_R)*(state->S_IV_IV) }));
    const double k1V_num = Stats::Sum(std::vector<double>({ (state->S_IA_IV)*(state->S_IA_R), 
                                                            -(state->S_IV_R)*(state->S_IA_IA) }));
    state->k1A = k1A_num / common_den;
    state->k1V = k1V_num / common_den;

    const double F = Stats::Sum(std::vector<double>({ 
                                  (state->S_R_R),
                                  (state->k1A)*(state->k1A)*(state->S_IA_IA),
                                  (state->k1V)*(state->k1V)*(state->S_IV_IV),
                                  2.0*(state->k1A)*(state->k1V)*(state->S_IA_IV),
                                  -2.0*(state->k1A)*(state->S_IA_R),
                                  -2.0*(state->k1V)*(state->S_IV_R) }));
    if(ComputeGradientToo){

        state->S_R_dtauA_IA  = Stats::Sum(S_R_dtauA_IA);
        state->S_IA_dtauA_IA = Stats::Sum(S_IA_dtauA_IA);
        state->S_IV_dtauA_IA = Stats::Sum(S_IV_dtauA_IA);

        state->S_R_dtauV_IV  = Stats::Sum(S_R_dtauV_IV);
        state->S_IV_dtauV_IV = Stats::Sum(S_IV_dtauV_IV);
        state->S_IA_dtauV_IV = Stats::Sum(S_IA_dtauV_IV);

        state->S_R_dk2_IA    = Stats::Sum(S_R_dk2_IA);
        state->S_R_dk2_IV    = Stats::Sum(S_R_dk2_IV);
        state->S_IA_dk2_IA   = Stats::Sum(S_IA_dk2_IA);
        state->S_IV_dk2_IV   = Stats::Sum(S_IV_dk2_IV);
        state->S_IA_dk2_IV   = Stats::Sum(S_IA_dk2_IV);
        state->S_IV_dk2_IA   = Stats::Sum(S_IV_dk2_IA);


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

FUNCINFO("{dF_dtauA, dF_dtauV, dF_dk2} = " << state->dF_dtauA << ", " << state->dF_dtauV << ", " << state->dF_dk2);

    return;
}




static
double 
MinimizationFunction_Reduced3Param(unsigned, const double *params, double *grad, void *voided_state){

    auto state = reinterpret_cast<KineticModel_1Compartment2Input_Reduced3Param_Chebyshev_Parameters*>(voided_state);

    //This function computes the residual-sum-of-squares between the ROI time course and a kinetic liver
    // perfusion model at the ROI sample t_i's. If gradients are requested, they are also computed.

    //Pack the current parameters into the state struct. This is done to unify the nlopt and user calling styles.
    state->tauA = params[0];
    state->tauV = params[1]; 
    state->k2   = params[2];

    const bool ComputeGradientToo = (grad != nullptr);
    ComputeIntegralSummations(state, ComputeGradientToo);

    if(grad != nullptr){
        grad[0] = state->dF_dtauA;
        grad[1] = state->dF_dtauV;
        grad[2] = state->dF_dk2;
    }

    return state->RSS; 
}

struct KineticModel_1Compartment2Input_Reduced3Param_Chebyshev_Parameters
Optimize_FreeformOptimization_Reduced3Param(KineticModel_1Compartment2Input_Reduced3Param_Chebyshev_Parameters state){

    state.FittingPerformed = false;
    state.FittingSuccess = false;

    const int dimen = 3;
    double func_min;

    //Fitting parameters: tauA, tauV, k2.
    // The following are arbitrarily chosen. They should be seeded from previous computations, or
    // at least be nominal values reported within the literature.
    double params[dimen];

    //If there were finite parameters provided, use them as the initial guesses.
    params[0] = std::isfinite(state.tauA) ? state.tauA : 0.0000;
    params[1] = std::isfinite(state.tauV) ? state.tauV : 0.0000;
    params[2] = std::isfinite(state.k2)   ? state.k2   : 0.0518;

    // U/L bounds:            tauA,  tauV,  k2.
    double l_bnds[dimen] = {  -20.0, -20.0, 0.0 };
    double u_bnds[dimen] = {   20.0,  20.0, 1.0 };
    // NOTE: If tmax ~= 150, and you permit exp(k2*tmax) to be <= 10^66, then k2 <= 1.
    //       So k2 = 1 seems like a reasonable limit to help prevent overflow. 
                    
    //Initial step sizes:       tauA,   tauV,   k2.
    double initstpsz[dimen] = { 3.2000, 3.2000, 0.0050 };

    //Absolute param. change thresholds:  tauA,  tauV,  k2.
    //double xtol_abs_thresholds[dimen] = { 1E-20, 1E-20, 1E-10 };
    double xtol_abs_thresholds[dimen] = { std::numeric_limits<double>::denorm_min(),
                                          std::numeric_limits<double>::denorm_min(),
                                          std::numeric_limits<double>::denorm_min() };

#ifdef DCMA_USE_NLOPT

    //First-pass fit.
    {
        nlopt_opt opt; //See `man nlopt` to get list of available algorithms.
        //opt = nlopt_create(NLOPT_LN_COBYLA, dimen);   //Local, no-derivative schemes.
        //opt = nlopt_create(NLOPT_LN_BOBYQA, dimen);
        //opt = nlopt_create(NLOPT_LN_SBPLX, dimen);

        opt = nlopt_create(NLOPT_LD_MMA, dimen);   //Local, derivative schemes.
        //opt = nlopt_create(NLOPT_LD_SLSQP, dimen);
        //opt = nlopt_create(NLOPT_LD_LBFGS, dimen);
        //opt = nlopt_create(NLOPT_LD_VAR2, dimen);
        //opt = nlopt_create(NLOPT_LD_TNEWTON_PRECOND_RESTART, dimen);
        //opt = nlopt_create(NLOPT_LD_TNEWTON_PRECOND, dimen);
        //opt = nlopt_create(NLOPT_LD_TNEWTON, dimen);

        //opt = nlopt_create(NLOPT_GN_DIRECT, dimen); //Global, no-derivative schemes.
        //opt = nlopt_create(NLOPT_GN_CRS2_LM, dimen);
        //opt = nlopt_create(NLOPT_GN_ESCH, dimen);
        //opt = nlopt_create(NLOPT_GN_ISRES, dimen);
                        
        nlopt_set_lower_bounds(opt, l_bnds);
        nlopt_set_upper_bounds(opt, u_bnds);
                        
        if(NLOPT_SUCCESS != nlopt_set_initial_step(opt, initstpsz)){
            FUNCERR("NLOpt unable to set initial step sizes");
        }
        if(NLOPT_SUCCESS != nlopt_set_min_objective(opt, MinimizationFunction_Reduced3Param, reinterpret_cast<void*>(&state))){
            FUNCERR("NLOpt unable to set objective function for minimization");
        }
        //if(NLOPT_SUCCESS != nlopt_set_xtol_rel(opt, 1.0E-3)){
        //    FUNCERR("NLOpt unable to set xtol stopping condition");
        //}
//        if(NLOPT_SUCCESS != nlopt_set_xtol_abs(opt, xtol_abs_thresholds)){
//            FUNCERR("NLOpt unable to set xtol_abs stopping condition");
//        }
//        if(NLOPT_SUCCESS != nlopt_set_ftol_rel(opt, 1.0E-99)){
//            FUNCERR("NLOpt unable to set ftol_rel stopping condition");
//        }
//        if(NLOPT_SUCCESS != nlopt_set_maxtime(opt, 30.0)){ // In seconds.
        if(NLOPT_SUCCESS != nlopt_set_maxtime(opt, 3.0)){ // In seconds.
            FUNCERR("NLOpt unable to set maxtime stopping condition");
        }
        if(NLOPT_SUCCESS != nlopt_set_maxeval(opt, 5'000'000)){ // Maximum # of objective func evaluations.
            FUNCERR("NLOpt unable to set maxeval stopping condition");
        }
        if(NLOPT_SUCCESS != nlopt_set_vector_storage(opt, 400)){ // Amount of memory to use (MB).
            FUNCERR("NLOpt unable to tell NLOpt to use more scratch space");
        }

        const auto opt_status = nlopt_optimize(opt, params, &func_min);

        if(opt_status < 0){
            state.FittingSuccess = false;
            if(opt_status == NLOPT_FAILURE){                FUNCWARN("NLOpt fail: generic failure");
            }else if(opt_status == NLOPT_INVALID_ARGS){     FUNCERR("NLOpt fail: invalid arguments");
            }else if(opt_status == NLOPT_OUT_OF_MEMORY){    FUNCWARN("NLOpt fail: out of memory");
            }else if(opt_status == NLOPT_ROUNDOFF_LIMITED){ FUNCWARN("NLOpt fail: roundoff limited");
            }else if(opt_status == NLOPT_FORCED_STOP){      FUNCWARN("NLOpt fail: forced termination");
            }else{ FUNCERR("NLOpt fail: unrecognized error code"); }
            // See http://ab-initio.mit.edu/wiki/index.php/NLopt_Reference for error code info.
        }else{
            state.FittingSuccess = true;
            if(true){
                if(opt_status == NLOPT_SUCCESS){                FUNCINFO("NLOpt: success");
                }else if(opt_status == NLOPT_STOPVAL_REACHED){  FUNCINFO("NLOpt: stopval reached");
                }else if(opt_status == NLOPT_FTOL_REACHED){     FUNCINFO("NLOpt: ftol reached");
                }else if(opt_status == NLOPT_XTOL_REACHED){     FUNCINFO("NLOpt: xtol reached");
                }else if(opt_status == NLOPT_MAXEVAL_REACHED){  FUNCINFO("NLOpt: maxeval count reached");
                }else if(opt_status == NLOPT_MAXTIME_REACHED){  FUNCINFO("NLOpt: maxtime reached");
                }else{ FUNCERR("NLOpt fail: unrecognized success code"); }
            }
        }

        nlopt_destroy(opt);
    }

    //Second-pass fit. Only bother if the first pass was reasonable.
//    if(state.FittingSuccess){
    if(false){
        nlopt_opt opt; //See `man nlopt` to get list of available algorithms.
        //opt = nlopt_create(NLOPT_LN_COBYLA, dimen);   //Local, no-derivative schemes.
        //opt = nlopt_create(NLOPT_LN_BOBYQA, dimen);
        //opt = nlopt_create(NLOPT_LN_SBPLX, dimen);

        //opt = nlopt_create(NLOPT_LD_MMA, dimen);   //Local, derivative schemes.
        //opt = nlopt_create(NLOPT_LD_SLSQP, dimen);
        //opt = nlopt_create(NLOPT_LD_LBFGS, dimen);
        //opt = nlopt_create(NLOPT_LD_VAR2, dimen);
        //opt = nlopt_create(NLOPT_LD_TNEWTON_PRECOND_RESTART, dimen);
        //opt = nlopt_create(NLOPT_LD_TNEWTON_PRECOND, dimen);
        opt = nlopt_create(NLOPT_LD_TNEWTON, dimen);

        //opt = nlopt_create(NLOPT_GN_DIRECT, dimen); //Global, no-derivative schemes.
        //opt = nlopt_create(NLOPT_GN_CRS2_LM, dimen);
        //opt = nlopt_create(NLOPT_GN_ESCH, dimen);
        //opt = nlopt_create(NLOPT_GN_ISRES, dimen);
                        
        nlopt_set_lower_bounds(opt, l_bnds);
        nlopt_set_upper_bounds(opt, u_bnds);
                        
        if(NLOPT_SUCCESS != nlopt_set_initial_step(opt, initstpsz)){
            FUNCERR("NLOpt unable to set initial step sizes");
        }
        if(NLOPT_SUCCESS != nlopt_set_min_objective(opt, MinimizationFunction_Reduced3Param, reinterpret_cast<void*>(&state))){
            FUNCERR("NLOpt unable to set objective function for minimization");
        }
        //if(NLOPT_SUCCESS != nlopt_set_xtol_rel(opt, 1.0E-3)){
        //    FUNCERR("NLOpt unable to set xtol stopping condition");
        //}
        //if(NLOPT_SUCCESS != nlopt_set_xtol_abs(opt, xtol_abs_thresholds)){
        //    FUNCERR("NLOpt unable to set xtol_abs stopping condition");
        //}
        if(NLOPT_SUCCESS != nlopt_set_ftol_rel(opt, 1.0E-5)){
            FUNCERR("NLOpt unable to set ftol_rel stopping condition");
        }
        if(NLOPT_SUCCESS != nlopt_set_maxtime(opt, 30.0)){ // In seconds.
            FUNCERR("NLOpt unable to set maxtime stopping condition");
        }
        if(NLOPT_SUCCESS != nlopt_set_maxeval(opt, 5'000'000)){ // Maximum # of objective func evaluations.
            FUNCERR("NLOpt unable to set maxeval stopping condition");
        }
        if(NLOPT_SUCCESS != nlopt_set_vector_storage(opt, 400)){ // Amount of memory to use (MB).
            FUNCERR("NLOpt unable to tell NLOpt to use more scratch space");
        }

        const auto opt_status = nlopt_optimize(opt, params, &func_min);

        if(opt_status < 0){
            state.FittingSuccess = false;
            if(opt_status == NLOPT_FAILURE){                FUNCWARN("NLOpt fail: generic failure");
            }else if(opt_status == NLOPT_INVALID_ARGS){     FUNCERR("NLOpt fail: invalid arguments");
            }else if(opt_status == NLOPT_OUT_OF_MEMORY){    FUNCWARN("NLOpt fail: out of memory");
            }else if(opt_status == NLOPT_ROUNDOFF_LIMITED){ FUNCWARN("NLOpt fail: roundoff limited");
            }else if(opt_status == NLOPT_FORCED_STOP){      FUNCWARN("NLOpt fail: forced termination");
            }else{ FUNCERR("NLOpt fail: unrecognized error code"); }
            // See http://ab-initio.mit.edu/wiki/index.php/NLopt_Reference for error code info.
        }else{
            state.FittingSuccess = true;
            if(true){
                if(opt_status == NLOPT_SUCCESS){                FUNCINFO("NLOpt: success");
                }else if(opt_status == NLOPT_STOPVAL_REACHED){  FUNCINFO("NLOpt: stopval reached");
                }else if(opt_status == NLOPT_FTOL_REACHED){     FUNCINFO("NLOpt: ftol reached");
                }else if(opt_status == NLOPT_XTOL_REACHED){     FUNCINFO("NLOpt: xtol reached");
                }else if(opt_status == NLOPT_MAXEVAL_REACHED){  FUNCINFO("NLOpt: maxeval count reached");
                }else if(opt_status == NLOPT_MAXTIME_REACHED){  FUNCINFO("NLOpt: maxtime reached");
                }else{ FUNCERR("NLOpt fail: unrecognized success code"); }
            }
        }

        nlopt_destroy(opt);
    }
    // ----------------------------------------------------------------------------

    state.FittingPerformed = true;

    state.RSS  = func_min;

    state.tauA = params[0];
    state.tauV = params[1];
    state.k2   = params[2];

// ... compute k1A and k1V here ...
//    state.k1A  = params[0];
//    state.k1V  = params[2];

#endif // DCMA_USE_NLOPT

    return std::move(state);
}

