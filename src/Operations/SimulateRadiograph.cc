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
#include <experimental/optional>

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



OperationDoc OpArgDocSimulateRadiograph(void){
    OperationDoc out;
    out.name = "SimulateRadiograph";

    out.desc = 
        "This routine uses ray sampling to simulate radiographs from an image array.";

    out.notes.emplace_back(
         "Images must be rectilinear."
         // Note: while this operation could be implemented without requiring rectilinearity, it is much faster to
         // require it. If this functionality is required then modify this operation.
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


    return out;
}



Drover SimulateRadiograph(Drover DICOM_data, 
                          OperationArgPkg OptArgs,
                          std::map<std::string,std::string> /*InvocationMetadata*/,
                          std::string /*FilenameLex*/){
    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();

    auto FilenameStr = OptArgs.getValueStr("Filename").value();

    const auto RadiographRows = 512;
    const auto RadiographColumns = 512;
    const auto Channel = 0;
    //-----------------------------------------------------------------------------------------------------------------


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


    // Ensure the image array is rectilinear. (This will allow us to use a faster postion-to-image lookup.)
    {
        std::list<std::reference_wrapper<planar_image<float,double>>> selected_imgs;
        for(auto &img : img_arr_ptr->imagecoll.images){
            selected_imgs.push_back( std::ref(img) );
        }

        if(!Images_Form_Rectilinear_Grid(selected_imgs)){
            throw std::invalid_argument("Images do not form a rectilinear grid. Cannot continue");
        }
        //const bool is_regular_grid = Images_Form_Regular_Grid(selected_imgs);
    }

    const auto row_unit = img_arr_ptr->imagecoll.images.front().row_unit.unit();
    const auto col_unit = img_arr_ptr->imagecoll.images.front().col_unit.unit();
    const auto ortho_unit = col_unit.Cross(row_unit).unit();

    planar_image_adjacency<float,double> img_adj( {}, { { std::ref(img_arr_ptr->imagecoll) } }, ortho_unit );
    if(img_adj.int_to_img.empty()){
        throw std::logic_error("Image array contained no images. Cannot continue.");
    }

    // Determine an appropriate radiograph orientation.
    const auto img_centre = img_arr_ptr->imagecoll.center(); // TODO: For TBI, should be at the t0 point (i.e., at the level of the lung).
    auto ray_source = img_centre + vec3<double>(0.0, 1390.0, 0.0);  // Should be relative to voxel at (0,0,0), not image centre.
    const line<double> source_centre_line(ray_source, img_centre); 

    // Determine which way will be 'up' in the radiograph.
    const auto ray_unit = (img_centre - ray_source).unit();
    auto rg_up = ortho_unit;
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

/*
    // Determine the bounds of the image volume at this orientation.

    // First determine the furthest voxel so we can place the image on the plane fully behind the image.
    double furthest_voxel_dist = std::numeric_limits<double>::quiet_NaN();
    vec3<double> furthest_voxel_pos;
    for(const auto &animg : img_arr_ptr->imagecoll.images){
        for(long int row = 0; row < animg.rows; ++row){
            for(long int col = 0; col < animg.columns; ++col){
                const auto p = animg.position(row, col);
                const auto dist = ray_source.distance(p);
                if(!std::isfinite(dist) || (dist > furthest_voxel_dist)){
                    furthest_voxel_dist = dist;
                    furthest_voxel_pos = p;
                }
            }
        }
    }
*/
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

//    const plane<double> detector_plane( -ray_unit, 
//                                        furthest_voxel_pos + ray_unit ); // Slightly further than the furthest voxel.
//    const plane<double> ray_source_plane( (ROI_centroid - Ref_centroid).unit(), ROI_centroid );

    // ============================================== Source, Detector creation  ==============================================
    //Create source and detector images. 
    //
    // NOTE: They do not need to be aligned with the geometry, contours, or grid. But leave a big margin so you
    //       can ensure you're getting all the surface available.

/*
    //const auto SDGridZ = vec3<double>(0.0, 1.0, 1.0).unit();
    const auto SDGridZ = ROICleaving.N_0.unit();
    vec3<double> SDGridY = vec3<double>(1.0, 0.0, 0.0);
    if(SDGridY.Dot(SDGridZ) > 0.25){
        SDGridY = SDGridZ.rotate_around_x(M_PI * 0.5);
    }
    vec3<double> SDGridX = SDGridZ.Cross(SDGridY);
    if(!SDGridZ.GramSchmidt_orthogonalize(SDGridY, SDGridX)){
        throw std::runtime_error("Unable to find grid orientation vectors.");
    }
    SDGridX = SDGridX.unit();
    SDGridY = SDGridY.unit();
*/

    double grid_x_margin = 5.0;
    double grid_y_margin = 5.0;
    double grid_z_margin = 5.0;

    //Generate a grid volume bounding the ROI(s). We ask for many images in order to compress the pxl_dz taken by each.
    // Only two are actually allocated.
    const auto NumberOfImages = 1000L;
    auto sd_image_collection = Symmetrically_Contiguously_Grid_Volume<float,double>(
             cc_ROIs, 
             grid_x_margin, grid_y_margin, grid_z_margin,
             RadiographRows, RadiographColumns, /*number_of_channels=*/ 1, NumberOfImages, 
             source_centre_line, (rg_up * -1.0), rg_left,
             /*pixel_fill=*/ 0.0, 
             /*only_top_and_bottom=*/ true);

    //Get handles for each image.
    planar_image<float, double> *DetectImg = &(*std::next(sd_image_collection.images.begin(),0));
    planar_image<float, double> *OrthoSrcImg = &(*std::next(sd_image_collection.images.begin(),1));

    DetectImg->metadata["Description"] = "Virtual radiograph detector";
    OrthoSrcImg->metadata["Description"] = "(unused)";

    //const auto detector_plane = DetectImg->image_plane();
    const auto detector_plane = DetectImg->image_plane();
    const auto orthosrc_plane = OrthoSrcImg->image_plane();


    // ============================================== Ray-cast ==============================================

    //Now ready to ray cast. Loop over integer pixel coordinates.
    {
        asio_thread_pool tp;
        std::mutex printer; // Who gets to print to the console and iterate the counter.
        long int completed = 0;

        for(long int row = 0; row < RadiographRows; ++row){
            tp.submit_task([&,row](void) -> void {
                for(long int col = 0; col < RadiographColumns; ++col){

                    //Construct a line segment between the source and detector. 
                    const auto ray_terminus = DetectImg->position(row, col);
                    const auto ray_line = line<double>(ray_terminus, ray_source);

                    // Find the intersection of the ray with the near and far bounding planes. This will reduce the
                    // amount of computation required.
                    vec3<double> near_bp_intersection;
                    if(!orthosrc_plane.Intersects_With_Line_Once(ray_line, near_bp_intersection)){
                        throw std::logic_error("Ray line does not intersect near image array bounding plane. Cannot continue.");
                    }
                    vec3<double> far_bp_intersection;
                    if(!detector_plane.Intersects_With_Line_Once(ray_line, far_bp_intersection)){
                        throw std::logic_error("Ray line does not intersect far image array bounding plane. Cannot continue.");
                    }
                    const vec3<double> ray_start = near_bp_intersection;
                    const vec3<double> ray_end = far_bp_intersection;

                    const auto travel_dist = ray_end.distance(ray_start);
                    const auto ray_advance_dist = 1.0; // DICOM units (mm);
                    const auto N_advances = static_cast<long int>(travel_dist/ray_advance_dist) + 1L;

                    double accumulated_contrast = 0.0;   //The total accumulated contrast by the ray.
                    long int accumulated_counts = 0;
                    for(long int i = 0; i <= N_advances; ++i){
                        const auto x = static_cast<double>(i)/static_cast<double>(N_advances);
                        const auto P = (ray_end - ray_start) * x + ray_start;

                        //Sample the image array at this point.
                        //const auto interp_val = img_arr_ptr->imagecoll.trilinearly_interpolate(P,Channel);
                        const auto interp_val = img_adj.trilinearly_interpolate(P,Channel,-1024.0f);

                        const auto intensity = (interp_val < -1024.0f) ? -1024.0f : interp_val; // Enforce physicality.
                        const auto density = 1.0f + (intensity / 1024.0f);  // Electron density, assuming linear map from CT number.
                        accumulated_contrast += density;
                        ++accumulated_counts;
                    }

                    //Record the result in the image.
                    DetectImg->reference(row, col, 0) = static_cast<float>(accumulated_contrast);
                }

                {
                    std::lock_guard<std::mutex> lock(printer);
                    ++completed;
                    FUNCINFO("Completed " << completed << " of " << RadiographRows 
                          << " --> " << static_cast<int>(1000.0*(completed)/RadiographRows)/10.0 << "\% done");
                }
            });
        }

        // Transform the image to the fraction of light that would have made it through.
        for(long int row = 0; row < RadiographRows; ++row){
            for(long int col = 0; col < RadiographColumns; ++col){
                const auto ad = DetectImg->reference(row, col, 0);
                const auto f = 1.0 - std::exp(-ad / 10.0);
                DetectImg->reference(row, col, 0) = f;
            }
        }

    } // Complete tasks and terminate thread pool.


    //-----------------------------------------------------------------------------------------------------------------

    // Save image maps to file.
    if(FilenameStr.empty()){
        FilenameStr = Get_Unique_Sequential_Filename("/tmp/dicomautomaton_simulateradiograph_", 6, ".fits");
    }

    if(!WriteToFITS(*DetectImg, FilenameStr)){
        throw std::runtime_error("Unable to write FITS file for simulated radiograph.");
    }

    // Insert the image maps as images for later processing and/or viewing, if desired.
    DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>() );
    DICOM_data.image_data.back()->imagecoll = sd_image_collection;

    return DICOM_data;
}
