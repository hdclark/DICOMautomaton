
#include <cmath>
#include <exception>
#include <any>
#include <optional>
#include <functional>
#include <limits>
#include <list>
#include <map>
#include <ostream>

#if defined(DCMA_USE_POSTGRES) && defined(DCMA_USE_JANSSON)
    #include <jansson.h>         //For JSON handling.
    #include <pqxx/pqxx>         //PostgreSQL C++ interface.
#endif

#include <string>
#include <utility>

#include "YgorImages.h"
#include "YgorMath.h"
#include "YgorMisc.h"
#include "YgorStats.h"       //Needed for Stats:: namespace.
#include "YgorString.h"      //Needed for GetFirstRegex(...)


const auto boxr = 2; //The inclusive 'radius' of the square box to use to average nearby pixels. Controls amount of spatial averaging.

typedef std::map<std::string,std::string> analysis_key_t;

static std::map<analysis_key_t, double> moments;

static bool ComputeCentralizedMomentsWasRun = false;


bool ComputeCentralizedMoments(planar_image_collection<float,double>::images_list_it_t first_img_it,
                         std::list<planar_image_collection<float,double>::images_list_it_t> selected_img_its,
                         std::list<std::reference_wrapper<planar_image_collection<float,double>>>,
                         std::list<std::reference_wrapper<contour_collection<double>>> ccsl,
                         std::any ){
    ComputeCentralizedMomentsWasRun = true; //Remember, this routine is called several times: for each image or group.

    //This routine computes image moments within the given ROI.

    //Figure out if there are any contours for which are within the spatial extent of the image. 
    // There are many ways to do this! Since we are merely highlighting the contours, we scan 
    // all specified collections and treat them homogeneously.
    //
    // NOTE: We only bother to grab individual contours here. You could alter this if you wanted 
    //       each contour_collection's contours to have an identifying colour.
    if(ccsl.empty()) FUNCERR("Missing contour info needed for voxel colouring. Cannot continue");
/*
    typedef std::list<contour_of_points<double>>::iterator contour_iter;
    std::list<std::pair<std::reference_wrapper<contour_collection<double>>,contour_iter>> rois;
    for(auto &ccs : ccsl){
        for(auto it =  ccs.get().contours.begin(); it != ccs.get().contours.end(); ++it){
            if(it->points.empty()) continue;
            if(first_img_it->encompasses_contour_of_points(*it)){
                rois.push_back(it);
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

    //Loop over the rois, rows, columns, channels, and finally any selected images (if applicable).
    for(const auto & ref_wrapped_cc : ccsl){
        const auto AssumePlanarContours = true;
        const auto CCCentroid = ref_wrapped_cc.get().Centroid(AssumePlanarContours);

        for(const auto & roi : ref_wrapped_cc.get().contours){
            if(roi.points.empty()) continue;
            if(!first_img_it->encompasses_contour_of_points(roi)) continue;  
 
            //Try figure out the contour's name.
            const auto StudyInstanceUID = roi.GetMetadataValueAs<std::string>("StudyInstanceUID");
            const auto ROIName =  roi.GetMetadataValueAs<std::string>("ROIName");
            const auto FrameOfReferenceUID = roi.GetMetadataValueAs<std::string>("FrameOfReferenceUID");
            if(!StudyInstanceUID || !ROIName || !FrameOfReferenceUID){
                FUNCWARN("Missing necessary tags for reporting analysis results. Cannot continue");
                return false;
            }
            const analysis_key_t BaseAnalysisKey = { {"StudyInstanceUID", StudyInstanceUID.value()},
                                                     {"ROIName", ROIName.value()},
                                                     {"FrameOfReferenceUID", FrameOfReferenceUID.value()},
                                                     {"SpatialBoxr", Xtostring(boxr)},
                                                     {"Description", "Centralized moments over entire ROI"} };

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
            auto BestFitPlane = roi.Least_Squares_Best_Fit_Plane(ortho_unit);
            auto ProjectedContour = roi.Project_Onto_Plane_Orthogonally(BestFitPlane);
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
    
                            //Cycle over the neighbouring voxels, averaging pixel values.
                            double AveragedPixelValue = std::numeric_limits<double>::quiet_NaN();
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
                                AveragedPixelValue = Stats::Mean(in_pixs);
                            }
                            if(!std::isfinite(AveragedPixelValue)) continue;
         
 
                            for(int p = 0; p < 5; ++p) for(int q = 0; q < 5; ++q) for(int r = 0; r < 5; ++r){
                                const auto spatial = std::pow(point.x - CCCentroid.x,p)
                                                   * std::pow(point.y - CCCentroid.y,q)
                                                   * std::pow(point.z - CCCentroid.z,r);
                                const auto grid_factor = first_img_it->pxl_dx * first_img_it->pxl_dy * first_img_it->pxl_dz;
                                const auto dmoment = spatial * AveragedPixelValue * grid_factor;

                                analysis_key_t FullAnalysisKey(BaseAnalysisKey);
                                FullAnalysisKey["p"] = Xtostring(p);
                                FullAnalysisKey["q"] = Xtostring(q);
                                FullAnalysisKey["r"] = Xtostring(r);
                                moments[FullAnalysisKey] += dmoment;
                            }
    
                            //Update the value.
                            //working.reference(row, col, chan) = static_cast<float>(0);
    
                            // ----------------------------------------------------------------------------
    
                        }//Loop over channels.
                    } // If is in ROI or ROI bbox.
                } //Loop over cols
            } //Loop over rows
        } //Loop over ROIs.
    } //Loop over contour_collections.

    //Swap the original image with the working image.
    *first_img_it = working;

    //Alter the first image's metadata to reflect that averaging has occurred. You might want to consider
    // a selective whitelist approach so that unique IDs are not duplicated accidentally.
    first_img_it->metadata["Description"] = "Centralized moments map";

    return true;
}

#if defined(DCMA_USE_POSTGRES) && defined(DCMA_USE_JANSSON)
static bool Push_Moment_to_Database(analysis_key_t thekey, double themoment){
    // ---------------------------------- Convert key to JSON ---------------------------------------
    json_t *obj = json_object();
    for(const auto& apair : thekey){
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

    // -------------------------------------- Push to database -------------------------------------------
    try{
        pqxx::connection c("dbname=pacs user=hal host=localhost port=5432");
        pqxx::work txn(c);

        std::stringstream ss;
        ss << "INSERT INTO moments_for_bigart2015 ";
        ss << "    (Parameters,ImportTimepoint,moment) ";
        ss << "VALUES ";
        ss << "    ( " << txn.quote(thejson) << "::JSONB, now(),";
        ss << "      " << txn.quote(themoment) << " ) ";
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
#endif // DCMA_USE_POSTGRES

void DumpCentralizedMoments(std::map<std::string,std::string> InvocationMetadata){
    if(!ComputeCentralizedMomentsWasRun){
        FUNCWARN("Forgoing dumping the centralized moments results; the analysis was not run");
        return;
    }

    for(const auto& apair : moments){
        auto thekey = apair.first;
        auto themoment = apair.second;
      
#if defined(DCMA_USE_POSTGRES) && defined(DCMA_USE_JANSSON)
        thekey.insert(InvocationMetadata.begin(),InvocationMetadata.end());
        if(!Push_Moment_to_Database(thekey, themoment)){
            FUNCWARN("Unable to push analysis result to database. Ignoring and continuing");
        }
#else
        FUNCWARN("This program was not compiled with PostgreSQL+Jansson support -- unable to write moment to DB");
#endif // DCMA_USE_POSTGRES
    } 

    //Purge global state and clear the indicator for a fresh run.
    moments.clear();

    ComputeCentralizedMomentsWasRun = false;

    return;
}


