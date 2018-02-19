
#include <list>
#include <functional>
#include <limits>
#include <map>
#include <cmath>

#include <pqxx/pqxx>         //PostgreSQL C++ interface.
#include <jansson.h>         //For JSON handling.

#include "YgorMisc.h"
#include "YgorMath.h"
#include "YgorImages.h"
#include "YgorStats.h"       //Needed for Stats:: namespace.
#include "YgorFilesDirs.h"   //Needed for Does_File_Exist_And_Can_Be_Read(...), etc..
#include "YgorAlgorithms.h"  //Needed for For_Each_In_Parallel<..>(...)
#include "YgorString.h"      //Needed for GetFirstRegex(...)
#include "YgorPlot.h"

#include "../ConvenienceRoutines.h"


const auto boxr = 2; //The inclusive 'radius' of the square box to use to average nearby pixels. Controls amount of spatial averaging.
const auto min_datum = 3; //The minimum number of nearby pixels needed to proceed with each average/variance estimate/etc.. 
                          // Note this is very sensistive to boxr. If boxr = 1 the max min_datum is 5. If boxr = 2 the max min_datum 
                          // is 13. In general, it is best to keep it at 3 or maybe 5 if you want to be extra precise about interpreting
                          // variance estimates.

typedef std::map<std::string,std::string> analysis_key_t;

static std::map<analysis_key_t, samples_1D<double>> sum_win_var;

static std::map<analysis_key_t, samples_1D<double>> s1D_avg_var3;
static std::map<analysis_key_t, samples_1D<double>> s1D_avg_var5;
static std::map<analysis_key_t, samples_1D<double>> s1D_avg_var7;

static std::map<analysis_key_t, std::map<double,std::vector<double>>> C_at_t;

static std::map<analysis_key_t,std::vector<double>> pixel_vals; //Raw pixel values, for histogramming.

static bool KitchenSinkAnalysisWasRun = false;


/*
static std::map<long int,double> histogram;
static std::map<long int,double> histogram_pm;
static std::map<long int,double> histogram_f;
static std::map<long int,double> histogram_absf;
static std::map<long int,double> histogram_ma;
static std::map<long int,double> histogram_ma_pm;
static std::map<long int,double> histogram_ma_f;
static std::map<long int,double> histogram_ma_absf;
static const double histobinwidth = 20.0;

static samples_1D<double> s1D_as_pm;
//static long int s1D_as_pm_cnt(0);
static samples_1D<double> s1D_as_absf;
//static long int s1D_as_absf_cnt(0);

static samples_1D<double> s1D_ma_as_pm;
//static long int s1D_ma_as_pm_cnt(0);
static samples_1D<double> s1D_ma_as_absf;
//static long int s1D_ma_as_absf_cnt(0);

static samples_1D<double> s1D_avg_dCdt_1;
static samples_1D<double> s1D_avg_dCdt_3;
static samples_1D<double> s1D_avg_dCdt_5;

static samples_1D<double> s1D_avg_abs_dCdt_1;
static samples_1D<double> s1D_avg_abs_dCdt_3;
static samples_1D<double> s1D_avg_abs_dCdt_5;

static samples_1D<double> s1D_post_tr_avg_var3;
static samples_1D<double> s1D_post_tr_avg_var5;
static samples_1D<double> s1D_post_tr_avg_var7;

//static std::map<float, std::vector<double>> trimmed_slopes;
//static std::map<float, std::vector<double>> spearmans_rank_coeffs;

//static long int plotinspector_i = 0;
//static long int plotinspector_max = 50;
*/


bool KitchenSinkAnalysis(planar_image_collection<float,double>::images_list_it_t first_img_it,
                         std::list<planar_image_collection<float,double>::images_list_it_t> selected_img_its,
                         std::list<std::reference_wrapper<planar_image_collection<float,double>>>,
                         std::list<std::reference_wrapper<contour_collection<double>>> ccsl, 
                         std::experimental::any ){
    KitchenSinkAnalysisWasRun = true; //Remember, this routine is called several times: for each image or group.

    //This routine performs a number of calculations. It is experimental and excerpts you plan to rely on should be
    // made into their own analysis functors.
    const bool InhibitSort = true; //Disable continuous sorting (defer to single sort later) to speed up data ingress.

/*
    ///////////////////////////////////////////////////////////////////////////////////////////////////////////
    //Scale individual contours (in the plane of the contour, about the centroid) for testing purposes.
    // To do this, we will replace the ccsl with a "mock" ccsl in which all contours are scaled.
    std::vector<contour_collection<double>> newcontours;
    for(auto &ccs : ccsl) newcontours.emplace_back(ccs.get());
    ccsl.clear();
    for(auto &ccs : newcontours){
        for(auto & c : ccs.contours){ 
            auto cent = c.Centroid();
            c = c.Scale_Dist_From_Point(cent, ROIScale);
        }
        ccsl.emplace_back( std::ref(ccs) );
    }
    ///////////////////////////////////////////////////////////////////////////////////////////////////////////
*/

    //Figure out if there are any contours for which are within the spatial extent of the image. 
    // There are many ways to do this! Since we are merely highlighting the contours, we scan 
    // all specified collections and treat them homogeneously.
    //
    // NOTE: We only bother to grab individual contours here. You could alter this if you wanted 
    //       each contour_collection's contours to have an identifying colour.
    if(ccsl.empty()) FUNCERR("Missing contour info needed for voxel colouring. Cannot continue");
    using contour_iter = std::list<contour_of_points<double> >::iterator;
    std::list<contour_iter> rois;
    for(auto &ccs : ccsl){
        for(auto it =  ccs.get().contours.begin(); it != ccs.get().contours.end(); ++it){
            if(it->points.empty()) continue;
            //if(first_img_it->encompasses_contour_of_points(*it)) rois.emplace_back(*it);
            if(first_img_it->encompasses_contour_of_points(*it)) rois.push_back(it);
        }
    }

    //Make a 'working' image which we can edit. Start by duplicating the first image.
    planar_image<float,double> working;
    working = *first_img_it;

    //Paint all pixels black.
    working.fill_pixels(static_cast<float>(0));

    //Loop over the rois, rows, columns, channels, and finally any selected images (if applicable).
    const auto row_unit   = first_img_it->row_unit;
    const auto col_unit   = first_img_it->col_unit;
    const auto ortho_unit = row_unit.Cross( col_unit ).unit();

    //Loop over the rois, rows, columns, channels, and finally any selected images (if applicable).
    for(const auto &roi : rois){
        //Try figure out the contour's name.
        const auto StudyInstanceUID = roi->GetMetadataValueAs<std::string>("StudyInstanceUID");
        const auto ROIName =  roi->GetMetadataValueAs<std::string>("ROIName");
        const auto FrameofReferenceUID = roi->GetMetadataValueAs<std::string>("FrameofReferenceUID");
        if(!StudyInstanceUID || !ROIName || !FrameofReferenceUID){
            FUNCWARN("Missing necessary tags for reporting analysis results. Cannot continue");
            return false;
        }
        const analysis_key_t BaseAnalysisKey = { {"StudyInstanceUID", StudyInstanceUID.value()},
                                                 {"ROIName", ROIName.value()},
                                                 {"FrameofReferenceUID", FrameofReferenceUID.value()},
                                                 {"SpatialBoxr", Xtostring(boxr)},
                                                 {"MinimumDatum", Xtostring(min_datum)} };
        //const auto ROIName = ReplaceAllInstances(roi->metadata["ROIName"], "[_]", " ");
        //const auto ROIName = roi->metadata["ROIName"];

/*
        //Construct a bounding box to reduce computational demand of checking every voxel.
        auto BBox = roi->Bounding_Box_Along(row_unit, 1.0);
        auto BBoxBestFitPlane = BBox.Least_Squares_Best_Fit_Plane(vec3<double>(0.0,0.0,1.0));
        auto BBoxProjectedContour = BBox.Project_Onto_Plane_Orthogonally(BBoxBestFitPlane);
        const bool BBoxAlreadyProjected = true;
*/

        //Prepare a contour for fast is-point-within-the-polygon checking.
        auto BestFitPlane = roi->Least_Squares_Best_Fit_Plane(ortho_unit);
        auto ProjectedContour = roi->Project_Onto_Plane_Orthogonally(BestFitPlane);
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
                        //Check if another ROI has already written to this voxel. Bail if so.
                        {
                            const auto curr_val = working.value(row, col, chan);
                            if(curr_val != 0) FUNCERR("There are overlapping ROIs. This code currently cannot handle this. "
                                                      "You will need to run the functor individually on the overlapping ROIs.");
                        }

                        //Cycle over the grouped images (temporal slices, or whatever the user has decided).
                        // Harvest the time course or any other voxel-specific numbers.
                        samples_1D<double> channel_time_course;
                        channel_time_course.uncertainties_known_to_be_independent_and_random = true;
                        for(auto & img_it : selected_img_its){
                            //Collect the datum of voxels and nearby voxels for an average.
                            std::list<double> in_pixs;
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
                            const auto avg_val = Stats::Mean(in_pixs);
                            if(in_pixs.size() < min_datum) continue; //If contours are too narrow so that there is too few datum for meaningful results.
                            const auto avg_val_sigma = std::sqrt(Stats::Unbiased_Var_Est(in_pixs))/std::sqrt(1.0*in_pixs.size());

                            auto dt = img_it->GetMetadataValueAs<double>("dt");
                            if(!dt) FUNCERR("Image is missing time metadata. Bailing");
                            channel_time_course.push_back(dt.value(), 0.0, avg_val, avg_val_sigma, InhibitSort);
                        }
                        channel_time_course.stable_sort();
                        if(channel_time_course.empty()) continue;
       
                        // --------------- Perform some calculations on the time course ---------------
        
                        //Insert each datum into the raw-C(t) buffer for variance calculations.
                        for(const auto &sample : channel_time_course.samples){
                            analysis_key_t FullAnalysisKey(BaseAnalysisKey);    
                            FullAnalysisKey["Description"] = "Variance of each time point over entire ROI";

                            C_at_t[FullAnalysisKey][ sample[0] ].push_back( sample[2] );
                        }

                        //Compute a windowed variance over the entire time course.
                        {
                            //Pre-normalize so that high-contrast regions do not obliterate the signal from low-contrast regions.
                            {
                                samples_1D<double> normed = channel_time_course;
                                normed.Normalize_wrt_Self_Overlap();
                           
                                analysis_key_t LocalAnalysisKey(BaseAnalysisKey);
                                LocalAnalysisKey["Description"] = "Sum of normalized time-windowed variance";
                                
                                for(auto HW : {2, 3, 5, 7}){
                                    analysis_key_t FullAnalysisKey(LocalAnalysisKey);
                                    FullAnalysisKey["MovingVarianceTwoSidedWidth"] = Xtostring(HW);
                                    
                                    sum_win_var[FullAnalysisKey] = sum_win_var[FullAnalysisKey].Sum_With( normed.Moving_Variance_Two_Sided( HW ) );
                                }
                            }

                            //Just let the high-contrast regions dominate.
                            {
                                analysis_key_t LocalAnalysisKey(BaseAnalysisKey);
                                LocalAnalysisKey["Description"] = "Sum of unnormalized time-windowed variance";

                                for(auto HW : {2, 3, 5, 7}){
                                    analysis_key_t FullAnalysisKey(LocalAnalysisKey);
                                    FullAnalysisKey["MovingVarianceTwoSidedWidth"] = Xtostring(HW);

                                    sum_win_var[FullAnalysisKey] = sum_win_var[FullAnalysisKey].Sum_With( channel_time_course.Moving_Variance_Two_Sided( HW ) );
                                }
                            }
                        }

/*
                        //Compute a finite-difference derivative of a smoothed time course.
                        {
                            //Pre-normalize so that high-contrast regions do not obliterate the signal from low-contrast regions.
                            if(true){
                                samples_1D<double> normed = channel_time_course;
                                normed.Normalize_wrt_Self_Overlap();

                                const auto s1 = normed.Moving_Average_Two_Sided_Gaussian_Weighting(1).Derivative_Centered_Finite_Differences();
                                const auto s3 = normed.Moving_Average_Two_Sided_Gaussian_Weighting(3).Derivative_Centered_Finite_Differences();
                                const auto s5 = normed.Moving_Average_Two_Sided_Gaussian_Weighting(5).Derivative_Centered_Finite_Differences();

                                s1D_avg_dCdt_1 = s1D_avg_dCdt_1.Sum_With( s1 );
                                s1D_avg_dCdt_3 = s1D_avg_dCdt_3.Sum_With( s3 );
                                s1D_avg_dCdt_5 = s1D_avg_dCdt_5.Sum_With( s5 );

                                s1D_avg_abs_dCdt_1 = s1D_avg_abs_dCdt_1.Sum_With( s1.Apply_Abs() );
                                s1D_avg_abs_dCdt_3 = s1D_avg_abs_dCdt_3.Sum_With( s3.Apply_Abs() );
                                s1D_avg_abs_dCdt_5 = s1D_avg_abs_dCdt_5.Sum_With( s5.Apply_Abs() );

                            //Or just let the high-contrast regions dominate.
                            }else{
                                const auto s1 = channel_time_course.Moving_Average_Two_Sided_Gaussian_Weighting(1).Derivative_Centered_Finite_Differences();
                                const auto s3 = channel_time_course.Moving_Average_Two_Sided_Gaussian_Weighting(3).Derivative_Centered_Finite_Differences();
                                const auto s5 = channel_time_course.Moving_Average_Two_Sided_Gaussian_Weighting(5).Derivative_Centered_Finite_Differences();

                                s1D_avg_dCdt_1 = s1D_avg_dCdt_1.Sum_With( s1 );
                                s1D_avg_dCdt_3 = s1D_avg_dCdt_3.Sum_With( s3 );
                                s1D_avg_dCdt_5 = s1D_avg_dCdt_5.Sum_With( s5 );

                                s1D_avg_abs_dCdt_1 = s1D_avg_abs_dCdt_1.Sum_With( s1.Apply_Abs() );
                                s1D_avg_abs_dCdt_3 = s1D_avg_abs_dCdt_3.Sum_With( s3.Apply_Abs() );
                                s1D_avg_abs_dCdt_5 = s1D_avg_abs_dCdt_5.Sum_With( s5.Apply_Abs() );

                            }
                        }

                        //Detect the peak location. Skip if the peak isn't reasonable.
                        {
                            //Option A: Using the raw data. This is highly susceptible to noise!
                            //const std::array<double,4> extrema = channel_time_course.Get_Extreme_Datum_y();

                            //Option B: Smoothing the data and finding its extreme. This could delay or skew the extrema's position.
                            auto smoothed = channel_time_course.Moving_Average_Two_Sided_Gaussian_Weighting(2.0);
                            const std::array<double,4> extrema = smoothed.Get_Extreme_Datum_y();

                            if(!isininc(30.0,extrema[0],100.0)) continue;
                        }
                      
                        //Trim the data to remove the initial uptake peak. Perform linear regression and record the slope.
                        {
                            samples_1D<double> trimmed;
                            trimmed = channel_time_course.Select_Those_Within_Inc(100.0, std::numeric_limits<double>::max());
                            bool wasOK;
                            auto res = trimmed.Linear_Least_Squares_Regression(&wasOK);
                            if(wasOK){
                                trimmed_slopes[(AnalysisKey-1)].push_back(res.slope);

                                //Update the pixel.
                                const float slope_sign = (res.slope > 0) ? 1 : 0; 
                                working.reference(row, col, chan) = slope_sign;
                            }
                        }

                        //Trim the data to remove the initial uptake peak. Compute the Spearman's rank correlation coefficient.
                        {
                            samples_1D<double> trimmed;
                            trimmed = channel_time_course.Select_Those_Within_Inc(100.0, std::numeric_limits<double>::max());

                            //working.reference(row, col, chan) = static_cast<float>(50);

                            bool wasOK;
                            const auto res = trimmed.Spearmans_Rank_Correlation_Coefficient(&wasOK);
                            if(wasOK){
                                const auto coeff = res[0];

                                //Update the pixel by scaling and offsetting the coefficient.
                                const float newval = static_cast<float>((1.0 + coeff) * 1000.0 + 1.0); 
                                working.reference(row, col, chan) = newval;

                                //Push the coefficient into the ROIs buffer.
                                spearmans_rank_coeffs[(AnalysisKey-1)].push_back(coeff);                                

                            }else{
                                FUNCWARN("Failed to compute Spearman's rank correlation coefficient. Continuing");
                            }
                        }
*/

                        //Harvest the pixel values into a buffer. We will use to generate a pixel intensity histogram.
                        {
                            analysis_key_t FullAnalysisKey(BaseAnalysisKey);
                            FullAnalysisKey["Description"] = "Voxel value histogram";

                            for(const auto &datum : channel_time_course.samples){
                                pixel_vals[FullAnalysisKey].push_back( datum[2] );
                            }
                        }

/*
 
                        //Shift the data to get rid of the highly-dynamic portions and make the fitting procedure easier.
                        {
                            //Option A: Assume that the stimulation response will shift in sync with the bolus arrival time.
                            //          Probably not true, but not entirely impossible...
                            ////Select only data to the right of the peak. Shift to the first element is now at the origin.
                            //const auto time_lhs_cutoff = extrema[0] + 30.0; //A few seconds after the peak, to let it settle.
                            //channel_time_course = channel_time_course.Sum_x_With(0.0 - time_lhs_cutoff);
                            //channel_time_course = channel_time_course.Select_Those_Within_Inc(0.0, std::numeric_limits<double>::max());
                            //if(channel_time_course.size() < 15) continue;

                            //Option B: Assume that although the bolus will arrive at different times, the stimulation should be
                            //          simultaneous for all voxels.
                            channel_time_course = channel_time_course.Select_Those_Within_Inc(100.0, std::numeric_limits<double>::max());
                            //channel_time_course = channel_time_course.Select_Those_Within_Inc(100.0, 260.0);

 
                            //Trim the tail before fitting. Seems to help keep the fits from getting hung up on time courses that appear
                            // to monotonically increase (slowly-diffusing tissues).
                            channel_time_course = channel_time_course.Sum_x_With(0.0 - 100.0);
                        }

                        //Fit a bi-exponential and remove the trend from the data.
                        if(true){
                            auto biexponential = [](const std::list<double> &X, const std::list<double> &Vars) -> double {
                                if(X.size() != 1) FUNCERR("Programming error A..");
                                if(Vars.size() != 3) FUNCERR("Programming error B..");
                                const auto x = X.front();
                                const auto A = *(std::next(Vars.begin(),0));
                                const auto B = *(std::next(Vars.begin(),1));
                                const auto C = *(std::next(Vars.begin(),2));
                                //const auto D = *(std::next(Vars.begin(),3));
                                const auto res = A*(std::exp(-std::abs(x/B)) + std::exp(-std::abs(x/C)));// + D;
                                if(!std::isfinite(res)) return std::numeric_limits<double>::max();
                                return res;
                            };

                            std::list<std::list<double>> fit_data;
                            for(const auto &datum : channel_time_course.samples) fit_data.push_back({datum[0],datum[2]});

                            bool wasOK;
                            auto fit_res = Ygor_Fit_LSS(&wasOK, biexponential, fit_data, {1213.87, 200.0, 2000.0}, 2, false, 10.0, 5000, 1E-6);
                            //NOTE: input is (wasOK, F, data, vars, dim, verbose, char_len, max_iters, Ftol).
                            //NOTE: output is (best_params, chi_sq, Qvalue, DOF, red_chi_sq, raw_coeff_deter, mod_coeff_deter, best_SRs).
                            if(!wasOK) continue;
                            const auto Vars = std::get<0>(fit_res); //A, B, and C here.

                            for(auto &datum : channel_time_course.samples){
                                const auto predicted_val = biexponential({datum[0]}, Vars);
                                datum[2] -= predicted_val;
                                datum[3] = std::abs(datum[3]); //Assuming predicted_val has no uncertainty! (Obviously not true!)
                            }

                            //Sum the moving variances of the post-trend-removal data into the buffers. Do not normalize.
                            s1D_post_tr_avg_var3 = s1D_post_tr_avg_var3.Sum_With( channel_time_course.Moving_Variance_Two_Sided(3) );
                            s1D_post_tr_avg_var5 = s1D_post_tr_avg_var5.Sum_With( channel_time_course.Moving_Variance_Two_Sided(5) );
                            s1D_post_tr_avg_var7 = s1D_post_tr_avg_var7.Sum_With( channel_time_course.Moving_Variance_Two_Sided(7) );

                        }

                        //Use a moving average of some sort to remove the trend from the data.
                        if(false){
                            auto trend = channel_time_course.Moving_Average_Two_Sided_Gaussian_Weighting(4.0);
                            channel_time_course = channel_time_course.Subtract(trend);
                        }

                        //Trim the data to a limited region around the stimulation, so as to ignore the perhiphery where 
                        // there is a lot of noise.
                        if(false){
                            channel_time_course = channel_time_course.Select_Those_Within_Inc(0.0, 160.0);
                        }

                        //Find the top-few outlying datum. Maybe the top 10 or so. Sum them into a histogram.
                        {
                            samples_1D<double> tc_copy;
                            tc_copy = channel_time_course;

                            //Sort so that the maximum |f_i| are first, but keep the sign intact.
                            const auto sort_on_abs_f_i = [](const std::array<double,4> &L, const std::array<double,4> &R) -> bool {
                                return std::abs(L[2]) < std::abs(R[2]);
                            };
                            std::stable_sort(tc_copy.samples.begin(), tc_copy.samples.end(), sort_on_abs_f_i);

                            samples_1D<double> samps_f, samps_absf, samps, samps_pm;
                            for(auto i = 0; i < 10; ++i){
                                auto datum = tc_copy.samples[i];
                                const auto x = datum[0];
                                const auto f = datum[2];
                                const auto bin_d = x / histobinwidth;
                                const auto bin = static_cast<long int>(std::floor(bin_d));

                                histogram_f[bin] += f;
                                histogram_absf[bin] += std::abs(f);
                                histogram[bin] += 1.0;
                                histogram_pm[bin] += std::copysign(1.0,f);
                            }
                        }

                        //Perform a moving average on the datum and then find the top-few outlying datum. Sum them into a histogram.
                        {
                            samples_1D<double> tc_copy;
                            tc_copy = channel_time_course;
                            tc_copy = tc_copy.Moving_Average_Two_Sided_Gaussian_Weighting(2.0);

                            //Sort so that the maximum |f_i| are first, but keep the sign intact.
                            const auto sort_on_abs_f_i = [](const std::array<double,4> &L, const std::array<double,4> &R) -> bool {
                                return std::abs(L[2]) < std::abs(R[2]);
                            };
                            std::stable_sort(tc_copy.samples.begin(), tc_copy.samples.end(), sort_on_abs_f_i);
                            for(auto i = 0; i < 10; ++i){
                                auto datum = tc_copy.samples[i];
                                const auto x = datum[0];
                                const auto f = datum[2];
                                const auto bin_d = x / histobinwidth;
                                const auto bin = static_cast<long int>(std::floor(bin_d));

                                histogram_ma_f[bin] += f;
                                histogram_ma_absf[bin] += std::abs(f);
                                histogram_ma[bin] += 1.0;
                                histogram_ma_pm[bin] += std::copysign(1.0,f);
                            }
                        }

                        //Transform and sum the data into the non-histogram buffers.
                        {
                            samples_1D<double> as_absf, as_pm;
                            as_absf = channel_time_course;
                            as_pm   = channel_time_course;
                
                            for(auto & sample : as_absf.samples) sample[2] = std::abs(sample[2]);
                            for(auto & sample : as_pm.samples)   sample[2] = std::copysign(1.0,sample[2]);

                            s1D_as_pm   = s1D_as_pm.Sum_With(as_pm);
                            ++s1D_as_pm_cnt;
                            s1D_as_absf = s1D_as_absf.Sum_With(as_absf);
                            ++s1D_as_absf_cnt;
                        }

                        //Transform and sum the data into the non-histogram buffers: moving average version.
                        {
                            samples_1D<double> ma_as_absf, ma_as_pm;
                            ma_as_absf = channel_time_course.Moving_Average_Two_Sided_Gaussian_Weighting(2.0);
                            ma_as_pm   = ma_as_absf;

                            for(auto & sample : ma_as_absf.samples) sample[2] = std::abs(sample[2]);
                            for(auto & sample : ma_as_pm.samples)   sample[2] = std::copysign(1.0,sample[2]);

                            s1D_ma_as_pm   = s1D_ma_as_pm.Sum_With(ma_as_pm);
                            ++s1D_ma_as_pm_cnt;
                            s1D_ma_as_absf = s1D_ma_as_absf.Sum_With(ma_as_absf);
                            ++s1D_ma_as_absf_cnt;
                        }
*/        

                        //Update the value.
                        //working.reference(row, col, chan) = static_cast<float>(0);

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

    //Swap the original image with the working image.
    *first_img_it = working;

    //Alter the first image's metadata to reflect that averaging has occurred. You might want to consider
    // a selective whitelist approach so that unique IDs are not duplicated accidentally.
    UpdateImageDescription( std::ref(*first_img_it), "Kitchen Sink Map" );

    return true;
}


static bool Push_Analysis_Results_to_Database(analysis_key_t thekey, samples_1D<double> s1D){
    // ---------------------------------- Convert key to JSON ---------------------------------------
    json_t *obj = json_object();
    for(auto apair : thekey){
        if(0 != json_object_set(obj, apair.first.c_str(), json_string(apair.second.c_str())) ){
            FUNCERR("Unable to generate JSON from analysis key");
        }
    }
   
    const char *dumped = json_dumps(obj, JSON_SORT_KEYS | JSON_ENSURE_ASCII | JSON_INDENT(0));
    if(dumped == nullptr){
        FUNCWARN("Unable to encode JSON to string. Cannot continue");
        json_decref(obj);
        return false;
    }

    std::string thejson(dumped);
    json_decref(obj);

    // ---------------------------------- Stringify the samples_1D ---------------------------------------
    std::stringstream s1Dss;
    s1Dss << s1D;

    // -------------------------------------- Push to database -------------------------------------------
    try{
        pqxx::connection c("dbname=pacs user=hal host=localhost port=5432");
        pqxx::work txn(c);

        std::stringstream ss;
        ss << "INSERT INTO samples1D_for_bigart2015 ";
        ss << "    (Parameters,ImportTimepoint,samples_1D) ";
        ss << "VALUES ";
        ss << "    ( " << txn.quote(thejson) << "::JSONB, now(),";
        ss << "      " << txn.quote(s1Dss.str()) << " ) ";
        ss << "RETURNING ImportTimepoint;";

        pqxx::result res = txn.exec(ss.str());
        if(res.size() != 1) throw "Unable to insert data into database";

        //Commit transaction.
        txn.commit();
        return true;
    }catch(const std::exception &e){
        FUNCWARN("Unable to select contours: exception caught: " << e.what());
    }
    return false;
}


//Dump lots of files and records. Calling this routine will also enable the KitchenSinkAnalysis() to be safely re-run.
void DumpKitchenSinkResults(std::map<std::string,std::string> InvocationMetadata){
    if(!KitchenSinkAnalysisWasRun){
        FUNCWARN("Forgoing dumping the kitchen sink analysis results; the analysis was not run");
        return;
    }

/*
    std::string bd;
    for(auto i = 0; i < 1000; ++i){
        const std::string basedir = "/tmp/pet_ct_perfusion_"_s + Xtostring(i) + "/";
        if(  !Does_Dir_Exist_And_Can_Be_Read(basedir)
        &&   Create_Dir_and_Necessary_Parents(basedir) ){
            bd = basedir;
            break;
        }
    } 
    if(bd.empty()) FUNCERR("Unable to allocate a new directory for kitchen sink analysis output. Cannot continue");
*/

/*
    {
      const std::string histotitle("Histogram: bins represent position of deviation from biexponential trend");
      std::cout << histotitle << std::endl;
      samples_1D<double> histoplot;
      for(const auto & dp : histogram){
          std::cout << dp.first << "\t" << dp.second << std::endl;
          histoplot.push_back(1.0*(dp.first), dp.second);
      }
      auto binned = histoplot.Histogram_Equal_Sized_Bins(histoplot.size(),true);
      auto zero = histoplot.Multiply_With(0.0);

      Plotter2 toplot;
      toplot.Set_Global_Title(histotitle);
      toplot.Insert_samples_1D(histoplot, "Binned datum", "points");
      toplot.Insert_samples_1D(binned, "", "linespoints");
      toplot.Insert_samples_1D(zero, "", "lines");
      toplot.Plot();
      toplot.Plot_as_PDF(Get_Unique_Sequential_Filename(bd + "histogram_",6,".pdf"));
    }
    {
      const std::string histotitle("Histogram_pm: bins represent deviation from biexponential trend: (num samples above 0) - (num samples below zero)");
      std::cout << histotitle << std::endl;
      samples_1D<double> histoplot;
      for(const auto & dp : histogram_pm){
          std::cout << dp.first << "\t" << dp.second << std::endl;
          histoplot.push_back(1.0*(dp.first), dp.second);
      }
      auto binned = histoplot.Histogram_Equal_Sized_Bins(histoplot.size(),true);
      auto zero = histoplot.Multiply_With(0.0);
      
      Plotter2 toplot;
      toplot.Set_Global_Title(histotitle);
      toplot.Insert_samples_1D(histoplot, "Binned datum", "points");
      toplot.Insert_samples_1D(binned, "", "linespoints");
      toplot.Insert_samples_1D(zero, "", "lines");
      toplot.Plot();
      toplot.Plot_as_PDF(Get_Unique_Sequential_Filename(bd + "histogram_pm_",6,".pdf"));
    }
    {
      const std::string histotitle("Histogram_f: bins represent position and strength of deviation from biexponential trend");
      std::cout << histotitle << std::endl;
      samples_1D<double> histoplot;
      for(const auto & dp : histogram_f){
          std::cout << dp.first << "\t" << dp.second << std::endl;
          histoplot.push_back(1.0*(dp.first), dp.second);
      }
      auto binned = histoplot.Histogram_Equal_Sized_Bins(histoplot.size(),true);
      auto zero = histoplot.Multiply_With(0.0);
      
      Plotter2 toplot;
      toplot.Set_Global_Title(histotitle);
      toplot.Insert_samples_1D(histoplot, "Binned datum", "points");
      toplot.Insert_samples_1D(binned, "", "linespoints");
      toplot.Insert_samples_1D(zero, "", "lines");
      toplot.Plot();
      toplot.Plot_as_PDF(Get_Unique_Sequential_Filename(bd + "histogram_f_",6,".pdf"));
    }
    {
      const std::string histotitle("Histogram_absf: bins represent position and abs strength of deviation from biexponential trend");
      std::cout << histotitle << std::endl;
      samples_1D<double> histoplot;
      for(const auto & dp : histogram_absf){
          std::cout << dp.first << "\t" << dp.second << std::endl;
          histoplot.push_back(1.0*(dp.first), dp.second);
      }
      auto binned = histoplot.Histogram_Equal_Sized_Bins(histoplot.size(),true);
      auto zero = histoplot.Multiply_With(0.0);
      
      Plotter2 toplot;
      toplot.Set_Global_Title(histotitle);
      toplot.Insert_samples_1D(histoplot, "Binned datum", "points");
      toplot.Insert_samples_1D(binned, "", "linespoints");
      toplot.Insert_samples_1D(zero, "", "lines");
      toplot.Plot();
      toplot.Plot_as_PDF(Get_Unique_Sequential_Filename(bd + "histogram_absf_",6,".pdf"));
    }
    {
      const std::string histotitle("Histogram_ma: bins represent position of deviation from biexponential trend with 4pt sigma Gaussian blur");
      std::cout << histotitle << std::endl;
      samples_1D<double> histoplot;
      for(const auto & dp : histogram_ma){
          std::cout << dp.first << "\t" << dp.second << std::endl;
          histoplot.push_back(1.0*(dp.first), dp.second);
      }
      auto binned = histoplot.Histogram_Equal_Sized_Bins(histoplot.size(),true);
      auto zero = histoplot.Multiply_With(0.0);
      
      Plotter2 toplot;
      toplot.Set_Global_Title(histotitle);
      toplot.Insert_samples_1D(histoplot, "Binned datum", "points");
      toplot.Insert_samples_1D(binned, "", "linespoints");
      toplot.Insert_samples_1D(zero, "", "lines");
      toplot.Plot();
      toplot.Plot_as_PDF(Get_Unique_Sequential_Filename(bd + "histogram_ma_",6,".pdf"));
    }
    {
      const std::string histotitle("Histogram_ma_pm: bins represent deviation from biexponential trend: (num samples above 0) - (num samples below zero) with 4pt sigma Gaussian blur");
      std::cout << histotitle << std::endl;
      samples_1D<double> histoplot;
      for(const auto & dp : histogram_ma_pm){
          std::cout << dp.first << "\t" << dp.second << std::endl;
          histoplot.push_back(1.0*(dp.first), dp.second);
      }
      auto binned = histoplot.Histogram_Equal_Sized_Bins(histoplot.size(),true);
      auto zero = histoplot.Multiply_With(0.0);
      
      Plotter2 toplot;
      toplot.Set_Global_Title(histotitle);
      toplot.Insert_samples_1D(histoplot, "Binned datum", "points");
      toplot.Insert_samples_1D(binned, "", "linespoints");
      toplot.Insert_samples_1D(zero, "", "lines");
      toplot.Plot();
      toplot.Plot_as_PDF(Get_Unique_Sequential_Filename(bd + "histogram_ma_pm_",6,".pdf"));
    }
    {
      const std::string histotitle("Histogram_ma_f: bins represent position and strength of deviation from biexponential trend trend with 4pt sigma Gaussian blur");
      std::cout << histotitle << std::endl;
      samples_1D<double> histoplot;
      for(const auto & dp : histogram_ma_f){
          std::cout << dp.first << "\t" << dp.second << std::endl;
          histoplot.push_back(1.0*(dp.first), dp.second);
      }
      auto binned = histoplot.Histogram_Equal_Sized_Bins(histoplot.size(),true);
      auto zero = histoplot.Multiply_With(0.0);
      
      Plotter2 toplot;
      toplot.Set_Global_Title(histotitle);
      toplot.Insert_samples_1D(histoplot, "Binned datum", "points");
      toplot.Insert_samples_1D(binned, "", "linespoints");
      toplot.Insert_samples_1D(zero, "", "lines");
      toplot.Plot();
      toplot.Plot_as_PDF(Get_Unique_Sequential_Filename(bd + "histogram_ma_f_",6,".pdf"));
    }
    {
      const std::string histotitle("Histogram_ma_absf: bins represent position and abs strength of deviation from biexponential trend trend with 4pt sigma Gaussian blur");
      std::cout << histotitle << std::endl;
      samples_1D<double> histoplot;
      for(const auto & dp : histogram_ma_absf){
          std::cout << dp.first << "\t" << dp.second << std::endl;
          histoplot.push_back(1.0*(dp.first), dp.second);
      }
      auto binned = histoplot.Histogram_Equal_Sized_Bins(histoplot.size(),true);
      auto zero = histoplot.Multiply_With(0.0);
       
      Plotter2 toplot;
      toplot.Set_Global_Title(histotitle);
      toplot.Insert_samples_1D(histoplot, "Binned datum", "points");
      toplot.Insert_samples_1D(binned, "", "linespoints");
      toplot.Insert_samples_1D(zero, "", "lines");
      toplot.Plot();
      toplot.Plot_as_PDF(Get_Unique_Sequential_Filename(bd + "histogram_ma_absf_",6,".pdf"));
    }



    {
      const std::string title("Difference from biexponential trend: above (+1) or below (-1).");
      std::cout << title << std::endl;
      samples_1D<double> histoplot = s1D_as_pm;

      auto binned = histoplot.Histogram_Equal_Sized_Bins(8,true);
      auto zero   = histoplot.Multiply_With(0.0);

      Plotter2 toplot;
      toplot.Set_Global_Title(title);
      toplot.Insert_samples_1D(histoplot, "Binned datum", "points");
      toplot.Insert_samples_1D(binned, "", "linespoints");
      toplot.Insert_samples_1D(zero, "", "lines");
      toplot.Plot();
      const std::string basefname = bd + "deviation_from_trend_as_pm_";
      toplot.Plot_as_PDF(Get_Unique_Sequential_Filename(basefname,6,".pdf"));
      WriteStringToFile(toplot.Dump_as_String(),
                        Get_Unique_Sequential_Filename(basefname,6,".dat"));
    }
    { 
      const std::string title("Difference from biexponential trend, moving averaged: above (+1) or below (-1).");
      std::cout << title << std::endl;
      samples_1D<double> histoplot = s1D_ma_as_pm;

      auto binned = histoplot.Histogram_Equal_Sized_Bins(8,true);
      auto zero   = histoplot.Multiply_With(0.0);

      Plotter2 toplot;
      toplot.Set_Global_Title(title);
      toplot.Insert_samples_1D(histoplot, "Binned datum", "points");
      toplot.Insert_samples_1D(binned, "", "linespoints");
      toplot.Insert_samples_1D(zero, "", "lines");
      toplot.Plot();
      const std::string basefname = bd + "deviation_from_trend_ma_as_pm_";
      toplot.Plot_as_PDF(Get_Unique_Sequential_Filename(basefname,6,".pdf"));
      WriteStringToFile(toplot.Dump_as_String(),
                        Get_Unique_Sequential_Filename(basefname,6,".dat"));
    }
    { 
      const std::string title("Difference from biexponential trend: abs(f)");
      std::cout << title << std::endl;
      samples_1D<double> histoplot = s1D_as_absf;

      auto binned = histoplot.Histogram_Equal_Sized_Bins(8,true);
      auto zero   = histoplot.Multiply_With(0.0);

      Plotter2 toplot;
      toplot.Set_Global_Title(title);
      toplot.Insert_samples_1D(histoplot, "Binned datum", "points");
      toplot.Insert_samples_1D(binned, "", "linespoints");
      toplot.Insert_samples_1D(zero, "", "lines");
      toplot.Plot();
      const std::string basefname = bd + "deviation_from_trend_as_absf_";
      toplot.Plot_as_PDF(Get_Unique_Sequential_Filename(basefname,6,".pdf"));
      WriteStringToFile(toplot.Dump_as_String(),
                        Get_Unique_Sequential_Filename(basefname,6,".dat"));
    }
    {
      const std::string title("Difference from biexponential trend, moving averaged: abs(f).");
      std::cout << title << std::endl;
      samples_1D<double> histoplot = s1D_ma_as_absf;

      auto binned = histoplot.Histogram_Equal_Sized_Bins(8,true);
      auto zero   = histoplot.Multiply_With(0.0);

      Plotter2 toplot;
      toplot.Set_Global_Title(title);
      toplot.Insert_samples_1D(histoplot, "Binned datum", "points");
      toplot.Insert_samples_1D(binned, "", "linespoints");
      toplot.Insert_samples_1D(zero, "", "lines");
      toplot.Plot();
      const std::string basefname = bd + "deviation_from_trend_ma_as_absf_";
      toplot.Plot_as_PDF(Get_Unique_Sequential_Filename(basefname,6,".pdf"));
      WriteStringToFile(toplot.Dump_as_String(),
                        Get_Unique_Sequential_Filename(basefname,6,".dat"));
    }
*/

    {
      for(auto apair : sum_win_var){
          auto thekey = apair.first;
          const auto thes1D = apair.second;
        
          thekey.insert(InvocationMetadata.begin(),InvocationMetadata.end());

          if(!Push_Analysis_Results_to_Database(thekey, thes1D)){
              FUNCWARN("Unable to push analysis result to database. Ignoring and continuing");
          }
      } 
/*
      //std::cout << title << std::endl;
      //samples_1D<double> histoplot = s1D_ma_as_absf;
      //auto binned = histoplot.Histogram_Equal_Sized_Bins(8,true);
      //auto zero   = histoplot.Multiply_With(0.0);

      Plotter2 toplot;
      toplot.Set_Global_Title(title);
      for(auto & apair : s1D_avg_var3){
          const auto ROIName  = apair.first;
          const auto samples  = apair.second;
          toplot.Insert_samples_1D(samples, "ROI '"_s + ROIName + "', Window width = 2*3+1", "linespoints");
          toplot.Insert_samples_1D(samples.Multiply_With(0.0), "", "lines");
      }
      for(auto & apair : s1D_avg_var5){
          const auto ROIName  = apair.first;
          const auto samples  = apair.second;
          toplot.Insert_samples_1D(samples, "ROI '"_s + ROIName + "', Window width = 2*5+1", "linespoints");
          toplot.Insert_samples_1D(samples.Multiply_With(0.0), "", "lines");
      }
      for(auto & apair : s1D_avg_var7){
          const auto ROIName  = apair.first;
          const auto samples  = apair.second;
          toplot.Insert_samples_1D(samples, "ROI '"_s + ROIName + "', Window width = 2*7+1", "linespoints");
          toplot.Insert_samples_1D(samples.Multiply_With(0.0), "", "lines");
      }
      toplot.Plot();
      const std::string basefname = bd + "windowed_variance_";
      toplot.Plot_as_PDF(Get_Unique_Sequential_Filename(basefname,6,".pdf"));
      WriteStringToFile(toplot.Dump_as_String(),
                        Get_Unique_Sequential_Filename(basefname,6,".dat"));
*/
    }

/*
    {
      const std::string title("Summed derivatives over Gaussian-smoothed time course.");

      Plotter2 toplot;
      toplot.Set_Global_Title(title);
      toplot.Insert_samples_1D(s1D_avg_dCdt_1, "Sigma = 1 datum", "linespoints");
      toplot.Insert_samples_1D(s1D_avg_dCdt_3, "Sigma = 3 datum", "linespoints");
      toplot.Insert_samples_1D(s1D_avg_dCdt_5, "Sigma = 5 datum", "linespoints");
      toplot.Insert_samples_1D(s1D_avg_dCdt_1.Multiply_With(0.0), "", "lines");
      toplot.Plot();
      const std::string basefname = bd + "windowed_smoothed_dCdt_";
      toplot.Plot_as_PDF(Get_Unique_Sequential_Filename(basefname,6,".pdf"));
      WriteStringToFile(toplot.Dump_as_String(),
                        Get_Unique_Sequential_Filename(basefname,6,".dat"));
    }
    {
      const std::string title("Summed |derivatives| over Gaussian-smoothed time course.");

      Plotter2 toplot;
      toplot.Set_Global_Title(title);
      toplot.Insert_samples_1D(s1D_avg_abs_dCdt_1, "Sigma = 1 datum", "linespoints");
      toplot.Insert_samples_1D(s1D_avg_abs_dCdt_3, "Sigma = 3 datum", "linespoints");
      toplot.Insert_samples_1D(s1D_avg_abs_dCdt_5, "Sigma = 5 datum", "linespoints");
      toplot.Insert_samples_1D(s1D_avg_abs_dCdt_1.Multiply_With(0.0), "", "lines");
      toplot.Plot();
      const std::string basefname = bd + "windowed_smoothed_abs_dCdt_";
      //toplot.Plot_as_PDF(Get_Unique_Sequential_Filename(basefname,6,".pdf"));
      WriteStringToFile(toplot.Dump_as_String(),
                        Get_Unique_Sequential_Filename(basefname,6,".dat"));
    }
    {
      const std::string title("Summed post-trend-removed variances (initial peak removed for fit).");

      Plotter2 toplot;
      toplot.Set_Global_Title(title);
      toplot.Insert_samples_1D(s1D_post_tr_avg_var3, "Window width = 2*3+1", "linespoints");
      toplot.Insert_samples_1D(s1D_post_tr_avg_var5, "Window width = 2*5+1", "linespoints");
      toplot.Insert_samples_1D(s1D_post_tr_avg_var7, "Window width = 2*7+1", "linespoints");
      toplot.Insert_samples_1D(s1D_post_tr_avg_var3.Multiply_With(0.0), "", "lines");
      toplot.Plot();
      const std::string basefname = bd + "windowed_post_trend_variance_";
      //toplot.Plot_as_PDF(Get_Unique_Sequential_Filename(basefname,6,".pdf"));
      WriteStringToFile(toplot.Dump_as_String(),
                        Get_Unique_Sequential_Filename(basefname,6,".dat"));
    }
*/
    {

      for(auto & apair : C_at_t){
          auto thekey = apair.first;
          thekey.insert(InvocationMetadata.begin(),InvocationMetadata.end());

          samples_1D<double> var;
          for(const auto &data : apair.second){
              var.push_back( data.first, std::sqrt(Stats::Unbiased_Var_Est(data.second))/std::sqrt(1.0*data.second.size()) );
          }

          if(!Push_Analysis_Results_to_Database(thekey, var)){
              FUNCWARN("Unable to push analysis result to database. Ignoring and continuing");
          }
      }

/*
      const std::string title("Actual std. dev.");
      Plotter2 toplot;
      toplot.Set_Global_Title(title);
  
      for(auto & apair : C_at_t){
          const auto ROIName = apair.first;
          samples_1D<double> var;
          for(const auto &data : apair.second){
              var.push_back( data.first, std::sqrt(Stats::Unbiased_Var_Est(data.second))/std::sqrt(1.0*data.second.size()) );
          }

          toplot.Insert_samples_1D(var, "ROI '"_s + ROIName + "', Std. Dev. by sqrt(N)", "linespoints");
      }
      toplot.Plot();
      const std::string basefname = bd + "actual_std_dev_";
      toplot.Plot_as_PDF(Get_Unique_Sequential_Filename(basefname,6,".pdf"));
      WriteStringToFile(toplot.Dump_as_String(),
                        Get_Unique_Sequential_Filename(basefname,6,".dat"));
*/
    }

/*
    {
      const std::string title("Distribution of trimmed C(t) linear regression slopes");

      Plotter2 toplot;
      toplot.Set_Global_Title(title);
      float ROIName = 0;
      for(auto & buff : trimmed_slopes){
          samples_1D<double> binned = Bag_of_numbers_to_N_equal_bin_samples_1D_histogram(buff.second, buff.second.size()/10, true);
          toplot.Insert_samples_1D(binned, "Coefficients for ROI "_s + Xtostring(ROIName), "lines");
          ++ROIName;
      }
      toplot.Plot();
      const std::string basefname = bd + "binned_lin_reg_slopes_";
      toplot.Plot_as_PDF(Get_Unique_Sequential_Filename(basefname,6,".pdf"));
      WriteStringToFile(toplot.Dump_as_String(),
                         Get_Unique_Sequential_Filename(basefname,6,".dat"));
    }

    {
      const std::string title("Distribution of spearman's rank correlation coefficients");

      Plotter2 toplot;
      toplot.Set_Global_Title(title);
      uint32_t ROIName = 0;
      for(auto & buff : spearmans_rank_coeffs){
          samples_1D<double> binned = Bag_of_numbers_to_N_equal_bin_samples_1D_histogram(buff.second, buff.second.size()/10, true);
          toplot.Insert_samples_1D(binned, "Coefficients for ROI "_s + Xtostring(ROIName), "lines");
          ++ROIName;
      }
      toplot.Plot();
      const std::string basefname = bd + "binned_spearmans_rank_coeffs_";
      toplot.Plot_as_PDF(Get_Unique_Sequential_Filename(basefname,6,".pdf"));
      WriteStringToFile(toplot.Dump_as_String(),
                         Get_Unique_Sequential_Filename(basefname,6,".dat"));
    }
*/

    {
      for(auto & apair : pixel_vals){
          auto thekey = apair.first;
          thekey.insert(InvocationMetadata.begin(),InvocationMetadata.end());

          const auto roi_vals = apair.second;

          const auto num_of_bins = 100;
          const bool bins_visible = true;
          samples_1D<double> binned = Bag_of_numbers_to_N_equal_bin_samples_1D_histogram(roi_vals, num_of_bins, bins_visible);
          thekey["NumberofBins"] = Xtostring(num_of_bins);
          thekey["VisibleBins"] = Xtostring(bins_visible ? 1 : 0);

          if(!Push_Analysis_Results_to_Database(thekey, binned)){
              FUNCWARN("Unable to push analysis result to database. Ignoring and continuing");
          }
      }
/*
      const std::string title("Histogram of pixel values");
      Plotter2 toplot;
      toplot.Set_Global_Title(title);
      for(auto & apair : pixel_vals){
          const auto ROIName  = apair.first;
          const auto roi_vals = apair.second;
          //samples_1D<double> binned = Bag_of_numbers_to_N_equal_bin_samples_1D_histogram(roi_vals, roi_vals.size()/10, true);
          samples_1D<double> binned = Bag_of_numbers_to_N_equal_bin_samples_1D_histogram(roi_vals, 100, true);
          toplot.Insert_samples_1D(binned, "ROI '"_s + ROIName + "'", "lines");
      }
      toplot.Plot();
      const std::string basefname = bd + "binned_pixel_values_";
      toplot.Plot_as_PDF(Get_Unique_Sequential_Filename(basefname,6,".pdf"));
      WriteStringToFile(toplot.Dump_as_String(),
                         Get_Unique_Sequential_Filename(basefname,6,".dat"));
*/
    }


    //Purge global state and clear the indicator for a fresh run.
    sum_win_var.clear();

    s1D_avg_var3.clear();
    s1D_avg_var5.clear();
    s1D_avg_var7.clear();

    C_at_t.clear();

    pixel_vals.clear();
 
    KitchenSinkAnalysisWasRun = false;

    return;
}


