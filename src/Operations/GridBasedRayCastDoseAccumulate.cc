//GridBasedRayCastDoseAccumulate.cc - A part of DICOMautomaton 2015, 2016. Written by hal clark.

#include <algorithm>
#include <cmath>
#include <cstdlib>            //Needed for exit() calls.
#include <any>
#include <optional>
#include <fstream>
#include <functional>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <regex>
#include <stdexcept>
#include <string>    

#include "../Dose_Meld.h"
#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../Thread_Pool.h"
#include "../YgorImages_Functors/Compute/GenerateSurfaceMask.h"
#include "../YgorImages_Functors/Grouping/Misc_Functors.h"
#include "../YgorImages_Functors/Processing/In_Image_Plane_Bicubic_Supersample.h"
#include "Explicator.h"       //Needed for Explicator class.
#include "GridBasedRayCastDoseAccumulate.h"
#include "YgorFilesDirs.h"    //Needed for Does_File_Exist_And_Can_Be_Read(...), etc..
#include "YgorImages.h"
#include "YgorImagesIO.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.



OperationDoc OpArgDocGridBasedRayCastDoseAccumulate(){
    OperationDoc out;
    out.name = "GridBasedRayCastDoseAccumulate";
    out.desc = 
        "This operation performs a ray casting to estimate the surface dose of an ROI.";


    out.args.emplace_back();
    out.args.back().name = "DoseMapFileName";
    out.args.back().desc = "A filename (or full path) for the dose image map."
                      " Note that this file is approximate, and may not be accurate."
                      " There is more information available when you use the length and dose*length maps instead."
                      " However, this file is useful for viewing and eyeballing tuning settings."
                      " The format is FITS. Leave empty to dump to generate a unique temporary file.";
    out.args.back().default_val = "";
    out.args.back().expected = true;
    out.args.back().examples = { "", "/tmp/dose.fits", "localfile.fits", "derivative_data.fits" };
    out.args.back().mimetype = "image/fits";

    out.args.emplace_back();
    out.args.back().name = "DoseLengthMapFileName";
    out.args.back().desc = "A filename (or full path) for the (dose)*(length traveled through the ROI peel) image map."
                      " The format is FITS. Leave empty to dump to generate a unique temporary file.";
    out.args.back().default_val = "";
    out.args.back().expected = true;
    out.args.back().examples = { "", "/tmp/doselength.fits", "localfile.fits", "derivative_data.fits" };
    out.args.back().mimetype = "image/fits";


    out.args.emplace_back();
    out.args.back().name = "LengthMapFileName";
    out.args.back().desc = "A filename (or full path) for the (length traveled through the ROI peel) image map."
                      " The format is FITS. Leave empty to dump to generate a unique temporary file.";
    out.args.back().default_val = "";
    out.args.back().expected = true;
    out.args.back().examples = { "", "/tmp/surfacelength.fits", "localfile.fits", "derivative_data.fits" };
    out.args.back().mimetype = "image/fits";


    out.args.emplace_back();
    out.args.back().name = "NormalizedReferenceROILabelRegex";
    out.args.back().desc = "A regex matching reference ROI labels/names to consider. The default will match"
                      " all available ROIs, which is non-sensical. The reference ROI is used to orient"
                      " the cleaving plane to trim the grid surface mask.";
    out.args.back().default_val = ".*";
    out.args.back().expected = true;
    out.args.back().examples = { ".*", ".*Prostate.*", "Left Kidney", "Gross Liver" };

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
    out.args.back().name = "ReferenceROILabelRegex";
    out.args.back().desc = "A regex matching reference ROI labels/names to consider. The default will match"
                      " all available ROIs, which is non-sensical. The reference ROI is used to orient"
                      " the cleaving plane to trim the grid surface mask.";
    out.args.back().default_val = ".*";
    out.args.back().expected = true;
    out.args.back().examples = { ".*", ".*[pP]rostate.*", "body", "Gross_Liver",
                            R"***(.*left.*parotid.*|.*right.*parotid.*|.*eyes.*)***",
                            R"***(left_parotid|right_parotid)***" };

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

    out.args.emplace_back();
    out.args.back().name = "SmallestFeature";
    out.args.back().desc = "A length giving an estimate of the smallest feature you want to resolve."
                      " Quantity is in the DICOM coordinate system.";
    out.args.back().default_val = "0.5";
    out.args.back().expected = true;
    out.args.back().examples = { "1.0", "2.0", "0.5", "5.0" };
    
    out.args.emplace_back();
    out.args.back().name = "RaydL";
    out.args.back().desc = "The distance to move a ray each iteration. Should be << img_thickness and << cylinder_radius."
                      " Making too large will invalidate results, causing rays to pass through the surface without"
                      " registering any dose accumulation. Making too small will cause the run-time to grow and may"
                      " eventually lead to truncation or round-off errors. Quantity is in the DICOM coordinate system.";
    out.args.back().default_val = "0.1";
    out.args.back().expected = true;
    out.args.back().examples = { "0.1", "0.05", "0.01", "0.005" };
   

    out.args.emplace_back();
    out.args.back().name = "GridRows";
    out.args.back().desc = "The number of rows in the surface mask grid images.";
    out.args.back().default_val = "512";
    out.args.back().expected = true;
    out.args.back().examples = { "10", "50", "128", "1024" };
    
    
    out.args.emplace_back();
    out.args.back().name = "GridColumns";
    out.args.back().desc = "The number of columns in the surface mask grid images.";
    out.args.back().default_val = "512";
    out.args.back().expected = true;
    out.args.back().examples = { "10", "50", "128", "1024" };

    out.args.emplace_back();
    out.args.back().name = "SourceDetectorRows";
    out.args.back().desc = "The number of rows in the resulting images."
                      " Setting too fine relative to the surface mask grid or dose grid is futile.";
    out.args.back().default_val = "1024";
    out.args.back().expected = true;
    out.args.back().examples = { "10", "50", "128", "1024" };
    
    out.args.emplace_back();
    out.args.back().name = "SourceDetectorColumns";
    out.args.back().desc = "The number of columns in the resulting images."
                      " Setting too fine relative to the surface mask grid or dose grid is futile.";
    out.args.back().default_val = "1024";
    out.args.back().expected = true;
    out.args.back().examples = { "10", "50", "128", "1024" };

    
    out.args.emplace_back();
    out.args.back().name = "NumberOfImages";
    out.args.back().desc = "The number of images used for grid-based surface detection. Leave negative for computation"
                      " of a reasonable value; set to something specific to force an override.";
    out.args.back().default_val = "-1";
    out.args.back().expected = true;
    out.args.back().examples = { "-1", "10", "50", "100" };
    
    
    return out;
}



Drover GridBasedRayCastDoseAccumulate(Drover DICOM_data, const OperationArgPkg& OptArgs, const std::map<std::string,std::string>& /*InvocationMetadata*/, const std::string& FilenameLex){

    //---------------------------------------------- User Parameters --------------------------------------------------
    auto DoseMapFileName = OptArgs.getValueStr("DoseMapFileName").value();
    auto DoseLengthMapFileName = OptArgs.getValueStr("DoseLengthMapFileName").value();
    auto LengthMapFileName = OptArgs.getValueStr("LengthMapFileName").value();
    const auto ROILabelRegex = OptArgs.getValueStr("ROILabelRegex").value();
    const auto NormalizedROILabelRegex = OptArgs.getValueStr("NormalizedROILabelRegex").value();
    const auto ReferenceROILabelRegex = OptArgs.getValueStr("ReferenceROILabelRegex").value();
    const auto NormalizedReferenceROILabelRegex = OptArgs.getValueStr("NormalizedReferenceROILabelRegex").value();
    const auto SmallestFeature = std::stod(OptArgs.getValueStr("SmallestFeature").value());
    const auto RaydL = std::stod(OptArgs.getValueStr("RaydL").value());
    const auto GridRows = std::stol(OptArgs.getValueStr("GridRows").value());
    const auto GridColumns = std::stol(OptArgs.getValueStr("GridColumns").value());
    const auto SourceDetectorRows = std::stol(OptArgs.getValueStr("SourceDetectorRows").value());
    const auto SourceDetectorColumns = std::stol(OptArgs.getValueStr("SourceDetectorColumns").value());
    auto NumberOfImages = std::stol(OptArgs.getValueStr("NumberOfImages").value());

    //-----------------------------------------------------------------------------------------------------------------
    const auto roiregex = Compile_Regex(ROILabelRegex);
    const auto roinormalizedregex = Compile_Regex(NormalizedROILabelRegex);
    const auto refregex = Compile_Regex(ReferenceROILabelRegex);
    const auto refnormalizedregex = Compile_Regex(NormalizedReferenceROILabelRegex);

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
    std::list<std::reference_wrapper<contour_collection<double>>> cc_all;
    for(auto & cc : DICOM_data.contour_data->ccs){
        auto base_ptr = reinterpret_cast<contour_collection<double> *>(&cc);
        cc_all.push_back( std::ref(*base_ptr) );
    }

    //Whitelist contours using the provided regex.
    auto cc_ROIs = cc_all;
    cc_ROIs.remove_if([=](std::reference_wrapper<contour_collection<double>> cc) -> bool {
                   const auto ROINameOpt = cc.get().contours.front().GetMetadataValueAs<std::string>("ROIName");
                   const auto& ROIName = ROINameOpt.value();
                   return !(std::regex_match(ROIName,roiregex));
    });
    cc_ROIs.remove_if([=](std::reference_wrapper<contour_collection<double>> cc) -> bool {
                   const auto ROINameOpt = cc.get().contours.front().GetMetadataValueAs<std::string>("NormalizedROIName");
                   const auto& ROIName = ROINameOpt.value();
                   return !(std::regex_match(ROIName,roinormalizedregex));
    });

    auto cc_Refs = cc_all;
    cc_Refs.remove_if([=](std::reference_wrapper<contour_collection<double>> cc) -> bool {
                   const auto ROINameOpt = cc.get().contours.front().GetMetadataValueAs<std::string>("ROIName");
                   const auto& ROIName = ROINameOpt.value();
                   return !(std::regex_match(ROIName,refregex));
    });
    cc_Refs.remove_if([=](std::reference_wrapper<contour_collection<double>> cc) -> bool {
                   const auto ROINameOpt = cc.get().contours.front().GetMetadataValueAs<std::string>("NormalizedROIName");
                   const auto& ROIName = ROINameOpt.value();
                   return !(std::regex_match(ROIName,refnormalizedregex));
    });

    if(cc_ROIs.empty()){
        throw std::invalid_argument("No ROI contours selected. Cannot continue.");
    }else if(cc_Refs.empty()){
        throw std::invalid_argument("No ReferenceROI contours selected. Cannot continue.");
    }

    // ============================================== Generate a grid  ==============================================

    //Record the unique contour planes (compared by some small threshold) in a sorted list.
    // These are used to derive information useful for optimal gridding.
    const auto est_cont_normal = cc_ROIs.front().get().contours.front().Estimate_Planar_Normal();
    const auto ucp = Unique_Contour_Planes(cc_ROIs, est_cont_normal, /*distance_eps=*/ 0.005);

    //Compute the number of images to make into the grid: number of unique contour planes + 2.
    // The extra two will contain some surface voxels.
    if(NumberOfImages <= 0) NumberOfImages = (ucp.size() + 2);
    FUNCINFO("Number of images: " << NumberOfImages);

    //Find grid alignment vectors.
    //
    // Note: Because we want to be able to compare images from different scans, we use a deterministic technique for
    //       generating two orthogonal directions involving the cardinal directions and Gram-Schmidt
    //       orthogonalization.
    const auto pi = std::acos(-1.0);
    const auto GridZ = est_cont_normal.unit();
    vec3<double> GridX = GridZ.rotate_around_z(pi * 0.5); // Try Z. Will often be idempotent.
    if(GridX.Dot(GridZ) > 0.25){
        GridX = GridZ.rotate_around_y(pi * 0.5);  //Should always work since GridZ is parallel to Z.
    }
    vec3<double> GridY = GridZ.Cross(GridX);
    if(!GridZ.GramSchmidt_orthogonalize(GridX, GridY)){
        throw std::runtime_error("Unable to find grid orientation vectors.");
    }
    GridX = GridX.unit();
    GridY = GridY.unit();

    //Figure out what z-margin is needed so the extra two images do not interfere with the grid lining up with the
    // contours. (Want exactly one contour plane per image.) So the margin should be large enough so the empty
    // images have no contours inside them, but small enough so that it doesn't affect the location of contours in the
    // other image slices. The ideal is if each image slice has the same thickness so contours are all separated by some
    // constant separation -- in this case we make the margin exactly as big as if two images were also included.
    double z_margin = 0.0;
    if(ucp.size() > 1){
        //Compute the total distance between the (centre of the) top and (centre of the) bottom planes.
        // (Note: the images associated with these contours will usually extend further. This is dealt with below.)
        const auto total_sep =  std::abs(ucp.front().Get_Signed_Distance_To_Point(ucp.back().R_0));
        const auto sep_per_plane = total_sep / static_cast<double>(ucp.size()-1);

        //Add TOTAL zmargin of 1*sep_per_plane each for 2 extra images, and 0.5*sep_per_plane for each of the images
        // which will stick out beyond the contour planes. (The margin is added at the top and the bottom.)
        z_margin = sep_per_plane * 1.5;
    }else{
        FUNCWARN("Only a single contour plane was detected. Guessing its thickness.."); 
        z_margin = 5.0;
    }

    //Figure out what a reasonable x-margin and y-margin are. 
    //
    // NOTE: Could also use (median? maximum?) distance from centroid to vertex.
    double x_margin = z_margin;
    double y_margin = z_margin;

    //Generate a grid volume bounding the ROI(s).
    auto grid_image_collection = Contiguously_Grid_Volume<float,double>(
             cc_ROIs, 
             x_margin, y_margin, z_margin,
             GridRows, GridColumns, /*number_of_channels=*/ 1, NumberOfImages,
             GridX, GridY, GridZ,
             /*pixel_fill=*/ std::numeric_limits<double>::quiet_NaN(),
             /*only_top_and_bottom=*/ false);
    DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>() );
    DICOM_data.image_data.back()->imagecoll.images.swap(grid_image_collection.images);
    auto grid_arr_ptr = DICOM_data.image_data.back();


    //Compute the surface mask using the new grid.
    const float void_mask_val     = 0.0;
    const float surface_mask_val  = 1.0;
    const float interior_mask_val = 0.0;

    //Perform the computation.
    {
        GenerateSurfaceMaskUserData ud;
        ud.background_val = void_mask_val;
        ud.surface_val    = surface_mask_val;
        ud.interior_val   = interior_mask_val; //So the user can easily visualize afterward.
        if(!grid_arr_ptr->imagecoll.Compute_Images( ComputeGenerateSurfaceMask, { },
                                                    cc_ROIs, &ud )){
            throw std::runtime_error("Unable to generate a surface mask.");
        }
    }

    // ============================================== Modify the mask ==============================================

    // Gaussian blur to help smooth the sharp edges.
    grid_arr_ptr->imagecoll.Gaussian_Pixel_Blur( {}, 2.0 ); // Sigma in terms of pixel count.

    // Supersample the surface mask.
    {
        InImagePlaneBicubicSupersampleUserData bicub_ud;
        bicub_ud.RowScaleFactor    = 3;
        bicub_ud.ColumnScaleFactor = 3;

        if(!grid_arr_ptr->imagecoll.Process_Images_Parallel( GroupIndividualImages,
                                                             InImagePlaneBicubicSupersample,
                                                             {}, {}, &bicub_ud )){
            FUNCERR("Unable to bicubically supersample surface mask");
        }
    }

    // Threshold the surface mask.
    grid_arr_ptr->imagecoll.apply_to_pixels([=](long int, long int, long int, float &val) -> void {
            if(!isininc(void_mask_val, val, surface_mask_val)){
                val = void_mask_val;
                return;
            }

            if( (val - void_mask_val) > 0.25*(surface_mask_val - void_mask_val) ){
                val = surface_mask_val;
            }else{
                val = void_mask_val;
            }
            return;
    });

    //Compute centroids for the ROI and Reference ROI volumes.
    vec3<double> ROI_centroid;
    vec3<double> Ref_centroid;
    {
        contour_collection<double> cc_ROIs_All;
        for(const auto &cc_ref : cc_ROIs){
            for(const auto &c : cc_ref.get().contours) cc_ROIs_All.contours.push_back(c);
        }

        contour_collection<double> cc_Refs_All;
        for(const auto &cc_ref : cc_Refs){ // Different references. One in the c++ sense, another in the 'reference textbook' sense.
            for(const auto &c : cc_ref.get().contours) cc_Refs_All.contours.push_back(c);
        }

        ROI_centroid = cc_ROIs_All.Centroid();
        Ref_centroid = cc_Refs_All.Centroid();
    }

    //Create a plane at the Bladder's centroid aligned with the ROI (bladder) that faces away from the referenceROI
    // (prostate).
    const plane<double> ROICleaving( (ROI_centroid - Ref_centroid).unit(), ROI_centroid );

    //'Cleave' the surface mask with the plane; set all voxels away from the referenceROI (prostate) to the 'void' mask value.
    grid_arr_ptr->imagecoll.set_voxels_above_plane(ROICleaving, void_mask_val, {});


    // ============================================== Source, Detector creation  ==============================================
    //Create source and detector images. 
    //
    // NOTE: They do not need to be aligned with the geometry, contours, or grid. But leave a big margin so you
    //       can ensure you're getting all the surface available.

    //const auto SDGridZ = vec3<double>(0.0, 1.0, 1.0).unit();
    const auto SDGridZ = ROICleaving.N_0.unit();
    vec3<double> SDGridY = vec3<double>(1.0, 0.0, 0.0);
    if(SDGridY.Dot(SDGridZ) > 0.25){
        SDGridY = SDGridZ.rotate_around_x(pi * 0.5);
    }
    vec3<double> SDGridX = SDGridZ.Cross(SDGridY);
    if(!SDGridZ.GramSchmidt_orthogonalize(SDGridY, SDGridX)){
        throw std::runtime_error("Unable to find grid orientation vectors.");
    }
    SDGridX = SDGridX.unit();
    SDGridY = SDGridY.unit();

    //Hope that using a margin twice the grid margin will capture all jutting surface.
    double sdgrid_x_margin = 2.0*x_margin;
    double sdgrid_y_margin = 2.0*y_margin;
    double sdgrid_z_margin = 2.0*z_margin;

    //Generate a grid volume bounding the ROI(s). We ask for many images in order to compress the pxl_dz taken by each.
    // Only two are actually allocated.
    auto sd_image_collection = Contiguously_Grid_Volume<float,double>(
             cc_ROIs, 
             sdgrid_x_margin, sdgrid_y_margin, sdgrid_z_margin,
             SourceDetectorRows, SourceDetectorColumns, /*number_of_channels=*/ 1, 100*NumberOfImages, 
             SDGridX, SDGridY, SDGridZ,
             /*pixel_fill=*/ std::numeric_limits<double>::quiet_NaN(),
             /*only_top_and_bottom=*/ true);

    planar_image<float, double> *DetectImg = &(sd_image_collection.images.front());
    DetectImg->metadata["Description"] = "Dose*Length Map";
    planar_image<float, double> *SourceImg = &(sd_image_collection.images.back());
    SourceImg->metadata["Description"] = "Length Map (distance ray travelled through surface)";

    //Make an extra image to quickly show dose for viewing purposes.
    sd_image_collection.images.emplace_back();
    sd_image_collection.images.back() = *SourceImg;
    planar_image<float, double> *DoseImg = &(sd_image_collection.images.back());
    DoseImg->metadata["Description"] = "Dose Map (Approximate! For Viewing Only)";

    // ============================================== Ray-cast ==============================================

    //Now ready to ray cast. Loop over integer pixel coordinates. Start and finish are image pixels.
    // The top image can be the length image.
    {
        asio_thread_pool tp;
        std::mutex printer; // Who gets to print to the console and iterate the counter.
        long int completed = 0;

        const double cleaved_gap_dist = std::abs(ROICleaving.Get_Signed_Distance_To_Point(ROI_centroid));

        for(long int row = 0; row < SourceDetectorRows; ++row){
            tp.submit_task([&,row]() -> void {
                for(long int col = 0; col < SourceDetectorColumns; ++col){
                    double accumulated_length = 0.0;      //Length of ray travel within the 'surface'.
                    double accumulated_doselength = 0.0;
                    vec3<double> ray_pos = SourceImg->position(row, col);
                    const vec3<double> terminus = DetectImg->position(row, col);
                    const vec3<double> ray_dir = (terminus - ray_pos).unit();

                    ray_pos += ray_dir * cleaved_gap_dist; // Skip the gap which has been cleaved out.

                    //Go until we get within certain distance or overshoot and the ray wants to backtrack.
                    while(    (ray_dir.Dot( (terminus - ray_pos).unit() ) > 0.8 ) // Ray orientation is still downward-facing.
                           && (ray_pos.distance(terminus) > std::max(RaydL, SmallestFeature)) ){ // Still far away from detector.

                        ray_pos += ray_dir * RaydL;
                        const auto midpoint = ray_pos - (ray_dir * RaydL * 0.5);

                        //Check if it was in the surface at the midpoint.
                        auto rel_img = grid_arr_ptr->imagecoll.get_images_which_encompass_point(midpoint);
                        if(rel_img.empty()) continue;
                        const auto mask_val = rel_img.front()->value(midpoint, 0);
                        const auto is_in_surface = (mask_val == surface_mask_val);
                        if(is_in_surface){
                            accumulated_length += RaydL;

                            //Find the dose at the half-way point.
                            auto encompass_imgs = img_arr_ptr->imagecoll.get_images_which_encompass_point( midpoint );
                            for(const auto &enc_img : encompass_imgs){
                                const auto pix_val = enc_img->value(midpoint, 0);
                                accumulated_doselength += RaydL * pix_val;
                            }
                        }
                    }

                    //Deposit the dose in the images.
                    SourceImg->reference(row, col, 0) = static_cast<float>(accumulated_length);
                    DetectImg->reference(row, col, 0) = static_cast<float>(accumulated_doselength);
                    DoseImg->reference(row, col, 0) = 0.0f;
                    if(accumulated_length != 0.0){
                        DoseImg->reference(row, col, 0) = static_cast<float>(accumulated_doselength)
                                                          / static_cast<float>(accumulated_length);
                    }
                }

                {
                    std::lock_guard<std::mutex> lock(printer);
                    ++completed;
                    FUNCINFO("Completed " << completed << " of " << SourceDetectorRows 
                          << " --> " << static_cast<int>(1000.0*(completed)/SourceDetectorRows)/10.0 << "% done");
                }
            });
        }
    } // Complete tasks and terminate thread pool.

    // Save image maps to file.
    if(LengthMapFileName.empty()){
        LengthMapFileName = Get_Unique_Sequential_Filename("/tmp/dicomautomaton_gridraycast_surfacelength_", 6, ".fits");
    }
    if(DoseLengthMapFileName.empty()){
        DoseLengthMapFileName = Get_Unique_Sequential_Filename("/tmp/dicomautomaton_gridraycast_dosesurfacelength_", 6, ".fits");
    }
    if(DoseMapFileName.empty()){
        DoseMapFileName = Get_Unique_Sequential_Filename("/tmp/dicomautomaton_gridraycast_surfacedose_", 6, ".fits");
    }


    if(!WriteToFITS(*SourceImg, LengthMapFileName)){
        throw std::runtime_error("Unable to write FITS file for length map.");
    }
    if(!WriteToFITS(*DetectImg, DoseLengthMapFileName)){
        throw std::runtime_error("Unable to write FITS file for dose-length map.");
    }
    if(!WriteToFITS(*DoseImg, DoseMapFileName)){
        throw std::runtime_error("Unable to write FITS file for dose map.");
    }

    // Insert the image maps as images for later processing and/or viewing, if desired.
    DICOM_data.image_data.emplace_back( std::make_shared<Image_Array>() );
    DICOM_data.image_data.back()->imagecoll = sd_image_collection;

    return DICOM_data;
}
