//WarpImages.cc - A part of DICOMautomaton 2022. Written by hal clark.

#include <algorithm>
#include <optional>
#include <fstream>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <regex>
#include <set> 
#include <stdexcept>
#include <string>    
#include <utility>            //Needed for std::pair.
#include <vector>
#include <cstdint>

#include "Explicator.h"       //Needed for Explicator class.

#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorLog.h"
#include "YgorStats.h"        //Needed for Stats:: namespace.
#include "YgorString.h"       //Needed for GetFirstRegex(...)
#include "YgorMathIOOFF.h"

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../Thread_Pool.h"
#include "../Metadata.h"

#include "../YgorImages_Functors/Grouping/Misc_Functors.h"
#include "../YgorImages_Functors/Processing/Partitioned_Image_Voxel_Visitor_Mutator.h"

#include "../Alignment_Rigid.h"
#include "../Alignment_TPSRPM.h"
#include "../Alignment_Field.h"

#include "WarpImages.h"

OperationDoc OpArgDocWarpImages(){
    OperationDoc out;
    out.name = "WarpImages";

    out.tags.emplace_back("category: image processing");
    out.tags.emplace_back("category: spatial transform processing");

    out.desc = 
        "This operation applies a transform object to the specified image arrays, warping them spatially.";
        
    out.notes.emplace_back(
        "A transform object must be selected; this operation cannot create transforms."
        " Transforms can be generated via registration or by parsing user-provided functions."
    );
    out.notes.emplace_back(
        "Image metadata may become invalidated by this operation."
    );
    out.notes.emplace_back(
        "This operation can only handle individual transforms. If multiple, sequential transforms"
        " are required, this operation must be invoked multiple time. This will guarantee the"
        " ordering of the transforms."
    );
    out.notes.emplace_back(
        "This operation currently supports only affine transformations."
        " Local transformations require special handling and voxel resampling that is not yet implemented."
    );
    out.notes.emplace_back(
        "Transformations are not (generally) restricted to the coordinate frame of reference that they were"
        " derived from. This permits a single transformation to be applicable to point clouds, surface meshes,"
        " images, and contours."
    );

    out.args.emplace_back();
    out.args.back() = IAWhitelistOpArgDoc();
    out.args.back().name = "ImageSelection";
    out.args.back().default_val = "last";
    out.args.back().desc = "The image array that will be transformed or sampled. "
                           "Voxel intensities from ImageSelection will be retain, but possibly resampled. "
                         + out.args.back().desc;


    out.args.emplace_back();
    out.args.back() = IAWhitelistOpArgDoc();
    out.args.back().name = "ReferenceImageSelection";
    out.args.back().default_val = "first";
    out.args.back().desc = "The image array that will be copied and voxel values overwritten. "
                           "The ImageSelection will inherit geometry from the ReferenceImageSelection. "
                         + out.args.back().desc;


    out.args.emplace_back();
    out.args.back() = T3WhitelistOpArgDoc();
    out.args.back().name = "TransformSelection";
    out.args.back().default_val = "last";
    out.args.back().desc = "Transformations to be applied to the ImageSelection array. "
                         + out.args.back().desc;


    out.args.emplace_back();
    out.args.back() = NCWhitelistOpArgDoc();
    out.args.back().name = "NormalizedROILabelRegex";
    out.args.back().default_val = ".*";
    out.args.back().desc = "Contours on the ReferenceImageSelection images that limit resampling. "
                         + out.args.back().desc;


    out.args.emplace_back();
    out.args.back() = RCWhitelistOpArgDoc();
    out.args.back().name = "ROILabelRegex";
    out.args.back().default_val = ".*";
    out.args.back().desc = "Contours on the ReferenceImageSelection images that limit resampling. "
                         + out.args.back().desc;

    out.args.emplace_back();
    out.args.back() = CCWhitelistOpArgDoc();
    out.args.back().name = "ROISelection";
    out.args.back().default_val = "all";
    out.args.back().desc = "Contours on the ReferenceImageSelection images that limit resampling. "
                         + out.args.back().desc;


    out.args.emplace_back();
    out.args.back().name = "ContourOverlap";
    out.args.back().desc = "Controls overlapping contours are treated."
                           " The default 'ignore' treats overlapping contours as a single contour, regardless of"
                           " contour orientation. This will effectively honour only the outermost contour regardless of"
                           " orientation, but provides the most predictable and consistent results."
                           " The option 'honour_opposite_orientations' makes overlapping contours"
                           " with opposite orientation cancel. Otherwise, orientation is ignored. This is useful"
                           " for Boolean structures where contour orientation is significant for interior contours (holes)."
                           " If contours do not have consistent overlap (e.g., if contours intersect) the results"
                           " can be unpredictable and hard to interpret."
                           " The option 'overlapping_contours_cancel' ignores orientation and alternately cancerls"
                           " all overlapping contours."
                           " Again, if the contours do not have consistent overlap (e.g., if contours intersect) the results"
                           " can be unpredictable and hard to interpret.";
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
    out.args.back().desc = "The channel to use (zero-based)."
                           " Setting to -1 will use each channel separately."
                           " Note that both images sets will share this specifier.";
    out.args.back().default_val = "0";
    out.args.back().expected = true;
    out.args.back().examples = { "-1",
                                 "0",
                                 "1",
                                 "2" };
 
    return out;
}


bool WarpImages(Drover &DICOM_data,
                       const OperationArgPkg& OptArgs,
                       std::map<std::string, std::string>& /*InvocationMetadata*/,
                       const std::string& /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();
    const auto ReferenceImageSelectionStr = OptArgs.getValueStr("ReferenceImageSelection").value();

    const auto TFormSelectionStr = OptArgs.getValueStr("TransformSelection").value();

    const auto NormalizedROILabelRegex = OptArgs.getValueStr("NormalizedROILabelRegex").value();
    const auto ROILabelRegex = OptArgs.getValueStr("ROILabelRegex").value();
    const auto ROISelection = OptArgs.getValueStr("ROISelection").value();
    const auto InclusivityStr = OptArgs.getValueStr("Inclusivity").value();
    const auto ContourOverlapStr = OptArgs.getValueStr("ContourOverlap").value();

    const auto Channel = std::stol( OptArgs.getValueStr("Channel").value() );

    const float InaccessibleValue = 0.0;

    //-----------------------------------------------------------------------------------------------------------------
    const auto regex_centre = Compile_Regex("^ce?n?t?[re]?[er]?");
    const auto regex_pci = Compile_Regex("^pl?a?n?a?r?[_-]?c?o?r?n?e?r?s?[_-]?inc?l?u?s?i?v?e?$");
    const auto regex_pce = Compile_Regex("^pl?a?n?a?r?[_-]?c?o?r?n?e?r?s?[_-]?exc?l?u?s?i?v?e?$");

    const auto regex_ignore = Compile_Regex("^ig?n?o?r?e?$");
    const auto regex_honopps = Compile_Regex("^ho?n?o?u?r?_?o?p?p?o?s?i?t?e?[_-]?o?r?i?e?n?t?a?t?i?o?n?s?$");
    const auto regex_cancel = Compile_Regex("^ov?e?r?l?a?p?p?i?n?g?[_-]?c?o?n?t?o?u?r?s?[_-]?c?a?n?c?e?l?s?$");

    const bool contour_overlap_ignore  = std::regex_match(ContourOverlapStr, regex_ignore);
    const bool contour_overlap_honopps = std::regex_match(ContourOverlapStr, regex_honopps);
    const bool contour_overlap_cancel  = std::regex_match(ContourOverlapStr, regex_cancel);


    //Stuff references to all contours into a list. Remember that you can still address specific contours through
    // the original holding containers (which are not modified here).
    auto cc_all = All_CCs( DICOM_data );
    auto cc_ROIs = Whitelist( cc_all, ROILabelRegex, NormalizedROILabelRegex, ROISelection );
    if(cc_ROIs.empty()){
        throw std::invalid_argument("No contours selected. Cannot continue.");
    }

    auto IAs_all = All_IAs( DICOM_data );
    auto IAs = Whitelist( IAs_all, ImageSelectionStr );
    if(IAs.size() != 1){
        throw std::invalid_argument("Only one image array can be specified.");
    }
    YLOGINFO("Selected " << IAs.size() << " image arrays");


    auto RIAs_all = All_IAs( DICOM_data );
    auto RIAs = Whitelist( RIAs_all, ReferenceImageSelectionStr );
    if(RIAs.size() != 1){
        throw std::invalid_argument("Only one reference image collection can be specified.");
    }
    std::list<std::reference_wrapper<planar_image_collection<float, double>>> RIARL = { std::ref( (*( RIAs.front() ))->imagecoll ) };
    YLOGINFO("Selected " << RIAs.size() << " reference image arrays");


    auto T3s_all = All_T3s( DICOM_data );
    auto T3s = Whitelist( T3s_all, TFormSelectionStr );
    YLOGINFO("Selected " << T3s.size() << " transformation objects");
    if(T3s.size() != 1){
        // I can't think of a better way to handle the ordering of multiple transforms right now. Disallowing for now...
        throw std::invalid_argument("Selection of only a single transformation is currently supported. Refusing to continue.");
    }

/*
// Apply a rigid transformation to the position and orientation of an image without resampling.

                        // We have to decompose the orientation vectors from the position vectors, since the orientation
                        // vectors can only be operated on by the rotational part of the Affine transform.
                        affine_transform<double> rot_scale_only = t;
                        rot_scale_only.coeff(0,3) = 0.0;
                        rot_scale_only.coeff(1,3) = 0.0;
                        rot_scale_only.coeff(2,3) = 0.0;

                        auto new_offset = animg.offset;
                        t.apply_to(new_offset);

                        auto new_r_axis = animg.row_unit.unit() * animg.pxl_dx;
                        auto new_c_axis = animg.col_unit.unit() * animg.pxl_dy;
                        auto new_o_axis = animg.row_unit.Cross( animg.col_unit ).unit() * animg.pxl_dz;

                        rot_scale_only.apply_to(new_r_axis);
                        rot_scale_only.apply_to(new_c_axis);
                        rot_scale_only.apply_to(new_o_axis);

                        const auto new_pxl_dx = new_r_axis.length();
                        const auto new_pxl_dy = new_c_axis.length();
                        const auto new_pxl_dz = new_o_axis.length();

                        const auto new_r_unit = new_r_axis.unit();
                        const auto new_c_unit = new_c_axis.unit();
                        const auto new_o_unit = new_o_axis.unit();

                        const auto eps = std::sqrt( std::numeric_limits<double>::epsilon() );
                        if( (eps < new_r_unit.Dot(new_c_unit))
                        ||  (eps < new_c_unit.Dot(new_o_unit))
                        ||  (eps < new_o_unit.Dot(new_r_unit)) ){
                            throw std::invalid_argument("Affine transformation includes shear. This is not yet supported.");
                            // Shear will require voxel resampling. This is not currently supported.
                            // To implement, Gram-schmidt orthogonalize the new units, make another image out of them,
                            // and then sample voxels into the square image.
                        }

                        animg.init_orientation( new_r_unit, new_c_unit );
                        animg.init_spatial( new_pxl_dx,
                                            new_pxl_dy,
                                            new_pxl_dz,
                                            animg.anchor, // Anchor is tied to the underlying space, not a specific object.
                                            new_offset );
*/


    for(auto & iap_it : IAs){
        // Build an index to interpolate the ImageSelection.
        {
            std::list<std::reference_wrapper<planar_image<float,double>>> selected_imgs;
            for(auto & iap_it : IAs){
                for(auto &img : (*iap_it)->imagecoll.images){
                    if( (0 <= Channel)
                    &&  (img.channels <= Channel) ){
                        throw std::invalid_argument("Encountered image without requested channel");
                    }
                    selected_imgs.push_back( std::ref(img) );
                }
            }
            if(selected_imgs.empty()){
                throw std::invalid_argument("No valid reference images selected. Cannot continue");
            }
            if(!Images_Form_Rectilinear_Grid(selected_imgs)){
                throw std::invalid_argument("Reference images do not form a rectilinear grid. Cannot continue");
            }
        }
        if((*iap_it)->imagecoll.images.empty()){
            throw std::logic_error("Provide better image orientation sampler");
        }
        const auto row_unit = (*iap_it)->imagecoll.images.front().row_unit.unit();
        const auto col_unit = (*iap_it)->imagecoll.images.front().col_unit.unit();
        const auto img_unit = (*iap_it)->imagecoll.images.front().ortho_unit();

        planar_image_adjacency<float,double> img_adj( {}, { { std::ref((*iap_it)->imagecoll) } }, img_unit );
        if(img_adj.int_to_img.empty()){
            throw std::invalid_argument("Reference image array (kernel) contained no images. Cannot continue.");
        }

        const auto ia_cm = (*iap_it)->imagecoll.get_common_metadata({});

        for(auto & t3p_it : T3s){
            // Invert the transformation, if possible.
            Transform3 t_inv;
            //std::optional<affine_transform<double>> t_inv;

            YLOGINFO("Inverting affine transformation now");
            std::visit([&](auto && t){
                using V = std::decay_t<decltype(t)>;
                if constexpr (std::is_same_v<V, std::monostate>){
                    throw std::invalid_argument("Transformation is invalid. Unable to continue.");

                // Affine transformations.
                }else if constexpr (std::is_same_v<V, affine_transform<double>>){
                    t_inv.transform = t.invert();

                // Thin-plate spline transformations.
                }else if constexpr (std::is_same_v<V, thin_plate_spline>){
                    throw std::invalid_argument("Inverting TPS transformations is not yet supported. Unable to continue.");

                // Deformation field transformations.
                }else if constexpr (std::is_same_v<V, deformation_field>){
                    throw std::invalid_argument("Inverting a deformation field is not yet supported. Unable to continue.");

                }else{
                    static_assert(std::is_same_v<V,void>, "Transformation not understood.");
                }
                return;
            }, (*t3p_it)->transform );
            
            if(std::holds_alternative<std::monostate>( t_inv.transform )){
                throw std::runtime_error("Unable to invert transformation. Unable to continue.");
            }

            // Process the image.
            for(auto & riap_it : RIAs){

                // Prepare a common metadata for the resampled images.
                auto l_meta = coalesce_metadata_for_basic_image(ia_cm);

                auto ria_cm = (*riap_it)->imagecoll.get_common_metadata({});
                ria_cm = coalesce_metadata_for_basic_image(ria_cm);
                const auto frameUID_opt = get_as<std::string>(ria_cm, "FrameOfReferenceUID");
                if(!frameUID_opt){
                    throw std::logic_error("Expected FrameOfReferenceUID to be present");
                }
                l_meta["FrameOfReferenceUID"] = frameUID_opt.value();


                // Copy the reference image array as a geometry placeholder for the sampling.
                auto edit_ia_ptr = std::make_shared<Image_Array>( *(*riap_it) );

                // Inherit the original image metadata, but update to the new frame UID.
                for(auto& rimg : edit_ia_ptr->imagecoll.images){
                    l_meta = coalesce_metadata_for_basic_image(l_meta, meta_evolve::iterate);
                    rimg.metadata = l_meta;
                }

                PartitionedImageVoxelVisitorMutatorUserData ud;
                ud.mutation_opts.editstyle = Mutate_Voxels_Opts::EditStyle::InPlace;
                ud.mutation_opts.aggregate = Mutate_Voxels_Opts::Aggregate::First;
                ud.mutation_opts.adjacency = Mutate_Voxels_Opts::Adjacency::SingleVoxel;
                ud.mutation_opts.maskmod   = Mutate_Voxels_Opts::MaskMod::Noop;
                ud.description = "Warped";

                if( contour_overlap_ignore ){
                    ud.mutation_opts.contouroverlap = Mutate_Voxels_Opts::ContourOverlap::Ignore;
                }else if( contour_overlap_honopps ){
                    ud.mutation_opts.contouroverlap = Mutate_Voxels_Opts::ContourOverlap::HonourOppositeOrientations;
                }else if( contour_overlap_cancel ){
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

                Mutate_Voxels_Functor<float,double> f_noop;
                ud.f_bounded = f_noop;
                ud.f_unbounded = f_noop;
                ud.f_visitor = f_noop;

                ud.f_bounded = [&](int64_t row, int64_t col, int64_t chan,
                                   std::reference_wrapper<planar_image<float,double>> img_refw,
                                   std::reference_wrapper<planar_image<float,double>> /*mask_img_refw*/,
                                   float &voxel_val) {
                    if( (Channel < 0) || (Channel == chan) ){
                        // Get position of this voxel.
                        const auto ref_p = img_refw.get().position(row, col);

                        // Apply inverse transform to get corresponding position in un-warped image array.
                        auto corr_p = ref_p;
                        std::visit([&](auto && t){
                            using V = std::decay_t<decltype(t)>;
                            if constexpr (std::is_same_v<V, std::monostate>){
                                throw std::invalid_argument("Transformation is invalid. Unable to continue.");

                            // Affine transformations.
                            }else if constexpr (std::is_same_v<V, affine_transform<double>>){
                                t.apply_to(corr_p);

                            // Thin-plate spline transformations.
                            }else if constexpr (std::is_same_v<V, thin_plate_spline>){
                                throw std::invalid_argument("Inverting TPS transformations is not yet supported. Unable to continue.");

                            // Deformation field transformations.
                            }else if constexpr (std::is_same_v<V, deformation_field>){
                                throw std::invalid_argument("Inverting a deformation field is not yet supported. Unable to continue.");

                            }else{
                                static_assert(std::is_same_v<V,void>, "Transformation not understood.");
                            }
                            return;
                        }, t_inv.transform );

                        // Interpolate the un-transformed image array.
                        const auto interp_val = img_adj.trilinearly_interpolate(corr_p, chan, InaccessibleValue);

                        voxel_val = interp_val;
                    }
                };

                // Optionally fill voxels with a 'background' intensity.
                //ud.f_unbounded = ...    TODO?

                if(!edit_ia_ptr->imagecoll.Process_Images_Parallel( GroupIndividualImages,
                                                                    PartitionedImageVoxelVisitorMutator,
                                                                    {}, cc_ROIs, &ud )){
                    throw std::runtime_error("Unable to warp image array");
                }

                // Insert the image into the Drover class.
                //DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>( *img_arr ) );
                DICOM_data.image_data.emplace_back( edit_ia_ptr );

            }
        }
    }

    return true;
}
