
#include <list>
#include <functional>
#include <limits>
#include <map>
#include <cmath>
#include <algorithm>
#include <string>

#include "YgorMisc.h"
#include "YgorMath.h"
#include "YgorImages.h"
#include "YgorStats.h"       //Needed for Stats:: namespace.
#include "YgorAlgorithms.h"  //Needed for For_Each_In_Parallel<..>(...)
#include "YgorString.h"      //Needed for GetFirstRegex(...)
#include "YgorPlot.h"

#include "../ConvenienceRoutines.h"

#include "In_Image_Plane_Bilinear_Supersample.h"

bool InImagePlaneBilinearSupersample(
                    planar_image_collection<float,double>::images_list_it_t first_img_it,
                    std::list<planar_image_collection<float,double>::images_list_it_t> selected_img_its,
                    std::list<std::reference_wrapper<planar_image_collection<float,double>>>,
                    std::list<std::reference_wrapper<contour_collection<double>>>, 
                    std::experimental::any user_data){

    //This routine supersamples images, making them have a greater number of pixels. It uses an
    // in-plane bilinear supersampling technique that is completely oblivious to the pixel dimensions.
    // Only nearest-neighbour adjacent pixels are used. "Mirror" boundaries are used.

    if(selected_img_its.size() != 1) FUNCERR("This routine operates on individual images only");
 
    //This routine requires a valid PerROITimeCoursesUserData struct packed into the user_data. Accept the throw
    // if the input is missing or invalid.
    InImagePlaneBilinearSupersampleUserData *user_data_s;
    try{
        user_data_s = std::experimental::any_cast<InImagePlaneBilinearSupersampleUserData *>(user_data);
    }catch(const std::exception &e){
        FUNCWARN("Unable to cast user_data to appropriate format. Cannot continue with computation");
        return false;
    }

    const auto RowScaleFactor    = user_data_s->RowScaleFactor;
    const auto ColumnScaleFactor = user_data_s->ColumnScaleFactor;

    const auto RowScaleFactorR = static_cast<double>(RowScaleFactor);
    const auto ColumnScaleFactorR = static_cast<double>(ColumnScaleFactor);

    vec3<double> newOffset = first_img_it->offset;
    newOffset -= first_img_it->row_unit * first_img_it->pxl_dx * 0.5;
    newOffset -= first_img_it->col_unit * first_img_it->pxl_dy * 0.5;
    newOffset += first_img_it->row_unit * first_img_it->pxl_dx * 0.5 / RowScaleFactorR;
    newOffset += first_img_it->col_unit * first_img_it->pxl_dy * 0.5 / ColumnScaleFactorR;


    //Make a destination image that has twice the linear dimensions as the input image.
    planar_image<float,double> working;
    working.init_buffer( first_img_it->rows * RowScaleFactor, 
                         first_img_it->columns * ColumnScaleFactor, 
                         first_img_it->channels );
    working.init_spatial( first_img_it->pxl_dx / RowScaleFactorR,
                          first_img_it->pxl_dy / ColumnScaleFactorR,
                          first_img_it->pxl_dz,
                          first_img_it->anchor,
                          newOffset );
    working.init_orientation( first_img_it->row_unit,
                              first_img_it->col_unit );
    working.metadata = first_img_it->metadata;

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

    first_img_it->metadata["Rows"] = std::to_string(first_img_it->rows);
    first_img_it->metadata["Columns"] = std::to_string(first_img_it->columns);

    return true;
}

