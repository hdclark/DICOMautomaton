//Volumetric_Correlation_Detector.cc.

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
                      std::any user_data ){

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
        user_data_s = std::any_cast<ComputeVolumetricCorrelationDetectorUserData *>(user_data);
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

    long int random_seed = 123456;
    std::mt19937 re( random_seed );
    std::uniform_real_distribution<> rd(0.0, 1.0);

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
        }
        return;
    };

    const auto samples_needed_for_areal_density = [&](double radius, double sa) -> long int {
        // Estimates the number of samples needed to maintain the given level of surface area density (approximately).
        return static_cast<long int>( std::round( 4.0*M_PI*std::pow(radius,2.0) / sa ) );
    };

    // Large sampling radii.
    //sample_at_radius( 15.0,  samples_needed_for_areal_density(  15.0, 1.0 ) );
    sample_at_radius( 5.0,  samples_needed_for_areal_density(  5.0, 1.0 ) );

    ud.f_reduce = [&](float v, std::vector<float> &shtl, vec3<double> pos) -> float {
        std::vector<float> shtl2;
        shtl2.reserve(shtl.size());

        for(const auto &nv : shtl){
            if(std::isfinite(nv)){
                shtl2.emplace_back(std::abs(nv - v));
            }
        }

/*
if((pos - vec3<double>(-15.9, 31.9, 1.15)).length() < 0.45){
    std::ofstream f("/tmp/percentile_filtering_good_01.data");
    for(size_t i = 0; i < 100; ++i) f << i << " " << Stats::Percentile(shtl2, 1.0 * i / 100.0) << std::endl;
    f.close();
}

if((pos - vec3<double>(-7.91, 40.71, 7.45)).length() < 0.45){
    std::ofstream f("/tmp/percentile_filtering_bad_01.data");
    for(size_t i = 0; i < 100; ++i) f << i << " " << Stats::Percentile(shtl2, 1.0 * i / 100.0) << std::endl;
    f.close();
}

if((pos - vec3<double>(-49.52, -17.69, 7.45)).length() < 0.45){
    std::ofstream f("/tmp/percentile_filtering_bad_02.data");
    for(size_t i = 0; i < 100; ++i) f << i << " " << Stats::Percentile(shtl2, 1.0 * i / 100.0) << std::endl;
    f.close();
}

if((pos - vec3<double>(-63.91, 74.31, 7.45)).length() < 0.45){
    std::ofstream f("/tmp/percentile_filtering_bad_03.data");
    for(size_t i = 0; i < 100; ++i) f << i << " " << Stats::Percentile(shtl2, 1.0 * i / 100.0) << std::endl;
    f.close();
}

if((pos - vec3<double>(0.08, 88.71, 57.85)).length() < 0.45){
    std::ofstream f("/tmp/percentile_filtering_bad_04.data");
    for(size_t i = 0; i < 100; ++i) f << i << " " << Stats::Percentile(shtl2, 1.0 * i / 100.0) << std::endl;
    f.close();
}
*/

        // Works pretty good for 5mm. Works OK for 15mm, but not great.
        //const auto low = Stats::Percentile(shtl2, 0.01);
        //const auto high = Stats::Percentile(shtl2, 0.25);

        // Tuned for 5mm, but works worse than previous.
        //const auto low = Stats::Percentile(shtl2, 0.15);
        //const auto high = Stats::Percentile(shtl2, 0.30);

        const auto low = Stats::Percentile(shtl2, user_data_s->low);
        const auto high = Stats::Percentile(shtl2, user_data_s->high);

        return (high - low);
    }; 

    FUNCINFO("Proceeding with each voxel sampling " << ud.voxel_triplets.size() << " neighbouring voxels");

    // Invoke the volumetric sampling routine to compute the above functors.
    if(!imagecoll.Compute_Images( ComputeVolumetricNeighbourhoodSampler, 
                                  {}, ccsl, &ud )){
        throw std::runtime_error("Unable to compute volumetric correlation.");
    }

    //Update the image metadata. 
    std::string img_desc;
    img_desc += "Self-correlation (" + std::to_string(user_data_s->low) 
                                     + " to " 
                                     + std::to_string(user_data_s->high) + ")";

    for(auto &img : imagecoll.images){
        UpdateImageDescription( std::ref(img), img_desc );
        UpdateImageWindowCentreWidth( std::ref(img) );
    }

    return true;
}

