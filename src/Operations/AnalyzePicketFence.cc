//AnalyzePicketFence.cc - A part of DICOMautomaton 2018. Written by hal clark.

#include <boost/interprocess/creation_tags.hpp>
#include <boost/interprocess/sync/named_mutex.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>

#include <limits>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>            //Needed for exit() calls.
#include <experimental/optional>
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
#include "AnalyzePicketFence.h"
#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMathPlottingGnuplot.h" //Needed for YgorMathPlottingGnuplot::*.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorStats.h"        //Needed for Stats:: namespace.


std::list<OperationArgDoc> OpArgDocAnalyzePicketFence(void){
    std::list<OperationArgDoc> out;

    // This operation extracts MLC positions from a picket fence image.
    //
    // Note: This routine requires data to be pre-processed. The gross picket area should be isolated and the leaf
    //       junction areas contoured (one contour per junction). Both can be accomplished via thresholding.

    out.emplace_back();
    out.back().name = "ImageSelection";
    out.back().desc = "Images to operate on. Either 'none', 'last', 'first', or 'all'.";
    out.back().default_val = "last";
    out.back().expected = true;
    out.back().examples = { "none", "last", "first", "all" };
    

    out.emplace_back();
    out.back().name = "MLCModel";
    out.back().desc = "The MLC design geometry to use."
                      " 'VarianMillenniumMLC80' has 80 leafs in each bank;"
                      " leaves are 10mm wide at isocentre;"
                      " and the maximum static field size is 40cm x 40cm."
                      " 'VarianMillenniumMLC120' has 120 leafs in each bank;"
                      " the 40 central leaves are 5mm wide at isocentre;"
                      " the 20 peripheral leaves are 10mm wide;"
                      " and the maximum static field size is 40cm x 40cm."
                      " 'VarianHD120' has 120 leafs in each bank;"
                      " the 32 central leaves are 2.5mm wide at isocentre;"
                      " the 28 peripheral leaves are 5mm wide;"
                      " and the maximum static field size is 40cm x 22cm.";
    out.back().default_val = "VarianMillenniumMLC120";
    out.back().expected = true;
    out.back().examples = { "VarianMillenniumMLC80",
                            "VarianMillenniumMLC120",
                            "VarianHD120" };  // It would be nice to try auto-detect this...

    out.emplace_back();
    out.back().name = "MLCROILabel";
    out.back().desc = "An ROI imitating the MLC axes of leaf pairs is created. This is the label to apply"
                      " to it. Note that the leaves are modeled with thin contour rectangles of virtually zero"
                      " area. Also note that the outline colour is significant and denotes leaf pair pass/fail.";
    out.back().default_val = "Leaves";
    out.back().expected = true;
    out.back().examples = { "MLC_leaves",
                            "MLC",
                            "approx_leaf_axes" };

    out.emplace_back();
    out.back().name = "JunctionROILabel";
    out.back().desc = "An ROI imitating the junction is created. This is the label to apply to it."
                      " Note that the junctions are modeled with thin contour rectangles of virtually zero"
                      " area.";
    out.back().default_val = "Junctions";
    out.back().expected = true;
    out.back().examples = { "Junctions",
                            "Picket_Fence_Junction" };

    out.emplace_back();
    out.back().name = "MinimumJunctionSeparation";
    out.back().desc = "The minimum distance between junctions in DICOM units. This number is used to"
                      " de-duplicate automatically detected junctions. Analysis results should not be"
                      " sensitive to the specific value.";
    out.back().default_val = "10.0";
    out.back().expected = true;
    out.back().examples = { "5.0", "10.0", "15.0", "25.0" };

    out.emplace_back();
    out.back().name = "ThresholdDistance";
    out.back().desc = "The threshold distance (in DICOM units) above which MLC separations are considered"
                      " to 'fail'. Each leaf pair is evaluated separately. Pass/fail status is also"
                      " indicated by setting the leaf axis contour colour (blue for pass, red for fail).";
    out.back().default_val = "1.0";
    out.back().expected = true;
    out.back().examples = { "0.5", "1.0", "2.0" };


    out.emplace_back();
    out.back().name = "LeafGapsFileName";
    out.back().desc = "A filename (or full path) in which to append leaf gap and offset data generated by this routine."
                      " The format is CSV. Leave empty to dump to generate a unique temporary file.";
    out.back().default_val = "";
    out.back().expected = true;
    out.back().examples = { "", "/tmp/somefile", "localfile.csv", "derivative_data.csv" };
    out.back().mimetype = "text/csv";


    out.emplace_back();
    out.back().name = "UserComment";
    out.back().desc = "A string that will be inserted into the output file which will simplify merging output"
                      " with differing parameters, from different sources, or using sub-selections of the data.";
    out.back().default_val = "";
    out.back().expected = true;
    out.back().examples = { "", "Using XYZ", "Patient treatment plan C" };


    out.emplace_back();
    out.back().name = "InteractivePlots";
    out.back().desc = "Whether to interactively show plots showing detected edges.";
    out.back().default_val = "false";
    out.back().expected = true;
    out.back().examples = { "true", "false" };

    return out;
}

Drover AnalyzePicketFence(Drover DICOM_data, OperationArgPkg OptArgs, std::map<std::string,std::string> /*InvocationMetadata*/, std::string FilenameLex){

    Explicator X(FilenameLex);

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();

    auto MLCModel = OptArgs.getValueStr("MLCModel").value();

    const auto MLCROILabel = OptArgs.getValueStr("MLCROILabel").value();
    const auto JunctionROILabel = OptArgs.getValueStr("JunctionROILabel").value();

    const auto MinimumJunctionSeparation = std::stod( OptArgs.getValueStr("MinimumJunctionSeparation").value() );

    const auto ThresholdDistance = std::stod( OptArgs.getValueStr("ThresholdDistance").value() );

    auto LeafGapsFileName = OptArgs.getValueStr("LeafGapsFileName").value();

    const auto UserComment = OptArgs.getValueStr("UserComment");

    const auto InteractivePlotsStr = OptArgs.getValueStr("InteractivePlots").value();

    //-----------------------------------------------------------------------------------------------------------------
    const auto regex_true = std::regex("^tr?u?e?$", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);

    const auto regex_none  = std::regex("^no?n?e?$", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto regex_first = std::regex("^fi?r?s?t?$", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto regex_last  = std::regex("^la?s?t?$", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto regex_all   = std::regex("^al?l?$",   std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);

    const auto regex_VMLC80  = std::regex("^va?r?i?a?n?m?i?l?l?e?n?n?i?u?m?m?l?c?80$", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto regex_VMLC120 = std::regex("^va?r?i?a?n?mille?n?n?i?u?m?m?l?c?120$", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    const auto regex_VHD120  = std::regex("^va?r?i?a?n?hd120$", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);

    const auto NormalizedMLCROILabel = X(MLCROILabel);
    const auto NormalizedJunctionROILabel = X(JunctionROILabel);

    const auto InteractivePlots = std::regex_match(InteractivePlotsStr, regex_true);

    if( !std::regex_match(ImageSelectionStr, regex_none)
    &&  !std::regex_match(ImageSelectionStr, regex_first)
    &&  !std::regex_match(ImageSelectionStr, regex_last)
    &&  !std::regex_match(ImageSelectionStr, regex_all) ){
        throw std::invalid_argument("Image selection is not valid. Cannot continue.");
    }

    std::vector< YgorMathPlottingGnuplot::Shuttle<samples_1D<double>> > junction_plot_shtl;
    std::vector< YgorMathPlottingGnuplot::Shuttle<samples_1D<double>> > leaf_plot_shtl;

    auto iap_it = DICOM_data.image_data.begin();
    if(false){
    }else if(std::regex_match(ImageSelectionStr, regex_none)){ iap_it = DICOM_data.image_data.end();
    }else if(std::regex_match(ImageSelectionStr, regex_last)){
        if(!DICOM_data.image_data.empty()) iap_it = std::prev(DICOM_data.image_data.end());
    }
    while(iap_it != DICOM_data.image_data.end()){

        if((*iap_it)->imagecoll.images.empty()) throw std::invalid_argument("Unable to find an image to analyze.");

        planar_image<float, double> *animg = &( (*iap_it)->imagecoll.images.front() );
        const auto row_unit = animg->row_unit;
        const auto col_unit = animg->col_unit;
        //const auto ort_unit = row_unit.Cross(col_unit);

        const auto corner_R = animg->position(0, 0); // A fixed point from which the profiles will be relative to. Better to use Isocentre instead! TODO
        const auto sample_spacing = std::max(animg->pxl_dx, animg->pxl_dy); // The sampling frequency for profiles.

        const auto NaN = std::numeric_limits<double>::quiet_NaN();
        const auto NaN_vec = vec3<double>(NaN, NaN, NaN);

        const std::string JunctionLineColour = "blue";

        std::string PatientID;
        if( auto o = animg->GetMetadataValueAs<std::string>("PatientID") ){
            PatientID = o.value();
        }else if( auto o = animg->GetMetadataValueAs<std::string>("StudyInstanceUID") ){
            PatientID = o.value();
        }else{
            PatientID = "unknown_patient";
        }

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
        auto StationName = animg->GetMetadataValueAs<std::string>("StationName");
        {

            const auto regex_FVAREA2TB = std::regex(".*FVAREA2TB.*", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
            const auto regex_FVAREA4TB = std::regex(".*FVAREA4TB.*", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
            const auto regex_FVAREA6TB = std::regex(".*FVAREA6TB.*", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);

            if(false){
            }else if( std::regex_match(StationName.value(), regex_FVAREA2TB)
                  ||  std::regex_match(StationName.value(), regex_FVAREA4TB)
                  ||  std::regex_match(StationName.value(), regex_FVAREA6TB) ){
                MLCModel = "VarianMillenniumMLC120";
            }else{
                MLCModel = "VarianMillenniumMLC80";
            }
        }


        //---------------------------------------------------------------------------
        // Extract the junction and leaf pair travel axes.
        auto BeamLimitingDeviceAngle = animg->GetMetadataValueAs<std::string>("BeamLimitingDeviceAngle");
        auto rot_ang = std::stod( BeamLimitingDeviceAngle.value() ) * M_PI/180.0; // in radians.

        // We now assume that at 0 deg the leaves are aligned with row_unit ( hopefully [1,0,0] ).
        // NOTE: SIMPLIFYING ASSUMPTIONS HERE:
        //       1. at collimator 0 degrees the image column direction and aligns with the MLC leaf travel axis.
        //       2. the rotation axes is the z axes.
        // This transformation is WRONG, but should work in normal situations (gantry ~0, coll 0 or 90, image panel
        // is parallel to the isocentric plane (i.e., orthogonal to the CAX).
        const auto leaf_axis     = col_unit.rotate_around_z(rot_ang);
        const auto junction_axis = col_unit.rotate_around_z(rot_ang + 0.5*M_PI);


        //---------------------------------------------------------------------------
        //Create lines for leaf pairs computed from knowledge about each machine's MLC geometry.
        std::vector<line<double>> leaf_lines;
        {
            std::vector<double> mlc_offsets; // Closest point along leaf-pair line from CAX.

            //We have to project the MLC leaf pair positions according to the magnification at the image panel.
            auto RTImageSID = std::stod( animg->GetMetadataValueAs<std::string>("RTImageSID").value_or("1000.0") );
            auto magnify = [=](double CAX_offset){
                return CAX_offset * (RTImageSID / 1000.0);
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

            leaf_lines.clear();
            for(const auto &offset : mlc_offsets){
                const auto origin = vec3<double>(0.0, 0.0, 0.0); // NOTE: assuming isocentre is (0,0,0). TODO.
                                                                 //       leafs need to be explicitly projected onto the image plane!
                const auto pos = origin + (junction_axis * offset);
                leaf_lines.emplace_back( pos, pos + leaf_axis );
            }
        }

        //---------------------------------------------------------------------------
        //Generate leaf-pair profiles.
        std::vector<samples_1D<double>> leaf_profiles;
        leaf_profiles.reserve( leaf_lines.size() );
        {
            for(const auto &leaf_line : leaf_lines){

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

                if(edge_ints.size() != 2){
                    leaf_profiles.emplace_back();
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

                leaf_profiles.emplace_back();
                const bool inhibit_sort = true;
                const long int chan = 0;
                animg->pxl_dz = 1.0; // Give the image some thickness.
                const std::vector<double> sample_dists { -2.0, -1.5, -0.5, 0.0, 0.5, 1.0, 1.5, 2.0 };
                for(const auto &R : R_list){
                    const auto rel_R = (R - corner_R);
                    const auto l = rel_R.Dot(leaf_axis);

                    std::vector<double> samples; // Sample vicinity of the leaf line.
                    for(const auto &offset : sample_dists){
                        const auto offset_R = R + (junction_axis * offset * sample_spacing);
                        try{
                            const auto val = animg->value(offset_R, chan);
                            samples.push_back(val);
                        }catch(const std::exception &){}
                    }
                    if(!samples.empty()){
                        const auto avgd = Stats::Mean(samples);
                        leaf_profiles.back().push_back( { l, 0.0, avgd, 0.0 }, inhibit_sort );
                    }
                }
                leaf_profiles.back().stable_sort();
                animg->pxl_dz = 0.0; // TODO -- reset to existing value?
            }
        }

        //---------------------------------------------------------------------------
        // Detect junctions.
        std::vector<line<double>> junction_lines;
        {
            samples_1D<double> avgd_profile;
            size_t N_visible_leaves = 0;
            for(auto & profile : leaf_profiles){
                if(!profile.empty()){
                    avgd_profile = avgd_profile.Sum_With(profile);
                    ++N_visible_leaves;
                }
            }
            if(N_visible_leaves == 0) throw std::logic_error("No visible leaves. Cannot continue.");
            avgd_profile = avgd_profile.Multiply_With(1.0 / static_cast<double>(N_visible_leaves));

            avgd_profile.Average_Coincident_Data(0.5 * sample_spacing);   // Summation can produce MANY samples.
            //avgd_profile = avgd_profile.Resample_Equal_Spacing(sample_spacing);
            leaf_plot_shtl.emplace_back(avgd_profile, 
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

            junction_plot_shtl.emplace_back( avgd_profile2, "High-pass filtered Junction Profile" );

            //Now find all (local) peaks via the derivative of the crossing points.
            auto peaks = avgd_profile2.Peaks();

            //Merge peaks that are separated by a small distance. These can be spurious, or can result if there is some
            // MLC leaf overtravel.
            //
            // Note: We are generous here because the junction separation should be >2-3 mm or so.
            peaks.Average_Coincident_Data(0.45*MinimumJunctionSeparation);

            junction_plot_shtl.emplace_back( peaks, "Junction Profile Peaks" );

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
            //junction_plot_shtl.emplace_back( avgd_profile3, "Aspect Ratio Profile" );

            if(peaks.size() < 2) std::runtime_error("Leaf-leakage peaks not correctly detected. Please verify input.");
            samples_1D<double> filtered_peaks;
            for(auto p4_it = peaks.samples.begin(); p4_it != peaks.samples.end(); ++p4_it){
                double centre = (*p4_it)[0]; // peak centre.
                const auto SearchDistance = 0.25*MinimumJunctionSeparation;

                //Only bother looking at peaks that have enough surrounding room to estimate the aspect ratio.
                if(!isininc(sum3_xmin + SearchDistance, centre, sum3_xmax - SearchDistance)) continue;

                auto vicinity = avgd_profile3.Select_Those_Within_Inc(centre - SearchDistance, centre + SearchDistance);
                const auto ar = Aspect_Ratio(vicinity) * 2.0 * SearchDistance; //Aspect ratio with x rescaled to 1.

                //FUNCINFO("Aspect ratio = " << ar);
                if(std::isfinite(ar) && (ar > 0.15)){ // A fairly slight aspect ratio threshold is needed.
                    filtered_peaks.push_back( *p4_it );
                }
            }
            filtered_peaks.stable_sort();
            peaks = filtered_peaks;
            if(peaks.size() < 2) std::runtime_error("Leaf-leakage peaks incorrectly filtered out. Please verify input.");

            junction_plot_shtl.emplace_back( peaks, "Filtered Junction Profile Peaks" );

            for(const auto &peak4 : peaks.samples){
                const auto d = peak4[0]; // Projection of relative position onto the leaf axis unit vector.
                const auto R_peak = corner_R + (leaf_axis * d);
                junction_lines.emplace_back( R_peak, R_peak + junction_axis );
            }
        }

        //---------------------------------------------------------------------------
        //Determine if the junction separation is consistent. Prune junctions that are not consistent.
        std::vector<double> junction_cax_separations;
        std::vector<double> junction_separations;
        double min_junction_sep = std::numeric_limits<double>::quiet_NaN();
        double max_junction_sep = std::numeric_limits<double>::quiet_NaN();
        {
            auto determine_junction_separations = [&](std::vector<line<double>> j_lines) -> void {
                junction_cax_separations.clear();
                for(const auto &junction_line : j_lines){
                    const auto dR = (junction_line.R_0 - vec3<double>(0.0,0.0,0.0)); // 0,0,0 should be replaced with actual isocentre ... TODO.
                    const auto sep = dR.Dot(leaf_axis);
                    junction_cax_separations.push_back( sep );
                }
                junction_separations.clear();
                for(size_t i = 1; i < junction_cax_separations.size(); ++i){
                    const auto sep = (junction_cax_separations[i] - junction_cax_separations[i-1]);
                    junction_separations.push_back( sep );
                }

                min_junction_sep = Stats::Min(junction_separations);
                max_junction_sep = Stats::Max(junction_separations);
                return;
            };
            auto all_except = [](std::vector<line<double>> j_lines, long int k) -> std::vector<line<double>> {
                std::vector<line<double>> out;
                const auto N_lines = static_cast<long int>(j_lines.size());
                for(long int j = 0; j < N_lines; ++j){
                    if(j != k) out.push_back(j_lines[j]);
                }
                return out;
            };

            auto trimmed_junction_lines = junction_lines;
            determine_junction_separations(junction_lines);
            while( RELATIVE_DIFF(min_junction_sep, max_junction_sep) > 0.10 ){
                FUNCWARN("Junction separation is not consistent. Pruning the worst junction..");

                std::vector<double> rdiffs;
                const auto N_lines = static_cast<long int>(trimmed_junction_lines.size());
                for(long int j = 0; j < N_lines; ++j){
                    auto trimmed = all_except(trimmed_junction_lines, j);
                    determine_junction_separations( trimmed );
                    rdiffs.push_back( std::abs( RELATIVE_DIFF(min_junction_sep, max_junction_sep) ) );
                }

                auto worst_it = std::min_element(rdiffs.begin(), rdiffs.end());
                const auto k = static_cast<long int>( std::distance(rdiffs.begin(), worst_it) );

                // Recompute to regenerate side-effects.
                trimmed_junction_lines = all_except(trimmed_junction_lines, k);
                determine_junction_separations(trimmed_junction_lines);
            }
            junction_lines = trimmed_junction_lines;

            std::cout << "Junction-CAX separations: ";
            for(auto &d : junction_cax_separations) std::cout << d << "  ";
            std::cout << std::endl;

            std::cout << "Junction-junction separations: ";
            for(auto &d : junction_separations) std::cout << d << "  ";
            std::cout << std::endl;

            std::cout << "Minimum junction separation: " << min_junction_sep << std::endl;
            std::cout << "Maximum junction separation: " << max_junction_sep << std::endl;

        }


        //---------------------------------------------------------------------------
        // Examine each profile in the vicinity of the junction.
        std::vector<std::string> leaf_line_colour(leaf_lines.size(), "green");
        {
            for(size_t i = 0; i < leaf_lines.size(); ++i){
                const auto leaf_line = leaf_lines[i];

                //Find the leaf-junction intersection points (i.e., the nominal peak position).
                //
                // Note: We permit junction and leaf axes to be non-orthogonal, so have to test
                //       for the intersection point for each junction each time.
                std::vector<double> leaf_junction_int;
                for(const auto &junction_line : junction_lines){
                    // Find the intersection point in 3D.
                    auto R = NaN_vec;
                    if(!leaf_line.Closest_Point_To_Line(junction_line, R)) R = NaN_vec;
                    if(!R.isfinite()) throw std::runtime_error("Cannot compute leaf-junction intersection.");

                    // If the point is not within the image bounds, ignore it.
                    animg->pxl_dz = 1.0;
                    const auto within_img = animg->encompasses_point(R);
                    animg->pxl_dz = 0.0;
                    if(!within_img) continue;

                    // Project the point into the same 2D space as the leaf profiles.
                    const auto rel_R = (R - corner_R);
                    const auto l = rel_R.Dot(leaf_axis);
                    leaf_junction_int.push_back(l);
                }
                if(leaf_junction_int.empty()) continue;


                //Determine the true peak location in the vicinity of the junction.
                std::vector<double> peak_offsets;
                std::vector<double> peak_spreads;
                for(const auto &nominal_peak : leaf_junction_int){
                    const auto SearchDistance = 0.5*min_junction_sep;
                    const auto vicinity = leaf_profiles[i].Select_Those_Within_Inc(nominal_peak - SearchDistance,
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

                    std::vector<double> est_peaks;
                    std::vector<double> est_spread;
                    for(const auto &threshold : thresholds){
                        auto crossings = vicinity.Crossings(threshold);
                        const auto c_mmx = crossings.Get_Extreme_Datum_x();
                        est_peaks.emplace_back( 0.5 * (c_mmx.second[0] + c_mmx.first[0]) );
                        est_spread.emplace_back( c_mmx.second[0] - c_mmx.first[0] );
                    }
                    const auto peak_offset = Stats::Median(est_peaks) - nominal_peak;
                    const auto peak_spread = Stats::Median(est_spread);

                    if(std::isfinite(peak_offset) && std::isfinite(peak_spread)){
                        peak_offsets.push_back(peak_offset);
                        peak_spreads.push_back(peak_spread);

                        if(std::abs(peak_offset) > ThresholdDistance){
                            leaf_plot_shtl.emplace_back(vicinity,
                                                        "vicinity used for peak detection",
                                                        std::vector<std::pair<std::string,std::string>>({{"1:2","lines"}}) );
                        }
                    }
                }


                if(peak_offsets.size() != leaf_junction_int.size()){
                    throw std::runtime_error("Unable to find leaf profile peak near nominal junction.");
                }

                std::vector<double> adjacent_peak_diffs;
                for(size_t i = 1; i < peak_offsets.size(); ++i){
                    const auto sep = (peak_offsets[i] - peak_offsets[i-1]);
                    adjacent_peak_diffs.push_back( sep );
                }

                const auto max_abs_sep = std::max( std::abs(Stats::Min(peak_offsets)),
                                                   std::abs(Stats::Max(peak_offsets)) );
                const auto mean_abs_sep = Stats::Mean(peak_offsets);

                const auto max_adj_diff = std::max( std::abs(Stats::Min(adjacent_peak_diffs)),
                                                    std::abs(Stats::Max(adjacent_peak_diffs)) );
                const auto mean_adj_diff = Stats::Mean(adjacent_peak_diffs);
                
                const auto within_abs_sep  = (max_abs_sep < ThresholdDistance);
                const auto within_adj_diff = (max_adj_diff < ThresholdDistance);

                const auto min_spread = Stats::Min(peak_spreads);
                const auto max_spread = Stats::Max(peak_spreads);
                const auto mean_spread = Stats::Mean(peak_spreads);

/*
                std::cout << "Leaf " << i 
                          << "   max |actual-nominal| = " << std::setw(15) << max_abs_sep
                          << "  mean (actual-nominal) = " << std::setw(15) << mean_abs_sep
                          << (within_abs_sep ? " Pass" : "     Fail" )
                          << std::endl
                          << "   max |adjacent_diff.| = " << std::setw(15) << max_adj_diff
                          << "  mean (adjacent_diff.) = " << std::setw(15) << mean_adj_diff
                          << (within_adj_diff ? " Pass" : "     Fail" )
                          << std::endl;
*/

                //Report the findings. 
                FUNCINFO("Attempting to claim a mutex");
                try{
                    //File-based locking is used so this program can be run over many patients concurrently.
                    //
                    //Try open a named mutex. Probably created in /dev/shm/ if you need to clear it manually...
                    boost::interprocess::named_mutex mutex(boost::interprocess::open_or_create,
                                                           "dicomautomaton_operation_analyzepicketfence_mutex");
                    boost::interprocess::scoped_lock<boost::interprocess::named_mutex> lock(mutex);

                    if(LeafGapsFileName.empty()){
                        LeafGapsFileName = Get_Unique_Sequential_Filename("/tmp/dicomautomaton_evaluatenpf_", 6, ".csv");
                    }
                    const auto FirstWrite = !Does_File_Exist_And_Can_Be_Read(LeafGapsFileName);
                    std::fstream FO_pf(LeafGapsFileName, std::fstream::out | std::fstream::app);
                    if(!FO_pf){
                        throw std::runtime_error("Unable to open file for reporting picket fence data. Cannot continue.");
                    }
                    if(FirstWrite){ // Write a CSV header.
                        FO_pf << "PatientID,"
                              << "StationName,"
                              << "LeafNumber,"

                              << "\"max |actual-nominal| position\","
                              << "\"mean (actual-nominal) position\","
                              << "\"Pass abs. sep. threshold?\","

                              << "\"max |adjacent difference|\","
                              << "\"mean (adjacent difference)\","
                              << "\"Pass adj. diff. threshold?\","

                              << "\"min dosimetric spread\","
                              << "\"max dosimetric spread\","
                              << "\"mean dosimetric spread\","
                              << "UserComment"
                              << std::endl;
                    }
                    FO_pf << PatientID << ","
                          << StationName.value_or("") << ","
                          << (i+1) << ","  // MLC numbers traditionally start at 1.

                          << max_abs_sep << ","
                          << mean_abs_sep << ","
                          << (within_abs_sep ? "pass" : "fail") << ","

                          << max_adj_diff << ","
                          << mean_adj_diff << ","
                          << (within_adj_diff ? "pass" : "fail") << ","

                          << min_spread << ","
                          << max_spread << ","
                          << mean_spread << ","
                          << UserComment.value_or("")
                          << std::endl;
                    FO_pf.flush();
                    FO_pf.close();

                }catch(const std::exception &e){
                    FUNCERR("Unable to write to output file: '" << e.what() << "'");
                }

                //If failed, display the profile for debugging.
                if( !within_abs_sep || !within_adj_diff ){
                    leaf_plot_shtl.emplace_back( leaf_profiles[i],
                                                 "Failed leaf "_s + std::to_string(i),
                                                 std::vector<std::pair<std::string,std::string>>({{"1:2","lines"}}) );
                }

                //Record whether this leaf passed or failed visually.
                if( within_abs_sep && within_adj_diff ){
                    leaf_line_colour[i] = "blue";
                }else{
                    leaf_line_colour[i] = "red";
                }
            }

        }

        //---------------------------------------------------------------------------
        //Add thin contours for visually inspecting the location of the peaks.
        if(DICOM_data.contour_data == nullptr){
            std::unique_ptr<Contour_Data> output (new Contour_Data());
            DICOM_data.contour_data = std::move(output);
        }

        DICOM_data.contour_data->ccs.emplace_back();
        {
            auto contour_metadata = animg->metadata;
            contour_metadata["ROIName"] = MLCROILabel;
            contour_metadata["NormalizedROIName"] = NormalizedMLCROILabel;
            for(size_t i = 0; i < leaf_lines.size(); ++i){
                const auto leaf_line = leaf_lines[i];
                contour_metadata["OutlineColour"] = leaf_line_colour[i];

                try{ // Will throw if grossly out-of-bounds, but it's a pain to pre-filter -- ignore exceptions for now... TODO
                    Inject_Thin_Line_Contour(*animg,
                                             leaf_line,
                                             DICOM_data.contour_data->ccs.back(),
                                             contour_metadata);
                }catch(const std::exception &){};
            }
        }


        //---------------------------------------------------------------------------
        //Add thin contours for visually inspecting the location of the junctions.
        DICOM_data.contour_data->ccs.emplace_back();
        {
            auto contour_metadata = animg->metadata;
            contour_metadata["ROIName"] = JunctionROILabel;
            contour_metadata["NormalizedROIName"] = NormalizedJunctionROILabel;
            contour_metadata["OutlineColour"] = JunctionLineColour;
            for(const auto &junction_line : junction_lines){
                try{ // Will throw if grossly out-of-bounds, but it's a pain to pre-filter -- ignore exceptions for now... TODO
                    Inject_Thin_Line_Contour(*animg,
                                             junction_line,
                                             DICOM_data.contour_data->ccs.back(),
                                             contour_metadata);
                }catch(const std::exception &){};
            }
        }


        //---------------------------------------------------------------------------
        //Display some interactive plots.
        if(InteractivePlots){

            // Add an average leaf profile to compare failed leaf profiles to.
            if(false){
                samples_1D<double> avgd;
                size_t N_visible_leaves = 0;
                for(auto & profile : leaf_profiles){
                    if(!profile.empty()){
                        avgd = avgd.Sum_With(profile);
                        ++N_visible_leaves;
                    }
                }
                if(N_visible_leaves == 0) throw std::logic_error("No visible leaves. Cannot continue.");
                avgd = avgd.Multiply_With(1.0 / static_cast<double>(N_visible_leaves));
                avgd.Average_Coincident_Data(0.5 * sample_spacing);   // Summation can produce MANY samples.

                leaf_plot_shtl.emplace_back(avgd, 
                                            "Average leaf profile",
                                            std::vector<std::pair<std::string,std::string>>({{"1:2","lines"}}) );
            }

            // Plot leaf-pair profiles that were over tolerance.
            if(true){
                YgorMathPlottingGnuplot::Plot<double>(leaf_plot_shtl, "Failed leaf-pair profiles", "DICOM position", "Pixel Intensity");
            }

            // Plot (all) individual leaf-pair profiles. (This is onnly relevant in a debugging or development context.)
            if(false){
                std::vector< YgorMathPlottingGnuplot::Shuttle<samples_1D<double>> > plot_shtl;
                size_t i = 0;
                for(auto & profile : leaf_profiles){
                    ++i;
                    if(!profile.empty()) plot_shtl.emplace_back( profile, "Leaf "_s + std::to_string(i) );
                }
                YgorMathPlottingGnuplot::Plot<double>(plot_shtl, "All visible leaf-pair profiles", "DICOM position", "Pixel Intensity");
            }

            // Plot junction profiles.
            if(false){
                YgorMathPlottingGnuplot::Plot<double>(junction_plot_shtl, "Junction profiles", "DICOM position", "Pixel Intensity");
            }
        }

        
        // Loop control.
        ++iap_it;
        if(std::regex_match(ImageSelectionStr, regex_first)) break;
    }

    return DICOM_data;
}
