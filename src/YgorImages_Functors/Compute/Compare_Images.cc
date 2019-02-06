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

    // This routine compares pixel values between two image arrays in any combination of 2D and 3D. It support multiple
    // comparison types, but all consider **only** voxel-to-voxel comparisons -- interpolation is **not** used.
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
    // The reference image array must be rectilinear.
    // For the fastest and most accurate results, test and reference image arrays should exactly align. However, it is not
    // necessary. Ii test and reference image arrays are aligned, image adjacency is precomputed. Otherwise image
    // adjacency is evaluated for every voxel.


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

    const auto ud_channel = user_data_s->channel;

    const auto inaccessible_val = std::numeric_limits<double>::quiet_NaN();

    const auto machine_eps = std::sqrt(std::numeric_limits<double>::epsilon());

    const auto relative_diff = [](const double &A, const double &B) -> double {
        const auto max_abs = std::max( std::abs(A), std::abs(B) );
        const auto machine_eps = std::sqrt(std::numeric_limits<double>::epsilon());
        return (max_abs < machine_eps) ? 0.0 
                                       : std::abs(A-B) / max_abs;
    };

    // Ensure the reference images form a regular grid.
    {
        std::list<std::reference_wrapper<planar_image<float,double>>> selected_imgs;
        for(auto &imgcoll_refw : external_imgs){
            for(auto &img : imgcoll_refw.get().images){
                selected_imgs.push_back( std::ref(img) );
            }
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

    size_t N_remaining_imgs = imagecoll.images.size();

    for(auto &img : imagecoll.images){
        std::reference_wrapper< planar_image<float, double>> img_refw( std::ref(img) );

//------------------ image indexing sub-routine -----------------

        const auto orientation_normal = img_refw.get().image_plane().N_0.unit();

        // Provide a look-up for quickly finding the nearest image for a given point.
        //
        // Note: Planes are used here, but could also use distance to image centres.
        //       Then at least you only have to work with vec3s rather than vec3s + planes.
        using img_ptr_t = planar_image<float,double>*;
        using img_planes_t = std::pair< plane<double>, img_ptr_t >;
        std::vector<img_planes_t> img_plane_to_img;
        for(auto &imgcoll_refw : external_imgs){
            for(auto &l_img : imgcoll_refw.get().images){
                auto plane = l_img.image_plane();
                img_plane_to_img.emplace_back( std::make_pair(plane, std::addressof(l_img)) );
            }
            //for(auto img_it = std::begin(imgcoll_refw.get().images); img_it != std::end(imgcoll_refw.get().images); ++img_it){
            //    auto plane = img_it->image_plane();
            //    img_plane_to_img.emplace_back( std::make_pair(plane, std::addressof(*img_it)) );
            //}
        }
        const auto pos_to_img = [&](const vec3<double> &pos){ // Find the nearest image plane; not necessarily overlapping!
            const auto it = std::min_element( std::begin(img_plane_to_img), 
                                              std::end(img_plane_to_img),
                                              [&](const img_planes_t &lhs, const img_planes_t &rhs){
                                                  const auto lhs_dist = std::abs(std::get<0>(lhs).Get_Signed_Distance_To_Point(pos));
                                                  const auto rhs_dist = std::abs(std::get<0>(rhs).Get_Signed_Distance_To_Point(pos));
                                                  return (lhs_dist < rhs_dist);
                                              } );
            if(it == std::end(img_plane_to_img)){
                throw std::logic_error("No nearest image can be located.");
            }
            return std::get<1>(*it);
        };

        // Sort the images along the planar normal. 
        std::sort( std::begin(img_plane_to_img),
                   std::end(img_plane_to_img),
                   [&](const img_planes_t &lhs, const img_planes_t &rhs){
                       const auto lhs_proj = (std::get<0>(lhs).R_0.Dot( orientation_normal ));
                       const auto rhs_proj = (std::get<0>(rhs).R_0.Dot( orientation_normal ));
                       return (lhs_proj < rhs_proj);
                   } );

        std::map<long int, img_ptr_t> int_to_img;
        std::map<img_ptr_t, long int> img_to_int;
        long int dummy = 1; // Arbitrary, but 0 is also the default map value initializer.
        for(auto &p : img_plane_to_img){
            const auto img_ptr = std::get<1>(p);
            int_to_img[dummy] = img_ptr;
            img_to_int[img_ptr] = dummy;
            ++dummy;
        }

//------------------------------------------------------------------------
        
        //---------
        // Identify the reference image which overlaps the whole image, if any.
        //
        // This approach attempts to identify a reference image which wholy overlaps the image to edit. This arrangement
        // is common in many scenarios and can be exploited to reduce costly checks for each voxel.
        // If no overlapping image is found, another lookup is performed for each voxel (which is much slower).
        if( (img_refw.get().rows < 1) || (img_refw.get().columns < 1) ){
            throw std::runtime_error("Edit image dimensions are too small to compare. Cannot continue.");
        }
        const auto corner_A = img_refw.get().position(0,0);
        const auto corner_B = img_refw.get().position(img_refw.get().rows-1,0);
        const auto corner_C = img_refw.get().position(0,img_refw.get().columns-1);

        auto int_img_its = external_imgs.front().get().get_images_which_encompass_all_points({ corner_A, corner_B, corner_C });
        img_ptr_t int_img_ptr = (int_img_its.empty()) ? nullptr
                                                      : std::addressof(*(int_img_its.front()));
        if(int_img_its.empty()) FUNCWARN("No wholy overlapping reference images found, using slower per-voxel sampling");
        //---------


        auto f_bounded = [&,img_refw](long int E_row, long int E_col, long int channel, float &voxel_val) {
            if( !isininc( user_data_s->inc_lower_threshold, voxel_val, user_data_s->inc_upper_threshold) ){
                return; // No-op if outside of the thresholds.
            }
            if( channel != ud_channel){
                return; // No-op if this is the wrong channel.
            }

            do{
                const auto edit_val = voxel_val;

                // Get the position of the voxel in the overlapping reference image.
                const auto pos = img_refw.get().position(E_row, E_col);

                // If no wholy overlapping image was identified, perform a lookup for this specific voxel.
                img_ptr_t l_int_img_ptr = int_img_ptr;
                if(l_int_img_ptr == nullptr){
                    try{
                        l_int_img_ptr = pos_to_img(pos);
                    }catch(const std::exception &){
                        voxel_val = inaccessible_val; // Cannot assess this voxel.
                        return;
                    }
                }

                // Ensure the image supports the specified channel.
                if(l_int_img_ptr->channels <= channel){
                    voxel_val = inaccessible_val;
                    break;
                }

                // Calculate the index in the intersecting image.
                const auto index = l_int_img_ptr->index(pos, channel);
                if(index < 0){ // If not valid, ignore the voxel.
                    voxel_val = inaccessible_val;
                    break;
                }

                // Verify if the voxel needs to be compared.
                const auto ring_0_val = l_int_img_ptr->value(index);
                if(!isininc( user_data_s->ref_img_inc_lower_threshold, ring_0_val, user_data_s->ref_img_inc_upper_threshold)){
                    voxel_val = inaccessible_val;
                    break;
                }

                // Determine the row, column, and image numbers for the reference image.
                const auto rcc = l_int_img_ptr->row_column_channel_from_index(index);
                const auto R_row = std::get<0>(rcc);
                const auto R_col = std::get<1>(rcc);
                const auto R_num = img_to_int.at( std::addressof( *l_int_img_ptr ) );

                // Determine the smallest dimension of the voxel, protecting against the pxl_dz = 0 case.
                const auto pxl_dx = l_int_img_ptr->pxl_dx;
                const auto pxl_dy = l_int_img_ptr->pxl_dy;
                const auto pxl_dz = l_int_img_ptr->pxl_dz;
                const auto pxl_dl = std::max( std::min({ pxl_dx, pxl_dy, pxl_dz }), 10.0 * machine_eps );

                // Ensure the voxel position in the edit image and reference image match reasonably.
                const auto ring_0_pos = l_int_img_ptr->position(R_row, R_col);
                if(ring_0_pos.distance(pos) > pxl_dl){ // If no suitable voxel for discrepancy testing, ignore voxel.
                    voxel_val = inaccessible_val;
                    break;
                }

                // Perform a discrepancy comparison.
                const auto Disc = relative_diff(edit_val, ring_0_val);

                // If computing the gamma index, check if we can avoid a costly DTA search.
                if( (user_data_s->comparison_method == ComputeCompareImagesUserData::ComparisonMethod::GammaIndex) 
                &&  (user_data_s->gamma_terminate_when_max_exceeded)
                &&  (100.0 * Disc > user_data_s->gamma_Dis_reldiff_threshold) ){
                    voxel_val = user_data_s->gamma_terminated_early;
                    break;
                }

                // Perform a DTA analysis IFF needed.
                double Dist = std::numeric_limits<double>::infinity();
                if( (user_data_s->comparison_method == ComputeCompareImagesUserData::ComparisonMethod::DTA)
                    ||
                    ( 
                       (user_data_s->comparison_method == ComputeCompareImagesUserData::ComparisonMethod::GammaIndex)
                       &&
                       (std::isfinite(Disc)) 
                    ) ){

                    // Create a growing 3D 'wavefront' in which the outer shell of a rectangular bunch of adjacent
                    // voxels is evaluated compared to the edit image's voxel value.
                    long int w = 0;                  // Neighbour voxel wavefront epoch number.
                    bool encountered_lower = false;  // Whether a voxel lower than required was found.
                    bool encountered_higher = false; // Whether a voxel higher than required was found.
                    while(true){
                        double nearest_dist = std::numeric_limits<double>::infinity(); // Nearest of any voxel considered.

                        // Evaluate all voxels on this wavefront before proceeding.
                        for(long int k = -w; k < (w+1); ++k){
                            const auto l_num = R_num + k; // Adjacent image number.
                            if(int_to_img.count(l_num) == 0) continue; // This adjacent image does not exist.
                            auto adj_img_ptr = int_to_img[l_num];

                            for(long int i = -w; i < (w+1); ++i){ 
                                const auto l_row = R_row + i;
                                if(!isininc(0, l_row, adj_img_ptr->rows-1)) continue; // Wavefront surface not valid.
                                for(long int j = -w; j < (w+1); ++j){
                                    const auto l_col = R_col + j;
                                    if(!isininc(0, l_col, adj_img_ptr->columns-1)) continue; // Wavefront surface not valid.

                                    // We only consider the voxels on the wavefront's surface . The wavefront is
                                    // characterized by at least one of i, j, or k being equal to w or -w.
                                    if( !(   (std::abs(k) == w)
                                          || (std::abs(i) == w)
                                          || (std::abs(j) == w) ) ) continue; // Not on the wavefront surface.

                                    // Update the current nearest suitable voxel, if appropriate.
                                    //
                                    // Note: We often have to continue to search to ensure no better match is available.
                                    //       This is because we search a rectangular wavefront but are interested in an
                                    //       ellipsoid (or spherical) shell of voxels.
                                    const auto adj_img_val = adj_img_ptr->value(l_row, l_col, channel);
                                    const auto adj_vox_pos = adj_img_ptr->position(l_row, l_col);
                                    const auto adj_vox_dist = adj_vox_pos.distance(ring_0_pos);
                                    if(adj_vox_dist < nearest_dist) nearest_dist = adj_vox_dist;

                                    // Check if voxel values have been seen both above and below the desired value.
                                    // If so, then the grid can be interpolated (at some unknown location) to achieve
                                    // the desired value, so we count the current voxel as a match (necessarily
                                    // over-estimating the value somewhat).
                                    const bool is_lower = (adj_img_val < edit_val);
                                    const bool is_higher = (edit_val < adj_img_val);
                                    if(!encountered_lower && is_lower){
                                        encountered_lower = true;
                                    }
                                    if(!encountered_higher && is_higher){
                                        encountered_higher = true;
                                    }
                                        
                                    // Evaluate whether this voxel should be marked as the current best.
                                    if(adj_vox_dist < Dist){
                                        if( false
                                        || ( encountered_lower && is_higher )
                                        || ( encountered_higher && is_lower )
                                        || ( std::abs(adj_img_val - edit_val) < user_data_s->DTA_vox_val_eq_abs )
                                        || ( 100.0*relative_diff(adj_img_val, edit_val) < user_data_s->DTA_vox_val_eq_reldiff ) ){
                                            Dist = adj_vox_dist;
                                        }
                                    }
                                }
                            }
                        }

                        if(Dist < nearest_dist){
                            // It is now impossible to improve the DTA because the next wavefront will all necessarily
                            // be further away. So terminate the search.
                            break; // note: voxel_val set below.
                        }
                        
                        if(!std::isfinite(nearest_dist)){
                            // No voxels found to assess within this epoch. Further epochs will be futile, so
                            // discontinue the search, taking whatever value (finite or infinite) was found to be best.
                            break; // note: voxel_val set below.
                        }
                        
                        if(nearest_dist > user_data_s->DTA_max){
                            // Terminate the search if the user has instructed so.
                            // Take the current best value if there is any.
                            break; // note: voxel_val set below.
                        }

                        // If computing the gamma index, check if we can avoid continuing the DTA search since gamma
                        // will necessarily be >1 at this point.
                        if( (user_data_s->gamma_terminate_when_max_exceeded)
                        &&  (nearest_dist > user_data_s->gamma_DTA_threshold) ){
                            voxel_val = user_data_s->gamma_terminated_early;
                            return;
                        }

                        // Otherwise, advance the wavefront and continue searching.
                        ++w;
                    }
                }

                // Assign the voxel a value.
                if(false){
                }else if(user_data_s->comparison_method == ComputeCompareImagesUserData::ComparisonMethod::Discrepancy){
                    voxel_val = Disc;

                }else if(user_data_s->comparison_method == ComputeCompareImagesUserData::ComparisonMethod::DTA){
                    if(std::isfinite(Dist)){
                        voxel_val = Dist;
                    }else{
                        voxel_val = inaccessible_val;
                    }

                }else if(user_data_s->comparison_method == ComputeCompareImagesUserData::ComparisonMethod::GammaIndex){
                    if(std::isfinite(Dist) && std::isfinite(Disc)){
                        voxel_val = std::sqrt( std::pow(Dist / user_data_s->gamma_DTA_threshold, 2.0)
                                             + std::pow(100.0 * Disc / user_data_s->gamma_Dis_reldiff_threshold, 2.0) );

                    }else{
                        voxel_val = inaccessible_val;
                    }

                }else{
                    throw std::logic_error("Unrecognized comparison operation requested. Refusing to continue.");
                }
                return;
            }while(false);
            return;
        };

        Mutate_Voxels<float,double>( img_refw,
                                     { img_refw },
                                     ccsl, 
                                     mv_opts, 
                                     f_bounded );

        if(false){
        }else if(user_data_s->comparison_method == ComputeCompareImagesUserData::ComparisonMethod::Discrepancy){
            UpdateImageDescription( img_refw, "Compared (discrepancy)" );
        }else if(user_data_s->comparison_method == ComputeCompareImagesUserData::ComparisonMethod::DTA){
            UpdateImageDescription( img_refw, "Compared (DTA)" );
        }else if(user_data_s->comparison_method == ComputeCompareImagesUserData::ComparisonMethod::GammaIndex){
            UpdateImageDescription( img_refw, "Compared (gamma-index)" );
        }
        UpdateImageWindowCentreWidth( img_refw );

        FUNCINFO("Images still to be assessed: " << N_remaining_imgs);
        --N_remaining_imgs;
    }


    return true;
}

