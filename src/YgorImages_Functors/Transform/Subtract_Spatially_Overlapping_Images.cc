
#include <YgorStats.h>
#include <experimental/any>
#include <functional>
#include <list>

#include "../ConvenienceRoutines.h"
#include "YgorImages.h"
#include "YgorMath.h"


//Subtracts the provided external images that spatially overlap on a voxel-by-voxel basis.
bool SubtractSpatiallyOverlappingImages(planar_image_collection<float,double>::images_list_it_t  local_img_it,
                                        std::list<std::reference_wrapper<planar_image_collection<float,double>>> external_imgs,
                                        std::list<std::reference_wrapper<contour_collection<double>>>, 
                                        std::experimental::any ){

    //Record the min and max actual pixel values for windowing purposes.
    Stats::Running_MinMax<float> minmax_pixel;

    //Iterate over the external images. We will subtract them all.
    const auto img_cntr  = local_img_it->center();
    const auto img_ortho = local_img_it->row_unit.Cross( local_img_it->col_unit ).unit();
    const std::list<vec3<double>> points = { img_cntr, img_cntr + img_ortho * local_img_it->pxl_dz * 0.25,
                                                       img_cntr - img_ortho * local_img_it->pxl_dz * 0.25 };
    for(auto & ext_img : external_imgs){
        auto overlapping_imgs = ext_img.get().get_images_which_encompass_all_points(points);

        for(auto & overlapping_img : overlapping_imgs){

            //Loop over the rows, columns, and channels.
            for(auto row = 0; row < local_img_it->rows; ++row){
                for(auto col = 0; col < local_img_it->columns; ++col){
                    for(auto chan = 0; chan < local_img_it->channels; ++chan){

                        const auto Lval = local_img_it->value(row, col, chan);
                        const auto Rval = overlapping_img->value(row, col, chan);
                        const auto newval = (Lval - Rval);

                        local_img_it->reference(row, col, chan) = newval;
                        minmax_pixel.Digest(newval);
                    }
                }
            }
        }
    }

    UpdateImageDescription( std::ref(*local_img_it), "Subtracted" );
    UpdateImageWindowCentreWidth( std::ref(*local_img_it), minmax_pixel );

    return true;
}




