
#include <list>
#include <functional>
#include <limits>
#include <map>
#include <set>
#include <cmath>
#include <memory>
#include <chrono>
#include <experimental/any>
#include <mutex>
#include <tuple>
#include <regex>

#include <pqxx/pqxx>         //PostgreSQL C++ interface.
#include <jansson.h>         //For JSON handling.

#include <nlopt.h>

#include <boost/date_time/posix_time/posix_time.hpp>

#include "YgorMisc.h"
#include "YgorMath.h"
#include "YgorMathChebyshev.h"
#include "YgorMathChebyshevFunctions.h"
#include "YgorMathPlottingGnuplot.h" //Needed for YgorMathPlottingGnuplot::*.
#include "YgorImages.h"
#include "YgorStats.h"       //Needed for Stats:: namespace.
#include "YgorFilesDirs.h"   //Needed for Does_File_Exist_And_Can_Be_Read(...), etc..
#include "YgorAlgorithms.h"  //Needed for For_Each_In_Parallel<..>(...)
#include "YgorString.h"      //Needed for GetFirstRegex(...)

#include "../../Common_Boost_Serialization.h"
#include "../../Common_Plotting.h"

#include "../ConvenienceRoutines.h"
#include "Liver_Pharmacokinetic_Model_5Param_Cheby.h"

#include "../../Pharmacokinetic_Modeling_via_Optimization.h"
#include "../../Pharmacokinetic_Modeling_via_Least_Squares.h"

std::mutex out_img_mutex;

bool
LiverPharmacoModel5ParamCheby(planar_image_collection<float,double>::images_list_it_t first_img_it,
                        std::list<planar_image_collection<float,double>::images_list_it_t> selected_img_its,
                        std::list<std::reference_wrapper<planar_image_collection<float,double>>> out_imgs,
                        std::list<std::reference_wrapper<contour_collection<double>>> cc_all,
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

    if( (user_data_s->time_courses.count("AIF") == 0 )
    ||  (user_data_s->time_courses.count("VIF") == 0 ) ){
        throw std::invalid_argument("Both arterial and venous input time courses are needed."
                                    "(Are they named differently to the hard-coded names?)");
    }

    //Figure out which pixels, if any, need to be plotted after modeling.
    std::set<std::pair<long int, long int>> PixelsToPlot;
    {
        auto inimg_metadata = first_img_it->metadata;
        for(const auto &critstruct : user_data_s->pixels_to_plot){
            bool should_add = true;
            for(const auto &crit : critstruct.metadata_criteria){
                if((inimg_metadata.count(crit.first) != 1) || !(std::regex_match(inimg_metadata[crit.first], crit.second))){
                    should_add = false;
                    break;
                }
            }
            if(should_add) PixelsToPlot.insert({ critstruct.row, critstruct.column });
        }
    }


    //Copy the incoming image to all the parameter maps. We will overwrite pixels as necessary.
    std::reference_wrapper<planar_image<float,double>> out_img_k1A  = std::ref( *first_img_it ); //Temporary refs.
    std::reference_wrapper<planar_image<float,double>> out_img_tauA = std::ref( *first_img_it );
    std::reference_wrapper<planar_image<float,double>> out_img_k1V  = std::ref( *first_img_it );
    std::reference_wrapper<planar_image<float,double>> out_img_tauV = std::ref( *first_img_it );
    std::reference_wrapper<planar_image<float,double>> out_img_k2   = std::ref( *first_img_it );

    {
        std::lock_guard<std::mutex> guard(out_img_mutex);

        std::next(out_imgs.begin(),0)->get().images.emplace_back( *first_img_it );
        std::next(out_imgs.begin(),1)->get().images.emplace_back( *first_img_it );
        std::next(out_imgs.begin(),2)->get().images.emplace_back( *first_img_it );
        std::next(out_imgs.begin(),3)->get().images.emplace_back( *first_img_it );
        std::next(out_imgs.begin(),4)->get().images.emplace_back( *first_img_it );

        out_img_k1A  = std::ref( std::next(out_imgs.begin(),0)->get().images.back() );
        out_img_tauA = std::ref( std::next(out_imgs.begin(),1)->get().images.back() );
        out_img_k1V  = std::ref( std::next(out_imgs.begin(),2)->get().images.back() );
        out_img_tauV = std::ref( std::next(out_imgs.begin(),3)->get().images.back() );
        out_img_k2   = std::ref( std::next(out_imgs.begin(),4)->get().images.back() );

        out_img_k1A.get().fill_pixels(std::numeric_limits<double>::quiet_NaN());
        out_img_tauA.get().fill_pixels(std::numeric_limits<double>::quiet_NaN());
        out_img_k1V.get().fill_pixels(std::numeric_limits<double>::quiet_NaN());
        out_img_tauV.get().fill_pixels(std::numeric_limits<double>::quiet_NaN());
        out_img_k2.get().fill_pixels(std::numeric_limits<double>::quiet_NaN());
    }

    auto ContrastInjectionLeadTime = user_data_s->ContrastInjectionLeadTime;

    //Get convenient handles for the arterial and venous input time courses.
    auto Carterial  = std::make_shared<cheby_approx<double>>( user_data_s->time_courses["AIF"] );
    auto dCarterial = std::make_shared<cheby_approx<double>>( user_data_s->time_course_derivatives["AIF"] );

    auto Cvenous  = std::make_shared<cheby_approx<double>>( user_data_s->time_courses["VIF"] );
    auto dCvenous = std::make_shared<cheby_approx<double>>( user_data_s->time_course_derivatives["VIF"] );

    //Pharmacokinetic_Parameters_5Param_Chebyshev_Optimization model_state;
    Pharmacokinetic_Parameters_5Param_Chebyshev_Least_Squares model_state;

    model_state.cAIF  = Carterial;
    model_state.dcAIF = dCarterial;
    model_state.cVIF  = Cvenous;;
    model_state.dcVIF = dCvenous;


    //Trim all but the ROIs we are interested in.
    auto cc_ROIs = cc_all;
    cc_ROIs.remove_if([=](std::reference_wrapper<contour_collection<double>> cc) -> bool {
                   const auto ROINameOpt = cc.get().contours.front().GetMetadataValueAs<std::string>("ROIName");
                   if(!ROINameOpt) return true; //Remove those without a name.
                   const auto ROIName = ROINameOpt.value();
                   return !(std::regex_match(ROIName,user_data_s->TargetROIs));
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
    //if(cc_ROIs.empty()){
    if(cc_ROIs.size() != 1){
        FUNCWARN("Missing needed contour information. Cannot continue with computation");
        return false;
    }

/*
    //typedef std::list<contour_of_points<double>>::iterator contour_iter;
    //std::list<contour_iter> rois;
    decltype(cc_ROIs) rois;
    for(auto &ccs : cc_ROIs){
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
    for(auto &ccs : cc_ROIs){
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



    //Loop over the cc_ROIs, rois, rows, columns, channels, and finally any selected images (if applicable).
    //for(const auto &roi : rois){
    boost::posix_time::ptime start_t = boost::posix_time::microsec_clock::local_time();
    double Actual_Operation_Count = 0.0;
    for(auto &ccs : cc_ROIs){
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
                            auto channel_time_course = std::make_shared<samples_1D<double>>();
                            channel_time_course->uncertainties_known_to_be_independent_and_random = true;
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
                                channel_time_course->push_back(dt.value(), 0.0, avg_val, 0.0, InhibitSort);
                            }
                            channel_time_course->stable_sort();
                            if(channel_time_course->empty()) continue;
  
                            //Correct any unaccounted-for contrast enhancement shifts. 
                            // (If we don't do this, the optimizer goes crazy!)
                            if(true){
                                //Subtract the minimum over the full time course.
                                if(false){
                                    const auto Cmin = channel_time_course->Get_Extreme_Datum_y().first;
                                    *channel_time_course = channel_time_course->Sum_With(0.0-Cmin[2]);

                                //Subtract the mean from the pre-injection period.
                                }else{
                                    const auto preinject = channel_time_course->Select_Those_Within_Inc(-1E99,ContrastInjectionLeadTime);
                                    const auto themean = preinject.Mean_y()[0];
                                    *channel_time_course = channel_time_course->Sum_With(0.0-themean);
                                }
                            }


                            //==============================================================================
                            //Fit the model.

                            // This routine fits a pharmacokinetic model to the observed liver perfusion data using a 
                            // Chebyshev polynomial approximation scheme.

                            model_state.FittingPerformed = false;
                            model_state.cROI = channel_time_course;
                            model_state.k1A  = std::numeric_limits<double>::quiet_NaN();
                            model_state.tauA = std::numeric_limits<double>::quiet_NaN();
                            model_state.k1V  = std::numeric_limits<double>::quiet_NaN();
                            model_state.tauV = std::numeric_limits<double>::quiet_NaN();
                            model_state.k2   = std::numeric_limits<double>::quiet_NaN();

                            //Pharmacokinetic_Parameters_5Param_Chebyshev_Optimization after_state = Pharmacokinetic_Model_3Param_Chebyshev_Optimization(model_state);
                            //Pharmacokinetic_Parameters_5Param_Chebyshev_Optimization after_state = Pharmacokinetic_Model_5Param_Chebyshev_Optimization(model_state);
                            //Pharmacokinetic_Parameters_5Param_Chebyshev_Least_Squares after_state = Pharmacokinetic_Model_3Param_Chebyshev_Least_Squares(model_state);
                            Pharmacokinetic_Parameters_5Param_Chebyshev_Least_Squares after_state = Pharmacokinetic_Model_5Param_Chebyshev_Least_Squares(model_state);

                            if(!after_state.FittingSuccess) ++Minimization_Failure_Count;

                            const double RSS  = after_state.RSS;
                            const double k1A  = after_state.k1A;
                            const double tauA = after_state.tauA;
                            const double k1V  = after_state.k1V;
                            const double tauV = after_state.tauV;
                            const double k2   = after_state.k2;
                            if(true) FUNCINFO("k1A,tauA,k1V,tauV,k2,RSS = " << k1A << ", " << tauA << ", " 
                                              << k1V << ", " << tauV << ", " << k2 << ", " << RSS);

                            const auto LiverPerfusion = (k1A + k1V);
                            const auto MeanTransitTime = 1.0 / k2;
                            const auto ArterialFraction = 100.0 * k1A / LiverPerfusion;
                            const auto DistributionVolume = 100.0 * LiverPerfusion * MeanTransitTime;

                            //==============================================================================
                            // Plot the fitted model with the ROI time course.
                            if(PixelsToPlot.count( {row, col}) != 0){ 
                                std::map<std::string, samples_1D<double>> time_courses;
                                std::string title;
                                //Add the ROI.
                                title = "ROI time course: row = " + std::to_string(row) + ", col = " + std::to_string(col);
                                time_courses[title] = *(after_state.cROI);
                                samples_1D<double> fitted_model;
                                //Pharmacokinetic_Parameters_5Param_Chebyshev_Optimization_Results eval_res;
                                Pharmacokinetic_Parameters_5Param_Chebyshev_Least_Squares_Results eval_res;
                                for(const auto &P : after_state.cROI->samples){
                                    const double t = P[0];
                                    //chebyshev_5param_model_optimization(after_state,t,eval_res);
                                    chebyshev_5param_model_least_squares(after_state,t,eval_res);
                                    fitted_model.push_back(t, 0.0, eval_res.I, 0.0);
                                }
                                title = "Fitted model";
                                time_courses[title] = fitted_model;

                                PlotTimeCourses("Raw ROI and Fitted Model", time_courses, {});
                            }
                            
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

                            {
                                out_img_k1A.get().reference(row, col, chan)  = k1A_f;
                                out_img_tauA.get().reference(row, col, chan) = tauA_f;
                                out_img_k1V.get().reference(row, col, chan)  = k1V_f;
                                out_img_tauV.get().reference(row, col, chan) = tauV_f;
                                out_img_k2.get().reference(row, col, chan)   = k2_f;
                            }
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


    //Serialize the state so we have enough info to apply the model later. But remove the per-voxel information (which
    // we can get through the parameter maps).
    model_state.cROI.reset();
    model_state.k1A  = std::numeric_limits<double>::quiet_NaN();
    model_state.tauA = std::numeric_limits<double>::quiet_NaN();
    model_state.k1V  = std::numeric_limits<double>::quiet_NaN();
    model_state.tauV = std::numeric_limits<double>::quiet_NaN();
    model_state.k2   = std::numeric_limits<double>::quiet_NaN();
    model_state.RSS  = std::numeric_limits<double>::quiet_NaN();

    const std::string ModelState = Serialize_Pharmacokinetic_Parameters_5Param_Chebyshev_Least_Squares_State(model_state);

    //Alter the first image's metadata to reflect that averaging has occurred. You might want to consider
    // a selective whitelist approach so that unique IDs are not duplicated accidentally.

    UpdateImageDescription( out_img_k1A, "Liver Pharmaco: k1A" );
    UpdateImageWindowCentreWidth( out_img_k1A, minmax_k1A );
    out_img_k1A.get().metadata["ModelState"] = ModelState;

    UpdateImageDescription( out_img_tauA, "Liver Pharmaco: tauA" );
    UpdateImageWindowCentreWidth( out_img_tauA, minmax_tauA );
    out_img_tauA.get().metadata["ModelState"] = ModelState;

    UpdateImageDescription( out_img_k1V, "Liver Pharmaco: k1V" );
    UpdateImageWindowCentreWidth( out_img_k1V, minmax_k1V );
    out_img_k1V.get().metadata["ModelState"] = ModelState;

    UpdateImageDescription( out_img_tauV, "Liver Pharmaco: tauV" );
    UpdateImageWindowCentreWidth( out_img_tauV, minmax_tauV );
    out_img_tauV.get().metadata["ModelState"] = ModelState;

    UpdateImageDescription( out_img_k2, "Liver Pharmaco: k2" );
    UpdateImageWindowCentreWidth( out_img_k2, minmax_k2 );
    out_img_k2.get().metadata["ModelState"] = ModelState;
    

    return true;
}

