
#include <list>
#include <functional>
#include <limits>
#include <map>
#include <cmath>
#include <experimental/any>

#include <pqxx/pqxx>         //PostgreSQL C++ interface.
#include <jansson.h>         //For JSON handling.

#include <nlopt.h>

#include "YgorMisc.h"
#include "YgorMath.h"
#include "YgorImages.h"
#include "YgorStats.h"       //Needed for Stats:: namespace.
#include "YgorFilesDirs.h"   //Needed for Does_File_Exist_And_Can_Be_Read(...), etc..
#include "YgorAlgorithms.h"  //Needed for For_Each_In_Parallel<..>(...)
#include "YgorString.h"      //Needed for GetFirstRegex(...)
#include "YgorPlot.h"

#include "../ConvenienceRoutines.h"

#include "Liver_Pharmacokinetic_Model_5Param_Structs.h"
#include "Liver_Pharmacokinetic_Model_5Param_Linear.h"

#include "../Compute/Per_ROI_Time_Courses.h"
//struct ComputePerROITimeCoursesUserData;


//This is the function we will minimize: the sum of the squared residuals between the ROI measured
// concentrations and the total concentration predicted by the fitted model.
static samples_1D<double> *theAIF;    //Global, but only so we can pass a c-style function to nlopt.
static samples_1D<double> *theVIF;    //Global, but only so we can pass a c-style function to nlopt.
static samples_1D<double> *theROI;    //Global, but only so we can pass a c-style function to nlopt.

static
double 
func_to_min(unsigned, const double *params, double *grad, void *){
    if(grad != nullptr) FUNCERR("NLOpt asking for a gradient. We don't have it at the moment...");
    const double k1A  = params[0];
    const double tauA = params[1];
    const double k1V  = params[2];
    const double tauV = params[3];
    const double k2   = params[4];

    double sqDist = 0.0;
    for(const auto &P : theROI->samples){
        const double t = P[0];
        const double Fexpdata = P[2];
        //double Ffitfunc = 0.0;

        //-------------------------------------------------------------------------------------------------------------------------
        //First, the arterial contribution. This involves an integral over the AIF.
        // Compute: \int_{tau=0}^{tau=t} k1A * AIF(tau - tauA) * exp((k2)*(tau-t)) dtau 
        //          = k1A \int_{tau=-tauA}^{tau=(t-tauA)} AIF(tau) * exp((k2)*(tau-(t-tauA))) dtau.
        // The integration coordinate is transformed to make it suit the integration-over-kernel-... implementation. 
        //
        double Integratedk1ACA = k1A*theAIF->Integrate_Over_Kernel_exp(0.0-tauA, t-tauA, {k2,0.0}, {-(t-tauA),0.0})[0];
        
        //-------------------------------------------------------------------------------------------------------------------------
        //The venous contribution is identical, but all the fitting parameters are different and AIF -> VIF.
        //
        double Integratedk1VCV = k1V*theVIF->Integrate_Over_Kernel_exp(0.0-tauV, t-tauV, {k2,0.0}, {-(t-tauV),0.0})[0];
        const double Ffitfunc = Integratedk1ACA + Integratedk1VCV;
 
        //Standard L2-norm.
        sqDist += std::pow(Fexpdata - Ffitfunc, 2.0);
    }
    return sqDist;
}
 



bool 
KineticModel_Liver_1C2I_5Param_Linear(planar_image_collection<float,double>::images_list_it_t first_img_it,
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
    ComputePerROITimeCoursesUserData *user_data_s;
    try{
        user_data_s = std::experimental::any_cast<ComputePerROITimeCoursesUserData *>(user_data);
    }catch(const std::exception &e){
        FUNCWARN("Unable to cast user_data to appropriate format. Cannot continue with computation");
        return false;
    }

    if( (user_data_s->time_courses.count("AIF") == 0 )
    ||  (user_data_s->time_courses.count("VIF") == 0 ) ){
        throw std::invalid_argument("Both arterial and venous input time courses are needed."
                                    "(Are they named differently to the hard-coded names?)");
    }

    //Copy the incoming image to all the parameter maps. We will overwrite pixels as necessary.
    for(auto &out_img_refw : out_imgs){
        out_img_refw.get().images.emplace_back( *first_img_it );
        out_img_refw.get().images.back().fill_pixels(std::numeric_limits<double>::quiet_NaN());
    }

    //Get convenient handles for the arterial and venous input time courses.
    //
    // NOTE: "Because the contrast agent does not enter the RBCs, the time series Caorta(t) and Cportal(t)
    //       were divided by one minus the hematocrit." (From Van Beers et al. 2000.)
    auto Carterial = user_data_s->time_courses["AIF"].Multiply_With(1.0/(1.0 - 0.42));
    auto Cvenous   = user_data_s->time_courses["VIF"].Multiply_With(1.0/(1.0 - 0.42));

    theAIF = &Carterial;
    theVIF = &Cvenous;

    //Trim all but the liver contour. 
    //
    //   TODO: hoist out of this function, and provide a convenience
    //         function called something like: Prune_Contours_Other_Than(cc_all, "Liver_Rough");
    //         You could do regex or whatever is needed.

    ccsl.remove_if([](std::reference_wrapper<contour_collection<double>> cc) -> bool {
                       const auto ROINameOpt = cc.get().contours.front().GetMetadataValueAs<std::string>("ROIName");
                       const auto ROIName = ROINameOpt.value();
                       return (ROIName != "Suspected_Liver_Rough");
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

    //Loop over the ccsl, rois, rows, columns, channels, and finally any selected images (if applicable).
    //for(const auto &roi : rois){
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
                            //Cycle over the grouped images (temporal slices, or whatever the user has decided).
                            // Harvest the time course or any other voxel-specific numbers.
                            samples_1D<double> channel_time_course;
                            channel_time_course.uncertainties_known_to_be_independent_and_random = true;
                            for(auto & img_it : selected_img_its){
                                //Collect the datum of voxels and nearby voxels for an average.
                                std::list<double> in_pixs;
                                const auto boxr = 0;
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
   
                            //==============================================================================
                            //Fit the model.

                            //theAIF = &Carterial;
                            //theVIF = &Cvenous;
                            theROI = &channel_time_course;

                            const int dimen = 5;
                            //Fitting parameters:      k1A,  tauA,   k1V,  tauV,  k2.
                            // The following are arbitrarily chosen. They should be seeded from previous computations, or
                            // at least be nominal values reported within the literature.
                            double params[dimen] = {  0.05,  10.0,  0.05,  10.0,  0.5 };

                            // U/L bounds:             k1A,  tauA,  k1V,  tauV,  k2.
                            //double l_bnds[dimen] = {   0.0, -20.0,  0.0, -20.0,  0.0 };
                            //double u_bnds[dimen] = {   1.0,   5.0,  1.0,   5.0,  1.0 };
                    
                            nlopt_opt opt;
                            opt = nlopt_create(NLOPT_LN_COBYLA, dimen);
                            //opt = nlopt_create(NLOPT_LN_BOBYQA, dimen);
                            //opt = nlopt_create(NLOPT_LN_SBPLX, dimen);
                    
                            //opt = nlopt_create(NLOPT_GN_DIRECT, dimen);
                            //opt = nlopt_create(NLOPT_GN_CRS2_LM, dimen);
                            //opt = nlopt_create(NLOPT_GN_ESCH, dimen);
                            //opt = nlopt_create(NLOPT_GN_ISRES, dimen);
                    
                            //nlopt_set_lower_bounds(opt, l_bnds);
                            //nlopt_set_upper_bounds(opt, u_bnds);
                    
                            nlopt_set_min_objective(opt, func_to_min, nullptr);
                            //nlopt_set_xtol_rel(opt, 1.0E-3);
                            nlopt_set_ftol_rel(opt, 1.0E-7);
                            nlopt_set_maxtime(opt, 30.0); // In seconds.
                            nlopt_set_maxeval(opt, 5'000'000); // Maximum number of times to evalute cost function.

                            double func_min;
                            const auto opt_status = nlopt_optimize(opt, params, &func_min);
                            if(opt_status < 0){
                                ++Minimization_Failure_Count;
                                if(opt_status == NLOPT_FAILURE){                FUNCWARN("NLOpt fail: generic failure");
                                }else if(opt_status == NLOPT_INVALID_ARGS){     FUNCERR("NLOpt fail: invalid arguments");
                                }else if(opt_status == NLOPT_OUT_OF_MEMORY){    FUNCWARN("NLOpt fail: out of memory");
                                }else if(opt_status == NLOPT_ROUNDOFF_LIMITED){ FUNCWARN("NLOpt fail: roundoff limited");
                                }else if(opt_status == NLOPT_FORCED_STOP){      FUNCWARN("NLOpt fail: forced termination");
                                }else{ FUNCERR("NLOpt fail: unrecognized error"); }
                                // See http://ab-initio.mit.edu/wiki/index.php/NLopt_Reference for error code info.
                            }
                            nlopt_destroy(opt);

                            const double k1A  = params[0];
                            const double tauA = params[1];
                            const double k1V  = params[2];
                            const double tauV = params[3];
                            const double k2   = params[4];

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

