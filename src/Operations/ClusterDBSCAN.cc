//ClusterDBSCAN.cc - A part of DICOMautomaton 2019. Written by hal clark.

#include <any>
#include <optional>
#include <functional>
#include <iterator>
#include <list>
#include <map>
#include <mutex>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>    

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../YgorImages_Functors/ConvenienceRoutines.h"
#include "../YgorImages_Functors/Grouping/Misc_Functors.h"
#include "../YgorImages_Functors/Compute/Volumetric_Neighbourhood_Sampler.h"
#include "../YgorImages_Functors/Processing/Partitioned_Image_Voxel_Visitor_Mutator.h"
#include "ClusterDBSCAN.h"
#include "YgorImages.h"
#include "YgorString.h"       //Needed for GetFirstRegex(...)
#include "YgorStats.h"       //Needed for Stats:: namespace.

#include "YgorClustering.hpp"


OperationDoc OpArgDocClusterDBSCAN(){
    OperationDoc out;
    out.name = "ClusterDBSCAN";

    out.desc = 
        "This routine performs DBSCAN clustering on an image volume."
        " The clustering is limited within ROI(s) and also within a range of voxel intensities."
        " Voxels values are overwritten with the cluster ID (if applicable) or a generic"
        " configurable background value.";
    
    out.notes.emplace_back(
        "This operation will work with single images and image volumes. Images need not be rectilinear."
    );
    

    out.args.emplace_back();
    out.args.back() = IAWhitelistOpArgDoc();
    out.args.back().name = "ImageSelection";
    out.args.back().default_val = "last";

    out.args.emplace_back();
    out.args.back() = NCWhitelistOpArgDoc();
    out.args.back().name = "NormalizedROILabelRegex";
    out.args.back().default_val = ".*";

    out.args.emplace_back();
    out.args.back() = RCWhitelistOpArgDoc();
    out.args.back().name = "ROILabelRegex";
    out.args.back().default_val = ".*";

    out.args.emplace_back();
    out.args.back().name = "ContourOverlap";
    out.args.back().desc = "Controls overlapping contours are treated."
                           " The default 'ignore' treats overlapping contours as a single contour, regardless of"
                           " contour orientation. The option 'honour_opposite_orientations' makes overlapping contours"
                           " with opposite orientation cancel. Otherwise, orientation is ignored. The latter is useful"
                           " for Boolean structures where contour orientation is significant for interior contours (holes)."
                           " The option 'overlapping_contours_cancel' ignores orientation and cancels all contour overlap.";
    out.args.back().default_val = "ignore";
    out.args.back().expected = true;
    out.args.back().examples = { "ignore", "honour_opposite_orientations", 
                                 "overlapping_contours_cancel", "honour_opps", "overlap_cancel" }; 
    out.args.back().samples = OpArgSamples::Exhaustive;

    out.args.emplace_back();
    out.args.back().name = "Inclusivity";
    out.args.back().desc = "Controls how voxels are deemed to be 'within' the interior of the selected ROI(s)."
                           " The default 'center' considers only the central-most point of each voxel."
                           " There are two corner options that correspond to a 2D projection of the voxel onto the image plane."
                           " The first, 'planar_corner_inclusive', considers a voxel interior if ANY corner is interior."
                           " The second, 'planar_corner_exclusive', considers a voxel interior if ALL (four) corners are interior.";
    out.args.back().default_val = "center";
    out.args.back().expected = true;
    out.args.back().examples = { "center", "centre", 
                                 "planar_corner_inclusive", "planar_inc",
                                 "planar_corner_exclusive", "planar_exc" };
    out.args.back().samples = OpArgSamples::Exhaustive;

    out.args.emplace_back();
    out.args.back().name = "Channel";
    out.args.back().desc = "The channel to operated on (zero-based)."
                           " Negative values will cause all channels to be operated on.";
    out.args.back().default_val = "0";
    out.args.back().expected = true;
    out.args.back().examples = { "-1",
                                 "0",
                                 "1" };

    out.args.emplace_back();
    out.args.back().name = "Lower";
    out.args.back().desc = "Lower threshold (inclusive) below which voxels will be ignored by this routine.";
    out.args.back().default_val = "-inf";
    out.args.back().expected = true;
    out.args.back().examples = { "-inf",
                                 "0.0",
                                 "1024" };

    out.args.emplace_back();
    out.args.back().name = "Upper";
    out.args.back().desc = "Upper threshold (inclusive) above which voxels will be ignored by this routine.";
    out.args.back().default_val = "inf";
    out.args.back().expected = true;
    out.args.back().examples = { "inf",
                                 "1.0",
                                 "2048" };

    out.args.emplace_back();
    out.args.back().name = "MinPoints";
    out.args.back().desc = "DBSCAN algorithm parameter representing"
                           " the minimum number of points that must appear in the vicinity for a cluster to be"
                           " recognized. Sanders, et al. (1998) recommend a default of twice the dimensionality, but"
                           " what is considered to be a reasonable value depends on the sparsity of the inputs and"
                           " geometry. For regular grids, a slightly smaller value might be more appropriate.";
    out.args.back().default_val = "5";
    out.args.back().expected = true;
    out.args.back().examples = { "4",
                                 "6",
                                 "15" };

    out.args.emplace_back();
    out.args.back().name = "MaxPoints";
    out.args.back().desc = "Reject clusters if they would contain more than this many members."
                           " This parameter can be used to reject irrelevant background clusters"
                           " or to help search for disconnected clusters. Setting this parameter"
                           " appropriately will improve both memory usage and runtime considerably.";
    out.args.back().default_val = "inf";
    out.args.back().expected = true;
    out.args.back().examples = { "10",
                                 "1000",
                                 "1E6",
                                 "inf" };

    out.args.emplace_back();
    out.args.back().name = "Eps";
    out.args.back().desc = "DBSCAN algorithm parameter representing"
                           " the threshold separation distance (in DICOM units; mm) between members of a cluster."
                           " All members in a cluster must be separated from at least MinPoints points"
                           " within a distance of Eps."
                           " There is a standard way to determine an optimal value from the data itself,"
                           " but requires generating a k-nearest-neighbours clustering first, and then"
                           " visually identifying an appropriate 'kink' in the k-distances plot."
                           " This approach is not implemented here. Alternatively, the sparsity of"
                           " the data and the specific problem domain must be used to estimate a"
                           " desirable separation Eps.";
    out.args.back().default_val = "4.0";
    out.args.back().expected = true;
    out.args.back().examples = { "1.5",
                                 "2.5",
                                 "4.0",
                                 "10.0" };

    out.args.emplace_back();
    out.args.back().name = "BackgroundValue";
    out.args.back().desc = "The voxel intensity that will be assigned to all voxels that are not members"
                           " of a cluster. Note that this value can be anything, but cluster numbers"
                           " are zero-based, so a negative background is probably desired.";
    out.args.back().default_val = "-1.0";
    out.args.back().expected = true;
    out.args.back().examples = { "-1.0",
                                 "0.0",
                                 "100.23",
                                 "nan",
                                 "-inf" };

    out.args.emplace_back();
    out.args.back().name = "Reduction";
    out.args.back().desc = "Voxels within a cluster can be marked as-is, or reduced in a variety of ways."
                           " If reduction is not used, voxels in a valid cluster will have their values replaced"
                           " with the cluster ID number. If 'median' reduction is specified, the component-wise"
                           " median is reported for each cluster; the x-, y-, and z-coordinates of all voxels"
                           " in each individual cluster will be reduced to the median coordinate.";
    out.args.back().default_val = "none";
    out.args.back().expected = true;
    out.args.back().examples = { "none",
                                 "median" };
    out.args.back().samples = OpArgSamples::Exhaustive;

    return out;
}

Drover ClusterDBSCAN(Drover DICOM_data,
                     const OperationArgPkg& OptArgs,
                     const std::map<std::string, std::string>&
                     /*InvocationMetadata*/,
                     const std::string& /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();

    const auto NormalizedROILabelRegex = OptArgs.getValueStr("NormalizedROILabelRegex").value();
    const auto ROILabelRegex = OptArgs.getValueStr("ROILabelRegex").value();

    const auto InclusivityStr = OptArgs.getValueStr("Inclusivity").value();
    const auto ContourOverlapStr = OptArgs.getValueStr("ContourOverlap").value();

    const auto Channel = std::stol( OptArgs.getValueStr("Channel").value() );

    const auto Lower = std::stod( OptArgs.getValueStr("Lower").value());
    const auto Upper = std::stod( OptArgs.getValueStr("Upper").value());

    const auto MinPoints = std::stod( OptArgs.getValueStr("MinPoints").value());
    const auto MaxPoints = std::stod( OptArgs.getValueStr("MaxPoints").value());
    const auto Eps = std::stod( OptArgs.getValueStr("Eps").value());

    const auto BackgroundValue = std::stod( OptArgs.getValueStr("BackgroundValue").value());;

    const auto ReductionStr = OptArgs.getValueStr("Reduction").value();;

    //-----------------------------------------------------------------------------------------------------------------
    const auto regex_centre = Compile_Regex("^cent.*");
    const auto regex_pci = Compile_Regex("^planar_?c?o?r?n?e?r?s?_?inc?l?u?s?i?v?e?$");
    const auto regex_pce = Compile_Regex("^planar_?c?o?r?n?e?r?s?_?exc?l?u?s?i?v?e?$");

    const auto regex_ignore = Compile_Regex("^ig?n?o?r?e?$");
    const auto regex_honopps = Compile_Regex("^ho?n?o?u?r?_?o?p?p?o?s?i?t?e?_?o?r?i?e?n?t?a?t?i?o?n?s?$");
    const auto regex_cancel = Compile_Regex("^ov?e?r?l?a?p?p?i?n?g?_?c?o?n?t?o?u?r?s?_?c?a?n?c?e?l?s?$");

    const auto regex_none = Compile_Regex("^no?n?e?$");
    const auto regex_median = Compile_Regex("^medi?a?n?$");


    //Stuff references to all contours into a list. Remember that you can still address specific contours through
    // the original holding containers (which are not modified here).
    auto cc_all = All_CCs( DICOM_data );
    auto cc_ROIs = Whitelist( cc_all, { { "ROIName", ROILabelRegex },
                                        { "NormalizedROIName", NormalizedROILabelRegex } } );
    if(cc_ROIs.empty()){
        throw std::invalid_argument("No contours selected. Cannot continue.");
    }

    // DBSCAN setup.
    constexpr size_t MaxElementsInANode = 6; // 16, 32, 128, 256, ... ?
    using RTreeParameter_t = boost::geometry::index::rstar<MaxElementsInANode>;

    using UserData_t = std::pair< planar_image<float,double>*, long int >;
    using CDat_t = ClusteringDatum<3, double, // Spatial dimensions.
                            0, double, // Attribute dimensions (not used).
                            uint64_t,  // Cluster ID type.
                            UserData_t>;
    //typedef boost::geometry::model::box<CDat_t> Box_t;
    using RTree_t = boost::geometry::index::rtree<CDat_t,RTreeParameter_t>;


    auto IAs_all = All_IAs( DICOM_data );
    auto IAs = Whitelist( IAs_all, ImageSelectionStr );
    for(auto & iap_it : IAs){

        // --------------------------------
        // Prepare for clustering.
        RTree_t rtree;
        std::mutex rtree_locker;

        PartitionedImageVoxelVisitorMutatorUserData ud;

        ud.mutation_opts.editstyle = Mutate_Voxels_Opts::EditStyle::InPlace;
        ud.mutation_opts.aggregate = Mutate_Voxels_Opts::Aggregate::First;
        ud.mutation_opts.adjacency = Mutate_Voxels_Opts::Adjacency::SingleVoxel;
        ud.mutation_opts.maskmod   = Mutate_Voxels_Opts::MaskMod::Noop;
        ud.description = "Clustered (DBSCAN)";

        if( std::regex_match(ContourOverlapStr, regex_ignore) ){
            ud.mutation_opts.contouroverlap = Mutate_Voxels_Opts::ContourOverlap::Ignore;
        }else if( std::regex_match(ContourOverlapStr, regex_honopps) ){
            ud.mutation_opts.contouroverlap = Mutate_Voxels_Opts::ContourOverlap::HonourOppositeOrientations;
        }else if( std::regex_match(ContourOverlapStr, regex_cancel) ){
            ud.mutation_opts.contouroverlap = Mutate_Voxels_Opts::ContourOverlap::ImplicitOrientations;
        }else{
            throw std::invalid_argument("ContourOverlap argument '"_s + ContourOverlapStr + "' is not valid");
        }
        if( std::regex_match(InclusivityStr, regex_centre) ){
            ud.mutation_opts.inclusivity = Mutate_Voxels_Opts::Inclusivity::Centre;
        }else if( std::regex_match(InclusivityStr, regex_pci) ){
            ud.mutation_opts.inclusivity = Mutate_Voxels_Opts::Inclusivity::Inclusive;
        }else if( std::regex_match(InclusivityStr, regex_pce) ){
            ud.mutation_opts.inclusivity = Mutate_Voxels_Opts::Inclusivity::Exclusive;
        }else{
            throw std::invalid_argument("Inclusivity argument '"_s + InclusivityStr + "' is not valid");
        }

        long int BeforeCount = 0;
        ud.f_bounded = [&](long int row, long int col, long int chan, std::reference_wrapper<planar_image<float,double>> img_refw, float &voxel_val) {
            if( (Channel < 0) || (Channel == chan) ){
                if(isininc(Lower, voxel_val, Upper)){
                //|| !std::isfinite(voxel_val) ){
                    const auto p = img_refw.get().position(row,col);
                    const auto index = img_refw.get().index(row,col,chan);

                    std::lock_guard<std::mutex> lock(rtree_locker);
                    rtree.insert(CDat_t({ p.x, p.y, p.z }, {}, 
                                        std::make_pair(std::addressof(img_refw.get()), index) ));
                    ++BeforeCount;
                }
            }

            // Assume all voxels are not part of any clusters unless otherwise determined later.
            voxel_val = static_cast<float>(BackgroundValue);
            return;
        };

        // Fill the r-tree.
        if(!(*iap_it)->imagecoll.Process_Images_Parallel( GroupIndividualImages,
                                                          PartitionedImageVoxelVisitorMutator,
                                                          {}, cc_ROIs, &ud )){
            throw std::runtime_error("Unable to identify voxels for clustering using the specified ROI(s).");
        }


        // --------------------------------
        // Cluster.
        FUNCINFO("Number of voxels being clustered: " << BeforeCount);

        DBSCAN<RTree_t,CDat_t>(rtree,Eps,MinPoints);

        // --------------------------------
        // Determine which clusters are too large.
        std::map<typename CDat_t::ClusterIDType_, long int> cluster_member_count;
        {
            constexpr auto RTreeSpatialQueryGetAll = [](const CDat_t &) -> bool { return true; };
            RTree_t::const_query_iterator it;
            it = rtree.qbegin(boost::geometry::index::satisfies( RTreeSpatialQueryGetAll ));
            for( ; it != rtree.qend(); ++it){
                if(it->CID.IsRegular()){
                    const auto cluster_id = it->CID.Raw;
                    cluster_member_count[cluster_id] += 1;
                }
            }
        }

        // --------------------------------
        // Overwrite voxel values for clustered voxels.
        if( std::regex_match(ReductionStr, regex_none) ){
            long int AfterCount = 0;
            {
                constexpr auto RTreeSpatialQueryGetAll = [](const CDat_t &) -> bool { return true; };
                RTree_t::const_query_iterator it;
                it = rtree.qbegin(boost::geometry::index::satisfies( RTreeSpatialQueryGetAll ));
                for( ; it != rtree.qend(); ++it){
                    const auto img_ptr = it->UserData.first;
                    const auto index = it->UserData.second;

                    if(it->CID.IsRegular()){
                        ++AfterCount;
                        const auto cluster_id = it->CID.Raw;
                        if(cluster_member_count[cluster_id] <= MaxPoints){
                            const auto new_val = static_cast<float>(cluster_id);
                            img_ptr->reference(index) = new_val;
                        }
                    }
                }
            }
            FUNCINFO("Number of voxels with valid cluster IDs: " << AfterCount 
                << " (" << (1.0 / 100.0) * static_cast<long int>( 10000.0 * AfterCount / BeforeCount ) << "%)");

        // Reduce the cluster members using component-wise median of the x-, y-, and z-coordinates separately.
        }else if( std::regex_match(ReductionStr, regex_median) ){

            // Segregate the data based on ClusterID.
            std::map<uint64_t, std::vector<double> > seg_x;
            std::map<uint64_t, std::vector<double> > seg_y;
            std::map<uint64_t, std::vector<double> > seg_z;
            {
                constexpr auto RTreeSpatialQueryGetAll = [](const CDat_t &) -> bool { return true; };
                RTree_t::const_query_iterator it;
                it = rtree.qbegin(boost::geometry::index::satisfies( RTreeSpatialQueryGetAll ));
                for( ; it != rtree.qend(); ++it){
                    if(it->CID.IsRegular()){
                        const auto cluster_id = it->CID.Raw;
                        if(cluster_member_count[cluster_id] <= MaxPoints){
                            const auto img_ptr = it->UserData.first;
                            const auto index = it->UserData.second;

                            const auto rcc = img_ptr->row_column_channel_from_index(index);
                            const auto row = std::get<0>(rcc);
                            const auto col = std::get<1>(rcc);

                            const auto pos = img_ptr->position(row, col);

                            seg_x[cluster_id].push_back( pos.x );
                            seg_y[cluster_id].push_back( pos.y );
                            seg_z[cluster_id].push_back( pos.z );
                        }
                    }
                }
            }

            // Perform the reduction and update the image.
            for(const auto &ap : seg_x){
                const auto cluster_id = ap.first;
                const auto new_val = static_cast<float>(cluster_id);

                const auto med_x = Stats::Median(ap.second);
                const auto med_y = Stats::Median( seg_y[cluster_id] );
                const auto med_z = Stats::Median( seg_z[cluster_id] );

                const vec3<double> med( med_x, med_y, med_z );

                // Replace the nearest voxel.
                //
                // TODO: support point clouds so this step will not be necessary.
                for(auto &img : (*iap_it)->imagecoll.images){
                    const auto index = img.index( med, Channel );
                    if(index < 0) continue;
                    img.reference(index) = new_val;
                    break;
                }
            }
        }
    }

    // Update the image window and level for display.
    for(auto & iap_it : IAs){
        for(auto & img_it : (*iap_it)->imagecoll.images){
        //for(const auto & animg : (*iap_it)->imagecoll.images){
        //    UpdateImageWindowCentreWidth( std::ref(animg) );
            UpdateImageWindowCentreWidth( img_it );
        }
    }

    return DICOM_data;
}
