//KineticModel_1Compartment2Input_5Param_Chebyshev_LevenbergMarquardt.cc.
// This file holds an isolated driver for fitting a pharmacokinetic model. It uses an algorithm, the
// Levenberg-Marquardt, that is specific to least-squares and therefore cannot be used for norms other than L2.

#ifdef DCMA_USE_GNU_GSL
#else
    #error "Attempting to compile this operation without GNU GSL, which is required."
#endif

#include <gsl/gsl_blas.h>
#include <gsl/gsl_errno.h>
#include <gsl/gsl_matrix_double.h>
#include <gsl/gsl_multifit_nlin.h>
#include <gsl/gsl_vector_double.h>
#include <cstddef>
#include <array>
#include <cmath>
#include <exception>
#include <limits>
#include <memory>
#include <vector>

#include "KineticModel_1Compartment2Input_5Param_Chebyshev_Common.h"
#include "KineticModel_1Compartment2Input_5Param_Chebyshev_LevenbergMarquardt.h"
#include "YgorMath.h"
#include "YgorMisc.h"


static
int
MinimizationFunction_f_5Param( const gsl_vector *params,  //Parameters being fitted.
                                void *voided_state,        //Other information (e.g., constant function parameters).
                                gsl_vector *f){            //Vector containtin function evaluats at {t_i}.

    //This function essentially computes the square-distance between the ROI time course and a kinetic liver perfusion
    // model at the ROI sample t_i's. However, instead of reporting the summed square-distance, the difference of
    // function values and observations at each t_i are reported (and summed internally within the optimizer).

    auto state = reinterpret_cast<KineticModel_1Compartment2Input_5Param_Chebyshev_Parameters*>(voided_state);

    //Pack the current parameters into the state struct.
    state->k1A  = gsl_vector_get(params, 0);
    state->tauA = gsl_vector_get(params, 1); 
    state->k1V  = gsl_vector_get(params, 2);
    state->tauV = gsl_vector_get(params, 3);
    state->k2   = gsl_vector_get(params, 4);

    KineticModel_1Compartment2Input_5Param_Chebyshev_Results model_res;

    size_t i = 0;
    double I;
    for(const auto &P : state->cROI->samples){
        const double t = P[0];
        const double R = P[2];
 
        I = std::numeric_limits<double>::quiet_NaN();
        try{
            Evaluate_Model(*state, t, model_res);
            I = model_res.I;
        }catch(std::exception &){ }
        I = std::isfinite(I) ? I : std::numeric_limits<double>::infinity();

        gsl_vector_set(f, i, I - R);
        ++i;
    }

    return GSL_SUCCESS;
}

static
int
MinimizationFunction_df_5Param( const gsl_vector *params,  //Parameters being fitted.
                                 void *voided_state,        //Other information (e.g., constant function parameters).
                                 gsl_matrix * J){           //Jacobian matrix.

    //This function prepares Jacobian matrix elements for gsl. The Jacobian is defined as:
    //  J(i,j) = \frac{\partial I(t_i;param_0, param_1, param_2, ...)}{\partial param_j}
    // where param_0 = k1A, param_1 = tauA, ..., param_4 = k2.

    auto state = reinterpret_cast<KineticModel_1Compartment2Input_5Param_Chebyshev_Parameters*>(voided_state);

    //Pack the current parameters into the state struct.
    state->k1A  = gsl_vector_get(params, 0);
    state->tauA = gsl_vector_get(params, 1); 
    state->k1V  = gsl_vector_get(params, 2);
    state->tauV = gsl_vector_get(params, 3);
    state->k2   = gsl_vector_get(params, 4);

    KineticModel_1Compartment2Input_5Param_Chebyshev_Results model_res;

    size_t i = 0;
    //double I;
    for(const auto &P : state->cROI->samples){
        const double t = P[0];
 
        gsl_matrix_set(J, i, 0, std::numeric_limits<double>::infinity());
        gsl_matrix_set(J, i, 1, std::numeric_limits<double>::infinity());
        gsl_matrix_set(J, i, 2, std::numeric_limits<double>::infinity());
        gsl_matrix_set(J, i, 3, std::numeric_limits<double>::infinity());
        gsl_matrix_set(J, i, 4, std::numeric_limits<double>::infinity());

        try{
            Evaluate_Model(*state, t, model_res);

            gsl_matrix_set(J, i, 0, model_res.d_I_d_k1A);
            gsl_matrix_set(J, i, 1, model_res.d_I_d_tauA);
            gsl_matrix_set(J, i, 2, model_res.d_I_d_k1V);
            gsl_matrix_set(J, i, 3, model_res.d_I_d_tauV);
            gsl_matrix_set(J, i, 4, model_res.d_I_d_k2);

        }catch(std::exception &){ }
        ++i;
    }

    return GSL_SUCCESS;
}


struct KineticModel_1Compartment2Input_5Param_Chebyshev_Parameters
Optimize_LevenbergMarquardt_5Param(KineticModel_1Compartment2Input_5Param_Chebyshev_Parameters state){
    //GSL-based fitter. This function performs a few passes to improve the likelihood of finding a solution.
    //
    // Note: Weights are not currently assigned, though they are supported by the available methods in gsl. Instead, 
    //       the GSL manual states: 
    //
    //       "This estimates the statistical error on the best-fit parameters from the scatter of the underlying data."
    //
    //       Weights could be derived more intelligently (adaptively) from the datum. It would be tricky to do
    //       correctly.

    state.FittingPerformed = true;
    state.FittingSuccess = false;

    bool skip_second_pass = false; //Gets set if a very good fit is found on the first pass.

    const size_t dimen = 5; //The number of fitting parameters.
    const size_t datum = state.cROI->samples.size(); //The number of samples or datum.

    double params[dimen]; //A shuttle into gsl for the parameters.

    gsl_matrix *J = gsl_matrix_alloc(datum, dimen);
    gsl_matrix *covar = gsl_matrix_alloc(dimen, dimen);

    //First-pass fit.
    {
        const size_t max_iters = 500;

        //If there were finite parameters provided, use them as the initial guesses.
        params[0] = std::isfinite(state.k1A)  ? state.k1A  : 0.0500;
        params[1] = std::isfinite(state.tauA) ? state.tauA : 1.0000;
        params[2] = std::isfinite(state.k1V)  ? state.k1V  : 0.0500;
        params[3] = std::isfinite(state.tauV) ? state.tauV : 1.0000;
        params[4] = std::isfinite(state.k2)   ? state.k2   : 0.0350;
        gsl_vector_view params_v = gsl_vector_view_array(params, dimen);

        const double paramtol_rel = 1.0E-3;
        //const double paramtol_rel = -10.0*std::numeric_limits<double>::epsilon();
        //const double paramtol_rel = std::pow(GSL_DBL_EPSILON, 0.5);
        //const double paramtol_rel = std::numeric_limits<double>::denorm_min();

        const double gtol_rel = 1.0E-3;
        //const double gtol_rel = 10.0*std::numeric_limits<double>::epsilon();
        //const double gtol_rel = std::pow(GSL_DBL_EPSILON, 0.5);
        //const double gtol_rel = std::numeric_limits<double>::denorm_min();

        const double ftol_rel = 1.0E-3; //CURRENTLY IGNORED BY SOME ROUTINES!
        //const double ftol_rel = std::numeric_limits<double>::denorm_min(); //CURRENTLY IGNORED BY SOME ROUTINES!

        gsl_multifit_function_fdf multifit_f;
        multifit_f.f = &MinimizationFunction_f_5Param;
        //multifit_f.df = nullptr; //If nullptr, use a finite-difference Jacobian derived from multifit_f.f.
        multifit_f.df = &MinimizationFunction_df_5Param;
        multifit_f.n = datum;
        multifit_f.p = dimen;
        multifit_f.params = reinterpret_cast<void*>(&state);

        //const gsl_multifit_fdfsolver_type *solver_type = gsl_multifit_fdfsolver_lmsder;
        const gsl_multifit_fdfsolver_type *solver_type = gsl_multifit_fdfsolver_lmder;
        //const gsl_multifit_fdfsolver_type *solver_type = gsl_multifit_fdfsolver_lmniel;

        gsl_multifit_fdfsolver *solver = gsl_multifit_fdfsolver_alloc(solver_type, datum, dimen);
        gsl_multifit_fdfsolver_set(solver, &multifit_f, &params_v.vector);

        //Perform the optimization.
        int info = -1;
        int status = gsl_multifit_fdfsolver_driver(solver, max_iters, paramtol_rel, gtol_rel, ftol_rel, &info);

        if(status == GSL_SUCCESS){
            gsl_vector *res_f = gsl_multifit_fdfsolver_residual(solver);
            const double chi = gsl_blas_dnrm2(res_f);
            const double chisq = chi * chi;
            const auto dof = static_cast<double>(datum - dimen);
            const double red_chisq = chisq/dof;

            state.RSS  = chisq;
            state.k1A  = gsl_vector_get(solver->x,0);
            state.tauA = gsl_vector_get(solver->x,1);
            state.k1V  = gsl_vector_get(solver->x,2);
            state.tauV = gsl_vector_get(solver->x,3);
            state.k2   = gsl_vector_get(solver->x,4);
            
            gsl_multifit_fdfsolver_free(solver);

            //If the fit was extremely good already, do not bother with another pass.
            // We assume a certain scale here, so it won't work in generality!
            if(red_chisq < 1E-10){
                skip_second_pass = true;
                state.FittingSuccess = true;
            }
        }
    }


    //Second-pass fit.
    if(!skip_second_pass){
        const size_t max_iters = 50'000;

        //If there were finite parameters provided, use them as the initial guesses.
        params[0] = std::isfinite(state.k1A)  ? state.k1A  : 0.0500;
        params[1] = std::isfinite(state.tauA) ? state.tauA : 1.0000;
        params[2] = std::isfinite(state.k1V)  ? state.k1V  : 0.0500;
        params[3] = std::isfinite(state.tauV) ? state.tauV : 1.0000;
        params[4] = std::isfinite(state.k2)   ? state.k2   : 0.0350;
        gsl_vector_view params_v = gsl_vector_view_array(params, dimen);

        const double paramtol_rel = 1.0E-3;
        //const double paramtol_rel = -10.0*std::numeric_limits<double>::epsilon();
        //const double paramtol_rel = std::pow(GSL_DBL_EPSILON, 0.5);
        //const double paramtol_rel = std::numeric_limits<double>::denorm_min();

        const double gtol_rel = 1.0E-3;
        //const double gtol_rel = 10.0*std::numeric_limits<double>::epsilon();
        //const double gtol_rel = std::pow(GSL_DBL_EPSILON, 0.5);
        //const double gtol_rel = std::numeric_limits<double>::denorm_min();

        const double ftol_rel = 1.0E-3; //CURRENTLY IGNORED BY SOME ROUTINES!
        //const double ftol_rel = std::numeric_limits<double>::denorm_min(); //CURRENTLY IGNORED BY SOME ROUTINES!

        gsl_multifit_function_fdf multifit_f;
        multifit_f.f = &MinimizationFunction_f_5Param;
        //multifit_f.df = nullptr; //If nullptr, use a finite-difference Jacobian derived from multifit_f.f.
        multifit_f.df = &MinimizationFunction_df_5Param;
        multifit_f.n = datum;
        multifit_f.p = dimen;
        multifit_f.params = reinterpret_cast<void*>(&state);

        const gsl_multifit_fdfsolver_type *solver_type = gsl_multifit_fdfsolver_lmsder;
        //const gsl_multifit_fdfsolver_type *solver_type = gsl_multifit_fdfsolver_lmder;
        //const gsl_multifit_fdfsolver_type *solver_type = gsl_multifit_fdfsolver_lmniel;

        gsl_multifit_fdfsolver *solver = gsl_multifit_fdfsolver_alloc(solver_type, datum, dimen);
        gsl_multifit_fdfsolver_set(solver, &multifit_f, &params_v.vector);

        //Perform the optimization.
        int info = -1;
        int status = gsl_multifit_fdfsolver_driver(solver, max_iters, paramtol_rel, gtol_rel, ftol_rel, &info);

        if(status == GSL_SUCCESS){
            //Compute the final residual norm.
            gsl_vector *res_f = gsl_multifit_fdfsolver_residual(solver);
            const double chi = gsl_blas_dnrm2(res_f);
            const double chisq = chi * chi;

            //Compute parameter uncertainties.
            //
            // NOTE: Uncertainties computed this way compare with those from Gnuplot on similar (analytical) data.
            //       I have confidence it is implemented correctly. Disabled because I'm not yet sure if I want
            //       uncertainties for any specific purpose.
            //
            //const double dof = static_cast<double>(datum - dimen);
            //const double red_chisq = chisq/dof;
            //const double c = std::sqrt(red_chisq);
            //gsl_multifit_fdfsolver_jac(solver, J);
            //gsl_multifit_covar(J, 0.0, covar);
            //fprintf(stderr, "    (k1A +/- %.5f), ", c*std::sqrt(gsl_matrix_get(covar,0,0)) );
            //fprintf(stderr, "(tauA +/- %.5f), ", c*std::sqrt(gsl_matrix_get(covar,1,1)) );
            //fprintf(stderr, "(k1V +/- %.5f), ", c*std::sqrt(gsl_matrix_get(covar,2,2)) );
            //fprintf(stderr, "(tauV +/- %.5f), ", c*std::sqrt(gsl_matrix_get(covar,3,3)) );
            //fprintf(stderr, "(k2 +/- %.5f).\n", c*std::sqrt(gsl_matrix_get(covar,4,4)) );

            state.FittingSuccess = true;
            state.RSS  = chisq;
            state.k1A  = gsl_vector_get(solver->x,0);
            state.tauA = gsl_vector_get(solver->x,1);
            state.k1V  = gsl_vector_get(solver->x,2);
            state.tauV = gsl_vector_get(solver->x,3);
            state.k2   = gsl_vector_get(solver->x,4);
        }
        
        gsl_multifit_fdfsolver_free(solver);
    }

    gsl_matrix_free(covar);
    gsl_matrix_free(J);

    return state;
}


//---------------------------------------------------------------------------------------------

/*
static
int
MinimizationFunction_f_3Param( const gsl_vector *params,  //Parameters being fitted.
                                void *voided_state,        //Other information (e.g., constant function parameters).
                                gsl_vector *f){            //Vector containtin function evaluats at {t_i}.

static
int
MinimizationFunction_df_3Param( const gsl_vector *params,  //Parameters being fitted.
                                 void *voided_state,        //Other information (e.g., constant function parameters).
                                 gsl_matrix * J){           //Jacobian matrix.


static
double 
chebyshev_3param_func_to_min(unsigned, const double *, double *, void *){

    FUNCERR("Not yet implemented");
    return 0.0;

/ *
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
* /
}

struct KineticModel_1Compartment2Input_5Param_Chebyshev_Parameters
Optimize_LevenbergMarquardt_3Param(KineticModel_1Compartment2Input_5Param_Chebyshev_Parameters state){

    FUNCERR("Not yet implemented");
    return state;

/ *

    state.FittingPerformed = false;
    state.FittingSuccess = false;

    const int dimen = 3;
    double func_min = 0.0;

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
        if(NLOPT_SUCCESS != nlopt_set_min_objective(opt, chebyshev_3param_func_to_min, reinterpret_cast<void*>(&state))){
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
        if(NLOPT_SUCCESS != nlopt_set_min_objective(opt, chebyshev_3param_func_to_min, reinterpret_cast<void*>(&state))){
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

    return std::move(state);
* /

}
*/

