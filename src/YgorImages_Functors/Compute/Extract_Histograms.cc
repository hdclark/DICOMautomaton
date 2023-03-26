//Extract_Histograms.cc.

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
#include "../../Metadata.h"
#include "../Grouping/Misc_Functors.h"
#include "../ConvenienceRoutines.h"
#include "Extract_Histograms.h"
#include "YgorImages.h"
#include "YgorMath.h"
#include "YgorMisc.h"
#include "YgorLog.h"
#include "YgorStats.h"       //Needed for Stats:: namespace.

#include "YgorClustering.hpp"


bool ComputeExtractHistograms(planar_image_collection<float,double> &imagecoll,
                      std::list<std::reference_wrapper<planar_image_collection<float,double>>> /*external_imgs*/,
                      std::list<std::reference_wrapper<contour_collection<double>>> ccsl,
                      std::any user_data ){

    // This routine extracts histograms from the bounded voxels of an image array. It can be used to generate
    // dose-volume histograms (DVHs) in differential form. These can be post-processed to generate cumulative DVHs and
    // axes-normalized variants of either differential or cumulative DVHs.
    //
    // Note: Non-finite voxels are excluded from analysis and do not contribute to the volume. If absolute volume is
    //       required, ensure all voxels are finite prior to invoking this routine.
    //
    // Note: This routine will consume a lot of memory if the resolution is too fine.
    //
    // Note: The image collection and contour collections will not be altered.
    //

    //We require a valid ComputeExtractHistogramsUserData struct packed into the user_data.
    ComputeExtractHistogramsUserData *user_data_s;
    try{
        user_data_s = std::any_cast<ComputeExtractHistogramsUserData *>(user_data);
    }catch(const std::exception &e){
        YLOGWARN("Unable to cast user_data to appropriate format. Cannot continue with computation");
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
        YLOGWARN("Missing needed contour information. Cannot continue with computation");
        return false;
    }

    // Logically partition the contours.
    //
    // Note: At the moment we exclusively use ROIName, but we *could* use any metadata tag here.
    //       I'm not sure how to provide a sane interface, and I'm not sure if it is actually needed, since an exterior
    //       grouping routine could accomplish the same. Nevertheless, it might be reasonable to expose a tailored
    //       metadata extractor here.
    std::list<contour_collection<double>> cc_storage;
    using ccsl_t = std::list<std::reference_wrapper<contour_collection<double>>>;

    std::map<std::string, ccsl_t> named_ccsls; // Note: Cannot use operator[] because ref_w is not DefaultContructible.
    for(auto &ccs : ccsl){
        for(auto & contour : ccs.get().contours){
            if(contour.points.empty()) continue;

            std::string key;
            if(user_data_s->grouping == ComputeExtractHistogramsUserData::GroupingMethod::Separate){
                const auto ROIName = contour.GetMetadataValueAs<std::string>("ROIName");
                if(!ROIName){
                    YLOGWARN("Found contour missing ROIName metadata element. Using placeholder name");
                }
                key = ROIName.value_or("unspecified");
            }else if(user_data_s->grouping == ComputeExtractHistogramsUserData::GroupingMethod::Combined){
                key = "";
            }else{
                throw std::invalid_argument("Encountered unanticipated grouping option. Cannot continue.");
            }
            
            if(named_ccsls.count(key) != 1){
                cc_storage.emplace_back();
                ccsl_t shtl;
                shtl.emplace_back( std::ref( cc_storage.back() ) );
                named_ccsls.emplace( std::make_pair(key, shtl) );
            }
            named_ccsls.at(key).front().get().contours.emplace_back( contour );
        }
    }

    // Determine voxel value extrema for each logical partition.
    std::map<std::string,               // ROIName.
             std::pair<double,          // Minimum voxel value (within the user's inclusive range).
                       double>>         // Maximum voxel value (within the user's inclusive range).
                       voxel_extrema;

    { // Scope for thread pool.
        work_queue<std::function<void(void)>> wq;
        std::mutex saver;
        std::mutex printer;
        long int completed = 0;
        const long int img_count = imagecoll.images.size();

        for(auto &img : imagecoll.images){
            std::reference_wrapper< planar_image<float, double>> img_refw( std::ref(img) );
            wq.submit_task([&,img_refw]() -> void {

                // Cycle over all the alike-named contour collections.
                for(auto & named_ccsl : named_ccsls){
                    double local_minimum = std::numeric_limits<double>::infinity();
                    double local_maximum = -local_minimum;

                    auto f_bounded = [&](long int /*E_row*/, 
                                         long int /*E_col*/,
                                         long int channel,
                                         std::reference_wrapper<planar_image<float,double>> /*l_img_refw*/,
                                         std::reference_wrapper<planar_image<float,double>> /*mask_img_refw*/,
                                         float &voxel_val) {
                        if( ( (user_data_s->channel < 0) || (user_data_s->channel == channel))
                        &&  std::isfinite(voxel_val)  // Ignore infinite and NaN voxels.
                        &&  (user_data_s->lower_threshold <= voxel_val)
                        &&  (voxel_val <= user_data_s->upper_threshold) ){
                            if(voxel_val < local_minimum) local_minimum = voxel_val;
                            if(local_maximum < voxel_val) local_maximum = voxel_val;
                        }
                        return;
                    };

                    Mutate_Voxels<float,double>( img_refw,
                                                 { img_refw },
                                                 named_ccsl.second, 
                                                 user_data_s->mutation_opts, 
                                                 f_bounded );

                    // Merge the results.
                    if( std::isfinite(local_minimum) 
                    &&  std::isfinite(local_maximum) ){
                        std::lock_guard<std::mutex> lock(saver);

                        const auto key = named_ccsl.first;
                        if(voxel_extrema.count(key) == 0){
                            voxel_extrema[key] = std::make_pair(local_minimum, local_maximum);
                        }else{
                            if(local_minimum < voxel_extrema[key].first)  voxel_extrema[key].first  = local_minimum;
                            if(voxel_extrema[key].second < local_maximum) voxel_extrema[key].second = local_maximum;
                        }
                    }
                } // Loop over all named ccs.

                //Report operation progress.
                {
                    std::lock_guard<std::mutex> lock(printer);
                    ++completed;
                    YLOGINFO("Completed " << completed << " of " << img_count
                          << " --> " << static_cast<int>(1000.0*(completed)/img_count)/10.0 << "% done");
                }

            }); // thread pool task closure.
        } // Loop over all images.
    }


    // Allocate storage for each logical partition.
    std::map<std::string, size_t> bin_counts;
    std::map<std::string, double> bin_widths;
    std::map<std::string,                        // ROIName.
             std::vector<std::array<double,4>>>  // Differential histogram with implicit bin numbering (min to max).
                 raw_diff_histograms;
    std::set<std::string> to_purge; // Groups to purge because they are invalid.
    for(const auto &extrema : voxel_extrema){
        const auto key = extrema.first;

        const auto voxel_min = extrema.second.first;
        const auto voxel_max = extrema.second.second;
        const auto range = (voxel_max - voxel_min);
        const auto count_f = range / user_data_s->dDose;
        if( !std::isfinite(count_f)
        ||  (1.0E9 < count_f) // approx 10 GB just for the bin values, actual usage would be higher.
        ||  (count_f <= 1.0) ){
            YLOGWARN("Excessive or invalid number of bins required for key '" << key << "'. Skipping it");
            to_purge.insert(key);
            continue;
        }
        const auto bin_count = static_cast<size_t>(std::ceil( count_f ));
        const auto bin_width = range / static_cast<double>(bin_count);

        // Prepare the histograms bins.
        //
        // Note: bin values are centred.
        std::vector<std::array<double,4>> hist(bin_count, {0.0, 0.0, 0.0, 0.0} );
        for(size_t i = 0; i < bin_count; ++i){
            hist[i][0] = voxel_min 
                       + (bin_width * static_cast<double>(i)) 
                       + (bin_width * 0.5);
        }

        bin_counts.emplace(key, bin_count);
        bin_widths.emplace(key, bin_width);
        raw_diff_histograms.emplace(key, hist);
    }
    for(const auto &key : to_purge) voxel_extrema.extract(key);


    // Visit all voxels to build the histograms.
    {
        work_queue<std::function<void(void)>> wq;
        std::mutex saver;
        std::mutex printer;
        long int completed = 0;
        const long int img_count = imagecoll.images.size();

        for(auto &img : imagecoll.images){
            std::reference_wrapper< planar_image<float, double>> img_refw( std::ref(img) );
            wq.submit_task([&,img_refw]() -> void {

                const auto pxl_dx = img_refw.get().pxl_dx;
                const auto pxl_dy = img_refw.get().pxl_dy;
                const auto pxl_dz = img_refw.get().pxl_dz;
                const auto pxl_vol = pxl_dx * pxl_dy * pxl_dz;

                std::vector<size_t> shuttle;  // Thread-specific storage buffer.
                const size_t N_shuttle = 1000; // Size of buffer.
                shuttle.reserve(N_shuttle);

                for(auto & named_ccsl : named_ccsls){
                    const auto key = named_ccsl.first;
                    if(bin_counts.count(key) != 1) continue; // Group did not enclose any voxels.

                    const auto bin_count = bin_counts.at(key);
                    const auto bin_width = bin_widths.at(key);
                    const auto voxel_min = voxel_extrema.at(key).first;
                    //const auto voxel_max = voxel_extrema.at(key).second;
                    std::vector<std::array<double,4>> &raw_diff_hist(raw_diff_histograms.at(key));

                    // Sub-routine to add all counts from the buffer and then reset the buffer.
                    const auto add_counts = [&]() -> void {
                        std::lock_guard<std::mutex> lock(saver);
                        for(const auto &bin_N : shuttle) raw_diff_hist[bin_N][2] += pxl_vol;
                        shuttle.clear();
                        shuttle.reserve(N_shuttle);
                        return;
                    };

                    auto f_bounded = [&](long int /*E_row*/, 
                                         long int /*E_col*/,
                                         long int channel,
                                         std::reference_wrapper<planar_image<float,double>> /*l_img_refw*/,
                                         std::reference_wrapper<planar_image<float,double>> /*mask_img_refw*/,
                                         float &voxel_val){

                        if( ( (user_data_s->channel < 0) || (user_data_s->channel == channel))
                        &&  std::isfinite(voxel_val)  // Ignore infinite and NaN voxels.
                        &&  (user_data_s->lower_threshold <= voxel_val)
                        &&  (voxel_val <= user_data_s->upper_threshold) ){

                            const auto d_low = voxel_val - voxel_min;
                            const auto bin_N = std::clamp( static_cast<size_t>( std::floor(d_low/bin_width) ),
                                                           static_cast<size_t>(0),
                                                           static_cast<size_t>(bin_count-1) );
                            //raw_diff_hist.at(bin_N)[2] += pxl_vol;
                            shuttle.push_back(bin_N);
                            if(N_shuttle == shuttle.size()) add_counts();
                        }
                        return;
                    };

                    Mutate_Voxels<float,double>( img_refw,
                                                 { img_refw },
                                                 named_ccsl.second, 
                                                 user_data_s->mutation_opts, 
                                                 f_bounded );

                    add_counts(); // Commit all remaining bins from the shuttle.
                } // Loop over all named ccs.

                //Report operation progress.
                {
                    std::lock_guard<std::mutex> lock(printer);
                    ++completed;
                    YLOGINFO("Completed " << completed << " of " << img_count
                          << " --> " << static_cast<int>(1000.0*(completed)/img_count)/10.0 << "% done");
                }

            }); // thread pool task closure.
        } // Loop over all images.
    }

    // Prepare differential histograms.
    auto cm = imagecoll.get_common_metadata({});
    for(auto & named_ccsl : named_ccsls){
        const auto key = named_ccsl.first;

        if( (bin_counts.count(key) != 1)
        ||  (bin_counts[key] < 2) ){
            YLOGWARN("Computed histogram with few enclosed voxels, or excessively coarse resolution. Skipping");
            continue;
            //Could be due to:
            // -contours being too small (much smaller than voxel size).
            // -dose and contours not aligning properly. Maybe due to incorrect offsets/rotations/coordinate system?
            // -dose/contours not being present. Maybe accidentally?
            // -dDose being too large.
        }

        const auto RescaleType = get_as<std::string>(cm, "RescaleType"); // For CT (should be HU).
        const auto DoseUnits = get_as<std::string>(cm, "DoseUnits"); // For RTDOSE (should be GY).
        const auto AbscissaStr = RescaleType ? RescaleType.value() : DoseUnits.value_or("unknown");

        user_data_s->differential_histograms[key].samples.swap( raw_diff_histograms.at(key) );

        user_data_s->differential_histograms[key].metadata["Modality"]        = "Histogram"; 
        user_data_s->differential_histograms[key].metadata["HistogramType"]   = "Differential";
        user_data_s->differential_histograms[key].metadata["AbscissaScaling"] = "None"; // Absolute values in DICOM units, Gy.
        user_data_s->differential_histograms[key].metadata["OrdinateScaling"] = "None"; // Absolute values in DICOM units, mm^3.
        user_data_s->differential_histograms[key].metadata["Ordinate"]        = "Volume (mm^3)";
        user_data_s->differential_histograms[key].metadata["Abscissa"]        = AbscissaStr;

        const auto voxel_min = voxel_extrema.at(key).first;
        const auto voxel_max = voxel_extrema.at(key).second;
        //const auto range = (voxel_max - voxel_min);
        //const auto bin_count = bin_counts.at(key);
        //const auto bin_width = bin_widths.at(key);

        Stats::Running_Sum<double> x_v;
        Stats::Running_Sum<double> v;
        std::vector<std::array<double,4>> &hist_samples( user_data_s->differential_histograms[key].samples );
        for(const auto &s : hist_samples){
            x_v.Digest(s[0]*s[2]);
            v.Digest(s[2]);
        }
        const auto voxel_mean = x_v.Current_Sum() / v.Current_Sum();
        if(!std::isfinite(voxel_mean)){
            throw std::runtime_error("Unable to estimate distribution mean. Refusing to continue.");
        }

        user_data_s->differential_histograms[key].metadata["DistributionMin"]  = std::to_string(voxel_min);
        user_data_s->differential_histograms[key].metadata["DistributionMean"] = std::to_string(voxel_mean);
        user_data_s->differential_histograms[key].metadata["DistributionMax"]  = std::to_string(voxel_max);

        const auto x_eps = std::numeric_limits<double>::infinity();  // Ignore the abscissa.
        //const auto y_eps = 0.0001;
        const auto y_eps = std::sqrt( 10.0 * std::numeric_limits<double>::epsilon() );
        {
            auto purged = user_data_s->differential_histograms[key].Purge_Redundant_Samples(x_eps, y_eps);
            user_data_s->differential_histograms[key].samples.swap(purged.samples);
        }
    }

    // Prepare cumulative histograms.
    //
    // Note: This step could be made optional to reduce memory usage. Also, the normalization could be adjusted.
    for(auto & hist : user_data_s->differential_histograms){
        const auto key = hist.first;

        user_data_s->cumulative_histograms[key] = hist.second;
        user_data_s->cumulative_histograms[key].metadata["HistogramType"] = "Cumulative";

        const auto bin_width = bin_widths.at(key);
        const auto extra_bin_x = user_data_s->cumulative_histograms[key].samples.back()[0] + bin_width;
        user_data_s->cumulative_histograms[key].push_back(extra_bin_x, 0.0);

        std::vector<std::array<double,4>> &hist_samples( user_data_s->cumulative_histograms[key].samples );
        const auto N_bins = hist_samples.size();

        Stats::Running_Sum<double> rs;
        for(size_t i = 0; i < N_bins; ++i){
            const auto j = (N_bins - 1) - i;
            rs.Digest(hist_samples[j][2]);
            hist_samples[j][2] = rs.Current_Sum();
        }
        //const auto final_sum = rs.Current_Sum();

        //if(false){
        //}else if(user_data_s->cumulative_type == ComputeExtractHistogramsUserData::CumulativeType::OrdinateScaled){
        //    user_data_s->cumulative_histograms[key].metadata["OrdinateScaling"] = "Normalized"; // Fractional, spanning [0:1].
        //    for(auto & s : hist_samples){
        //        s[2] /= final_sum;
        //    }
        //}else{
        //    throw std::invalid_argument("Encountered cumulative histogram scaling option. Cannot continue.");
        //}
    }

    YLOGINFO("Generated " << user_data_s->differential_histograms.size() << " histograms");

    return true;
}

