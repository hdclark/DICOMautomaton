//Volumetric_Spatial_Derivative.cc.

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

#include "Volumetric_Spatial_Derivative.h"


bool ComputeVolumetricSpatialDerivative(planar_image_collection<float,double> &imagecoll,
                      std::list<std::reference_wrapper<planar_image_collection<float,double>>> /*external_imgs*/,
                      std::list<std::reference_wrapper<contour_collection<double>>> ccsl,
                      std::experimental::any user_data ){

    // This routine computes 3D (spatial) partial derivatives (or the gradient). This routine computes first-order
    // partial derivatives (using centered finite difference estimators) along the row-, column-, and image-aligned
    // axes. All use pixel coordinates (i.e., ignoring pixel shape/extent and real-space coordinates, which can be found
    // by an appropriate multiplicative factor if desired). These derivatives are not directly suitable for physical
    // calculations due to the use of pixel coordinates, but are suitable for boundary visualization and edge detection.
    //
    // Note: The provided image collection must be rectilinear. This requirement comes foremost from a limitation of
    //       the implementation. However, since derivatives are based on pixel coordinates, it is not clear how the
    //       derivative could be computed with non-rectilinear adjacency.
    //

    //We require a valid ComputeVolumetricSpatialDerivativeUserData struct packed into the user_data.
    ComputeVolumetricSpatialDerivativeUserData *user_data_s;
    try{
        user_data_s = std::experimental::any_cast<ComputeVolumetricSpatialDerivativeUserData *>(user_data);
    }catch(const std::exception &e){
        FUNCWARN("Unable to cast user_data to appropriate format. Cannot continue with computation");
        return false;
    }

    if( ccsl.empty() ){
        FUNCWARN("Missing needed contour information. Cannot continue with computation");
        return false;
    }

    // If non-maximum suppression has been requested, pre-compute the magnitude via recursion.
    planar_image_collection<float,double> nms_working;  // Additional storage for edge thinning.
    if(user_data_s->method == VolumetricSpatialDerivativeMethod::non_maximum_suppression){
        nms_working = imagecoll; // Deep copy.

        ComputeVolumetricSpatialDerivativeUserData nms_ud;
        nms_ud = *user_data_s;
        nms_ud.method = VolumetricSpatialDerivativeMethod::magnitude;
        const auto ret = ComputeVolumetricSpatialDerivative(nms_working, {}, ccsl, &nms_ud);
        if(!ret) return false;
    }

    ComputeVolumetricNeighbourhoodSamplerUserData ud;
    ud.channel = user_data_s->channel;
    ud.neighbourhood = ComputeVolumetricNeighbourhoodSamplerUserData::Neighbourhood::Selection;


    if(false){
    }else if(user_data_s->order == VolumetricSpatialDerivativeEstimator::first){
        ud.voxel_triplets = {{ {  0,  0,  0 },    // 0
                               { -1,  0,  0 },    // 1
                               {  1,  0,  0 },    // 2
                               {  0, -1,  0 },    // 3
                               {  0,  1,  0 },    // 4
                               {  0,  0, -1 },    // 5
                               {  0,  0,  1 } }}; // 6
        if(false){
        }else if(user_data_s->method == VolumetricSpatialDerivativeMethod::row_aligned){
            ud.f_reduce = [](float, std::vector<float> &shtl, vec3<double>) -> float {
                              const auto col_m = std::isfinite(shtl[3]) ? shtl[3] : shtl[0];
                              const auto col_p = std::isfinite(shtl[4]) ? shtl[4] : shtl[0];
                              return (col_p - col_m) * 0.5f;
                          };

        }else if(user_data_s->method == VolumetricSpatialDerivativeMethod::column_aligned){
            ud.f_reduce = [](float, std::vector<float> &shtl, vec3<double>) -> float {
                              const auto row_m = std::isfinite(shtl[1]) ? shtl[1] : shtl[0];
                              const auto row_p = std::isfinite(shtl[2]) ? shtl[2] : shtl[0];
                              return (row_p - row_m) * 0.5f;
                          };

        }else if(user_data_s->method == VolumetricSpatialDerivativeMethod::image_aligned){
            ud.f_reduce = [](float, std::vector<float> &shtl, vec3<double>) -> float {
                              const auto img_m = std::isfinite(shtl[5]) ? shtl[5] : shtl[0];
                              const auto img_p = std::isfinite(shtl[6]) ? shtl[6] : shtl[0];
                              return (img_p - img_m) * 0.5f;
                          };

        }else if(user_data_s->method == VolumetricSpatialDerivativeMethod::magnitude){
            ud.f_reduce = [](float, std::vector<float> &shtl, vec3<double>) -> float {
                              const auto row_m = std::isfinite(shtl[1]) ? shtl[1] : shtl[0];
                              const auto row_p = std::isfinite(shtl[2]) ? shtl[2] : shtl[0];
                              const auto col_m = std::isfinite(shtl[3]) ? shtl[3] : shtl[0];
                              const auto col_p = std::isfinite(shtl[4]) ? shtl[4] : shtl[0];
                              const auto img_m = std::isfinite(shtl[5]) ? shtl[5] : shtl[0];
                              const auto img_p = std::isfinite(shtl[6]) ? shtl[6] : shtl[0];
                              return std::hypot( (col_p - col_m) * 0.5f,
                                                 (row_p - row_m) * 0.5f,
                                                 (img_p - img_m) * 0.5f );
                          };

        }else if(user_data_s->method == VolumetricSpatialDerivativeMethod::non_maximum_suppression){
            // Note: This method is extremely slow, requiring a fresh search for each trilinear interpolation.
            // It can easily be made faster, but might require modifying the volumetric neighbourhood sampler. TODO.
            ud.f_reduce = [&](float v, std::vector<float> &shtl, vec3<double> pos) -> float {
                    const auto row_m = std::isfinite(shtl[1]) ? shtl[1] : shtl[0];
                    const auto row_p = std::isfinite(shtl[2]) ? shtl[2] : shtl[0];
                    const auto col_m = std::isfinite(shtl[3]) ? shtl[3] : shtl[0];
                    const auto col_p = std::isfinite(shtl[4]) ? shtl[4] : shtl[0];
                    const auto img_m = std::isfinite(shtl[5]) ? shtl[5] : shtl[0];
                    const auto img_p = std::isfinite(shtl[6]) ? shtl[6] : shtl[0];

                    const auto ra = (col_p - col_m) * 0.5f;
                    const auto ca = (row_p - row_m) * 0.5f;
                    const auto ia = (img_p - img_m) * 0.5f;

                    const auto magn = std::hypot(ra, ca, ia);
                    const auto unit = vec3<double>(ra, ca, ia).unit(); // Unit vector in direction of gradient.

                    const long int channel = (user_data_s->channel < 0) ? 0 : user_data_s->channel;
                    const auto n_magn_m = nms_working.trilinearly_interpolate(pos - unit, channel);
                    const auto n_magn_p = nms_working.trilinearly_interpolate(pos + unit, channel);

                    float new_val = 0.0f;
                    if( true
                    && std::isfinite(n_magn_m)
                    && std::isfinite(n_magn_p)
                    && (n_magn_m <= magn)
                    && (n_magn_p <= magn) ){
                        new_val = magn;
                    }
                    return new_val;
            };

        }else{
            throw std::invalid_argument("Selected method not applicable to selected order or estimator.");
        }

    }else if(user_data_s->order == VolumetricSpatialDerivativeEstimator::Sobel_3x3x3){
        ud.voxel_triplets = {{ { -1, -1, -1 },    //  0
                               { -1,  0, -1 },    //  1
                               { -1,  1, -1 },    //  2
                               {  0, -1, -1 },    //  3
                               {  0,  0, -1 },    //  4
                               {  0,  1, -1 },    //  5
                               {  1, -1, -1 },    //  6
                               {  1,  0, -1 },    //  7
                               {  1,  1, -1 },    //  8

                               { -1, -1,  0 },    //  9
                               { -1,  0,  0 },    // 10
                               { -1,  1,  0 },    // 11
                               {  0, -1,  0 },    // 12
                               {  0,  0,  0 },    // 13
                               {  0,  1,  0 },    // 14
                               {  1, -1,  0 },    // 15
                               {  1,  0,  0 },    // 16
                               {  1,  1,  0 },    // 17

                               { -1, -1,  1 },    // 18
                               { -1,  0,  1 },    // 19
                               { -1,  1,  1 },    // 20
                               {  0, -1,  1 },    // 21
                               {  0,  0,  1 },    // 22
                               {  0,  1,  1 },    // 23
                               {  1, -1,  1 },    // 24
                               {  1,  0,  1 },    // 25
                               {  1,  1,  1 } }}; // 26

        // Note: The convolution kernel used here was adapted from
        // https://en.wikipedia.org/wiki/Sobel_operator#Extension_to_other_dimensions (accessed 20190226).
        const auto row_aligned = [](float, std::vector<float> &shtl, vec3<double>) -> float {
                                const auto r_m_c_m_i_m = std::isfinite(shtl[ 0]) ? shtl[ 0] : shtl[13];
                                //const auto r_m_c_0_i_m = std::isfinite(shtl[ 1]) ? shtl[ 1] : shtl[13];
                                const auto r_m_c_p_i_m = std::isfinite(shtl[ 2]) ? shtl[ 2] : shtl[13];

                                const auto r_0_c_m_i_m = std::isfinite(shtl[ 3]) ? shtl[ 3] : shtl[13];
                                //const auto r_0_c_0_i_m = std::isfinite(shtl[ 4]) ? shtl[ 4] : shtl[13];
                                const auto r_0_c_p_i_m = std::isfinite(shtl[ 5]) ? shtl[ 5] : shtl[13];

                                const auto r_p_c_m_i_m = std::isfinite(shtl[ 6]) ? shtl[ 6] : shtl[13];
                                //const auto r_p_c_0_i_m = std::isfinite(shtl[ 7]) ? shtl[ 7] : shtl[13];
                                const auto r_p_c_p_i_m = std::isfinite(shtl[ 8]) ? shtl[ 8] : shtl[13];

                                const auto r_m_c_m_i_0 = std::isfinite(shtl[ 9]) ? shtl[ 9] : shtl[13];
                                //const auto r_m_c_0_i_0 = std::isfinite(shtl[10]) ? shtl[10] : shtl[13];
                                const auto r_m_c_p_i_0 = std::isfinite(shtl[11]) ? shtl[11] : shtl[13];

                                const auto r_0_c_m_i_0 = std::isfinite(shtl[12]) ? shtl[12] : shtl[13];
                                //const auto r_0_c_0_i_0 = shtl[13];
                                const auto r_0_c_p_i_0 = std::isfinite(shtl[14]) ? shtl[14] : shtl[13];

                                const auto r_p_c_m_i_0 = std::isfinite(shtl[15]) ? shtl[15] : shtl[13];
                                //const auto r_p_c_0_i_0 = std::isfinite(shtl[16]) ? shtl[16] : shtl[13];
                                const auto r_p_c_p_i_0 = std::isfinite(shtl[17]) ? shtl[17] : shtl[13];

                                const auto r_m_c_m_i_p = std::isfinite(shtl[18]) ? shtl[18] : shtl[13];
                                //const auto r_m_c_0_i_p = std::isfinite(shtl[19]) ? shtl[19] : shtl[13];
                                const auto r_m_c_p_i_p = std::isfinite(shtl[20]) ? shtl[20] : shtl[13];

                                const auto r_0_c_m_i_p = std::isfinite(shtl[21]) ? shtl[21] : shtl[13];
                                //const auto r_0_c_0_i_p = std::isfinite(shtl[22]) ? shtl[22] : shtl[13];
                                const auto r_0_c_p_i_p = std::isfinite(shtl[23]) ? shtl[23] : shtl[13];

                                const auto r_p_c_m_i_p = std::isfinite(shtl[24]) ? shtl[24] : shtl[13];
                                //const auto r_p_c_0_i_p = std::isfinite(shtl[25]) ? shtl[25] : shtl[13];
                                const auto r_p_c_p_i_p = std::isfinite(shtl[26]) ? shtl[26] : shtl[13];

                                return static_cast<float>( 
                                       (  1.0 * r_p_c_p_i_p
                                        + 1.0 * r_m_c_p_i_p
                                        + 1.0 * r_p_c_p_i_m
                                        + 1.0 * r_m_c_p_i_m

                                        + 2.0 * r_0_c_p_i_m
                                        + 2.0 * r_0_c_p_i_p
                                        + 2.0 * r_m_c_p_i_0
                                        + 2.0 * r_p_c_p_i_0

                                        + 4.0 * r_0_c_p_i_0

                                        - 1.0 * r_p_c_m_i_p
                                        - 1.0 * r_m_c_m_i_p
                                        - 1.0 * r_p_c_m_i_m
                                        - 1.0 * r_m_c_m_i_m

                                        - 2.0 * r_0_c_m_i_m
                                        - 2.0 * r_0_c_m_i_p
                                        - 2.0 * r_m_c_m_i_0
                                        - 2.0 * r_p_c_m_i_0

                                        - 4.0 * r_0_c_m_i_0)
                                       / 32.0 );
        };

        const auto col_aligned = [](float, std::vector<float> &shtl, vec3<double>) -> float {
                                const auto r_m_c_m_i_m = std::isfinite(shtl[ 0]) ? shtl[ 0] : shtl[13];
                                const auto r_m_c_0_i_m = std::isfinite(shtl[ 1]) ? shtl[ 1] : shtl[13];
                                const auto r_m_c_p_i_m = std::isfinite(shtl[ 2]) ? shtl[ 2] : shtl[13];

                                //const auto r_0_c_m_i_m = std::isfinite(shtl[ 3]) ? shtl[ 3] : shtl[13];
                                //const auto r_0_c_0_i_m = std::isfinite(shtl[ 4]) ? shtl[ 4] : shtl[13];
                                //const auto r_0_c_p_i_m = std::isfinite(shtl[ 5]) ? shtl[ 5] : shtl[13];

                                const auto r_p_c_m_i_m = std::isfinite(shtl[ 6]) ? shtl[ 6] : shtl[13];
                                const auto r_p_c_0_i_m = std::isfinite(shtl[ 7]) ? shtl[ 7] : shtl[13];
                                const auto r_p_c_p_i_m = std::isfinite(shtl[ 8]) ? shtl[ 8] : shtl[13];

                                const auto r_m_c_m_i_0 = std::isfinite(shtl[ 9]) ? shtl[ 9] : shtl[13];
                                const auto r_m_c_0_i_0 = std::isfinite(shtl[10]) ? shtl[10] : shtl[13];
                                const auto r_m_c_p_i_0 = std::isfinite(shtl[11]) ? shtl[11] : shtl[13];

                                //const auto r_0_c_m_i_0 = std::isfinite(shtl[12]) ? shtl[12] : shtl[13];
                                //const auto r_0_c_0_i_0 = shtl[13];
                                //const auto r_0_c_p_i_0 = std::isfinite(shtl[14]) ? shtl[14] : shtl[13];

                                const auto r_p_c_m_i_0 = std::isfinite(shtl[15]) ? shtl[15] : shtl[13];
                                const auto r_p_c_0_i_0 = std::isfinite(shtl[16]) ? shtl[16] : shtl[13];
                                const auto r_p_c_p_i_0 = std::isfinite(shtl[17]) ? shtl[17] : shtl[13];

                                const auto r_m_c_m_i_p = std::isfinite(shtl[18]) ? shtl[18] : shtl[13];
                                const auto r_m_c_0_i_p = std::isfinite(shtl[19]) ? shtl[19] : shtl[13];
                                const auto r_m_c_p_i_p = std::isfinite(shtl[20]) ? shtl[20] : shtl[13];

                                //const auto r_0_c_m_i_p = std::isfinite(shtl[21]) ? shtl[21] : shtl[13];
                                //const auto r_0_c_0_i_p = std::isfinite(shtl[22]) ? shtl[22] : shtl[13];
                                //const auto r_0_c_p_i_p = std::isfinite(shtl[23]) ? shtl[23] : shtl[13];

                                const auto r_p_c_m_i_p = std::isfinite(shtl[24]) ? shtl[24] : shtl[13];
                                const auto r_p_c_0_i_p = std::isfinite(shtl[25]) ? shtl[25] : shtl[13];
                                const auto r_p_c_p_i_p = std::isfinite(shtl[26]) ? shtl[26] : shtl[13];

                                return static_cast<float>( 
                                       (  1.0 * r_p_c_p_i_p
                                        + 1.0 * r_p_c_m_i_p
                                        + 1.0 * r_p_c_p_i_m
                                        + 1.0 * r_p_c_m_i_m

                                        + 2.0 * r_p_c_0_i_m
                                        + 2.0 * r_p_c_0_i_p
                                        + 2.0 * r_p_c_m_i_0
                                        + 2.0 * r_p_c_p_i_0

                                        + 4.0 * r_p_c_0_i_0

                                        - 1.0 * r_m_c_p_i_p
                                        - 1.0 * r_m_c_m_i_p
                                        - 1.0 * r_m_c_p_i_m
                                        - 1.0 * r_m_c_m_i_m

                                        - 2.0 * r_m_c_0_i_m
                                        - 2.0 * r_m_c_0_i_p
                                        - 2.0 * r_m_c_m_i_0
                                        - 2.0 * r_m_c_p_i_0

                                        - 4.0 * r_m_c_0_i_0)
                                       / 32.0 );
        };

        const auto img_aligned = [](float, std::vector<float> &shtl, vec3<double>) -> float {
                                const auto r_m_c_m_i_m = std::isfinite(shtl[ 0]) ? shtl[ 0] : shtl[13];
                                const auto r_m_c_0_i_m = std::isfinite(shtl[ 1]) ? shtl[ 1] : shtl[13];
                                const auto r_m_c_p_i_m = std::isfinite(shtl[ 2]) ? shtl[ 2] : shtl[13];

                                const auto r_0_c_m_i_m = std::isfinite(shtl[ 3]) ? shtl[ 3] : shtl[13];
                                const auto r_0_c_0_i_m = std::isfinite(shtl[ 4]) ? shtl[ 4] : shtl[13];
                                const auto r_0_c_p_i_m = std::isfinite(shtl[ 5]) ? shtl[ 5] : shtl[13];

                                const auto r_p_c_m_i_m = std::isfinite(shtl[ 6]) ? shtl[ 6] : shtl[13];
                                const auto r_p_c_0_i_m = std::isfinite(shtl[ 7]) ? shtl[ 7] : shtl[13];
                                const auto r_p_c_p_i_m = std::isfinite(shtl[ 8]) ? shtl[ 8] : shtl[13];

                                //const auto r_m_c_m_i_0 = std::isfinite(shtl[ 9]) ? shtl[ 9] : shtl[13];
                                //const auto r_m_c_0_i_0 = std::isfinite(shtl[10]) ? shtl[10] : shtl[13];
                                //const auto r_m_c_p_i_0 = std::isfinite(shtl[11]) ? shtl[11] : shtl[13];

                                //const auto r_0_c_m_i_0 = std::isfinite(shtl[12]) ? shtl[12] : shtl[13];
                                //const auto r_0_c_0_i_0 = shtl[13];
                                //const auto r_0_c_p_i_0 = std::isfinite(shtl[14]) ? shtl[14] : shtl[13];

                                //const auto r_p_c_m_i_0 = std::isfinite(shtl[15]) ? shtl[15] : shtl[13];
                                //const auto r_p_c_0_i_0 = std::isfinite(shtl[16]) ? shtl[16] : shtl[13];
                                //const auto r_p_c_p_i_0 = std::isfinite(shtl[17]) ? shtl[17] : shtl[13];

                                const auto r_m_c_m_i_p = std::isfinite(shtl[18]) ? shtl[18] : shtl[13];
                                const auto r_m_c_0_i_p = std::isfinite(shtl[19]) ? shtl[19] : shtl[13];
                                const auto r_m_c_p_i_p = std::isfinite(shtl[20]) ? shtl[20] : shtl[13];

                                const auto r_0_c_m_i_p = std::isfinite(shtl[21]) ? shtl[21] : shtl[13];
                                const auto r_0_c_0_i_p = std::isfinite(shtl[22]) ? shtl[22] : shtl[13];
                                const auto r_0_c_p_i_p = std::isfinite(shtl[23]) ? shtl[23] : shtl[13];

                                const auto r_p_c_m_i_p = std::isfinite(shtl[24]) ? shtl[24] : shtl[13];
                                const auto r_p_c_0_i_p = std::isfinite(shtl[25]) ? shtl[25] : shtl[13];
                                const auto r_p_c_p_i_p = std::isfinite(shtl[26]) ? shtl[26] : shtl[13];

                                return static_cast<float>( 
                                       (  1.0 * r_p_c_p_i_p
                                        + 1.0 * r_m_c_p_i_p
                                        + 1.0 * r_p_c_m_i_p
                                        + 1.0 * r_m_c_m_i_p

                                        + 2.0 * r_0_c_m_i_p
                                        + 2.0 * r_0_c_p_i_p
                                        + 2.0 * r_m_c_0_i_p
                                        + 2.0 * r_p_c_0_i_p

                                        + 4.0 * r_0_c_0_i_p

                                        - 1.0 * r_p_c_p_i_m
                                        - 1.0 * r_m_c_p_i_m
                                        - 1.0 * r_p_c_m_i_m
                                        - 1.0 * r_m_c_m_i_m

                                        - 2.0 * r_0_c_m_i_m
                                        - 2.0 * r_0_c_p_i_m
                                        - 2.0 * r_m_c_0_i_m
                                        - 2.0 * r_p_c_0_i_m

                                        - 4.0 * r_0_c_0_i_m)
                                       / 32.0 );
        };

        if(false){
        }else if(user_data_s->method == VolumetricSpatialDerivativeMethod::row_aligned){
            ud.f_reduce = row_aligned;

        }else if(user_data_s->method == VolumetricSpatialDerivativeMethod::column_aligned){
            ud.f_reduce = col_aligned;

        }else if(user_data_s->method == VolumetricSpatialDerivativeMethod::image_aligned){
            ud.f_reduce = img_aligned;

        }else if(user_data_s->method == VolumetricSpatialDerivativeMethod::magnitude){
            ud.f_reduce = [&](float v, std::vector<float> &shtl, vec3<double> pos) -> float {
                                return std::hypot( row_aligned(v, shtl, pos),
                                                   col_aligned(v, shtl, pos),
                                                   img_aligned(v, shtl, pos) );
            };

        }else if(user_data_s->method == VolumetricSpatialDerivativeMethod::non_maximum_suppression){
            // Note: This method is extremely slow, requiring a fresh search for each trilinear interpolation.
            // It can easily be made faster, but might require modifying the volumetric neighbourhood sampler. TODO.
            ud.f_reduce = [&](float v, std::vector<float> &shtl, vec3<double> pos) -> float {
                    const auto ra = row_aligned(v, shtl, pos);
                    const auto ca = col_aligned(v, shtl, pos);
                    const auto ia = img_aligned(v, shtl, pos);

                    const auto magn = std::hypot(ra, ca, ia);
                    const auto unit = vec3<double>(ra, ca, ia).unit(); // Unit vector in direction of gradient.

                    const long int channel = (user_data_s->channel < 0) ? 0 : user_data_s->channel;
                    const auto n_magn_m = nms_working.trilinearly_interpolate(pos - unit, channel);
                    const auto n_magn_p = nms_working.trilinearly_interpolate(pos + unit, channel);

                    float new_val = 0.0f;
                    if( true
                    && std::isfinite(n_magn_m)
                    && std::isfinite(n_magn_p)
                    && (n_magn_m <= magn)
                    && (n_magn_p <= magn) ){
                        new_val = magn;
                    }
                    return new_val;
            }; 

        }else{
            throw std::invalid_argument("Selected method not applicable to selected order or estimator.");
        }
    }else{
        throw std::invalid_argument("Unrecognized user-provided estimator argument.");
    }

    // Invoke the volumetric sampling routine to compute the above functors.
    if(!imagecoll.Compute_Images( ComputeVolumetricNeighbourhoodSampler, 
                                  {}, ccsl, &ud )){
        throw std::runtime_error("Unable to compute volumetric spatial derivative.");
    }


    //Update the image metadata. 
    std::string img_desc;
    if(false){
    }else if(user_data_s->order == VolumetricSpatialDerivativeEstimator::first){
        img_desc += "First-order spatial deriv.,";

    }else if(user_data_s->order == VolumetricSpatialDerivativeEstimator::Sobel_3x3x3){
        img_desc += "Sobel 3x3x3 estimator,";

    }else{
        throw std::invalid_argument("Unrecognized user-provided derivative order.");
    }

    if(false){
    }else if(user_data_s->method == VolumetricSpatialDerivativeMethod::row_aligned){
        img_desc += " row-aligned";

    }else if(user_data_s->method == VolumetricSpatialDerivativeMethod::column_aligned){
        img_desc += " column-aligned";

    }else if(user_data_s->method == VolumetricSpatialDerivativeMethod::image_aligned){
        img_desc += " image-aligned";

    }else if(user_data_s->method == VolumetricSpatialDerivativeMethod::magnitude){
        img_desc += " magnitude";

    }else if(user_data_s->method == VolumetricSpatialDerivativeMethod::non_maximum_suppression){
        img_desc += " magnitude (thinned)";

    }else{
        throw std::invalid_argument("Unrecognized user-provided derivative method.");
    }
    img_desc += " (in pixel coord.s)";

    for(auto &img : imagecoll.images){
        UpdateImageDescription( std::ref(img), img_desc );
        UpdateImageWindowCentreWidth( std::ref(img) );
    }

    return true;
}

