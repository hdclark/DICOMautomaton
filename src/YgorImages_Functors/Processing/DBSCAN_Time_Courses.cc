
#include <YgorClusterID.hpp>
#include <YgorClusteringDBSCAN.hpp>
#include <YgorClusteringDatum.hpp>
#include <YgorClusteringHelpers.hpp>
#include <boost/geometry/index/parameters.hpp>
#include <boost/geometry/index/rtree.hpp>
#include <array>
#include <cmath>
#include <cstdint>
#include <any>
#include <optional>
#include <functional>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <random>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "../ConvenienceRoutines.h"
#include "DBSCAN_Time_Courses.h"
#include "YgorFilesDirs.h"   //Needed for Does_File_Exist_And_Can_Be_Read(...), etc..
#include "YgorImages.h"
#include "YgorMath.h"
#include "YgorMisc.h"
#include "YgorStats.h"       //Needed for Stats:: namespace.


bool DBSCANTimeCourses(planar_image_collection<float,double>::images_list_it_t first_img_it,
                       std::list<planar_image_collection<float,double>::images_list_it_t> selected_img_its,
                       std::list<std::reference_wrapper<planar_image_collection<float,double>>>,
                       std::list<std::reference_wrapper<contour_collection<double>>> ccsl, 
                       std::any /*user_data*/ ){

    //This routine requires a valid PerROITimeCoursesUserData struct packed into the user_data. Accept the throw
    // if the input is missing or invalid.

/*
    DBSCANTimeCoursesUserData *user_data_s;
    try{
        user_data_s = std::any_cast<DBSCANTimeCoursesUserData *>(&user_data);
    }catch(const std::exception &e){
        FUNCWARN("Unable to cast user_data to appropriate format. Cannot continue with computation");
        return false;
    }
*/

    //Work out some reasonable DBSCAN algorithm parameters.
    //const size_t MinPts = user_data_s->MinPts;
    //const double Eps = user_data_s->Eps;
    auto InputWindowCenter = first_img_it->GetMetadataValueAs<float>("WindowCenter");
    auto InputWindowWidth  = first_img_it->GetMetadataValueAs<float>("WindowWidth");
    if(!InputWindowCenter || !InputWindowWidth){
        FUNCWARN("The input image array does not have window information. This info is needed for a heuristic"
                 " that sets DBSCAN clustering parameters. Cannot continue");
        return false;
    }

    //const size_t NumberOfTimePoints = selected_img_its.size();
    const size_t MinPts = 5;
  
    const double Eps = static_cast<double>(InputWindowWidth.value())*2.0 / 20.0;  
    std::cout << "Using MinPts = " << MinPts << " and Eps = " << Eps << std::endl;


    //This routine performs a number of calculations. It is experimental and excerpts you plan to rely on should be
    // made into their own analysis functors.
    const bool InhibitSort = true; //Disable continuous sorting (defer to single sort later) to speed up data ingress.

    //Prepare suitable YgorClustering classes and a Boost.Geometry R*-tree.
    //Find a timestamp for each file. Attach the data to a ClusteringDatum_t and insert into a tree.
    constexpr size_t MaxElementsInANode = 6; // 16, 32, 128, 256, ... ?
    using RTreeParameter_t = boost::geometry::index::rstar<MaxElementsInANode>;
    constexpr size_t ClusteringSpatialDimensionCount = 100; // Can this be made dynamic? (Can Eps and MinPts choices cope?)
    typedef std::pair<size_t,size_t> ClusteringDatumUserData; //For storing row and column numbers.
    using ClusterIDRaw_t = uint32_t;
    typedef ClusteringDatum<ClusteringSpatialDimensionCount, double, 0, double, ClusterIDRaw_t, ClusteringDatumUserData> CDat_t;
    //typedef boost::geometry::model::box<CDat_t> Box_t;
    typedef boost::geometry::index::rtree<CDat_t,RTreeParameter_t> RTree_t;
    RTree_t rtree;


    //Record the min and max (outgoing) pixel values for windowing purposes.
    Stats::Running_MinMax<float> minmax_pixel;


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

    //Used to reject some data randomly, so the computational burden isn't so great.
    size_t FixedSeed = 9137;
    std::mt19937 re(FixedSeed);
    std::uniform_real_distribution<> rd(0.0, 1.0);


    //Loop over the ccsl, rois, rows, columns, channels, and finally any selected images (if applicable).
    //for(const auto &roi : rois){
    for(auto &ccs : ccsl){
        for(auto & contour : ccs.get().contours){
            if(contour.points.empty()) continue;
            //if(first_img_it->encompasses_contour_of_points(*it)) rois.push_back(it);

            //auto roi = *it;
            if(! first_img_it->encompasses_contour_of_points(contour)) continue;

            const auto ROIName =  contour.GetMetadataValueAs<std::string>("ROIName");
            if(!ROIName){
                FUNCWARN("Missing necessary tags for reporting analysis results. Cannot continue");
                return false;
            }
            
    /*
            //Try figure out the contour's name.
            const auto StudyInstanceUID = roi_it->GetMetadataValueAs<std::string>("StudyInstanceUID");
            const auto ROIName =  roi_it->GetMetadataValueAs<std::string>("ROIName");
            const auto FrameOfReferenceUID = roi_it->GetMetadataValueAs<std::string>("FrameOfReferenceUID");
            if(!StudyInstanceUID || !ROIName || !FrameOfReferenceUID){
                FUNCWARN("Missing necessary tags for reporting analysis results. Cannot continue");
                return false;
            }
            const analysis_key_t BaseAnalysisKey = { {"StudyInstanceUID", StudyInstanceUID.value()},
                                                     {"ROIName", ROIName.value()},
                                                     {"FrameOfReferenceUID", FrameOfReferenceUID.value()},
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
            auto BestFitPlane = contour.Least_Squares_Best_Fit_Plane(ortho_unit);
            auto ProjectedContour = contour.Project_Onto_Plane_Orthogonally(BestFitPlane);
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


                            // --------------- Load the time course into the R*-tree ----------------

                            if(rd(re) < 0.2){
                                const ClusteringDatumUserData ImageCoords(row,col);
                                std::array<double,ClusteringSpatialDimensionCount> DataVec;
                                for(auto & coord : DataVec) coord = 0.0;
                                for(size_t i = 0; (i < channel_time_course.size()) 
                                                  && (i < ClusteringSpatialDimensionCount) ; ++i){
                                    DataVec[i] = channel_time_course.samples[i][2];
                                }
                                rtree.insert(CDat_t(DataVec, {}, ImageCoords)); 
                            }
    
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

    //Produce a k-distance plot so the user can visually identify a suitable value for the DBSCAN Eps parameter.
    if(true){
        //auto SortedkDistGraphData = DBSCANSortedkDistGraph<RTree_t,CDat_t>(rtree,MinPts);
        auto SortedkDistGraphData = DBSCANSortedkDistGraph<RTree_t,CDat_t>(rtree,MinPts);
        samples_1D<double> kDistGraph;
        double x = 1.0;
        for(auto & Datum : SortedkDistGraphData){
            kDistGraph.samples.push_back({ x, 0.0, Datum, 0.0 });
            x += 1.0;
        }
        kDistGraph.Plot("k-Dist Graph");
        kDistGraph.Plot_as_PDF("k-Dist Graph", 
                               Get_Unique_Sequential_Filename("/tmp/k-dist-graph_", 4, ".pdf"));
        kDistGraph.Write_To_File(Get_Unique_Sequential_Filename("/tmp/k-dist-graph_", 4, ".dat"));
    }

    //Perform the clustering.
    DBSCAN<RTree_t,CDat_t>(rtree,Eps,MinPts);

    //Now store the cluster data info in a map for easier access.
    std::map<ClusteringDatumUserData,ClusterID<ClusterIDRaw_t>> ClusterIDMap;
    std::set<ClusterID<ClusterIDRaw_t>> ClusterIDUniques;
    OnEachDatum<RTree_t,CDat_t,std::function<void(const typename RTree_t::const_query_iterator &)>>(rtree,
        [&ClusterIDMap,&ClusterIDUniques](const typename RTree_t::const_query_iterator &it) -> void {
            ClusterIDMap[ it->UserData ] = it->CID;
            ClusterIDUniques.insert( it->CID );
        });
    
    {
        const auto NumberOfClusters = ClusterIDUniques.size();
//        user_data_s->NumberOfClusters = NumberOfClusters;
        std::cout << "Found " << NumberOfClusters 
                  << " clusters:"
                  << std::endl; 
        for(auto & aClusterID : ClusterIDUniques) std::cout << "    " << aClusterID.ToText() << std::endl;                               
    }

    //Loop over the working image, updating pixel values.
    for(auto row = 0; row < working.rows; ++row){
        for(auto col = 0; col < working.columns; ++col){
            for(auto chan = 0; chan < first_img_it->channels; ++chan){

                const auto theClusterID = ClusterIDMap[ std::make_pair<size_t,size_t>(row,col) ];
                const auto newval = static_cast<float>(theClusterID.Raw);
                working.reference(row, col, chan) = newval;

                if(theClusterID.IsRegular()) minmax_pixel.Digest(newval);

            } //Loop over channels
        } //Loop over columnss.
    } //Loop over rows.

    //Swap the original image with the working image.
    *first_img_it = working;

    //Alter the first image's metadata to reflect that averaging has occurred. You might want to consider
    // a selective whitelist approach so that unique IDs are not duplicated accidentally.
    UpdateImageDescription( std::ref(*first_img_it), "DBSCAN Time Course Clustered" );
    UpdateImageWindowCentreWidth( std::ref(*first_img_it), minmax_pixel );

    return true;
}


