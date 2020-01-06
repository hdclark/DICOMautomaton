//Compare_Images.cc.

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

#include "../../Thread_Pool.h"
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
                          std::any user_data ){

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
        user_data_s = std::any_cast<ComputeCompareImagesUserData *>(user_data);
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

    // Determine how discrepancy should be estimated.
    std::function< double (const double &, const double &) > estimate_discrepancy;
    if(false){
    }else if(user_data_s->discrepancy_type == ComputeCompareImagesUserData::DiscrepancyType::Relative){
        estimate_discrepancy = relative_diff;

    }else if(user_data_s->discrepancy_type == ComputeCompareImagesUserData::DiscrepancyType::Difference){
        estimate_discrepancy = [](const double &A, const double &B) -> double {
            return std::abs(A - B);
        };

    }else if(user_data_s->discrepancy_type == ComputeCompareImagesUserData::DiscrepancyType::PinnedToMax){
        Stats::Running_MinMax<float> rmm;
        auto find_max = [&rmm,ud_channel](long int, long int, long int chnl, float val) -> void {
            if(chnl == ud_channel){
                rmm.Digest(val);
            }
            return;
        };
        imagecoll.apply_to_pixels(find_max);
        // TODO: identify max_val from all images, or just the selected images?
        // ...

        const auto max_val = rmm.Current_Max();
        FUNCINFO("Maximum intensity found: " << max_val);
        estimate_discrepancy = [max_val](const double &A, const double &B) -> double {
            return std::abs( (A - B) / max_val );
        };

    }else{
        throw std::invalid_argument("Unknown discrepancy method requested. Cannot continue.");
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

            planar_image_adjacency<float,double> img_adj( {}, external_imgs, orientation_normal );

            using img_ptr_t = planar_image<float,double> *;

            // Identify the reference image which overlaps the whole image, if any.
            //
            // This approach attempts to identify a reference image which wholly overlaps the image to edit. This arrangement
            // is common in many scenarios and can be exploited to reduce costly checks for each voxel.
            // If no overlapping image is found, another lookup is performed for each voxel (which is much slower).
            auto overlapping_img_refws = img_adj.get_wholly_overlapping_images(img_refw);
            img_ptr_t int_img_ptr = (overlapping_img_refws.empty()) ? nullptr
                                                                    : std::addressof(overlapping_img_refws.front().get());
            if(overlapping_img_refws.empty()) FUNCWARN("No wholly overlapping reference images found, using slower per-voxel sampling");


            auto f_bounded = [&,img_refw](long int E_row, long int E_col, long int channel, std::reference_wrapper<planar_image<float,double>> /*img_refw*/, float &voxel_val) {
                if( !isininc( user_data_s->inc_lower_threshold, voxel_val, user_data_s->inc_upper_threshold) ){
                    return; // No-op if outside of the thresholds.
                }
                if( channel != ud_channel){
                    return; // No-op if this is the wrong channel.
                }

                do{ // Do-while-false, so we can break out early.
                    const auto edit_val = voxel_val;

                    // Get the position of the voxel in the overlapping reference image.
                    const auto pos = img_refw.get().position(E_row, E_col);

                    // If no wholly overlapping image was identified, perform a lookup for this specific voxel.
                    img_ptr_t l_int_img_ptr = int_img_ptr;
                    if(l_int_img_ptr == nullptr){
                        try{
                            l_int_img_ptr = std::addressof( img_adj.position_to_image(pos).get() );
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
                    if(!img_adj.image_present( std::ref( *l_int_img_ptr ) )){
                        throw std::logic_error("One or more images were not included in the image adjacency determination. Refusing to continue.");
                    }
                    const auto R_num = img_adj.image_to_index( std::ref( *l_int_img_ptr ) );

//-------------
// TODO: determine the largest pxl_dz from all images and use it here instead.

                    // Determine the smallest dimension of the voxel, protecting against the pxl_dz = 0 case.
                    const auto pxl_dx = l_int_img_ptr->pxl_dx;
                    const auto pxl_dy = l_int_img_ptr->pxl_dy;
                    const auto pxl_dz = l_int_img_ptr->pxl_dz;
                    const auto pxl_dl = std::max( std::min({ pxl_dx, pxl_dy, pxl_dz }), 10.0 * machine_eps );

                    // The max distance separating adjacent next-next-nearest neighbouring (i.e., 3D diagonally-adjacent) voxels. 
                    const auto max_interp_dist = std::hypot( pxl_dx, pxl_dy, pxl_dz ); 
//-------------

                    // Ensure the voxel position in the edit image and reference image match reasonably.
                    const auto ring_0_pos = l_int_img_ptr->position(R_row, R_col);
                    if(ring_0_pos.distance(pos) > pxl_dl){ // If no suitable voxel for discrepancy testing, ignore voxel.
                        voxel_val = inaccessible_val;
                        break;
                    }

                    // Perform a discrepancy comparison.
                    const auto Disc = estimate_discrepancy(edit_val, ring_0_val);

                    // If computing the gamma index, check if we can avoid a costly DTA search.
                    if( (user_data_s->comparison_method == ComputeCompareImagesUserData::ComparisonMethod::GammaIndex) 
                    &&  (user_data_s->gamma_terminate_when_max_exceeded)
                    &&  (Disc > user_data_s->gamma_Dis_threshold) ){
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
                            double nearest_dist = std::numeric_limits<double>::infinity(); // Nearest of any voxel considered in this wavefront.

                            // Evaluate all voxels on this wavefront before proceeding.
                            for(long int k = -w; k < (w+1); ++k){
                                const auto l_num = R_num + k; // Adjacent image number.
                                if(!img_adj.index_present(l_num)) continue; // This adjacent image does not exist.
                                auto adj_img_ptr = std::addressof( img_adj.index_to_image(l_num).get() );

// TODO: try generate either the full range (-w...w) or merely endpoints (-w,w) based on whether |k|=w.                                
                                for(long int i = -w; i < (w+1); ++i){ 
                                    const auto l_row = R_row + i;
                                    if(!isininc(0, l_row, adj_img_ptr->rows-1)) continue; // Wavefront surface not valid.
// TODO: try generate either the full range (-w...w) or merely endpoints (-w,w) based on whether |k|=w.                                
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
                                        //const auto adj_vox_dist = adj_vox_pos.distance(ring_0_pos);
                                        const auto adj_vox_dist = adj_vox_pos.distance(pos);
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
                                        if( ( encountered_lower && is_higher )
                                        ||  ( encountered_higher && is_lower ) ){
                                            // If this voxel is one of two that straddle the target value, then consider
                                            // it as a match. However, since we don't know exactly where the transition
                                            // point is, we have to assume the worst-case distance so a more precise
                                            // estimate will not be obliterated. So we tack on the maximum distance to
                                            // the next-next-nearest (i.e., 3D diagonal) adjancet voxel.
                                            //
                                            // Note that if the voxel dimensions are small, then this will probably
                                            // suffice. Otherwise, proper interpolation should be preferred. This is how
                                            // we bias the result to (more safely, in the case of the gamma comparison)
                                            // overestimate distance and thus not ruin a more accurate interpolated value. 
                                            const auto worst_case_straddle_dist = adj_vox_dist + max_interp_dist;
                                            if(worst_case_straddle_dist < Dist){
                                                Dist = worst_case_straddle_dist;
                                            }
                                        }

                                        // Check if we can mark the voxel as the current best outright, without
                                        // having to interpolate.
                                        if( ( std::abs(adj_img_val - edit_val) < user_data_s->DTA_vox_val_eq_abs )
                                        ||  ( relative_diff(adj_img_val, edit_val) < user_data_s->DTA_vox_val_eq_reldiff ) ){
                                            if(adj_vox_dist < Dist){
                                                Dist = adj_vox_dist;
                                            }

                                        // Interpolate the neighbours.
                                        //
                                        // If neighbouring voxel values have been seen both above and below the
                                        // target value, then the grid can be interpolated (at some unknown location) to achieve
                                        // the target value. 
                                        //
                                        // However, the interpolation can only possibly be better than the current by a
                                        // certain amount.
                                        }else if(adj_vox_dist < (Dist + max_interp_dist)){

                                            // Sample the (6) 3D nearest neighbours and interpolate between them if necessary.
                                            // 
                                            // Note that this technique merely interpolates along the edges of the voxel-to-voxel grid.
                                            // It is robust and comparable in speed to no interpolation.
                                            if( (user_data_s->interpolation_method == ComputeCompareImagesUserData::InterpolationMethod::NN)
                                            ||  (user_data_s->interpolation_method == ComputeCompareImagesUserData::InterpolationMethod::NNN) ){

                                                // In pixel coordinates, these points are all a distance of sqrt(1)=1 from the centre voxel.
                                                std::array<std::array<long int, 3>, 6> nn_triplets = {{
                                                        { -1,  0,  0 },
                                                        {  1,  0,  0 },
                                                        {  0, -1,  0 },
                                                        {  0,  1,  0 },
                                                        {  0,  0, -1 },
                                                        {  0,  0,  1 }
                                                }};

                                                for(const auto &triplets : nn_triplets){
                                                    const auto nn_row = l_row + triplets[0];
                                                    const auto nn_col = l_col + triplets[1];
                                                    const auto nn_img = l_num + triplets[2];

                                                    if(img_adj.index_present(nn_img)
                                                    && isininc(0, nn_row, adj_img_ptr->rows - 1L)
                                                    && isininc(0, nn_col, adj_img_ptr->columns - 1L) ){
                                                    
                                                        auto nn_img_refw = img_adj.index_to_image(nn_img);
                                                        const auto nn_val = nn_img_refw.get().value(nn_row, nn_col, channel);

                                                        const bool nn_is_lower = (nn_val < edit_val);
                                                        const bool nn_is_higher = (edit_val < nn_val);

                                                        // Skip this neighbour if it does not complement the central
                                                        // voxel and therefore cannot be interpolated to the target
                                                        // value.
                                                        //if( !( (nn_is_lower || is_lower) && (nn_is_higher || is_higher) ) ) continue;
                                                        // equiv to:  (?)
                                                        if( !( (is_higher && nn_is_lower) || (is_lower && nn_is_higher) ) ) continue;

                                                        // Determine the 3D point at which the target value is reached.
                                                        const auto nn_pos = nn_img_refw.get().position(nn_row, nn_col);
                                                        const auto nn_v_unit = (adj_vox_pos - nn_pos).unit();
                                                        //if( ! nn_v_unit.isfinite() ) continue;
                                                        if( ! nn_v_unit.isfinite() ){
                                                            throw std::logic_error("Diagonal and centre overlap. Cannot continue.");
                                                        }

                                                        // Abandon the calculation if the point is on the wrong side of
                                                        // the centre voxel (and thus the interpolation will necessarily
                                                        // be further away than the centre voxel).
                                                        //if( (pos - nn_pos).Dot(nn_v_unit) <= 0.0 ) continue;
                                                        //if( (pos - nn_pos).Dot(nn_v_unit) >= 0.0 ) continue;

                                                        // Since either:
                                                        //    adj_img_val <= edit_val <= nn_val
                                                        // or
                                                        //    adj_img_val >= edit_val >= nn_val
                                                        // then
                                                        //   |adj_img_val - nn_val| >= |edit_val - nn_val|.
                                                        // so we can use this to scale the translation from nn to adj_img.
                                                        const auto dR = nn_pos.distance( adj_vox_pos );
                                                        const auto d_target = std::abs(edit_val - nn_val);
                                                        const auto d_val = std::abs(adj_img_val - nn_val);
                                                        const auto R_target = nn_pos + (nn_v_unit * dR * d_target / d_val);

                                                        const auto R_dist = R_target.distance(pos);
                                                        if(R_dist < Dist) Dist = R_dist;
                                                    } // If: triplet is valid.
                                                } // Loop over adjacent neighbours.
                                            } // If: using NN interpolation.
                                            
                                            // Sample the (12) 3D next-nearest neighbours and interpolate between them if necessary.
                                            // 
                                            // Note that this technique interpolates the planar diagonal along the edges of the voxel-to-voxel grid.
                                            // It requires solving a quadratic polynomial and is therefore more computationally demanding.
                                            // Numerical difficulties are also amplified, which results in lower accuracy than nearest-neighbour
                                            // interpolation.
                                            if(user_data_s->interpolation_method == ComputeCompareImagesUserData::InterpolationMethod::NNN){
                                                //In pixel coordinates, these points are all sqrt(2) distance from the centre voxel.
                                                // The following triplets come in packs of triplets: the first triplet is the diagonal position
                                                // and the second and third triplets are corners which are needed for interpolation.
                                                //
                                                // As you can see, the corners can be summed to give the diagonals; they could also be decomposed
                                                // this way, but it seemed easier to just write them all out.
                                                std::array<std::array<std::array<long int, 3>, 3>, 12> nnn_triplets = {{
                                                        {{ { -1,  0, -1 },   {  0,  0, -1 },  { -1,  0,  0 } }},
                                                        {{ {  0, -1, -1 },   {  0,  0, -1 },  {  0, -1,  0 } }},
                                                        {{ {  0,  1, -1 },   {  0,  0, -1 },  {  0,  1,  0 } }},
                                                        {{ {  1,  0, -1 },   {  0,  0, -1 },  {  1,  0,  0 } }},
                                                                        
                                                        {{ { -1, -1,  0 },   {  0, -1,  0 },  { -1,  0,  0 } }},
                                                        {{ { -1,  1,  0 },   {  0,  1,  0 },  { -1,  0,  0 } }},
                                                        {{ {  1, -1,  0 },   {  0, -1,  0 },  {  1,  0,  0 } }},
                                                        {{ {  1,  1,  0 },   {  0,  1,  0 },  {  1,  0,  0 } }},
                                                                        
                                                        {{ { -1,  0,  1 },   {  0,  0,  1 },  { -1,  0,  0 } }},
                                                        {{ {  0, -1,  1 },   {  0,  0,  1 },  {  0, -1,  0 } }},
                                                        {{ {  0,  1,  1 },   {  0,  0,  1 },  {  0,  1,  0 } }},
                                                        {{ {  1,  0,  1 },   {  0,  0,  1 },  {  1,  0,  0 } }}
                                                }};

                                                for(const auto &t_triplets : nnn_triplets){
                                                    const auto diag_row = l_row + t_triplets[0][0];  // Diagonal.
                                                    const auto diag_col = l_col + t_triplets[0][1];
                                                    const auto diag_img = l_num + t_triplets[0][2];

                                                    const auto cA_row = l_row + t_triplets[1][0];  // Corner A.
                                                    const auto cA_col = l_col + t_triplets[1][1];
                                                    const auto cA_img = l_num + t_triplets[1][2];

                                                    const auto cB_row = l_row + t_triplets[2][0];  // Corner B.
                                                    const auto cB_col = l_col + t_triplets[2][1];
                                                    const auto cB_img = l_num + t_triplets[2][2];  

                                                    if(img_adj.index_present(diag_img)
                                                    && img_adj.index_present(cA_img)
                                                    && img_adj.index_present(cB_img)
                                                    && isininc(0, diag_row, adj_img_ptr->rows - 1L)
                                                    && isininc(0, diag_col, adj_img_ptr->columns - 1L)
                                                    && isininc(0, cA_row,   adj_img_ptr->rows - 1L)
                                                    && isininc(0, cA_col,   adj_img_ptr->columns - 1L)
                                                    && isininc(0, cB_row,   adj_img_ptr->rows - 1L)
                                                    && isininc(0, cB_col,   adj_img_ptr->columns - 1L) ){
                                                    
                                                        auto diag_img_refw = img_adj.index_to_image(diag_img);
                                                        const auto diag_val = diag_img_refw.get().value(diag_row, diag_col, channel);

                                                        const bool diag_is_lower = (diag_val < edit_val);
                                                        const bool diag_is_higher = (edit_val < diag_val);

                                                        // Skip this neighbour if it does not complement the central
                                                        // voxel and therefore cannot be interpolated to the target
                                                        // value.
                                                        if( !( (is_higher && diag_is_lower) || (is_lower && diag_is_higher) ) ) continue;

                                                        auto cA_img_refw  = img_adj.index_to_image(cA_img);
                                                        auto cB_img_refw  = img_adj.index_to_image(cB_img);
                                                        const auto cA_val = cA_img_refw.get().value(cA_row, cA_col, channel);
                                                        const auto cB_val = cB_img_refw.get().value(cB_row, cB_col, channel);

                                                        // Determine the 3D point at which the target value is reached.
                                                        const auto a = adj_img_val - edit_val;
                                                        const auto b = (cA_val - adj_img_val) + (cB_val - adj_img_val);
                                                        const auto d = diag_val + adj_img_val - cA_val - cB_val;

                                                        const auto x_a = (-b + std::sqrt( b*b - 4.0*d*a ) ) / (2.0 * d);
                                                        const auto x_b = (-b - std::sqrt( b*b - 4.0*d*a ) ) / (2.0 * d);
                                                        if(!std::isfinite(x_a) && !std::isfinite(x_b)) continue;

                                                        auto x = (isininc(0.0, x_a, 1.0)) ? x_a : x_b;

                                                        if( !(isininc(0.0, x_a, 1.0)) && !(isininc(0.0, x_b, 1.0)) ){
                                                            // This is probably a numerical error. Accept values slightly
                                                            // beyond the limits.
                                                            const auto x_a_c = std::clamp(x_a, 0.0, 1.0);
                                                            const auto x_b_c = std::clamp(x_b, 0.0, 1.0);
                                                            x = (std::abs(x_a - x_a_c) < std::abs(x_b - x_b_c)) ? x_a_c : x_b_c;
                                                        }

                                                        if( (isininc(0.0, x_a, 1.0)) && (isininc(0.0, x_b, 1.0)) ){
                                                            // This is probably a numerical error. Accept the value closest
                                                            // to the middle of the range since the phony root is likely to
                                                            // hover around the range extrema.
                                                            x = (std::abs(x_a - 0.5) < std::abs(x_b - 0.5)) ? x_a : x_b;
                                                        }

                                                        const auto diag_pos = diag_img_refw.get().position(diag_row, diag_col);
                                                        const auto diag_v = (diag_pos - adj_vox_pos);
                                                        if( ! diag_v.isfinite() ){
                                                            throw std::logic_error("Diagonal and centre overlap. Cannot continue.");
                                                        }

                                                        // Abandon the calculation if the point is on the wrong side of
                                                        // the centre voxel (and thus the interpolation will necessarily
                                                        // be further away than the centre voxel).
                                                        //if( (pos - diag_pos).Dot(diag_v_unit) <= 0.0 ) continue;
                                                        //if( (pos - diag_pos).Dot(diag_v_unit) >= 0.0 ) continue;

                                                        const auto R_target = adj_vox_pos + (diag_v * x);
                                                        const auto R_dist = R_target.distance(pos);
                                                        if(R_dist < Dist){
                                                            Dist = R_dist;
                                                        }

                                                    } // If: triplet is valid.
                                                } // Loop over adjacent neighbours.
                                            } // If: using NNN interpolation.


                                        } // If-else: avoid interpolating neighbours.
                                    } // Loop: j.
                                } // Loop: i.
                            } // Loop: k.

                            if((Dist + max_interp_dist) < nearest_dist){
                                // It is now impossible to improve the DTA because the next wavefront will all necessarily
                                // be further away. So terminate the search.
                                break; // note: voxel_val set below.
                            }
                            
                            if(!std::isfinite(nearest_dist)){
                                // No voxels found to assess within this epoch. Further epochs will be futile, so
                                // discontinue the search, taking whatever value (finite or infinite) was found to be best.
                                break; // note: voxel_val set below.
                            }
                            
                            if(nearest_dist > (user_data_s->DTA_max + max_interp_dist)){
                                // Terminate the search if the user has instructed so.
                                // Take the current best value if there is any.
                                break; // note: voxel_val set below.
                            }

                            // If computing the gamma index, check if we can avoid continuing the DTA search since gamma
                            // will necessarily be >1 at this point.
                            if( (user_data_s->gamma_terminate_when_max_exceeded)
                            &&  (nearest_dist > (user_data_s->gamma_DTA_threshold + max_interp_dist)) ){
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
                        std::lock_guard<std::mutex> lock(passing_counter);
                        user_data_s->count += 1;

                        if(std::isfinite(Dist) && std::isfinite(Disc)){
                            voxel_val = std::sqrt( std::pow(Dist / user_data_s->gamma_DTA_threshold, 2.0)
                                                 + std::pow(Disc / user_data_s->gamma_Dis_threshold, 2.0) );

                            if(voxel_val < 1.0) user_data_s->passed += 1;
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

