//GenerateCalibrationCurve.cc - A part of DICOMautomaton 2019. Written by hal clark.

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
#include <mutex>

#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorString.h"       //Needed for GetFirstRegex(...)
#include "YgorStats.h"

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../YgorImages_Functors/Grouping/Misc_Functors.h"
#include "../YgorImages_Functors/Processing/Partitioned_Image_Voxel_Visitor_Mutator.h"

#include "GenerateCalibrationCurve.h"


OperationDoc OpArgDocGenerateCalibrationCurve(){
    OperationDoc out;
    out.name = "GenerateCalibrationCurve";

    out.desc = 
        "This operation uses two overlapping images volumes to generate a calibration curve mapping from the first"
        " image volume to the second. Only the region within the specified ROI(s) is considered.";

    out.notes.emplace_back(
        "ROI(s) are interpretted relative to the mapped-to ('reference' or 'fixed') image."
        " The reason for this is that typically the reference images are associated with contours"
        " (e.g., planning data) and the mapped-from images do not (e.g., CBCTs that have been registered)."
    );
    out.notes.emplace_back(
        "This routine can handle overlapping or duplicate contours."
    );

    out.args.emplace_back();
    out.args.back().name = "Channel";
    out.args.back().desc = "The image channel to use. Zero-based. Use '-1' to operate on all available channels.";
    out.args.back().default_val = "-1";
    out.args.back().expected = true;
    out.args.back().examples = { "-1", "0", "1", "2" };

    out.args.emplace_back();
    out.args.back() = IAWhitelistOpArgDoc();
    out.args.back().name = "ImageSelection";
    out.args.back().desc += " Note that these images are the 'mapped-from' or 'moving' images.";
    out.args.back().default_val = "last";

    out.args.emplace_back();
    out.args.back() = IAWhitelistOpArgDoc();
    out.args.back().name = "RefImageSelection";
    out.args.back().desc += " Note that these images are the 'mapped-to' or 'fixed' images.";
    out.args.back().default_val = "last";

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

    out.args.emplace_back();
    out.args.back().name = "CalibCurveFileName";
    out.args.back().desc = "The file to which a calibration curve will be written to."
                           " The format is line-based with 4 numbers per line:"
                           " (original pixel value) (uncertainty) (new pixel value) (uncertainty)."
                           " Uncertainties refer to the prior number and may be uniformly zero if unknown."
                           " Lines beginning with '#' are comments."
                           " The curve is meant to be interpolated. (Later attempts to extrapolate may result"
                           " in failure.)";
    out.args.back().default_val = "";
    out.args.back().expected = true;
    out.args.back().examples = { "./calib.dat" };

    out.args.emplace_back();
    out.args.back().name = "NormalizedROILabelRegex";
    out.args.back().desc = "A regex matching ROI labels/names to consider. The default will match"
                           " all available ROIs. Be aware that input spaces are trimmed to a single space."
                           " If your ROI name has more than two sequential spaces, use regex to avoid them."
                           " All ROIs have to match the single regex, so use the 'or' token if needed."
                           " Regex is case insensitive and uses extended POSIX syntax.";
    out.args.back().default_val = ".*";
    out.args.back().expected = true;
    out.args.back().examples = { ".*", ".*Body.*", "Body", "Gross_Liver",
                                 R"***(.*Left.*Parotid.*|.*Right.*Parotid.*|.*Eye.*)***",
                                 R"***(Left Parotid|Right Parotid)***" };

    out.args.emplace_back();
    out.args.back().name = "ROILabelRegex";
    out.args.back().desc = "A regex matching ROI labels/names to consider. The default will match"
                           " all available ROIs. Be aware that input spaces are trimmed to a single space."
                           " If your ROI name has more than two sequential spaces, use regex to avoid them."
                           " All ROIs have to match the single regex, so use the 'or' token if needed."
                           " Regex is case insensitive and uses extended POSIX syntax.";
    out.args.back().default_val = ".*";
    out.args.back().expected = true;
    out.args.back().examples = { ".*", ".*body.*", "body", "Gross_Liver",
                                 R"***(.*left.*parotid.*|.*right.*parotid.*|.*eyes.*)***",
                                 R"***(left_parotid|right_parotid)***" };

    return out;
}



Drover GenerateCalibrationCurve(Drover DICOM_data, 
                           OperationArgPkg OptArgs, 
                           std::map<std::string,std::string> /*InvocationMetadata*/, 
                           std::string /*FilenameLex*/ ){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto Channel = std::stol( OptArgs.getValueStr("Channel").value() );
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();
    const auto RefImageSelectionStr = OptArgs.getValueStr("RefImageSelection").value();
    const auto InclusivityStr = OptArgs.getValueStr("Inclusivity").value();
    const auto ContourOverlapStr = OptArgs.getValueStr("ContourOverlap").value();

    const auto CalibCurveFileName = OptArgs.getValueStr("CalibCurveFileName").value();

    const auto NormalizedROILabelRegex = OptArgs.getValueStr("NormalizedROILabelRegex").value();
    const auto ROILabelRegex = OptArgs.getValueStr("ROILabelRegex").value();

    //-----------------------------------------------------------------------------------------------------------------

    //const auto regex_none = Compile_Regex("no?n?e?$");
    //const auto regex_last = Compile_Regex("la?s?t?$");
    //const auto regex_all  = Compile_Regex("al?l?$");
    const auto TrueRegex = Compile_Regex("^tr?u?e?$");

    const auto regex_centre = Compile_Regex("^cent.*");
    const auto regex_pci = Compile_Regex("^planar_?c?o?r?n?e?r?s?_?inc?l?u?s?i?v?e?$");
    const auto regex_pce = Compile_Regex("^planar_?c?o?r?n?e?r?s?_?exc?l?u?s?i?v?e?$");

    const auto regex_ignore = Compile_Regex("^ig?n?o?r?e?$");
    const auto regex_honopps = Compile_Regex("^ho?n?o?u?r?_?o?p?p?o?s?i?t?e?_?o?r?i?e?n?t?a?t?i?o?n?s?$");
    const auto regex_cancel = Compile_Regex("^ov?e?r?l?a?p?p?i?n?g?_?c?o?n?t?o?u?r?s?_?c?a?n?c?e?l?s?$");

    //if( !std::regex_match(ImageSelectionStr, regex_none)
    //&&  !std::regex_match(ImageSelectionStr, regex_last)
    //&&  !std::regex_match(ImageSelectionStr, regex_all) ){
    //    throw std::invalid_argument("Image selection is not valid. Cannot continue.");
    //}

    //if( !std::regex_match(RefImageSelectionStr, regex_none)
    //&&  !std::regex_match(RefImageSelectionStr, regex_last)
    //&&  !std::regex_match(RefImageSelectionStr, regex_all) ){
    //    throw std::invalid_argument("Reference image selection is not valid. Cannot continue.");
    //}

    //Stuff references to all contours into a list. Remember that you can still address specific contours through
    // the original holding containers (which are not modified here).
    auto cc_all = All_CCs( DICOM_data );
    auto cc_ROIs = Whitelist( cc_all, { { "ROIName", ROILabelRegex },
                                        { "NormalizedROIName", NormalizedROILabelRegex } } );
    if(cc_ROIs.empty()){
        throw std::invalid_argument("No contours selected. Cannot continue.");
    }


    auto IAs_all = All_IAs( DICOM_data );
    auto IAs = Whitelist( IAs_all, ImageSelectionStr );
    if(IAs.empty()){
        throw std::invalid_argument("No mapping-from images selected. Cannot continue.");
    }
    auto IA = IAs.front();

    auto RIAs_all = All_IAs( DICOM_data );
    auto RIAs = Whitelist( RIAs_all, RefImageSelectionStr );
    if(RIAs.empty()){
        throw std::invalid_argument("No mapping-to images selected. Cannot continue.");
    }


    std::mutex guard;
    std::map<float, std::vector<float>> A_to_B; 

    for(auto & iap_it : RIAs){
        PartitionedImageVoxelVisitorMutatorUserData ud;

        ud.mutation_opts.editstyle = Mutate_Voxels_Opts::EditStyle::InPlace;
        ud.mutation_opts.aggregate = Mutate_Voxels_Opts::Aggregate::First;
        ud.mutation_opts.adjacency = Mutate_Voxels_Opts::Adjacency::SingleVoxel;
        ud.mutation_opts.maskmod   = Mutate_Voxels_Opts::MaskMod::Noop;
//        ud.description = "Corrected via calibration curve";

        if(false){
        }else if( std::regex_match(ContourOverlapStr, regex_ignore) ){
            ud.mutation_opts.contouroverlap = Mutate_Voxels_Opts::ContourOverlap::Ignore;
        }else if( std::regex_match(ContourOverlapStr, regex_honopps) ){
            ud.mutation_opts.contouroverlap = Mutate_Voxels_Opts::ContourOverlap::HonourOppositeOrientations;
        }else if( std::regex_match(ContourOverlapStr, regex_cancel) ){
            ud.mutation_opts.contouroverlap = Mutate_Voxels_Opts::ContourOverlap::ImplicitOrientations;
        }else{
            throw std::invalid_argument("ContourOverlap argument '"_s + ContourOverlapStr + "' is not valid");
        }
        if(false){
        }else if( std::regex_match(InclusivityStr, regex_centre) ){
            ud.mutation_opts.inclusivity = Mutate_Voxels_Opts::Inclusivity::Centre;
        }else if( std::regex_match(InclusivityStr, regex_pci) ){
            ud.mutation_opts.inclusivity = Mutate_Voxels_Opts::Inclusivity::Inclusive;
        }else if( std::regex_match(InclusivityStr, regex_pce) ){
            ud.mutation_opts.inclusivity = Mutate_Voxels_Opts::Inclusivity::Exclusive;
        }else{
            throw std::invalid_argument("Inclusivity argument '"_s + InclusivityStr + "' is not valid");
        }

        ud.f_bounded = [&](long int row, long int col, long int chan, std::reference_wrapper<planar_image<float,double>> img_refw, float &voxel_val) {
            if( (Channel < 0) || (Channel == chan) ){
                try{
                    // Sample the mapping-from image.
                    const auto P = img_refw.get().position(row, col);
                    const auto interp = (*IA)->imagecoll.trilinearly_interpolate(P,chan);

                    // Round to nearest integer.
                    //const auto rounded = std::round(interp); 

                    // Bin (with a bin boundary fixed at 0).
                    const auto width = 5.0;
                    const auto rounded = std::round(interp / width) * width + 0.5 * width; // Nominal value at centre of bin.

                    std::lock_guard<std::mutex> lock(guard);
                    A_to_B[rounded].push_back(voxel_val);
                }catch(const std::exception &){ };
            }
        };
        //std::function<void(long int, long int, long int, std::reference_wrapper<planar_image<float,double>>, float &)> f_noop;
        //ud.f_visitor = f_noop;
        //ud.f_unbounded = f_noop;

        if(!(*iap_it)->imagecoll.Process_Images_Parallel( GroupIndividualImages,
                                                          PartitionedImageVoxelVisitorMutator,
                                                          {}, cc_ROIs, &ud )){
            throw std::runtime_error("Unable to apply calibration curve to voxels with the specified ROI(s).");
        }
    }

    // Reduce the distributions to some descriptive statistic and write the calibration curve file.
    {
        std::fstream FO(CalibCurveFileName, std::fstream::out); // | std::fstream::app);
        for(auto &p : A_to_B){
            const auto A = p.first;
            const auto B = Stats::Median(p.second);
            FO << A << " " << B << std::endl;
        }
        if(!FO.good()){
            throw std::invalid_argument("Calibration curve file could not be written. Cannot continue.");
        }
    }

    return DICOM_data;
}
