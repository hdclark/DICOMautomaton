
#include <exception>
#include <functional>
#include <list>
#include <map>
#include <string>

#include "../ConvenienceRoutines.h"
#include "In_Image_Plane_Bilinear_Supersample.h"
#include "YgorImages.h"
#include "YgorMath.h"
#include "YgorMisc.h"
#include "YgorLog.h"
#include "YgorStats.h"       //Needed for Stats:: namespace.
#include "YgorString.h"      //Needed for GetFirstRegex(...)

bool InImagePlaneBilinearSupersample(
                    planar_image_collection<float,double>::images_list_it_t first_img_it,
                    std::list<planar_image_collection<float,double>::images_list_it_t> selected_img_its,
                    std::list<std::reference_wrapper<planar_image_collection<float,double>>>,
                    std::list<std::reference_wrapper<contour_collection<double>>>, 
                    std::any user_data){

    //This routine supersamples images, making them have a greater number of pixels. It uses an
    // in-plane bilinear supersampling technique that is completely oblivious to the pixel dimensions.
    // Only nearest-neighbour adjacent pixels are used. "Mirror" boundaries are used.

    if(selected_img_its.size() != 1) YLOGERR("This routine operates on individual images only");
 
    //This routine requires a valid PerROITimeCoursesUserData struct packed into the user_data. Accept the throw
    // if the input is missing or invalid.
    InImagePlaneBilinearSupersampleUserData *user_data_s;
    try{
        user_data_s = std::any_cast<InImagePlaneBilinearSupersampleUserData *>(user_data);
    }catch(const std::exception &e){
        YLOGWARN("Unable to cast user_data to appropriate format. Cannot continue with computation");
        return false;
    }

    const auto RowScaleFactor    = user_data_s->RowScaleFactor;
    const auto ColumnScaleFactor = user_data_s->ColumnScaleFactor;

    const auto RowScaleFactorR = static_cast<double>(RowScaleFactor);
    const auto ColumnScaleFactorR = static_cast<double>(ColumnScaleFactor);

    const auto new_pxl_dx = first_img_it->pxl_dx / ColumnScaleFactorR;
    const auto new_pxl_dy = first_img_it->pxl_dy / RowScaleFactorR;

    const auto new_rows = first_img_it->rows * RowScaleFactor;
    const auto new_columns = first_img_it->columns * ColumnScaleFactor;

    vec3<double> newOffset = first_img_it->offset;
    newOffset -= first_img_it->row_unit * first_img_it->pxl_dx * 0.5;
    newOffset -= first_img_it->col_unit * first_img_it->pxl_dy * 0.5;
    newOffset += first_img_it->row_unit * new_pxl_dx * 0.5;
    newOffset += first_img_it->col_unit * new_pxl_dy * 0.5;

    //Make a destination image that has twice the linear dimensions as the input image.
    planar_image<float,double> working;
    working.init_buffer( new_rows,
                         new_columns,
                         first_img_it->channels );
    working.init_spatial( new_pxl_dx,
                          new_pxl_dy,
                          first_img_it->pxl_dz,
                          first_img_it->anchor,
                          newOffset );
    working.init_orientation( first_img_it->row_unit,
                              first_img_it->col_unit );
    working.metadata = first_img_it->metadata;

    working.metadata["Rows"] = std::to_string(new_rows);
    working.metadata["Columns"] = std::to_string(new_columns);

    working.metadata["PixelSpacing"] = std::to_string(new_pxl_dy) 
                                       + R"***(\)***"_s 
                                       + std::to_string(new_pxl_dx);

    //Paint all pixels black.
    //working.fill_pixels(static_cast<float>(0));

    //Record the min and max actual pixel values for windowing purposes.
    Stats::Running_MinMax<float> minmax_pixel;

    //Loop over the rows, columns, and channels.
    for(auto row = 0; row < working.rows; ++row){
        for(auto col = 0; col < working.columns; ++col){
            for(auto chan = 0; chan < working.channels; ++chan){
                const double row_sample_pos = (2.0*row + 1.0 - RowScaleFactorR)/(2.0*RowScaleFactorR);
                const double col_sample_pos = (2.0*col + 1.0 - ColumnScaleFactorR)/(2.0*ColumnScaleFactorR);
                const auto newval = first_img_it->bilinearly_interpolate_in_pixel_number_space(row_sample_pos, col_sample_pos, chan);

                working.reference(row, col, chan) = newval;
                minmax_pixel.Digest(newval);
            }//Loop over channels.
        } //Loop over cols
    } //Loop over rows

    //Replace the old image data with the new image data.
    *first_img_it = working;

    UpdateImageDescription( std::ref(*first_img_it), "In-plane Bilinearly Supersampled "_s 
                                                      + std::to_string(RowScaleFactor) + "x,"_s
                                                      + std::to_string(ColumnScaleFactor) + "x ");
    UpdateImageWindowCentreWidth( std::ref(*first_img_it), minmax_pixel );

    return true;
}

