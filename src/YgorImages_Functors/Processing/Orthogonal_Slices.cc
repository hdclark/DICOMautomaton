
#include <cstdlib>
#include <algorithm>
#include <cmath>
#include <any>
#include <functional>
#include <iterator>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

//#include "../Grouping/Misc_Functors.h"
#include "Orthogonal_Slices.h"
#include "YgorImages.h"
#include "YgorMath.h"
#include "YgorMisc.h"


bool OrthogonalSlices(planar_image_collection<float,double>::images_list_it_t first_img_it,
                      std::list<planar_image_collection<float,double>::images_list_it_t> selected_img_its,
                      std::list<std::reference_wrapper<planar_image_collection<float,double>>> out_imgs,
                      std::list<std::reference_wrapper<contour_collection<double>>> ,
                      std::any ){

    //This routine computes slices that are orthogonal to the current slices (using the row and col units)
    // by explicitly sampling original slice voxels that overlap the new image voxels.
    //
    // There is no interpolation or smoothing that happens in this routine. A best-guess is given for the
    // number of outgoing images based on the maximum spatial separation between images (top and bottom).
    // The number of outgoing rows and columns will be dictated by the incoming number of rows and columns.
    // If you want to adjust the aspect ratio (e.g., if one of the dimensions is 'squat' compared to the
    // others), do so *after* this routine. The easiest way is to adjust each image's pxl_dz and offset.
    //
    // The input images do not necessarily need to be sorted in any particular order, nor do they have to 
    // have the same dimensions. However, if the row and column units or anchor/offset are different for
    // any images, this routine is not guaranteed to capture those voxels properly. You'll need to ensure
    // that: (1) the 'lowest' and 'highest' slices (as determined by sorting the distance along the 
    // orthogonal [i.e., the (row)x(col) unit axis]) are representative of the other images; (2) that
    // all images have the same (or approximately the same -- probably not an issue about machine precision
    // if the pixels are large enough and you can deal with an occasional aliased pixel) row- and column-
    // units; and (3) slices are not 'skewed' relative to one another (like a messy stack of paper).
    //
    // If the above criteria are not met, the worst that will happen is that some voxels will not be
    // captured (and will thus be filled with NaNs). If it is not possible to achieve the above, consider
    // writing a routine that takes an explicit bounding box, or pre-computed outgoing image geometries as
    // input.
    //
    // This routine will not work *well* with: unevenly-spaced or unequally-thick slices; images with over-
    // or under-lapping spatial extent; and image collections with differing numbers of rows/columns (among
    // other conditions!). In these situations, some voxels or even whole images may not be sampled 
    // properly. You might be able to address this by over-sampling (perhaps in specific directions), but 
    // this is not implemented here. [Just bump up N if you need a quick and dirty fix.]
    //
    
    //Verify the correct number of outgoing slots have been provided.
    if(out_imgs.size() != 2){
        throw std::invalid_argument("This routine needs exactly two outgoing planar_image_collection"
                                    " so the results can be passed back.");
    }

    //Get the common metadata with a dummy (empty) image collection.
    planar_image_collection<float,double> dummy;
    const auto common_metadata = dummy.get_common_metadata(selected_img_its);

    //Figure out image spatial attributes.
    const auto old_row_unit = first_img_it->row_unit;
    const auto old_col_unit = first_img_it->col_unit;
    const auto old_orto_unit = old_row_unit.Cross( old_col_unit );

    //Figure out the spatial extent of the images along the ortho_unit direction.
    const auto mm = std::minmax_element( std::begin(selected_img_its), 
                                         std::end(selected_img_its),
                    [=]( decltype(selected_img_its.front()) &A, 
                         decltype(selected_img_its.front()) &B ) -> bool {
                        //Work out the distance from an arbitrary slice.
                        //const auto A_dist = (A->offset - first_img_it->offset).Dot(old_orto_unit);
                        //const auto B_dist = (B->offset - first_img_it->offset).Dot(old_orto_unit);
                        //return Adist < B_dist;
                        return (A->offset).Dot(old_orto_unit) < (B->offset).Dot(old_orto_unit);
                    });
    const auto L = std::abs( ( (*(std::get<1>(mm)))->offset 
                             - (*(std::get<0>(mm)))->offset ).Dot(old_orto_unit) );

    //Guess at how many images will be needed in the ortho direction. We choose the minimum
    // needed to make a continuous outgoing image if each incoming image is just touching
    // the adjacent slice (i.e., the input images span a solid, non-overlapping volume).
    const auto N = L / first_img_it->pxl_dz;

    //const auto anchor = first_img_it->anchor;
    //const auto offset = first_img_it->offset;
    const auto anchor = (*(std::get<0>(mm)))->anchor;
    const auto offset = (*(std::get<0>(mm)))->offset;

    const auto new_row_unit = old_orto_unit;
    const auto new_pxl_dx = first_img_it->pxl_dz;
    const auto numb_of_rows = static_cast<long int>( std::ceil(N) );
    const auto numb_of_chns = first_img_it->channels;
    const long int img_skip = 50;  //Reduces output. Keep every Nth image; 1: keep all, 2: keep every other, etc..

    // ---- First set: 'row' aligned orthogonal images ----
    {
        const auto new_col_unit = old_row_unit; //Chosen to new_row x new_col = old_col
        const auto new_ortho_unit = old_col_unit;

        const auto new_pxl_dy = first_img_it->pxl_dx;
        const auto new_pxl_dz = first_img_it->pxl_dy;

        const auto numb_of_imgs = first_img_it->columns;
        const auto numb_of_cols = first_img_it->rows;

        for(auto i = 0; i < numb_of_imgs; ++i){
            if((i % img_skip) != 0) continue;

            out_imgs.front().get().images.emplace_back();
            out_imgs.front().get().images.back().init_buffer( numb_of_rows, numb_of_cols, numb_of_chns );
            out_imgs.front().get().images.back().init_spatial( new_pxl_dx, new_pxl_dy, new_pxl_dz, 
                                                               anchor, offset + new_ortho_unit * new_pxl_dz * i );
            out_imgs.front().get().images.back().init_orientation( new_row_unit, new_col_unit );
 
            out_imgs.front().get().images.back().fill_pixels(std::numeric_limits<float>::quiet_NaN());
          
            const auto count = Intersection_Copy( out_imgs.front().get().images.back(), selected_img_its );
            if(count == 0) FUNCWARN("Produced image with zero intersections. Bounds were not specified properly."
                                    " This is not an error, but a wasteful extra image has been created");

            out_imgs.front().get().images.back().metadata = common_metadata;
            out_imgs.front().get().images.back().metadata["Rows"] = std::to_string(numb_of_rows);
            out_imgs.front().get().images.back().metadata["Columns"] = std::to_string(numb_of_cols);
            out_imgs.front().get().images.back().metadata["PixelSpacing"] = std::to_string(new_pxl_dx) 
                                                                            + "^" + std::to_string(new_pxl_dy);
            out_imgs.front().get().images.back().metadata["SliceThickness"] = std::to_string(new_pxl_dz);
            out_imgs.front().get().images.back().metadata["Description"] = "Ortho Volume Intersection: Row";
        }
    }

    // ---- Second set: 'col' aligned orthogonal images ----
    {
        const auto new_col_unit = old_col_unit; //Chosen to new_row x new_col = old_col
        const auto new_ortho_unit = old_row_unit;

        const auto new_pxl_dy = first_img_it->pxl_dy;
        const auto new_pxl_dz = first_img_it->pxl_dx;
                                                                          
        const auto numb_of_imgs = first_img_it->rows;
        const auto numb_of_cols = first_img_it->columns;

        for(auto i = 0; i < numb_of_imgs; ++i){
            if((i % img_skip) != 0) continue;

            out_imgs.back().get().images.emplace_back();
            out_imgs.back().get().images.back().init_buffer( numb_of_rows, numb_of_cols, numb_of_chns );
            out_imgs.back().get().images.back().init_spatial( new_pxl_dx, new_pxl_dy, new_pxl_dz, 
                                                              anchor, offset + new_ortho_unit * new_pxl_dz * i );
            out_imgs.back().get().images.back().init_orientation( new_row_unit, new_col_unit );
 
            out_imgs.back().get().images.back().fill_pixels(std::numeric_limits<float>::quiet_NaN());
          
            const auto count = Intersection_Copy( out_imgs.back().get().images.back(), selected_img_its );
            if(count == 0) FUNCWARN("Produced image with zero intersections. Bounds were not specified properly."
                                    " This is not an error, but a wasteful extra image has been created");

            out_imgs.back().get().images.back().metadata = common_metadata;
            out_imgs.back().get().images.back().metadata["Rows"] = std::to_string(numb_of_rows);
            out_imgs.back().get().images.back().metadata["Columns"] = std::to_string(numb_of_cols);
            out_imgs.back().get().images.back().metadata["PixelSpacing"] = std::to_string(new_pxl_dx) 
                                                                           + "^" + std::to_string(new_pxl_dy);
            out_imgs.back().get().images.back().metadata["SliceThickness"] = std::to_string(new_pxl_dz);
            out_imgs.back().get().images.back().metadata["Description"] = "Ortho Volume Intersection: Column";

        }
    }

    return true;
}

