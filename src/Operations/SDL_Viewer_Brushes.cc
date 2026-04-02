// SDL_Viewer_Brushes.cc - Brush types and painting functions for SDL_Viewer.
//
// A part of DICOMautomaton 2020-2025. Written by hal clark.
//

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <vector>

#include "YgorImages.h"
#include "YgorLog.h"
#include "YgorMath.h"
#include "YgorStats.h"

#include "SDL_Viewer_Brushes.h"
#include "SDL_Viewer_Utils.h"


void draw_with_brush( const decltype(planar_image_collection<float,double>().get_all_images()) &img_its,
                      const std::vector<line_segment<double>> &lss,
                      brush_t brush,
                      float radius,
                      float intensity,
                      int64_t channel,
                      float intensity_min,
                      float intensity_max,
                      bool is_additive){

    YLOGINFO("Implementing brush stroke");

    // Pre-extract the line segment vertices for bounding-box calculation.
    std::vector<vec3<double>> verts;
    for(const auto& l : lss){
        for(const auto &p : { l.Get_R0(), l.Get_R1() }){
            verts.emplace_back(p);
        }
    }
    double buffer_space = radius;
    if( (brush == brush_t::rigid_circle)
    ||  (brush == brush_t::rigid_square)
    ||  (brush == brush_t::median_circle)
    ||  (brush == brush_t::median_square)
    ||  (brush == brush_t::mean_circle)
    ||  (brush == brush_t::mean_square)
    ||  (brush == brush_t::rigid_sphere)
    ||  (brush == brush_t::rigid_cube)
    ||  (brush == brush_t::median_sphere)
    ||  (brush == brush_t::median_cube)
    ||  (brush == brush_t::mean_sphere)
    ||  (brush == brush_t::mean_cube) ){
        buffer_space = radius;

    }else if( (brush == brush_t::gaussian_2D)
    ||        (brush == brush_t::gaussian_3D) ){
        buffer_space = radius * 2.25;

    }else if( (brush == brush_t::tanh_2D)
    ||        (brush == brush_t::tanh_3D) ){
        buffer_space = radius * 1.5;
    }

    const auto apply_to_inner_pixels = [&](const decltype(planar_image_collection<float,double>().get_all_images()) &l_img_its,
                                           const std::function<float(const vec3<double> &, double, float)> &f){
        for(auto &cit : l_img_its){

            // Filter out irrelevant images.
            const auto img_is_relevant = [&]() -> bool {
                if( (cit->rows <= 0)
                ||  (cit->columns <= 0)
                ||  (cit->channels <= 0) ) return false;

                for(const auto& l : lss){
                    const auto plane_dist_R0 = cit->image_plane().Get_Signed_Distance_To_Point(l.Get_R0());
                    const auto plane_dist_R1 = cit->image_plane().Get_Signed_Distance_To_Point(l.Get_R1());

                    if( std::signbit(plane_dist_R0) != std::signbit(plane_dist_R1) ){
                        // Line segment crosses the image plane, so is automatically relevant.
                        return true;
                    }

                    // 2D brushes.
                    if( (brush == brush_t::rigid_circle)
                    ||  (brush == brush_t::rigid_square)
                    ||  (brush == brush_t::tanh_2D)
                    ||  (brush == brush_t::gaussian_2D)
                    ||  (brush == brush_t::median_circle)
                    ||  (brush == brush_t::median_square)
                    ||  (brush == brush_t::mean_circle)
                    ||  (brush == brush_t::mean_square) ){
                        if( (std::abs(plane_dist_R0) <= cit->pxl_dz * 0.5)
                        ||  (std::abs(plane_dist_R1) <= cit->pxl_dz * 0.5) ){
                            return true;
                        }

                    // 3D brushes.
                    }else if( (brush == brush_t::rigid_sphere)
                          ||  (brush == brush_t::rigid_cube) 
                          ||  (brush == brush_t::gaussian_3D)
                          ||  (brush == brush_t::tanh_3D)
                          ||  (brush == brush_t::median_sphere) 
                          ||  (brush == brush_t::median_cube)
                          ||  (brush == brush_t::mean_sphere) 
                          ||  (brush == brush_t::mean_cube) ){
                        if( (std::abs(plane_dist_R0) <= buffer_space)
                        ||  (std::abs(plane_dist_R1) <= buffer_space) ){
                            return true;
                        }
                    }
                }
                return false;
            };
            if(!img_is_relevant()) continue;

            // Compute pixel-space axis-aligned bounding box to reduce overall computation.
            //
            // Process relevant images.
            const auto [row_min, row_max, col_min, col_max] = get_pixelspace_axis_aligned_bounding_box(*cit, verts, buffer_space);
            for(int64_t r = row_min; r <= row_max; ++r){
                for(int64_t c = col_min; c <= col_max; ++c){
                    const auto pos = cit->position(r,c);
                    vec3<double> closest;
                    {
                        double closest_dist = 1E99;
                        for(const auto &l : lss){
                            const bool degenerate = ( (l.Get_R0()).sq_dist(l.Get_R1()) < 0.01 );
                            const auto closest_l = (degenerate) ? l.Get_R0() : l.Closest_Point_To(pos);
                            const auto dist = closest_l.distance(pos);
                            if(dist < closest_dist){
                                closest = closest_l;
                                closest_dist = dist;
                            }
                        }
                    }

                    const auto dR = closest.distance(pos);
                    if( (brush == brush_t::rigid_circle)
                    ||  (brush == brush_t::rigid_sphere)
                    ||  (brush == brush_t::median_circle)
                    ||  (brush == brush_t::mean_circle)
                    ||  (brush == brush_t::median_sphere)
                    ||  (brush == brush_t::mean_sphere)
                    ||  (brush == brush_t::tanh_2D)
                    ||  (brush == brush_t::gaussian_2D)
                    ||  (brush == brush_t::gaussian_3D) 
                    ||  (brush == brush_t::tanh_3D) ){
                        if(buffer_space < dR) continue;

                    }else if( (brush == brush_t::rigid_square)
                          ||  (brush == brush_t::median_square)
                          ||  (brush == brush_t::mean_square) ){
                        if( (buffer_space < std::abs((closest - pos).Dot(cit->row_unit)))
                        ||  (buffer_space < std::abs((closest - pos).Dot(cit->col_unit))) ) continue;

                    }else if( (brush == brush_t::median_cube)
                          ||  (brush == brush_t::rigid_cube)
                          ||  (brush == brush_t::mean_cube) ){
                        if( (buffer_space < std::abs((closest - pos).Dot(cit->row_unit)))
                        ||  (buffer_space < std::abs((closest - pos).Dot(cit->col_unit)))
                        ||  (buffer_space < std::abs((closest - pos).Dot(cit->row_unit.Cross(cit->col_unit)))) ) continue;

                        //if(buffer_space < dR) continue;
                    }

                    cit->reference( r, c, channel ) = std::clamp<float>(f(pos, dR, cit->value(r, c, channel)), intensity_min, intensity_max);
                }
            }
        }
    };


    // Implement brushes.
    if( (brush == brush_t::rigid_circle)
    ||  (brush == brush_t::rigid_square) ){
        for(const auto &img_it : img_its){
            apply_to_inner_pixels({img_it}, [intensity,is_additive](const vec3<double> &, double, float) -> float {
                const float l_intensity = (is_additive) ? intensity : 0.0f;
                return l_intensity;
            });
        }

    }else if( (brush == brush_t::gaussian_2D)
          ||  (brush == brush_t::gaussian_3D) ){
        for(const auto &img_it : img_its){
            apply_to_inner_pixels({img_it}, [radius,intensity,is_additive](const vec3<double> &, double dR, float v) -> float {
                // Approach the desired intensity at a rate dependent on the location; proportional to a spatial
                // Gaussian.
                const float l_intensity = (is_additive) ? intensity : 0.0f;
                const float scale = 0.65f;
                const auto l_exp = std::exp( -std::pow(dR / (scale * radius), 2.0f) );
                return (l_intensity - v) * l_exp + v;
            });
        }

    }else if( (brush == brush_t::tanh_2D)
          ||  (brush == brush_t::tanh_3D) ){
        for(const auto &img_it : img_its){
            apply_to_inner_pixels({img_it}, [radius,intensity,is_additive](const vec3<double> &, double dR, float v) -> float {
                const float l_intensity = (is_additive) ? intensity : 0.0f;
                const float old_v = v;
                const float steepness = 1.5f;  // How steep the perimeter of the brush is. Also impacts contour detail.
                const float paint_flow_rate = 1.0f; // "Strength" of the brush stroke.

                // Find proposed brush intensity.
                auto l_tanh = 0.5 * (1.0 + std::tanh( steepness * (radius - dR)));
                l_tanh = is_additive ? l_tanh : (1.0f - l_tanh); // Flip distribution vertically if subtracting.
                l_tanh *= intensity; // Scale distribution to target intensity @ maximum.

                // Alter brush behaviour based on whether the current voxel's intensity is above or below the target,
                // whether in additive or subtractive mode, and whether the voxel is within the brush boundary.
                //
                // This system has weird behaviour for negative intensities and when in drawing mode and painting
                // multiple intensities. But it otherwise works intuitively and provides accurate contours (e.g.,
                // the contours produced have the correct dimensions). It is also economical, requiring lower mask
                // resolution to accomplish the same contour smoothness.
                const bool is_mode_aligned = (is_additive == (l_tanh >= old_v));
                const bool is_inside_brush = (dR <= radius);
                float new_v = old_v;
                if(is_mode_aligned){
                    // Free to increase or decrease in intensity. The boundary should stay reasonably accurate.
                    new_v = l_tanh;

                }else if(!is_mode_aligned && is_inside_brush){
                    // Pull the intensity to the target intensity somewhat quickly, i.e., the maximum intensity
                    // the brush can make. This allows the brush to honour the proposed intensity, but won't leave
                    // noticeable edges when performing a brush stroke.

                    //const bool is_beyond = (is_additive == (l_intensity < old_v));
                    new_v = (l_intensity - old_v) * 0.5f + old_v;

                }else if(!is_mode_aligned && !is_inside_brush){
                    // Do nothing.

                    // Note: pulling the intensity to the desired tanh shape *outside* the brush when not mode
                    // aligned produces counter-intuitive results. Performing a brush stroke results in a jagged and
                    // rough line, and sweeping results in a shape like an exclamation mark. It will also produce a
                    // 'moat' around the current brush location if held long enough.
                }

                // Perform final blend using brush stroke strength.
                return (new_v - old_v) * paint_flow_rate + old_v;
            });
        }

    }else if( (brush == brush_t::median_circle)
          ||  (brush == brush_t::median_square) ){
        for(const auto &img_it : img_its){
            std::vector<float> vals;
            apply_to_inner_pixels({img_it}, [&vals](const vec3<double> &, double, float v) -> float {
                vals.emplace_back(v);
                return v;
            });
            const auto median = Stats::Median(vals);
            apply_to_inner_pixels({img_it}, [median](const vec3<double> &, double, float) -> float {
                return median;
            });
        }

    }else if( (brush == brush_t::mean_circle)
          ||  (brush == brush_t::mean_square) ){
        for(const auto &img_it : img_its){
            std::vector<float> vals;
            apply_to_inner_pixels({img_it}, [&vals](const vec3<double> &, double, float v) -> float {
                vals.emplace_back(v);
                return v;
            });
            const auto mean = Stats::Mean(vals);
            apply_to_inner_pixels({img_it}, [mean](const vec3<double> &, double, float) -> float {
                return mean;
            });
        }

    }else if( (brush == brush_t::rigid_sphere)
          ||  (brush == brush_t::rigid_cube) ){
        apply_to_inner_pixels(img_its, [intensity,is_additive](const vec3<double> &, double, float) -> float {
            const float l_intensity = (is_additive) ? intensity : 0.0f;
            return l_intensity;
        });

    }else if( (brush == brush_t::median_sphere)
          ||  (brush == brush_t::median_cube) ){
        std::vector<float> vals;
        for(const auto &img_it : img_its){
            apply_to_inner_pixels({img_it}, [&vals](const vec3<double> &, double, float v) -> float {
                vals.emplace_back(v);
                return v;
            });
        }
        const auto median = Stats::Median(vals);
        apply_to_inner_pixels(img_its, [median](const vec3<double> &, double, float) -> float {
            return median;
        });

    }else if( (brush == brush_t::mean_sphere)
          ||  (brush == brush_t::mean_cube) ){
        std::vector<float> vals;
        for(const auto &img_it : img_its){
            apply_to_inner_pixels({img_it}, [&vals](const vec3<double> &, double, float v) -> float {
                vals.emplace_back(v);
                return v;
            });
        }
        const auto mean = Stats::Mean(vals);
        apply_to_inner_pixels(img_its, [mean](const vec3<double> &, double, float) -> float {
            return mean;
        });
    }

    return;
}

