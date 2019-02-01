//Compare_Images.cc.

#include <exception>
#include <experimental/any>
#include <experimental/optional>
#include <functional>
#include <list>
#include <map>
#include <algorithm>
#include <random>
#include <ostream>
#include <stdexcept>

#include <boost/geometry.hpp>
#include <boost/geometry/geometries/point.hpp>
//#include <boost/geometry/geometries/box.hpp>

#include <boost/geometry/index/rtree.hpp>

#include "../Grouping/Misc_Functors.h"
#include "../ConvenienceRoutines.h"
#include "Compare_Images.h"
#include "YgorImages.h"
#include "YgorMath.h"
#include "YgorMisc.h"
#include "YgorStats.h"       //Needed for Stats:: namespace.

#include "YgorClustering.hpp"


bool ComputeCompareImages(planar_image_collection<float,double> &imagecoll,
                          std::list<std::reference_wrapper<planar_image_collection<float,double>>> external_imgs,
                          std::list<std::reference_wrapper<contour_collection<double>>> ccsl,
                          std::experimental::any user_data ){

    FUNCERR("This routine is not yet implemented.");

    // This routine compares pixel values between two image arrays in any combination of 2D and 3D. It support multiple
    // comparison types.
    //
    // Distance-to-agreement is a measure of how far away the nearest voxel (from the external set) is with a voxel
    // intensity sufficiently close to each voxel in the present image. This comparison ignores pixel intensities except
    // to test if the values match within the specified tolerance.
    //
    // A discrepancy comparison measures the point-dose intensity discrepancy without accounting for spatial shifts.
    //
    // A gamma analysis combines distance-to-agreement and point dose differences into a single index which is best used
    // to test if both DTA and discrepancy criteria are satisfied (gamma <= 1 iff both pass).
    // It was proposed by Low et al. in 1998 (doi:10.1118/1.598248). Gamma analyses permits trade-offs between spatial
    // and dosimetric discrepancies which can arise when the image arrays slightly differ in alignment or pixel values.
    //
    // Contiguous image volumes must be processed together as a whole for a proper 3D gamma analysis. Because grouping
    // is outside of the scope of this routine, all images are assumed to comprise a single volume. 
    //
    // For best results, image arrays should align and contain the same number of rows and columns. However, it is not
    // strictly necessary; image adjacency is determined by spatially sorting and voxel adjacency is determined by
    // projecting voxel positions onto adjacent images (nb. so the number of rows and columns are in some sense
    // irrelevant).


    //We require a valid ComputeCompareImagesUserData struct packed into the user_data.
    ComputeCompareImagesUserData *user_data_s;
    try{
        user_data_s = std::experimental::any_cast<ComputeCompareImagesUserData *>(user_data);
    }catch(const std::exception &e){
        FUNCWARN("Unable to cast user_data to appropriate format. Cannot continue with computation");
        return false;
    }

    if( ccsl.empty() ){
        FUNCWARN("Missing needed contour information. Cannot continue with computation");
        return false;
    }

    if( external_imgs.empty() ){
        FUNCWARN("No reference images provided. Cannot continue");
        return false;
    }
    if( external_imgs.size() != 1 ){
        FUNCWARN("Too many reference images provided. Refusing to continue");
        return false;
    }

    const auto channel = user_data_s->channel;

    std::list<std::reference_wrapper<planar_image<float,double>>> selected_imgs;
    for(auto &imgcoll_refw : external_imgs){
        for(auto &img : imgcoll_refw.get().images){
            selected_imgs.push_back( std::ref(img) );
        }
    }

    // Spatially-index the (relevant) voxels in the selected images so we can easily query for nearby voxels.
    constexpr size_t MaxElementsInANode = 16; // 16, 32, 128, 256, ... ?
    typedef boost::geometry::index::rstar<MaxElementsInANode> RTreeParameter_t;

    typedef float UserData_t;
    typedef boost::geometry::model::point<float, 3, boost::geometry::cs::cartesian> point_t;
    typedef std::pair<point_t, UserData_t> dat_t;
    typedef boost::geometry::index::rtree<dat_t,RTreeParameter_t> RTree_t;

    RTree_t rtree;
    std::reference_wrapper<RTree_t> rtree_refw( std::ref(rtree) ); // A copyable reference to the rtree.

    long int VoxelCount = 0;
    for(auto & img_refw : selected_imgs){
        for(auto row = 0; row < img_refw.get().rows; ++row){
            for(auto col = 0; col < img_refw.get().columns; ++col){
                const auto val = static_cast<double>(img_refw.get().value(row, col, channel));
                if(isininc( user_data_s->ref_img_inc_lower_threshold, val, user_data_s->ref_img_inc_upper_threshold)){
                    const auto p = img_refw.get().position(row,col);
                    //const auto index = img_refw.get().index(row,col,chan);
                    //rtree.insert(CDat_t({ p.x, p.y, p.z }, {}, 
                    //                    std::make_pair(&(*img_it), index) ));

                    point_t bgi_p(p.x, p.y, p.z);
                    rtree.insert(std::make_pair(bgi_p, val));
                    ++VoxelCount;
                }
            } //Loop over cols
        } //Loop over rows
    } // Loop over images.
    if( VoxelCount == 0 ){
        throw std::runtime_error("No voxels could be indexed. Unable to continue.");
        // Note: Check if the images contain any voxels and also check the thresholds are appropriate.
    }
    FUNCINFO("Number of voxels spatially indexed: " << VoxelCount);
    

    Mutate_Voxels_Opts mv_opts;
    mv_opts.editstyle      = Mutate_Voxels_Opts::EditStyle::InPlace;
    mv_opts.inclusivity    = Mutate_Voxels_Opts::Inclusivity::Centre;
    mv_opts.contouroverlap = Mutate_Voxels_Opts::ContourOverlap::Ignore;
    mv_opts.aggregate      = Mutate_Voxels_Opts::Aggregate::First;
    mv_opts.adjacency      = Mutate_Voxels_Opts::Adjacency::SingleVoxel;
    mv_opts.maskmod        = Mutate_Voxels_Opts::MaskMod::Noop;

    for(auto &img : imagecoll.images){
        std::reference_wrapper< planar_image<float, double>> img_refw( std::ref(img) );
        auto f_bounded = [=](long int row, long int col, long int channel, float &voxel_val) {
            if( !isininc( user_data_s->inc_lower_threshold, voxel_val, user_data_s->inc_upper_threshold) ){
                return; // No-op if outside of the thresholds.
            }

            //Get the position of the voxel.
            const auto pos = img_refw.get().position(row, col);

            // Find the nearest voxel for discrepancy assessment.
            point_t bgi_p(pos.x, pos.y, pos.z);
            RTree_t::const_query_iterator it;
            it = rtree_refw.get().qbegin(boost::geometry::index::nearest( bgi_p, 1));
            for( ; it != rtree_refw.get().qend(); ++it){
                break;
                //const auto bgi_p = it->first;
                //const auto val = it->second;
                // ... do something with bgi_p and val
            }

//            voxel_val = ;
            return;
        };

        Mutate_Voxels<float,double>( img_refw,
                                     { img_refw },  // Not a typo! Neighbour look-ups happen in the visitor functor.
                                     ccsl, 
                                     mv_opts, 
                                     f_bounded );

        UpdateImageDescription( img_refw, "Compared" );
        UpdateImageWindowCentreWidth( img_refw );
    }

// GAMEPLAN:
//  [X] 1. Move to Compute operation.
//  [X] 2. R*-tree index the entirety of selected imgs ASAP.
//  [X] 3. Use Mutate_Voxels() to limit the computation region to some ROIs.  (The update functional will call into the R*-tree).
//  [ ] 4. For each visited voxel:
//         IFF no exact image match: a. trilinearlly interpolate for the discrepancy assessment (( IFF the geometry does not match exactly for SOME selected image! -- and then warn about it! ))
//         Otherwise:                b. iterate over the nearest voxels processing them one-at-a-time to compute DTA.
//                                      - Break when you reach the distance limit.
//                                      - If you have seen one lower and then see one higher, consider that some interpolation will give the same value.
//                                        So accept the DTA agreement and worst-case assume the current voxel is the point of agreement (over estimates!)
//
//  [ ] 5. Parallelize the loop. (Or optionally parallelize Mutate_Voxels() somehow.)
//
//  [ ] 6. Assess if Sheppard interpolation should be added to Mutate_Voxels. It PROBABLY should... (Would require addition of Boost dependency for Ygor, but not too bad...)
//         NO! It naturally fits in as a separate routine that complements Mutate_Voxels!
////





/*

    //Partition external images into those above, below, and overlapping with this image.
    auto local_img_plane = local_img_it->image_plane();
    std::list< std::pair< double, std::reference_wrapper<planar_image<float,double>> > > ext_imgs_above;
    std::list< std::pair< double, std::reference_wrapper<planar_image<float,double>> > > ext_imgs_overlapping;
    std::list< std::pair< double, std::reference_wrapper<planar_image<float,double>> > > ext_imgs_below;
    for(auto & ext_img_refw : all_ext_imgs){
        const auto ext_img_plane = ext_img_refw.get().image_plane();
        const auto signed_dist = ext_img_plane.Get_Signed_Distance_To_Point(local_img_plane.R_0);
        const auto dist = std::abs(signed_dist);
        const auto overlap_dist_max = local_img_it->pxl_dz * 0.50;

        const auto overlaps = (dist < overlap_dist_max);
        const auto is_above = (signed_dist >= static_cast<double>(0));

        if(false){
        }else if(overlaps){
            ext_imgs_overlapping.push_back(std::make_pair<R,std::reference_wrapper<planar_image<T,R>>>(dist, std::ref(animg)));
        }else if(is_above){
            ext_imgs_above.push_back(std::make_pair<R,std::reference_wrapper<planar_image<T,R>>>(dist, std::ref(animg)));
        }else{
            ext_imgs_below.push_back(std::make_pair<R,std::reference_wrapper<planar_image<T,R>>>(dist, std::ref(animg)));
        }
    }

    //Sort both lists using the distance magnitude.
    const auto sort_less = [](const std::pair<R,std::reference_wrapper<planar_image<T,R>>> &A,
                              const std::pair<R,std::reference_wrapper<planar_image<T,R>>> &B) -> bool {
        return A.first < B.first;
    };
    ext_imgs_above.sort(sort_less);
    ext_imgs_overlapping.sort(sort_less);
    ext_imgs_below.sort(sort_less);

    if(ext_imgs_overlapping.empty()){
        FUNCWARN("No reference images overlap.
    ext_imgs_overlapping.resize(1);




    const auto out_of_bounds = std::numeric_limits<double>::quiet_NaN();

    const auto relative_diff = [](const double &A, const double &B) -> double {
        const auto max_abs = std::max( std::abs(A), std::abs(B) );
        const auto machine_eps = std::sqrt(std::numeric_limits<double>::epsilon());
        return (max_abs < machine_eps) ? 0.0 
                                       : std::abs(A-B) / max_abs;
    };

    for(auto row = 0; row < local_img_it->rows; ++row){
        for(auto col = 0; col < local_img_it->columns; ++col){
            for(auto chan = 0; chan < local_img_it->channels; ++chan){

                const auto Lval = local_img_it->value(row, col, chan);
                const auto Lpos = local_img_it->position(row, col);

                const auto Rval = ref_img_refw.get().trilinearly_interpolate(Lpos, chan, out_of_bounds);

                const auto Dis = relative_diff(Lval, Rval);

                const auto newval = std::isfinite(Rval) ? Dis : Rval;

                local_img_it->reference(row, col, chan) = newval;
                minmax_pixel.Digest(newval);
            }
        }
    }


    for(auto & ext_img : external_imgs){
        auto overlapping_imgs = ext_img.get().get_images_which_encompass_all_points(points);

        for(auto & overlapping_img : overlapping_imgs){

            const auto N_rows = local_img_it->rows;
            const auto N_cols = local_img_it->columns;
            if( (N_rows < 1) || (N_cols < 1) ) continue;


            // For images that exactly overlap.
            if( (local_img_it->rows == overlapping_img->rows)
            &&  (local_img_it->columns == overlapping_img->columns)
            &&  (local_img_it->channels == overlapping_img->channels)
            &&  (local_img_it->position(0,0) == overlapping_img->position(0,0)) 
            &&  (local_img_it->position(N_rows-1,0) == overlapping_img->position(N_rows-1,0)) 
            &&  (local_img_it->position(0,N_cols-1) == overlapping_img->position(0,N_cols-1)) ){

                for(auto row = 0; row < local_img_it->rows; ++row){
                    for(auto col = 0; col < local_img_it->columns; ++col){
                        for(auto chan = 0; chan < local_img_it->channels; ++chan){
                            const auto Lval = local_img_it->value(row, col, chan);
                            const auto Rval = overlapping_img->value(row, col, chan);
                            const auto newval = (Lval - Rval);
     
                            local_img_it->reference(row, col, chan) = newval;
                            minmax_pixel.Digest(newval);
                        }
                    }
                }

            // For images that need to be interpolated because they don't exactly overlap.
            // This is much slower.
            }else{
                for(auto row = 0; row < local_img_it->rows; ++row){
                    for(auto col = 0; col < local_img_it->columns; ++col){
                        for(auto chan = 0; chan < local_img_it->channels; ++chan){

                            const auto Lval = local_img_it->value(row, col, chan);
                            const auto Lpos = local_img_it->position(row, col);

                            try{
                                const auto R_row_col = overlapping_img->fractional_row_column(Lpos);

                                const auto R_row = R_row_col.first;
                                const auto R_col = R_row_col.second;
                                const auto Rval = overlapping_img->bilinearly_interpolate_in_pixel_number_space(R_row, R_col, chan);

                                const auto newval = (Lval - Rval);

                                local_img_it->reference(row, col, chan) = newval;
                                minmax_pixel.Digest(newval);
                            }catch(const std::exception &){}
                        }
                    }
                }
            }

        }
    }

    UpdateImageDescription( std::ref(*local_img_it), "Compared" );
    UpdateImageWindowCentreWidth( std::ref(*local_img_it), minmax_pixel );

    return true;
}







    //Generate a comprehensive list of iterators to all as-of-yet-unused images. This list will be
    // pruned after images have been successfully operated on.
    auto all_images = imagecoll.get_all_images();
    while(!all_images.empty()){
        FUNCINFO("Images still to be processed: " << all_images.size());

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
        typedef boost::geometry::index::rstar<MaxElementsInANode> RTreeParameter_t;

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
        FUNCINFO("Number of voxels being clustered: " << BeforeCount);

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
        FUNCINFO("Number of voxels with valid cluster IDs: " << AfterCount 
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
        FUNCINFO("Number of unique clusters: " << Segregated.size());


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
                    FUNCINFO("The fitted sphere for cluster " << ClusterID << " has"
                          << " centre = " << asphere.C_0 << " and radius = " << asphere.r_0);


                    const auto aplane = Plane_Orthogonal_Regression( sampled );
                    FUNCINFO("The fitted plane for cluster " << ClusterID << " has"
                          << " anchor = " << aplane.R_0 << " and normal = " << aplane.N_0);

                }catch(const std::exception &e){
                    FUNCWARN("Fitting of cluster " << ClusterID << " failed to converge. Ignoring it");
                };
            }
        }


        for(auto & img_it : selected_imgs){
            UpdateImageDescription( std::ref(*img_it), "Clustered voxels" );
            UpdateImageWindowCentreWidth( std::ref(*img_it), minmax_pixel );
        }

    }
*/

    return true;
}

