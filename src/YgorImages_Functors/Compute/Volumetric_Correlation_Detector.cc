//Volumetric_Correlation_Detector.cc.

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

#include "YgorImages.h"
#include "YgorMath.h"
#include "YgorMisc.h"
#include "YgorStats.h"       //Needed for Stats:: namespace.

#include "YgorClustering.hpp"
#include "../../Thread_Pool.h"
#include "../Grouping/Misc_Functors.h"
#include "../ConvenienceRoutines.h"
#include "Volumetric_Neighbourhood_Sampler.h"

#include "Volumetric_Correlation_Detector.h"


bool ComputeVolumetricCorrelationDetector(planar_image_collection<float,double> &imagecoll,
                      std::list<std::reference_wrapper<planar_image_collection<float,double>>> /*external_imgs*/,
                      std::list<std::reference_wrapper<contour_collection<double>>> ccsl,
                      std::experimental::any user_data ){

    // This routine samples the 3D neighbourhood around each voxel and evaluates similarity and disimilarity according
    // to some criteria. It works best for detecting periodic signals.
    //
    // Note: The provided image collection must be rectilinear. This requirement comes foremost from a limitation of
    //       the implementation. However, since derivatives are based on pixel coordinates, it is not clear how the
    //       derivative could be computed with non-rectilinear adjacency.
    //

    //We require a valid ComputeVolumetricCorrelationDetectorUserData struct packed into the user_data.
    ComputeVolumetricCorrelationDetectorUserData *user_data_s;
    try{
        user_data_s = std::experimental::any_cast<ComputeVolumetricCorrelationDetectorUserData *>(user_data);
    }catch(const std::exception &e){
        FUNCWARN("Unable to cast user_data to appropriate format. Cannot continue with computation");
        return false;
    }

    if( ccsl.empty() ){
        FUNCWARN("Missing needed contour information. Cannot continue with computation");
        return false;
    }

    if(imagecoll.images.empty()){
        FUNCWARN("No images available for computation. Nothing to do.");
        return false;
    }

    // Estimate the typical image pxl_dx, pxl_dy, and pxl_dz in case it is needed for thinning later.
    //
    // Note: this routine assumes the first image is representative of all images.
    const auto pxl_dx = imagecoll.images.front().pxl_dx;
    const auto pxl_dy = imagecoll.images.front().pxl_dy;
    const auto pxl_dz = imagecoll.images.front().pxl_dz;

    ComputeVolumetricNeighbourhoodSamplerUserData ud;
    ud.channel = user_data_s->channel;
    ud.neighbourhood = ComputeVolumetricNeighbourhoodSamplerUserData::Neighbourhood::Selection;


    // Pack the sampler with a variety of voxels to sample. A sample will be accompanied by instructions whether to
    // penalize or reward the difference in intensity (aka the 'score multiplier').
    ud.voxel_triplets.clear();
    std::vector<double> score_multipliers;

    long int random_seed = 123456;
    std::mt19937 re( random_seed );
    std::uniform_real_distribution<> rd(0.0, 1.0);

    const auto score_multiplier = [&](double radius) -> double { // In DICOM coordinates (mm).
        // Determine if the point should be penalized or rewarded when compared to the central voxel.
        const double dr = 1.5;   // Radius of cylinder.
        const double dl = 15.0;  // Separation of adjacent grid lines.

        const double should_be_similar = 1.0;
        const double should_be_dissimilar = -1.0;
        double score_multiplier = should_be_dissimilar;
/*
        // Centered on a 3D union.
        if(false){
        }else if(isininc(0.0, radius, dr)){
            score_multiplier = should_be_similar;

        }else if(isininc(0.0*dl+dr, radius, 1.0*dl-dr)){
            score_multiplier = should_be_dissimilar;
        }else if(isininc(1.0*dl-dr, radius, 1.0*dl+dr)){
            score_multiplier = should_be_similar;

        }else if(isininc(1.0*dl+dr, radius, 2.0*dl-dr)){
            score_multiplier = should_be_dissimilar;
        }else if(isininc(2.0*dl-dr, radius, 2.0*dl+dr)){
            score_multiplier = should_be_similar;

        }else if(isininc(2.0*dl+dr, radius, 3.0*dl-dr)){
            score_multiplier = should_be_dissimilar;
        }else if(isininc(3.0*dl-dr, radius, 3.0*dl+dr)){
            score_multiplier = should_be_similar;

        }else if(isininc(3.0*dl+dr, radius, 4.0*dl-dr)){
            score_multiplier = should_be_dissimilar;
        }else if(isininc(4.0*dl-dr, radius, 4.0*dl+dr)){
            score_multiplier = should_be_similar;

        }else if(isininc(4.0*dl+dr, radius, 5.0*dl-dr)){
            score_multiplier = should_be_dissimilar;
        }else if(isininc(5.0*dl-dr, radius, 5.0*dl+dr)){
            score_multiplier = should_be_similar;

        }else{
            // Can keep going indefinitely, but better to simply use the remainder to project it.
            score_multiplier = should_be_dissimilar;
        }
*/        

        // Centered on the middle of a square.
        if(false){
        }else if(isininc(0.0, radius, dr)){
            score_multiplier = should_be_similar;

        }else if(isininc(0.0*dl+dr, radius, 1.0*dl-dr)){
            score_multiplier = should_be_dissimilar;
        }else if(isininc(1.0*dl-dr, radius, 1.0*dl+dr)){
            score_multiplier = should_be_similar;

        }else if(isininc(1.0*dl+dr, radius, 2.0*dl-dr)){
            score_multiplier = should_be_dissimilar;
        }else if(isininc(2.0*dl-dr, radius, 2.0*dl+dr)){
            score_multiplier = should_be_similar;

        }else if(isininc(2.0*dl+dr, radius, 3.0*dl-dr)){
            score_multiplier = should_be_dissimilar;
        }else if(isininc(3.0*dl-dr, radius, 3.0*dl+dr)){
            score_multiplier = should_be_similar;

        }else if(isininc(3.0*dl+dr, radius, 4.0*dl-dr)){
            score_multiplier = should_be_dissimilar;
        }else if(isininc(4.0*dl-dr, radius, 4.0*dl+dr)){
            score_multiplier = should_be_similar;

        }else if(isininc(4.0*dl+dr, radius, 5.0*dl-dr)){
            score_multiplier = should_be_dissimilar;
        }else if(isininc(5.0*dl-dr, radius, 5.0*dl+dr)){
            score_multiplier = should_be_similar;

        }else{
            // Can keep going indefinitely, but better to simply use the remainder to project it.
            score_multiplier = should_be_dissimilar;
        }
        return score_multiplier;
    };

    const auto sample_at_radius = [&](double radius, // In DICOM coordinates (mm).
                                      long int count ) -> void {

        // This routine samples the surface of a sphere such that samples will (approximately) be spread out evenly.
        //
        const vec3<double> N_z(0.0, 0.0, 1.0);
        for(long int i = 0; i < count; ++i){
            const auto theta = std::acos(2.0 * rd(re) - 1.0);
            const auto phi = 2.0 * M_PI * rd(re);

            const auto rot_A = N_z.rotate_around_y(phi);
            const auto rot_B = rot_A.rotate_around_z(theta);

            const auto R = rot_B * radius; // Vector to the sampled point.

            // Convert the sampled point to pixel offset coordinates.
            const auto dx_i = static_cast<long int>( std::round(R.x / pxl_dx) );
            const auto dy_i = static_cast<long int>( std::round(R.y / pxl_dy) );
            const auto dz_i = static_cast<long int>( std::round(R.z / pxl_dz) );

            // Insert the sampling triplet.
            ud.voxel_triplets.emplace_back( std::array<long int, 3>{dx_i, dy_i, dz_i} );
            score_multipliers.emplace_back( score_multiplier(radius) );
        }
        return;
    };

    const auto samples_needed_for_areal_density = [&](double radius, double sa) -> long int {
        // Estimates the number of samples needed to maintain the given level of surface area density (approximately).
        return static_cast<long int>( std::round( 4.0*M_PI*std::pow(radius,2.0) / sa ) );
    };

    // Large sampling radii.
    //sample_at_radius( 5.0,  samples_needed_for_areal_density(  5.0, 15.0 ) );
    //sample_at_radius( 10.0, samples_needed_for_areal_density( 10.0, 40.0 ) );
    //sample_at_radius( 15.0, samples_needed_for_areal_density( 15.0, 20.0 ) );
    //sample_at_radius( 20.0, samples_needed_for_areal_density( 20.0, 60.0 ) );


/*    
    // Focus on a smaller region surrounding a grid cylinder or 3D union point.
    sample_at_radius( 1.0,  samples_needed_for_areal_density(  1.0, 2.0 ) );
    sample_at_radius( 1.5,  samples_needed_for_areal_density(  1.5, 2.0 ) );
    sample_at_radius( 2.0,  samples_needed_for_areal_density(  2.0, 3.0 ) );
    sample_at_radius( 3.0,  samples_needed_for_areal_density(  3.0, 4.0 ) );
    sample_at_radius( 4.0,  samples_needed_for_areal_density(  4.0, 5.0 ) );
    sample_at_radius( 5.0,  samples_needed_for_areal_density(  5.0, 6.0 ) );
    sample_at_radius( 6.0,  samples_needed_for_areal_density(  6.0, 7.0 ) );
    sample_at_radius( 7.0,  samples_needed_for_areal_density(  7.0, 8.0 ) );
    sample_at_radius( 8.0,  samples_needed_for_areal_density(  8.0, 9.0 ) );
    sample_at_radius( 9.0,  samples_needed_for_areal_density(  9.0,15.0 ) );
    sample_at_radius(10.0,  samples_needed_for_areal_density( 10.0,15.0 ) );
    sample_at_radius(11.0,  samples_needed_for_areal_density( 11.0,20.0 ) );
    sample_at_radius(12.0,  samples_needed_for_areal_density( 12.0,20.0 ) );
    sample_at_radius(13.0,  samples_needed_for_areal_density( 13.0,20.0 ) );

    sample_at_radius(14.0,  samples_needed_for_areal_density( 14.0,20.0 ) );
    sample_at_radius(15.0,  samples_needed_for_areal_density( 15.0,20.0 ) );
    sample_at_radius(16.0,  samples_needed_for_areal_density( 16.0,20.0 ) );
    sample_at_radius(17.0,  samples_needed_for_areal_density( 17.0,25.0 ) );
    sample_at_radius(18.0,  samples_needed_for_areal_density( 18.0,25.0 ) );

    sample_at_radius(28.0,  samples_needed_for_areal_density( 28.0,25.0 ) );
    sample_at_radius(30.0,  samples_needed_for_areal_density( 30.0,25.0 ) );
    sample_at_radius(32.0,  samples_needed_for_areal_density( 32.0,25.0 ) );
*/


    ud.f_reduce = [&](float v, std::vector<float> &shtl, vec3<double> pos) -> float {
        double score = 0.0;
        auto sm_it = std::begin(score_multipliers);
        double valid = 0.0;
        for(const auto &nv : shtl){
            if(std::isfinite(nv)){
                score += (*sm_it) * std::abs(nv - v);
                valid += 1.0;
            }
            ++sm_it;
        }
        const auto new_val = score / valid;
        return new_val;
    }; 

    // Verify consistency.
    if(ud.voxel_triplets.size() != score_multipliers.size()){
        throw std::logic_error("Voxel sampling triplets are uncoupled from score multipliers. Refusing to continue.");
    }
    FUNCINFO("Proceeding with each voxel sampling " << ud.voxel_triplets.size() << " neighbouring voxels");

    // Invoke the volumetric sampling routine to compute the above functors.
    if(!imagecoll.Compute_Images( ComputeVolumetricNeighbourhoodSampler, 
                                  {}, ccsl, &ud )){
        throw std::runtime_error("Unable to compute volumetric correlation.");
    }

    //Update the image metadata. 
    std::string img_desc;
    img_desc += "Self-correlation";

    for(auto &img : imagecoll.images){
        UpdateImageDescription( std::ref(img), img_desc );
        UpdateImageWindowCentreWidth( std::ref(img) );
    }

    return true;
}

