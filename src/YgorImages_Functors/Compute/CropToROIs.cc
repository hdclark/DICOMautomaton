//CropToROIs.cc.

#include <cmath>
#include <exception>
#include <any>
#include <functional>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <stdexcept>
#include <cstdint>

#include "CropToROIs.h"
#include "YgorImages.h"
#include "YgorMath.h"
#include "YgorMisc.h"
#include "YgorLog.h"


bool ComputeCropToROIs(planar_image_collection<float,double> &imagecoll,
                          std::list<std::reference_wrapper<planar_image_collection<float,double>>>,
                          std::list<std::reference_wrapper<contour_collection<double>>> ccsl,
                          std::any user_data ){

    //This routine takes an image volume (which does not necessarily have to cover a contiguous volume) and some ROI(s)
    // and crops the images, shrink-wrapping them to the row and column-unit axes-aligned bounding box (+ optional
    // margin). Orientation normals are currently derived from the first image -- the bounding volume will be aligned
    // with these normals.
    //
    // NOTE: The crop is performed globally, so all images will be cropped using the same planes. However, if images
    //       have differing pxl_dx, pxl_dy, or anchors/offsets then the volume edges are not guaranteed to be smooth.
    //
    // NOTE: This implementation assumes all images have identical row and column unit normals. They do not need to have
    //       identical sizes, origins, or extent pixel spacing.
    //
    // NOTE: The bounding boxes have an additional small number (epsilon) added as margin so thatvoxels on the boundary
    //       will be included in the inner volume. To counteract this, pass a negative margin.
    //
    // NOTE: Currently, if any of the corners are not bounded within the plane parallel to the first image's plane, then
    //       the whole image is cropped. (It would be costly to check the oblique intersection and this is not currently
    //       needed.)

    //We require a valid CropToROIsUserData struct packed into the user_data.
    CropToROIsUserData *user_data_s;
    try{
        user_data_s = std::any_cast<CropToROIsUserData *>(user_data);
    }catch(const std::exception &e){
        YLOGWARN("Unable to cast user_data to appropriate format. Cannot continue with computation");
        return false;
    }

    //Check that there are contours to operate on.
    if(ccsl.empty()){
        YLOGWARN("No contours provided. Cannot continue with computation");
        return false;
    }

    //Check that there are images to operate on.
    if(imagecoll.images.empty()){
        YLOGWARN("Missing images. There is nothing to crop");
        return false;
    }

    //Get orientation normals.
    const auto GridX = imagecoll.images.front().row_unit;
    const auto GridY = imagecoll.images.front().col_unit;
    const auto GridZ = imagecoll.images.front().image_plane().N_0;

    //Generate global bounds for the ROI vertices.
    double grid_x_min = std::numeric_limits<double>::quiet_NaN();
    double grid_x_max = std::numeric_limits<double>::quiet_NaN();
    double grid_y_min = std::numeric_limits<double>::quiet_NaN();
    double grid_y_max = std::numeric_limits<double>::quiet_NaN();
    double grid_z_min = std::numeric_limits<double>::quiet_NaN();
    double grid_z_max = std::numeric_limits<double>::quiet_NaN();

    const vec3<double> zero(0.0, 0.0, 0.0);
    const plane<double> GridXZeroPlane(GridX, zero);
    const plane<double> GridYZeroPlane(GridY, zero);
    const plane<double> GridZZeroPlane(GridZ, zero);

    for(const auto &cc_ref : ccsl){
        for(const auto &cop : cc_ref.get().contours){
            for(const auto &v : cop.points){
                //Compute the distance to each plane.
                const auto distX = GridXZeroPlane.Get_Signed_Distance_To_Point(v);
                const auto distY = GridYZeroPlane.Get_Signed_Distance_To_Point(v);
                const auto distZ = GridZZeroPlane.Get_Signed_Distance_To_Point(v);
                
                //Score the minimum and maximum distances.
                if(!std::isfinite(grid_x_min) || (distX < grid_x_min)) grid_x_min = distX;
                if(!std::isfinite(grid_x_max) || (distX > grid_x_max)) grid_x_max = distX;
                if(!std::isfinite(grid_y_min) || (distY < grid_y_min)) grid_y_min = distY;
                if(!std::isfinite(grid_y_max) || (distY > grid_y_max)) grid_y_max = distY;
                if(!std::isfinite(grid_z_min) || (distZ < grid_z_min)) grid_z_min = distZ;
                if(!std::isfinite(grid_z_max) || (distZ > grid_z_max)) grid_z_max = distZ;
            }
        }
    }

    //Add margins.
    grid_x_min -= (user_data_s->row_margin + std::sqrt(std::numeric_limits<double>::epsilon()));
    grid_x_max += (user_data_s->row_margin + std::sqrt(std::numeric_limits<double>::epsilon()));
    grid_y_min -= (user_data_s->col_margin + std::sqrt(std::numeric_limits<double>::epsilon()));
    grid_y_max += (user_data_s->col_margin + std::sqrt(std::numeric_limits<double>::epsilon()));
    grid_z_min -= (user_data_s->ort_margin + std::sqrt(std::numeric_limits<double>::epsilon()));
    grid_z_max += (user_data_s->ort_margin + std::sqrt(std::numeric_limits<double>::epsilon()));

    //Create planes that bound the volume to crop.
    const auto row_min_plane = plane<double>(GridX, zero + (GridX * grid_x_min));
    const auto row_max_plane = plane<double>(GridX, zero + (GridX * grid_x_max));
    const auto col_min_plane = plane<double>(GridY, zero + (GridY * grid_y_min));
    const auto col_max_plane = plane<double>(GridY, zero + (GridY * grid_y_max));
    const auto ort_min_plane = plane<double>(GridZ, zero + (GridZ * grid_z_min));
    const auto ort_max_plane = plane<double>(GridZ, zero + (GridZ * grid_z_max));

    //Cycle over images.
    auto img_it = imagecoll.images.begin();
    while(img_it != imagecoll.images.end()){

        //Check if there enough dimensions to perform the check.
        if( (img_it->rows < 1) || (img_it->columns < 1) ){
            throw std::runtime_error("Asked to crop image with no spatial extent. Crop or keep? Cannot continue.");
        }

        //Check if all corners are within the z-planes. If any are not, the image can be trimmed.
        {
            bool SkipToNext = false;
            for(const auto &p : img_it->corners2D()){
                if( ort_min_plane.Is_Point_Above_Plane(p) == ort_max_plane.Is_Point_Above_Plane(p) ){
                    img_it = imagecoll.images.erase(img_it);
                    SkipToNext = true;
                    break;
                }
            }
            if(SkipToNext) continue;
        }

        //Scan inward, assuming row_unit and col_unit align with GridX and GridY. Stop when we first pass out of the
        // cropping planes.
        int64_t row_min = 0;
        int64_t col_min = 0;
        int64_t row_max = (img_it->rows - 1);
        int64_t col_max = (img_it->columns - 1);

        for( ; row_min <= row_max; ++row_min){
            const auto p = img_it->position(row_min, 0);
            if( row_min_plane.Is_Point_Above_Plane(p) != row_max_plane.Is_Point_Above_Plane(p) ) break;
        }
        for( ; col_min <= col_max; ++col_min){
            const auto p = img_it->position(0, col_min);
            if( col_min_plane.Is_Point_Above_Plane(p) != col_max_plane.Is_Point_Above_Plane(p)) break;
        }
        for( ; row_min <= row_max; --row_max){
            const auto p = img_it->position(row_max, 0);
            if( row_min_plane.Is_Point_Above_Plane(p) != row_max_plane.Is_Point_Above_Plane(p)) break;
        }
        for( ; col_min <= col_max; --col_max){
            const auto p = img_it->position(0, col_max);
            if( col_min_plane.Is_Point_Above_Plane(p) != col_max_plane.Is_Point_Above_Plane(p)) break;
        }

        //Back off the extrema (if possible) to ensure all voxels are within the bounds.
        row_min = ((row_min-1) >= 0) ? (row_min-1) : 0;
        col_min = ((col_min-1) >= 0) ? (col_min-1) : 0;
        row_max = ((row_max+1) <= (img_it->rows-1)) ? (row_max+1) : (img_it->rows-1);
        col_max = ((col_max+1) <= (img_it->columns-1)) ? (col_max+1) : (img_it->columns-1);

        //We now have the crop extent. Create a working image.
        planar_image<float,double> replacement;
        replacement.init_buffer( (row_max - row_min), (col_max - col_min), img_it->channels );
        replacement.init_spatial( img_it->pxl_dx, img_it->pxl_dy, img_it->pxl_dz, 
                                  img_it->anchor, ( img_it->position(row_min, col_min) - img_it->anchor ) );
        replacement.init_orientation( img_it->row_unit, img_it->col_unit );
        replacement.metadata = img_it->metadata;
                                  
        for(int64_t i = 0; i < (row_max - row_min); ++i){
            for(int64_t j = 0; j < (col_max - col_min); ++j){
                for(int64_t c = 0; c < img_it->channels; ++c){
                    replacement.reference(i, j, c) = img_it->value(row_min + i, col_min + j, c);
                }
            }
        }
        *img_it = replacement;
        ++img_it;
    }
    
    return true;
}

