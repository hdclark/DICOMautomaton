
#include <cmath>
#include <exception>
#include <functional>
#include <limits>
#include <list>
#include <stdexcept>
#include <string>

#include "../ConvenienceRoutines.h"
#include "ImagePartialDerivative.h"
#include "YgorImages.h"
#include "YgorMisc.h"
#include "YgorStats.h"       //Needed for Stats:: namespace.

template <class T> class contour_collection;

bool ImagePartialDerivative(
                    planar_image_collection<float,double>::images_list_it_t first_img_it,
                    std::list<planar_image_collection<float,double>::images_list_it_t> selected_img_its,
                    std::list<std::reference_wrapper<planar_image_collection<float,double>>>,
                    std::list<std::reference_wrapper<contour_collection<double>>>, 
                    std::experimental::any user_data){

    ImagePartialDerivativeUserData *user_data_s;
    try{
        user_data_s = std::experimental::any_cast<ImagePartialDerivativeUserData *>(user_data);
    }catch(const std::exception &e){
        FUNCWARN("Unable to cast user_data to appropriate format. Cannot continue with computation");
        return false;
    }

    //This routine computes first- and second-order partial derivatives (using centered finite differences) along the
    // row- and column-aligned axes, as well as a 'cross' second-order partial derivative. All use pixel coordinates
    // (i.e., ignoring pixel shape/extent and real-space coordinates, which can be found by an appropriate multiplicative
    // factor if desired). The 'cross' partial derivative is:
    //
    //    $\frac{\partial^{2} P(row,col)}{\partial_{row} \partial_{col}}$
    //
    // These derivatives are not directly suitable for physical calculations due to the use of pixel coordinates, but
    // are suitable for boundary visualization and edge detection.

    if(selected_img_its.size() != 1) throw std::invalid_argument("This routine operates on individual images only.");

    //Make a destination image that has the same linear dimensions as the input image.
    planar_image<float,double> working = *first_img_it;
    planar_image<float,double> nms_working;  // Additional storage for edge thinning.
    if(user_data_s->method == PartialDerivativeMethod::non_maximum_suppression){
        nms_working = *first_img_it;
    }

    //Record the min and max actual pixel values for windowing purposes.
    Stats::Running_MinMax<float> minmax_pixel;

    //Loop over the rows, columns, and channels.
    for(auto row = 0; row < working.rows; ++row){
        for(auto col = 0; col < working.columns; ++col){
            for(auto chan = 0; chan < working.channels; ++chan){

                auto newval = std::numeric_limits<float>::quiet_NaN();
                auto nms_newval = std::numeric_limits<float>::quiet_NaN();

                if(false){
                }else if(user_data_s->order == PartialDerivativeEstimator::first){
                    if(false){
                    }else if(user_data_s->method == PartialDerivativeMethod::row_aligned){
                        newval = first_img_it->row_aligned_derivative_centered_finite_difference(row, col, chan);

                    }else if(user_data_s->method == PartialDerivativeMethod::column_aligned){
                        newval = first_img_it->column_aligned_derivative_centered_finite_difference(row, col, chan);

                    }else if(user_data_s->method == PartialDerivativeMethod::magnitude){
                        const auto ra = first_img_it->row_aligned_derivative_centered_finite_difference(row, col, chan);
                        const auto ca = first_img_it->column_aligned_derivative_centered_finite_difference(row, col, chan);
                        newval = std::hypot(ra,ca);

                    }else if(user_data_s->method == PartialDerivativeMethod::orientation){
                        const auto ra = first_img_it->row_aligned_derivative_centered_finite_difference(row, col, chan);
                        const auto ca = first_img_it->column_aligned_derivative_centered_finite_difference(row, col, chan);
                        newval = std::atan2(ca,ra) + M_PI;

                    }else if(user_data_s->method == PartialDerivativeMethod::non_maximum_suppression){
                        const auto ra = first_img_it->row_aligned_derivative_centered_finite_difference(row, col, chan);
                        const auto ca = first_img_it->column_aligned_derivative_centered_finite_difference(row, col, chan);
                        newval = std::hypot(ra,ca); // magnitude.
                        nms_newval = std::atan2(ca,ra) + M_PI; // orientation.

                    }else{
                        throw std::invalid_argument("Selected method not applicable to selected order or estimator.");
                    }

                }else if(user_data_s->order == PartialDerivativeEstimator::Roberts_cross_3x3){
                    if(false){
                    }else if(user_data_s->method == PartialDerivativeMethod::prow_pcol_aligned){
                        newval = first_img_it->prow_pcol_aligned_Roberts_cross_3x3(row, col, chan);

                    }else if(user_data_s->method == PartialDerivativeMethod::nrow_pcol_aligned){
                        newval = first_img_it->nrow_pcol_aligned_Roberts_cross_3x3(row, col, chan);

                    }else if(user_data_s->method == PartialDerivativeMethod::magnitude){
                        const auto prpca = first_img_it->prow_pcol_aligned_Roberts_cross_3x3(row, col, chan);
                        const auto nrpca = first_img_it->nrow_pcol_aligned_Roberts_cross_3x3(row, col, chan);
                        newval = std::hypot(prpca,nrpca);

                    }else if(user_data_s->method == PartialDerivativeMethod::orientation){
                        const auto prpca = first_img_it->prow_pcol_aligned_Roberts_cross_3x3(row, col, chan);
                        const auto nrpca = first_img_it->nrow_pcol_aligned_Roberts_cross_3x3(row, col, chan);
                        newval = std::atan2(prpca,nrpca) + M_PI/8.0 + M_PI/2.0; // For consistency with others.
                        newval = std::fmod(newval + 2.0*M_PI, 2.0*M_PI); 

                    }else if(user_data_s->method == PartialDerivativeMethod::non_maximum_suppression){
                        const auto prpca = first_img_it->prow_pcol_aligned_Roberts_cross_3x3(row, col, chan);
                        const auto nrpca = first_img_it->nrow_pcol_aligned_Roberts_cross_3x3(row, col, chan);
                        newval = std::hypot(prpca,nrpca); // magnitude.
                        nms_newval = std::atan2(prpca,nrpca) + M_PI/8.0 + M_PI/2.0; // For consistency with others.
                        nms_newval = std::fmod(nms_newval + 2.0*M_PI, 2.0*M_PI); // orientation.

                    }else{
                        throw std::invalid_argument("Selected method not applicable to selected order or estimator.");
                    }

                }else if(user_data_s->order == PartialDerivativeEstimator::Prewitt_3x3){
                    if(false){
                    }else if(user_data_s->method == PartialDerivativeMethod::row_aligned){
                        newval = first_img_it->row_aligned_Prewitt_derivative_3x3(row, col, chan);

                    }else if(user_data_s->method == PartialDerivativeMethod::column_aligned){
                        newval = first_img_it->column_aligned_Prewitt_derivative_3x3(row, col, chan);

                    }else if(user_data_s->method == PartialDerivativeMethod::magnitude){
                        const auto ra = first_img_it->row_aligned_Prewitt_derivative_3x3(row, col, chan);
                        const auto ca = first_img_it->column_aligned_Prewitt_derivative_3x3(row, col, chan);
                        newval = std::hypot(ra,ca);

                    }else if(user_data_s->method == PartialDerivativeMethod::orientation){
                        const auto ra = first_img_it->row_aligned_Prewitt_derivative_3x3(row, col, chan);
                        const auto ca = first_img_it->column_aligned_Prewitt_derivative_3x3(row, col, chan);
                        newval = std::atan2(ca,ra) + M_PI;

                    }else if(user_data_s->method == PartialDerivativeMethod::non_maximum_suppression){
                        const auto ra = first_img_it->row_aligned_Prewitt_derivative_3x3(row, col, chan);
                        const auto ca = first_img_it->column_aligned_Prewitt_derivative_3x3(row, col, chan);
                        newval = std::hypot(ra,ca); // magnitude.
                        nms_newval = std::atan2(ca,ra) + M_PI; // orientation.

                    }else{
                        throw std::invalid_argument("Selected method not applicable to selected order or estimator.");
                    }

                }else if(user_data_s->order == PartialDerivativeEstimator::Sobel_3x3){
                    if(false){
                    }else if(user_data_s->method == PartialDerivativeMethod::row_aligned){
                        newval = first_img_it->row_aligned_Sobel_derivative_3x3(row, col, chan);

                    }else if(user_data_s->method == PartialDerivativeMethod::column_aligned){
                        newval = first_img_it->column_aligned_Sobel_derivative_3x3(row, col, chan);

                    }else if(user_data_s->method == PartialDerivativeMethod::magnitude){
                        const auto ra = first_img_it->row_aligned_Sobel_derivative_3x3(row, col, chan);
                        const auto ca = first_img_it->column_aligned_Sobel_derivative_3x3(row, col, chan);
                        newval = std::hypot(ra,ca);

                    }else if(user_data_s->method == PartialDerivativeMethod::orientation){
                        const auto ra = first_img_it->row_aligned_Sobel_derivative_3x3(row, col, chan);
                        const auto ca = first_img_it->column_aligned_Sobel_derivative_3x3(row, col, chan);
                        newval = std::atan2(ca,ra) + M_PI;

                    }else if(user_data_s->method == PartialDerivativeMethod::non_maximum_suppression){
                        const auto ra = first_img_it->row_aligned_Sobel_derivative_3x3(row, col, chan);
                        const auto ca = first_img_it->column_aligned_Sobel_derivative_3x3(row, col, chan);
                        newval = std::hypot(ra,ca); // magnitude.
                        nms_newval = std::atan2(ca,ra) + M_PI; // orientation.

                    }else{
                        throw std::invalid_argument("Selected method not applicable to selected order or estimator.");
                    }

                }else if(user_data_s->order == PartialDerivativeEstimator::Sobel_5x5){
                    if(false){
                    }else if(user_data_s->method == PartialDerivativeMethod::row_aligned){
                        newval = first_img_it->row_aligned_Sobel_derivative_5x5(row, col, chan);

                    }else if(user_data_s->method == PartialDerivativeMethod::column_aligned){
                        newval = first_img_it->column_aligned_Sobel_derivative_5x5(row, col, chan);

                    }else if(user_data_s->method == PartialDerivativeMethod::magnitude){
                        const auto ra = first_img_it->row_aligned_Sobel_derivative_5x5(row, col, chan);
                        const auto ca = first_img_it->column_aligned_Sobel_derivative_5x5(row, col, chan);
                        newval = std::hypot(ra,ca);

                    }else if(user_data_s->method == PartialDerivativeMethod::orientation){
                        const auto ra = first_img_it->row_aligned_Sobel_derivative_5x5(row, col, chan);
                        const auto ca = first_img_it->column_aligned_Sobel_derivative_5x5(row, col, chan);
                        newval = std::atan2(ca,ra) + M_PI;

                    }else if(user_data_s->method == PartialDerivativeMethod::non_maximum_suppression){
                        const auto ra = first_img_it->row_aligned_Sobel_derivative_5x5(row, col, chan);
                        const auto ca = first_img_it->column_aligned_Sobel_derivative_5x5(row, col, chan);
                        newval = std::hypot(ra,ca); // magnitude.
                        nms_newval = std::atan2(ca,ra) + M_PI; // orientation.

                    }else{
                        throw std::invalid_argument("Selected method not applicable to selected order or estimator.");
                    }

                }else if(user_data_s->order == PartialDerivativeEstimator::Scharr_3x3){
                    if(false){
                    }else if(user_data_s->method == PartialDerivativeMethod::row_aligned){
                        newval = first_img_it->row_aligned_Scharr_derivative_3x3(row, col, chan);

                    }else if(user_data_s->method == PartialDerivativeMethod::column_aligned){
                        newval = first_img_it->column_aligned_Scharr_derivative_3x3(row, col, chan);

                    }else if(user_data_s->method == PartialDerivativeMethod::magnitude){
                        const auto ra = first_img_it->row_aligned_Scharr_derivative_3x3(row, col, chan);
                        const auto ca = first_img_it->column_aligned_Scharr_derivative_3x3(row, col, chan);
                        newval = std::hypot(ra,ca);

                    }else if(user_data_s->method == PartialDerivativeMethod::orientation){
                        const auto ra = first_img_it->row_aligned_Scharr_derivative_3x3(row, col, chan);
                        const auto ca = first_img_it->column_aligned_Scharr_derivative_3x3(row, col, chan);
                        newval = std::atan2(ca,ra) + M_PI;

                    }else if(user_data_s->method == PartialDerivativeMethod::non_maximum_suppression){
                        const auto ra = first_img_it->row_aligned_Scharr_derivative_3x3(row, col, chan);
                        const auto ca = first_img_it->column_aligned_Scharr_derivative_3x3(row, col, chan);
                        newval = std::hypot(ra,ca); // magnitude.
                        nms_newval = std::atan2(ca,ra) + M_PI; // orientation.

                    }else{
                        throw std::invalid_argument("Selected method not applicable to selected order or estimator.");
                    }

                }else if(user_data_s->order == PartialDerivativeEstimator::Scharr_5x5){
                    if(false){
                    }else if(user_data_s->method == PartialDerivativeMethod::row_aligned){
                        newval = first_img_it->row_aligned_Scharr_derivative_5x5(row, col, chan);

                    }else if(user_data_s->method == PartialDerivativeMethod::column_aligned){
                        newval = first_img_it->column_aligned_Scharr_derivative_5x5(row, col, chan);

                    }else if(user_data_s->method == PartialDerivativeMethod::magnitude){
                        const auto ra = first_img_it->row_aligned_Scharr_derivative_5x5(row, col, chan);
                        const auto ca = first_img_it->column_aligned_Scharr_derivative_5x5(row, col, chan);
                        newval = std::hypot(ra,ca);

                    }else if(user_data_s->method == PartialDerivativeMethod::orientation){
                        const auto ra = first_img_it->row_aligned_Scharr_derivative_5x5(row, col, chan);
                        const auto ca = first_img_it->column_aligned_Scharr_derivative_5x5(row, col, chan);
                        newval = std::atan2(ca,ra) + M_PI;

                    }else if(user_data_s->method == PartialDerivativeMethod::non_maximum_suppression){
                        const auto ra = first_img_it->row_aligned_Scharr_derivative_5x5(row, col, chan);
                        const auto ca = first_img_it->column_aligned_Scharr_derivative_5x5(row, col, chan);
                        newval = std::hypot(ra,ca); // magnitude.
                        nms_newval = std::atan2(ca,ra) + M_PI; // orientation.

                    }else{
                        throw std::invalid_argument("Selected method not applicable to selected order or estimator.");
                    }

                }else if(user_data_s->order == PartialDerivativeEstimator::second){
                    if(false){
                    }else if(user_data_s->method == PartialDerivativeMethod::row_aligned){
                        newval = first_img_it->row_aligned_second_derivative_centered_finite_difference(row, col, chan);

                    }else if(user_data_s->method == PartialDerivativeMethod::column_aligned){
                        newval = first_img_it->column_aligned_second_derivative_centered_finite_difference(row, col, chan);

                    }else if(user_data_s->method == PartialDerivativeMethod::cross){
                        newval = first_img_it->cross_second_derivative_centered_finite_difference(row, col, chan);

                    }else if(user_data_s->method == PartialDerivativeMethod::magnitude){
                        const auto ra = first_img_it->row_aligned_second_derivative_centered_finite_difference(row, col, chan);
                        const auto ca = first_img_it->column_aligned_second_derivative_centered_finite_difference(row, col, chan);
                        newval = std::hypot(ra,ca);

                    }else if(user_data_s->method == PartialDerivativeMethod::orientation){
                        const auto ra = first_img_it->row_aligned_second_derivative_centered_finite_difference(row, col, chan);
                        const auto ca = first_img_it->column_aligned_second_derivative_centered_finite_difference(row, col, chan);
                        newval = std::atan2(ca,ra) + M_PI;

                    }else if(user_data_s->method == PartialDerivativeMethod::non_maximum_suppression){
                        const auto ra = first_img_it->row_aligned_second_derivative_centered_finite_difference(row, col, chan);
                        const auto ca = first_img_it->column_aligned_second_derivative_centered_finite_difference(row, col, chan);
                        newval = std::hypot(ra,ca); // magnitude.
                        nms_newval = std::atan2(ca,ra) + M_PI; // orientation.

                    }else{
                        throw std::invalid_argument("Selected method not applicable to selected order or estimator.");
                    }
                }else{
                    throw std::invalid_argument("Unrecognized user-provided derivative order.");
                }

                working.reference(row, col, chan) = newval;
                if(user_data_s->method == PartialDerivativeMethod::non_maximum_suppression){
                    nms_working.reference(row, col, chan) = nms_newval; // orientation.
                }
                minmax_pixel.Digest(newval);
            }//Loop over channels.
        } //Loop over cols
    } //Loop over rows


    //Thin edges if requested.
    if(user_data_s->method == PartialDerivativeMethod::non_maximum_suppression){
        for(auto row = 0; row < working.rows; ++row){
            for(auto col = 0; col < working.columns; ++col){
                for(auto chan = 0; chan < working.channels; ++chan){
                    const auto magn = working.value(row, col, chan);
                    const auto angle = nms_working.value(row, col, chan) - M_PI;
                    const auto ra = std::cos(angle); // pixel-space unit vector of gradient direction.
                    const auto ca = std::sin(angle);

                    const auto row_r = static_cast<double>(row);
                    const auto col_r = static_cast<double>(col);

                    auto row_p = row_r + ca;
                    auto row_m = row_r - ca;
                    auto col_p = col_r + ra;
                    auto col_m = col_r - ra;

                    const auto row_min = static_cast<double>(0.0);
                    const auto col_min = static_cast<double>(0.0);
                    const auto row_max = static_cast<double>(working.rows)-1.0;
                    const auto col_max = static_cast<double>(working.columns)-1.0;

                    row_p = (row_p < row_min) ? row_min : row_p;
                    row_m = (row_m < row_min) ? row_min : row_m;
                    col_p = (col_p < col_min) ? col_min : col_p;
                    col_m = (col_m < col_min) ? col_min : col_m;

                    row_p = (row_p > row_max) ? row_max : row_p;
                    row_m = (row_m > row_max) ? row_max : row_m;
                    col_p = (col_p > col_max) ? col_max : col_p;
                    col_m = (col_m > col_max) ? col_max : col_m;

                    if( !std::isfinite(row_p)
                    ||  !std::isfinite(row_m)
                    ||  !std::isfinite(col_p)
                    ||  !std::isfinite(col_m) ){
                        throw std::logic_error("Non-finite row/column numbers encountered. Verify gradient computation.");
                    }

                    const auto g_p = working.bilinearly_interpolate_in_pixel_number_space( row_p, col_p, chan );
                    const auto g_m = working.bilinearly_interpolate_in_pixel_number_space( row_m, col_m, chan );

                    // Embed the updated magnitude in the orientation image so we can continue sampling the original magnitudes.
                    if( (magn >= g_p) && (magn >= g_m) ){
                        nms_working.reference(row, col, chan) = magn;
                    }else{
                        const auto zero = static_cast<float>(0);
                        nms_working.reference(row, col, chan) = zero;
                        minmax_pixel.Digest(zero);
                    }
                }//Loop over channels.
            } //Loop over cols
        } //Loop over rows
        working = nms_working;
    }

    //Replace the old image data with the new image data.
    *first_img_it = working;

    //Update the image metadata. 
    std::string img_desc;
    if(false){
    }else if(user_data_s->order == PartialDerivativeEstimator::first){
        img_desc += "First-order partial deriv.,";

    }else if(user_data_s->order == PartialDerivativeEstimator::Roberts_cross_3x3){
        img_desc += "Roberts' 3x3 cross estimator,";

    }else if(user_data_s->order == PartialDerivativeEstimator::Prewitt_3x3){
        img_desc += "Prewitt 3x3 estimator,";

    }else if(user_data_s->order == PartialDerivativeEstimator::Sobel_3x3){
        img_desc += "Sobel 3x3 estimator,";

    }else if(user_data_s->order == PartialDerivativeEstimator::Sobel_5x5){
        img_desc += "Sobel 5x5 estimator,";

    }else if(user_data_s->order == PartialDerivativeEstimator::Scharr_3x3){
        img_desc += "Scharr 3x3 estimator,";

    }else if(user_data_s->order == PartialDerivativeEstimator::Scharr_5x5){
        img_desc += "Scharr 5x5 estimator,";

    }else if(user_data_s->order == PartialDerivativeEstimator::second){
        img_desc += "Second-order partial deriv.,";

    }else{
        throw std::invalid_argument("Unrecognized user-provided derivative order.");
    }

    if(false){
    }else if(user_data_s->method == PartialDerivativeMethod::row_aligned){
        img_desc += " row-aligned";

    }else if(user_data_s->method == PartialDerivativeMethod::column_aligned){
        img_desc += " column-aligned";

    }else if(user_data_s->method == PartialDerivativeMethod::prow_pcol_aligned){
        img_desc += " +row,+column-aligned";

    }else if(user_data_s->method == PartialDerivativeMethod::nrow_pcol_aligned){
        img_desc += " -row,+column-aligned";

    }else if(user_data_s->method == PartialDerivativeMethod::cross){
        img_desc += " cross";

    }else if(user_data_s->method == PartialDerivativeMethod::magnitude){
        img_desc += " magnitude";

    }else if(user_data_s->method == PartialDerivativeMethod::non_maximum_suppression){
        img_desc += " magnitude (thinned)";

    }else if(user_data_s->method == PartialDerivativeMethod::orientation){
        img_desc += " orientation";

    }else{
        throw std::invalid_argument("Unrecognized user-provided derivative method.");
    }
    img_desc += " (in pixel coord.s)";

    UpdateImageDescription( std::ref(*first_img_it), img_desc );
    UpdateImageWindowCentreWidth( std::ref(*first_img_it), minmax_pixel );

    return true;
}

