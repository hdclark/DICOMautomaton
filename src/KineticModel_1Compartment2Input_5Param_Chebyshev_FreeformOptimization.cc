//KineticModel_1Compartment2Input_5Param_Chebyshev_FreeformOptimization.cc.
// This file holds an isolated driver for fitting a pharmacokinetic model. It uses generic optimzation so norms other
// than L2 can be used. Note that if using the L2 norm it seems most useful to use the Levenberg-Marquardt algorithm
// instead.

#include <array>
#include <cmath>
#include <limits>
#include <memory>

#ifdef DCMA_USE_NLOPT
#include <nlopt.h>
#endif // DCMA_USE_NLOPT

#include "KineticModel_1Compartment2Input_5Param_Chebyshev_Common.h"
#include "KineticModel_1Compartment2Input_5Param_Chebyshev_FreeformOptimization.h"
#include "YgorMath.h"
#include "YgorMisc.h"


static
double 
MinimizationFunction_5Param(unsigned, const double *params, double *grad, void *voided_state){

    auto state = reinterpret_cast<KineticModel_1Compartment2Input_5Param_Chebyshev_Parameters*>(voided_state);

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

    KineticModel_1Compartment2Input_5Param_Chebyshev_Results model_res;
    for(const auto &P : state->cROI->samples){
        const double t = P[0];
        const double R = P[2];

        Evaluate_Model(*state, t, model_res);
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

struct KineticModel_1Compartment2Input_5Param_Chebyshev_Parameters
Optimize_FreeformOptimization_5Param(KineticModel_1Compartment2Input_5Param_Chebyshev_Parameters state){

    state.FittingPerformed = false;
    state.FittingSuccess = false;

    const int dimen = 5;
    double func_min;

    //Fitting parameters:      k1A,  tauA,   k1V,  tauV,  k2.
    // The following are arbitrarily chosen. They should be seeded from previous computations, or
    // at least be nominal values reported within the literature.
    double params[dimen];

    //If there were finite parameters provided, use them as the initial guesses.
    params[0] = std::isfinite(state.k1A)  ? state.k1A  : 0.0030;
    params[1] = std::isfinite(state.tauA) ? state.tauA : 0.0000;
    params[2] = std::isfinite(state.k1V)  ? state.k1V  : 0.0030;
    params[3] = std::isfinite(state.tauV) ? state.tauV : 0.0000;
    params[4] = std::isfinite(state.k2)   ? state.k2   : 0.0518;

    // U/L bounds:             k1A,  tauA,  k1V,  tauV,  k2.
    double l_bnds[dimen] = {   0.0, -20.0,  0.0, -20.0,  0.0 };
    double u_bnds[dimen] = {  10.0,  20.0, 10.0,  20.0, 10.0 };
                    
    //Initial step sizes:          k1A,   tauA,    k1V,   tauV,    k2.
    double initstpsz[dimen] = { 0.0040, 3.2000, 0.0040, 3.2000, 0.0050 };

    //Absolute parameter change thresholds:   k1A,    tauA,     k1V,    tauV,     k2.
    double xtol_abs_thresholds[dimen] = { 0.00005, 0.00010, 0.00005, 0.00010, 0.00005 };

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
                        
        //nlopt_set_lower_bounds(opt, l_bnds);
        //nlopt_set_upper_bounds(opt, u_bnds);
                        
        if(NLOPT_SUCCESS != nlopt_set_initial_step(opt, initstpsz)){
            FUNCERR("NLOpt unable to set initial step sizes");
        }
        if(NLOPT_SUCCESS != nlopt_set_min_objective(opt, MinimizationFunction_5Param, reinterpret_cast<void*>(&state))){
            FUNCERR("NLOpt unable to set objective function for minimization");
        }
        //if(NLOPT_SUCCESS != nlopt_set_xtol_rel(opt, 1.0E-3)){
        //    FUNCERR("NLOpt unable to set xtol stopping condition");
        //}
        if(NLOPT_SUCCESS != nlopt_set_xtol_abs(opt, xtol_abs_thresholds)){
            FUNCERR("NLOpt unable to set xtol_abs stopping condition");
        }
        if(NLOPT_SUCCESS != nlopt_set_ftol_rel(opt, 1.0E-7)){
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

    //Second-pass fit. Only bother if the first pass was reasonable.
    if(state.FittingSuccess){
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
                        
        //nlopt_set_lower_bounds(opt, l_bnds);
        //nlopt_set_upper_bounds(opt, u_bnds);
                        
        if(NLOPT_SUCCESS != nlopt_set_initial_step(opt, initstpsz)){
            FUNCERR("NLOpt unable to set initial step sizes");
        }
        if(NLOPT_SUCCESS != nlopt_set_min_objective(opt, MinimizationFunction_5Param, reinterpret_cast<void*>(&state))){
            FUNCERR("NLOpt unable to set objective function for minimization");
        }
        //if(NLOPT_SUCCESS != nlopt_set_xtol_rel(opt, 1.0E-3)){
        //    FUNCERR("NLOpt unable to set xtol stopping condition");
        //}
        //if(NLOPT_SUCCESS != nlopt_set_xtol_abs(opt, xtol_abs_thresholds)){
        //    FUNCERR("NLOpt unable to set xtol_abs stopping condition");
        //}
        if(NLOPT_SUCCESS != nlopt_set_ftol_rel(opt, 1.0E-7)){
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

    state.k1A  = params[0];
    state.tauA = params[1];
    state.k1V  = params[2];
    state.tauV = params[3];
    state.k2   = params[4];

#endif // DCMA_USE_NLOPT

    return state;
}


//---------------------------------------------------------------------------------------------
static
double 
MinimizationFunction_3Param(unsigned, const double *params, double *grad, void *voided_state){

    auto state = reinterpret_cast<KineticModel_1Compartment2Input_5Param_Chebyshev_Parameters*>(voided_state);

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
    state->k1V  = params[1];
    state->tauV = 0.0;
    state->k2   = params[2];

    KineticModel_1Compartment2Input_5Param_Chebyshev_Results model_res;
    for(const auto &P : state->cROI->samples){
        const double t = P[0];
        const double R = P[2];

        Evaluate_Model(*state, t, model_res);
        const double I = model_res.I;
        
        sqDist += std::pow(R - I, 2.0); //Standard L2-norm.
        if(grad != nullptr){
            const double chain = -2.0*(R-I);
            grad[0] += chain * model_res.d_I_d_k1A;
            grad[1] += chain * model_res.d_I_d_k1V;
            grad[2] += chain * model_res.d_I_d_k2;
        }
    }

    if(!std::isfinite(sqDist)) sqDist = std::numeric_limits<double>::max();
    return sqDist;
}

struct KineticModel_1Compartment2Input_5Param_Chebyshev_Parameters
Optimize_FreeformOptimization_3Param(KineticModel_1Compartment2Input_5Param_Chebyshev_Parameters state){

    state.FittingPerformed = false;
    state.FittingSuccess = false;

    const int dimen = 3;
    double func_min;

    //Fitting parameters:      k1A,  k1V,  k2.
    // The following are arbitrarily chosen. They should be seeded from previous computations, or
    // at least be nominal values reported within the literature.
    double params[dimen];

    //If there were finite parameters provided, use them as the initial guesses.
    params[0] = std::isfinite(state.k1A)  ? state.k1A  : 0.0030;
    params[1] = std::isfinite(state.k1V)  ? state.k1V  : 0.0030;
    params[2] = std::isfinite(state.k2)   ? state.k2   : 0.0518;

    // U/L bounds:               k1A,  k1V,  k2.
    //double l_bnds[dimen] = {   0.0,  0.0,  0.0 };
    //double u_bnds[dimen] = {  10.0, 10.0, 10.0 };
                    
    //Initial step sizes:      k1A,  k1V,  k2.
    double initstpsz[dimen] = { 0.0040, 0.0040, 0.0050 };

    //Absolute parameter change thresholds:   k1A,  k1V,  k2.
    double xtol_abs_thresholds[dimen] = { 0.00005, 0.00005, 0.00005 };

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
                        
        //nlopt_set_lower_bounds(opt, l_bnds);
        //nlopt_set_upper_bounds(opt, u_bnds);
                        
        if(NLOPT_SUCCESS != nlopt_set_initial_step(opt, initstpsz)){
            FUNCERR("NLOpt unable to set initial step sizes");
        }
        if(NLOPT_SUCCESS != nlopt_set_min_objective(opt, MinimizationFunction_3Param, reinterpret_cast<void*>(&state))){
            FUNCERR("NLOpt unable to set objective function for minimization");
        }
        //if(NLOPT_SUCCESS != nlopt_set_xtol_rel(opt, 1.0E-4)){
        //    FUNCERR("NLOpt unable to set xtol_rel stopping condition");
        //}
        if(NLOPT_SUCCESS != nlopt_set_xtol_abs(opt, xtol_abs_thresholds)){
            FUNCERR("NLOpt unable to set xtol_abs stopping condition");
        }
        if(NLOPT_SUCCESS != nlopt_set_ftol_rel(opt, 1.0E-7)){
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

        double func_min;
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
    if(state.FittingSuccess){
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
                        
        //nlopt_set_lower_bounds(opt, l_bnds);
        //nlopt_set_upper_bounds(opt, u_bnds);
                        
        if(NLOPT_SUCCESS != nlopt_set_initial_step(opt, initstpsz)){
            FUNCERR("NLOpt unable to set initial step sizes");
        }
        if(NLOPT_SUCCESS != nlopt_set_min_objective(opt, MinimizationFunction_3Param, reinterpret_cast<void*>(&state))){
            FUNCERR("NLOpt unable to set objective function for minimization");
        }
        //if(NLOPT_SUCCESS != nlopt_set_xtol_rel(opt, 1.0E-4)){
        //    FUNCERR("NLOpt unable to set xtol_rel stopping condition");
        //}
        //if(NLOPT_SUCCESS != nlopt_set_xtol_abs(opt, xtol_abs_thresholds)){
        //    FUNCERR("NLOpt unable to set xtol_abs stopping condition");
        //}
        if(NLOPT_SUCCESS != nlopt_set_ftol_rel(opt, 1.0E-7)){
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

        double func_min;
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

    state.k1A  = params[0];
    state.tauA = 0.0;
    state.k1V  = params[1];
    state.tauV = 0.0;
    state.k2   = params[2];

#endif // DCMA_USE_NLOPT

    return state;
}

