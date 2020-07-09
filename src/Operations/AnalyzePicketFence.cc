//AnalyzePicketFence.cc - A part of DICOMautomaton 2018. Written by hal clark.

#include <limits>
#include <algorithm>
#include <numeric>            //Needed for std::adjacent_difference().
#include <array>
#include <cmath>
#include <cstdlib>            //Needed for exit() calls.
#include <optional>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>    
#include <vector>
#include <utility>

#include "../Insert_Contours.h"
#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../Write_File.h"

#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMathPlottingGnuplot.h" //Needed for YgorMathPlottingGnuplot::*.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorStats.h"        //Needed for Stats:: namespace.
#include "YgorFilesDirs.h"

#include "Explicator.h"

#include "AnalyzePicketFence.h"

OperationDoc OpArgDocAnalyzePicketFence(){
    OperationDoc out;
    out.name = "AnalyzePicketFence";

    out.desc = "This operation extracts MLC positions from a picket fence image.";

    out.notes.emplace_back(
        "This routine requires data to be pre-processed. The gross picket area should be isolated and the leaf"
        " junction areas contoured (one contour per junction). Both can be accomplished via thresholding."
        " Additionally, stray pixels should be filtered out using, for example, median or conservative filters."
    );

    out.notes.emplace_back(
        "This routine analyzes the picket fences on the plane in which they are specified within the DICOM file,"
        " which often coincides with the image receptor ('RTImageSID'). Tolerances are evaluated on the isoplane,"
        " so the image is projected before measuring distances, but the image itself is not altered; a uniform"
        " magnification factor of SAD/SID is applied to all distances."
    );

    out.args.emplace_back();
    out.args.back() = IAWhitelistOpArgDoc();
    out.args.back().name = "ImageSelection";
    out.args.back().default_val = "last";

    out.args.emplace_back();
    out.args.back().name = "MLCModel";
    out.args.back().desc = "The MLC design geometry to use."
                      " 'VarianMillenniumMLC80' has 40 leafs in each bank;"
                      " leaves are 10mm wide at isocentre;"
                      " and the maximum static field size is 40cm x 40cm."
                      " 'VarianMillenniumMLC120' has 60 leafs in each bank;"
                      " the 40 central leaves are 5mm wide at isocentre;"
                      " the 20 peripheral leaves are 10mm wide;"
                      " and the maximum static field size is 40cm x 40cm."
                      " 'VarianHD120' has 60 leafs in each bank;"
                      " the 32 central leaves are 2.5mm wide at isocentre;"
                      " the 28 peripheral leaves are 5mm wide;"
                      " and the maximum static field size is 40cm x 22cm.";
    out.args.back().default_val = "VarianMillenniumMLC120";
    out.args.back().expected = true;
    out.args.back().examples = { "VarianMillenniumMLC80",
                            "VarianMillenniumMLC120",
                            "VarianHD120" };  // It would be nice to try auto-detect this...

    out.args.emplace_back();
    out.args.back().name = "MLCROILabel";
    out.args.back().desc = "An ROI imitating the MLC axes of leaf pairs is created. This is the label to apply"
                      " to it. Note that the leaves are modeled with thin contour rectangles of virtually zero"
                      " area. Also note that the outline colour is significant and denotes leaf pair pass/fail.";
    out.args.back().default_val = "Leaves";
    out.args.back().expected = true;
    out.args.back().examples = { "MLC_leaves",
                            "MLC",
                            "approx_leaf_axes" };

    out.args.emplace_back();
    out.args.back().name = "JunctionROILabel";
    out.args.back().desc = "An ROI imitating the junction is created. This is the label to apply to it."
                      " Note that the junctions are modeled with thin contour rectangles of virtually zero"
                      " area.";
    out.args.back().default_val = "Junction";
    out.args.back().expected = true;
    out.args.back().examples = { "Junction",
                                 "Picket_Fence_Junction" };

    out.args.emplace_back();
    out.args.back().name = "PeakROILabel";
    out.args.back().desc = "ROIs encircling the leaf profile peaks are created. This is the label to apply to it."
                      " Note that the peaks are modeled with small squares.";
    out.args.back().default_val = "Peak";
    out.args.back().expected = true;
    out.args.back().examples = { "Peak",
                                 "Picket_Fence_Peak" };

    out.args.emplace_back();
    out.args.back().name = "MinimumJunctionSeparation";
    out.args.back().desc = "The minimum distance between junctions on the SAD isoplane in DICOM units (mm)."
                      " This number is used to"
                      " de-duplicate automatically detected junctions. Analysis results should not be"
                      " sensitive to the specific value.";
    out.args.back().default_val = "10.0";
    out.args.back().expected = true;
    out.args.back().examples = { "5.0", "10.0", "15.0", "25.0" };

    out.args.emplace_back();
    out.args.back().name = "ThresholdDistance";
    out.args.back().desc = "The threshold distance in DICOM units (mm) above which MLC separations are considered"
                      " to 'fail'. Each leaf pair is evaluated separately. Pass/fail status is also"
                      " indicated by setting the leaf axis contour colour (blue for pass, red for fail).";
    out.args.back().default_val = "1.0";
    out.args.back().expected = true;
    out.args.back().examples = { "0.5", "1.0", "2.0" };


    out.args.emplace_back();
    out.args.back().name = "LeafGapsFileName";
    out.args.back().desc = "This file will contain gap and nominal-vs-actual offset distances for each leaf pair."
                      " The format is CSV. Leave empty to dump to generate a unique temporary file."
                      " If an existing file is present, rows will be appended without writing a header.";
    out.args.back().default_val = "";
    out.args.back().expected = true;
    out.args.back().examples = { "", "/tmp/somefile", "localfile.csv", "derivative_data.csv" };
    out.args.back().mimetype = "text/csv";


    out.args.emplace_back();
    out.args.back().name = "ResultsSummaryFileName";
    out.args.back().desc = "This file will contain a brief summary of the results."
                      " The format is CSV. Leave empty to dump to generate a unique temporary file."
                      " If an existing file is present, rows will be appended without writing a header.";
    out.args.back().default_val = "";
    out.args.back().expected = true;
    out.args.back().examples = { "", "/tmp/somefile", "localfile.csv", "derivative_data.csv" };
    out.args.back().mimetype = "text/csv";


    out.args.emplace_back();
    out.args.back().name = "UserComment";
    out.args.back().desc = "A string that will be inserted into the output file which will simplify merging output"
                      " with differing parameters, from different sources, or using sub-selections of the data.";
    out.args.back().default_val = "";
    out.args.back().expected = true;
    out.args.back().examples = { "", "Using XYZ", "Patient treatment plan C" };


    out.args.emplace_back();
    out.args.back().name = "InteractivePlots";
    out.args.back().desc = "Whether to interactively show plots showing detected edges.";
    out.args.back().default_val = "false";
    out.args.back().expected = true;
    out.args.back().examples = { "true", "false" };

    return out;
}

Drover AnalyzePicketFence(Drover DICOM_data, OperationArgPkg OptArgs, std::map<std::string,std::string> /*InvocationMetadata*/, std::string FilenameLex){

    Explicator X(FilenameLex);

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();

    auto MLCModel = OptArgs.getValueStr("MLCModel").value();

    const auto MLCROILabel = OptArgs.getValueStr("MLCROILabel").value();
    const auto JunctionROILabel = OptArgs.getValueStr("JunctionROILabel").value();
    const auto PeakROILabel = OptArgs.getValueStr("PeakROILabel").value();

    const auto MinimumJunctionSeparation = std::stod( OptArgs.getValueStr("MinimumJunctionSeparation").value() );

    const auto ThresholdDistance = std::stod( OptArgs.getValueStr("ThresholdDistance").value() );

    auto LeafGapsFileName = OptArgs.getValueStr("LeafGapsFileName").value();
    auto ResultsSummaryFileName = OptArgs.getValueStr("ResultsSummaryFileName").value();

    const auto UserComment = OptArgs.getValueStr("UserComment");

    const auto InteractivePlotsStr = OptArgs.getValueStr("InteractivePlots").value();

    //-----------------------------------------------------------------------------------------------------------------
    const auto regex_true = Compile_Regex("^tr?u?e?$");

    const auto regex_VMLC80  = Compile_Regex("^va?r?i?a?n?m?i?l?l?e?n?n?i?u?m?m?l?c?80$");
    const auto regex_VMLC120 = Compile_Regex("^va?r?i?a?n?mille?n?n?i?u?m?m?l?c?120$");
    const auto regex_VHD120  = Compile_Regex("^va?r?i?a?n?hd120$");

    const auto NormalizedMLCROILabel = X(MLCROILabel);
    const auto NormalizedJunctionROILabel = X(JunctionROILabel);
    const auto NormalizedPeakROILabel = X(PeakROILabel);

    const auto InteractivePlots = std::regex_match(InteractivePlotsStr, regex_true);

    auto IAs_all = All_IAs( DICOM_data );
    auto IAs = Whitelist( IAs_all, ImageSelectionStr );
    if(IAs.empty()){
        throw std::invalid_argument("No image arrays selected. Cannot continue.");
    }

    for(auto & iap_it : IAs){
        if((*iap_it)->imagecoll.images.empty()) throw std::invalid_argument("Unable to find an image to analyze.");

        planar_image<float, double> *animg = &( (*iap_it)->imagecoll.images.front() );
        const auto row_unit = animg->row_unit;
        const auto col_unit = animg->col_unit;
        //const auto ort_unit = row_unit.Cross(col_unit);

        const auto ImageDate = animg->GetMetadataValueAs<std::string>("AcquisitionDate").value_or("Unknown");
        const auto PatientID = animg->GetMetadataValueAs<std::string>("PatientID").value_or("Unknown");

        const auto RTImageSID = std::stod( animg->GetMetadataValueAs<std::string>("RTImageSID").value_or("1000.0") );
        const auto SAD = std::stod( animg->GetMetadataValueAs<std::string>("RadiationMachineSAD").value_or("1000.0") );

        const auto SIDToSAD = SAD / RTImageSID; // Factor for scaling distance on image panel plane to distance on SAD plane.
        const auto SADToSID = RTImageSID / SAD; // Factor for scaling distance on SAD plane to distance on image panel plane.

        const auto corner_R = animg->position(0, 0); // A fixed point from which the profiles will be relative to. Better to use Isocentre instead! TODO
        const auto sample_spacing = std::max(animg->pxl_dx, animg->pxl_dy); // The sampling frequency for profiles.

        const auto NaN = std::numeric_limits<double>::quiet_NaN();
        const auto NaN_vec = vec3<double>(NaN, NaN, NaN);

        const auto IsocentreStr = animg->GetMetadataValueAs<std::string>("IsocenterPosition");
        auto Isocentre = vec3<double>(0.0, 0.0, 0.0);
        if(IsocentreStr){
            // Note: assuming the form "1.23/2.34/3.45".
            const auto xyz = SplitStringToVector(IsocentreStr.value(), '\\', 'd');
            if(xyz.size() != 3) throw std::logic_error("Unable to parse DICOM IsocenterPosition. Refusing to continue.");
            Isocentre.x = std::stod(xyz.at(0));
            Isocentre.y = std::stod(xyz.at(1));
            Isocentre.z = std::stod(xyz.at(2));
        }

        const std::string JunctionLineColour = "blue";
        const std::string PeakLineColour = "black";

        {
            auto Modality     = animg->GetMetadataValueAs<std::string>("Modality").value_or("");
            auto RTImagePlane = animg->GetMetadataValueAs<std::string>("RTImagePlane").value_or("");
            if((Modality != "RTIMAGE") || (RTImagePlane != "NORMAL")){
                throw std::domain_error("This routine can only handle RTIMAGES with RTImagePlane=NORMAL.");
            }
        }

        //---------------------------------------------------------------------------
        // Flip pixel values if the image is inverted. The junction peaks should be more positive than the baseline.
        {
            // Extract only from the inner portion of the image where there is likely to be background and junctions.
            const auto row_c = static_cast<long int>(animg->rows / 2);
            const auto col_c = static_cast<long int>(animg->columns / 2);
            const auto drow = static_cast<long int>(row_c / 2);
            const auto dcol = static_cast<long int>(col_c / 2);
            const auto chnl = static_cast<long int>(0);

            const auto mean = animg->block_average( row_c - drow, row_c + drow, col_c - dcol, col_c + dcol, chnl );
            const auto median = animg->block_median( row_c - drow, row_c + drow, col_c - dcol, col_c + dcol, chnl );
            if(mean < median){
                FUNCINFO("Image was found to be inverted. Pixels were flipped so the peaks are positive");
                animg->apply_to_pixels([](long int, long int, long int, float &val) -> void {
                    val *= -1.0;
                    return;
                });
            }
        }


        //---------------------------------------------------------------------------
        //Auto-detect the MLC model, if possible.
        const auto StationName = animg->GetMetadataValueAs<std::string>("StationName").value_or("Unknown");
        {
            const auto regex_FVAREA2TB = Compile_Regex(".*FVAREA2TB.*");
            const auto regex_FVAREA4TB = Compile_Regex(".*FVAREA4TB.*");
            const auto regex_FVAREA5TB = Compile_Regex(".*FVAREA5TB.*");
            const auto regex_FVAREA6TB = Compile_Regex(".*FVAREA6TB.*");

            if(false){
            }else if( std::regex_match(StationName, regex_FVAREA2TB)
                  ||  std::regex_match(StationName, regex_FVAREA4TB)
                  ||  std::regex_match(StationName, regex_FVAREA6TB) ){
                MLCModel = "VarianMillenniumMLC120";
            }else if( std::regex_match(StationName, regex_FVAREA5TB) ){
                MLCModel = "VarianHD120";
            }
        }

        // Picket fence context. This holds the mutated state for a picket fence analysis.
        struct PF_Context {
            using l_num = long int;  // Leaf-pair number. Controlled by the MLC.
            using j_num = long int;  // Junction number. Detected.

            // Plotting shuttles.
            std::vector< YgorMathPlottingGnuplot::Shuttle<samples_1D<double>> > junction_plot_shtl;
            std::vector< YgorMathPlottingGnuplot::Shuttle<samples_1D<double>> > leaf_plot_shtl;

            // Uncompensated collimator rotation. Can be specified or detected automatically.
            double CollimatorCompensation;

            // Unit vectors along the lead- and junction-axes.
            vec3<double> leaf_axis;
            vec3<double> junction_axis;

            // Lines along which individual MLC leaf pairs travel.
            std::map<l_num, line<double>> leaf_lines;

            // Dose profiles along each leaf-pair line.
            std::map<l_num, samples_1D<double>> leaf_profiles;

            // Lines along which the junctions (nominally) coincide. These are detected.
            std::map<j_num, line<double>> junction_lines;

            // The separation of junctions from one another and from the central axis.
            std::map<j_num, double> junction_cax_separations;
            std::map<j_num, double> junction_separations;
            double min_junction_sep;
            double max_junction_sep;

            // Information about individual leaf failures.
            //std::vector<std::string> leaf_line_colour(PFC.leaf_lines.size(), "green");
            std::map<l_num, std::string> leaf_line_colour;
            l_num failed_leaf_count;
            l_num worst_leaf_pair;
            double worst_discrepancy;

            // Junction- and leaf-pair intersections: actual and nominal.
            std::map<l_num, std::map<j_num, vec3<double>>> nominal_jl_int;
            std::map<l_num, std::map<j_num, vec3<double>>> actual_jl_int;

            // Shuttles for various contours.
            std::list<contours_with_meta> peak_contours;
            std::list<contours_with_meta> leaf_pair_contours;
            std::list<contours_with_meta> junction_contours;

        };

        PF_Context PFC;
        PFC.CollimatorCompensation = std::numeric_limits<double>::quiet_NaN();


        // This is the core routine for extracting the position of MLC leaves along junctions.
        // Analysis gets restarted if a collimator rotation is detected at the end, so it is wrapped in a function.
        auto Extract_Leaf_Positions = [&]() -> void {
            //---------------------------------------------------------------------------
            // Extract the junction and leaf pair travel axes.
            auto BeamLimitingDeviceAngle = animg->GetMetadataValueAs<std::string>("BeamLimitingDeviceAngle");
            auto rot_ang = std::stod( BeamLimitingDeviceAngle.value() ) * M_PI/180.0; // in radians.
            if(std::isfinite(PFC.CollimatorCompensation)){
                rot_ang -= PFC.CollimatorCompensation * M_PI/180.0;
            }

            // We now assume that at 0 deg the leaves are aligned with row_unit ( hopefully [1,0,0] ).
            // NOTE: SIMPLIFYING ASSUMPTIONS HERE:
            //       1. at collimator 0 degrees the image column direction and aligns with the MLC leaf travel axis.
            //       2. the rotation axes is the z axes.
            // This transformation is WRONG, but should work in normal situations (gantry ~0, coll 0 or 90, image panel
            // is parallel to the isocentric plane (i.e., orthogonal to the CAX).
            PFC.leaf_axis     = col_unit.rotate_around_z(rot_ang);
            PFC.junction_axis = col_unit.rotate_around_z(rot_ang + 0.5*M_PI);

            //---------------------------------------------------------------------------
            //Create lines for leaf pairs computed from knowledge about each machine's MLC geometry.
            //
            // Note: Leaf geometry is specified on the isoplane. These lines are magnified (or shrunk) to correspond to the
            //       SID plane, and then projected directly onto the plane of the image. Because they are magnified prior to
            //       projection, the projection should be orthographic (i.e., magnificcation-free).
            PFC.leaf_lines.clear();
            {
                auto img_plane = animg->image_plane();
                std::vector<double> mlc_offsets; // Closest point along leaf-pair line from CAX.

                //We have to project the MLC leaf pair positions according to the magnification at the image panel.
                auto magnify = [=](double CAX_offset){
                    return CAX_offset * (RTImageSID / SAD);
                };

                if(false){
                }else if( std::regex_match(MLCModel, regex_VHD120) ){
                    for(long int i = 0; i < 16; ++i){ // The middle 32 leaves.
                        const double x = (2.5 * i + 1.25);
                        const double X = magnify(x);
                        mlc_offsets.emplace_back( X );
                        mlc_offsets.emplace_back(-X );
                    }
                    for(long int i = 0; i < 14; ++i){ // The peripheral 28 leaves.
                        const double x = 40.0 + (5.0 * i + 2.5);
                        const double X = magnify(x);
                        mlc_offsets.emplace_back( X );
                        mlc_offsets.emplace_back(-X );
                    }

                }else if( std::regex_match(MLCModel, regex_VMLC120) ){
                    for(long int i = 0; i < 20; ++i){ // The middle 40 leaves.
                        const double x = (5.0 * i + 2.5);
                        const double X = magnify(x);
                        mlc_offsets.emplace_back( X );
                        mlc_offsets.emplace_back(-X );
                    }
                    for(long int i = 0; i < 10; ++i){ // The peripheral 20 leaves.
                        const double x = 100.0 + (10.0 * i + 5.0);
                        const double X = magnify(x);
                        mlc_offsets.emplace_back( X );
                        mlc_offsets.emplace_back(-X );
                    }

                }else if( std::regex_match(MLCModel, regex_VMLC80) ){
                    for(long int i = 0; i < 20; ++i){
                        const double x = (10.0 * i + 5.0);
                        const double X = magnify(x);
                        mlc_offsets.emplace_back( X );
                        mlc_offsets.emplace_back(-X );
                    }

                }else{
                    throw std::invalid_argument("MLC model not understood");
                }

                std::sort(mlc_offsets.begin(), mlc_offsets.end());

                PFC.leaf_lines.clear();
                PF_Context::l_num leaf_num = 1;
                for(const auto &offset : mlc_offsets){
                    const auto pos = Isocentre + (PFC.junction_axis * offset);
                    const auto proj_pos = img_plane.Project_Onto_Plane_Orthogonally(pos);
                    const auto theline = line<double>(proj_pos, proj_pos + PFC.leaf_axis);
                    PFC.leaf_lines.emplace(leaf_num, theline );
                    ++leaf_num;
                }
            }

            //---------------------------------------------------------------------------
            //Generate leaf-pair profiles.
            PFC.leaf_profiles.clear();
            {
                for(const auto &leaf_line_p : PFC.leaf_lines){
                    auto leaf_num = leaf_line_p.first;
                    auto leaf_line = leaf_line_p.second;

                    // Determine if the leaf is within view in the image. If not, push_back() an empty and continue.
                    //
                    // Note: leaves have been projected onto the image plane, so we only need to check the in-plane
                    //       bounding box.
                    auto img_corners = animg->corners2D();
                    auto img_it = img_corners.begin();
                    const auto CA = *(img_it++);
                    const auto CB = *(img_it++);
                    const auto CC = *(img_it++);
                    const auto CD = *(img_it);

                    const line_segment<double> LSA(CA,CB);
                    const line_segment<double> LSB(CB,CC);
                    const line_segment<double> LSC(CC,CD);
                    const line_segment<double> LSD(CD,CA);

                    auto Intersects_Edge = [=](line_segment<double> LS,
                                               line<double> L) -> vec3<double> {
                        const line<double> LSL(LS.Get_R0(), LS.Get_R1()); // The unbounded line segment line.
                        auto L_LSL_int = NaN_vec; // The place where the unbounded lines intersect.
                        if(!LSL.Closest_Point_To_Line(L, L_LSL_int)) return NaN_vec;

                        if(!LS.Within_Cylindrical_Volume(L_LSL_int, 1E-3)) return NaN_vec;
                        return L_LSL_int;
                    };

                    const auto LSA_int = Intersects_Edge(LSA, leaf_line);
                    const auto LSB_int = Intersects_Edge(LSB, leaf_line);
                    const auto LSC_int = Intersects_Edge(LSC, leaf_line);
                    const auto LSD_int = Intersects_Edge(LSD, leaf_line);

                    std::vector<vec3<double>> edge_ints;
                    if(LSA_int.isfinite()) edge_ints.push_back(LSA_int);
                    if(LSB_int.isfinite()) edge_ints.push_back(LSB_int);
                    if(LSC_int.isfinite()) edge_ints.push_back(LSC_int);
                    if(LSD_int.isfinite()) edge_ints.push_back(LSD_int);

                    // There should be two edge intersection points.
                    //
                    // Note: Oblique profiles that do not contain sufficient data are dealt with on-the-fly since it is
                    // hard to predict if they are sufficiently within the bounds to coincide with the junctions.
                    if(edge_ints.size() != 2){
                        continue;
                    }

                    // Determine bounds where the line intersects the image (endpoints for interpolation).
                    line_segment<double> leaf_edge_ints( edge_ints.front(), edge_ints.back() );

                    // Sample the image, interpolating every min(pxl_dx,pxl_dy) or so.
                    const double sample_offset = 0.5 * sample_spacing;
                    double remainder;
                    const auto R_list = leaf_edge_ints.Sample_With_Spacing( sample_spacing, 
                                                                            sample_offset,
                                                                            remainder );

                    samples_1D<double> profile;
                    const bool inhibit_sort = true;
                    const long int chan = 0;
                    const auto orig_pxl_dz = animg->pxl_dz;
                    animg->pxl_dz = 1.0; // Ensure there is some image thickness.
                    const std::vector<double> sample_dists { -2.0, -1.5, -0.5, 0.0, 0.5, 1.0, 1.5, 2.0 };
                    for(const auto &R : R_list){
                        const auto rel_R = (R - corner_R);
                        const auto l = rel_R.Dot(PFC.leaf_axis);

                        std::vector<double> samples; // Sample vicinity of the leaf line.
                        for(const auto &offset : sample_dists){
                            const auto offset_R = R + (PFC.junction_axis * offset * sample_spacing);
                            try{
                                const auto val = animg->value(offset_R, chan);
                                samples.push_back(val);
                            }catch(const std::exception &){}
                        }
                        if(!samples.empty()){
                            const auto avgd = Stats::Mean(samples);
                            profile.push_back( { l, 0.0, avgd, 0.0 }, inhibit_sort );
                        }
                    }
                    profile.stable_sort();
                    PFC.leaf_profiles.emplace(leaf_num, profile);
                    animg->pxl_dz = orig_pxl_dz;
                }
            }

            //---------------------------------------------------------------------------
            // Detect junctions.
            PFC.junction_lines.clear();
            {
                samples_1D<double> avgd_profile;
                size_t N_visible_leaves = 0;
                for(auto & profile_p : PFC.leaf_profiles){
                    auto profile = profile_p.second;
                    if(!profile.empty()){
                        avgd_profile = avgd_profile.Sum_With(profile);
                        ++N_visible_leaves;
                    }
                }
                if(N_visible_leaves == 0) throw std::logic_error("No visible leaves. Cannot continue.");
                avgd_profile = avgd_profile.Multiply_With(1.0 / static_cast<double>(N_visible_leaves));

                avgd_profile.Average_Coincident_Data(0.5 * sample_spacing);   // Summation can produce MANY samples.
                //avgd_profile = avgd_profile.Resample_Equal_Spacing(sample_spacing);
                PFC.leaf_plot_shtl.emplace_back(avgd_profile.Multiply_x_With(SIDToSAD), 
                                            "Average leaf profile", 
                                            std::vector<std::pair<std::string,std::string>>({{"1:2","lines"}}) );

                //auto avgd_profile2 = avgd_profile.Moving_Average_Two_Sided_Hendersons_23_point();
                //auto avgd_profile2 = avgd_profile.Moving_Median_Filter_Two_Sided_Equal_Weighting(2);
                //auto avgd_profile2 = avgd_profile.Moving_Average_Two_Sided_Gaussian_Weighting(4);
                auto avgd_profile2 = avgd_profile.Moving_Average_Two_Sided_Spencers_15_point();
                //bool l_OK;
                //auto avgd_profile2 = NPRLL::Attempt_Auto_Smooth(avgd_profile, &l_OK);
                //if(!l_OK) throw std::runtime_error("Unable to perform NPRLL to smooth junction-orthogonal profile.");

                //Perform a high-pass filter to remove some beam profile and imager bias dependence.
                //avgd_profile2 = avgd_profile2.Subtract(
                //                                avgd_profile2.Moving_Average_Two_Sided_Gaussian_Weighting(15) );

                PFC.junction_plot_shtl.emplace_back( avgd_profile2.Multiply_x_With(SIDToSAD), "High-pass filtered Junction Profile" );

                //Now find all (local) peaks via the derivative of the crossing points.
                auto peaks = avgd_profile2.Peaks();

                //Merge peaks that are separated by a small distance. These can be spurious, or can result if there is some
                // MLC leaf overtravel.
                //
                // Note: We are generous here because the junction separation should be >2-3 mm or so.
                peaks.Average_Coincident_Data(0.45 * (MinimumJunctionSeparation * SADToSID));

                PFC.junction_plot_shtl.emplace_back( peaks.Multiply_x_With(SIDToSAD), "Junction Profile Peaks" );

                //Filter out spurious peaks that are not 'sharp' enough.
                auto Aspect_Ratio = [](const samples_1D<double> &A) -> double { // Computes aspect ratio for a snippet.
                    double A_aspect_r = std::numeric_limits<double>::quiet_NaN();
                    try{
                        const auto A_extrema_x = A.Get_Extreme_Datum_x();
                        const auto A_extrema_y = A.Get_Extreme_Datum_y();
                        A_aspect_r = (A_extrema_y.second[2] - A_extrema_y.first[2])/(A_extrema_x.second[0] - A_extrema_x.first[0]);
                    }catch(const std::exception &){}
                    return A_aspect_r;
                };

                // Flatten and normalize the profile so we can consistently estimate which peaks are 'major'.
                auto avgd_profile3 = avgd_profile2.Subtract(
                                                avgd_profile2.Moving_Average_Two_Sided_Gaussian_Weighting(10) );

                // Normalize using only the inner region. Outer edges can be fairly noisy.
                const auto sum3_xmm = avgd_profile3.Get_Extreme_Datum_x();
                const auto sum3_xmin = sum3_xmm.first[0];
                const auto sum3_xmax = sum3_xmm.second[0];
                const auto dL = (sum3_xmax - sum3_xmin);
                const auto inner_region = avgd_profile3.Select_Those_Within_Inc( sum3_xmin + 0.2*dL, sum3_xmax - 0.2*dL );

                // Normalize so the lowest trough = 0 and highest peak = 1.
                const auto sum3_ymm = inner_region.Get_Extreme_Datum_y();
                const auto sum3_ymin = sum3_ymm.first[2];
                const auto sum3_ymax = sum3_ymm.second[2];

                avgd_profile3 = avgd_profile3.Sum_With(-sum3_ymin);
                avgd_profile3 = avgd_profile3.Multiply_With(1.0/(sum3_ymax - sum3_ymin));
                //PFC.junction_plot_shtl.emplace_back( avgd_profile3.Multiply_x_With(SIDToSAD), "Aspect Ratio Profile" );

                if(peaks.size() < 2) std::runtime_error("Leaf-leakage peaks not correctly detected. Please verify input.");

                // Aspect ratio filtering.
                if(true){
                    samples_1D<double> filtered_peaks;
                    for(const auto &p4 : peaks.samples){
                        double centre = p4[0]; // peak centre.
                        const auto SearchDistance = 0.25 * (MinimumJunctionSeparation * SADToSID);

                        //Only bother looking at peaks that have enough surrounding room to estimate the aspect ratio.
                        if(!isininc(sum3_xmin + SearchDistance, centre, sum3_xmax - SearchDistance)) continue;

                        auto vicinity = avgd_profile3.Select_Those_Within_Inc(centre - SearchDistance, centre + SearchDistance);
                        const auto ar = Aspect_Ratio(vicinity) * 2.0 * SearchDistance; //Aspect ratio with x rescaled to 1.

                        //FUNCINFO("Aspect ratio = " << ar);
                        if(std::isfinite(ar) && (ar > 0.20)){ // A fairly slight aspect ratio threshold is needed.
                            filtered_peaks.push_back( p4 );
                        }
                    }
                    filtered_peaks.stable_sort();
                    peaks = filtered_peaks;
                }

                // Median value filtering. This approach considers the height of the peak relative to the background of the
                // near vicinity. The peak must be higher than background median on the left and right (separately) to avoid
                // 'me-too' fake peaks found near true peaks.
                if(false){
                    samples_1D<double> filtered_peaks;
                    for(const auto &p4 : peaks.samples){
                        double centre = p4[0]; // peak centre.

                        const auto BGSearchDistance = 1.5 * (MinimumJunctionSeparation * SADToSID);
                        auto vicinity_BG = avgd_profile2.Select_Those_Within_Inc(centre - BGSearchDistance,
                                                                                 centre + BGSearchDistance);
                        const auto bg_median = vicinity_BG.Median_y()[0];

                        //Only bother looking at peaks that have enough surrounding room to estimate the aspect ratio.
                        const auto SearchDistance = 0.20 * (MinimumJunctionSeparation * SADToSID);
                        if(!isininc(sum3_xmin + SearchDistance, centre, sum3_xmax - SearchDistance)) continue;

                        auto vicinity_L = avgd_profile2.Select_Those_Within_Inc(centre - SearchDistance, centre);
                        auto vicinity_R = avgd_profile2.Select_Those_Within_Inc(centre, centre + SearchDistance);
                        //const auto peak_max_L = vicinity.Get_Extreme_Datum_y().first[2];
                        //const auto peak_max_R = vicinity.Get_Extreme_Datum_y().first[2];
                        const auto peak_max_L = vicinity_L.Median_y()[0];
                        const auto peak_max_R = vicinity_R.Median_y()[0];

                        if((peak_max_L > bg_median) && (peak_max_R > bg_median)) filtered_peaks.push_back( p4 );
                    }
                    filtered_peaks.stable_sort();
                    peaks = filtered_peaks;
                }
                if(peaks.size() < 2) std::runtime_error(
                    "Leaf-leakage peaks incorrectly filtered out. Please verify input."
                    " There may be insufficient SNR to extract picket fences; consider delivering more dose."
                );

                PFC.junction_plot_shtl.emplace_back( peaks.Multiply_x_With(SIDToSAD), "Filtered Junction Profile Peaks" );

                PF_Context::j_num junction_num = 0;
                for(const auto &peak4 : peaks.samples){
                    const auto d = peak4[0]; // Projection of relative position onto the leaf axis unit vector.
                    const auto R_peak = corner_R + (PFC.leaf_axis * d);
                    const auto theline = line<double>(R_peak, R_peak + PFC.junction_axis);
                    PFC.junction_lines.emplace(junction_num, theline);
                    ++junction_num;
                }
            }

            //---------------------------------------------------------------------------
            //Determine if the junction separation is consistent. Prune junctions that are not consistent.
            //
            // NOTE: While junction separation is likely to be consistent, it is not necessarily so, and the presence of
            //       many spurious junction lines could drown out legitimate junction lines. A better approach would be to
            //       try selecting junctions based on projected amplitude -- the most likely peaks will have the highest
            //       amplitude, and the amplitudes will *probably* be above the profile median amplitude.
            PFC.junction_cax_separations.clear();
            PFC.junction_separations.clear();
            PFC.min_junction_sep = std::numeric_limits<double>::quiet_NaN();
            PFC.max_junction_sep = std::numeric_limits<double>::quiet_NaN();
            {
                using j_lines_t = decltype(PF_Context::junction_lines);

                const auto p_lt = [](std::pair<PF_Context::j_num, double> A,
                                     std::pair<PF_Context::j_num, double> B){
                                     return (A.second < B.second);
                                  };

                auto determine_junction_separations = [&](j_lines_t j_lines) -> void {
                    PFC.junction_cax_separations.clear();
                    for(const auto &j_line_p : j_lines){
                        const auto junction_num = j_line_p.first;
                        const auto junction_line = j_line_p.second;
                        const auto dR = (junction_line.R_0 - Isocentre);
                        const auto sep = dR.Dot(PFC.leaf_axis);
                        PFC.junction_cax_separations.emplace( junction_num, sep );
                    }
                    PFC.junction_separations.clear();

                    //Adjacent difference. std::adjacent_difference() causing issues here due to std::pair<> (?) ... TODO.
                    if(PFC.junction_cax_separations.size() >= 2){
                        auto it_A = PFC.junction_cax_separations.begin();
                        auto it_B = std::next(it_A);
                        while(it_B != PFC.junction_cax_separations.end()){
                            auto j_num_A = it_A->first;
                            auto sep_diff = (it_B->second - it_A->second);
                            PFC.junction_separations.emplace(j_num_A, sep_diff);
                            ++it_A;
                            ++it_B;
                        }
                    }

                    if(PFC.junction_separations.empty()){
                        throw std::logic_error("No junction separations. Refusing to continue.");
                    }
                    PFC.min_junction_sep = (std::min_element(PFC.junction_separations.begin(),
                                                             PFC.junction_separations.end(), p_lt))->second;
                    PFC.max_junction_sep = (std::max_element(PFC.junction_separations.begin(),
                                                             PFC.junction_separations.end(), p_lt))->second;
                    return;
                };
                auto all_except = [](j_lines_t j_lines_in, PF_Context::j_num k) -> j_lines_t {
                    j_lines_t out;
                    for(auto &j_line_p : j_lines_in){
                        const auto j_num = j_line_p.first;
                        if(j_num != k) out.emplace(j_line_p);
                    }
                    return out;
                };

                auto trimmed_junction_lines = PFC.junction_lines;
                determine_junction_separations(PFC.junction_lines);
                while( RELATIVE_DIFF(PFC.min_junction_sep, PFC.max_junction_sep) > 0.10 ){
                    FUNCWARN("Junction separation is not consistent. Pruning the worst junction..");

                    std::map<PF_Context::j_num, double> rdiffs;
                    for(auto &t_j_line_p : trimmed_junction_lines){
                        const auto j_num = t_j_line_p.first;
                        auto trimmed = all_except(trimmed_junction_lines, j_num);
                        determine_junction_separations( trimmed );
                        rdiffs.emplace( j_num, std::abs( RELATIVE_DIFF(PFC.min_junction_sep, PFC.max_junction_sep) ) );
                    }

                    auto worst_it = std::min_element(rdiffs.begin(), rdiffs.end(), p_lt);
                    const auto k = worst_it->first;

                    // Recompute to regenerate side-effects.
                    trimmed_junction_lines = all_except(trimmed_junction_lines, k);
                    determine_junction_separations(trimmed_junction_lines);
                }
                PFC.junction_lines = trimmed_junction_lines;

                std::cout << "Junction-CAX separations: ";
                for(auto &d : PFC.junction_cax_separations) std::cout << (d.second * SIDToSAD) << "  ";
                std::cout << std::endl;

                std::cout << "Junction-junction separations: ";
                for(auto &d : PFC.junction_separations) std::cout << (d.second * SIDToSAD) << "  ";
                std::cout << std::endl;

                std::cout << "Minimum junction separation: " << (PFC.min_junction_sep * SIDToSAD) << std::endl;
                std::cout << "Maximum junction separation: " << (PFC.max_junction_sep * SIDToSAD) << std::endl;

            }


            //---------------------------------------------------------------------------
            // Extract peak locations (actual and nominal) for later comparison.
            PFC.actual_jl_int.clear();
            PFC.nominal_jl_int.clear();

            {
                auto img_corners = animg->corners2D();
                auto img_it = img_corners.begin();
                const auto CA = *(img_it++);
                const auto CB = *(img_it++);
                const auto CC = *(img_it++);
                const auto CD = *(img_it);

                const line<double> LSA(CA,CB);
                const line<double> LSB(CB,CC);
                const line<double> LSC(CC,CD);
                const line<double> LSD(CD,CA);

                auto Nearest_Edge_Dist_Edge = [=](const vec3<double> &R) -> double {
                    std::vector<double> dists;
                    dists.emplace_back( LSA.Distance_To_Point(R) );
                    dists.emplace_back( LSB.Distance_To_Point(R) );
                    dists.emplace_back( LSC.Distance_To_Point(R) );
                    dists.emplace_back( LSD.Distance_To_Point(R) );
                    return Stats::Min(dists);
                };

                auto l_leaf_lines = PFC.leaf_lines;
                for(auto &leaf_line_p : l_leaf_lines){
                    const auto leaf_num = leaf_line_p.first;
                    const auto leaf_line = leaf_line_p.second;

                    //Find the leaf-junction intersection points (i.e., the nominal peak position).
                    //
                    // Note: We permit junction and leaf axes to be non-orthogonal, so have to test
                    //       for the intersection point for each junction each time.
                    for(const auto &j_line_p : PFC.junction_lines){
                        const auto junction_num = j_line_p.first;
                        const auto junction_line = j_line_p.second;

                        // Find the intersection point in 3D.
                        auto lj_int = NaN_vec;
                        if(!leaf_line.Closest_Point_To_Line(junction_line, lj_int)) lj_int = NaN_vec;
                        if(!lj_int.isfinite()) throw std::runtime_error("Cannot compute leaf-junction intersection.");

                        // If the point is not within the image bounds, ignore it.
                        const auto orig_pxl_dz = animg->pxl_dz;
                        animg->pxl_dz = 1.0; // Ensure there is some thickness.
                        const auto within_img = animg->encompasses_point(lj_int);
                        animg->pxl_dz = orig_pxl_dz;
                        if(!within_img) continue;

                        PFC.nominal_jl_int[leaf_num][junction_num] = lj_int;
                    }
                    if(PFC.nominal_jl_int[leaf_num].empty()) continue;


                    //Determine the true peak location in the vicinity of the junction.
                    auto l_nominal_jl_int = PFC.nominal_jl_int[leaf_num];
                    for(const auto &lj_int_p : l_nominal_jl_int){
                        const auto j_num = lj_int_p.first;
                        const auto lj_int = lj_int_p.second;

                        // Project the point into the same 2D space as the leaf profiles.
                        const auto rel_R = (lj_int - corner_R);
                        const auto nominal_peak = rel_R.Dot(PFC.leaf_axis); // = an offset projected along the leaf axis.

                        const auto SearchDistance = 0.5*PFC.min_junction_sep;
                        const auto vicinity = PFC.leaf_profiles[leaf_num].Select_Those_Within_Inc(nominal_peak - SearchDistance,
                                                                                       nominal_peak + SearchDistance);

                        // Using a robust, non-iterative peak-bounding procedure. It is robust in the sense that it uses the
                        // median to estimate peak location.
                        //
                        // Note: This technique assumes the peak is symmetrical, and exploits this to estimate the peak
                        //       centre. I can't think of a scenario where the symmetry assumption would be violated but the
                        //       test would still pass.
                        // 
                        // Note: Thresholds were selected to avoid noise at the baseline and overtravel 'M'-shaped profiles
                        //       with slightly differing heights. Since 'M'-shaped profiles are not as tall as uni-peaked
                        //       profiles, the aspect ratio will generally differ. So you should not compare the profile
                        //       width (e.g., FWHM) from profile to profile without somehow incorporating the profile shape.
                        //
                        const auto v_mmy = vicinity.Get_Extreme_Datum_y();
                        const auto min_y = v_mmy.first[2];
                        const auto max_y = v_mmy.second[2];
                        std::vector<double> thresholds {   
                            min_y + 0.85 * (max_y - min_y),
                            min_y + 0.80 * (max_y - min_y),
                            min_y + 0.75 * (max_y - min_y),
                            min_y + 0.70 * (max_y - min_y),
                            min_y + 0.65 * (max_y - min_y),
                            min_y + 0.60 * (max_y - min_y),
                            min_y + 0.55 * (max_y - min_y),
                            min_y + 0.50 * (max_y - min_y),
                            min_y + 0.40 * (max_y - min_y),
                            min_y + 0.30 * (max_y - min_y) };

                        try{
                            std::vector<double> est_peaks;

                            //Sample the crossings. 
                            for(const auto &threshold : thresholds){
                                auto crossings = vicinity.Crossings(threshold); // Might throw.
                                const auto c_mmx = crossings.Get_Extreme_Datum_x();
                                est_peaks.emplace_back( 0.5 * (c_mmx.second[0] + c_mmx.first[0]) );
                            }

                            const auto est_peak = Stats::Median(est_peaks);
                            const auto peak_offset = est_peak - nominal_peak;

                            const auto peak_actual = lj_int + (PFC.leaf_axis * peak_offset); // R^3 position of peak.
                            PFC.actual_jl_int[leaf_num][j_num] = peak_actual;

                            // If the peak is too close to the image edge, decline to analyze it.
                            if(Nearest_Edge_Dist_Edge(peak_actual) < (2.0 * sample_spacing)){
                                throw std::runtime_error("Peak too close to the image edge.");
                            }

                        }catch(const std::exception &){
                            // If there is insufficient data, purge the leaf-pair and move on.
                            PFC.leaf_lines.erase(leaf_num);
                            PFC.leaf_profiles.erase(leaf_num);
                            PFC.actual_jl_int.erase(leaf_num);
                            PFC.nominal_jl_int.erase(leaf_num);
                            PFC.leaf_line_colour.erase(leaf_num);
                            continue;
                        };
                    }
                }
            }
        };
        Extract_Leaf_Positions();

        //---------------------------------------------------------------------------
        // Detect uncompensated collimator rotation by looking for trends in peak offsets along the junctions.
        if(!std::isfinite(PFC.CollimatorCompensation)){
            std::vector<double> j_rot;
            for(auto &j_line_p : PFC.junction_lines){
                const auto j_num = j_line_p.first;
                //const auto j_line = j_line_p.second;

                samples_1D<double> offsets;
                for(auto &leaf_line_p : PFC.leaf_lines){
                    const auto leaf_num = leaf_line_p.first;
                    //const auto leaf_line = leaf_line_p.second;

                    const auto peak_actual = PFC.actual_jl_int[leaf_num][j_num];
                    const auto x = (peak_actual - Isocentre).Dot(PFC.junction_axis);
                    const auto y = (peak_actual - Isocentre).Dot(PFC.leaf_axis);
                    offsets.push_back(x, 0.0, y, 0.0);
                }
                bool fitOK = false;
                const bool SkipExtras = true;
                auto res = offsets.Linear_Least_Squares_Regression(&fitOK, SkipExtras);
                if(!fitOK) throw std::runtime_error("Encountered error detecting uncompensated collimator rotation.");
                const auto eff_rtn = std::atan(res.slope);
                j_rot.emplace_back(eff_rtn);
            }
            PFC.CollimatorCompensation = Stats::Median(j_rot) * 180 / M_PI;

            FUNCINFO("Detected an uncompensated collimator rotation of " << PFC.CollimatorCompensation << " deg");
            FUNCINFO("Restarting analysis to incorporate the extra collimator rotation");
            Extract_Leaf_Positions();
        }

        //---------------------------------------------------------------------------
        // Compare peak locations.
        PFC.leaf_line_colour.clear();
        for(const auto &leaf_profile : PFC.leaf_profiles){
            const auto l_num = leaf_profile.first;
            PFC.leaf_line_colour.emplace(l_num, "green");
        }
        PFC.failed_leaf_count = static_cast<PF_Context::l_num>(0);
        PFC.worst_leaf_pair = static_cast<PF_Context::l_num>(-1);
        PFC.worst_discrepancy = 0.0;

        PFC.peak_contours.clear();
        PFC.peak_contours.emplace_back();

        {
            for(auto &leaf_line_p : PFC.leaf_lines){
                const auto leaf_num = leaf_line_p.first;
                const auto leaf_line = leaf_line_p.second;

                if(PFC.nominal_jl_int[leaf_num].empty()) continue;

                // Compare nominal peak and actual peak positions.
                std::vector<double> peak_offsets;
                for(const auto &lj_int_p : PFC.nominal_jl_int[leaf_num]){
                    const auto j_num = lj_int_p.first;
                    const auto peak_nominal = lj_int_p.second;
                    const auto peak_actual = PFC.actual_jl_int[leaf_num][j_num];

                    const auto peak_offset = (peak_actual - peak_nominal).Dot(PFC.leaf_axis);
                    if(std::isfinite(peak_offset)){
                        peak_offsets.push_back(peak_offset);
                    }
                }

                // Compare the nominal and actual intersections.
                if(peak_offsets.size() != PFC.nominal_jl_int[leaf_num].size()){
                    throw std::runtime_error("Unable to find leaf profile peak near nominal junction.");
                }

                std::vector<double> adjacent_peak_diffs;
                std::adjacent_difference(peak_offsets.begin(), peak_offsets.end(),
                                         std::back_inserter(adjacent_peak_diffs));

                const auto max_abs_sep = std::max( std::abs(Stats::Min(peak_offsets)),
                                                   std::abs(Stats::Max(peak_offsets)) );
                const auto mean_abs_sep = Stats::Mean(peak_offsets);

                const auto max_adj_diff = std::max( std::abs(Stats::Min(adjacent_peak_diffs)),
                                                    std::abs(Stats::Max(adjacent_peak_diffs)) );
                const auto mean_adj_diff = Stats::Mean(adjacent_peak_diffs);
                
                const auto within_abs_sep  = (max_abs_sep < (ThresholdDistance * SADToSID));
                const auto within_adj_diff = (max_adj_diff < (ThresholdDistance * SADToSID));

                if(max_abs_sep > PFC.worst_discrepancy){
                    PFC.worst_discrepancy = max_abs_sep;
                    PFC.worst_leaf_pair = leaf_num;
                }

                //Report findings about this leaf-pair.
                FUNCINFO("Attempting to claim a mutex");
                try{
                    auto gen_filename = [&]() -> std::string {
                        if(LeafGapsFileName.empty()){
                            LeafGapsFileName = Get_Unique_Sequential_Filename("/tmp/dicomautomaton_evaluatepf_", 6, ".csv");
                        }
                        return LeafGapsFileName;
                    };

                    std::stringstream header;
                    header << "PatientID,"
                           << "StationName,"
                           << "LeafNumber,"

                           << "\"max |actual-nominal| position\","
                           << "\"mean (actual-nominal) position\","
                           << "\"Pass abs. sep. threshold?\","

                           << "\"max |adjacent difference|\","
                           << "\"mean (adjacent difference)\","
                           << "\"Pass adj. diff. threshold?\","

                           << "UserComment" // Keep this column even if empty so we can append/concat files.
                           << std::endl;

                    std::stringstream body;
                    body << PatientID << ","
                         << StationName << ","
                         << leaf_num << ","

                         << (max_abs_sep * SIDToSAD) << ","
                         << (mean_abs_sep * SIDToSAD) << ","
                         << (within_abs_sep ? "pass" : "fail") << ","

                         << (max_adj_diff * SIDToSAD) << ","
                         << (mean_adj_diff * SIDToSAD) << ","
                         << (within_adj_diff ? "pass" : "fail") << ","

                         << UserComment.value_or("")
                         << std::endl;

                    Append_File( gen_filename,
                                 "dicomautomaton_operation_analyzepicketfence_mutex",
                                 header.str(),
                                 body.str() );

                }catch(const std::exception &e){
                    FUNCERR("Unable to write to output file: '" << e.what() << "'");
                }


                //Record whether this leaf passed or failed visually.
                if( within_abs_sep && within_adj_diff ){
                    PFC.leaf_line_colour[leaf_num] = "blue";
                }else{
                    PFC.leaf_line_colour[leaf_num] = "red";
                    PFC.failed_leaf_count++;

                    //Display the failing profile for inspection.
                    PFC.leaf_plot_shtl.emplace_back( PFC.leaf_profiles[leaf_num].Multiply_x_With(SIDToSAD),
                                                 "Failed leaf "_s + std::to_string(leaf_num),
                                                 std::vector<std::pair<std::string,std::string>>({{"1:2","lines"}}) );
                }
            }
        }


        //Report a summary.
        FUNCINFO("Attempting to claim a mutex");
        try{
            auto gen_filename = [&]() -> std::string {
                if(ResultsSummaryFileName.empty()){
                    ResultsSummaryFileName = Get_Unique_Sequential_Filename("/tmp/dicomautomaton_pfsummary_", 6, ".csv");
                }
                return ResultsSummaryFileName;
            };

            std::stringstream header;
            header << "Quantity,"
                   << "Result"
                   << std::endl;

            std::stringstream body;
            body << "Patient ID,"
                 << PatientID
                 << std::endl;
            body << "Station name,"
                 << StationName
                 << std::endl;
            body << "Acquisition date,"
                 << ImageDate
                 << std::endl;
            body << "Overall result,"
                 << ( (PFC.failed_leaf_count == 0) ? "pass" : "fail" )
                 << std::endl;
            body << "Number of failed leaf pairs,"
                 << PFC.failed_leaf_count
                 << std::endl;
            body << "Worst leaf pair (#),"
                 << PFC.worst_leaf_pair 
                 << std::endl;
            body << "Worst discrepancy (mm; at SAD isoplane),"
                 << (PFC.worst_discrepancy * SIDToSAD)
                 << std::endl;
            body << "Detected uncompensated rotation (degrees),"
                 << PFC.CollimatorCompensation
                 << std::endl;

            Append_File( gen_filename,
                         "dicomautomaton_operation_analyzepicketfence_mutex",
                         header.str(),
                         body.str() );

        }catch(const std::exception &e){
            FUNCERR("Unable to write to output file: '" << e.what() << "'");
        }

        //---------------------------------------------------------------------------
        //Add thin contours for visually inspecting the location of the peaks.
        PFC.leaf_pair_contours.clear();
        PFC.leaf_pair_contours.emplace_back();
        {
            auto contour_metadata = animg->metadata;
            contour_metadata["ROIName"] = MLCROILabel;
            contour_metadata["NormalizedROIName"] = NormalizedMLCROILabel;
            for(const auto &leaf_line_p : PFC.leaf_lines){
                const auto leaf_num = leaf_line_p.first;
                const auto leaf_line = leaf_line_p.second;
                contour_metadata["OutlineColour"] = PFC.leaf_line_colour[leaf_num];
                contour_metadata["PicketFenceLeafNumber"] = std::to_string(leaf_num);

                try{ // Will throw if grossly out-of-bounds, but it's a pain to pre-filter -- ignore exceptions for now... TODO
                    Inject_Thin_Line_Contour(*animg,
                                             leaf_line,
                                             PFC.leaf_pair_contours.back(),
                                             contour_metadata);
                }catch(const std::exception &){};
            }
        }


        //---------------------------------------------------------------------------
        //Add thin contours for visually inspecting the location of the junctions.
        PFC.junction_contours.clear();
        PFC.junction_contours.emplace_back();
        {
            auto contour_metadata = animg->metadata;
            contour_metadata["ROIName"] = JunctionROILabel;
            contour_metadata["NormalizedROIName"] = NormalizedJunctionROILabel;
            contour_metadata["OutlineColour"] = JunctionLineColour;
            for(const auto &j_line_p : PFC.junction_lines){
                const auto junction_num = j_line_p.first;
                const auto junction_line = j_line_p.second;
                contour_metadata["PicketFenceJunctionNumber"] = std::to_string(junction_num);
                try{ // Will throw if grossly out-of-bounds, but it's a pain to pre-filter -- ignore exceptions for now... TODO
                    Inject_Thin_Line_Contour(*animg,
                                             junction_line,
                                             PFC.junction_contours.back(),
                                             contour_metadata,
                                             2.0*(ThresholdDistance * SADToSID) );
                }catch(const std::exception &){};
            }
        }


        //---------------------------------------------------------------------------
        //Draw a small contour to indicate the position of the peak.
        PFC.peak_contours.clear();
        PFC.peak_contours.emplace_back();
        {
            auto contour_metadata = animg->metadata;
            contour_metadata["ROIName"] = PeakROILabel;
            contour_metadata["NormalizedROIName"] = NormalizedPeakROILabel;
            contour_metadata["OutlineColour"] = PeakLineColour;
            const auto radius = 2.0 * std::max(animg->pxl_dx, animg->pxl_dy); // Something reasonable relative to the image features.
            const long int num_verts = 4; // Squares.

            for(const auto &l_int_p : PFC.actual_jl_int){
                const auto leaf_num = l_int_p.first;
                for(const auto &j_int_p : l_int_p.second){
                    const auto junction_num = j_int_p.first;
                    const auto peak_actual = j_int_p.second;
                    contour_metadata["PicketFenceLeafNumber"] = std::to_string(leaf_num);
                    contour_metadata["PicketFenceJunctionNumber"] = std::to_string(junction_num);
                    try{
                        Inject_Point_Contour(*animg,
                                             peak_actual,
                                             PFC.peak_contours.back(),
                                             contour_metadata,
                                             radius,
                                             num_verts );
                    }catch(const std::exception &){};
                }
            }
        }


        //---------------------------------------------------------------------------
        //Display some interactive plots.
        if(InteractivePlots){

            // Plot leaf-pair profiles that were over tolerance.
            if(true){
                YgorMathPlottingGnuplot::Plot<double>(PFC.leaf_plot_shtl, "Failed leaf-pair profiles", "DICOM position", "Pixel Intensity");
            }

            // Plot junction profiles.
            if(false){
                YgorMathPlottingGnuplot::Plot<double>(PFC.junction_plot_shtl, "Junction profiles", "DICOM position", "Pixel Intensity");
            }
        }

        // Insert contours.
        if(DICOM_data.contour_data == nullptr){
            std::unique_ptr<Contour_Data> output (new Contour_Data());
            DICOM_data.contour_data = std::move(output);
        }
        DICOM_data.contour_data->ccs.splice( DICOM_data.contour_data->ccs.end(), PFC.peak_contours );
        DICOM_data.contour_data->ccs.splice( DICOM_data.contour_data->ccs.end(), PFC.leaf_pair_contours );
        DICOM_data.contour_data->ccs.splice( DICOM_data.contour_data->ccs.end(), PFC.junction_contours );
    }

    return DICOM_data;
}
