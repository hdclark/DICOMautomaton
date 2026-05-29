//Volumetric_Spatial_Blur.cc.

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

#include "YgorImages.h"
#include "YgorMath.h"
#include "YgorMisc.h"
#include "YgorLog.h"
#include "YgorStats.h"       //Needed for Stats:: namespace.

#include "YgorClustering.hpp"
#include "../../Thread_Pool.h"
#include "../Grouping/Misc_Functors.h"
#include "../ConvenienceRoutines.h"
#include "Volumetric_Neighbourhood_Sampler.h"

#include "Volumetric_Spatial_Blur.h"


bool ComputeVolumetricSpatialBlur(planar_image_collection<float,double> &imagecoll,
                      std::list<std::reference_wrapper<planar_image_collection<float,double>>> /*external_imgs*/,
                      std::list<std::reference_wrapper<contour_collection<double>>> ccsl,
                      std::any user_data ){

    // This routine computes 3D blurs. Currently, only Gaussians are supported. Specifically, a 1-sigma Gaussian (in
    // pixel units, not DICOM units) with a fixed 3*sigma extent. This blur is separable and is thus applied in three
    // directions successively. The spacing between adjacent voxels is not taken into account, so voxels should have
    // isotropic dimensions (or the blur will be non-isotropic). The effective window considered by this Gaussian is
    // 7x7x7 voxels. If voxels are inaccessible or non-finite they will be ignored and other voxels in the neighbourhood
    // will be more heavily weighted.
    //
    // Note: The provided image collection must be rectilinear. This requirement comes foremost from a limitation of the
    // implementation. 
    //

    //We require a valid ComputeVolumetricSpatialBlurUserData struct packed into the user_data.
    ComputeVolumetricSpatialBlurUserData *user_data_s;
    try{
        user_data_s = std::any_cast<ComputeVolumetricSpatialBlurUserData *>(user_data);
    }catch(const std::exception &e){
        YLOGWARN("Unable to cast user_data to appropriate format. Cannot continue with computation");
        return false;
    }

    if( ccsl.empty() ){
        YLOGWARN("Missing needed contour information. Cannot continue with computation");
        return false;
    }

    if(user_data_s->estimator == VolumetricSpatialBlurEstimator::Gaussian){
        auto f_reduce = [](float, std::vector<float> &shtl, vec3<double>) -> float {
                          double f = 0.0;
                          double w = 0.0;

                          // Note: The following weights come from the 1D Gaussian with sigma=1 integrated over the
                          //       length of each voxel. These weights are normalized to 1, so the w summation is only
                          //       necessary in case some voxels are inaccessible.
                          if(std::isfinite(shtl[0])){
                              w += 0.006;
                              f += 0.006 * shtl[0];
                          }
                          if(std::isfinite(shtl[1])){
                              w += 0.061;
                              f += 0.061 * shtl[1];
                          }
                          if(std::isfinite(shtl[2])){
                              w += 0.242;
                              f += 0.242 * shtl[2];
                          }
                          if(std::isfinite(shtl[3])){
                              w += 0.382;
                              f += 0.382 * shtl[3];
                          }
                          if(std::isfinite(shtl[4])){
                              w += 0.242;
                              f += 0.242 * shtl[4];
                          }
                          if(std::isfinite(shtl[5])){
                              w += 0.061;
                              f += 0.061 * shtl[5];
                          }
                          if(std::isfinite(shtl[6])){
                              w += 0.006;
                              f += 0.006 * shtl[6];
                          }
                          
                          if(w < 1E-3){
                              f = std::numeric_limits<double>::quiet_NaN();
                          }
                          return f / w;
                      };

        // row-direction.
        {
            YLOGINFO("Convolving row-aligned direction now..");
            ComputeVolumetricNeighbourhoodSamplerUserData ud;
            ud.channel = user_data_s->channel;
            ud.neighbourhood = ComputeVolumetricNeighbourhoodSamplerUserData::Neighbourhood::Selection;
            ud.f_reduce = f_reduce;

            ud.voxel_triplets = {{ std::array<int64_t, 3>{ -3,  0,  0 },    // 0
                                   std::array<int64_t, 3>{ -2,  0,  0 },    // 1
                                   std::array<int64_t, 3>{ -1,  0,  0 },    // 2
                                   std::array<int64_t, 3>{  0,  0,  0 },    // 3
                                   std::array<int64_t, 3>{  1,  0,  0 },    // 4
                                   std::array<int64_t, 3>{  2,  0,  0 },    // 5
                                   std::array<int64_t, 3>{  3,  0,  0 } }}; // 6


            // Invoke the volumetric sampling routine to compute the above functors.
            if(!imagecoll.Compute_Images( ComputeVolumetricNeighbourhoodSampler, 
                                          {}, ccsl, &ud )){
                throw std::runtime_error("Unable to compute row-aligned volumetric spatial blur.");
            }
        }

        // column-direction.
        {
            YLOGINFO("Convolving column-aligned direction now..");
            ComputeVolumetricNeighbourhoodSamplerUserData ud;
            ud.channel = user_data_s->channel;
            ud.neighbourhood = ComputeVolumetricNeighbourhoodSamplerUserData::Neighbourhood::Selection;
            ud.f_reduce = f_reduce;

            ud.voxel_triplets = {{ std::array<int64_t, 3>{  0, -3,  0 },    // 0
                                   std::array<int64_t, 3>{  0, -2,  0 },    // 1
                                   std::array<int64_t, 3>{  0, -1,  0 },    // 2
                                   std::array<int64_t, 3>{  0,  0,  0 },    // 3
                                   std::array<int64_t, 3>{  0,  1,  0 },    // 4
                                   std::array<int64_t, 3>{  0,  2,  0 },    // 5
                                   std::array<int64_t, 3>{  0,  3,  0 } }}; // 6

            // Invoke the volumetric sampling routine to compute the above functors.
            if(!imagecoll.Compute_Images( ComputeVolumetricNeighbourhoodSampler, 
                                          {}, ccsl, &ud )){
                throw std::runtime_error("Unable to compute column-aligned volumetric blur.");
            }
        }

        // ortho-direction.
        {
            YLOGINFO("Convolving ortho-aligned direction now..");
            ComputeVolumetricNeighbourhoodSamplerUserData ud;
            ud.channel = user_data_s->channel;
            ud.neighbourhood = ComputeVolumetricNeighbourhoodSamplerUserData::Neighbourhood::Selection;
            ud.f_reduce = f_reduce;

            ud.voxel_triplets = {{ std::array<int64_t, 3>{  0,  0, -3 },    // 0
                                   std::array<int64_t, 3>{  0,  0, -2 },    // 1
                                   std::array<int64_t, 3>{  0,  0, -1 },    // 2
                                   std::array<int64_t, 3>{  0,  0,  0 },    // 3
                                   std::array<int64_t, 3>{  0,  0,  1 },    // 4
                                   std::array<int64_t, 3>{  0,  0,  2 },    // 5
                                   std::array<int64_t, 3>{  0,  0,  3 } }}; // 6

            // Invoke the volumetric sampling routine to compute the above functors.
            if(!imagecoll.Compute_Images( ComputeVolumetricNeighbourhoodSampler, 
                                          {}, ccsl, &ud )){
                throw std::runtime_error("Unable to compute ortho-aligned volumetric blur.");
            }
        }

    }else{
        throw std::invalid_argument("Unrecognized user-provided estimator argument.");
    }


    //Update the image metadata. 
    std::string img_desc;
    if(user_data_s->estimator == VolumetricSpatialBlurEstimator::Gaussian){
        img_desc += "volumetric Gaussian blurred";

    }else{
        throw std::invalid_argument("Unrecognized user-provided estimator");
    }

    img_desc += " (in pixel coord.s)";

    for(auto &img : imagecoll.images){
        UpdateImageDescription( std::ref(img), img_desc );
        UpdateImageWindowCentreWidth( std::ref(img) );
    }

    return true;
}

