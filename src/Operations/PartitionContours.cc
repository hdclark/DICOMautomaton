//PartitionContours.cc - A part of DICOMautomaton 2019. Written by hal clark.

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
#include <numeric>

#include "Explicator.h"       //Needed for Explicator class.

#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorLog.h"
#include "YgorStats.h"        //Needed for Stats:: namespace.
#include "YgorString.h"       //Needed for GetFirstRegex(...)
#include "YgorFilesDirs.h"    //Needed for Does_File_Exist_And_Can_Be_Read(...), etc..
#include "YgorImages.h"

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "PartitionContours.h"

OperationDoc OpArgDocPartitionContours(){
    OperationDoc out;
    out.name = "PartitionContours";
    out.desc = "This operation partitions the selected contours, producing a number of sub-segments that"
               " could be re-combined to re-create the original contours.";


    out.args.emplace_back();
    out.args.back() = RCWhitelistOpArgDoc();
    out.args.back().name = "ROILabelRegex";
    out.args.back().default_val = ".*";

    out.args.emplace_back();
    out.args.back() = NCWhitelistOpArgDoc();
    out.args.back().name = "NormalizedROILabelRegex";
    out.args.back().default_val = ".*";

    out.args.emplace_back();
    out.args.back().name = "PlanarOrientation";
    out.args.back().desc = "A string instructing how to orient the cleaving planes."
                           " Currently supported:"
                           "\n\n"
                           "1. 'cardinal', which aligns the planes with the cardinal direction axes "
                           " unit vectors. This method is most consistent, but does not adapt to the"
                           " anatomy of the subject. It works best when the subject's contours are"
                           " defined on axial slices in HFS position or have otherwise been"
                           " transferred or transformed to this alignment."
                           "\n\n"
                           "2. 'axis-aligned', which aligns the cleave plane's Z axis with the average"
                           " contour normal and uses a Gram-Schmidt process to provide best-guesses for"
                           " appropriate X and Y unit vectors. This method adapts to the subject's position"
                           " but because the contour planes and the cleaving planes are coplanar, it can"
                           " result in splitting difficulties (e.g., degeneracies, numerical precision"
                           " issues like contours being split into many thin, jagged sub-segments)."
                           " If using this method, consider limiting the number of iterations and fractional"
                           " tolerance to help minimize risk of numerical issues."
                           "\n\n"
                           "3. 'static-oblique', which is the same as 'axis-aligned', but rotates all"
                           " unit vectors by 22.5 degrees to reduce coplanarity of the contour plane and"
                           " the cleaving plane. This sometimes improves sub-segment area consistency,"
                           " but results in oblique sub-segments.";
    out.args.back().default_val = "axis-aligned";
    out.args.back().expected = true;
    out.args.back().examples = { "cardinal", "axis-aligned", "static-oblique" };
    out.args.back().samples = OpArgSamples::Exhaustive;


    out.args.emplace_back();
    out.args.back().name = "SubsegmentRootROILabel";
    out.args.back().desc = "The root ROI label to attach to the sub-segments."
                           " The full name will be this root followed by '_' and the number of the subsegment.";
    out.args.back().default_val = "subsegment";
    out.args.back().expected = true;
    out.args.back().examples = { "subsegment", "ss", "partition" };


    out.args.emplace_back();
    out.args.back().name = "SubsegMethod";
    out.args.back().desc = "The method to use for sub-segmentation. Nested sub-segmentation should almost"
                           " always be preferred unless you know what you're doing. It should be faster too."
                           " Compound sub-segmentation is known to cause problems, e.g., with zero-area"
                           " sub-segments and spatial dependence in sub-segment volume."
                           " Nested cleaving will produce sub-segments of equivalent area (volume) throughout"
                           " the entire ROI whereas compound sub-segmentation will not.";
    out.args.back().default_val = "nested-cleave";
    out.args.back().expected = true;
    out.args.back().examples = { "nested-cleave", "compound-cleave" };
    out.args.back().samples = OpArgSamples::Exhaustive;


    out.args.emplace_back();
    out.args.back().name = "NestedCleaveOrder";
    out.args.back().desc = "The order in which to apply nested cleaves. This routine requires one of 'ZYX', 'ZXY',"
                           " 'XYZ', 'XZY', 'YZX', or 'YXZ'."
                           " Cleaves are implemented from left to right using the specified X, Y, and Z"
                           " selection criteria.";
    out.args.back().default_val = "ZXY";
    out.args.back().expected = true;
    out.args.back().examples = { "ZXY", "ZYX", "XYZ", "XZY", "YZX", "YXZ" };
    out.args.back().samples = OpArgSamples::Exhaustive;


    out.args.emplace_back();
    out.args.back().name = "XPartitions";
    out.args.back().desc = "The number of partitions to find along the 'X' axis."
                           " The total number of sub-segments produced along the 'X' axis will be (1+XPartitions)."
                           " A value of zero will disable the partitioning along the 'X' axis.";
    out.args.back().default_val = "0";
    out.args.back().expected = true;
    out.args.back().examples = { "0", "1", "3", "5", "50" };


    out.args.emplace_back();
    out.args.back().name = "YPartitions";
    out.args.back().desc = "The number of partitions to find along the 'Y' axis."
                           " The total number of sub-segments produced along the 'Y' axis will be (1+YPartitions)."
                           " A value of zero will disable the partitioning along the 'Y' axis.";
    out.args.back().default_val = "0";
    out.args.back().expected = true;
    out.args.back().examples = { "0", "1", "3", "5", "50" };


    out.args.emplace_back();
    out.args.back().name = "ZPartitions";
    out.args.back().desc = "The number of partitions to find along the 'Z' axis."
                           " The total number of sub-segments produced along the 'Z' axis will be (1+ZPartitions)."
                           " A value of zero will disable the partitioning along the 'Z' axis.";
    out.args.back().default_val = "0";
    out.args.back().expected = true;
    out.args.back().examples = { "0", "1", "3", "5", "50" };


    out.args.emplace_back();
    out.args.back().name = "ReverseXTraversalOrder";
    out.args.back().desc = "Controls the order in which sub-segments are numbered."
                           " If set to 'true' the numbering will be reversed along the X axis."
                           " This option is most useful when the 'X' axis intersects mirrored ROIs"
                           " (e.g., left and right parotid glands).";
    out.args.back().default_val = "false";
    out.args.back().expected = true;
    out.args.back().examples = { "false", "true" };


    out.args.emplace_back();
    out.args.back().name = "ReverseYTraversalOrder";
    out.args.back().desc = "Controls the order in which sub-segments are numbered."
                           " If set to 'true' the numbering will be reversed along the Y axis."
                           " This option is most useful when the 'Y' axis intersects mirrored ROIs"
                           " (e.g., left and right parotid glands).";
    out.args.back().default_val = "false";
    out.args.back().expected = true;
    out.args.back().examples = { "false", "true" };


    out.args.emplace_back();
    out.args.back().name = "ReverseZTraversalOrder";
    out.args.back().desc = "Controls the order in which sub-segments are numbered."
                           " If set to 'true' the numbering will be reversed along the Z axis."
                           " This option is most useful when the 'Z' axis intersects mirrored ROIs"
                           " (e.g., left and right parotid glands).";
    out.args.back().default_val = "false";
    out.args.back().expected = true;
    out.args.back().examples = { "false", "true" };



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



bool PartitionContours(Drover &DICOM_data,
                         const OperationArgPkg& OptArgs,
                         std::map<std::string, std::string>& /*InvocationMetadata*/,
                         const std::string& FilenameLex){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ROILabelRegex = OptArgs.getValueStr("ROILabelRegex").value();
    const auto NormalizedROILabelRegex = OptArgs.getValueStr("NormalizedROILabelRegex").value();

    const auto PlanarOrientation = OptArgs.getValueStr("PlanarOrientation").value();
    const auto SubsegmentRootROILabel = OptArgs.getValueStr("SubsegmentRootROILabel").value();
    const auto SubsegMethodReq = OptArgs.getValueStr("SubsegMethod").value();

    const auto XPartitions = std::stol( OptArgs.getValueStr("XPartitions").value() );
    const auto YPartitions = std::stol( OptArgs.getValueStr("YPartitions").value() );
    const auto ZPartitions = std::stol( OptArgs.getValueStr("ZPartitions").value() );

    const auto ReverseXTraversalOrderStr = OptArgs.getValueStr("ReverseXTraversalOrder").value();
    const auto ReverseYTraversalOrderStr = OptArgs.getValueStr("ReverseYTraversalOrder").value();
    const auto ReverseZTraversalOrderStr = OptArgs.getValueStr("ReverseZTraversalOrder").value();

    const auto FractionalTolerance = std::stod( OptArgs.getValueStr("FractionalTolerance").value() );
    const auto MaxBisects = std::stol( OptArgs.getValueStr("MaxBisects").value() );

    const auto NestedCleaveOrder = OptArgs.getValueStr("NestedCleaveOrder").value();

    //-----------------------------------------------------------------------------------------------------------------
    const auto theregex = Compile_Regex(ROILabelRegex);
    const auto TrueRegex = Compile_Regex("^tr?u?e?$");
    const auto thenormalizedregex = Compile_Regex(NormalizedROILabelRegex);
    const auto SubsegMethodCompound = Compile_Regex("co?m?p?o?u?n?d?[-_]?c?l?e?a?v?e?");
    const auto SubsegMethodNested = Compile_Regex("ne?s?t?e?d?[-_]?c?l?e?a?v?e?");

    const auto OrientAxisAligned = Compile_Regex("ax?i?s?[-_]?a?l?i?g?n?e?d?");
    const auto OrientStaticObl = Compile_Regex("st?a?t?i?c?[-_]?o?b?l?i?q?u?e?");
    const auto OrientCardinal = Compile_Regex("ca?r?d?i?n?a?l?");

    const auto ReverseXTraversalOrder = std::regex_match(ReverseXTraversalOrderStr, TrueRegex);
    const auto ReverseYTraversalOrder = std::regex_match(ReverseYTraversalOrderStr, TrueRegex);
    const auto ReverseZTraversalOrder = std::regex_match(ReverseZTraversalOrderStr, TrueRegex);

    if(!isininc(0, XPartitions, 5000)){
        throw std::invalid_argument("Requested number of partitions along 'X' axis is not valid. Refusing to continue.");
    }else if(!isininc(0, YPartitions, 5000)){
        throw std::invalid_argument("Requested number of partitions along 'Y' axis is not valid. Refusing to continue.");
    }else if(!isininc(0, ZPartitions, 5000)){
        throw std::invalid_argument("Requested number of partitions along 'Z' axis is not valid. Refusing to continue.");
    }

    Explicator X(FilenameLex);

    // Stuff references to all contours into a list. Remember that you can still address specific contours through
    // the original holding containers (which are not modified here).
    auto cc_all = All_CCs( DICOM_data );
    auto cc_ROIs = Whitelist( cc_all, { { "ROIName", ROILabelRegex },
                                        { "NormalizedROIName", NormalizedROILabelRegex } } );

    if(cc_ROIs.empty()){
        throw std::invalid_argument("No contours selected. Cannot continue.");
    }

    // Identify a set of three orthogonal planes along which the contours should be cleaved.
    vec3<double> x_normal(0.0, 1.0, 0.0); // Standard image row_unit.
    vec3<double> y_normal(1.0, 0.0, 0.0); // Standard image col_unit.
    vec3<double> z_normal(0.0, 0.0, 1.0); // Standard image ortho unit.

    const auto find_coordinate_directions = [&]() -> void {
        if(std::regex_match(PlanarOrientation, OrientCardinal)){
            // Do nothing.
            return;
        }

        // Use the contours to estimate the normal vector, but fall-back to 'typical' image row and column units.
        // This works best if the subject is rotated forward or backward (e.g., head on a pillow vs. head without a
        // pillow).
        auto ort_normal = Average_Contour_Normals(cc_ROIs);
        auto row_normal = x_normal;
        auto col_normal = y_normal;
        ort_normal.GramSchmidt_orthogonalize(row_normal, col_normal);
        ort_normal = ort_normal.unit();
        row_normal = row_normal.unit();
        col_normal = col_normal.unit();

        // Use the image-axes aligned normals directly. Sub-segmentation might get snagged on voxel rows or columns.
        if(std::regex_match(PlanarOrientation, OrientAxisAligned)){
            x_normal = row_normal;
            y_normal = col_normal;
            z_normal = ort_normal;
            return;
        }

        // Try to offset the axes slightly so they don't align perfectly with the voxel grid. (Align along the row and
        // column directions, or align along the diagonals, which can be just as bad.)
        if(std::regex_match(PlanarOrientation, OrientStaticObl)){
            x_normal = (row_normal + col_normal * 0.5).unit();
            y_normal = (col_normal - row_normal * 0.5).unit();
            z_normal = (ort_normal - col_normal * 0.5).unit();
            z_normal.GramSchmidt_orthogonalize(x_normal, y_normal);
            x_normal = x_normal.unit();
            y_normal = y_normal.unit();
            z_normal = z_normal.unit();
            return;
        }

        throw std::invalid_argument("Planar orientations not understood. Cannot continue.");
        return;
    };
    find_coordinate_directions();

    YLOGINFO("Proceeding with x_normal = " << x_normal);
    YLOGINFO("Proceeding with y_normal = " << y_normal);
    YLOGINFO("Proceeding with z_normal = " << z_normal);

    // This routine returns a pair of planes that approximately encompass the desired interior volume. The ROIs are not
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


    // Ensure the contours have some place to be stored.
    DICOM_data.Ensure_Contour_Data_Allocated();

    std::list<long int> X_parts(XPartitions+1);
    std::list<long int> Y_parts(YPartitions+1);
    std::list<long int> Z_parts(ZPartitions+1);
    std::iota(std::begin(X_parts), std::end(X_parts), 0);
    std::iota(std::begin(Y_parts), std::end(Y_parts), 0);
    std::iota(std::begin(Z_parts), std::end(Z_parts), 0);
    if(ReverseXTraversalOrder) std::reverse(std::begin(X_parts), std::end(X_parts));
    if(ReverseYTraversalOrder) std::reverse(std::begin(Y_parts), std::end(Y_parts));
    if(ReverseZTraversalOrder) std::reverse(std::begin(Z_parts), std::end(Z_parts));

    // Loop over all compartments (= # of partitions + 1 along each axis).
    long int subsegment_count = -1;
    for(const auto &X_part : X_parts){
        for(const auto &Y_part : Y_parts){
            for(const auto &Z_part : Z_parts){
    //for(long int X_part = 0; X_part <= XPartitions; ++X_part){
    //    for(long int Y_part = 0; Y_part <= YPartitions; ++Y_part){
    //        for(long int Z_part = 0; Z_part <= ZPartitions; ++Z_part){
                ++subsegment_count; // Increment after every partition so contour numbers are always synchronized.

                const double XSelectionThickness        = (XPartitions == 0) ? 1.0 : (1.0 / (1.0 + static_cast<double>(XPartitions)));
                const double YSelectionThickness        = (YPartitions == 0) ? 1.0 : (1.0 / (1.0 + static_cast<double>(YPartitions)));
                const double ZSelectionThickness        = (ZPartitions == 0) ? 1.0 : (1.0 / (1.0 + static_cast<double>(ZPartitions)));
                const double XSelectionOffsetFromBottom = X_part * static_cast<double>(XSelectionThickness);
                const double YSelectionOffsetFromBottom = Y_part * static_cast<double>(YSelectionThickness);
                const double ZSelectionOffsetFromBottom = Z_part * static_cast<double>(ZSelectionThickness);

                // The bisection routine requires for input the fractional area above the plane. We convert from (thickness,offset)
                // units to the fractional area above the plane for both upper and lower extents.
                const double XSelectionLower = std::clamp(1.0 - XSelectionOffsetFromBottom, 0.0, 1.0);
                const double YSelectionLower = std::clamp(1.0 - YSelectionOffsetFromBottom, 0.0, 1.0);
                const double ZSelectionLower = std::clamp(1.0 - ZSelectionOffsetFromBottom, 0.0, 1.0);
                const double XSelectionUpper = std::clamp(1.0 - XSelectionOffsetFromBottom - XSelectionThickness, 0.0, 1.0);
                const double YSelectionUpper = std::clamp(1.0 - YSelectionOffsetFromBottom - YSelectionThickness, 0.0, 1.0);
                const double ZSelectionUpper = std::clamp(1.0 - ZSelectionOffsetFromBottom - ZSelectionThickness, 0.0, 1.0);

                // Perform the sub-segmentation.
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

                        for(const auto &cleave : NestedCleaveOrder){
                            if( (cleave == static_cast<unsigned char>('X'))
                                  ||  (cleave == static_cast<unsigned char>('x')) ){
                                const auto x_planes_pair = bisect_ROIs(running, x_normal, XSelectionLower, XSelectionUpper);
                                running = subsegment_interior(running, x_planes_pair);

                            }else if( (cleave == static_cast<unsigned char>('Y'))
                                  ||  (cleave == static_cast<unsigned char>('y')) ){
                                const auto y_planes_pair = bisect_ROIs(running, y_normal, YSelectionLower, YSelectionUpper);
                                running = subsegment_interior(running, y_planes_pair);

                            }else if( (cleave == static_cast<unsigned char>('Z'))
                                  ||  (cleave == static_cast<unsigned char>('z')) ){
                                const auto z_planes_pair = bisect_ROIs(running, z_normal, ZSelectionLower, ZSelectionUpper);
                                running = subsegment_interior(running, z_planes_pair);

                            }else{
                                throw std::invalid_argument("Cleave axis '"_s + cleave + "' not understood. Cannot continue.");
                            }
                        }

                        cc_selection.emplace_back(running);

                    }else{
                        throw std::invalid_argument("Subsegmentation method not understood. Cannot continue.");
                    }
                }

                //Store the sub-segments.
                const double MinimumSeparation = 1.0; // TODO: is there a routine to do this? (YES: Unique_Contour_Planes()...)
                const auto ROIName = SubsegmentRootROILabel + "_" + std::to_string(subsegment_count);
                for(auto &cc : cc_selection){
                    cc.Insert_Metadata("ROIName", ROIName);
                    cc.Insert_Metadata("NormalizedROIName", X(ROIName));
                    cc.Insert_Metadata("ROINumber", "10000"); // TODO: find highest existing and ++ it.
                    cc.Insert_Metadata("MinimumSeparation", std::to_string(MinimumSeparation));
                    DICOM_data.Ensure_Contour_Data_Allocated();
                    DICOM_data.contour_data->ccs.emplace_back( cc );  // TODO -- place all subsegment contours inside cc_selection into the same cc.
                }

                // Embed partition information in the contour/ROI metadata.

                // ... TODO ...

            }
        }
    }

    return true;
}
