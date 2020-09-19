//SimulateRadiograph.cc - A part of DICOMautomaton 2019. Written by hal clark.

#include <iostream>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <string>    
#include <vector>
#include <set> 
#include <map>
#include <unordered_map>
#include <list>
#include <functional>
#include <thread>
#include <array>
#include <mutex>
#include <limits>
#include <cmath>
#include <regex>

#include <getopt.h>           //Needed for 'getopts' argument parsing.
#include <cstdlib>            //Needed for exit() calls.
#include <utility>            //Needed for std::pair.
#include <algorithm>
#include <optional>

#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMathIOOFF.h"    //Needed for WritePointsToOFF(...)
#include "YgorMathPlottingGnuplot.h" //Needed for YgorMathPlottingGnuplot::*.
#include "YgorMathChebyshev.h" //Needed for cheby_approx class.
#include "YgorStats.h"        //Needed for Stats:: namespace.
#include "YgorFilesDirs.h"    //Needed for Does_File_Exist_And_Can_Be_Read(...), etc..
#include "YgorContainers.h"   //Needed for bimap class.
#include "YgorPerformance.h"  //Needed for YgorPerformance_dt_from_last().
#include "YgorAlgorithms.h"   //Needed for For_Each_In_Parallel<..>(...)
#include "YgorArguments.h"    //Needed for ArgumentHandler class.
#include "YgorString.h"       //Needed for GetFirstRegex(...)
#include "YgorImages.h"
#include "YgorImagesIO.h"
#include "YgorImagesPlotting.h"

#include "Explicator.h"       //Needed for Explicator class.

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../Thread_Pool.h"
#include "../Dose_Meld.h"

#include "../YgorImages_Functors/Grouping/Misc_Functors.h"

#include "SimulateRadiograph.h"



OperationDoc OpArgDocSimulateRadiograph(){
    OperationDoc out;
    out.name = "SimulateRadiograph";

    out.desc = 
        "This routine uses ray marching and volumetric sampling to simulate radiographs using a CT image array."
        " Voxels are assumed to have intensities in HU. A simplisitic conversion"
        " from CT number (in HU) to relative electron density (see note below) is performed for marched"
        " rays.";

    out.notes.emplace_back(
        "Images must be regular."
        // Note: while this operation could be implemented without requiring regularity, it is much faster to
        // require it. If this functionality is required then modify this operation.
    );
    out.notes.emplace_back(
        "This operation currently takes a simplistic approach and should only be used for purposes"
        " where the simulated radiograph contrast can be tuned and validated (e.g., in a relative way)."
    );
    out.notes.emplace_back(
        "This operation assumes mass density (in g/cm^3^) and relative"
        " electron density (dimensionless; relative to electron density of water, which is $3.343E23$ cm^3^)"
        " are numerically equivalent. This assumption appears to be reasonable for bulk human tissue"
        " (arXiv:1508.00226v1)."
    );

    out.args.emplace_back();
    out.args.back() = IAWhitelistOpArgDoc();
    out.args.back().name = "ImageSelection";
    out.args.back().default_val = "last";


    out.args.emplace_back();
    out.args.back().name = "Filename";
    out.args.back().desc = "The filename (or full path) to which the simulated image will be saved to."
                           " The format is FITS. Leaving empty will result in a unique name being generated.";
    out.args.back().default_val = "";
    out.args.back().expected = true;
    out.args.back().examples = { "", "./img.fits", "sim_radiograph.fits", "/tmp/out.fits" };
    out.args.back().mimetype = "image/fits";


    out.args.emplace_back();
    out.args.back().name = "SourcePosition";
    out.args.back().desc = "This parameter controls where the virtual point source is."
                           " Both absolute and relative positioning are available."
                           " A source located at point (1.0, -2.3, 4.5) in the DICOM coordinate system of a given image"
                           " can be specified as 'absolute(1.0, -2.3, 4.5)'."
                           " A source located relative to the image centre by offset (10.0, -23.4, 45.6) in the DICOM"
                           " coordinate system of a given image can be specified as 'relative(10.0, -23.4, 45.6)'."
                           " Relative offsets must be specified relative to the image centre."
                           " Note that DICOM units (i.e., mm) are used for all coordinates.";
    out.args.back().default_val = "relative(0.0, 1000.0, 20.0)";
    out.args.back().expected = true;
    out.args.back().examples = { "relative(0.0, 1610.0, 20.0)",
                                 "absolute(-123.0, 123.0, 1.23)" };


    out.args.emplace_back();
    out.args.back().name = "AttenuationScale";
    out.args.back().desc = "This parameter globally scales all attenuation factors derived via ray marching."
                           " Adjusting this parameter will alter the radiograph image contrast the exponential"
                           " attenuation model;"
                           " numbers within (0:1) will result in less attenuation, whereas numbers within (1:inf) will"
                           " result in more attenuation. Thin or low-mass subjects might require artifically increased"
                           " attenuation, whereas thick or high-mass subjects might require artifically decreased"
                           " attenuation. Setting this number to 1 will result in no scaling."
                           " This parameter has units 1/length, and the magnitude should *roughly* correspond with the"
                           " inverse of about $3\\times$ the length transited by a typical ray (in mm).";
    out.args.back().default_val = "0.001";
    out.args.back().expected = true;
    out.args.back().examples = { "1.0E-4", "0.001", "0.01", "0.1", "1.0", "10.0", "1E2" };


    out.args.emplace_back();
    out.args.back().name = "ImageModel";
    out.args.back().desc = "This parameter adjusts how the final image is constructed."
                           " As rays transit a voxel, the approximate transit distance is multiplied with the voxel's"
                           " attenuation coefficient (i.e., $\\mu \\cdot dL$) to give the ray's attenuation."
                           " The sum of all per-voxel attenuations constitutes the total attenuation."
                           " There are many ways this information can be converted into an image."
                           ""
                           " First, the 'attenuation-length' model directly outputs the total attenuation for each ray."
                           " The simulated image's pixels will contain the total attenuation for one ray."
                           " It will almost always provide an image since the attenutation is not performed."
                           " This can be thought of as a log transform of a standard radiograph."
                           ""
                           " Second, the 'exponential' model performs the attenuation assuming the radiation beam is"
                           " monoenergetic, narrow, and has the same energy spectrum as the original imaging device."
                           " This model produces a typical radiograph, where each image pixel contains"
                           " $1 - \\exp{-\\sum \\mu \\cdot dL}$. Note that the values will all $\\in [0:1]$"
                           " (i.e., Hounsfield units are *not* used)."
                           " The overall contrast can be adjusted using the AttenuationScale parameter, however"
                           " it is easiest to assess a reasonable tuning factor by inspecting the image produced by"
                           " the 'attenutation-length' model.";
    out.args.back().default_val = "attenuation-length";
    out.args.back().expected = true;
    out.args.back().examples = { "attenuation-length", "exponential" };


    out.args.emplace_back();
    out.args.back().name = "Rows";
    out.args.back().desc = "The number of rows that the simulated radiograph will contain."
                           " Note that the field of view is determined separately from the number of rows and columns,"
                           " so increasing the row count will only result in increased spatial resolution.";
    out.args.back().default_val = "512";
    out.args.back().expected = true;
    out.args.back().examples = { "100", "500", "2000" };


    out.args.emplace_back();
    out.args.back().name = "Columns";
    out.args.back().desc = "The number of columns that the simulated radiograph will contain."
                           " Note that the field of view is determined separately from the number of rows and columns,"
                           " so increasing the column count will only result in increased spatial resolution.";
    out.args.back().default_val = "512";
    out.args.back().expected = true;
    out.args.back().examples = { "100", "500", "2000" };


    return out;
}



Drover SimulateRadiograph(Drover DICOM_data, 
                          const OperationArgPkg& OptArgs,
                          const std::map<std::string,std::string>& /*InvocationMetadata*/,
                          const std::string& /*FilenameLex*/, const std::list<OperationArgPkg>& /*Children*/){
    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();

    auto FilenameStr = OptArgs.getValueStr("Filename").value();

    const auto SourcePositionStr = OptArgs.getValueStr("SourcePosition").value();

    const auto AttenuationScale = std::stod( OptArgs.getValueStr("AttenuationScale").value() );

    const auto ImageModelStr = OptArgs.getValueStr("ImageModel").value();

    const auto RadiographRows = std::stol( OptArgs.getValueStr("Rows").value() );
    const auto RadiographColumns = std::stol( OptArgs.getValueStr("Columns").value() );

    //-----------------------------------------------------------------------------------------------------------------
    const auto Channel = 0;

    const auto regex_rel = Compile_Regex("^re?l?a?t?i?v?e?.*$");
    const auto regex_abs = Compile_Regex("^ab?s?o?l?u?t?e?.*$");

    const auto regex_mudl = Compile_Regex("^at?t?e?n?u?a?t?i?o?n?[-_]?l?e?n?g?t?h?$");
    const auto regex_exp = Compile_Regex("^expo?n?e?n?t?i?a?l?$");

    const bool spos_is_relative = std::regex_match(SourcePositionStr, regex_rel);
    const bool spos_is_absolute = std::regex_match(SourcePositionStr, regex_abs);

    const bool imgmodel_is_mudl = std::regex_match(ImageModelStr, regex_mudl);
    const bool imgmodel_is_exp  = std::regex_match(ImageModelStr, regex_exp);

    const vec3<double> vec3_nan( std::numeric_limits<double>::quiet_NaN(),
                                 std::numeric_limits<double>::quiet_NaN(),
                                 std::numeric_limits<double>::quiet_NaN() );
    //-----------------------------------------------------------------------------------------------------------------
    const auto machine_eps = std::sqrt( 10.0 * std::numeric_limits<double>::epsilon() );

    vec3<double> source_position = vec3_nan;
    {
        auto split = SplitStringToVector(SourcePositionStr, '(', 'd');
        split = SplitVector(split, ')', 'd');
        split = SplitVector(split, ',', 'd');

        std::vector<double> numbers;
        for(const auto &w : split){
           try{
               const auto x = std::stod(w);
               numbers.emplace_back(x);
           }catch(const std::exception &){ }
        }
        if(numbers.size() != 3){
            throw std::invalid_argument("Unable to parse grid/cube shape parameters. Cannot continue.");
        }

        source_position = vec3<double>( numbers.at(0),
                                        numbers.at(1),
                                        numbers.at(2) );
        if(!source_position.isfinite()) throw std::invalid_argument("Source position invalid.");
    }

    auto IAs_all = All_IAs( DICOM_data );
    //auto IAs = Whitelist( IAs_all, "Modality@CT" );
    auto IAs = Whitelist( IAs_all, ImageSelectionStr );
    if(IAs.empty()){
        throw std::invalid_argument("No image arrays selected. Cannot continue.");
    }
    if(IAs.size() != 1){
        throw std::invalid_argument("Multiple image arrays selected. Cannot continue.");
    }
    auto img_arr_ptr = (*(IAs.front()));
    if(img_arr_ptr == nullptr){
        throw std::runtime_error("Encountered a nullptr when expecting a valid Image_Array ptr.");
    }else if(img_arr_ptr->imagecoll.images.empty()){
        throw std::runtime_error("Encountered a Image_Array without valid images -- no images found.");
    }


    // Ensure the image array is regular. (This will allow us to use a faster postion-to-image lookup.)
    {
        std::list<std::reference_wrapper<planar_image<float,double>>> selected_imgs;
        for(auto &img : img_arr_ptr->imagecoll.images){
            selected_imgs.push_back( std::ref(img) );
        }

        if(!Images_Form_Regular_Grid(selected_imgs)){
            throw std::invalid_argument("Images do not form a rectilinear grid. Cannot continue");
        }
    }

    const auto row_unit = img_arr_ptr->imagecoll.images.front().row_unit.unit();
    const auto col_unit = img_arr_ptr->imagecoll.images.front().col_unit.unit();
    const auto img_unit = col_unit.Cross(row_unit).unit();

    planar_image_adjacency<float,double> img_adj( {}, { { std::ref(img_arr_ptr->imagecoll) } }, img_unit );
    if(img_adj.int_to_img.empty()){
        throw std::logic_error("Image array contained no images. Cannot continue.");
    }

    const auto pxl_dx = img_arr_ptr->imagecoll.images.front().pxl_dx;
    const auto pxl_dy = img_arr_ptr->imagecoll.images.front().pxl_dy;
    const auto pxl_dz = img_arr_ptr->imagecoll.images.front().pxl_dz;
    const auto pxl_diagonal_sq_length = (pxl_dx*pxl_dx + pxl_dy*pxl_dy + pxl_dz*pxl_dz);

    const auto grid_zero = img_adj.index_to_image(0).get().position(0,0); // Centre of the (0,0,0) voxel.
    const auto img_bps = img_adj.bounding_volume_planes;

    const auto N_rows = static_cast<long int>(img_arr_ptr->imagecoll.images.front().rows);
    const auto N_cols = static_cast<long int>(img_arr_ptr->imagecoll.images.front().columns);
    const auto N_imgs = static_cast<long int>(img_adj.int_to_img.size());

    // Determine an appropriate radiograph orientation.
    const auto img_centre = img_arr_ptr->imagecoll.center(); // TODO: For TBI, should be at the t0 point (i.e., at the level of the lung).
    auto ray_source = vec3_nan;
    if(spos_is_relative){
        ray_source = img_centre + source_position;  // Should be relative to voxel at (0,0,0), not image centre.
    }else if(spos_is_absolute){
        ray_source = source_position;
    }else{
        throw std::logic_error("Unknown option. Cannot continue.");
    }
    if(ray_source.distance(img_centre) < machine_eps){
        throw std::invalid_argument("Ray source point cannot coincide with image centre. Refusing to continue.");
    }
    const line<double> source_centre_line(ray_source, img_centre); 

    // Determine which way will be 'up' in the radiograph.
    const auto ray_unit = (img_centre - ray_source).unit();
    auto rg_up = img_unit;
    auto rg_left = rg_up.Cross(ray_unit).unit();
    if(!ray_unit.GramSchmidt_orthogonalize(rg_up, rg_left)){
        throw std::invalid_argument("Cannot orthogonalize radiograph orientation unit vectors. Cannot continue.");
    }
    rg_up = rg_up.unit();
    rg_left = rg_left.unit();

    FUNCINFO("Proceeding with radiograph into-plane orientation unit vector: " << ray_unit);
    FUNCINFO("Proceeding with radiograph leftward orientation unit vector: " << rg_left);
    FUNCINFO("Proceeding with radiograph upward orientation unit vector: " << rg_up);
    FUNCINFO("Proceeding with ray source at: " << ray_source);
    FUNCINFO("Proceeding with image centre at: " << img_centre);
    FUNCINFO("Proceeding with ray source - image centre line: " << source_centre_line);

    // Confirm the bounding planes are all correctly oriented.
    for(const auto & img_bp : img_bps){
        if(!img_bp.Is_Point_Above_Plane(img_centre)){
            throw std::logic_error("Bounding planes are not inward oriented. Refusing to continue.");
            // Note: could just re-orient them here...
        }
    }
    if(img_bps.size() != 6){
        throw std::logic_error("Incorrect number of bounding planes provided. Cannot continue.");
    }

    // Pre-compute whether the ray source position is bounded within the image volume.
    bool ray_source_is_within_image_volume = false;
    {
        long int N_bounds = 0;
        for(const auto & img_bp : img_bps){
            N_bounds += (img_bp.Is_Point_Above_Plane(ray_source)) ? 1L : 0L;
        }
        ray_source_is_within_image_volume = (N_bounds == 6);
    }

    // Encode the image geometry as contours for volumetric bounds determination.
    contour_collection<double> cc;
    for(const auto &animg : img_arr_ptr->imagecoll.images){
        cc.contours.emplace_back();
        cc.contours.back().closed = true;
        cc.contours.back().points = animg.corners2D();

        const auto perimeter = cc.contours.back().Perimeter();
        if(!std::isfinite(perimeter)){
            throw std::invalid_argument("Encountered non-finite contour. Cannot continue.");
        }
    }
    std::list<std::reference_wrapper<contour_collection<double>>> cc_ROIs = { std::ref(cc) };

    //------------------------
    // Create a detector that will encompass the images.
    //
    // Note: We are generous here because the source is a single point. The image projection will therefore be
    //       magnified. If the source is too close the projection will 
    double grid_x_margin = 5.0;
    double grid_y_margin = 5.0;
    double grid_z_margin = 5.0;

    //Generate a grid volume bounding the ROI(s). We ask for many images in order to compress the pxl_dz taken by each.
    // Only two are actually allocated.
    const auto NumberOfPanelImages = 1000L;
    auto sd_image_collection = Symmetrically_Contiguously_Grid_Volume<float,double>(
             cc_ROIs, 
             grid_x_margin, grid_y_margin, grid_z_margin,
             RadiographRows, RadiographColumns, /*number_of_channels=*/ 1, NumberOfPanelImages, 
             source_centre_line, (rg_up * -1.0), rg_left,
             /*pixel_fill=*/ 0.0, 
             /*only_top_and_bottom=*/ true);

    //Get handles for each image.
    planar_image<float, double> *DetectImg = &(*std::next(sd_image_collection.images.begin(),0));
    planar_image<float, double> *OrthoSrcImg = &(*std::next(sd_image_collection.images.begin(),1));

    // Confirm the detector image is oriented correctly.
    //
    // Note: the detector will always be on the opposite side of the image centre compared with the source point
    // (i.e., the source will always points towards the image centre).
    {
        const auto dICSP = img_centre - source_position;
        const auto dDPIC = DetectImg->center() - img_centre;
        if(dICSP.Dot(dDPIC) < 0.0){
            std::swap(DetectImg, OrthoSrcImg);
        }
    }

    DetectImg->metadata["Description"] = "Virtual radiograph detector";
    OrthoSrcImg->metadata["Description"] = "(unused)";

    const auto detector_plane = DetectImg->image_plane();
    const auto orthosrc_plane = OrthoSrcImg->image_plane();

    //------------------------
    // March rays through the image data.
    {
        asio_thread_pool tp;
        std::mutex printer; // Who gets to print to the console and iterate the counter.
        long int completed = 0;

        for(long int RadiographRow = 0; RadiographRow < RadiographRows; ++RadiographRow){
            tp.submit_task([&,RadiographRow]() -> void {
                for(long int RadiographCol = 0; RadiographCol < RadiographColumns; ++RadiographCol){

                    // Construct a line segment between the source and detector. 
                    const auto ray_terminus = DetectImg->position(RadiographRow, RadiographCol);
                    const auto ray_line = line<double>(ray_source, ray_terminus);

                    // Find the intersection of the ray with the detector bounding planes.
                    vec3<double> detector_panel_bp_intersection;
                    if(!detector_plane.Intersects_With_Line_Once(ray_line, detector_panel_bp_intersection)){
                        throw std::logic_error("Ray line does not intersect far image array bounding plane. Cannot continue.");
                    }
                    const auto ray_ls = line_segment<double>(ray_source, detector_panel_bp_intersection);

                    // Find the intersections of the ray and the bounding box containing the images.
                    std::vector<vec3<double>> bp_intersections;
                    for(const auto & img_bp : img_bps){
                        vec3<double> P;
                        //if(img_bp.Intersects_With_Line_Once(ray_line, P)){
                        if(img_bp.Intersects_With_Line_Segment_Once(ray_ls, P)){

                            // Determine if the intersection point is on a face of the cube.
                            const auto bp_centre = img_bp.Project_Onto_Plane_Orthogonally(img_centre);
                            const auto dP = (P - bp_centre);
                            const auto dP_row = std::abs(dP.Dot(row_unit));
                            const auto dP_col = std::abs(dP.Dot(col_unit));
                            const auto dP_img = std::abs(dP.Dot(img_unit));

                            const auto max_row = (static_cast<double>(N_rows) * pxl_dx * 0.5);
                            const auto max_col = (static_cast<double>(N_cols) * pxl_dy * 0.5);
                            const auto max_img = (static_cast<double>(N_imgs) * pxl_dz * 0.5);

                            if( (dP_row <= max_row)
                            &&  (dP_col <= max_col)
                            &&  (dP_img <= max_img) ){
                                bp_intersections.emplace_back(P);
                            }
                        }
                    }

                    // Explicitly add the ray source point if it is bounded within the image volume.
                    if(ray_source_is_within_image_volume){
                        bp_intersections.emplace_back(ray_source);
                    }

                    // Skip rays that do not intersect the image volume twice.
                    if(bp_intersections.size() != 2){
                        continue;
                    }

                    // Explicitly state the ray start and end positions using identified bounding-box intersection points.
                    const vec3<double> ray_start = bp_intersections[0];
                    const vec3<double> ray_end = bp_intersections[1];
                    const auto ray_direction = (ray_end - ray_start).unit();
                    const auto ray_total_sq_dist = ray_end.sq_dist(ray_start);

                    // Determine whether moving from tail to head along the ray will increase or decrease the
                    // row/col/img coordinates. Note that the direction will never change.
                    const long int incr_row = (row_unit.Dot(ray_direction) < 0.0) ? -1L : 1L;
                    const long int incr_col = (col_unit.Dot(ray_direction) < 0.0) ? -1L : 1L;
                    const long int incr_img = (img_unit.Dot(ray_direction) < 0.0) ? -1L : 1L;

                    // Determine the amount the ray will traverse due to incrementing i, j, or k individually.
                    const auto true_ray_pos_dR_incr_row = ray_direction * (std::abs(row_unit.Dot(ray_direction)) * pxl_dx);
                    const auto true_ray_pos_dR_incr_col = ray_direction * (std::abs(col_unit.Dot(ray_direction)) * pxl_dy);
                    const auto true_ray_pos_dR_incr_img = ray_direction * (std::abs(img_unit.Dot(ray_direction)) * pxl_dz);

                    const auto true_ray_pos_dR_incr_row_length = true_ray_pos_dR_incr_row.length();
                    const auto true_ray_pos_dR_incr_col_length = true_ray_pos_dR_incr_col.length();
                    const auto true_ray_pos_dR_incr_img_length = true_ray_pos_dR_incr_img.length();

                    const auto blocky_ray_pos_dR_incr_row = row_unit * (pxl_dx * static_cast<double>(incr_row));
                    const auto blocky_ray_pos_dR_incr_col = col_unit * (pxl_dy * static_cast<double>(incr_col));
                    const auto blocky_ray_pos_dR_incr_img = img_unit * (pxl_dz * static_cast<double>(incr_img));

                    // Determine the pseudo integer coordinates for the starting point.
                    //
                    // Note that these coordinates will not necessarily intersect any real voxels. They are defined only
                    // by the (infinite) regular grid that coincides with the real voxels.
                    const auto ray_start_grid_offset = ray_start - grid_zero;
                    const auto ray_start_row_index = static_cast<long int>( std::round( ray_start_grid_offset.Dot(row_unit)/pxl_dx ) );
                    const auto ray_start_col_index = static_cast<long int>( std::round( ray_start_grid_offset.Dot(col_unit)/pxl_dy ) );
                    const auto ray_start_img_index = static_cast<long int>( std::round( ray_start_grid_offset.Dot(img_unit)/pxl_dz ) );

                    long int ray_i = ray_start_row_index;
                    long int ray_j = ray_start_col_index;
                    long int ray_k = ray_start_img_index;

                    vec3<double> true_ray_pos = ray_start;
                    vec3<double> blocky_ray_pos = grid_zero + row_unit * (static_cast<double>(ray_i) * pxl_dx)
                                                            + col_unit * (static_cast<double>(ray_j) * pxl_dy)
                                                            + img_unit * (static_cast<double>(ray_k) * pxl_dz);

                    const auto ray_start_side_of_terminus_plane = detector_plane.Is_Point_Above_Plane(ray_start);

                    // Each time the ray samples the CT number, the ray is simulated to have interacted with the medium
                    // for the length of the ray advancement.
                    //
                    // For purposes of simulating a radiograph, the remaining fractional ray intensity could be
                    // immediately reduced by multiplying by a factor of exp(-attenuation_coeff*dL). However, it is
                    // easier to sum all the attenuation_coeff*dL contributions and apply the reduction factor once at
                    // the end.
                    double accumulated_attenuation_length_product = 0.0;
                    double last_move_dist = 0.0;

                    while(true){
                        // Test which single increment (either i, j, or k) remaing the closest to the ray line.
                        const auto cand_pos_i = blocky_ray_pos + blocky_ray_pos_dR_incr_row;
                        const auto cand_pos_j = blocky_ray_pos + blocky_ray_pos_dR_incr_col;
                        const auto cand_pos_k = blocky_ray_pos + blocky_ray_pos_dR_incr_img;

                        const auto cand_sq_dist_i = ray_line.Sq_Distance_To_Point( cand_pos_i );
                        const auto cand_sq_dist_j = ray_line.Sq_Distance_To_Point( cand_pos_j );
                        const auto cand_sq_dist_k = ray_line.Sq_Distance_To_Point( cand_pos_k );

                        if( (cand_sq_dist_i <= cand_sq_dist_j) && (cand_sq_dist_i <= cand_sq_dist_k) ){
                            blocky_ray_pos = cand_pos_i;
                            true_ray_pos += true_ray_pos_dR_incr_row;
                            last_move_dist = true_ray_pos_dR_incr_row_length;
                            ray_i += incr_row;
                        }else if( cand_sq_dist_j <= cand_sq_dist_k ){
                            blocky_ray_pos = cand_pos_j;
                            true_ray_pos += true_ray_pos_dR_incr_col;
                            last_move_dist = true_ray_pos_dR_incr_col_length;
                            ray_j += incr_col;
                        }else{
                            blocky_ray_pos = cand_pos_k;
                            true_ray_pos += true_ray_pos_dR_incr_img;
                            last_move_dist = true_ray_pos_dR_incr_img_length;
                            ray_k += incr_img;
                        }

                        // Terminate if the geometry is invalid.
                        if( pxl_diagonal_sq_length < true_ray_pos.sq_dist(blocky_ray_pos) ){
                            throw std::runtime_error("Real ray position and blocky ray position differ by more than a voxel diagonal");
                        }

                        // Process the voxel.
                        if( ( 0 <= ray_i ) && (ray_i < N_rows)
                        &&  ( 0 <= ray_j ) && (ray_j < N_cols)
                        &&  ( 0 <= ray_k ) && (ray_k < N_imgs) ){
                            const auto intersecting_img_refw = img_adj.index_to_image(ray_k);
                            const auto voxel_val = intersecting_img_refw.get().value(ray_i, ray_j, Channel);

                            // Ficticious mass density encountered by the ray.
                            const auto intensity = (voxel_val < -1000.0f) ? -1000.0f : voxel_val; // Enforce physicality.
                            const auto attenuation_coeff = 1.0f + (intensity / 1000.0f); 

                            accumulated_attenuation_length_product += attenuation_coeff * last_move_dist;
                            
                            // Could alternately invoke a more generic user function using (i,j,k) and the various ray
                            // positions/distances here.

                            //  ... TODO ...

                        }

                        // Terminate if the ray has traveled far enough.
                        const auto ray_traveled_sq_dist = ray_start.sq_dist(true_ray_pos);
                        if(ray_total_sq_dist <= ray_traveled_sq_dist){
                            break;
                        }
                    }

                    //Record the result in the image.
                    DetectImg->reference(RadiographRow, RadiographCol, 0) = static_cast<float>(accumulated_attenuation_length_product);
                }

                {
                    // Report progress.
                    std::lock_guard<std::mutex> lock(printer);
                    ++completed;
                    FUNCINFO("Completed " << completed << " of " << RadiographRows 
                          << " --> " << static_cast<int>(1000.0*(completed)/RadiographRows)/10.0 << "% done");
                }
            });

        }
    } // Complete tasks and terminate thread pool.

    //------------------------

    // Post-process the image according to user criteria.
    if(imgmodel_is_mudl){
        // Do nothing -- no need to transform.

    }else if(imgmodel_is_exp){
        // Implement a generic radiograph image with exponential attenuation.
        for(long int row = 0; row < RadiographRows; ++row){
            for(long int col = 0; col < RadiographColumns; ++col){
                const auto alp = DetectImg->reference(row, col, 0);
                const auto att = 1.0 - std::exp(-alp * AttenuationScale);
                DetectImg->reference(row, col, 0) = att;
            }
        }

    }else{
        throw std::invalid_argument("Image model not understood. Unable to continue.");
    }

    // Save image maps to file.
    if(FilenameStr.empty()){
        FilenameStr = Get_Unique_Sequential_Filename("/tmp/dicomautomaton_simulateradiograph_", 6, ".fits");
    }

    if(!WriteToFITS(*DetectImg, FilenameStr)){
        throw std::runtime_error("Unable to write FITS file for simulated radiograph.");
    }

    // Insert the image maps as images for later processing and/or viewing, if desired.
    *OrthoSrcImg = *DetectImg;
    sd_image_collection.images.resize(1);

    DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>() );
    DICOM_data.image_data.back()->imagecoll = sd_image_collection;

    return DICOM_data;
}

