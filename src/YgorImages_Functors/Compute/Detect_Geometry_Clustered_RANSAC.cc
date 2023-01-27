//Detect_Geometry_Clustered_RANSAC.cc.

#include <exception>
#include <any>
#include <optional>
#include <functional>
#include <list>
#include <map>
#include <algorithm>
#include <random>
#include <ostream>
#include <stdexcept>

#include "../Grouping/Misc_Functors.h"
#include "../ConvenienceRoutines.h"
#include "Detect_Geometry_Clustered_RANSAC.h"
#include "YgorImages.h"
#include "YgorMath.h"
#include "YgorMisc.h"
#include "YgorLog.h"
#include "YgorStats.h"       //Needed for Stats:: namespace.

#include "YgorClustering.hpp"


bool ComputeDetectGeometryClusteredRANSAC(planar_image_collection<float,double> &imagecoll,
                          std::list<std::reference_wrapper<planar_image_collection<float,double>>>,
                          std::list<std::reference_wrapper<contour_collection<double>>>,
                          std::any user_data ){

    //This routine performs shape detection in 3D to identify fixed-radius spheres. 
    //
    // 3D shape detection methods are computationally expensive. This routine should be provided images that already have
    // edges and/or lines separated from irrelevant voxels. A Canny edge detector is typical.
    // 
    // Contiguous image volumes must be processed together as a whole for proper 3D shape detection. Because grouping
    // is outside of the scope of this routine, all images are assumed to comprise a single volume. 
    //
    // All images must align exactly and contain the same number of rows and columns. If something more exotic or 
    // robust is needed, images muct be combined prior to calling this routine.
    //
    // This routine does not modify the images it uses, so there is no need to create copies.
    //

    //We require a valid DetectGeometryClusteredRANSACUserData struct packed into the user_data.
    DetectGeometryClusteredRANSACUserData *user_data_s;
    try{
        user_data_s = std::any_cast<DetectGeometryClusteredRANSACUserData *>(user_data);
    }catch(const std::exception &e){
        YLOGWARN("Unable to cast user_data to appropriate format. Cannot continue with computation");
        return false;
    }

    //Generate a comprehensive list of iterators to all as-of-yet-unused images. This list will be
    // pruned after images have been successfully operated on.
    auto all_images = imagecoll.get_all_images();
    while(!all_images.empty()){
        YLOGINFO("Images still to be processed: " << all_images.size());

        // Find the images which fit with this image.
        auto curr_img_it = all_images.front();
        //auto selected_imgs = GroupSpatiallyOverlappingImages(curr_img_it, std::ref(imagecoll));
        auto selected_imgs = all_images;

        if(selected_imgs.empty()){
            throw std::logic_error("No grouped images found. There should be at least one"
                                   " image (the 'seed' image) which should match. Verify the spatial" 
                                   " overlap grouping routine.");
        }
        {
          auto rows     = curr_img_it->rows;
          auto columns  = curr_img_it->columns;
          auto channels = curr_img_it->channels;

          for(const auto &an_img_it : selected_imgs){
              if( (rows     != an_img_it->rows)
              ||  (columns  != an_img_it->columns)
              ||  (channels != an_img_it->channels) ){
                  throw std::domain_error("Images have differing number of rows, columns, or channels."
                                          " This is not currently supported -- though it could be if needed."
                                          " Are you sure you've got the correct data?");
              }
              // NOTE: We assume the first image in the selected_images set is representative of the following
              //       images. We assume they all share identical row and column units, spatial extent, planar
              //       orientation, and (possibly) that row and column indices for one image are spatially
              //       equal to all other images. Breaking the last assumption would require an expensive 
              //       position_space-to-row_and_column_index lookup for each voxel.
          }
        }
        for(auto &an_img_it : selected_imgs){
             all_images.remove(an_img_it); //std::list::remove() erases all elements equal to input value.
        }

        // ----- Perform DBSCAN clustering -----

        constexpr size_t MaxElementsInANode = 6; // 16, 32, 128, 256, ... ?
        using RTreeParameter_t = boost::geometry::index::rstar<MaxElementsInANode>;

        typedef std::pair< planar_image<float,double>*, long int > UserData_t;
        typedef ClusteringDatum<3, double, // Spatial dimensions.
                                0, double, // Attribute dimensions (not used).
                                uint32_t,  // Cluster ID type.
                                UserData_t> CDat_t;
        //typedef boost::geometry::model::box<CDat_t> Box_t;
        typedef boost::geometry::index::rtree<CDat_t,RTreeParameter_t> RTree_t;

        RTree_t rtree;

        long int BeforeCount = 0;
        for(auto & img_it : selected_imgs){
            for(auto row = 0; row < img_it->rows; ++row){
                for(auto col = 0; col < img_it->columns; ++col){
                    for(auto chan = 0; chan < img_it->channels; ++chan){
                        const auto val = static_cast<double>(img_it->value(row, col, chan));
                        if(isininc( user_data_s->inc_lower_threshold, val, user_data_s->inc_upper_threshold)){
                            const auto p = img_it->position(row,col);
                            const auto index = img_it->index(row,col,chan);

                            rtree.insert(CDat_t({ p.x, p.y, p.z }, {}, 
                                                std::make_pair(&(*img_it), index) ));
                            ++BeforeCount;
                        }
                    }//Loop over channels.
                } //Loop over cols
            } //Loop over rows
        } // Loop over images.
        YLOGINFO("Number of voxels being clustered: " << BeforeCount);

        //const size_t MinPts = 6; // 2 * dimensionality.
        const size_t MinPts = 6; // 2 * dimensionality.
        const double Eps = 4.0; // DICOM units (mm).

        DBSCAN<RTree_t,CDat_t>(rtree,Eps,MinPts);

        //Record the min and max actual pixel values for windowing purposes.
        Stats::Running_MinMax<float> minmax_pixel;

        std::set<typename CDat_t::ClusterIDType_> unique_cluster_ids;
        long int AfterCount = 0;
        {
            constexpr auto RTreeSpatialQueryGetAll = [](const CDat_t &) -> bool { return true; };
            RTree_t::const_query_iterator it;
            it = rtree.qbegin(boost::geometry::index::satisfies( RTreeSpatialQueryGetAll ));
            for( ; it != rtree.qend(); ++it){
                const auto img_ptr = it->UserData.first;
                const auto index = it->UserData.second;

                if(it->CID.IsRegular()){
                    ++AfterCount;
                    const auto cluster_id = it->CID.Raw;
                    unique_cluster_ids.insert(cluster_id);

                    const auto new_val = static_cast<float>(cluster_id);
                    img_ptr->reference(index) = new_val;
                    minmax_pixel.Digest(new_val);

                }else{
                    const auto new_val = static_cast<float>(0);
                    img_ptr->reference(index) = new_val;
                    minmax_pixel.Digest(new_val);
                    continue;
                }
            }
        }
        YLOGINFO("Number of voxels with valid cluster IDs: " << AfterCount 
            << " (" << (1.0 / 100.0) * static_cast<long int>( 10000.0 * AfterCount / BeforeCount ) << "%)");


        //Segregate the data based on ClusterID.
        std::map<uint32_t, std::vector<CDat_t> > Segregated;
        if(true){
            constexpr auto RTreeSpatialQueryGetAll = [](const CDat_t &) -> bool { return true; };
            RTree_t::const_query_iterator it;
            it = rtree.qbegin(boost::geometry::index::satisfies( RTreeSpatialQueryGetAll ));
            for( ; it != rtree.qend(); ++it){
                if(it->CID.IsRegular()){
                    Segregated[it->CID.Raw].push_back( *it );
                }
            }
        }
        YLOGINFO("Number of unique clusters: " << Segregated.size());


        // ----- Fit the clusters -----
        for(const auto &cluster_p : Segregated){
            const auto ClusterID = cluster_p.first;

            std::vector<vec3<double>> positions;
            positions.reserve( cluster_p.second.size() );

            for(const auto &cdat : cluster_p.second){
                const auto img_ptr = cdat.UserData.first;
                const auto index = cdat.UserData.second;

                const auto rcc = img_ptr->row_column_channel_from_index(index);
                const auto row = std::get<0>(rcc);
                const auto col = std::get<1>(rcc);

                const auto pos = img_ptr->position(row, col);
                positions.push_back(pos);
            }

            if(positions.size() >= 4){
                try{ // Note that co-linear clusters will lead to infinite spheres that won't converge.


// NOTE: At the moment, no RANSAC is being performed. All elements (actually a random sampling of N of them) in each
// cluster is fitted to a sphere/plane. The clustered RANSAC algorithm will randomly sample inter and intra-cluster
// (the latter with a higher cost) to locate shapes.

                    const long int max_iters = 2500;
                    const double centre_stopping_tol = 0.05; // DICOM units (mm).
                    const double radius_stopping_tol = 0.05; // DICOM units (mm).
                    const size_t max_N_sample = 5000;
                    const unsigned int random_seed = 17317;
                    
                    // Randomly sample the positions (if there are many points) to reduce the fitting difficulty.
                    const size_t N_sample = std::min(positions.size(), max_N_sample);
                    std::vector<vec3<double>> sampled;
                    if(N_sample == positions.size()){ // Avoid sampling if everything will be used.
                        sampled = positions;
                    }else{
                        sampled.reserve(N_sample);
                        std::sample( std::begin(positions), std::end(positions),
                                     std::back_inserter(sampled), N_sample,
                                     std::mt19937{random_seed});
                    }

                    // Fit a sphere.
                    const auto asphere = Sphere_Orthogonal_Regression( sampled, max_iters, centre_stopping_tol, radius_stopping_tol );
                    YLOGINFO("The fitted sphere for cluster " << ClusterID << " has"
                          << " centre = " << asphere.C_0 << " and radius = " << asphere.r_0);


                    const auto aplane = Plane_Orthogonal_Regression( sampled );
                    YLOGINFO("The fitted plane for cluster " << ClusterID << " has"
                          << " anchor = " << aplane.R_0 << " and normal = " << aplane.N_0);

                }catch(const std::exception &e){
                    YLOGWARN("Fitting of cluster " << ClusterID << " failed to converge. Ignoring it");
                };
            }
        }


        for(auto & img_it : selected_imgs){
            UpdateImageDescription( std::ref(*img_it), "Clustered voxels" );
            UpdateImageWindowCentreWidth( std::ref(*img_it), minmax_pixel );
        }

    }

    return true;
}

