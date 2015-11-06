
#include <list>
#include <functional>
#include <limits>
#include <map>
#include <cmath>
#include <experimental/any>

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

#include "YgorImages_Processing_Functor_per_ROI_Time_Courses.h"


bool PerROITimeCourses(planar_image_collection<float,double>::images_list_it_t first_img_it,
                       std::list<planar_image_collection<float,double>::images_list_it_t> selected_img_its,
                       std::list<std::reference_wrapper<contour_collection<double>>> ccsl, 
                       std::experimental::any user_data ){

    //This routine requires a valid PerROITimeCoursesUserData struct packed into the user_data. Accept the throw
    // if the input is missing or invalid.
    PerROITimeCoursesUserData *user_data_s;
    try{
        user_data_s = std::experimental::any_cast<PerROITimeCoursesUserData *>(user_data);
    }catch(const std::exception &e){
        FUNCWARN("Unable to cast user_data to appropriate format. Cannot continue with computation");
        return false;
    }

    //This routine performs a number of calculations. It is experimental and excerpts you plan to rely on should be
    // made into their own analysis functors.
    const bool InhibitSort = true; //Disable continuous sorting (defer to single sort later) to speed up data ingress.


    //Figure out if there are any contours for which are within the spatial extent of the image. 
    // There are many ways to do this! Since we are merely highlighting the contours, we scan 
    // all specified collections and treat them homogeneously.
    //
    // NOTE: We only bother to grab individual contours here. You could alter this if you wanted 
    //       each contour_collection's contours to have an identifying colour.
    if(ccsl.empty()){
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
    //Make a 'working' image which we can edit. Start by duplicating the first image.
    planar_image<float,double> working;
    working = *first_img_it;

    //Paint all pixels black.
    working.fill_pixels(static_cast<float>(0));

    //Loop over the rois, rows, columns, channels, and finally any selected images (if applicable).
    const auto row_unit   = first_img_it->row_unit;
    const auto col_unit   = first_img_it->col_unit;
    const auto ortho_unit = row_unit.Cross( col_unit ).unit();

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
                                const auto avg_val = Stats::Mean(in_pixs);
                                if(in_pixs.size() < min_datum) continue; //If contours are too narrow so that there is too few datum for meaningful results.
                                const auto avg_val_sigma = std::sqrt(Stats::Unbiased_Var_Est(in_pixs))/std::sqrt(1.0*in_pixs.size());
    
                                auto dt = img_it->GetMetadataValueAs<double>("dt");
                                if(!dt) FUNCERR("Image is missing time metadata. Bailing");
                                channel_time_course.push_back(dt.value(), 0.0, avg_val, avg_val_sigma, InhibitSort);
                            }
                            channel_time_course.stable_sort();
                            if(channel_time_course.empty()) continue;
    
                            //Fill in some basic time course metadata.
     
                            // ... TODO ...  (Worthwhile at this stage?)
 
                            // --------------- Append the time course data into the user_data struct ------------------
                            //user_data_s->total_voxel_count[ std::ref(ccs) ] += channel_time_course.size();
                            //user_data_s->voxel_count[ std::ref(ccs) ] += 1;
                            user_data_s->time_courses[ ROIName.value() ] = user_data_s->time_courses[ ROIName.value() ].Sum_With(channel_time_course);
                            user_data_s->total_voxel_count[ ROIName.value() ] += channel_time_course.size();
                            user_data_s->voxel_count[ ROIName.value() ] += 1;
    

           
    /*
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
    
                            //Harvest the pixel values into a buffer. We will use to generate a pixel intensity histogram.
                            {
                                analysis_key_t FullAnalysisKey(BaseAnalysisKey);
                                FullAnalysisKey["Description"] = "Voxel value histogram";
    
                                for(const auto &datum : channel_time_course.samples){
                                    pixel_vals[FullAnalysisKey].push_back( datum[2] );
                                }
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
    } //Loop over contour_collections.

    //Swap the original image with the working image.
    *first_img_it = working;

    //Alter the first image's metadata to reflect that averaging has occurred. You might want to consider
    // a selective whitelist approach so that unique IDs are not duplicated accidentally.
    first_img_it->metadata["Description"] = "Kitchen sink map";

    return true;
}


/*
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
*/


/*
//Dump lots of files and records. Calling this routine will also enable the KitchenSinkAnalysis() to be safely re-run.
void DumpKitchenSinkResults(std::map<std::string,std::string> InvocationMetadata){
    if(!KitchenSinkAnalysisWasRun){
        FUNCWARN("Forgoing dumping the kitchen sink analysis results; the analysis was not run");
        return;
    }

    {
      for(auto apair : sum_win_var){
          auto thekey = apair.first;
          const auto thes1D = apair.second;
        
          thekey.insert(InvocationMetadata.begin(),InvocationMetadata.end());

          if(!Push_Analysis_Results_to_Database(thekey, thes1D)){
              FUNCWARN("Unable to push analysis result to database. Ignoring and continuing");
          }
      } 
    }

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

    }

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
*/

