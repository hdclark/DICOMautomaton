//ApplyCalibrationCurve.cc - A part of DICOMautomaton 2018. Written by hal clark.

#include <experimental/any>
#include <experimental/optional>
#include <functional>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>    

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../YgorImages_Functors/Grouping/Misc_Functors.h"
#include "../YgorImages_Functors/Processing/Partitioned_Image_Voxel_Visitor_Mutator.h"
#include "ApplyCalibrationCurve.h"
#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorString.h"       //Needed for GetFirstRegex(...)



std::list<OperationArgDoc> OpArgDocApplyCalibrationCurve(void){
    std::list<OperationArgDoc> out;

    // This operation overwrites applies a given calibration curve to voxel data inside the specified ROI(s).
    // This routine can handle overlapping or duplicate contours.

    out.emplace_back();
    out.back().name = "Channel";
    out.back().desc = "The image channel to use. Zero-based. Use '-1' to operate on all available channels.";
    out.back().default_val = "-1";
    out.back().expected = true;
    out.back().examples = { "-1", "0", "1", "2" };

    out.emplace_back();
    out.back().name = "ImageSelection";
    out.back().desc = "Images to operate on. Either 'none', 'last', or 'all'.";
    out.back().default_val = "last";
    out.back().expected = true;
    out.back().examples = { "none", "last", "all" };

    out.emplace_back();
    out.back().name = "ContourOverlap";
    out.back().desc = "Controls overlapping contours are treated."
                      " The default 'ignore' treats overlapping contours as a single contour, regardless of"
                      " contour orientation. The option 'honour_opposite_orientations' makes overlapping contours"
                      " with opposite orientation cancel. Otherwise, orientation is ignored. The latter is useful"
                      " for Boolean structures where contour orientation is significant for interior contours (holes)."
                      " The option 'overlapping_contours_cancel' ignores orientation and cancels all contour overlap.";
    out.back().default_val = "ignore";
    out.back().expected = true;
    out.back().examples = { "ignore", "honour_opposite_orientations", 
                            "overlapping_contours_cancel", "honour_opps", "overlap_cancel" }; 

    out.emplace_back();
    out.back().name = "Inclusivity";
    out.back().desc = "Controls how voxels are deemed to be 'within' the interior of the selected ROI(s)."
                      " The default 'center' considers only the central-most point of each voxel."
                      " There are two corner options that correspond to a 2D projection of the voxel onto the image plane."
                      " The first, 'planar_corner_inclusive', considers a voxel interior if ANY corner is interior."
                      " The second, 'planar_corner_exclusive', considers a voxel interior if ALL (four) corners are interior.";
    out.back().default_val = "center";
    out.back().expected = true;
    out.back().examples = { "center", "centre", 
                            "planar_corner_inclusive", "planar_inc",
                            "planar_corner_exclusive", "planar_exc" };

    out.emplace_back();
    out.back().name = "CalibCurveFileName";
    out.back().desc = "The file from which a calibration curve should be read from."
                      " The format should be line-based with either 2 or 4 numbers per line. For 2 numbers:"
                      " (current pixel value) (new pixel value) and for 4 numbers:"
                      " (current pixel value) (uncertainty) (new pixel value) (uncertainty)."
                      " Uncertainties refer to the prior number and may be uniformly zero if unknown."
                      " Lines beginning with '#' are treated as comments and ignored."
                      " The curve is linearly interpolated, and must span the full range of pixel values."
                      " This is done to avoid extrapolation within the operation since the correct"
                      " behaviour will differ depending on the specifics of the calibration.";
    out.back().default_val = "";
    out.back().expected = true;
    out.back().examples = { "/tmp/calib.dat" };


    out.emplace_back();
    out.back().name = "NormalizedROILabelRegex";
    out.back().desc = "A regex matching ROI labels/names to consider. The default will match"
                      " all available ROIs. Be aware that input spaces are trimmed to a single space."
                      " If your ROI name has more than two sequential spaces, use regex to avoid them."
                      " All ROIs have to match the single regex, so use the 'or' token if needed."
                      " Regex is case insensitive and uses extended POSIX syntax.";
    out.back().default_val = ".*";
    out.back().expected = true;
    out.back().examples = { ".*", ".*Body.*", "Body", "Gross_Liver",
                            R"***(.*Left.*Parotid.*|.*Right.*Parotid.*|.*Eye.*)***",
                            R"***(Left Parotid|Right Parotid)***" };

    out.emplace_back();
    out.back().name = "ROILabelRegex";
    out.back().desc = "A regex matching ROI labels/names to consider. The default will match"
                      " all available ROIs. Be aware that input spaces are trimmed to a single space."
                      " If your ROI name has more than two sequential spaces, use regex to avoid them."
                      " All ROIs have to match the single regex, so use the 'or' token if needed."
                      " Regex is case insensitive and uses extended POSIX syntax.";
    out.back().default_val = ".*";
    out.back().expected = true;
    out.back().examples = { ".*", ".*body.*", "body", "Gross_Liver",
                            R"***(.*left.*parotid.*|.*right.*parotid.*|.*eyes.*)***",
                            R"***(left_parotid|right_parotid)***" };

    return out;
}



Drover ApplyCalibrationCurve(Drover DICOM_data, 
                           OperationArgPkg OptArgs, 
                           std::map<std::string,std::string> /*InvocationMetadata*/, 
                           std::string /*FilenameLex*/ ){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto Channel = std::stol( OptArgs.getValueStr("Channel").value() );
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();
    const auto InclusivityStr = OptArgs.getValueStr("Inclusivity").value();
    const auto ContourOverlapStr = OptArgs.getValueStr("ContourOverlap").value();

    const auto CalibCurveFileName = OptArgs.getValueStr("CalibCurveFileName").value();

    const auto NormalizedROILabelRegex = OptArgs.getValueStr("NormalizedROILabelRegex").value();
    const auto ROILabelRegex = OptArgs.getValueStr("ROILabelRegex").value();

    //-----------------------------------------------------------------------------------------------------------------

    const auto regex_none = std::regex("no?n?e?$", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto regex_last = std::regex("la?s?t?$", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto regex_all  = std::regex("al?l?$",   std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto TrueRegex = std::regex("^tr?u?e?$", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);

    const auto regex_centre = std::regex("^cent.*", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto regex_pci = std::regex("^planar_?c?o?r?n?e?r?s?_?inc?l?u?s?i?v?e?$", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto regex_pce = std::regex("^planar_?c?o?r?n?e?r?s?_?exc?l?u?s?i?v?e?$", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);

    const auto regex_ignore = std::regex("^ig?n?o?r?e?$", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto regex_honopps = std::regex("^ho?n?o?u?r?_?o?p?p?o?s?i?t?e?_?o?r?i?e?n?t?a?t?i?o?n?s?$", 
                                          std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto regex_cancel = std::regex("^ov?e?r?l?a?p?p?i?n?g?_?c?o?n?t?o?u?r?s?_?c?a?n?c?e?l?s?$",
                                          std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);

    if( !std::regex_match(ImageSelectionStr, regex_none)
    &&  !std::regex_match(ImageSelectionStr, regex_last)
    &&  !std::regex_match(ImageSelectionStr, regex_all) ){
        throw std::invalid_argument("Image selection is not valid. Cannot continue.");
    }

    //Read the calibration curve file.
    samples_1D<double> calib_curve;
    if(!calib_curve.Read_From_File(CalibCurveFileName)){
        throw std::invalid_argument("Calibration curve file could not be read or was invalid. Cannot continue.");
    }

    //Stuff references to all contours into a list. Remember that you can still address specific contours through
    // the original holding containers (which are not modified here).
    auto cc_all = All_CCs( DICOM_data );
    auto cc_ROIs = Whitelist( cc_all, { { "ROIName", ROILabelRegex },
                                        { "NormalizedROIName", NormalizedROILabelRegex } } );
    if(cc_ROIs.empty()){
        throw std::invalid_argument("No contours selected. Cannot continue.");
    }


    //Image data.
    auto iap_it = DICOM_data.image_data.begin();
    if(false){
    }else if(std::regex_match(ImageSelectionStr, regex_none)){
        iap_it = DICOM_data.image_data.end();
    }else if(std::regex_match(ImageSelectionStr, regex_last)){
        if(!DICOM_data.image_data.empty()) iap_it = std::prev(DICOM_data.image_data.end());
    }
    while(iap_it != DICOM_data.image_data.end()){
        PartitionedImageVoxelVisitorMutatorUserData ud;

        ud.mutation_opts.editstyle = Mutate_Voxels_Opts::EditStyle::InPlace;
        ud.mutation_opts.aggregate = Mutate_Voxels_Opts::Aggregate::First;
        ud.mutation_opts.adjacency = Mutate_Voxels_Opts::Adjacency::SingleVoxel;
        ud.mutation_opts.maskmod   = Mutate_Voxels_Opts::MaskMod::Noop;
        ud.description = "Corrected via calibration curve";

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

        ud.f_bounded = [&](long int /*row*/, long int /*col*/, long int chan, float &voxel_val) {
            if( (Channel < 0) || (Channel == chan) ){
                voxel_val = calib_curve.Interpolate_Linearly(voxel_val)[2];
            }
        };
        std::function<void(long int, long int, long int, float &)> f_noop;
        ud.f_visitor = f_noop;
        ud.f_unbounded = f_noop;

        if(!(*iap_it)->imagecoll.Process_Images_Parallel( GroupIndividualImages,
                                                          PartitionedImageVoxelVisitorMutator,
                                                          {}, cc_ROIs, &ud )){
            throw std::runtime_error("Unable to apply calibration curve to voxels with the specified ROI(s).");
        }
        ++iap_it;
    }

    return DICOM_data;
}
