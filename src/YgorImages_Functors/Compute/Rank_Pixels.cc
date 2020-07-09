//Rank_Pixels.cc.

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

#include "../../Thread_Pool.h"
#include "../Grouping/Misc_Functors.h"
#include "../ConvenienceRoutines.h"
#include "Rank_Pixels.h"
#include "YgorImages.h"
#include "YgorMath.h"
#include "YgorMisc.h"
#include "YgorStats.h"       //Needed for Stats:: namespace.

#include "YgorClustering.hpp"


bool ComputeRankPixels(planar_image_collection<float,double> &imagecoll,
                          std::list<std::reference_wrapper<planar_image_collection<float,double>>>,
                          std::list<std::reference_wrapper<contour_collection<double>>>,
                          std::any user_data ){

    //This routine ranks pixel values throughout all provided images.
    //
    // This operation is often performed prior to thresholding or for helping characterize/pre-process images for shape
    // detection workflows.
    //

    //We require a valid RankPixelsUserData struct packed into the user_data.
    RankPixelsUserData *user_data_s;
    try{
        user_data_s = std::any_cast<RankPixelsUserData *>(user_data);
    }catch(const std::exception &e){
        FUNCWARN("Unable to cast user_data to appropriate format. Cannot continue with computation");
        return false;
    }

    auto all_imgs = imagecoll.get_all_images();
    size_t N_imgs = all_imgs.size();

    // Construct the pixel ordering. 
    std::vector<double> samples;
    for(auto & img_it : all_imgs){
        FUNCINFO("Images still to be assessed: " << N_imgs);
        --N_imgs;

        for(auto row = 0; row < img_it->rows; ++row){
            for(auto col = 0; col < img_it->columns; ++col){
                for(auto chan = 0; chan < img_it->channels; ++chan){
                    const auto val = static_cast<double>(img_it->value(row, col, chan));
                    if(isininc( user_data_s->inc_lower_threshold, val, user_data_s->inc_upper_threshold)){
                        samples.emplace_back(val);
                    }
                }//Loop over channels.
            } //Loop over cols
        } //Loop over rows
    } // Loop over images.

    std::sort(std::begin(samples), std::end(samples));
    const auto N_voxels = std::size(samples);
    const auto N_voxels_f = static_cast<double>(N_voxels);

    // Update the images using the pixel ordering.
    if(N_voxels == 0){
        FUNCWARN("No voxels were selected to participate in the rank; nothing to do");

    }else{
        asio_thread_pool tp;
        std::mutex saver_printer; // Who gets to save generated contours, print to the console, and iterate the counter.
        long int completed = 0;
        const long int img_count = imagecoll.images.size();

        for(auto & img_it : all_imgs){
            std::reference_wrapper< planar_image<float, double>> img_refw( std::ref(*img_it) );

            tp.submit_task([&,img_refw]() -> void {
                //Record the min and max actual pixel values for windowing purposes.
                Stats::Running_MinMax<float> minmax_pixel;

                for(auto row = 0; row < img_refw.get().rows; ++row){
                    for(auto col = 0; col < img_refw.get().columns; ++col){
                        for(auto chan = 0; chan < img_refw.get().channels; ++chan){
                            const auto origval = static_cast<double>(img_refw.get().value(row, col, chan));
                            if(isininc( user_data_s->inc_lower_threshold, origval, user_data_s->inc_upper_threshold)){

                                auto l_it = std::lower_bound(std::begin(samples), std::end(samples), origval); // First instance of val.
                                auto u_it = std::upper_bound(l_it, std::end(samples), origval); // One after last instance of val.

                                double newval = std::numeric_limits<double>::quiet_NaN();
                                const auto l_rank = std::distance(std::begin(samples), l_it);

                                if(false){
                                }else if(user_data_s->replacement_method == RankPixelsUserData::ReplacementMethod::Rank){
                                    newval = l_rank;
                                }else if(user_data_s->replacement_method == RankPixelsUserData::ReplacementMethod::Percentile){
                                    const auto u_rank = std::distance(std::begin(samples), u_it) - 1;
                                    const auto l_ptile = 100.0 * static_cast<double>(l_rank) / (N_voxels_f - 1.0);
                                    const auto u_ptile = 100.0 * static_cast<double>(u_rank) / (N_voxels_f - 1.0);
                                    const auto ptile = 0.5 * (u_ptile + l_ptile);
                                    newval = ptile;
                                }else{
                                    throw std::invalid_argument("Replacement not understood. Cannot proceed.");
                                }

                                img_refw.get().reference(row,col,chan) = newval;
                                minmax_pixel.Digest(newval);

                            }else{
                                minmax_pixel.Digest(origval);
                            }
                        }//Loop over channels.
                    } //Loop over cols
                } //Loop over rows

                UpdateImageDescription( img_refw, "Ranked voxels" );
                UpdateImageWindowCentreWidth( img_refw, minmax_pixel );

                //Report operation progress.
                {
                    std::lock_guard<std::mutex> lock(saver_printer);
                    ++completed;
                    FUNCINFO("Completed " << completed << " of " << img_count
                          << " --> " << static_cast<int>(1000.0*(completed)/img_count)/10.0 << "% done");
                }
            }); // thread pool task closure.
                
        } // Loop over images.
    }

    return true;
}

