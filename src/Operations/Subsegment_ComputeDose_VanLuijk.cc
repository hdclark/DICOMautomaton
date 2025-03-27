//Subsegment_ComputeDose_VanLuijk.cc - A part of DICOMautomaton 2015, 2016. Written by hal clark.

#include <boost/interprocess/creation_tags.hpp>
#include <boost/interprocess/sync/named_mutex.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <cmath>
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
#include <filesystem>

#include "../Dose_Meld.h"
#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../YgorImages_Functors/Compute/AccumulatePixelDistributions.h"
#include "Explicator.h"       //Needed for Explicator class.
#include "Subsegment_ComputeDose_VanLuijk.h"
#include "YgorFilesDirs.h"    //Needed for Does_File_Exist_And_Can_Be_Read(...), etc..
#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorLog.h"
#include "YgorStats.h"        //Needed for Stats:: namespace.
#include "YgorString.h"       //Needed for GetFirstRegex(...)



OperationDoc OpArgDocSubsegment_ComputeDose_VanLuijk(){
    OperationDoc out;
    out.name = "Subsegment_ComputeDose_VanLuijk";
    out.tags.emplace_back("category: contour processing");

    out.desc = "This operation sub-segments the selected ROI(s) and computes dose within the resulting sub-segments.";


    out.args.emplace_back();
    out.args.back().name = "AreaDataFileName";
    out.args.back().desc = "A filename (or full path) in which to append sub-segment areaa data generated by this routine."
                      " The format is CSV. Note that if a sub-segment has zero area or does not exist, no area will"
                      " be printed. You'll have to manually add sub-segments with zero area as needed if this info"
                      " is relevant to you (e.g., if you are deriving a population average)."
                      " Leave empty to NOT dump anything.";
    out.args.back().default_val = "";
    out.args.back().expected = true;
    out.args.back().examples = { "", "/tmp/somefile", "localfile.csv", "area_data.csv" };
    out.args.back().mimetype = "text/csv";


    out.args.emplace_back();
    out.args.back().name = "DerivativeDataFileName";
    out.args.back().desc = "A filename (or full path) in which to append derivative data generated by this routine."
                      " The format is CSV. Leave empty to dump to generate a unique temporary file.";
    out.args.back().default_val = "";
    out.args.back().expected = true;
    out.args.back().examples = { "", "/tmp/somefile", "localfile.csv", "derivative_data.csv" };
    out.args.back().mimetype = "text/csv";


    out.args.emplace_back();
    out.args.back().name = "DistributionDataFileName";
    out.args.back().desc = "A filename (or full path) in which to append raw distribution data generated by this routine."
                      " The format is one line of description followed by one line for the distribution;"
                      " pixel intensities are listed with a single space between elements; the descriptions contain"
                      " the patient ID, ROIName, and subsegment description (guaranteed) and possibly various other"
                      " data afterward. Leave empty to NOT dump anything.";
    out.args.back().default_val = "";
    out.args.back().expected = true;
    out.args.back().examples = { "", "/tmp/somefile", "localfile.csv", "distributions.data" };
    out.args.back().mimetype = "text/csv";


    out.args.emplace_back();
    out.args.back() = NCWhitelistOpArgDoc();
    out.args.back().name = "NormalizedROILabelRegex";
    out.args.back().default_val = ".*";

    out.args.emplace_back();
    out.args.back().name = "PlanarOrientation";
    out.args.back().desc = "A string instructing how to orient the cleaving planes."
                      " Currently only 'AxisAligned' (i.e., align with the image/dose grid row and column"
                      " unit vectors) and 'StaticOblique' (i.e., same as AxisAligned but rotated 22.5 degrees"
                      " to reduce colinearity, which sometimes improves sub-segment area consistency).";
    out.args.back().default_val = "AxisAligned";
    out.args.back().expected = true;
    out.args.back().examples = { "AxisAligned", "StaticOblique" };
    out.args.back().samples = OpArgSamples::Exhaustive;


    out.args.emplace_back();
    out.args.back().name = "ReplaceAllWithSubsegment";
    out.args.back().desc = "Keep the sub-segment and remove any existing contours from the original ROIs."
                      " This is most useful for further processing, such as nested sub-segmentation."
                      " Note that sub-segment contours currently have identical metadata to their"
                      " parent contours.";
    out.args.back().default_val = "false";
    out.args.back().expected = true;
    out.args.back().examples = { "true", "false" };


    out.args.emplace_back();
    out.args.back().name = "RetainSubsegment";
    out.args.back().desc = "Keep the sub-segment as part of the original ROIs. The contours are appended to"
                      " the original ROIs, but the contour ROIName and NormalizedROIName are set to the"
                      " argument provided. (If no argument is provided, sub-segments are not retained.)"
                      " This is most useful for inspection of sub-segments. Note"
                      " that sub-segment contours currently have identical metadata to their"
                      " parent contours, except they are renamed accordingly.";
    out.args.back().default_val = "";
    out.args.back().expected = true;
    out.args.back().examples = { "subsegment_01", "subsegment_02", "selected_subsegment" };


    out.args.emplace_back();
    out.args.back() = RCWhitelistOpArgDoc();
    out.args.back().name = "ROILabelRegex";
    out.args.back().default_val = ".*";

    out.args.emplace_back();
    out.args.back() = CCWhitelistOpArgDoc();
    out.args.back().name = "ROISelection";
    out.args.back().default_val = "all";

    out.args.emplace_back();
    out.args.back().name = "SubsegMethod";
    out.args.back().desc = "The method to use for sub-segmentation. Nested sub-segmentation should almost"
                      " always be preferred unless you know what you're doing. It should be faster too."
                      " The compound method was used in the van Luijk paper, but it is known to have"
                      " serious problems.";
    out.args.back().default_val = "nested";
    out.args.back().expected = true;
    out.args.back().examples = { "nested", "compound" };
    out.args.back().samples = OpArgSamples::Exhaustive;


    out.args.emplace_back();
    out.args.back().name = "XSelection";
    out.args.back().desc = "(See ZSelection description.) The \"X\" direction is defined in terms of movement"
                      " on an image when the row number increases. This is generally VERTICAL and DOWNWARD."
                      " All selections are defined in terms of the original ROIs.";
    out.args.back().default_val = "1.0;0.0";
    out.args.back().expected = true;
    out.args.back().examples = { "0.50;0.50", "0.50;0.0", "0.30;0.0", "0.30;0.70" };


    out.args.emplace_back();
    out.args.back().name = "YSelection";
    out.args.back().desc = "(See ZSelection description.) The \"Y\" direction is defined in terms of movement"
                      " on an image when the column number increases. This is generally HORIZONTAL and RIGHTWARD."
                      " All selections are defined in terms of the original ROIs.";
    out.args.back().default_val = "1.0;0.0";
    out.args.back().expected = true;
    out.args.back().examples = { "0.50;0.50", "0.50;0.0", "0.30;0.0", "0.30;0.70" };

    
    out.args.emplace_back();
    out.args.back().name = "ZSelection";
    out.args.back().desc = "The thickness and offset defining the single, continuous extent of the sub-segmentation in"
                      " terms of the fractional area remaining above a plane. The planes define the portion extracted"
                      " and are determined such that sub-segmentation will give the desired fractional planar areas."
                      " The numbers specify the thickness and offset from the bottom of the ROI volume to the bottom"
                      " of the extent. The 'upper' direction is take from the contour plane orientation and assumed to"
                      " be positive if pointing toward the positive-z direction. Only a single 3D selection can be made"
                      " per operation invocation. Sub-segmentation can be performed in transverse (\"Z\"), row_unit"
                      " (\"X\"), and column_unit (\"Y\") directions (in that order)."
                      " All selections are defined in terms of the original ROIs."
                      " Note that it is possible to perform nested sub-segmentation"
                      " (including passing along the original contours) by opting to"
                      " replace the original ROI contours with this sub-segmentation and invoking this operation"
                      " again with the desired sub-segmentation."
                      " If you want the middle 50% of an ROI, specify '0.50;0.25'."
                      " If you want the upper 50% then specify '0.50;0.50'."
                      " If you want the lower 50% then specify '0.50;0.0'."
                      " If you want the upper 30% then specify '0.30;0.70'."
                      " If you want the lower 30% then specify '0.30;0.70'.";
    out.args.back().default_val = "1.0;0.0";
    out.args.back().expected = true;
    out.args.back().examples = { "0.50;0.50", "0.50;0.0", "0.30;0.0", "0.30;0.70" };


    out.args.emplace_back();
    out.args.back().name = "FractionalTolerance";
    out.args.back().desc = "The tolerance of X, Y, and Z fractional area bisection criteria"
                           " (see ZSelection description)."
                           " This parameter specifies a stopping condition for the bisection procedure."
                           " If it is set too high, sub-segments may be inadequatly rough."
                           " If it is set too low, bisection below the machine precision floor may be attempted,"
                           " which will result in instabilities."
                           " Note that the number of permitted iterations will control whether this tolerance"
                           " can possibly be reached; if strict adherence is required, set the maximum number"
                           " of iterations to be excessively large.";
    out.args.back().default_val = "0.001";
    out.args.back().expected = true;
    out.args.back().examples = { "1E-2", "1E-3", "1E-4", "1E-5" };
    


    out.args.emplace_back();
    out.args.back().name = "MaxBisects";
    out.args.back().desc = "The maximum number of iterations the bisection procedure can perform."
                           " This parameter specifies a stopping condition for the bisection procedure."
                           " If it is set too low, sub-segments may be inadequatly rough."
                           " If it is set too high, bisection below the machine precision floor may be attempted,"
                           " which will result in instabilities."
                           " Note that the fractional tolerance will control whether this tolerance"
                           " can possibly be reached; if an exact number of iterations is required, set"
                           " the fractional tolerance to be excessively small.";
    out.args.back().default_val = "20";
    out.args.back().expected = true;
    out.args.back().examples = { "10", "20", "30" };

    return out;
}



bool Subsegment_ComputeDose_VanLuijk(Drover &DICOM_data,
                                       const OperationArgPkg& OptArgs,
                                       std::map<std::string, std::string>& /*InvocationMetadata*/,
                                       const std::string& FilenameLex){

    //---------------------------------------------- User Parameters --------------------------------------------------
    auto AreaDataFileName = OptArgs.getValueStr("AreaDataFileName").value();
    auto DerivativeDataFileName = OptArgs.getValueStr("DerivativeDataFileName").value();
    auto DistributionDataFileName = OptArgs.getValueStr("DistributionDataFileName").value();
    const auto PlanarOrientation = OptArgs.getValueStr("PlanarOrientation").value();
    const auto ReplaceAllWithSubsegmentStr = OptArgs.getValueStr("ReplaceAllWithSubsegment").value();
    const auto RetainSubsegment = OptArgs.getValueStr("RetainSubsegment").value();
    const auto ROILabelRegex = OptArgs.getValueStr("ROILabelRegex").value();
    const auto ROISelection = OptArgs.getValueStr("ROISelection").value();
    const auto NormalizedROILabelRegex = OptArgs.getValueStr("NormalizedROILabelRegex").value();
    const auto SubsegMethodReq = OptArgs.getValueStr("SubsegMethod").value();
    const auto XSelectionStr = OptArgs.getValueStr("XSelection").value();
    const auto YSelectionStr = OptArgs.getValueStr("YSelection").value();
    const auto ZSelectionStr = OptArgs.getValueStr("ZSelection").value();

    const auto FractionalTolerance = std::stod( OptArgs.getValueStr("FractionalTolerance").value() );
    const auto MaxBisects = std::stol( OptArgs.getValueStr("MaxBisects").value() );

    //-----------------------------------------------------------------------------------------------------------------
    const auto theregex = Compile_Regex(ROILabelRegex);
    const auto TrueRegex = Compile_Regex("^tr?u?e?$");
    const auto thenormalizedregex = Compile_Regex(NormalizedROILabelRegex);
    const auto SubsegMethodCompound = Compile_Regex("Compound");
    const auto SubsegMethodNested = Compile_Regex("Nested");

    const auto OrientAxisAligned = Compile_Regex("AxisAligned");
    const auto OrientStaticObl = Compile_Regex("StaticOblique");

    const auto ReplaceAllWithSubsegment = std::regex_match(ReplaceAllWithSubsegmentStr, TrueRegex);

    const auto XSelectionTokens = SplitStringToVector(XSelectionStr, ';', 'd');
    const auto YSelectionTokens = SplitStringToVector(YSelectionStr, ';', 'd');
    const auto ZSelectionTokens = SplitStringToVector(ZSelectionStr, ';', 'd');
    if( (XSelectionTokens.size() != 2) || (YSelectionTokens.size() != 2) || (ZSelectionTokens.size() != 2) ){
        throw std::invalid_argument("The spatial extent selections must consist of exactly two numbers. Cannot continue.");
    }
    const double XSelectionThickness = std::stod(XSelectionTokens.front());
    const double XSelectionOffsetFromBottom = std::stod(XSelectionTokens.back());
    const double YSelectionThickness = std::stod(YSelectionTokens.front());
    const double YSelectionOffsetFromBottom = std::stod(YSelectionTokens.back());
    const double ZSelectionThickness = std::stod(ZSelectionTokens.front());
    const double ZSelectionOffsetFromBottom = std::stod(ZSelectionTokens.back());

    //The bisection routine requires for input the fractional area above the plane. We convert from (thickness,offset)
    // units to the fractional area above the plane for both upper and lower extents.
    const double XSelectionLower = 1.0 - XSelectionOffsetFromBottom;
    const double XSelectionUpper = 1.0 - XSelectionOffsetFromBottom - XSelectionThickness;
    const double YSelectionLower = 1.0 - YSelectionOffsetFromBottom;
    const double YSelectionUpper = 1.0 - YSelectionOffsetFromBottom - YSelectionThickness;
    const double ZSelectionLower = 1.0 - ZSelectionOffsetFromBottom;
    const double ZSelectionUpper = 1.0 - ZSelectionOffsetFromBottom - ZSelectionThickness;

    if(!isininc(0.0,XSelectionLower,1.0) || !isininc(0.0,XSelectionUpper,1.0)){
        YLOGWARN("XSelection is not valid. The selection exceeds [0,1]. Lower and Upper are "
                 << XSelectionLower << " and " << XSelectionUpper << " respectively");
    }else if(!isininc(0.0,YSelectionLower,1.0) || !isininc(0.0,YSelectionUpper,1.0)){
        YLOGWARN("YSelection is not valid. The selection exceeds [0,1]. Lower and Upper are "
                 << YSelectionLower << " and " << YSelectionUpper << " respectively");
    }else if(!isininc(0.0,ZSelectionLower,1.0) || !isininc(0.0,ZSelectionUpper,1.0)){
        YLOGWARN("ZSelection is not valid. The selection exceeds [0,1]. Lower and Upper are "
                 << ZSelectionLower << " and " << ZSelectionUpper << " respectively");
    }

    Explicator X(FilenameLex);

    //Merge the dose arrays if multiple are available.
    DICOM_data = Meld_Only_Dose_Data(DICOM_data);

    //Gather only dose images.
    auto IAs_all = All_IAs( DICOM_data );
    auto IAs = Whitelist( IAs_all, "Modality@RTDOSE" );
    if(IAs.empty()){
        throw std::invalid_argument("No dose arrays selected. Cannot continue.");
    }
    if(IAs.size() != 1){
        throw std::invalid_argument("Unable to meld images into a single image array. Cannot continue.");
    }
    auto img_arr_ptr = (*(IAs.front()));
    if(img_arr_ptr == nullptr){
        throw std::runtime_error("Encountered a nullptr when expecting a valid Image_Array ptr.");
    }else if(img_arr_ptr->imagecoll.images.empty()){
        throw std::runtime_error("Encountered a Image_Array with valid images -- no images found.");
    }


    //Stuff references to all contours into a list. Remember that you can still address specific contours through
    // the original holding containers (which are not modified here).
    auto cc_all = All_CCs( DICOM_data );
    auto cc_ROIs = Whitelist( cc_all, ROILabelRegex, NormalizedROILabelRegex, ROISelection );

    if(cc_ROIs.empty()){
        throw std::invalid_argument("No contours selected. Cannot continue.");
    }

    const auto patient_ID = cc_ROIs.front().get().contours.front().GetMetadataValueAs<std::string>("PatientID").value();

    //Get the plane in which the contours are defined on. Since we require a dose array for this routine, and the dose
    // slices and contours must align to make sense, we can pull orientation normals from the slices. Alternatively we
    // can pull an 
    //
    // Note: the planar orientation will point to the next image, not the 'upward' direction. We cannot guarantee the
    // subject will be properly oriented, so the safest thing to do is ensure all patients share the same orientation
    // and then somehow verify (e.g., by inspection) that the sub-segments you think you're selecting are actually being
    // selected. Another idea would be using spatial relationships between known anatomical structures, or using the
    // expected shape to compare ROI slices to figure out orientation. 
    //const auto z_normal = img_arr_ptr->imagecoll.images.front().image_plane().N_0 * -1.0;
    //const auto x_normal = img_arr_ptr->imagecoll.images.front().row_unit;
    //const auto y_normal = img_arr_ptr->imagecoll.images.front().col_unit;

    // Image-axes aligned normals.
    const auto row_normal = img_arr_ptr->imagecoll.images.front().row_unit;
    const auto col_normal = img_arr_ptr->imagecoll.images.front().col_unit;
    const auto ort_normal = img_arr_ptr->imagecoll.images.front().image_plane().N_0 * -1.0;

    vec3<double> x_normal;
    vec3<double> y_normal;
    vec3<double> z_normal;

    // Use the image-axes aligned normals directly. Sub-segmentation might get snagged on voxel rows or columns.
    if(std::regex_match(PlanarOrientation, OrientAxisAligned)){
        x_normal = col_normal;
        y_normal = row_normal;
        z_normal = ort_normal;

    // Try to offset the axes slightly so they don't align perfectly with the voxel grid. (Align along the row and
    // column directions, or align along the diagonals, which can be just as bad.)
    }else if(std::regex_match(PlanarOrientation, OrientStaticObl)){
        x_normal = (col_normal - ort_normal * 0.5).unit();
        y_normal = (row_normal - col_normal * 0.5).unit();
        z_normal = (ort_normal - row_normal * 0.5).unit();
        z_normal.GramSchmidt_orthogonalize(x_normal, y_normal);
        x_normal = x_normal.unit();
        y_normal = y_normal.unit();
        z_normal = z_normal.unit();

    }else{
        throw std::invalid_argument("Planar orientations not understood. Cannot continue.");
    }

    //This routine returns a pair of planes that approximately encompass the desired interior volume. The ROIs are not
    // altered. The lower plane is the first element of the pair. This routine can be applied to any contour_collection
    // and the planes can also be applied to any contour_collection.
    const auto bisect_ROIs = [FractionalTolerance,MaxBisects](
                                 const contour_collection<double> &ROIs,
                                 const vec3<double> &planar_normal,
                                 double SelectionLower,
                                 double SelectionUpper) -> 
                                     std::pair<plane<double>, plane<double>> {

        if(ROIs.contours.empty()) throw std::logic_error("Unable to split empty contour collection.");
        size_t iters_taken = 0;
        double final_area_frac = 0.0;

        //Find the lower plane.
        plane<double> lower_plane;
        ROIs.Total_Area_Bisection_Along_Plane(planar_normal,
                                                      SelectionLower,
                                                      FractionalTolerance,
                                                      MaxBisects,
                                                      &lower_plane,
                                                      &iters_taken,
                                                      &final_area_frac);
        YLOGINFO("Bisection: planar area fraction"
                 << " above LOWER plane with normal: " << planar_normal
                 << " was " << final_area_frac << "."
                 << " Requested: " << SelectionLower << "."
                 << " Iters: " << iters_taken);

        //Find the upper plane.
        plane<double> upper_plane;
        ROIs.Total_Area_Bisection_Along_Plane(planar_normal,
                                                      SelectionUpper,
                                                      FractionalTolerance,
                                                      MaxBisects,
                                                      &upper_plane,
                                                      &iters_taken,
                                                      &final_area_frac);
        YLOGINFO("Bisection: planar area fraction"
                 << " above UPPER plane with normal: " << planar_normal
                 << " was " << final_area_frac << "."
                 << " Requested: " << SelectionUpper << "."
                 << " Iters: " << iters_taken);

        return std::make_pair(lower_plane, upper_plane);
    };

    const auto subsegment_interior = [](const contour_collection<double> &ROIs,
                                        const std::pair<plane<double>, plane<double>> &planes) ->
                                            contour_collection<double> {
        const plane<double> lower_plane(planes.first);
        const plane<double> upper_plane(planes.second);

        //Implements the sub-segmentation, selecting only the interior portion.
        auto split1 = ROIs.Split_Along_Plane(lower_plane);
        if(split1.size() != 2){
            throw std::logic_error("Expected exactly two groups, above and below plane.");
        }
        auto split2 = split1.back().Split_Along_Plane(upper_plane);
        if(split2.size() != 2){
            throw std::logic_error("Expected exactly two groups, above and below plane.");
        }

        if(false) for(auto & it : split2){ it.Plot(); }

        const contour_collection<double> cc_selection( split2.front() );
        if( cc_selection.contours.empty() ){
            YLOGWARN("Selection contains no contours. Try adjusting your criteria.");
        }
        return cc_selection;
    };

    //Perform the sub-segmentation.
    std::list<contour_collection<double>> cc_selection;
    for(const auto &cc_ref : cc_ROIs){
        if(cc_ref.get().contours.empty()) continue;

        // ---------------------------------- Compound sub-segmentation --------------------------------------
        //Generate all planes using the original contour_collection before sub-segmenting.
        //
        // NOTE: This method results in sub-segments of different volumes depending on the location within the ROI.
        //       Do not use this method unless you know what you're doing.
        if( std::regex_match(SubsegMethodReq,SubsegMethodCompound) ){
            const auto x_planes_pair = bisect_ROIs(cc_ref.get(), x_normal, XSelectionLower, XSelectionUpper);
            const auto y_planes_pair = bisect_ROIs(cc_ref.get(), y_normal, YSelectionLower, YSelectionUpper);
            const auto z_planes_pair = bisect_ROIs(cc_ref.get(), z_normal, ZSelectionLower, ZSelectionUpper);

            //Perform the sub-segmentation.
            contour_collection<double> running(cc_ref.get());
            running = subsegment_interior(running, x_planes_pair);
            running = subsegment_interior(running, y_planes_pair);
            running = subsegment_interior(running, z_planes_pair);
            cc_selection.emplace_back(running);

        // ----------------------------------- Nested sub-segmentation ---------------------------------------
        // Instead of relying on whole-organ sub-segmentation, attempt to fairly partition the *remaining* volume 
        // at each pair of cleaves.
        //
        // NOTE: This method will generate sub-segments with equal volumes (as best possible given the number of slices
        //       if the plane orientations are aligned with the contour planes) and should be preferred over compound
        //       sub-segmentation in almost all cases. It should be faster too.
        }else if( std::regex_match(SubsegMethodReq,SubsegMethodNested) ){
            contour_collection<double> running(cc_ref.get());

            const auto z_planes_pair = bisect_ROIs(running, z_normal, ZSelectionLower, ZSelectionUpper);
            running = subsegment_interior(running, z_planes_pair);

            const auto x_planes_pair = bisect_ROIs(running, x_normal, XSelectionLower, XSelectionUpper);
            running = subsegment_interior(running, x_planes_pair);

            const auto y_planes_pair = bisect_ROIs(running, y_normal, YSelectionLower, YSelectionUpper);
            running = subsegment_interior(running, y_planes_pair);

            cc_selection.emplace_back(running);

        }else{
            throw std::invalid_argument("Subsegmentation method not understood. Cannot continue.");
        }
    }

    //Generate references.
    decltype(cc_ROIs) final_selected_ROI_refs;
    for(auto &cc : cc_selection) final_selected_ROI_refs.push_back( std::ref(cc) );

    //Accumulate the voxel intensity distributions.
    AccumulatePixelDistributionsUserData ud;
    if(!img_arr_ptr->imagecoll.Compute_Images( AccumulatePixelDistributions, { },
                                           final_selected_ROI_refs, &ud )){
        throw std::runtime_error("Unable to accumulate pixel distributions.");
    }

    //Report the findings.
    {
        //Try open a named mutex. Probably created in /dev/shm/ if you need to clear it manually...
        boost::interprocess::named_mutex mutex(boost::interprocess::open_or_create,
                                               "dcma_op_van_luijk_subsegmentation_mutex");
        boost::interprocess::scoped_lock<boost::interprocess::named_mutex> lock(mutex);

        if(DerivativeDataFileName.empty()){
            const auto base = std::filesystem::temp_directory_path() / "dcma_subsegment_vanluijk_derivatives_";
            DerivativeDataFileName = Get_Unique_Sequential_Filename(base.string(), 6, ".csv");
        }
        std::fstream FO_deriv(DerivativeDataFileName, std::fstream::out | std::fstream::app);
        if(!FO_deriv){
            throw std::runtime_error("Unable to open file for reporting derivative data. Cannot continue.");
        }
        for(const auto &av : ud.accumulated_voxels){
            const auto lROIname = av.first;
            const auto MeanDose = Stats::Mean( av.second );
            const auto MedianDose = Stats::Median( av.second );

            FO_deriv  << "PatientID='" << patient_ID << "',"
                      << "NormalizedROIname='" << X(lROIname) << "',"
                      << "ROIname='" << lROIname << "',"
                      << "MeanDose=" << MeanDose << ","
                      << "MedianDose=" << MedianDose << ","
                      << "VoxelCount=" << av.second.size() << std::endl;
        }
        FO_deriv.flush();
        FO_deriv.close();
     
        if(!AreaDataFileName.empty()){
            std::fstream FO_area(AreaDataFileName, std::fstream::out | std::fstream::app);
            if(!FO_area){
                throw std::runtime_error("Unable to open file for reporting area data. Cannot continue.");
            }

            std::map<std::string, double> Area;

            for(const auto &cc_ref : final_selected_ROI_refs){
                for(const auto &c : cc_ref.get().contours){
                    const auto lROIname = c.GetMetadataValueAs<std::string>("ROIName").value();
                    Area[X(lROIname)] += std::abs(c.Get_Signed_Area());
                }
            }

            for(const auto& apair : Area){
                FO_area << "PatientID='" << patient_ID << "',"
                        << "NormalizedROIname='" << apair.first << "',"
                        << "Area='" << apair.second << "'" << std::endl;
            }
            FO_area.flush();
            FO_area.close();
        }


        if(!DistributionDataFileName.empty()){
            std::fstream FO_dist(DistributionDataFileName, std::fstream::out | std::fstream::app);
            if(!FO_dist){
                throw std::runtime_error("Unable to open file for reporting distribution data. Cannot continue.");
            }

            for(const auto &av : ud.accumulated_voxels){
                const auto lROIname = av.first;
                FO_dist << "PatientID='" << patient_ID << "' "
                        << "NormalizedROIname='" << X(lROIname) << "' "
                        << "ROIname='" << lROIname << "' " << std::endl;
                for(const auto &d : av.second) FO_dist << d << " ";
                FO_dist << std::endl;
            }
            FO_dist.flush();
            FO_dist.close();
        }
    }

    //Keep the sub-segment if the user wants it.
    if( !RetainSubsegment.empty() ){
        for(auto &cc : final_selected_ROI_refs){
            cc.get().Insert_Metadata("ROIName", RetainSubsegment);
            cc.get().Insert_Metadata("NormalizedROIName", RetainSubsegment);
            DICOM_data.Ensure_Contour_Data_Allocated();
            DICOM_data.contour_data->ccs.emplace_back( cc.get() );
        }
    }
    if(ReplaceAllWithSubsegment){
        DICOM_data.Ensure_Contour_Data_Allocated();
        DICOM_data.contour_data->ccs.clear();
        for(auto &cc : final_selected_ROI_refs){
            DICOM_data.contour_data->ccs.emplace_back( cc.get() );
        }
    }

    return true;
}
