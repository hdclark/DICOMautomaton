//ExtractImageHistograms.cc - A part of DICOMautomaton 2018, 2020. Written by hal clark.

#include <boost/interprocess/creation_tags.hpp>
#include <boost/interprocess/sync/named_mutex.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <algorithm>
#include <cstdlib>            //Needed for exit() calls.
#include <exception>
#include <any>
#include <optional>
#include <fstream>
#include <functional>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>    
#include <utility>            //Needed for std::pair.
#include <vector>

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../YgorImages_Functors/Compute/Extract_Histograms.h"
#include "ExtractImageHistograms.h"
#include "Explicator.h"       //Needed for Explicator class.
#include "YgorFilesDirs.h"    //Needed for Does_File_Exist_And_Can_Be_Read(...), etc..
#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorStats.h"        //Needed for Stats:: namespace.



OperationDoc OpArgDocExtractImageHistograms(){
    OperationDoc out;
    out.name = "ExtractImageHistograms";

    out.desc = 
        "This operation extracts histograms (e.g., dose-volume -- DVH, or pixel intensity-volume)"
        " for the selected image(s) and ROI(s)."
        " Results are stored as line samples for later analysis or export.";
        
    out.notes.emplace_back(
        "This routine generates differential histograms with unscaled abscissae and ordinate axes."
        " It also generates cumulative histograms with unscaled abscissae and *both* unscaled"
        " and peak-normalized-to-one ordinates. Unscaled abscissa are reported in DICOM units (typically"
        " HU or Gy), unscaled ordinates are reported in volumetric DICOM units (mm^3^), and normalized"
        " ordinates are reported as a fraction of the given ROI's total volume."
    );
    out.notes.emplace_back(
        "Non-finite voxels are excluded from analysis and do not contribute to the volume."
        " If exact volume is required, ensure all voxels are finite prior to invoking this routine."
    );
    out.notes.emplace_back(
        "This routine can handle contour partitions where the physical layout (i.e., storage order)"
        " differs from the logical layout. See the 'grouping' options for available configuration."
    );
    out.notes.emplace_back(
        "This routine will correctly handle non-overlapping voxels with varying volumes"
        " (i.e., rectilinear image arrays). It will *not* correctly handle"
        " overlapping voxels (i.e., each overlapping voxel will be counted without regard for overlap)."
        " If necessary, resample image arrays to be rectilinear."
    );


    out.args.emplace_back();
    out.args.back() = IAWhitelistOpArgDoc();
    out.args.back().name = "ImageSelection";
    out.args.back().default_val = "last";
    

    out.args.emplace_back();
    out.args.back().name = "Channel";
    out.args.back().desc = "The image channel to use. Zero-based. Use '-1' to operate on all available channels.";
    out.args.back().default_val = "-1";
    out.args.back().expected = true;
    out.args.back().examples = { "-1", "0", "1", "2" };


    out.args.emplace_back();
    out.args.back() = RCWhitelistOpArgDoc();
    out.args.back().name = "ROILabelRegex";
    out.args.back().default_val = ".*";


    out.args.emplace_back();
    out.args.back() = NCWhitelistOpArgDoc();
    out.args.back().name = "NormalizedROILabelRegex";
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
    out.args.back().name = "Grouping";
    out.args.back().desc = "This routine partitions individual contours using their ROI labels."
                           " This parameter controls whether contours with different names should be treated"
                           " as though they belong to distinct logical groups ('separate') or whether *all* contours"
                           " should be treated as though they belong to a single logical group ('combined')."
                           ""
                           " The 'separate' option works best for exploratory analysis, extracting histograms for many OARs"
                           " at once, or when you know the 'physical' grouping of contours by label reflects a"
                           " consistent logical grouping."
                           ""
                           " The 'combined' option works best when the physical and logical groupings are inconsistent."
                           " For example, when you need a combined histograms from multiple contours or organs, or when"
                           " similar structures should be combined (e.g., spinal cord + canal; or distinct left + right"
                           " lateral organs that should be paired, e.g.. 'combined parotids')."
                           " Note that when the 'combined' option is used, the 'GroupLabel' parameter *must* also be"
                           " provided.";
    out.args.back().default_val = "separate";
    out.args.back().expected = true;
    out.args.back().examples = { "separate", "grouped" };
    out.args.back().samples = OpArgSamples::Exhaustive;


    out.args.emplace_back();
    out.args.back().name = "GroupLabel";
    out.args.back().desc = "If the 'Grouping' parameter is set to 'combined', the value of the 'GroupLabel' parameter"
                           " will be used in lieu of any consitituent ROILabel."
                           " Note that this parameter *must* be provided when the 'Grouping' parameter is set to"
                           "'combined'.";
    out.args.back().default_val = "";
    out.args.back().expected = false;
    out.args.back().examples = { "combination", "multiple_rois", "logical_oar", "both_oars" };


    out.args.emplace_back();
    out.args.back().name = "Lower";
    out.args.back().desc = "Disregard all voxel values lower than this value."
                           " This parameter can be used to filter out spurious values."
                           " All voxels with infinite or NaN intensities are excluded regardless of this parameter."
                           " Note that disregarded values will not contribute any volume.";
    out.args.back().default_val = "-inf";
    out.args.back().expected = true;
    out.args.back().examples = { "-inf", "-100.0", "0.0", "1.2", "5.0E23" };


    out.args.emplace_back();
    out.args.back().name = "Upper";
    out.args.back().desc = "Disregard all voxel values greater than this value."
                           " This parameter can be used to filter out spurious values."
                           " All voxels with infinite or NaN intensities are excluded regardless of this parameter."
                           " Note that disregarded values will not contribute any volume.";
    out.args.back().default_val = "inf";
    out.args.back().expected = true;
    out.args.back().examples = { "-100.0", "0.0", "1.2", "5.0E23", "inf" };


    out.args.emplace_back();
    out.args.back().name = "dDose";
    out.args.back().desc = "The (fixed) bin width, in units of dose (DICOM units; nominally Gy)."
                           " Note that this is the *maximum* bin width, in practice bins may be"
                           " smaller to account for slop (i.e., excess caused by the extrema being"
                           " separated by a non-integer number of bins of width $dDose$).";
    out.args.back().default_val = "0.1";
    out.args.back().expected = true;
    out.args.back().examples = { "0.0001", "0.001", "0.01", "5.0", "10", "50" };


    out.args.emplace_back();
    out.args.back().name = "UserComment";
    out.args.back().desc = "A string that will be inserted into the output file which will simplify merging output"
                           " with differing parameters, from different sources, or using sub-selections of the data."
                           " If left empty, the column will be omitted from the output.";
    out.args.back().default_val = "";
    out.args.back().expected = false;
    out.args.back().examples = { "Using XYZ", "Patient treatment plan C" };


    return out;
}



bool ExtractImageHistograms(Drover &DICOM_data,
                              const OperationArgPkg& OptArgs,
                              const std::map<std::string, std::string>& /*InvocationMetadata*/,
                              const std::string& FilenameLex){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();

    const auto Channel = std::stol( OptArgs.getValueStr("Channel").value() );

    const auto ROILabelRegex = OptArgs.getValueStr("ROILabelRegex").value();
    const auto NormalizedROILabelRegex = OptArgs.getValueStr("NormalizedROILabelRegex").value();
    const auto InclusivityStr = OptArgs.getValueStr("Inclusivity").value();
    const auto ContourOverlapStr = OptArgs.getValueStr("ContourOverlap").value();
    const auto GroupingStr = OptArgs.getValueStr("Grouping").value();
    const auto GroupLabelOpt = OptArgs.getValueStr("GroupLabel");

    const auto Lower = std::stod(OptArgs.getValueStr("Lower").value());
    const auto Upper = std::stod(OptArgs.getValueStr("Upper").value());
    const auto dDose = std::stod(OptArgs.getValueStr("dDose").value());

    const auto UserComment = OptArgs.getValueStr("UserComment");

    //-----------------------------------------------------------------------------------------------------------------

    const auto regex_centre = Compile_Regex("^cent.*");
    const auto regex_pci = Compile_Regex("^planar_?c?o?r?n?e?r?s?_?inc?l?u?s?i?v?e?$");
    const auto regex_pce = Compile_Regex("^planar_?c?o?r?n?e?r?s?_?exc?l?u?s?i?v?e?$");

    const auto regex_ignore = Compile_Regex("^ig?n?o?r?e?$");
    const auto regex_honopps = Compile_Regex("^ho?n?o?u?r?_?o?p?p?o?s?i?t?e?_?o?r?i?e?n?t?a?t?i?o?n?s?$");
    const auto regex_cancel = Compile_Regex("^ov?e?r?l?a?p?p?i?n?g?_?c?o?n?t?o?u?r?s?_?c?a?n?c?e?l?s?$");

    const auto regex_separate = Compile_Regex("^se?p?[ea]?r?a?t?e?$");
    const auto regex_combined = Compile_Regex("^co?m?b?i?n?e?d?$");

    Explicator X(FilenameLex);

    if( std::regex_match(GroupingStr, regex_combined) && !GroupLabelOpt ){
        throw std::invalid_argument("A valid 'GroupLabel' must be provided when 'Grouping'='combined'.");
    }
    const bool override_name = std::regex_match(GroupingStr, regex_combined);

    if(DICOM_data.image_data.empty()){
        throw std::invalid_argument("This routine requires at least one image array. Cannot continue");
    }

    //Stuff references to all contours into a list. Remember that you can still address specific contours through
    // the original holding containers (which are not modified here).
    auto cc_all = All_CCs( DICOM_data );
    auto cc_ROIs = Whitelist( cc_all, { { "ROIName", ROILabelRegex },
                                        { "NormalizedROIName", NormalizedROILabelRegex } } );
    if(cc_ROIs.empty()){
        throw std::invalid_argument("No contours selected. Cannot continue.");
    }

    std::string patient_ID;
    if( auto o = cc_ROIs.front().get().contours.front().GetMetadataValueAs<std::string>("PatientID") ){
        patient_ID = o.value();
    }else if( auto o = cc_ROIs.front().get().contours.front().GetMetadataValueAs<std::string>("StudyInstanceUID") ){
        patient_ID = o.value();
    }else{
        patient_ID = "unknown_patient";
    }

    //-----------------------------------------------------------------------------------------------------------------
    auto IAs_all = All_IAs( DICOM_data );
    auto IAs = Whitelist( IAs_all, ImageSelectionStr );
    for(auto & iap_it : IAs){
        ComputeExtractHistogramsUserData ud;

        ud.dDose = dDose;
        ud.channel = Channel;
        ud.lower_threshold = Lower;
        ud.upper_threshold = Upper;

        if( std::regex_match(GroupingStr, regex_separate) ){
            ud.grouping = ComputeExtractHistogramsUserData::GroupingMethod::Separate;
        }else if( std::regex_match(GroupingStr, regex_combined) ){
            ud.grouping = ComputeExtractHistogramsUserData::GroupingMethod::Combined;
        }

        ud.mutation_opts.editstyle = Mutate_Voxels_Opts::EditStyle::InPlace;
        ud.mutation_opts.aggregate = Mutate_Voxels_Opts::Aggregate::First;
        ud.mutation_opts.adjacency = Mutate_Voxels_Opts::Adjacency::SingleVoxel;
        ud.mutation_opts.maskmod   = Mutate_Voxels_Opts::MaskMod::Noop;

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

        if(!(*iap_it)->imagecoll.Compute_Images( ComputeExtractHistograms, { },
                                                   cc_ROIs, &ud )){
            throw std::runtime_error("Unable to extract histograms.");
        }

        //Store the histograms in the Drover class for further analysis.
        for(auto &advh_p : ud.differential_histograms){
            const auto lROIname = (override_name) ? GroupLabelOpt.value() : advh_p.first;
            
            DICOM_data.lsamp_data.emplace_back( std::make_shared<Line_Sample>() );
            auto lsamp_abs_ptr = DICOM_data.lsamp_data.back();

            lsamp_abs_ptr->line.metadata.swap( advh_p.second.metadata );
            lsamp_abs_ptr->line.samples.swap( advh_p.second.samples );

            lsamp_abs_ptr->line.metadata["PatientID"] = patient_ID;
            lsamp_abs_ptr->line.metadata["LineName"] = lROIname;
            lsamp_abs_ptr->line.metadata["ROIName"] = lROIname;
            lsamp_abs_ptr->line.metadata["NormalizedROIName"] = X(lROIname);
            //lsamp_abs_ptr->line.metadata["HistogramType"] = "Differential";
            //lsamp_abs_ptr->line.metadata["AbscissaScaling"] = "None"; // Absolute values.
            //lsamp_abs_ptr->line.metadata["OrdinateScaling"] = "None"; // Absolute values.
            if(UserComment) lsamp_abs_ptr->line.metadata["UserComment"] = UserComment.value();
        }

        for(auto &advh_p : ud.cumulative_histograms){
            const auto lROIname = (override_name) ? GroupLabelOpt.value() : advh_p.first;
            
            DICOM_data.lsamp_data.emplace_back( std::make_shared<Line_Sample>() );
            auto lsamp_abs_ptr = DICOM_data.lsamp_data.back();

            lsamp_abs_ptr->line.metadata.swap( advh_p.second.metadata );
            lsamp_abs_ptr->line.samples.swap( advh_p.second.samples );

            lsamp_abs_ptr->line.metadata["PatientID"] = patient_ID;
            lsamp_abs_ptr->line.metadata["LineName"] = lROIname;
            lsamp_abs_ptr->line.metadata["ROIName"] = lROIname;
            lsamp_abs_ptr->line.metadata["NormalizedROIName"] = X(lROIname);
            //lsamp_abs_ptr->line.metadata["HistogramType"] = "Cumulative";
            //lsamp_abs_ptr->line.metadata["AbscissaScaling"] = "None"; // Absolute values.
            //lsamp_abs_ptr->line.metadata["OrdinateScaling"] = "None"; // Absolute values.
            if(UserComment) lsamp_abs_ptr->line.metadata["UserComment"] = UserComment.value();
        }
    }

    return true;
}
