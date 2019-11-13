//KineticModel_1Compartment2Input_5Param_LinearInterp_LevenbergMarquardt.cc.
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
#include <stddef.h>
#include <array>
#include <cmath>
#include <exception>
#include <limits>
#include <memory>
#include <vector>

#include "KineticModel_1Compartment2Input_5Param_LinearInterp_Common.h"
#include "KineticModel_1Compartment2Input_5Param_LinearInterp_LevenbergMarquardt.h"
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

    auto state = reinterpret_cast<KineticModel_1Compartment2Input_5Param_LinearInterp_Parameters*>(voided_state);

    //Pack the current parameters into the state struct.
    state->k1A  = gsl_vector_get(params, 0);
    state->tauA = gsl_vector_get(params, 1); 
    state->k1V  = gsl_vector_get(params, 2);
    state->tauV = gsl_vector_get(params, 3);
    state->k2   = gsl_vector_get(params, 4);

    KineticModel_1Compartment2Input_5Param_LinearInterp_Results model_res;

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


struct KineticModel_1Compartment2Input_5Param_LinearInterp_Parameters
Optimize_LevenbergMarquardt_5Param(KineticModel_1Compartment2Input_5Param_LinearInterp_Parameters state){
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
        multifit_f.df = nullptr;
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

        gsl_vector *res_f = gsl_multifit_fdfsolver_residual(solver);
        const double chi = gsl_blas_dnrm2(res_f);
        const double chisq = chi * chi;
        const double dof = static_cast<double>(datum - dimen);
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
        multifit_f.df = nullptr;
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

*/

static
double 
chebyshev_3param_func_to_min(unsigned, const double *, double *, void *){

    FUNCERR("Not yet implemented");
    return 0.0;

}

struct KineticModel_1Compartment2Input_5Param_LinearInterp_Parameters
Optimize_LevenbergMarquardt_3Param(KineticModel_1Compartment2Input_5Param_LinearInterp_Parameters state){

    FUNCERR("Not yet implemented");
    return state;
}

