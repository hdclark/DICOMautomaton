//Pharmacokinetic_Modeling.cc.
// This file holds isolated drivers for fitting pharmacokinetic models.

#include <list>
#include <functional>
#include <limits>
#include <cmath>

#include <nlopt.h>

#include <boost/date_time/posix_time/posix_time.hpp>

#include "YgorMisc.h"
#include "YgorMath.h"
#include "YgorMathChebyshev.h"
#include "YgorMathChebyshevFunctions.h"
#include "YgorStats.h"       //Needed for Stats:: namespace.

#include "Pharmacokinetic_Modeling.h"

void
chebyshev_5param_model( const Pharmacokinetic_Parameters_5Param_Chebyshev &state,
                        const double t,
                        Pharmacokinetic_Parameters_5Param_Chebyshev_Results &res){
                
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

    const size_t exp_approx_N = 10; // 5 is probably OK. 10 should suffice. 20 could be overkill. Depends on params, though.

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
        integrand = exp_kern * (*(state.cAIF));
        integral = integrand.Chebyshev_Integral();
        int_AIF_exp = (integral.Sample(taumax) - integral.Sample(taumin));

        //Compute things for gradients.
        {
            //Evaluate $\partial_{k2}$ part of gradient.
            integrand = integrand * Chebyshev_Basis_Exact_Linear(expmin,expmax,1.0,tauA-t);
            integral = integrand.Chebyshev_Integral();
            int_AIF_exp_tau = (integral.Sample(taumax) - integral.Sample(taumin));

            //Evaluate $\partial_{tauA}$ part of gradient.
            integrand = exp_kern * (*(state.dcAIF));
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
        integrand = exp_kern * (*state.cVIF);
        integral = integrand.Chebyshev_Integral();
        int_VIF_exp = (integral.Sample(taumax) - integral.Sample(taumin));

        //Compute things for gradients.
        {
            //Evaluate $\partial_{k2}$ part of gradient.
            integrand = integrand * Chebyshev_Basis_Exact_Linear(expmin,expmax,1.0,tauV-t);
            integral = integrand.Chebyshev_Integral();
            int_VIF_exp_tau = (integral.Sample(taumax) - integral.Sample(taumin));

            //Evaluate $\partial_{tauV}$ part of gradient.
            integrand = exp_kern * ((*state.dcVIF));
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



static
double 
chebyshev_5param_func_to_min(unsigned, const double *params, double *grad, void *voided_state){

    auto state = reinterpret_cast<Pharmacokinetic_Parameters_5Param_Chebyshev*>(voided_state);

    //This function computes the square-distance between the ROI time course and a kinetic liver
    // perfusion model at the ROI sample t_i's. If gradients are requested, they are also computed.
    double sqDist = 0.0;
    if(grad != nullptr){
        grad[0] = 0.0;
        grad[1] = 0.0;
        grad[2] = 0.0;
        grad[3] = 0.0;
        grad[4] = 0.0;
    }

    //Pack the current parameters into the state struct. This is done to unify the nlopt and user calling styles.
    state->k1A  = params[0];
    state->tauA = params[1]; 
    state->k1V  = params[2];
    state->tauV = params[3];
    state->k2   = params[4];

    Pharmacokinetic_Parameters_5Param_Chebyshev_Results model_res;
    for(const auto &P : state->cROI->samples){
        const double t = P[0];
        const double R = P[2];

        chebyshev_5param_model(*state, t, model_res);
        const double I = model_res.I;
        
        sqDist += std::pow(R - I, 2.0); //Standard L2-norm.
        if(grad != nullptr){
            const double chain = -2.0*(R-I);
            grad[0] += chain * model_res.d_I_d_k1A;
            grad[1] += chain * model_res.d_I_d_tauA;
            grad[2] += chain * model_res.d_I_d_k1V;
            grad[3] += chain * model_res.d_I_d_tauV;
            grad[4] += chain * model_res.d_I_d_k2;
        }
    }
    if(!std::isfinite(sqDist)) sqDist = std::numeric_limits<double>::max();
    return sqDist;
}

struct Pharmacokinetic_Parameters_5Param_Chebyshev
Pharmacokinetic_Model_5Param_Chebyshev(Pharmacokinetic_Parameters_5Param_Chebyshev state){

    state.FittingPerformed = false;
    state.FittingSuccess = false;

    const int dimen = 5;

    //Fitting parameters:      k1A,  tauA,   k1V,  tauV,  k2.
    // The following are arbitrarily chosen. They should be seeded from previous computations, or
    // at least be nominal values reported within the literature.
    double params[dimen];

    //If there were finite parameters provided, use them as the initial guesses.
    params[0] = std::isfinite(state.k1A)  ? state.k1A  : 0.0057;
    params[1] = std::isfinite(state.tauA) ? state.tauA : 2.87;
    params[2] = std::isfinite(state.k1V)  ? state.k1V  : 0.0052;
    params[3] = std::isfinite(state.tauV) ? state.tauV : -14.4;
    params[4] = std::isfinite(state.k2)   ? state.k2   : 0.033;

    // U/L bounds:             k1A,  tauA,  k1V,  tauV,  k2.
    //double l_bnds[dimen] = {   0.0, -20.0,  0.0, -20.0,  0.0 };
    //double u_bnds[dimen] = {   1.0,  20.0,  1.0,  20.0,  1.0 };
                    
    //Initial step sizes:      k1A,  tauA,  k1V,  tauV,  k2.
    double initstpsz[dimen] = { 0.004, 1.0, 0.003, 1.0, 0.010 };

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
                    
    //nlopt_set_lower_bounds(opt, l_bnds);
    //nlopt_set_upper_bounds(opt, u_bnds);
                    
    if(NLOPT_SUCCESS != nlopt_set_initial_step(opt, initstpsz)){
        FUNCERR("NLOpt unable to set initial step sizes");
    }
    if(NLOPT_SUCCESS != nlopt_set_min_objective(opt, chebyshev_5param_func_to_min, reinterpret_cast<void*>(&state))){
        FUNCERR("NLOpt unable to set objective function for minimization");
    }
    if(NLOPT_SUCCESS != nlopt_set_xtol_rel(opt, 1.0E-3)){
        FUNCERR("NLOpt unable to set xtol stopping condition");
    }
    if(NLOPT_SUCCESS != nlopt_set_maxtime(opt, 30.0)){ // In seconds.
        FUNCERR("NLOpt unable to set maxtime stopping condition");
    }
    if(NLOPT_SUCCESS != nlopt_set_maxeval(opt, 5'000'000)){ // Maximum # of objective func evaluations.
        FUNCERR("NLOpt unable to set maxeval stopping condition");
    }
    if(NLOPT_SUCCESS != nlopt_set_vector_storage(opt, 200)){ // Amount of memory to use (MB).
        FUNCERR("NLOpt unable to tell NLOpt to use more scratch space");
    }

    double func_min;
    const auto opt_status = nlopt_optimize(opt, params, &func_min);
    state.FittingPerformed = true;

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
    // ----------------------------------------------------------------------------

    state.k1A  = params[0];
    state.tauA = params[1];
    state.k1V  = params[2];
    state.tauV = params[3];
    state.k2   = params[4];

    return std::move(state);
}



static
double 
chebyshev_3param_func_to_min(unsigned, const double *params, double *grad, void *voided_state){

    auto state = reinterpret_cast<Pharmacokinetic_Parameters_5Param_Chebyshev*>(voided_state);

    //This function computes the square-distance between the ROI time course and a kinetic liver
    // perfusion model at the ROI sample t_i's. If gradients are requested, they are also computed.
    double sqDist = 0.0;
    if(grad != nullptr){
        grad[0] = 0.0;
        grad[1] = 0.0;
        grad[2] = 0.0;
    }

    //Pack the current parameters into the state struct. This is done to unify the nlopt and user calling styles.
    state->k1A  = params[0];
    state->tauA = 0.0;
    state->k1V  = params[2];
    state->tauV = 0.0;
    state->k2   = params[4];

    Pharmacokinetic_Parameters_5Param_Chebyshev_Results model_res;
    for(const auto &P : state->cROI->samples){
        const double t = P[0];
        const double R = P[2];

        chebyshev_5param_model(*state, t, model_res);
        const double I = model_res.I;
        
        sqDist += std::pow(R - I, 2.0); //Standard L2-norm.
        if(grad != nullptr){
            const double chain = -2.0*(R-I);
            grad[0] += chain * model_res.d_I_d_k1A;
            grad[1] += chain * model_res.d_I_d_k1V;
            grad[2] += chain * model_res.d_I_d_k2;
        }
    }

std::cout << "Exiting the 'chebyshev_3param_func_to_min' now. sqDist = " << sqDist << std::endl;    
    if(!std::isfinite(sqDist)) sqDist = std::numeric_limits<double>::max();
    return sqDist;
}

struct Pharmacokinetic_Parameters_5Param_Chebyshev
Pharmacokinetic_Model_3Param_Chebyshev(Pharmacokinetic_Parameters_5Param_Chebyshev state){

    state.FittingPerformed = false;
    state.FittingSuccess = false;

    const int dimen = 3;

    //Fitting parameters:      k1A,  tauA,   k1V,  tauV,  k2.
    // The following are arbitrarily chosen. They should be seeded from previous computations, or
    // at least be nominal values reported within the literature.
    double params[dimen];

    //If there were finite parameters provided, use them as the initial guesses.
    params[0] = std::isfinite(state.k1A)  ? state.k1A  : 0.0057;
    params[1] = std::isfinite(state.k1V)  ? state.k1V  : 0.0052;
    params[2] = std::isfinite(state.k2)   ? state.k2   : 0.033;

    // U/L bounds:               k1A,  k1V,  k2.
    //double l_bnds[dimen] = {   0.0,  0.0,  0.0 };
    //double u_bnds[dimen] = {   1.0,  1.0,  1.0 };
                    
    //Initial step sizes:      k1A,  k1V,  k2.
    double initstpsz[dimen] = { 0.004, 0.003, 0.010 };

    nlopt_opt opt; //See `man nlopt` to get list of available algorithms.
    opt = nlopt_create(NLOPT_LN_COBYLA, dimen);   //Local, no-derivative schemes.
    //opt = nlopt_create(NLOPT_LN_BOBYQA, dimen);
    //opt = nlopt_create(NLOPT_LN_SBPLX, dimen);

    //opt = nlopt_create(NLOPT_LD_MMA, dimen);   //Local, derivative schemes.
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
                    
    //nlopt_set_lower_bounds(opt, l_bnds);
    //nlopt_set_upper_bounds(opt, u_bnds);
                    
    if(NLOPT_SUCCESS != nlopt_set_initial_step(opt, initstpsz)){
        FUNCERR("NLOpt unable to set initial step sizes");
    }
    if(NLOPT_SUCCESS != nlopt_set_min_objective(opt, chebyshev_3param_func_to_min, reinterpret_cast<void*>(&state))){
        FUNCERR("NLOpt unable to set objective function for minimization");
    }
    if(NLOPT_SUCCESS != nlopt_set_xtol_rel(opt, 1.0E-4)){
        FUNCERR("NLOpt unable to set xtol stopping condition");
    }
    if(NLOPT_SUCCESS != nlopt_set_maxtime(opt, 60.0)){ // In seconds.
        FUNCERR("NLOpt unable to set maxtime stopping condition");
    }
    if(NLOPT_SUCCESS != nlopt_set_maxeval(opt, 5'000)){ // Maximum # of objective func evaluations.
        FUNCERR("NLOpt unable to set maxeval stopping condition");
    }
    if(NLOPT_SUCCESS != nlopt_set_vector_storage(opt, 200)){ // Amount of memory to use (MB).
        FUNCERR("NLOpt unable to tell NLOpt to use more scratch space");
    }

FUNCINFO("About to begin optimization");

    double func_min;
    const auto opt_status = nlopt_optimize(opt, params, &func_min);
    state.FittingPerformed = true;

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
    // ----------------------------------------------------------------------------

    state.k1A  = params[0];
    state.tauA = 0.0;
    state.k1V  = params[1];
    state.tauV = 0.0;
    state.k2   = params[2];

    return std::move(state);
}

