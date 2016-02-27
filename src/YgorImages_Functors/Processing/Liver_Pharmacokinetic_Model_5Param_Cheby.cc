
#include <list>
#include <functional>
#include <limits>
#include <map>
#include <cmath>
#include <experimental/any>

#include <pqxx/pqxx>         //PostgreSQL C++ interface.
#include <jansson.h>         //For JSON handling.

#include <nlopt.h>

#include <boost/date_time/posix_time/posix_time.hpp>

#include "YgorMisc.h"
#include "YgorMath.h"
#include "YgorMathChebyshev.h"
#include "YgorMathChebyshevFunctions.h"
#include "YgorImages.h"
#include "YgorStats.h"       //Needed for Stats:: namespace.
#include "YgorFilesDirs.h"   //Needed for Does_File_Exist_And_Can_Be_Read(...), etc..
#include "YgorAlgorithms.h"  //Needed for For_Each_In_Parallel<..>(...)
#include "YgorString.h"      //Needed for GetFirstRegex(...)
#include "YgorPlot.h"

#include "../ConvenienceRoutines.h"
#include "Liver_Pharmacokinetic_Model_5Param_Cheby.h"


//This is the function we will minimize: the sum of the squared residuals between the ROI measured
// concentrations and the total concentration predicted by the fitted model.
static cheby_approx<double> *cAIF; //Global, but only so we can pass a c-style function to nlopt.
static cheby_approx<double> *cVIF;

static cheby_approx<double> *dcAIF; //Derivative.
static cheby_approx<double> *dcVIF; //Derivative.

static samples_1D<double> *theROI;  



static
inline
double 
chebyshev_model(const double t, const double *params, double *grad){
    // Chebyshev polynomial approximation method. 
    // 
    // This function computes the predicted contrast enhancement of a kinetic
    // liver perfusion model at the ROI sample t_i's. Gradients are able to be
    // computed using this method, so we split the computation up into several
    // parts to aide in gradient construction.

    const double k1A  = params[0];
    const double tauA = params[1];
    const double k1V  = params[2];
    const double tauV = params[3];
    const double k2   = params[4];

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
        std::tie(expmin,expmax) = cAIF->Get_Domain();
    
        cheby_approx<double> exp_kern = Chebyshev_Basis_Approx_Exp_Analytic1(exp_approx_N,expmin,expmax, A,B,C);
        cheby_approx<double> integrand;
        cheby_approx<double> integral;

        //Evaluate the model.
        integrand = exp_kern * (*cAIF);
        integral = integrand.Chebyshev_Integral();
        int_AIF_exp = (integral.Sample(taumax) - integral.Sample(taumin));

        if(grad != nullptr){
            //Evaluate $\partial_{k2}$ part of gradient.
            integrand = integrand * Chebyshev_Basis_Exact_Linear(expmin,expmax,1.0,tauA-t);
            integral = integrand.Chebyshev_Integral();
            int_AIF_exp_tau = (integral.Sample(taumax) - integral.Sample(taumin));

            //Evaluate $\partial_{tauA}$ part of gradient.
            integrand = exp_kern * (*dcAIF);
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
        std::tie(expmin,expmax) = cVIF->Get_Domain();
 
        cheby_approx<double> exp_kern = Chebyshev_Basis_Approx_Exp_Analytic1(exp_approx_N,expmin,expmax, A,B,C);
        cheby_approx<double> integrand;
        cheby_approx<double> integral;

        //Evaluate the model.
        integrand = exp_kern * (*cVIF);
        integral = integrand.Chebyshev_Integral();
        int_VIF_exp = (integral.Sample(taumax) - integral.Sample(taumin));

        if(grad != nullptr){
            //Evaluate $\partial_{k2}$ part of gradient.
            integrand = integrand * Chebyshev_Basis_Exact_Linear(expmin,expmax,1.0,tauV-t);
            integral = integrand.Chebyshev_Integral();
            int_VIF_exp_tau = (integral.Sample(taumax) - integral.Sample(taumin));

            //Evaluate $\partial_{tauV}$ part of gradient.
            integrand = exp_kern * (*dcVIF);
            integral = integrand.Chebyshev_Integral();
            int_dVIF_exp = (integral.Sample(taumax) - integral.Sample(taumin));
        }
    }

    //Evaluate the model's integral. This is the model's predicted contrast enhancement.
    const double I = (k1A * int_AIF_exp) + (k1V * int_VIF_exp);

    //Work out gradient information, if desired.
    if(grad != nullptr){
        grad[0] = ( int_AIF_exp );  // $\partial_{k1A}$
        grad[1] = ( -k1A * int_dAIF_exp ); // $\partial_{tauA}$
        grad[2] = ( int_VIF_exp ); // $\partial_{k1V}$
        grad[3] = ( -k1V * int_dVIF_exp ); // $\partial_{tauV}$
        grad[4] = ( (k1A * int_AIF_exp_tau) + (k1V * int_VIF_exp_tau)  ); // $\partial_{k2}$
    }

    return I;
}


static
double 
func_to_min(unsigned, const double *params, double *grad, void *){
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
    double dgrad[5];

    for(const auto &P : theROI->samples){
        const double t = P[0];
        const double R = P[2];

        const double I = chebyshev_model(t, params, ((grad == nullptr) ? nullptr : dgrad) );

        sqDist += std::pow(R - I, 2.0); //Standard L2-norm.
 
        if(grad != nullptr){
            const double chain = -2.0*(R-I);
            grad[0] += chain * dgrad[0];
            grad[1] += chain * dgrad[1];
            grad[2] += chain * dgrad[2];
            grad[3] += chain * dgrad[3];
            grad[4] += chain * dgrad[4];
        }
    }

    return sqDist;
}
 




bool
LiverPharmacoModel5ParamCheby(planar_image_collection<float,double>::images_list_it_t first_img_it,
                        std::list<planar_image_collection<float,double>::images_list_it_t> selected_img_its,
                        std::list<std::reference_wrapper<planar_image_collection<float,double>>> out_imgs,
                        std::list<std::reference_wrapper<contour_collection<double>>> ccsl,
                        std::experimental::any user_data ){


    //This takes aggregate time courses for (1) hepatic portal vein and (2) ascending aorta and attempts to 
    // fit a pharmacokinetic model to each voxel within the provided gross liver ROI. This entails fitting
    // a convolution model to the data, and a general optimization procedure is used.
    //
    // The input images must be grouped in the same way that the ROI time courses were computed. This will
    // most likely mean grouping spatially-overlapping images that have identical 'image acquisition time' 
    // (or just 'dt' for short) together.

    //Verify the correct number of outgoing parameter map slots have been provided.
    if(out_imgs.size() != 5){
        throw std::invalid_argument("This routine needs exactly five outgoing planar_image_collection"
                                    " so the resulting fitted parameter maps can be passed back.");
    }

    //Ensure we have the needed time course ROIs.
    LiverPharmacoModel5ParamChebyUserData *user_data_s;
    try{
        user_data_s = std::experimental::any_cast<LiverPharmacoModel5ParamChebyUserData *>(user_data);
    }catch(const std::exception &e){
        FUNCWARN("Unable to cast user_data to appropriate format. Cannot continue with computation");
        return false;
    }

    if( (user_data_s->time_courses.count("Abdominal_Aorta") == 0 )
    ||  (user_data_s->time_courses.count("Hepatic_Portal_Vein") == 0 ) ){
        throw std::invalid_argument("Both arterial and venous input time courses are needed."
                                    "(Are they named differently to the hard-coded names?)");
    }

    //Copy the incoming image to all the parameter maps. We will overwrite pixels as necessary.
    for(auto &out_img_refw : out_imgs){
        out_img_refw.get().images.emplace_back( *first_img_it );
        out_img_refw.get().images.back().fill_pixels(std::numeric_limits<double>::quiet_NaN());
    }

    auto ContrastInjectionLeadTime = user_data_s->ContrastInjectionLeadTime;

    //Get convenient handles for the arterial and venous input time courses.
    auto Carterial  = user_data_s->time_courses["Abdominal_Aorta"];
    auto dCarterial = user_data_s->time_course_derivatives["Abdominal_Aorta"];

    auto Cvenous    = user_data_s->time_courses["Hepatic_Portal_Vein"];
    auto dCvenous   = user_data_s->time_course_derivatives["Hepatic_Portal_Vein"];

    cAIF = &Carterial;
    cVIF = &Cvenous;

    dcAIF = &dCarterial;
    dcVIF = &dCvenous;




    //Trim all but the liver contour. 
    //
    //   TODO: hoist out of this function, and provide a convenience
    //         function called something like: Prune_Contours_Other_Than(cc_all, "Liver_Rough");
    //         You could do regex or whatever is needed.

    ccsl.remove_if([](std::reference_wrapper<contour_collection<double>> cc) -> bool {
                       const auto ROINameOpt = cc.get().contours.front().GetMetadataValueAs<std::string>("ROIName");
                       const auto ROIName = ROINameOpt.value();
                       //return (ROIName != "Suspected_Liver_Rough");
                       //return (ROIName != "Liver_Patches_For_Testing");
                       return (ROIName != "Liver_Patches_For_Testing_Smaller");
                   });


    //This routine performs a number of calculations. It is experimental and excerpts you plan to rely on should be
    // made into their own analysis functors.
    const bool InhibitSort = true; //Disable continuous sorting (defer to single sort later) to speed up data ingress.


    //Figure out if there are any contours for which are within the spatial extent of the image. 
    // There are many ways to do this! Since we are merely highlighting the contours, we scan 
    // all specified collections and treat them homogeneously.
    //
    // NOTE: We only bother to grab individual contours here. You could alter this if you wanted 
    //       each contour_collection's contours to have an identifying colour.
    //if(ccsl.empty()){
    if(ccsl.size() != 1){
        FUNCWARN("Missing needed contour information. Cannot continue with computation");
        return false;
    }

/*
    //typedef std::list<contour_of_points<double>>::iterator contour_iter;
    //std::list<contour_iter> rois;
    decltype(ccsl) rois;
    for(auto &ccs : ccsl){
        for(auto it =  ccs.get().contours.begin(); it != ccs.get().contours.end(); ++it){
            if(it->points.empty()) continue;
            //if(first_img_it->encompasses_contour_of_points(*it)) rois.push_back(it);
            if(first_img_it->encompasses_contour_of_points(*it)) rois.push_back(
        }
    }
*/

    //Loop over the rois, rows, columns, channels, and finally any selected images (if applicable).
    const auto row_unit   = first_img_it->row_unit;
    const auto col_unit   = first_img_it->col_unit;
    const auto ortho_unit = row_unit.Cross( col_unit ).unit();

    size_t Minimization_Failure_Count = 0;


    //Record the min and max actual pixel values for windowing purposes.
    Stats::Running_MinMax<float> minmax_k1A;
    Stats::Running_MinMax<float> minmax_tauA;
    Stats::Running_MinMax<float> minmax_k1V;
    Stats::Running_MinMax<float> minmax_tauV;
    Stats::Running_MinMax<float> minmax_k2;


    //Perform a loop with no payload to determine how many optimizations will be needed.
    //
    // NOTE: We expect optimization to take far longer than cycling through the contours and images.
    double Expected_Operation_Count = 0.0;
    for(auto &ccs : ccsl){
        for(auto roi_it = ccs.get().contours.begin(); roi_it != ccs.get().contours.end(); ++roi_it){
            if(roi_it->points.empty()) continue;
            if(! first_img_it->encompasses_contour_of_points(*roi_it)) continue;

            const auto ROIName =  roi_it->GetMetadataValueAs<std::string>("ROIName");
            if(!ROIName){
                FUNCWARN("Missing necessary tags for reporting analysis results. Cannot continue");
                return false;
            }
            
    /*
            //Construct a bounding box to reduce computational demand of checking every voxel.
            auto BBox = roi_it->Bounding_Box_Along(row_unit, 1.0);
            auto BBoxBestFitPlane = BBox.Least_Squares_Best_Fit_Plane(vec3<double>(0.0,0.0,1.0));
            auto BBoxProjectedContour = BBox.Project_Onto_Plane_Orthogonally(BBoxBestFitPlane);
            const bool BBoxAlreadyProjected = true;
    */
    
            //Prepare a contour for fast is-point-within-the-polygon checking.
            auto BestFitPlane = roi_it->Least_Squares_Best_Fit_Plane(ortho_unit);
            auto ProjectedContour = roi_it->Project_Onto_Plane_Orthogonally(BestFitPlane);
            const bool AlreadyProjected = true;
    
            for(auto row = 0; row < first_img_it->rows; ++row){
                for(auto col = 0; col < first_img_it->columns; ++col){
                    //Figure out the spatial location of the present voxel.
                    const auto point = first_img_it->position(row,col);
    
    /*
                    //Check if within the bounding box. It will generally be cheaper than the full contour (4 points vs. ? points).
                    auto BBoxProjectedPoint = BBoxBestFitPlane.Project_Onto_Plane_Orthogonally(point);
                    if(!BBoxProjectedContour.Is_Point_In_Polygon_Projected_Orthogonally(BBoxBestFitPlane,
                                                                                        BBoxProjectedPoint,
                                                                                        BBoxAlreadyProjected)) continue;
    */
    
                    //Perform a more detailed check to see if we are in the ROI.
                    auto ProjectedPoint = BestFitPlane.Project_Onto_Plane_Orthogonally(point);
                    if(ProjectedContour.Is_Point_In_Polygon_Projected_Orthogonally(BestFitPlane,
                                                                                   ProjectedPoint,
                                                                                   AlreadyProjected)){
                        for(auto chan = 0; chan < first_img_it->channels; ++chan){
   
                            Expected_Operation_Count += 1.0;
                        }//Loop over channels.
    
                    //If we're in the bounding box but not the ROI, do something.
                    }else{
                        //for(auto chan = 0; chan < first_img_it->channels; ++chan){
                        //    const auto curr_val = working.value(row, col, chan);
                        //    if(curr_val != 0) FUNCERR("There are overlapping ROI bboxes. This code currently cannot handle this. "
                        //                              "You will need to run the functor individually on the overlapping ROIs.");
                        //    working.reference(row, col, chan) = static_cast<float>(10);
                        //}
                    } // If is in ROI or ROI bbox.
                } //Loop over cols
            } //Loop over rows
        } //Loop over ROIs.
    } //Loop over contour_collections.



    //Loop over the ccsl, rois, rows, columns, channels, and finally any selected images (if applicable).
    //for(const auto &roi : rois){
    boost::posix_time::ptime start_t = boost::posix_time::microsec_clock::local_time();
    double Actual_Operation_Count = 0.0;
    for(auto &ccs : ccsl){
        for(auto roi_it = ccs.get().contours.begin(); roi_it != ccs.get().contours.end(); ++roi_it){
            if(roi_it->points.empty()) continue;
            //if(first_img_it->encompasses_contour_of_points(*it)) rois.push_back(it);

            //auto roi = *it;
            if(! first_img_it->encompasses_contour_of_points(*roi_it)) continue;

            const auto ROIName =  roi_it->GetMetadataValueAs<std::string>("ROIName");
            if(!ROIName){
                FUNCWARN("Missing necessary tags for reporting analysis results. Cannot continue");
                return false;
            }
            
    /*
            //Try figure out the contour's name.
            const auto StudyInstanceUID = roi_it->GetMetadataValueAs<std::string>("StudyInstanceUID");
            const auto ROIName =  roi_it->GetMetadataValueAs<std::string>("ROIName");
            const auto FrameofReferenceUID = roi_it->GetMetadataValueAs<std::string>("FrameofReferenceUID");
            if(!StudyInstanceUID || !ROIName || !FrameofReferenceUID){
                FUNCWARN("Missing necessary tags for reporting analysis results. Cannot continue");
                return false;
            }
            const analysis_key_t BaseAnalysisKey = { {"StudyInstanceUID", StudyInstanceUID.value()},
                                                     {"ROIName", ROIName.value()},
                                                     {"FrameofReferenceUID", FrameofReferenceUID.value()},
                                                     {"SpatialBoxr", Xtostring(boxr)},
                                                     {"MinimumDatum", Xtostring(min_datum)} };
            //const auto ROIName = ReplaceAllInstances(roi_it->metadata["ROIName"], "[_]", " ");
            //const auto ROIName = roi_it->metadata["ROIName"];
    */
    
    /*
            //Construct a bounding box to reduce computational demand of checking every voxel.
            auto BBox = roi_it->Bounding_Box_Along(row_unit, 1.0);
            auto BBoxBestFitPlane = BBox.Least_Squares_Best_Fit_Plane(vec3<double>(0.0,0.0,1.0));
            auto BBoxProjectedContour = BBox.Project_Onto_Plane_Orthogonally(BBoxBestFitPlane);
            const bool BBoxAlreadyProjected = true;
    */
    
            //Prepare a contour for fast is-point-within-the-polygon checking.
            auto BestFitPlane = roi_it->Least_Squares_Best_Fit_Plane(ortho_unit);
            auto ProjectedContour = roi_it->Project_Onto_Plane_Orthogonally(BestFitPlane);
            const bool AlreadyProjected = true;
    
            for(auto row = 0; row < first_img_it->rows; ++row){
                for(auto col = 0; col < first_img_it->columns; ++col){
                    //Figure out the spatial location of the present voxel.
                    const auto point = first_img_it->position(row,col);
    
    /*
                    //Check if within the bounding box. It will generally be cheaper than the full contour (4 points vs. ? points).
                    auto BBoxProjectedPoint = BBoxBestFitPlane.Project_Onto_Plane_Orthogonally(point);
                    if(!BBoxProjectedContour.Is_Point_In_Polygon_Projected_Orthogonally(BBoxBestFitPlane,
                                                                                        BBoxProjectedPoint,
                                                                                        BBoxAlreadyProjected)) continue;
    */
    
                    //Perform a more detailed check to see if we are in the ROI.
                    auto ProjectedPoint = BestFitPlane.Project_Onto_Plane_Orthogonally(point);
                    if(ProjectedContour.Is_Point_In_Polygon_Projected_Orthogonally(BestFitPlane,
                                                                                   ProjectedPoint,
                                                                                   AlreadyProjected)){
                        for(auto chan = 0; chan < first_img_it->channels; ++chan){

                            //Provide a prediction time.
                            if(Actual_Operation_Count > 0.5){
                                boost::posix_time::ptime current_t = boost::posix_time::microsec_clock::local_time();
                                auto elapsed_dt = (current_t - start_t).total_milliseconds();
                                auto expected_dt_f = static_cast<double>(elapsed_dt) * (Expected_Operation_Count/Actual_Operation_Count);
                                auto expected_dt = static_cast<long int>(expected_dt_f);
                                boost::posix_time::ptime predicted_dt( start_t + boost::posix_time::milliseconds(expected_dt) );
                                FUNCINFO("Progress: " 
                                    << Actual_Operation_Count << "/" << Expected_Operation_Count << " = " 
                                    << static_cast<double>(static_cast<size_t>(1000.0*Actual_Operation_Count/Expected_Operation_Count))/10.0 
                                    << "%. Expected finish time: " << predicted_dt);
                            }
                            Actual_Operation_Count += 1.0;

                            //Cycle over the grouped images (temporal slices, or whatever the user has decided).
                            // Harvest the time course or any other voxel-specific numbers.
                            samples_1D<double> channel_time_course;
                            channel_time_course.uncertainties_known_to_be_independent_and_random = true;
                            for(auto & img_it : selected_img_its){
                                //Collect the datum of voxels and nearby voxels for an average.
                                std::list<double> in_pixs;
                                const auto boxr = 3;
                                const auto min_datum = 1;
    
                                for(auto lrow = (row-boxr); lrow <= (row+boxr); ++lrow){
                                    for(auto lcol = (col-boxr); lcol <= (col+boxr); ++lcol){
                                        //Check if the coordinates are legal and in the ROI.
                                        if( !isininc(0,lrow,img_it->rows-1) || !isininc(0,lcol,img_it->columns-1) ) continue;
    
                                        //const auto boxpoint = first_img_it->spatial_location(row,col);  //For standard contours(?).
                                        //const auto neighbourpoint = vec3<double>(lrow*1.0, lcol*1.0, SliceLocation*1.0);  //For the pixel integer contours.
                                        const auto neighbourpoint = first_img_it->position(lrow,lcol);
                                        auto ProjectedNeighbourPoint = BestFitPlane.Project_Onto_Plane_Orthogonally(neighbourpoint);
                                        if(!ProjectedContour.Is_Point_In_Polygon_Projected_Orthogonally(BestFitPlane,
                                                                                                        ProjectedNeighbourPoint,
                                                                                                        AlreadyProjected)) continue;
                                        const auto val = static_cast<double>(img_it->value(lrow, lcol, chan));
                                        in_pixs.push_back(val);
                                    }
                                }
                                auto dt = img_it->GetMetadataValueAs<double>("dt");
                                if(!dt) FUNCERR("Image is missing time metadata. Bailing");

                                const auto avg_val = Stats::Mean(in_pixs);
                                if(in_pixs.size() < min_datum) continue; //If contours are too narrow so that there is too few datum for meaningful results.
                                //const auto avg_val_sigma = std::sqrt(Stats::Unbiased_Var_Est(in_pixs))/std::sqrt(1.0*in_pixs.size());
                                //channel_time_course.push_back(dt.value(), 0.0, avg_val, avg_val_sigma, InhibitSort);
                                channel_time_course.push_back(dt.value(), 0.0, avg_val, 0.0, InhibitSort);
                            }
                            channel_time_course.stable_sort();
                            if(channel_time_course.empty()) continue;
  
                            //Correct any unaccounted-for contrast enhancement shifts. 
                            // (If we don't do this, the optimizer goes crazy!)
                            if(true){
                                //Subtract the minimum over the full time course.
                                if(false){
                                    const auto Cmin = channel_time_course.Get_Extreme_Datum_y().first;
                                    channel_time_course = channel_time_course.Sum_With(0.0-Cmin[2]);

                                //Subtract the mean from the pre-injection period.
                                }else{
                                    const auto preinject = channel_time_course.Select_Those_Within_Inc(-1E99,ContrastInjectionLeadTime);
                                    const auto themean = preinject.Mean_y()[0];
                                    channel_time_course = channel_time_course.Sum_With(0.0-themean);
                                }
                            }


                            //==============================================================================
                            //Fit the model.

                            theROI = &channel_time_course;

                            const int dimen = 5;
                            //Fitting parameters:      k1A,  tauA,   k1V,  tauV,  k2.
                            // The following are arbitrarily chosen. They should be seeded from previous computations, or
                            // at least be nominal values reported within the literature.
                            double params[dimen] = {  0.0057,  2.87,  0.0052,  -14.4,  0.033 };

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
                            if(NLOPT_SUCCESS != nlopt_set_min_objective(opt, func_to_min, nullptr)){
                                FUNCERR("NLOpt unable to set objective function for minimization");
                            }
                            if(NLOPT_SUCCESS != nlopt_set_xtol_rel(opt, 1.0E-4)){
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
                            if(opt_status < 0){
                                ++Minimization_Failure_Count;
                                if(opt_status == NLOPT_FAILURE){                FUNCWARN("NLOpt fail: generic failure");
                                }else if(opt_status == NLOPT_INVALID_ARGS){     FUNCERR("NLOpt fail: invalid arguments");
                                }else if(opt_status == NLOPT_OUT_OF_MEMORY){    FUNCWARN("NLOpt fail: out of memory");
                                }else if(opt_status == NLOPT_ROUNDOFF_LIMITED){ FUNCWARN("NLOpt fail: roundoff limited");
                                }else if(opt_status == NLOPT_FORCED_STOP){      FUNCWARN("NLOpt fail: forced termination");
                                }else{ FUNCERR("NLOpt fail: unrecognized error code"); }
                                // See http://ab-initio.mit.edu/wiki/index.php/NLopt_Reference for error code info.
                            }else if(true){
                                if(opt_status == NLOPT_SUCCESS){                FUNCINFO("NLOpt: success");
                                }else if(opt_status == NLOPT_STOPVAL_REACHED){  FUNCINFO("NLOpt: stopval reached");
                                }else if(opt_status == NLOPT_FTOL_REACHED){     FUNCINFO("NLOpt: ftol reached");
                                }else if(opt_status == NLOPT_XTOL_REACHED){     FUNCINFO("NLOpt: xtol reached");
                                }else if(opt_status == NLOPT_MAXEVAL_REACHED){  FUNCINFO("NLOpt: maxeval count reached");
                                }else if(opt_status == NLOPT_MAXTIME_REACHED){  FUNCINFO("NLOpt: maxtime reached");
                                }else{ FUNCERR("NLOpt fail: unrecognized success code"); }
                            }

                            nlopt_destroy(opt);
                            // ----------------------------------------------------------------------------

                            const double k1A  = params[0];
                            const double tauA = params[1];
                            const double k1V  = params[2];
                            const double tauV = params[3];
                            const double k2   = params[4];
                            if(true) FUNCINFO("k1A,tauA,k1V,tauV,k2 = " << k1A << ", " << tauA << ", " << k1V << ", " << tauV << ", " << k2);

                            const auto LiverPerfusion = (k1A + k1V);
                            const auto MeanTransitTime = 1.0 / k2;
                            const auto ArterialFraction = 100.0 * k1A / LiverPerfusion;
                            const auto DistributionVolume = 100.0 * LiverPerfusion * MeanTransitTime;

                            //==============================================================================
 
                            //Update pixel values.
                            const auto k1A_f  = static_cast<float>(k1A);
                            const auto tauA_f = static_cast<float>(tauA);
                            const auto k1V_f  = static_cast<float>(k1V);
                            const auto tauV_f = static_cast<float>(tauV);
                            const auto k2_f   = static_cast<float>(k2);

                            minmax_k1A.Digest(k1A_f);
                            minmax_tauA.Digest(tauA_f);
                            minmax_k1V.Digest(k1V_f);
                            minmax_tauV.Digest(tauV_f);
                            minmax_k2.Digest(k2_f);

                            auto out_img_refw_it = out_imgs.begin();
                            std::next(out_img_refw_it,0)->get().images.back().reference(row, col, chan) = k1A_f;
                            std::next(out_img_refw_it,1)->get().images.back().reference(row, col, chan) = tauA_f;
                            std::next(out_img_refw_it,2)->get().images.back().reference(row, col, chan) = k1V_f;
                            std::next(out_img_refw_it,3)->get().images.back().reference(row, col, chan) = tauV_f;
                            std::next(out_img_refw_it,4)->get().images.back().reference(row, col, chan) = k2_f;

                            // ----------------------------------------------------------------------------
    
                        }//Loop over channels.
    
                    //If we're in the bounding box but not the ROI, do something.
                    }else{
                        //for(auto chan = 0; chan < first_img_it->channels; ++chan){
                        //    const auto curr_val = working.value(row, col, chan);
                        //    if(curr_val != 0) FUNCERR("There are overlapping ROI bboxes. This code currently cannot handle this. "
                        //                              "You will need to run the functor individually on the overlapping ROIs.");
                        //    working.reference(row, col, chan) = static_cast<float>(10);
                        //}
                    } // If is in ROI or ROI bbox.
                } //Loop over cols
            } //Loop over rows
        } //Loop over ROIs.
    } //Loop over contour_collections.

    FUNCWARN("Minimization failure count: " << Minimization_Failure_Count);

    //Alter the first image's metadata to reflect that averaging has occurred. You might want to consider
    // a selective whitelist approach so that unique IDs are not duplicated accidentally.

    {
        auto it = out_imgs.begin();
        auto out_img_refw = std::ref( it->get().images.back() );
        UpdateImageDescription( out_img_refw, "Liver Pharmaco: k1A" );
        UpdateImageWindowCentreWidth( out_img_refw, minmax_k1A );

        ++it;
        out_img_refw = std::ref( it->get().images.back() );
        UpdateImageDescription( out_img_refw, "Liver Pharmaco: tauA" );
        UpdateImageWindowCentreWidth( out_img_refw, minmax_tauA );

        ++it;
        out_img_refw = std::ref( it->get().images.back() );
        UpdateImageDescription( out_img_refw, "Liver Pharmaco: k1V" );
        UpdateImageWindowCentreWidth( out_img_refw, minmax_k1V );

        ++it;
        out_img_refw = std::ref( it->get().images.back() );
        UpdateImageDescription( out_img_refw, "Liver Pharmaco: tauV" );
        UpdateImageWindowCentreWidth( out_img_refw, minmax_tauV );

        ++it;
        out_img_refw = std::ref( it->get().images.back() );
        UpdateImageDescription( out_img_refw, "Liver Pharmaco: k2" );
        UpdateImageWindowCentreWidth( out_img_refw, minmax_k2 );
    }

    return true;
}

