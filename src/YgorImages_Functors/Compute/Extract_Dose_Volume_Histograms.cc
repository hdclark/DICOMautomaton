//Extract_Dose_Volume_Histograms.cc.

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

#include "../../Thread_Pool.h"
#include "../Grouping/Misc_Functors.h"
#include "../ConvenienceRoutines.h"
#include "Extract_Dose_Volume_Histograms.h"
#include "YgorImages.h"
#include "YgorMath.h"
#include "YgorMisc.h"
#include "YgorStats.h"       //Needed for Stats:: namespace.

#include "YgorClustering.hpp"


bool ComputeExtractDoseVolumeHistograms(planar_image_collection<float,double> &imagecoll,
                      std::list<std::reference_wrapper<planar_image_collection<float,double>>> /*external_imgs*/,
                      std::list<std::reference_wrapper<contour_collection<double>>> ccsl,
                      std::experimental::any user_data ){

    // This routine walks the voxels of an image collection, invoking a user-provided functor when inside one or more
    // contours.
    //
    // Note: The image collection will not be altered.
    //

    //We require a valid ComputeExtractDoseVolumeHistogramsUserData struct packed into the user_data.
    ComputeExtractDoseVolumeHistogramsUserData *user_data_s;
    try{
        user_data_s = std::experimental::any_cast<ComputeExtractDoseVolumeHistogramsUserData *>(user_data);
    }catch(const std::exception &e){
        FUNCWARN("Unable to cast user_data to appropriate format. Cannot continue with computation");
        return false;
    }

    // Override mutation options.
    user_data_s->mutation_opts.editstyle      = Mutate_Voxels_Opts::EditStyle::InPlace;
    //user_data_s->mutation_opts.inclusivity    = Mutate_Voxels_Opts::Inclusivity::Centre;
    //user_data_s->mutation_opts.contouroverlap = Mutate_Voxels_Opts::ContourOverlap::Ignore;
    user_data_s->mutation_opts.aggregate      = Mutate_Voxels_Opts::Aggregate::First;
    user_data_s->mutation_opts.adjacency      = Mutate_Voxels_Opts::Adjacency::SingleVoxel;
    user_data_s->mutation_opts.maskmod        = Mutate_Voxels_Opts::MaskMod::Noop;

    if( ccsl.empty() ){
        FUNCWARN("Missing needed contour information. Cannot continue with computation");
        return false;
    }

    // Partition the contours by ROIName. (Note: could use any metadata tag here.)
    std::list<contour_collection<double>> cc_storage; // Note: This storage must last until the end of the function!
    using ccsl_t = std::list<std::reference_wrapper<contour_collection<double>>>;

    std::map<std::string, ccsl_t> named_ccsls; // Note: Cannot use operator[] because ref_w is not DefaultContructible!
    for(auto &ccs : ccsl){
        //auto common_metadata = ccs.get().get_common_metadata({},{});
        //if(common_metadata.count("ROIName") != 1){
        //    FUNCWARN("Contour collection contains mixed ROIName tag. Cannot continue");
        //    return false;
        //}
        //const auto ROIName = common_metadata["ROIName"];
            
        for(auto & contour : ccs.get().contours){
            if(contour.points.empty()) continue;

            const auto ROIName = contour.GetMetadataValueAs<std::string>("ROIName");
            if(!ROIName){
                FUNCWARN("Missing necessary tags for reporting analysis results. Cannot continue");
                return false;
            }
            const auto key = ROIName.value();
            
            if(named_ccsls.count(key) != 1){
                cc_storage.emplace_back();
                ccsl_t shtl;
                shtl.emplace_back( std::ref( cc_storage.back() ) );
                named_ccsls.emplace( std::make_pair(key, shtl) );
            }
            named_ccsls.at(key).front().get().contours.emplace_back( contour );
        }
    }

    // ----------------------------------------

    std::map<std::string, std::pair<std::vector<float>,std::vector<float>>> named_distributions; // dose/intensity, volume.

    { // Scope for thread pool.
        asio_thread_pool tp;
        std::mutex saver_printer; // Who gets to save generated contours, print to the console, and iterate the counter.
        long int completed = 0;
        const long int img_count = imagecoll.images.size();

        for(auto &img : imagecoll.images){
            std::reference_wrapper< planar_image<float, double>> img_refw( std::ref(img) );
            tp.submit_task([&,img_refw](void) -> void {

                // Get the voxel dimensions to compute volume.
                const auto pxl_dx = img_refw.get().pxl_dx;
                const auto pxl_dy = img_refw.get().pxl_dy;
                const auto pxl_dz = img_refw.get().pxl_dz;
                const auto pxl_vol = pxl_dx * pxl_dy * pxl_dz;

                // Cycle over all the alike-named contour collections.
                for(auto & named_ccsl : named_ccsls){

                    // Create task-local storage (i.e., thread-local, but not actually std::thread_local_storage) that can later
                    // be merged.
                    std::vector<float> doses;
                    doses.reserve(100*100); // An arbitrary guess.
                    std::vector<float> volumes;
                    volumes.reserve(100*100); // An arbitrary guess.

                    auto f_bounded = [&](long int /*E_row*/, 
                                         long int /*E_col*/,
                                         long int channel,
                                         std::reference_wrapper<planar_image<float,double>> /*l_img_refw*/,
                                         float &voxel_val) {

                        // No-op if this is the wrong channel.
                        if( (user_data_s->channel >= 0) && (channel != user_data_s->channel) ){
                            return;
                        }

                        volumes.emplace_back(pxl_vol);
                        doses.emplace_back(voxel_val);
                        return;
                    };

                    Mutate_Voxels<float,double>( img_refw,
                                                 { img_refw },
                                                 named_ccsl.second, 
                                                 user_data_s->mutation_opts, 
                                                 f_bounded );

                    // If there were any voxels within the contours, merge the results.
                    if(!doses.empty()){
                        std::lock_guard<std::mutex> lock(saver_printer);

                        // Ensure the vectors are instantiated for this ROI.
                        std::reference_wrapper<std::vector<float>> dose_dist( std::ref( named_distributions[ named_ccsl.first ].first ) );
                        std::reference_wrapper<std::vector<float>> vol_dist( std::ref( named_distributions[ named_ccsl.first ].second ) );

                        dose_dist.get().insert( std::end(dose_dist.get()),
                                                std::begin(doses),
                                                std::end(doses) );

                        vol_dist.get().insert( std::end(vol_dist.get()),
                                               std::begin(volumes),
                                               std::end(volumes) );

                    }

                } // Loop over all named ccs.

                //Report operation progress.
                {
                    std::lock_guard<std::mutex> lock(saver_printer);
                    ++completed;
                    FUNCINFO("Completed " << completed << " of " << img_count
                          << " --> " << static_cast<int>(1000.0*(completed)/img_count)/10.0 << "\% done");
                }

            }); // thread pool task closure.
        } // Loop over all images.

    }
    // Wait for the thread pool to complete.

    FUNCINFO("Generated voxel distributions for " << named_distributions.size() << " distinct ROIs");

    // Generate histograms from the paired dose-volume distributions.
    {
        for(const auto &named_dists : named_distributions){
            const auto key = named_dists.first;

            if( named_dists.second.first.size() != named_dists.second.second.size() ){
                throw std::logic_error("Voxel dose is not in correspondence with voxel volume. Cannot continue.");
            }

            if(named_dists.second.first.empty()){
                FUNCWARN("Asked to compute DVH when no voxels appear to have any dose. This is physically possible, but please be sure it is what you expected");
                //Could be due to:
                // -contours being too small (much smaller than voxel size).
                // -dose and contours not aligning properly. Maybe due to incorrect offsets/rotations/coordinate system?
                // -dose/contours not being present. Maybe accidentally?
                user_data_s->dvhs[key][0.0] = std::make_pair(0.0, 0.0); //Nothing over 0Gy is delivered to any voxel (0% of volume).

            }else{
                const auto D_min = std::min( 0.0f, Stats::Min(named_dists.second.first) );
                const auto N_vox = named_dists.second.first.size();
                const auto N = static_cast<double>(N_vox);
                const auto V_total = Stats::Sum(named_dists.second.second);

                for(size_t i = 0;  ; ++i){
                    const double test_dose = D_min + (user_data_s->dDose * static_cast<double>(i));
                    double cumulative_vol = 0.0; // Volume of voxels.
                    double cumulative_cnt = 0.0; // Number of voxels.

                    for(size_t j = 0; j < N_vox; ++j){
                        const auto l_dose = named_dists.second.first[j];
                        if(l_dose >= test_dose){
                            const auto l_vol = named_dists.second.second[j];
                            cumulative_vol += l_vol;
                            cumulative_cnt += 1.0;
                        }
                    }

                    const auto dose_abs = test_dose; // No scaling.
                    const auto vol_abs = cumulative_vol;
                    const auto vol_rel = cumulative_vol / V_total;
                    user_data_s->dvhs[key][dose_abs] = std::make_pair(vol_abs, vol_rel);
                    if(cumulative_cnt < 0.5) break; // Terminate the loop if nothing will change for future iterations.
                }
            }
        }

    }

    FUNCINFO("Completed DVH generation for " << user_data_s->dvhs.size() << " ROIs");

    return true;
}

