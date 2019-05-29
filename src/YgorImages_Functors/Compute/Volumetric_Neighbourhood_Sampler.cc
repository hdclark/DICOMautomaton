//Volumetric_Neighbourhood_Sampler.cc.

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

#include "../../Thread_Pool.h"
#include "../Grouping/Misc_Functors.h"
#include "../ConvenienceRoutines.h"
#include "Volumetric_Neighbourhood_Sampler.h"
#include "YgorImages.h"
#include "YgorMath.h"
#include "YgorMisc.h"
#include "YgorStats.h"       //Needed for Stats:: namespace.

#include "YgorClustering.hpp"


bool ComputeVolumetricNeighbourhoodSampler(planar_image_collection<float,double> &imagecoll,
                      std::list<std::reference_wrapper<planar_image_collection<float,double>>> /*external_imgs*/,
                      std::list<std::reference_wrapper<contour_collection<double>>> ccsl,
                      std::experimental::any user_data ){

    // This routine walks or samples the voxels of a 3D rectilinear image collection, invoking a user-provided functor
    // to reduce the sampled distribution of voxel values in the vicinity of each voxel to a scalar value, and then
    // updates the voxel value with this scalar. The primary benefit of this routine is that it provides a variety of
    // options for accessing the local neighbourhood of a voxel. Whole-neighbourhood options with boundaries specified
    // in terms of real-space (i.e., DICOM; in mm) and voxel-coordinate methods (i.e., integer triplets) are available.
    //
    // Note: The provided image collection must be rectilinear.
    //
    // Note: The image collection will be duplicated so that voxel modification can be accomplished directly, without
    //       worrying about modifications to the neighbourhood of adjacent voxels. Be aware that the copy is consulted
    //       as the pristine image collection so the provided image collection can be updated in-place. In particular,
    //       un-modified voxel values will be bit-stable. THIS WILL RUIN ADJACENCY COMPUTATION in the sense that any
    //       pre-computed or externally-computed image adjacency information will refer to the images being edited!
    //
    // Note: Because walking all voxels in 3D will inevitably be costly, contours are used to limit the computation.
    //

    //We require a valid ComputeVolumetricNeighbourhoodSamplerUserData struct packed into the user_data.
    ComputeVolumetricNeighbourhoodSamplerUserData *user_data_s;
    try{
        user_data_s = std::experimental::any_cast<ComputeVolumetricNeighbourhoodSamplerUserData *>(user_data);
    }catch(const std::exception &e){
        FUNCWARN("Unable to cast user_data to appropriate format. Cannot continue with computation");
        return false;
    }

    if( ccsl.empty() ){
        FUNCWARN("Missing needed contour information. Cannot continue with computation");
        return false;
    }

    if( ! user_data_s->f_reduce ){
        throw std::invalid_argument("User-provided reduction functor not valid. Cannot proceed.");
    }

    // Ensure the images form a regular grid.
    auto ref_imagecoll = imagecoll;
    
    std::list<std::reference_wrapper<planar_image<float,double>>> selected_imgs;
    for(auto &img : ref_imagecoll.images){
        selected_imgs.push_back( std::ref(img) );
    }

    if(!Images_Form_Rectilinear_Grid(selected_imgs)){
        FUNCWARN("Images do not form a rectilinear grid. Cannot continue");
        return false;
    }
    const bool is_regular_grid = Images_Form_Regular_Grid(selected_imgs);

    const auto orientation_normal = Average_Contour_Normals(ccsl);
    planar_image_adjacency<float,double> img_adj( {}, { { std::ref(ref_imagecoll) } }, orientation_normal );

    Mutate_Voxels_Opts mv_opts;
    mv_opts.editstyle      = Mutate_Voxels_Opts::EditStyle::InPlace;
    mv_opts.inclusivity    = Mutate_Voxels_Opts::Inclusivity::Centre;
    mv_opts.contouroverlap = Mutate_Voxels_Opts::ContourOverlap::Ignore;
    mv_opts.aggregate      = Mutate_Voxels_Opts::Aggregate::First;
    mv_opts.adjacency      = Mutate_Voxels_Opts::Adjacency::SingleVoxel;
    mv_opts.maskmod        = Mutate_Voxels_Opts::MaskMod::Noop;


    asio_thread_pool tp;
    std::mutex saver_printer; // Who gets to save generated contours, print to the console, and iterate the counter.
    long int completed = 0;
    const long int img_count = imagecoll.images.size();

    for(auto &img : imagecoll.images){
        std::reference_wrapper< planar_image<float, double>> img_refw( std::ref(img) );
        tp.submit_task([&,img_refw](void) -> void {

            // Identify the reference image which overlaps the whole image, if any.
            //
            // This approach attempts to identify a reference image which wholly overlaps the image to edit. This arrangement
            // is common in many scenarios and can be exploited to reduce costly checks for each voxel.
            // If no overlapping image is found, another lookup is performed for each voxel (which is much slower).
            auto overlapping_img_refws = img_adj.get_wholly_overlapping_images(img_refw);
            if(overlapping_img_refws.size() != 1){
                throw std::logic_error("Duplicated image volume does not self-overlap. Cannot continue.");
            }
            auto ref_img_refw = overlapping_img_refws.front();

            const auto pxl_dx = ref_img_refw.get().pxl_dx;
            const auto pxl_dy = ref_img_refw.get().pxl_dy;
            const auto pxl_dz = ref_img_refw.get().pxl_dz;

            std::vector<float> shtl;
            shtl.reserve(100); // An arbitrary guess.

            auto f_bounded = [&,ref_img_refw](long int E_row, long int E_col, long int channel, std::reference_wrapper<planar_image<float,double>> /*img_refw*/, float &voxel_val) {
                // No-op if this is the wrong channel.
                if( (user_data_s->channel >= 0) && (channel != user_data_s->channel) ){
                    return;
                }

                // Get the position of the voxel in the overlapping reference image.
                const auto E_pos = ref_img_refw.get().position(E_row, E_col);
                const auto E_val = ref_img_refw.get().value(E_row, E_col, channel);

                // Calculate the index in the intersecting image.
                const auto index = ref_img_refw.get().index(E_pos, channel);
                if(index < 0){
                    throw std::logic_error("Duplicated image volume differs in position. Cannot continue.");
                }

                // Determine the row, column, and image numbers for the reference image.
                const auto rcc = ref_img_refw.get().row_column_channel_from_index(index);
                const auto R_row = std::get<0>(rcc);
                const auto R_col = std::get<1>(rcc);
                if(!img_adj.image_present( ref_img_refw )){
                    throw std::logic_error("One or more images were not included in the image adjacency determination. Refusing to continue.");
                }
                const auto R_num = img_adj.image_to_index( ref_img_refw );

                shtl.clear();

                // Sample the neighbourhood in a growing cubic pattern until a spherical boundary is reached.
                // Growth of the pattern continues until the entire spherical neighbourhood has been sampled.
                if(user_data_s->neighbourhood == ComputeVolumetricNeighbourhoodSamplerUserData::Neighbourhood::Spherical){

                    // Create a growing 3D 'wavefront' in which the outer shell of a rectangular bunch of adjacent
                    // voxels is evaluated compared to the edit image's voxel value.
                    long int w = 0; // Neighbour voxel wavefront epoch number.
                    while(true){
                        double nearest_dist = std::numeric_limits<double>::infinity(); // Nearest of any voxel considered.

                        // Evaluate all voxels on this wavefront before proceeding.
                        for(long int k = -w; k < (w+1); ++k){
                            const auto l_num = R_num + k; // Adjacent image number.
                            if(!img_adj.index_present(l_num)) continue; // This adjacent image does not exist.
                            auto adj_img_refw = img_adj.index_to_image(l_num);

                            for(long int i = -w; i < (w+1); ++i){ 
                                const auto l_row = R_row + i;
                                if(!isininc(0, l_row, adj_img_refw.get().rows-1)) continue; // Wavefront surface not valid.
                                for(long int j = -w; j < (w+1); ++j){
                                    const auto l_col = R_col + j;
                                    if(!isininc(0, l_col, adj_img_refw.get().columns-1)) continue; // Wavefront surface not valid.

                                    // We only consider the voxels on the wavefront's surface . The wavefront is
                                    // characterized by at least one of i, j, or k being equal to w or -w.
                                    if( !(   (std::abs(k) == w)
                                          || (std::abs(i) == w)
                                          || (std::abs(j) == w) ) ) continue; // Not on the wavefront surface.

                                    const auto adj_vox_val = adj_img_refw.get().value(l_row, l_col, channel);
                                    const auto adj_vox_pos = adj_img_refw.get().position(l_row, l_col);
                                    const auto adj_vox_dist = adj_vox_pos.distance(E_pos);
                                    if(adj_vox_dist < nearest_dist) nearest_dist = adj_vox_dist;

                                    // Only contribute to the new voxel value if this voxel is within the spherical shell.
                                    //if(!isininc(user_data_s->ss_inner_radius, adj_vox_dist, user_data_s->ss_outer_radius)){
                                    if(adj_vox_dist > user_data_s->maximum_distance){
                                        continue;
                                    }

                                    shtl.emplace_back( adj_vox_val ) ;
                                }
                            }
                        }

                        if(!std::isfinite(nearest_dist)){
                            // No voxels found to assess within this epoch. Further epochs will be futile, so
                            // discontinue the search, taking whatever value (finite or infinite) was found to be best.
                            break; // note: voxel_val set below.
                        }
                        
                        if(nearest_dist > user_data_s->maximum_distance){
                            // Terminate the search if the user has instructed so.
                            // Take the current best value if there is any.
                            break; // note: voxel_val set below.
                        }

                        // Otherwise, advance the wavefront and continue searching.
                        ++w;
                    }

                // Sample the cubic neighbourhood of a regular grid.
                }else if( (user_data_s->neighbourhood == ComputeVolumetricNeighbourhoodSamplerUserData::Neighbourhood::Cubic )
                          && is_regular_grid ){

                    // Determine the extent of the cubic neighbourhood.
                    //
                    // Note: The neighbouring voxel CENTRE must be within the user-provided maximum distance.
                    const auto dx_u = static_cast<long int>( std::floor( user_data_s->maximum_distance / pxl_dx ) );
                    const auto dy_u = static_cast<long int>( std::floor( user_data_s->maximum_distance / pxl_dy ) );
                    const auto dz_u = static_cast<long int>( std::floor( user_data_s->maximum_distance / pxl_dz ) );

                    const long int l_row_min = std::max( R_row - dx_u, 0L );
                    const long int l_row_max = std::min( R_row + dx_u, ref_img_refw.get().rows - 1L );

                    const long int l_col_min = std::max( R_col - dy_u, 0L );
                    const long int l_col_max = std::min( R_col + dy_u, ref_img_refw.get().columns - 1L );

                    const long int l_img_min = (R_num - dz_u);
                    const long int l_img_max = (R_num + dz_u);

                    for(long int l_img = l_img_min; l_img <= l_img_max; ++l_img){
                        if(!img_adj.index_present(l_img)) continue; // This adjacent image does not exist.
                        auto adj_img_refw = img_adj.index_to_image(l_img);

                        for(long int l_row = l_row_min; l_row <= l_row_max; ++l_row){
                            for(long int l_col = l_col_min; l_col <= l_col_max; ++l_col){
                                const auto adj_vox_val = adj_img_refw.get().value(l_row, l_col, channel);
                                shtl.emplace_back( adj_vox_val ) ;
                            }
                        }
                    }

                // Sample the cubic neighbourhood of a rectilinear grid.
                }else if( (user_data_s->neighbourhood == ComputeVolumetricNeighbourhoodSamplerUserData::Neighbourhood::Cubic )
                          && !is_regular_grid ){
                    throw std::logic_error("Cubic neighbourhoods are not yet supported with non-regular image data.");
                    // Note: This will be simple to implement, but will probably require a hybrid approach of the
                    // spherical and cubic/regular approaches; a wavefront will need to be grown in the positive and
                    // negative image grid directions.

                // Sample specific voxels.
                }else if( user_data_s->neighbourhood == ComputeVolumetricNeighbourhoodSamplerUserData::Neighbourhood::Selection ){

                    for(const auto &triplets : user_data_s->voxel_triplets){
                        const auto l_row = R_row + triplets[0];
                        const auto l_col = R_col + triplets[1];
                        const auto l_img = R_num + triplets[2];

                        float res = std::numeric_limits<float>::quiet_NaN();
                        if(img_adj.index_present(l_img)
                        && isininc(0, l_row, ref_img_refw.get().rows - 1L)
                        && isininc(0, l_col, ref_img_refw.get().columns - 1L) ){
                            auto adj_img_refw = img_adj.index_to_image(l_img);
                            res = adj_img_refw.get().value(l_row, l_col, channel);
                        }
                        shtl.emplace_back( res );
                    }

                }else{
                    throw std::logic_error("Neighbourhood argument not understood.");

                }

                // Assign the voxel a value.
                voxel_val = user_data_s->f_reduce(E_val, shtl, E_pos);

                return;
            };

            Mutate_Voxels<float,double>( img_refw,
                                         { img_refw },
                                         ccsl, 
                                         mv_opts, 
                                         f_bounded );

            if(!(user_data_s->description.empty())){
                UpdateImageDescription( img_refw, user_data_s->description );
            }
            UpdateImageWindowCentreWidth( img_refw );

            //Report operation progress.
            {
                std::lock_guard<std::mutex> lock(saver_printer);
                ++completed;
                FUNCINFO("Completed " << completed << " of " << img_count
                      << " --> " << static_cast<int>(1000.0*(completed)/img_count)/10.0 << "\% done");
            }
        }); // thread pool task closure.

    }


    return true;
}

