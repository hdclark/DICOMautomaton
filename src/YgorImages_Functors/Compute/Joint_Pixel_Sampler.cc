//Joint_Pixel_Sampler.cc.

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
#include "Joint_Pixel_Sampler.h"
#include "YgorImages.h"
#include "YgorMath.h"
#include "YgorMisc.h"
#include "YgorStats.h"       //Needed for Stats:: namespace.

#include "YgorClustering.hpp"


bool ComputeJointPixelSampler(planar_image_collection<float,double> &imagecoll,
                              std::list<std::reference_wrapper<planar_image_collection<float,double>>> external_imgs,
                              std::list<std::reference_wrapper<contour_collection<double>>> ccsl,
                              std::experimental::any user_data ){

    // This routine iterates over the selected voxels of an image, sampling all the spatially overlapping voxels from a
    // set of user-provided reference image arrays. A user-provided reduction function is used to condense all the
    // numbers down to a single number. The first image is overwritten with the reduced voxel value.
    //
    // This routine can be used to combine voxels in spatially-overlapping images. However, the images need not fully
    // overlap, nor do they need to align perfectly. Voxels in the external images will be interpolated as necessary.
    //
    // In this version all reference image array must be rectilinear.


    //We require a valid ComputeJointPixelSamplerUserData struct packed into the user_data.
    ComputeJointPixelSamplerUserData *user_data_s;
    try{
        user_data_s = std::experimental::any_cast<ComputeJointPixelSamplerUserData *>(user_data);
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

    if( ! user_data_s->f_reduce ){
        FUNCWARN("Reduction function is not valid; this operation will amount to a no-op. (Is this intentional?) Refusing continue");
        return false;
    }

    const auto ud_channel = user_data_s->channel;

    const auto inaccessible_val = std::numeric_limits<double>::quiet_NaN();

    // Ensure each reference image array forms a regular grid.
    for(auto & picrw : external_imgs){
        std::list<std::reference_wrapper<planar_image<float,double>>> selected_imgs;
        for(auto &img : picrw.get().images){
            selected_imgs.push_back( std::ref(img) );
        }

        if(!Images_Form_Rectilinear_Grid(selected_imgs)){
            FUNCWARN("Reference images do not form a rectilinear grid. Cannot continue");
            return false;
        }
    }

    Mutate_Voxels_Opts mv_opts;
    mv_opts.editstyle      = Mutate_Voxels_Opts::EditStyle::InPlace;
    mv_opts.inclusivity    = Mutate_Voxels_Opts::Inclusivity::Centre;
    mv_opts.contouroverlap = Mutate_Voxels_Opts::ContourOverlap::Ignore;
    mv_opts.aggregate      = Mutate_Voxels_Opts::Aggregate::First;
    mv_opts.adjacency      = Mutate_Voxels_Opts::Adjacency::SingleVoxel;
    mv_opts.maskmod        = Mutate_Voxels_Opts::MaskMod::Noop;


    std::mutex passing_counter; // Used to tally the gamma passing rate.

    asio_thread_pool tp;
    std::mutex saver_printer; // Who gets to save generated contours, print to the console, and iterate the counter.
    long int completed = 0;
    const long int img_count = imagecoll.images.size();

    for(auto &img : imagecoll.images){
        std::reference_wrapper< planar_image<float, double>> img_refw( std::ref(img) );

        tp.submit_task([&,img_refw](void) -> void {
            const auto orientation_normal = img_refw.get().image_plane().N_0.unit();

            // Prepare adjacency lists for each external image array.
            std::list< planar_image_adjacency<float,double> > img_adj_l;
            for(auto & picrw : external_imgs){
                decltype(external_imgs) shtl;
                shtl.emplace_back(picrw);
                std::list< std::reference_wrapper< planar_image<float,double> > > empty;
                img_adj_l.emplace_back( empty, shtl, orientation_normal );
            }

            // Identify the reference images which wholly overlap with the image to edit, if any.
            //
            // This arrangement is common in many scenarios and can be exploited to reduce costly checks for each voxel.
            // If no overlapping image is found, another lookup is performed for each voxel (which is much slower).
            using img_ptr_t = planar_image<float,double> *;
            std::list<img_ptr_t> int_img_ptr_l;
            bool envel_overlap = true; // Images that are enveloped, but may have different spatial characteristics.
            bool exact_overlap = true; // Images which have same spatial layout, number of rows and columns, etc.
            for(const auto & img_adj : img_adj_l){
                auto overlapping_img_refws = img_adj.get_wholly_overlapping_images(img_refw);
                if(!overlapping_img_refws.empty()){
                    int_img_ptr_l.emplace_back( std::addressof(overlapping_img_refws.front().get()) );

                    if( (img_refw.get().rows == overlapping_img_refws.front().get().rows)
                    &&  (img_refw.get().columns == overlapping_img_refws.front().get().columns)
                    &&  (img_refw.get().channels == overlapping_img_refws.front().get().channels)
                    &&  (0.99 < img_refw.get().row_unit.Dot(overlapping_img_refws.front().get().row_unit))
                    &&  (0.99 < img_refw.get().col_unit.Dot(overlapping_img_refws.front().get().col_unit)) ){
                        // exact_overlap *= 1.0;
                    }else{
                        exact_overlap = false;
                    }

                }else{
                    int_img_ptr_l.emplace_back( nullptr );
                    envel_overlap = false;
                    exact_overlap = false;
                }
            }
            if(!envel_overlap){
                std::lock_guard<std::mutex> lock(saver_printer);
                FUNCWARN("Reference images do not all envelop-overlap; using slow per-voxel sampling");
            }
            if(envel_overlap && !exact_overlap){
                std::lock_guard<std::mutex> lock(saver_printer);
                FUNCWARN("Reference images do not all exact-overlap; using per-image sampling");
            }

            auto f_bounded = [&,img_refw](long int E_row,  // "edit-image" row.
                                          long int E_col,  // "edit-image" column.
                                          long int channel, 
                                          std::reference_wrapper<planar_image<float,double>> /*img_refw*/, 
                                          float &voxel_val) {
                if( !isininc( user_data_s->inc_lower_threshold, voxel_val, user_data_s->inc_upper_threshold) ){
                    return; // No-op if outside of the thresholds.
                }
                if( (ud_channel >= 0) && (channel != ud_channel) ){
                    return; // No-op if this is the wrong channel.
                }

                // Tabulate all reference images sampled in order.
                std::vector<float> vals;
                vals.emplace_back(voxel_val);

                // Default the output to an invalid voxel value.
                voxel_val = inaccessible_val;

                // Get the position of the voxel in the image to edit.
                const auto pos = img_refw.get().position(E_row, E_col);

                // Sample each external image volume.
//                auto int_img_it = std::begin(int_img_ptr_l);
//                auto img_adj_it = std::begin(img_adj_l);
//                for( ;    (int_img_it != std::end(int_img_ptr_l)) 
//                       && (img_adj_it != std::end(img_adj_l))  ; ++int_img_it, ++img_adj_it ){
                const size_t N_ext_img_arrs = int_img_ptr_l.size();
                for(size_t i = 0; i < N_ext_img_arrs; ++i){                               // TODO: replace this dual iteration with iteration over a list of a class that combines both items (paired).
                    auto int_img_it = std::next( std::begin(int_img_ptr_l), i );
                    auto img_adj_it = std::next( std::begin(img_adj_l), i );

                    // Sample the image.
                    if(false){
                    }else if(exact_overlap){
                        vals.emplace_back( (*(int_img_it))->value(E_row, E_col, channel) );

                    }else if(user_data_s->sampling_method == ComputeJointPixelSamplerUserData::SamplingMethod::NearestVoxel){
                        // If no wholly overlapping image was previously identified, perform a lookup for this specific voxel.
                        // 
                        // Note: this is a costly pathway, but is necessary if images are disaligned.
                        img_ptr_t l_int_img_ptr = *(int_img_it); // De-reference the iterator to get the pointer..
                        if(l_int_img_ptr == nullptr){
                            try{
                                l_int_img_ptr = std::addressof( img_adj_it->position_to_image(pos).get() );
                            }catch(const std::exception &){
                                vals.emplace_back(inaccessible_val); // Cannot access this voxel.
                                continue;
                            }
                        }

                        // Ensure the image supports the specified channel.
                        if(l_int_img_ptr->channels <= channel){
                            vals.emplace_back(inaccessible_val); // Cannot access this voxel.
                            continue;
                        }

                        // Calculate the index in the intersecting image.
                        const auto index = l_int_img_ptr->index(pos, channel);
                        if(index < 0){ // If not valid, ignore the voxel.
                            vals.emplace_back(inaccessible_val); // Cannot access this voxel.
                            continue;
                        }

                        const auto sampled_val = l_int_img_ptr->value(index);
                        vals.emplace_back( sampled_val );

                    }else if(user_data_s->sampling_method == ComputeJointPixelSamplerUserData::SamplingMethod::LinearInterpolation){
                        const auto sampled_val = img_adj_it->trilinearly_interpolate(pos, channel);
                        vals.emplace_back( sampled_val );

                    }else{
                        throw std::runtime_error("Sampling method not understood. Cannot continue.");
                    }
                }

                // Apply the user's functor.
                try{
                    voxel_val = user_data_s->f_reduce( vals, pos );
                }catch(const std::exception &){ 
                    voxel_val = inaccessible_val;
                }
                return;
            };

            Mutate_Voxels<float,double>( img_refw,
                                         { img_refw },
                                         ccsl, 
                                         mv_opts, 
                                         f_bounded );

            UpdateImageDescription( img_refw, user_data_s->description );
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

