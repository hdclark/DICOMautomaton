//ConvolveImages.cc - A part of DICOMautomaton 2019. Written by hal clark.

#include <any>
#include <optional>
#include <functional>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <regex>
#include <stdexcept>
#include <numeric>        //Needed for std::inner_product().
#include <string>    

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../YgorImages_Functors/ConvenienceRoutines.h"
#include "../YgorImages_Functors/Grouping/Misc_Functors.h"
#include "../YgorImages_Functors/Compute/Volumetric_Neighbourhood_Sampler.h"

#include "YgorImages.h"
#include "YgorString.h"       //Needed for GetFirstRegex(...)
#include "YgorStats.h"       //Needed for Stats:: namespace.

#include "ConvolveImages.h"


OperationDoc OpArgDocConvolveImages(){
    OperationDoc out;
    out.name = "ConvolveImages";

    out.desc = 
        "This routine convolves, correlates, or pattern-matches one rectilinear image array with another in voxel number space"
        " (i.e., the DICOM coordinate system of the convolution kernel image is entirely disregarded).";
    
    out.notes.emplace_back(
        "Both provided image arrays must be rectilinear. In many instances they should both be"
        " regular, not just rectilinear, but rectilinearity is sufficient for constructing voxel-by-voxel"
        " adjacency relatively quickly, and some applications may require rectilinear kernels to be"
        " supported, so rectilinear inputs are permitted."
    );
    out.notes.emplace_back(
        "This operation can be used to apply arbitrary convolution kernels to an image array."
        " It can also be used to search for instances of one image array in another."
    );
    out.notes.emplace_back(
         "If the magnitude of the outgoing voxels will be interpretted in absolute"
         " (i.e., thresholding based on an absolute magnitude) then the kernel should be"
         " weighted so that the sum of all kernel voxel intensities is zero. This will maintain"
         " the average voxel intensity. However, for pattern matching the kernel need not"
         " be normalized (though it may make interpretting partial matches easier.)"
    );
    
    out.args.emplace_back();
    out.args.back() = IAWhitelistOpArgDoc();
    out.args.back().name = "ImageSelection";
    out.args.back().default_val = "last";


    out.args.emplace_back();
    out.args.back() = IAWhitelistOpArgDoc();
    out.args.back().name = "ReferenceImageSelection";
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
    out.args.back().name = "Channel";
    out.args.back().desc = "The channel to operate on (zero-based)."
                           " Negative values will cause all channels to be operated on.";
    out.args.back().default_val = "0";
    out.args.back().expected = true;
    out.args.back().examples = { "-1",
                                 "0",
                                 "1" };


    out.args.emplace_back();
    out.args.back().name = "Operation";
    out.args.back().desc = "Controls the way the kernel is applied and the reduction is tallied."
                           " Currently, 'convolution', 'correlation', and 'pattern-match' are supported."
                           " For convolution, the reference image is spatially inverted along row-, column-,"
                           " and image-axes. The outgoing voxel intensity is the inner (i.e., dot) product"
                           " of the paired intensities of the surrounding voxel neighbourhood (i.e., the voxel"
                           " at (-1,3,0) from the centre of the kernel is paired with the neighbouring voxel"
                           " at (-1,3,0) from the current/outgoing voxel)."
                           " For pattern-matching, the difference between the kernel and each voxel's neighbourhood"
                           " voxels is compared using a 2-norm (i.e., Euclidean) cost function."
                           " With this cost function, a perfect, pixel-for-pixel match (i.e., if the kernel"
                           " images appears exactly in the image being transformed) will"
                           " result in the outgoing voxel having zero intensity (i.e., zero cost)."
                           " For correlation, the kernel is applied as-is (just like pattern-matching), but the"
                           " inner product of the paired voxel neighbourhood intensities is reported"
                           " (just like convolution)."
                           " In all cases the kernel is (approximately) centred.";
    out.args.back().default_val = "convolution";
    out.args.back().expected = true;
    out.args.back().examples = { "convolution",
                                 "correlation",
                                 "pattern-match" };
    out.args.back().samples = OpArgSamples::Exhaustive;

    return out;
}

bool ConvolveImages(Drover &DICOM_data,
                      const OperationArgPkg& OptArgs,
                      std::map<std::string, std::string>& /*InvocationMetadata*/,
                      const std::string& /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();
    const auto ReferenceImageSelectionStr = OptArgs.getValueStr("ReferenceImageSelection").value();

    const auto NormalizedROILabelRegex = OptArgs.getValueStr("NormalizedROILabelRegex").value();
    const auto ROILabelRegex = OptArgs.getValueStr("ROILabelRegex").value();

    const auto Channel = std::stol( OptArgs.getValueStr("Channel").value() );
    const auto OperationStr = OptArgs.getValueStr("Operation").value();

    //-----------------------------------------------------------------------------------------------------------------
    const auto regex_conv = Compile_Regex("^conv?o?l?u?t?i?o?n?$");
    const auto regex_corr = Compile_Regex("^corr?e?l?a?t?i?o?n?$");
    const auto regex_mtch = Compile_Regex("^pa?t?t?e?r?n?.*ma?t?c?h?$");

    const bool op_is_conv = std::regex_match(OperationStr, regex_conv);
    const bool op_is_corr = std::regex_match(OperationStr, regex_corr);
    const bool op_is_mtch = std::regex_match(OperationStr, regex_mtch);
    //-----------------------------------------------------------------------------------------------------------------

    // Identify the contours to use.
    auto cc_all = All_CCs( DICOM_data );
    auto cc_ROIs = Whitelist( cc_all, { { "ROIName", ROILabelRegex },
                                        { "NormalizedROIName", NormalizedROILabelRegex } } );
    if(cc_ROIs.empty()){
        throw std::invalid_argument("No contours selected. Cannot continue.");
    }

    // Perform the convolution once for every kernel.
    const auto IAs_all = All_IAs( DICOM_data );
    auto RIAs = Whitelist( IAs_all, ReferenceImageSelectionStr );
    for(auto & riap_it : RIAs){
        // Construct the kernel, keeping track of the voxel order.
        //
        // Note that the kernel is applied symmetrically, meaning that the midpoint of the kernel is taken as the point
        // of application. However, since voxel coordinate space is discretized even-sized kernels (in any direction)
        // will be offset by half a single voxel's width along that dimension. For example, if the kernel is 3x3x4 the
        // kernel will be offset along the z axis by 0.5*pxl_dz.

        {
            std::list<std::reference_wrapper<planar_image<float,double>>> selected_imgs;
            for(auto &img : (*riap_it)->imagecoll.images){
                selected_imgs.push_back( std::ref(img) );
            }

            if(!Images_Form_Rectilinear_Grid(selected_imgs)){
                throw std::invalid_argument("Images do not form a rectilinear grid. Cannot continue");
            }
            //const bool is_regular_grid = Images_Form_Regular_Grid(selected_imgs);
        }

        const auto orientation_normal = Average_Contour_Normals(cc_ROIs);
        planar_image_adjacency<float,double> img_adj( {}, { { std::ref((*riap_it)->imagecoll) } }, orientation_normal );
        if(img_adj.int_to_img.empty()){
            throw std::invalid_argument("Reference image array (kernel) contained no images. Cannot continue.");
        }

        auto IAs = Whitelist( IAs_all, ImageSelectionStr );
        for(auto & iap_it : IAs){

            ComputeVolumetricNeighbourhoodSamplerUserData ud;
            ud.channel = Channel;
            ud.description = "Image Convolved";
            ud.maximum_distance = std::numeric_limits<double>::quiet_NaN();
            ud.neighbourhood = ComputeVolumetricNeighbourhoodSamplerUserData::Neighbourhood::Selection;

            // Convert the kernel into a list of voxel triplets and voxel values.
            //
            // Note: The following vectors will be kept synchronized. This will obviate the need to perform any
            // correspondence look-ups later.
            std::vector<std::array<long int, 3>> triplets;
            std::vector<float> k_values;

            const auto first_img_num = 0L;
            const auto first_img_refw = img_adj.index_to_image(first_img_num);
            const long int k_rows = first_img_refw.get().rows;
            const long int k_columns = first_img_refw.get().columns;
            const auto k_imgs = static_cast<long int>(img_adj.int_to_img.size());
            //std::map<long int, img_ptr_t> int_to_img;

            const auto d_r = k_rows / 2;   // Offsets to (approximately) centre the kernel.
            const auto d_c = k_columns / 2;
            const auto d_i = k_imgs / 2;

            for(long int r = 0; r < k_rows; ++r){
                for(long int c = 0; c < k_columns; ++c){
                    for(long int i = 0; i < k_imgs; ++i){
                        std::array<long int, 3> t = { r - d_r,
                                                      c - d_c,
                                                      i - d_i };
                        triplets.emplace_back( t );

                        const auto i_num = i + first_img_num;
                        const auto l_img_refw = img_adj.index_to_image(i_num);
                        const auto val = l_img_refw.get().value(r, c, Channel);
                        k_values.emplace_back( static_cast<float>(val) );
                    }
                }
            }
            
            // Perform any necessary post-processing.
            if(op_is_conv){
                // Spatially flip the kernel. This can be accomplished by negating since the kernel is (approximately)
                // centred.
                for(auto &t : triplets){
                    t[0] *= -1L;
                    t[1] *= -1L;
                    t[2] *= -1L;
                }

            }else if( op_is_corr
                  ||  op_is_mtch ){
                // No-op...

            }else{
                throw std::logic_error("Requested operation is not understood. Cannot continue.");
            }
            ud.voxel_triplets = triplets;

            if( op_is_conv
                  ||  op_is_corr ){
                ud.f_reduce = [=](float /*v*/, std::vector<float> &shtl, vec3<double>) -> float {
                                  // Multiply the kernel and image samples together and sum them up.
                                  const auto val = std::inner_product( std::begin(k_values), std::end(k_values),
                                                                       std::begin(shtl), static_cast<double>(0.0) );
                                  return val;
                              };

            }else if(op_is_mtch){
                ud.f_reduce = [=](float /*v*/, std::vector<float> &shtl, vec3<double>) -> float {
                                  // Compute the Euclidean distance between the kernel and image voxel intensities.
                                  float val = 0.0;
                                  for(size_t i = 0; i < shtl.size(); ++i){
                                      val += std::pow(shtl[i] - k_values[i], 2.0);
                                  }
                                  return std::sqrt(val);
                              };

            }else{
                throw std::logic_error("Requested operation is not understood. Cannot continue.");
            }

            if(!ud.voxel_triplets.empty()){
                FUNCINFO("Neighbourhood comprises " << ud.voxel_triplets.size() << " neighbours");
            }

            if(!(*iap_it)->imagecoll.Compute_Images( ComputeVolumetricNeighbourhoodSampler, 
                                                     {}, cc_ROIs, &ud )){
                throw std::runtime_error("Unable to convolve images.");
            }
        }
    }

    return true;
}
