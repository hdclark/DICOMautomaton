//ReduceNeighbourhood.cc - A part of DICOMautomaton 2019. Written by hal clark.

#include <any>
#include <optional>
#include <functional>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>    
#include <cstdint>

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../YgorImages_Functors/ConvenienceRoutines.h"
#include "../YgorImages_Functors/Grouping/Misc_Functors.h"
#include "../YgorImages_Functors/Compute/Volumetric_Neighbourhood_Sampler.h"
#include "ReduceNeighbourhood.h"
#include "YgorImages.h"
#include "YgorString.h"       //Needed for GetFirstRegex(...)
#include "YgorStats.h"       //Needed for Stats:: namespace.


OperationDoc OpArgDocReduceNeighbourhood(){
    OperationDoc out;
    out.name = "ReduceNeighbourhood";

    out.desc = 
        "This routine walks the voxels of a 3D rectilinear image collection, "
        " reducing the distribution of voxels within the local volumetric neighbourhood to a scalar value,"
        " and updating the voxel value with this scalar. This routine can be used to implement mean and"
        " median filters (amongst others) that operate over a variety of 3D neighbourhoods."
        " Besides purely statistical reductions, logical reductions can be applied.";
    
    out.notes.emplace_back(
        "The provided image collection must be rectilinear."
    );
    out.notes.emplace_back(
        "This operation can be used to compute core 3D morphology operations (erosion and dilation)"
        " as well as composite operations like opening (i.e., erosion followed by dilation),"
        " closing (i.e., dilation followed by erosion), 'gradient' (i.e., the difference between"
        " dilation and erosion, which produces an outline), and various other combinations of core"
        " and composite operations."
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
    out.args.back() = CCWhitelistOpArgDoc();
    out.args.back().name = "ROISelection";
    out.args.back().default_val = "all";


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
    out.args.back().name = "Neighbourhood";
    out.args.back().desc = "Controls how the neighbourhood surrounding a voxel is defined."
                           "\n\n"
                           " Variable-size neighbourhoods 'spherical' and 'cubic' are defined."
                           " An appropriate isotropic extent must be provided for these neighbourhoods."
                           " (See below; extents must be provided in DICOM units, i.e., mm.)"
                           "\n\n"
                           " Fixed-size neighbourhoods specify a fixed number of adjacent voxels."
                           "\n\n"
                           " Fixed rectagular neighbourhoods are specified like 'RxCxI' for"
                           " row, column, and image slice extents (as integer number of rows, columns,"
                           " and slices)."
                           "\n\n"
                           " Fixed spherical neighbourhoods are specified like 'Wsphere' where W"
                           " is the width (i.e., the number of voxels wide)."
                           " In morphological terminology, the neighbourhood is referred to as a"
                           " 'structuring element.' A similar concept is the convolutional 'kernel.'";
    out.args.back().default_val = "spherical";
    out.args.back().expected = true;
    out.args.back().examples = { "spherical",
                                 "cubic",
                                 "3x3x3",
                                 "5x5x5",
                                 "3sphere",
                                 "5sphere",
                                 "7sphere",
                                 "9sphere",
                                 "11sphere",
                                 "13sphere",
                                 "15sphere" };
    out.args.back().samples = OpArgSamples::Exhaustive;


    out.args.emplace_back();
    out.args.back().name = "Reduction";
    out.args.back().desc = "Controls how the distribution of voxel values from neighbouring voxels is reduced."
                           "\n\n"
                           "Statistical distribution reducers 'min', 'mean', 'median', and 'max' are defined."
                           " 'min' is also known as the 'erosion' operation. Likewise, 'max' is also known as"
                           " the 'dilation' operation."
                           " Note that the morphological 'opening' operation can be accomplished by sequentially"
                           " performing an erosion and then a dilation using the same neighbourhood."
                           " The 'standardize' reduction method can be used for adaptive rescaling by"
                           " subtracting the local neighbourhood mean and dividing the local neighbourhood"
                           " standard deviation."
                           "\n\n"
                           "The 'geometric_mean' implements the Nth root of the product of N intensities within"
                           " the neighbourhood. It is a smoothing filter often used to remove Gaussian noise."
                           " Note that all pixels should be non-negative, otherwise the geometric mean is"
                           " not well-defined. Otherwise NaN is returned."
                           "\n\n"
                           "The 'standardize' reduction method is a way to (locally) transform variables on"
                           " different scales so they can more easily be compared. Note that standardization can"
                           " result in undefined voxel values when the local neighbourhood is perfectly uniform."
                           " Also, since only the local neighbourhood is considered, voxels will in general have"
                           " *neither* zero mean *nor* a  unit standard deviation (growing the neighbourhood"
                           " extremely large *will* accomplish this, but the calculation will be inefficient)."
                           "\n\n"
                           "The 'percentile01' reduction method evaluates which percentile the central voxel"
                           " occupies within the local neighbourhood."
                           " It is reported scaled to $[0,1]$. 'percentile01' can be used to"
                           " implement non-parametric adaptive scaling since only the local neighbourhood is"
                           " examined. (Duplicate values assume the percentile of the middle of the range.)"
                           " In contrast to 'standardize', the 'percentile01' reduction should remain valid"
                           " anywhere the local neighbourhood has a non-zero number of finite voxels."
                           "\n\n"
                           "Logical reducers 'is_min' and 'is_max' are also available -- is_min (is_max)"
                           " replace the voxel value with 1.0 if it was the min (max) in the neighbourhood and"
                           " 0.0 otherwise. Logical reducers 'is_min_nan' and 'is_max_nan' are variants that"
                           " replace the voxel with a NaN instead of 1.0 and otherwise do not overwrite the"
                           " original voxel value.";
    out.args.back().default_val = "median";
    out.args.back().expected = true;
    out.args.back().examples = { "min",
                                 "erode",
                                 "mean", 
                                 "median",
                                 "max",
                                 "dilate",
                                 "geometric_mean",
                                 "standardize",
                                 "percentile01",
                                 "is_min",
                                 "is_max",
                                 "is_min_nan",
                                 "is_max_nan" };
    out.args.back().samples = OpArgSamples::Exhaustive;


    out.args.emplace_back();
    out.args.back().name = "MaxDistance";
    out.args.back().desc = "The maximum distance (inclusive, in DICOM units: mm) within which neighbouring"
                           " voxels will be evaluated for variable-size neighbourhoods."
                           " Note that this parameter will be ignored if a fixed-size neighbourhood has"
                           " been specified."
                           "\n\n"
                           " For spherical neighbourhoods, this distance refers to the"
                           " radius. For cubic neighbourhoods, this distance refers to 'box radius' or the distance"
                           " from the cube centre to the nearest point on each bounding face."
                           " Voxels separated by more than this distance will not be evaluated together.";
    out.args.back().default_val = "2.0";
    out.args.back().expected = true;
    out.args.back().examples = { "0.5",
                                 "2.0",
                                 "15.0" };

    return out;
}

bool ReduceNeighbourhood(Drover &DICOM_data,
                           const OperationArgPkg& OptArgs,
                           std::map<std::string, std::string>& /*InvocationMetadata*/,
                           const std::string& /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();

    const auto NormalizedROILabelRegex = OptArgs.getValueStr("NormalizedROILabelRegex").value();
    const auto ROILabelRegex = OptArgs.getValueStr("ROILabelRegex").value();
    const auto ROISelection = OptArgs.getValueStr("ROISelection").value();

    const auto Channel = std::stol( OptArgs.getValueStr("Channel").value() );

    const auto NeighbourhoodStr = OptArgs.getValueStr("Neighbourhood").value();
    const auto ReductionStr = OptArgs.getValueStr("Reduction").value();

    const auto MaxDistance = std::stod( OptArgs.getValueStr("MaxDistance").value() );


    //-----------------------------------------------------------------------------------------------------------------
    const auto regex_sph = Compile_Regex("^sp?h?e?r?i?c?a?l?$");
    const auto regex_cub = Compile_Regex("^cu?b?i?c?$");
    const auto regex_333 = Compile_Regex("^3x?3x?3$");
    const auto regex_555 = Compile_Regex("^5x?5x?5$");
    const auto regex_sp3 = Compile_Regex("^3sphe?r?e?$");
    const auto regex_sp5 = Compile_Regex("^5sphe?r?e?$");
    const auto regex_sp7 = Compile_Regex("^7sphe?r?e?$");
    const auto regex_sp9 = Compile_Regex("^9sphe?r?e?$");
    const auto regex_sp11 = Compile_Regex("^11sphe?r?e?$");
    const auto regex_sp13 = Compile_Regex("^13sphe?r?e?$");
    const auto regex_sp15 = Compile_Regex("^15sphe?r?e?$");

    const auto regex_min     = Compile_Regex("^mini?m?u?m?$");
    const auto regex_erode   = Compile_Regex("^er?o?.*"); // 'erode' and 'erosion'.
    const auto regex_median  = Compile_Regex("^medi?a?n?$");
    const auto regex_mean    = Compile_Regex("^mean?$");
    const auto regex_max     = Compile_Regex("^maxi?m?u?m?$");
    const auto regex_dilate  = Compile_Regex("^di?l?a?t?.*"); // 'dilate' and 'dilation'.
    const auto regex_geomean = Compile_Regex("^ge?o?m?e?t?r?i?c?[-_]?mean?$");

    const auto regex_stdize  = Compile_Regex("^st?a?n?d?a?r?d?i?z?e?d?$");
    const auto regex_ptile01 = Compile_Regex("^pe?r?c?e?n?[-_]?t?i?l?e?0?1?$");

    const auto regex_is_min = Compile_Regex("^is?[-_]?m?ini?m?u?m?$");
    const auto regex_is_max = Compile_Regex("^is?[-_]?m?axi?m?u?m?$");
    const auto regex_is_min_nan = Compile_Regex("^is?[-_]?m?ini?m?u?m?[-_]?nan$");
    const auto regex_is_max_nan = Compile_Regex("^is?[-_]?m?axi?m?u?m?[-_]?nan$");

    //Stuff references to all contours into a list. Remember that you can still address specific contours through
    // the original holding containers (which are not modified here).
    auto cc_all = All_CCs( DICOM_data );
    auto cc_ROIs = Whitelist( cc_all, ROILabelRegex, NormalizedROILabelRegex, ROISelection );
    if(cc_ROIs.empty()){
        throw std::invalid_argument("No contours selected. Cannot continue.");
    }


    // This routine constructs an isotropic voxel neighbourhood (in pixel number coordinates) to speed up neighbourhood
    // sampling for large neighbourhoods.
    //
    // Note that this routine assumes the grid is isotropically regular. If the grid is not fully regular but is
    // still rectilinear, the neighbourhood will be non-isotropic (e.g., elliptical).
    //
    // Note that this routine accounts for a small amount of numerical uncertainty when determining voxel inclusivity.
    // Specifying a max_radius of 2.5 should reliably give you a sphere 5 voxels wide.
    //
    // Note that this routine includes the self voxel (0,0,0). This is done for consistency.
    //
    const auto make_fixed_neighbourhood_spherical = [](double max_radius){ // in pixel coordinates.
        std::vector<std::array<int64_t,3>> voxel_triplets;

        const auto max_px_coord = static_cast<int64_t>(std::ceil(max_radius));
        const auto machine_eps = 2.0 * std::sqrt(std::numeric_limits<double>::epsilon());
        const vec3<double> zero(0.0, 0.0, 0.0);
        for(int64_t i = -max_px_coord; i <= max_px_coord; ++i){
            for(int64_t j = -max_px_coord; j <= max_px_coord; ++j){
                for(int64_t k = -max_px_coord; k <= max_px_coord; ++k){
                    const vec3<double> R( static_cast<double>(i),
                                          static_cast<double>(j),
                                          static_cast<double>(k) );
                    const auto dist = (R - zero).length();
                    if(dist <= (max_radius + machine_eps)){
                        voxel_triplets.emplace_back( std::array<int64_t, 3>{i, j, k} );
                    }
                }
            }
        }
        return voxel_triplets;
    };

    auto IAs_all = All_IAs( DICOM_data );
    auto IAs = Whitelist( IAs_all, ImageSelectionStr );
    for(auto & iap_it : IAs){

        ComputeVolumetricNeighbourhoodSamplerUserData ud;
        ud.channel = Channel;
        ud.maximum_distance = MaxDistance;
        ud.description = "Neighbourhood-reduced";

        if( std::regex_match(NeighbourhoodStr, regex_sph) ){
            ud.neighbourhood = ComputeVolumetricNeighbourhoodSamplerUserData::Neighbourhood::Spherical;
            ud.description += " (spherical, max-radius=" + std::to_string(ud.maximum_distance) + ")";

        }else if( std::regex_match(NeighbourhoodStr, regex_cub) ){
            ud.neighbourhood = ComputeVolumetricNeighbourhoodSamplerUserData::Neighbourhood::Cubic;
            ud.description += " (cubic, max-dist=" + std::to_string(ud.maximum_distance) + ")";

        }else if( std::regex_match(NeighbourhoodStr, regex_333) ){
            ud.neighbourhood = ComputeVolumetricNeighbourhoodSamplerUserData::Neighbourhood::Selection;
            ud.maximum_distance = std::numeric_limits<double>::quiet_NaN();
            ud.description += " (3x3x3 pixel cube)";
            ud.voxel_triplets = { { 

                std::array<int64_t, 3>{ -1, -1, -1 },
                std::array<int64_t, 3>{ -1, -1,  0 },
                std::array<int64_t, 3>{ -1, -1,  1 },
                std::array<int64_t, 3>{ -1,  0, -1 },
                std::array<int64_t, 3>{ -1,  0,  0 },
                std::array<int64_t, 3>{ -1,  0,  1 },
                std::array<int64_t, 3>{ -1,  1, -1 },
                std::array<int64_t, 3>{ -1,  1,  0 },
                std::array<int64_t, 3>{ -1,  1,  1 },

                std::array<int64_t, 3>{  0, -1, -1 },
                std::array<int64_t, 3>{  0, -1,  0 },
                std::array<int64_t, 3>{  0, -1,  1 },
                std::array<int64_t, 3>{  0,  0, -1 },
                std::array<int64_t, 3>{  0,  0,  0 },  // Note: this sampler includes the self voxel for consistency with other neighbourhoods.
                std::array<int64_t, 3>{  0,  0,  1 },
                std::array<int64_t, 3>{  0,  1, -1 },
                std::array<int64_t, 3>{  0,  1,  0 },
                std::array<int64_t, 3>{  0,  1,  1 },

                std::array<int64_t, 3>{  1, -1, -1 },
                std::array<int64_t, 3>{  1, -1,  0 },
                std::array<int64_t, 3>{  1, -1,  1 },
                std::array<int64_t, 3>{  1,  0, -1 },
                std::array<int64_t, 3>{  1,  0,  0 },
                std::array<int64_t, 3>{  1,  0,  1 },
                std::array<int64_t, 3>{  1,  1, -1 },
                std::array<int64_t, 3>{  1,  1,  0 },
                std::array<int64_t, 3>{  1,  1,  1 }
                
            } };

        }else if( std::regex_match(NeighbourhoodStr, regex_555) ){
            ud.neighbourhood = ComputeVolumetricNeighbourhoodSamplerUserData::Neighbourhood::Selection;
            ud.maximum_distance = std::numeric_limits<double>::quiet_NaN();
            ud.description += " (5x5x5 pixel cube)";
            ud.voxel_triplets = { { 

                std::array<int64_t, 3>{ -2, -2, -2 },
                std::array<int64_t, 3>{ -2, -2, -1 },
                std::array<int64_t, 3>{ -2, -2,  0 },
                std::array<int64_t, 3>{ -2, -2,  1 },
                std::array<int64_t, 3>{ -2, -2,  2 },
                std::array<int64_t, 3>{ -2, -1, -2 },
                std::array<int64_t, 3>{ -2, -1, -1 },
                std::array<int64_t, 3>{ -2, -1,  0 },
                std::array<int64_t, 3>{ -2, -1,  1 },
                std::array<int64_t, 3>{ -2, -1,  2 },
                std::array<int64_t, 3>{ -2,  0, -2 },
                std::array<int64_t, 3>{ -2,  0, -1 },
                std::array<int64_t, 3>{ -2,  0,  0 },
                std::array<int64_t, 3>{ -2,  0,  1 },
                std::array<int64_t, 3>{ -2,  0,  2 },
                std::array<int64_t, 3>{ -2,  1, -2 },
                std::array<int64_t, 3>{ -2,  1, -1 },
                std::array<int64_t, 3>{ -2,  1,  0 },
                std::array<int64_t, 3>{ -2,  1,  1 },
                std::array<int64_t, 3>{ -2,  1,  2 },
                std::array<int64_t, 3>{ -2,  2, -2 },
                std::array<int64_t, 3>{ -2,  2, -1 },
                std::array<int64_t, 3>{ -2,  2,  0 },
                std::array<int64_t, 3>{ -2,  2,  1 },
                std::array<int64_t, 3>{ -2,  2,  2 },

                std::array<int64_t, 3>{ -1, -2, -2 },
                std::array<int64_t, 3>{ -1, -2, -1 },
                std::array<int64_t, 3>{ -1, -2,  0 },
                std::array<int64_t, 3>{ -1, -2,  1 },
                std::array<int64_t, 3>{ -1, -2,  2 },
                std::array<int64_t, 3>{ -1, -1, -2 },
                std::array<int64_t, 3>{ -1, -1, -1 },
                std::array<int64_t, 3>{ -1, -1,  0 },
                std::array<int64_t, 3>{ -1, -1,  1 },
                std::array<int64_t, 3>{ -1, -1,  2 },
                std::array<int64_t, 3>{ -1,  0, -2 },
                std::array<int64_t, 3>{ -1,  0, -1 },
                std::array<int64_t, 3>{ -1,  0,  0 },
                std::array<int64_t, 3>{ -1,  0,  1 },
                std::array<int64_t, 3>{ -1,  0,  2 },
                std::array<int64_t, 3>{ -1,  1, -2 },
                std::array<int64_t, 3>{ -1,  1, -1 },
                std::array<int64_t, 3>{ -1,  1,  0 },
                std::array<int64_t, 3>{ -1,  1,  1 },
                std::array<int64_t, 3>{ -1,  1,  2 },
                std::array<int64_t, 3>{ -1,  2, -2 },
                std::array<int64_t, 3>{ -1,  2, -1 },
                std::array<int64_t, 3>{ -1,  2,  0 },
                std::array<int64_t, 3>{ -1,  2,  1 },
                std::array<int64_t, 3>{ -1,  2,  2 },

                std::array<int64_t, 3>{  0, -2, -2 },
                std::array<int64_t, 3>{  0, -2, -1 },
                std::array<int64_t, 3>{  0, -2,  0 },
                std::array<int64_t, 3>{  0, -2,  1 },
                std::array<int64_t, 3>{  0, -2,  2 },
                std::array<int64_t, 3>{  0, -1, -2 },
                std::array<int64_t, 3>{  0, -1, -1 },
                std::array<int64_t, 3>{  0, -1,  0 },
                std::array<int64_t, 3>{  0, -1,  1 },
                std::array<int64_t, 3>{  0, -1,  2 },
                std::array<int64_t, 3>{  0,  0, -2 },
                std::array<int64_t, 3>{  0,  0, -1 },
                std::array<int64_t, 3>{  0,  0,  0 }, // Note: this sampler includes the self voxel for consistency with other neighbourhoods.
                std::array<int64_t, 3>{  0,  0,  1 },
                std::array<int64_t, 3>{  0,  0,  2 },
                std::array<int64_t, 3>{  0,  1, -2 },
                std::array<int64_t, 3>{  0,  1, -1 },
                std::array<int64_t, 3>{  0,  1,  0 },
                std::array<int64_t, 3>{  0,  1,  1 },
                std::array<int64_t, 3>{  0,  1,  2 },
                std::array<int64_t, 3>{  0,  2, -2 },
                std::array<int64_t, 3>{  0,  2, -1 },
                std::array<int64_t, 3>{  0,  2,  0 },
                std::array<int64_t, 3>{  0,  2,  1 },
                std::array<int64_t, 3>{  0,  2,  2 },

                std::array<int64_t, 3>{  1, -2, -2 },
                std::array<int64_t, 3>{  1, -2, -1 },
                std::array<int64_t, 3>{  1, -2,  0 },
                std::array<int64_t, 3>{  1, -2,  1 },
                std::array<int64_t, 3>{  1, -2,  2 },
                std::array<int64_t, 3>{  1, -1, -2 },
                std::array<int64_t, 3>{  1, -1, -1 },
                std::array<int64_t, 3>{  1, -1,  0 },
                std::array<int64_t, 3>{  1, -1,  1 },
                std::array<int64_t, 3>{  1, -1,  2 },
                std::array<int64_t, 3>{  1,  0, -2 },
                std::array<int64_t, 3>{  1,  0, -1 },
                std::array<int64_t, 3>{  1,  0,  0 },
                std::array<int64_t, 3>{  1,  0,  1 },
                std::array<int64_t, 3>{  1,  0,  2 },
                std::array<int64_t, 3>{  1,  1, -2 },
                std::array<int64_t, 3>{  1,  1, -1 },
                std::array<int64_t, 3>{  1,  1,  0 },
                std::array<int64_t, 3>{  1,  1,  1 },
                std::array<int64_t, 3>{  1,  1,  2 },
                std::array<int64_t, 3>{  1,  2, -2 },
                std::array<int64_t, 3>{  1,  2, -1 },
                std::array<int64_t, 3>{  1,  2,  0 },
                std::array<int64_t, 3>{  1,  2,  1 },
                std::array<int64_t, 3>{  1,  2,  2 },

                std::array<int64_t, 3>{  2, -2, -2 },
                std::array<int64_t, 3>{  2, -2, -1 },
                std::array<int64_t, 3>{  2, -2,  0 },
                std::array<int64_t, 3>{  2, -2,  1 },
                std::array<int64_t, 3>{  2, -2,  2 },
                std::array<int64_t, 3>{  2, -1, -2 },
                std::array<int64_t, 3>{  2, -1, -1 },
                std::array<int64_t, 3>{  2, -1,  0 },
                std::array<int64_t, 3>{  2, -1,  1 },
                std::array<int64_t, 3>{  2, -1,  2 },
                std::array<int64_t, 3>{  2,  0, -2 },
                std::array<int64_t, 3>{  2,  0, -1 },
                std::array<int64_t, 3>{  2,  0,  0 },
                std::array<int64_t, 3>{  2,  0,  1 },
                std::array<int64_t, 3>{  2,  0,  2 },
                std::array<int64_t, 3>{  2,  1, -2 },
                std::array<int64_t, 3>{  2,  1, -1 },
                std::array<int64_t, 3>{  2,  1,  0 },
                std::array<int64_t, 3>{  2,  1,  1 },
                std::array<int64_t, 3>{  2,  1,  2 },
                std::array<int64_t, 3>{  2,  2, -2 },
                std::array<int64_t, 3>{  2,  2, -1 },
                std::array<int64_t, 3>{  2,  2,  0 },
                std::array<int64_t, 3>{  2,  2,  1 },
                std::array<int64_t, 3>{  2,  2,  2 }

            } };

        }else if( std::regex_match(NeighbourhoodStr, regex_sp3) ){
            ud.neighbourhood = ComputeVolumetricNeighbourhoodSamplerUserData::Neighbourhood::Selection;
            ud.maximum_distance = std::numeric_limits<double>::quiet_NaN();
            ud.description += " (3-pixel-wide sphere)";

            const double max_radius = 1.5;
            ud.voxel_triplets = make_fixed_neighbourhood_spherical(max_radius);

        }else if( std::regex_match(NeighbourhoodStr, regex_sp5) ){
            ud.neighbourhood = ComputeVolumetricNeighbourhoodSamplerUserData::Neighbourhood::Selection;
            ud.maximum_distance = std::numeric_limits<double>::quiet_NaN();
            ud.description += " (5-pixel-wide sphere)";

            const double max_radius = 2.5;
            ud.voxel_triplets = make_fixed_neighbourhood_spherical(max_radius);

        }else if( std::regex_match(NeighbourhoodStr, regex_sp7) ){
            ud.neighbourhood = ComputeVolumetricNeighbourhoodSamplerUserData::Neighbourhood::Selection;
            ud.maximum_distance = std::numeric_limits<double>::quiet_NaN();
            ud.description += " (7-pixel-wide sphere)";

            const double max_radius = 3.5;
            ud.voxel_triplets = make_fixed_neighbourhood_spherical(max_radius);

        }else if( std::regex_match(NeighbourhoodStr, regex_sp9) ){
            ud.neighbourhood = ComputeVolumetricNeighbourhoodSamplerUserData::Neighbourhood::Selection;
            ud.maximum_distance = std::numeric_limits<double>::quiet_NaN();
            ud.description += " (9-pixel-wide sphere)";

            const double max_radius = 4.5;
            ud.voxel_triplets = make_fixed_neighbourhood_spherical(max_radius);

        }else if( std::regex_match(NeighbourhoodStr, regex_sp11) ){
            ud.neighbourhood = ComputeVolumetricNeighbourhoodSamplerUserData::Neighbourhood::Selection;
            ud.maximum_distance = std::numeric_limits<double>::quiet_NaN();
            ud.description += " (11-pixel-wide sphere)";

            const double max_radius = 5.5;
            ud.voxel_triplets = make_fixed_neighbourhood_spherical(max_radius);

        }else if( std::regex_match(NeighbourhoodStr, regex_sp13) ){
            ud.neighbourhood = ComputeVolumetricNeighbourhoodSamplerUserData::Neighbourhood::Selection;
            ud.maximum_distance = std::numeric_limits<double>::quiet_NaN();
            ud.description += " (13-pixel-wide sphere)";

            const double max_radius = 6.5;
            ud.voxel_triplets = make_fixed_neighbourhood_spherical(max_radius);

        }else if( std::regex_match(NeighbourhoodStr, regex_sp15) ){
            ud.neighbourhood = ComputeVolumetricNeighbourhoodSamplerUserData::Neighbourhood::Selection;
            ud.maximum_distance = std::numeric_limits<double>::quiet_NaN();
            ud.description += " (15-pixel-wide sphere)";

            const double max_radius = 7.5;
            ud.voxel_triplets = make_fixed_neighbourhood_spherical(max_radius);

        }else{
            throw std::invalid_argument("Neighbourhood argument '"_s + NeighbourhoodStr + "' is not valid");
        }

        if( std::regex_match(ReductionStr, regex_min)
              ||  std::regex_match(ReductionStr, regex_erode) ){
            ud.f_reduce = [](float, std::vector<float> &shtl, vec3<double>) -> float {
                              return Stats::Min(shtl);
                          };
        }else if( std::regex_match(ReductionStr, regex_median) ){
            ud.f_reduce = [](float, std::vector<float> &shtl, vec3<double>) -> float {
                              return Stats::Median(shtl);
                          };
        }else if( std::regex_match(ReductionStr, regex_mean) ){
            ud.f_reduce = [](float, std::vector<float> &shtl, vec3<double>) -> float {
                              return Stats::Mean(shtl);
                          };
        }else if( std::regex_match(ReductionStr, regex_max)
              ||  std::regex_match(ReductionStr, regex_dilate) ){
            ud.f_reduce = [](float, std::vector<float> &shtl, vec3<double>) -> float {
                              return Stats::Max(shtl);
                          };

        }else if( std::regex_match(ReductionStr, regex_geomean) ){
            const auto nan = std::numeric_limits<double>::quiet_NaN();
            ud.f_reduce = [nan](float, std::vector<float> &shtl, vec3<double>) -> float {
                              double prod = 1.0;
                              if(!shtl.empty()){
                                  const double expon = 1.0 / static_cast<double>(shtl.size());
                                  for(const auto& x : shtl) prod *= std::pow(x, expon);
                              }else{
                                  prod = nan;
                              }
                              return static_cast<float>(prod);
                          };

        }else if( std::regex_match(ReductionStr, regex_stdize) ){
            const auto nan = std::numeric_limits<double>::quiet_NaN();
            ud.f_reduce = [nan](float f, std::vector<float> &shtl, vec3<double>) -> float {
                              if( std::isfinite(f) ){
                                  const auto mean = Stats::Mean(shtl);
                                  const auto var = Stats::Unbiased_Var_Est(shtl);
                                  const auto std_dev = std::sqrt( var );
                                  f = (f - mean) / std_dev;
                                  if(!std::isfinite(f)){
                                      f = nan;
                                  }
                              } 
                              return f;
                          };

        }else if( std::regex_match(ReductionStr, regex_ptile01) ){
            const auto nan = std::numeric_limits<double>::quiet_NaN();
            ud.f_reduce = [nan](float f, std::vector<float> &shtl, vec3<double>) -> float {
                              if( !std::isnan(f) ){
                                  // Purge NaNs.
                                  auto f_beg = shtl.begin();
                                  auto f_end = std::remove_if(f_beg, shtl.end(),
                                      [](const float &x) -> bool { return std::isnan(x); });
                                  const auto N_remain = static_cast<int64_t>( std::distance(f_beg, f_end) );
                                  if( N_remain == 0 ){
                                      f = nan;
                                  }else{
                                      // Determine the percentile where duplicates use the middle position.
                                      std::sort(f_beg, f_end);
                                      const auto bounds = std::equal_range(f_beg, f_end, f);
                                      const auto lhs_it = bounds.first;
                                      const auto rhs_it = bounds.second;
                                      if(lhs_it == rhs_it){
                                          f = nan;
                                      }else{
                                          const auto N_lhs = static_cast<int64_t>( std::distance(f_beg, lhs_it) );
                                          // Correct for rhs being +1 past the RHS.
                                          const auto N_rhs = static_cast<int64_t>( std::distance(f_beg, rhs_it) ) - 1;
                                          f = 0.5 * static_cast<float>(N_rhs + N_lhs) / (static_cast<float>(N_remain) - 1.0);
                                      }
                                  }
                              } 
                              return f;
                          };

        }else if( std::regex_match(ReductionStr, regex_is_min_nan) ){
            const auto machine_eps = std::sqrt( std::numeric_limits<float>::epsilon() );
            ud.f_reduce = [=](float v, std::vector<float> &shtl, vec3<double>) -> float {
                              const auto min = Stats::Min(shtl);
                              const auto diff = std::abs(v - min);
                              return (diff < machine_eps) ? std::numeric_limits<float>::quiet_NaN() : v;
                          };
        }else if( std::regex_match(ReductionStr, regex_is_max_nan) ){
            const auto machine_eps = std::sqrt( std::numeric_limits<float>::epsilon() );
            ud.f_reduce = [=](float v, std::vector<float> &shtl, vec3<double>) -> float {
                              const auto max = Stats::Max(shtl);
                              const auto diff = std::abs(v - max);
                              return (diff < machine_eps) ? std::numeric_limits<float>::quiet_NaN() : v;
                          };

        }else if( std::regex_match(ReductionStr, regex_is_min) ){
            const auto machine_eps = std::sqrt( std::numeric_limits<float>::epsilon() );
            ud.f_reduce = [=](float v, std::vector<float> &shtl, vec3<double>) -> float {
                              const auto min = Stats::Min(shtl);
                              const auto diff = std::abs(v - min);
                              return (diff < machine_eps) ? 1.0f : 0.0f;
                          };
        }else if( std::regex_match(ReductionStr, regex_is_max) ){
            const auto machine_eps = std::sqrt( std::numeric_limits<float>::epsilon() );
            ud.f_reduce = [=](float v, std::vector<float> &shtl, vec3<double>) -> float {
                              const auto max = Stats::Max(shtl);
                              const auto diff = std::abs(v - max);
                              return (diff < machine_eps) ? 1.0f : 0.0f;
                          };

        }else{
            throw std::invalid_argument("Reduction argument '"_s + ReductionStr + "' is not valid");
        }

        if(!ud.voxel_triplets.empty()){
            YLOGINFO("Neighbourhood comprises " << ud.voxel_triplets.size() << " neighbours");
        }

        if(!(*iap_it)->imagecoll.Compute_Images( ComputeVolumetricNeighbourhoodSampler, 
                                                 {}, cc_ROIs, &ud )){
            throw std::runtime_error("Unable to reduce voxel neighbourhood.");
        }
    }

    return true;
}
