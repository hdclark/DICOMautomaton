//ExtractDoseVolumeHistograms.cc - A part of DICOMautomaton 2018. Written by hal clark.

#include <boost/interprocess/creation_tags.hpp>
#include <boost/interprocess/sync/named_mutex.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <algorithm>
#include <cstdlib>            //Needed for exit() calls.
#include <exception>
#include <experimental/any>
#include <experimental/optional>
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
//#include "../YgorImages_Functors/Compute/AccumulatePixelDistributions.h"
#include "../YgorImages_Functors/Compute/Extract_Dose_Volume_Histograms.h"
#include "ExtractDoseVolumeHistograms.h"
#include "Explicator.h"       //Needed for Explicator class.
#include "YgorFilesDirs.h"    //Needed for Does_File_Exist_And_Can_Be_Read(...), etc..
#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorStats.h"        //Needed for Stats:: namespace.



OperationDoc OpArgDocExtractDoseVolumeHistograms(void){
    OperationDoc out;
    out.name = "ExtractDoseVolumeHistograms";

    out.desc = 
        " This operation extracts dose-volume histograms (or pixel intensity-volume histograms)"
        " for the selected image(s) and ROI(s)."
        " Results are stored as line samples for later analysis or export.";
        
    out.notes.emplace_back(
        "This routine generates cumulative DVHs with absolute dose on the x-axis and both absolute"
        " and fractional volume on the y-axis. Dose is reported in DICOM units (nominally Gy),"
        " absolute volume is reported in volumetric DICOM units (mm^3^), and relative volume is"
        " reported as a fraction of the given ROI's total volume."
    );
        
    out.notes.emplace_back(
        "This routine will correctly handle logically-related contours that are scattered amongst many"
        " contour collections, re-partitioning them based on ROIName. While this is often the desired"
        " behaviour, beware that any user-specified partitions will be overridden."
    );
        
    out.notes.emplace_back(
        "This routine will correctly handle voxels of different volumes. It will not correctly handle"
        " overlapping voxels (i.e., each overlapping voxel will be counted without regard for overlap)."
        " If necessary, resample image arrays to be rectilinear."
    );
        
    out.notes.emplace_back(
        "This routine will combine spatially-overlapping images by summing voxel intensities. It will not"
        " combine separate image_arrays. If needed, you'll have to perform a meld on them beforehand."
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
    out.args.back().name = "NormalizedROILabelRegex";
    out.args.back().desc = "A regex matching the ROI labels/names to consider. The default will match"
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
    out.args.back().desc = "A regex matching the ROI labels/names to consider. The default will match"
                           " all available ROIs. Be aware that input spaces are trimmed to a single space."
                           " If your ROI name has more than two sequential spaces, use regex to avoid them."
                           " All ROIs have to match the single regex, so use the 'or' token if needed."
                           " Regex is case insensitive and uses extended POSIX syntax.";
    out.args.back().default_val = ".*";
    out.args.back().expected = true;
    out.args.back().examples = { ".*", ".*body.*", "body", "Gross_Liver",
                            R"***(.*left.*parotid.*|.*right.*parotid.*|.*eyes.*)***",
                            R"***(left_parotid|right_parotid)***" };


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
    out.args.back().name = "dDose";
    out.args.back().desc = "The (fixed) bin width, in units of dose (DICOM units; nominally Gy).";
    out.args.back().default_val = "1.0";
    out.args.back().expected = true;
    out.args.back().examples = { "0.1", "0.5", "2.0", "5.0", "10", "50" };


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



Drover ExtractDoseVolumeHistograms(Drover DICOM_data, OperationArgPkg OptArgs, std::map<std::string,std::string> /*InvocationMetadata*/, std::string FilenameLex){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();

    const auto Channel = std::stol( OptArgs.getValueStr("Channel").value() );

    const auto ROILabelRegex = OptArgs.getValueStr("ROILabelRegex").value();
    const auto NormalizedROILabelRegex = OptArgs.getValueStr("NormalizedROILabelRegex").value();
    const auto InclusivityStr = OptArgs.getValueStr("Inclusivity").value();
    const auto ContourOverlapStr = OptArgs.getValueStr("ContourOverlap").value();

    const auto dDose = std::stod(OptArgs.getValueStr("dDose").value());

    const auto UserComment = OptArgs.getValueStr("UserComment");

    //-----------------------------------------------------------------------------------------------------------------

    const auto regex_centre = Compile_Regex("^cent.*");
    const auto regex_pci = Compile_Regex("^planar_?c?o?r?n?e?r?s?_?inc?l?u?s?i?v?e?$");
    const auto regex_pce = Compile_Regex("^planar_?c?o?r?n?e?r?s?_?exc?l?u?s?i?v?e?$");

    const auto regex_ignore = Compile_Regex("^ig?n?o?r?e?$");
    const auto regex_honopps = Compile_Regex("^ho?n?o?u?r?_?o?p?p?o?s?i?t?e?_?o?r?i?e?n?t?a?t?i?o?n?s?$");
    const auto regex_cancel = Compile_Regex("^ov?e?r?l?a?p?p?i?n?g?_?c?o?n?t?o?u?r?s?_?c?a?n?c?e?l?s?$");

    Explicator X(FilenameLex);


    //Merge the image arrays if necessary.
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
        ComputeExtractDoseVolumeHistogramsUserData ud;

        ud.dDose = dDose;
        ud.channel = Channel;

        ud.mutation_opts.editstyle = Mutate_Voxels_Opts::EditStyle::InPlace;
        ud.mutation_opts.aggregate = Mutate_Voxels_Opts::Aggregate::First;
        ud.mutation_opts.adjacency = Mutate_Voxels_Opts::Adjacency::SingleVoxel;
        ud.mutation_opts.maskmod   = Mutate_Voxels_Opts::MaskMod::Noop;

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

        if(!(*iap_it)->imagecoll.Compute_Images( ComputeExtractDoseVolumeHistograms, { },
                                                   cc_ROIs, &ud )){
            throw std::runtime_error("Unable to extract DVHs.");
        }

        //Store the DVHs in the Drover class for further analysis.
        for(const auto &advh_p : ud.dvhs){
            // The absolute volume variant.
            DICOM_data.lsamp_data.emplace_back( std::make_shared<Line_Sample>() );
            auto lsamp_abs_ptr = DICOM_data.lsamp_data.back();

            const auto lROIname = advh_p.first;
            const bool inhibit_sort = true;

            lsamp_abs_ptr->line.metadata["PatientID"] = patient_ID;
            lsamp_abs_ptr->line.metadata["LineName"] = lROIname;
            lsamp_abs_ptr->line.metadata["Modality"] = "DVH";
            lsamp_abs_ptr->line.metadata["DVHType"] = "Cumulative";
            lsamp_abs_ptr->line.metadata["DoseScaling"] = "None"; // Absolute values.
            lsamp_abs_ptr->line.metadata["VolumeScaling"] = "None"; // Absolute values.
            lsamp_abs_ptr->line.metadata["ROIName"] = lROIname;
            lsamp_abs_ptr->line.metadata["NormalizedROIName"] = X(lROIname);
            lsamp_abs_ptr->line.metadata["DifferentialDVHMinDose"]  = std::to_string(ud.min_dose[advh_p.first]);
            lsamp_abs_ptr->line.metadata["DifferentialDVHMeanDose"] = std::to_string(ud.mean_dose[advh_p.first]);
            lsamp_abs_ptr->line.metadata["DifferentialDVHMaxDose"]  = std::to_string(ud.max_dose[advh_p.first]);
            if(UserComment){
                lsamp_abs_ptr->line.metadata["UserComment"] = UserComment.value();
            }

            for(const auto &m_p : advh_p.second){
                const auto dose = m_p.first;
                const auto vol_abs = m_p.second.first;
                lsamp_abs_ptr->line.push_back(dose, vol_abs, inhibit_sort);
            }
            lsamp_abs_ptr->line.stable_sort();

            // The relative volume variant.
            DICOM_data.lsamp_data.emplace_back( std::make_shared<Line_Sample>() );
            auto lsamp_rel_ptr = DICOM_data.lsamp_data.back();

            *lsamp_rel_ptr = *lsamp_abs_ptr;
            lsamp_rel_ptr->line.metadata["VolumeScaling"] = "Normalized";

            lsamp_rel_ptr->line.samples.clear();
            for(const auto &m_p : advh_p.second){
                const auto dose = m_p.first;
                const auto vol_rel = m_p.second.second;
                lsamp_rel_ptr->line.push_back(dose, vol_rel, inhibit_sort);
            }
            lsamp_rel_ptr->line.stable_sort();
        }
    }

    return DICOM_data;
}
