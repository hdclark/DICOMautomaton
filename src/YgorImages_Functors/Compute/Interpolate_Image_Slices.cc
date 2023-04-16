//Interpolate_Image_Slices.cc.

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
#include <cstdint>

#include "../../Thread_Pool.h"
#include "../Grouping/Misc_Functors.h"
#include "../ConvenienceRoutines.h"
#include "Interpolate_Image_Slices.h"
#include "YgorImages.h"
#include "YgorMath.h"
#include "YgorMisc.h"
#include "YgorLog.h"
#include "YgorStats.h"       //Needed for Stats:: namespace.

#include "YgorClustering.hpp"


bool ComputeInterpolateImageSlices(planar_image_collection<float,double> &imagecoll,
                          std::list<std::reference_wrapper<planar_image_collection<float,double>>> external_imgs,
                          std::list<std::reference_wrapper<contour_collection<double>>> /*ccsl*/,
                          std::any user_data ){

    // This routine interpolates image slices to match the geometry of a collection of reference images.
    // The purpose of such interpolation will often be to support direct voxel-to-voxel comparisons.
    //
    // The reference images (external_imgs) should be rectilinear. The images that will hold the interpolation
    // (imagecoll) should be co-rectilinear with the reference images and pre-allocated with the correct spatial
    // information -- the only thing that will be modified are the voxel values.
    //

/*
    // -----------------------------
    // The type of interpolation method to use.
    enum class
    InterpolationMethod {
        Linear,               // Linear interpolation without extrapolation at the extrema.
        LinearExtrapolation,  // Linear interpolation with extrapolation at the extrema.
    } interpolation_method = CInterpolationMethod::LinearExtrapolation;
*/

    //We require a valid ComputeInterpolateImageSlicesUserData struct packed into the user_data.
    ComputeInterpolateImageSlicesUserData *user_data_s;
    try{
        user_data_s = std::any_cast<ComputeInterpolateImageSlicesUserData *>(user_data);
    }catch(const std::exception &e){
        YLOGWARN("Unable to cast user_data to appropriate format. Cannot continue with computation");
        return false;
    }

/*
    if( ccsl.empty() ){
        YLOGWARN("Missing needed contour information. Cannot continue with computation");
        return false;
    }
*/

    if( external_imgs.empty() ){
        YLOGWARN("No reference images provided. Cannot continue");
        return false;
    }
/*    
    if( external_imgs.size() != 1 ){
        YLOGWARN("Too many reference images provided. Refusing to continue");
        return false;
    }
*/
    const auto ud_channel = user_data_s->channel;

/*
    const auto inaccessible_val = std::numeric_limits<double>::quiet_NaN();

    const auto machine_eps = std::sqrt(std::numeric_limits<double>::epsilon());

    const auto relative_diff = [](const double &A, const double &B) -> double {
        const auto max_abs = std::max( std::abs(A), std::abs(B) );
        const auto machine_eps = std::sqrt(std::numeric_limits<double>::epsilon());
        return (max_abs < machine_eps) ? 0.0 
                                       : std::abs(A-B) / max_abs;
    };
*/

    // Ensure the images form a regular grid.
    std::list<std::reference_wrapper<planar_image<float,double>>> all_imgs;
    for(auto &img : imagecoll.images){
        all_imgs.push_back( std::ref(img) );
    }

    std::list<std::reference_wrapper<planar_image<float,double>>> reference_imgs;
    for(auto &pic_refw : external_imgs){
        for(auto &img : pic_refw.get().images){
            reference_imgs.push_back( std::ref(img) );
            all_imgs.push_back( std::ref(img) );
        }
    }

    const bool ImagesAreRectilinear = Images_Form_Rectilinear_Grid(all_imgs);

    if(reference_imgs.empty()){
        YLOGWARN("No images are available to interpolate. Cannot continue");
        return false;
    }

/*
    Mutate_Voxels_Opts mv_opts;
    mv_opts.editstyle      = Mutate_Voxels_Opts::EditStyle::InPlace;
    mv_opts.inclusivity    = Mutate_Voxels_Opts::Inclusivity::Centre;
    mv_opts.contouroverlap = Mutate_Voxels_Opts::ContourOverlap::Ignore;
    mv_opts.aggregate      = Mutate_Voxels_Opts::Aggregate::First;
    mv_opts.adjacency      = Mutate_Voxels_Opts::Adjacency::SingleVoxel;
    mv_opts.maskmod        = Mutate_Voxels_Opts::MaskMod::Noop;
*/

/*
    const auto N_0 = reference_imgs.front().get().image_plane().N_0;
    planar_image_adjacency<float,double> img_adj( reference_imgs, {}, N_0 );
*/



    work_queue<std::function<void(void)>> wq;
    std::mutex saver_printer; // Who gets to save generated contours, print to the console, and iterate the counter.
    int64_t completed = 0;
    const int64_t img_count = imagecoll.images.size();

    for(auto &img : imagecoll.images){
        std::reference_wrapper< planar_image<float, double>> img_refw( std::ref(img) );

        wq.submit_task([&,img_refw]() -> void {

            const auto N_rows = img_refw.get().rows;
            const auto N_columns = img_refw.get().columns;
            const auto N_channels = img_refw.get().channels;

            //These parameters get updated by the following lambda.
            planar_image<float,double> *nearest_above = nullptr;
            planar_image<float,double> *nearest_below = nullptr;
            auto above_dist = std::numeric_limits<double>::infinity();
            auto below_dist = std::numeric_limits<double>::infinity();
            auto total_dist = std::numeric_limits<double>::infinity();

            //Identify the nearest planes above and below a specific point in space.
            const auto identify_nearest_adjacent_neighbours = [&](const vec3<double> pos) -> void {
                nearest_above = nullptr;
                nearest_below = nullptr;
                above_dist = std::numeric_limits<double>::infinity();
                below_dist = std::numeric_limits<double>::infinity();
                total_dist = std::numeric_limits<double>::infinity();

                for(auto &ref_img_refw : reference_imgs){
                    const auto theplane = ref_img_refw.get().image_plane();
                    const auto signed_dist = theplane.Get_Signed_Distance_To_Point(pos);
                    const auto is_above = (signed_dist >= static_cast<double>(0));
                    const auto dist = std::abs(signed_dist);

                    if(is_above){
                        if(dist < above_dist){
                            above_dist = dist;
                            nearest_above = std::addressof(ref_img_refw.get());
                        }
                    }else{
                        if(dist < below_dist){
                            below_dist = dist;
                            nearest_below = std::addressof(ref_img_refw.get());
                        }
                    }
                }

                // If there are two overlapping images, simply re-weight them equally but in a more
                // numerically-stable way.
                total_dist = below_dist + above_dist;
                if(total_dist < 1E-3){ 
                    above_dist = 1.0;
                    below_dist = 1.0;
                    total_dist = 2.0;
                }
                return;
            };

            
            // If all images are rectilinear, then no in-plane interpolation is needed and we can avoid adjacency lookup
            // for each voxel.
            if(ImagesAreRectilinear){
                const auto v_pos = img_refw.get().position(0, 0); // Pick any point...
                identify_nearest_adjacent_neighbours(v_pos);

                if( ( (nearest_below != nullptr) && (nearest_below->data.size() != img_refw.get().data.size()) )
                ||  ( (nearest_above != nullptr) && (nearest_above->data.size() != img_refw.get().data.size()) ) ){
                    throw std::logic_error("Non-rectilinear images found after rectilinearity check. Verify implementation.");
                }

                if( (nearest_above != nullptr) && (nearest_below != nullptr) ){
                    const auto N_elem = img_refw.get().data.size();
                    for(size_t i = 0; i < N_elem; ++i){
                        img_refw.get().data[i] = ( nearest_above->data[i] * below_dist
                                                 + nearest_below->data[i] * above_dist ) / total_dist;  
                    }

                }else if( (nearest_above == nullptr) && (nearest_below != nullptr) ){
                    img_refw.get().data = nearest_below->data;

                }else if( (nearest_above != nullptr) && (nearest_below == nullptr) ){
                    img_refw.get().data = nearest_above->data;

                }else{
                    std::logic_error("No neighbouring planes found. Cannot interpolate. Cannot continue.");
                }

            // If all images are NOT rectilinear, then in-plane interpolation is needed because the voxel
            // coordinates will differ in general..
            }else{
                for(auto row = 0; row < N_rows; ++row){
                    for(auto col = 0; col < N_columns; ++col){
                        for(auto chan = 0; chan < N_channels; ++chan){
                            if( (chan != ud_channel) && (ud_channel >= 0) ) continue;
                            const auto v_pos = img_refw.get().position(row, col);

                            //Identify the nearest planes above and below this voxel.
                            identify_nearest_adjacent_neighbours(v_pos);

                            float newval = std::numeric_limits<float>::quiet_NaN();

                            // Routine for projecting a point onto a planar image and interpolating in pixel coordinates.
                            auto project_and_interpolate = [chan]( planar_image<float,double> *img_ptr, vec3<double> pos ) -> double {
                                    auto proj_pos = img_ptr->image_plane().Project_Onto_Plane_Orthogonally(pos);

                                    // Note that interpolation will fail if out-of-bounds. 
                                    // If this happens, we have to live with it.
                                    double interp_val = std::numeric_limits<double>::quiet_NaN();
                                    try{
                                        const auto row_col = img_ptr->fractional_row_column(proj_pos);

                                        const auto row = row_col.first;
                                        const auto col = row_col.second;
                                        interp_val = img_ptr->bilinearly_interpolate_in_pixel_number_space(row, col, chan);

                                    }catch(const std::exception &){}
                                    return interp_val;
                            };

                            if( (nearest_above != nullptr) && (nearest_below != nullptr) ){
                                const auto val_a = project_and_interpolate(nearest_above,v_pos);
                                const auto val_b = project_and_interpolate(nearest_above,v_pos);
                                newval = ( val_a * below_dist
                                         + val_b * above_dist ) / total_dist;  // Note: Not a typo! Weights should be anti-paired.
                                
                            }else if( (nearest_above == nullptr) && (nearest_below != nullptr) ){
                                newval = project_and_interpolate(nearest_below,v_pos);

                            }else if( (nearest_above != nullptr) && (nearest_below == nullptr) ){
                                newval = project_and_interpolate(nearest_above,v_pos);

                            }else{
                                std::logic_error("No neighbouring planes found. Cannot interpolate. Cannot continue.");
                            }

                            img_refw.get().reference(row, col, chan) = newval;
                        }
                    }
                }
            }

            UpdateImageDescription( img_refw, user_data_s->description );
            UpdateImageWindowCentreWidth( img_refw );

            //Report operation progress.
            {
                std::lock_guard<std::mutex> lock(saver_printer);
                ++completed;
                YLOGINFO("Completed " << completed << " of " << img_count
                      << " --> " << static_cast<int>(1000.0*(completed)/img_count)/10.0 << "% done");
            }
        }); // thread pool task closure.

    }

    return true;
}

