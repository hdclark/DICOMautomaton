//Alignment_Field.cc - A part of DICOMautomaton 2021. Written by hal clark.

#include <algorithm>
#include <optional>
#include <fstream>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <regex>
#include <set> 
#include <stdexcept>
#include <string>    
#include <utility>            //Needed for std::pair.
#include <vector>
#include <functional>

#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorLog.h"
#include "YgorStats.h"        //Needed for Stats:: namespace.
#include "YgorString.h"       //Needed for GetFirstRegex(...)

#include "Structs.h"
#include "Regex_Selectors.h"
#include "Thread_Pool.h"

#include "Alignment_Rigid.h"
#include "Alignment_Field.h"

using namespace AlignViaFieldHelpers;

// Helper functions.
//
// Don't forget to prefix helper definitions here with AlignViaDemonsHelpers namespace if needed.
//
// ...


// Class members.

deformation_field::deformation_field(std::istream &is){
    if(!this->read_from(is)){
        throw std::invalid_argument("Input not understood, refusing to contruct empty field");
    }
}

deformation_field::deformation_field(planar_image_collection<double,double> &&in){
    this->swap_and_rebuild(in);
}

void
deformation_field::swap_and_rebuild(planar_image_collection<double,double> &in){

    // Imbib the images to avoid invalid references making their way into the index.
    this->field.Swap(in);
    try{
        // Ensure the image array is regular. (This will allow us to use a faster postion-to-image lookup.)
        // Also ensure images are present, and every image has three channels.
        std::list<std::reference_wrapper<planar_image<double,double>>> selected_imgs;
        for(auto &img : this->field.images){
            if(img.channels != 3) throw std::invalid_argument("Encountered image without three channels");
            selected_imgs.push_back( std::ref(img) );
        }
        if( selected_imgs.empty() ) throw std::invalid_argument("No images provided");
        if(!Images_Form_Regular_Grid(selected_imgs)){
            throw std::invalid_argument("Images do not form a rectilinear grid. Cannot continue");
        }

        const auto row_unit = this->field.images.front().row_unit.unit();
        const auto col_unit = this->field.images.front().col_unit.unit();
        const auto img_unit = this->field.images.front().ortho_unit();

        planar_image_adjacency<double,double> img_adj( {}, { { std::ref(this->field) } }, img_unit );
        this->adj = img_adj;
        
    }catch(const std::exception &){
        this->field.Swap(in);
        throw;
    }
    return;
}

std::reference_wrapper<const planar_image_collection<double,double>>
deformation_field::get_imagecoll_crefw() const {
    return std::cref(this->field);
};

vec3<double> 
deformation_field::transform(const vec3<double> &v) const {
    const double oob = 0.0;
    const auto dx = this->adj.value().trilinearly_interpolate(v, 0, 0.0);
    const auto dy = this->adj.value().trilinearly_interpolate(v, 1, 0.0);
    const auto dz = this->adj.value().trilinearly_interpolate(v, 2, 0.0);
    return v + vec3<double>(dx, dy, dz);
}

void
deformation_field::apply_to(point_set<double> &ps) const {
    for(auto &p : ps.points) p = this->transform(p);
    return;
}

void
deformation_field::apply_to(vec3<double> &v) const {
    v = this->transform(v);
    return;
}

void
deformation_field::apply_to(planar_image<float, double> &img,
                            deformation_field_warp_method method) const {
    const int64_t N_rows = img.rows;
    const int64_t N_cols = img.columns;
    const int64_t N_chnls = img.channels;

    if(method == deformation_field_warp_method::pull){
        // Pull-based: for each output voxel, approximate the inverse displacement,
        // then sample from the original image at the source position.
        //
        // The deformation field D maps: output(x) = input(x + D(x)).
        // To invert, we need to find x' such that x' + D(x') = x, i.e., x' = x - D(x').
        // We approximate the inverse iteratively using fixed-point iteration:
        //   x'_0 = x
        //   x'_{n+1} = x - D(x'_n)
        // A small number of iterations suffices for smooth fields.
        const int64_t N_inversion_iters = 10;

        // Make a copy of the original image data for sampling.
        const planar_image<float, double> orig_img = img;
        planar_image_collection<float, double> orig_coll;
        orig_coll.images.push_back(orig_img);

        for(int64_t row = 0; row < N_rows; ++row){
            for(int64_t col = 0; col < N_cols; ++col){
                const auto pos = img.position(row, col);

                // Iterative inversion of the deformation field.
                vec3<double> source_pos = pos;
                for(int64_t iter = 0; iter < N_inversion_iters; ++iter){
                    const auto dx = this->adj.value().trilinearly_interpolate(source_pos, 0, 0.0);
                    const auto dy = this->adj.value().trilinearly_interpolate(source_pos, 1, 0.0);
                    const auto dz = this->adj.value().trilinearly_interpolate(source_pos, 2, 0.0);
                    source_pos = pos - vec3<double>(dx, dy, dz);
                }

                for(int64_t chnl = 0; chnl < N_chnls; ++chnl){
                    const auto val = orig_coll.trilinearly_interpolate(source_pos, chnl,
                                                                       std::numeric_limits<float>::quiet_NaN());
                    img.reference(row, col, chnl) = val;
                }
            }
        }

    }else if(method == deformation_field_warp_method::push){
        // Push-based: for each input voxel, push its value to the displaced position.
        // This can leave gaps in the output where no source voxel maps to.
        const planar_image<float, double> orig_img = img;

        // Initialize output to NaN to identify gaps.
        for(auto &val : img.data){
            val = std::numeric_limits<float>::quiet_NaN();
        }
        // Also track accumulated weights for averaging overlapping contributions.
        std::vector<double> weights(img.data.size(), 0.0);

        for(int64_t row = 0; row < N_rows; ++row){
            for(int64_t col = 0; col < N_cols; ++col){
                const auto pos = orig_img.position(row, col);
                const auto displaced_pos = this->transform(pos);

                // Determine which output voxel the displaced position falls into.
                // Use nearest-neighbor assignment for the push.
                // Compute fractional row/col in the output image.
                const auto diff = displaced_pos - img.anchor - img.offset;
                const double frac_col = diff.Dot(img.row_unit) / img.pxl_dx;
                const double frac_row = diff.Dot(img.col_unit) / img.pxl_dy;

                const int64_t out_col = static_cast<int64_t>(std::round(frac_col));
                const int64_t out_row = static_cast<int64_t>(std::round(frac_row));

                if(out_row >= 0 && out_row < N_rows && out_col >= 0 && out_col < N_cols){
                    for(int64_t chnl = 0; chnl < N_chnls; ++chnl){
                        const float src_val = orig_img.value(row, col, chnl);
                        if(!std::isfinite(src_val)) continue;

                        const int64_t idx = img.index(out_row, out_col, chnl);
                        if(!std::isfinite(img.data[idx])){
                            img.data[idx] = src_val;
                        }else{
                            img.data[idx] += src_val;
                        }
                        weights[idx] += 1.0;
                    }
                }
            }
        }

        // Normalize by weight where multiple source voxels contributed.
        for(size_t i = 0; i < img.data.size(); ++i){
            if(weights[i] > 1.0){
                img.data[i] /= static_cast<float>(weights[i]);
            }
        }
    }else{
        throw std::logic_error("Unknown warp method");
    }
    return;
}

void
deformation_field::apply_to(planar_image_collection<float, double> &img_coll,
                            deformation_field_warp_method method) const {
    for(auto &img : img_coll.images){
        this->apply_to(img, method);
    }
    return;
}

bool
deformation_field::write_to( std::ostream &os ) const {
    // Maximize precision prior to emitting any floating-point numbers.
    const auto original_precision = os.precision();
    os.precision( std::numeric_limits<double>::max_digits10 );

    const auto N_imgs = static_cast<int64_t>(this->field.images.size());
    os << N_imgs << std::endl;

    for(const auto &img : this->field.images){
        // Write image geometry.
        os << img.rows << " " << img.columns << " " << img.channels << std::endl;
        os << img.pxl_dx << " " << img.pxl_dy << " " << img.pxl_dz << std::endl;
        os << img.anchor << std::endl;
        os << img.offset << std::endl;
        os << img.row_unit << std::endl;
        os << img.col_unit << std::endl;

        // Write pixel data.
        for(const auto &val : img.data){
            os << val << std::endl;
        }
    }

    os.precision( original_precision );
    os.flush();
    return (!os.fail());
}

bool
deformation_field::read_from( std::istream &is ){
    int64_t N_imgs = 0;
    is >> N_imgs;
    if( is.fail() || !isininc(1, N_imgs, 1'000'000'000) ){
        YLOGWARN("Number of images could not be read, or is invalid.");
        return false;
    }

    planar_image_collection<double, double> new_field;

    try{
        for(int64_t i = 0; i < N_imgs; ++i){
            int64_t rows = 0, cols = 0, channels = 0;
            is >> rows >> cols >> channels;
            if( is.fail() || rows <= 0 || cols <= 0 || channels != 3 ){
                YLOGWARN("Image dimensions could not be read, or are invalid.");
                return false;
            }

            double pxl_dx = 0.0, pxl_dy = 0.0, pxl_dz = 0.0;
            is >> pxl_dx >> pxl_dy >> pxl_dz;
            if( is.fail() ){
                YLOGWARN("Pixel dimensions could not be read.");
                return false;
            }

            vec3<double> anchor, offset, row_unit, col_unit;
            is >> anchor >> offset >> row_unit >> col_unit;
            if( is.fail() ){
                YLOGWARN("Image spatial parameters could not be read.");
                return false;
            }

            planar_image<double, double> img;
            img.init_orientation(row_unit, col_unit);
            img.init_buffer(rows, cols, channels);
            img.init_spatial(pxl_dx, pxl_dy, pxl_dz, anchor, offset);

            for(auto &val : img.data){
                is >> val;
            }
            if( is.fail() ){
                YLOGWARN("Pixel data could not be read.");
                return false;
            }

            new_field.images.push_back(img);
        }
    }catch(const std::exception &e){
        YLOGWARN("Failed to read deformation field: " << e.what());
        return false;
    }

    this->swap_and_rebuild(new_field);
    return (!is.fail());
}

