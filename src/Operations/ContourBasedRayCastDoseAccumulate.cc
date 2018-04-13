//ContourBasedRayCastDoseAccumulate.cc - A part of DICOMautomaton 2015, 2016. Written by hal clark.

#include <algorithm>
#include <cmath>
#include <experimental/optional>
#include <fstream>
#include <functional>
#include <iterator>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>    
#include <vector>

#include "../Dose_Meld.h"
#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "ContourBasedRayCastDoseAccumulate.h"
#include "Explicator.h"       //Needed for Explicator class.
#include "YgorImages.h"
#include "YgorImagesIO.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.



OperationDoc OpArgDocContourBasedRayCastDoseAccumulate(void){
    OperationDoc out;
    out.name = "ContourBasedRayCastDoseAccumulate";
    out.desc = "";

    out.notes.emplace_back("");


    out.args.emplace_back();
    out.args.back().name = "DoseLengthMapFileName";
    out.args.back().desc = "A filename (or full path) for the (dose)*(length traveled through the ROI peel) image map."
                      " The format is TBD. Leave empty to dump to generate a unique temporary file.";
    out.args.back().default_val = "";
    out.args.back().expected = true;
    out.args.back().examples = { "", "/tmp/somefile", "localfile.img", "derivative_data.img" };
    out.args.back().mimetype = "image/fits";


    out.args.emplace_back();
    out.args.back().name = "LengthMapFileName";
    out.args.back().desc = "A filename (or full path) for the (length traveled through the ROI peel) image map."
                      " The format is TBD. Leave empty to dump to generate a unique temporary file.";
    out.args.back().default_val = "";
    out.args.back().expected = true;
    out.args.back().examples = { "", "/tmp/somefile", "localfile.img", "derivative_data.img" };
    out.args.back().mimetype = "image/fits";


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
    out.args.back().name = "CylinderRadius";
    out.args.back().desc = "The radius of the cylinder surrounding contour line segments that defines the 'surface'."
                      " Quantity is in the DICOM coordinate system.";
    out.args.back().default_val = "3.0";
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
    out.args.back().name = "Rows";
    out.args.back().desc = "The number of rows in the resulting images.";
    out.args.back().default_val = "256";
    out.args.back().expected = true;
    out.args.back().examples = { "10", "50", "128", "1024" };
    
    
    out.args.emplace_back();
    out.args.back().name = "Columns";
    out.args.back().desc = "The number of columns in the resulting images.";
    out.args.back().default_val = "256";
    out.args.back().expected = true;
    out.args.back().examples = { "10", "50", "128", "1024" };
    
    
    return out;
}



Drover ContourBasedRayCastDoseAccumulate(Drover DICOM_data, OperationArgPkg OptArgs, std::map<std::string,std::string> /*InvocationMetadata*/, std::string FilenameLex){

    //---------------------------------------------- User Parameters --------------------------------------------------
    auto DoseLengthMapFileName = OptArgs.getValueStr("DoseLengthMapFileName").value();
    auto LengthMapFileName = OptArgs.getValueStr("LengthMapFileName").value();
    const auto ROILabelRegex = OptArgs.getValueStr("ROILabelRegex").value();
    const auto NormalizedROILabelRegex = OptArgs.getValueStr("NormalizedROILabelRegex").value();
    const auto CylinderRadiusStr = OptArgs.getValueStr("CylinderRadius").value();
    const auto RaydLStr = OptArgs.getValueStr("RaydL").value();
    const auto RowsStr = OptArgs.getValueStr("Rows").value();
    const auto ColumnsStr = OptArgs.getValueStr("Columns").value();
    //-----------------------------------------------------------------------------------------------------------------

    const auto CylinderRadius = std::stod(CylinderRadiusStr);
    const auto RaydL = std::stod(RaydLStr);
    const auto Rows = std::stol(RowsStr);
    const auto Columns = std::stol(ColumnsStr);

    Explicator X(FilenameLex);

    //Ensure the Ray dL is sufficiently small. We enforce that ray cannot step over the cylinder in a single iteration
    // for 95% of the width of the cylinder. So if the rays are oncoming and directed at the cylinder perpendicularly,
    // but randomly distributed over the width of the cylinder, then rays will only be able to step over the cylinder
    // without the code 'noticing' 5 times out of every 100 rays. See figure.
    //
    //                  !                       !  
    //                 \!/                     \!/  
    //       -          x                       !                  
    //       |          ! o  o                  x   o  o          -
    //    dL |         o!       o               !o        o       |
    //       |        o !        o              o          o      |   cylinder
    //       -        o x        o              o          o      | diameter/width
    //                 o!       o               xo        o       |
    //                  ! o  o                  !   o  o          -  
    //                  !                       !
    //                  x                       !
    //                  !                       x
    //                 \!/                     \!/
    //                  !                       !
    //
    //              Intersecting Ray       Non-intersecting Ray
    //
    // Note: Glancing rays will be *systematically* lost, but not all glancing rays will be lost. Only some. The amount
    //       lost will depend on the distance from the centre. The probability of a specific ray being lost depends on
    //       the phase relative to the centre.
    //
    // Note: In practice, fewer rays will be lost than predicted if they travel obliquely (not perpendicular) to the
    //       cylinders. The choice of a reasonable (or optimal) ray dL will somewhat depend on the estimated average
    //       obliquity. If there is a lot of obliquity (say, many cylinders are at 45 degrees) then the ray dL can be
    //       relaxed at the same RELATIVE error rate will be achieved. If cylinders are randomly oriented, then the same
    //       is true -- but if you want to control the ABSOLUTE error rate then you are forced to keep ray dL small
    //       enough to handle the perpendicular case. For this reason, we assume the work-case scenario (rays and
    //       cylinders are perpendicular) and hope to get a better-than expected error rate. (But it should not be worse
    //       than we predict!)
    //       
    // Note: Here is how the relationship behaves:
    //       gnuplot> plot 2.0*sqrt(1.0 - x) with impulse ls -1
    //                                                                                                    
    //                                         minRaydL / cyl_radius                                      
    //  0.9 +-+---------------+-----------------+-----------------+-----------------+---------------+-+   
    //      +||+++++          +                 +                 +                 +                 +   
    //      ||||||||++++++                          (minRaydL/cyl_radius) = 2.0*sqrt(1.0 - x) +-----+ |   
    //  0.8 +-+|||||||||||++++                                                                      +-+   
    //      ||||||||||||||||||++++++                                                                  |   
    //      ||||||||||||||||||||||||++++                                                              |   
    //  0.7 +-+|||||||||||||||||||||||||+++++                                                       +-+   
    //      |||||||||||||||||||||||||||||||||+++                                                      |   
    //  0.6 +-+|||||||||||||||||||||||||||||||||+++++                                               +-+   
    //      |||||||||||||||||||||||||||||||||||||||||++++                                             |   
    //      |||||||||||||||||||||||||||||||||||||||||||||+++                                          |   
    //  0.5 +-+|||||||||||||||||||||||||||||||||||||||||||||+++                                     +-+   
    //      |||||||||||||||||||||||||||||||||||||||||||||||||||++++                                   |   
    //      |||||||||||||||||||||||||||||||||||||||||||||||||||||||++                                 |   
    //  0.4 +-+||||||||||||||||||||||||||||||||||||||||||||||||||||||+++                            +-+   
    //      ||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||++                            |   
    //      ||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||++                          |   
    //  0.3 +-+|||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||++                      +-+   
    //      ||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||+                       |   
    //  0.2 +-+||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||++                   +-+   
    //      |||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||+                    |   
    //      ||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||+                   |   
    //  0.1 +-+||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||+                +-+   
    //      ||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||                  |   
    //      +|||||||||||||||||+|||||||||||||||||+|||||||||||||||||+|||||||||||||||||+                 +   
    //    0 +-+---------------+-----------------+-----------------+-----------------+---------------+-+   
    //     0.8               0.85              0.9               0.95               1                1.05 
    //                                    Fraction of rays caught/noticed
    //
    //
    const double catch_frac = 0.95; // Catch 95% of randomly distributed rays incident perpendicularly.
    const double minRaydL = 2.0 * CylinderRadius * std::sqrt(1.0 - catch_frac);
    if(RaydL > minRaydL){
        throw std::runtime_error("Ray dL is too small. MinRaydL=" + std::to_string(minRaydL) + ". Are you sure this is OK? (edit me if so).");
    } 


    //Merge the dose arrays if necessary.
    if(DICOM_data.dose_data.empty()){
        throw std::invalid_argument("This routine requires at least one dose image array. Cannot continue");
    }
    DICOM_data.dose_data = Meld_Dose_Data(DICOM_data.dose_data);
    if(DICOM_data.dose_data.size() != 1){
        throw std::invalid_argument("Unable to meld doses into a single dose array. Cannot continue.");
    }

    //auto img_arr_ptr = DICOM_data.image_data.front();
    auto img_arr_ptr = DICOM_data.dose_data.front();

    if(img_arr_ptr == nullptr){
        throw std::runtime_error("Encountered a nullptr when expecting a valid Image_Array or Dose_Array ptr.");
    }else if(img_arr_ptr->imagecoll.images.empty()){
        throw std::runtime_error("Encountered a Image_Array or Dose_Array with valid images -- no images found.");
    }


    //Stuff references to all contours into a list. Remember that you can still address specific contours through
    // the original holding containers (which are not modified here).
    auto cc_all = All_CCs( DICOM_data );
    auto cc_ROIs = Whitelist( cc_all, { { "ROIName", ROILabelRegex },
                                        { "NormalizedROIName", NormalizedROILabelRegex } } );

    if(cc_ROIs.empty()){
        throw std::invalid_argument("No contours selected. Cannot continue.");
    }

    //Pre-compute the line segments and spheres we will use to define the surface boundary. 
    //
    // NOTE: We should be using a spatial indexing data structure, like R*-tree. (Does it support cylinders? Can it be
    // made to support cylinders?) TODO.
    std::vector<line_segment<double>> cylinders; // Radii are all the same: CylinderRadius.
    std::vector<vec3<double>> spheres; // Centres of the spheres. The radii are the same as the cylinder radii.

    for(const auto &cc_opt : cc_ROIs){
        for(const auto &cop : cc_opt.get().contours){
            if(cop.points.empty()){
                continue;
            }else if(cop.points.size() == 1){
                spheres.emplace_back( cop.points.back() );
            }else if(cop.points.size() == 2){
                spheres.emplace_back( cop.points.front() );
                spheres.emplace_back( cop.points.back() );
                if(cop.closed) cylinders.emplace_back(cop.points.front(), cop.points.back());
            }else{
                for(const auto &v : cop.points) spheres.emplace_back(v);

                auto itA = cop.points.begin();
                auto itB = std::next(itA);
                for( ; itB != cop.points.end(); ++itA, ++itB){
                     cylinders.emplace_back(*itB, *itA); //Orientation doesn't matter.
                }
                if(cop.closed) cylinders.emplace_back(cop.points.front(), cop.points.back());
            }
        }
    }

    //Trim geometry above some user-specified plane.
    // ... ideal? necessary? cumbersome? ...


    //Find an appropriate unit vector which will define the orientation of a plane parallel to the detector and source grids.
    // Rays will travel perpendicular to this plane, and it therefore controls.
    const auto GridNormal = vec3<double>(0.0, 0.0, 1.0).unit();
    const vec3<double> zero(0.0, 0.0, 0.0);
    const plane<double> GridPlane(GridNormal, zero);

    //Find two more directions (unit vectors) with which to align the bounding box.
    // Note: because we want to be able to compare images from different scans, we use a deterministic 
    // technique for generating two orthogonal directions involving the cardinal directions and Gram-Schmidt
    // orthogonalization.
    vec3<double> GridX = GridNormal.rotate_around_z(M_PI * 0.5); // Try Z. Will often be idempotent.
    if(GridX.Dot(GridNormal) > 0.25){
        GridX = GridNormal.rotate_around_y(M_PI * 0.5);  //Should always work since GridNormal is parallel to Z.
    }
    vec3<double> GridY = GridNormal.Cross(GridX);
    if(!GridNormal.GramSchmidt_orthogonalize(GridX, GridY)){
        throw std::runtime_error("Unable to find grid orientation vectors.");
    }
    GridX = GridX.unit();
    GridY = GridY.unit();


    //Find an appropriate bounding box encompassing the ROI surface.
    double grid_x_min = std::numeric_limits<double>::quiet_NaN();
    double grid_x_max = std::numeric_limits<double>::quiet_NaN();
    double grid_y_min = std::numeric_limits<double>::quiet_NaN();
    double grid_y_max = std::numeric_limits<double>::quiet_NaN();
    double grid_z_min = std::numeric_limits<double>::quiet_NaN();
    double grid_z_max = std::numeric_limits<double>::quiet_NaN();

    //Make three planes defined by GridNormal, GridX, and GridY.
    // (The planes intersect the origin so it is easier to compute offsets later.)
    const plane<double> GridXZeroPlane(GridX, zero);
    const plane<double> GridYZeroPlane(GridY, zero);
    const plane<double> GridZZeroPlane(GridNormal, zero);

    for(const auto &asphere : spheres){
        //Compute the distance to each plane.
        const auto distX = GridXZeroPlane.Get_Signed_Distance_To_Point(asphere);
        const auto distY = GridYZeroPlane.Get_Signed_Distance_To_Point(asphere);
        const auto distZ = GridZZeroPlane.Get_Signed_Distance_To_Point(asphere);

        //Score the minimum and maximum distances.
        if(false){
        }else if(!std::isfinite(grid_x_min) || (distX < grid_x_min)){  grid_x_min = distX;
        }else if(!std::isfinite(grid_x_max) || (distX > grid_x_max)){  grid_x_max = distX;
        }else if(!std::isfinite(grid_y_min) || (distY < grid_y_min)){  grid_y_min = distY;
        }else if(!std::isfinite(grid_y_max) || (distY > grid_y_max)){  grid_y_max = distY;
        }else if(!std::isfinite(grid_z_min) || (distZ < grid_z_min)){  grid_z_min = distZ;
        }else if(!std::isfinite(grid_z_max) || (distZ > grid_z_max)){  grid_z_max = distZ;
        }
    }
    for(const auto &acylinder : cylinders){
        //const std::vector<vec3<double>> vlist = { acylinder.Get_R0(), acylinder.Get_R1() };
        //for(const auto &v : vlist){
        for(const auto &v : { acylinder.Get_R0(), acylinder.Get_R1() } ){
            const auto distX = GridXZeroPlane.Get_Signed_Distance_To_Point(v);
            const auto distY = GridYZeroPlane.Get_Signed_Distance_To_Point(v);
            const auto distZ = GridZZeroPlane.Get_Signed_Distance_To_Point(v);

            if(false){
            }else if(!std::isfinite(grid_x_min) || (distX < grid_x_min)){  grid_x_min = distX;
            }else if(!std::isfinite(grid_x_min) || (distX > grid_x_max)){  grid_x_max = distX;
            }else if(!std::isfinite(grid_y_min) || (distY < grid_y_min)){  grid_y_min = distY;
            }else if(!std::isfinite(grid_y_min) || (distY > grid_y_max)){  grid_y_max = distY;
            }else if(!std::isfinite(grid_z_min) || (distZ < grid_z_min)){  grid_z_min = distZ;
            }else if(!std::isfinite(grid_z_min) || (distZ > grid_z_max)){  grid_z_max = distZ;
            }
        }
    }

    //Leave a margin.
    const double grid_margin = 2.0*CylinderRadius;
    grid_x_min -= grid_margin;
    grid_x_max += grid_margin;
    grid_y_min -= grid_margin;
    grid_y_max += grid_margin;
    grid_z_min -= 2.0*grid_margin;
    grid_z_max += 2.0*grid_margin;
    
    //Using the minimum and maximum distances along z, place planes at the top and bottom + a margin.
    const plane<double> GridZTopPlane( GridNormal, zero + GridNormal * grid_z_max );
    const plane<double> GridZBotPlane( GridNormal, zero + GridNormal * grid_z_min );
    
    //const vec3<double> far_corner_zero  = zero + (GridX * grid_x_max) + (GridY * grid_y_max);
    const vec3<double> near_corner_zero = zero + (GridX * grid_x_min) + (GridY * grid_y_min);

    const double xwidth = grid_x_max - grid_x_min;
    const double ywidth = grid_y_max - grid_y_min;

    //Project the corner on the zero plane onto the top and bottom Z-planes.
    const auto GridZTopNearCorner = GridZTopPlane.Project_Onto_Plane_Orthogonally(near_corner_zero);
    const auto GridZBotNearCorner = GridZBotPlane.Project_Onto_Plane_Orthogonally(near_corner_zero);


    //Create images that live on each Z-plane.
    const auto voxel_dx = xwidth / Columns;
    const auto voxel_dy = ywidth / Rows;
    const auto voxel_dz = grid_margin; // Doesn't really matter much. Not used for anything.

    const auto SourceImgOffset = GridZTopNearCorner + (GridX * voxel_dx * 0.5) + (GridY * voxel_dy * 0.5);
    const auto DetectImgOffset = GridZBotNearCorner + (GridX * voxel_dx * 0.5) + (GridY * voxel_dy * 0.5);

    planar_image<float, double> SourceImg;
    SourceImg.init_buffer(Rows, Columns, 1);
    SourceImg.init_spatial(voxel_dx, voxel_dy, voxel_dz, zero, SourceImgOffset);
    SourceImg.init_orientation(GridX, GridY);
    SourceImg.fill_pixels(0.0);

    planar_image<float, double> DetectImg;
    DetectImg.init_buffer(Rows, Columns, 1);
    DetectImg.init_spatial(voxel_dx, voxel_dy, voxel_dz, zero, DetectImgOffset);
    DetectImg.init_orientation(GridX, GridY);
    DetectImg.fill_pixels(0.0);

    //Now ready to ray cast. Loop over integer pixel coordinates. Start and finish are image pixels.
    // The top image can be the length image.
    const auto sq_radius = std::pow(CylinderRadius, 2.0);
    for(long int row = 0; row < Rows; ++row){
        FUNCINFO("Working on row " << (row+1) << " of " << Rows 
                  << " --> " << static_cast<int>(1000.0*(row+1)/Rows)/10.0 << "\% done");
        for(long int col = 0; col < Columns; ++col){
            double accumulated_length = 0.0;      //Length of ray travel within the 'surface'.
            double accumulated_doselength = 0.0;

            vec3<double> ray_pos = SourceImg.position(row, col);
            const vec3<double> terminus = DetectImg.position(row, col);
            const vec3<double> ray_dir = (terminus - ray_pos).unit();

            //Go until we get within certain distance or overshoot and the ray wants to backtrack.
            while(    (ray_dir.Dot( (terminus - ray_pos).unit() ) > 0.8 ) // Ray orientation is still downward-facing.
                   && (ray_pos.distance(terminus) > std::max(RaydL, grid_margin)) ){ // Still far away from detector.

                ray_pos += ray_dir * RaydL;
                const auto midpoint = ray_pos - (ray_dir * RaydL * 0.5);

                //Search to see if ray is in an object.
                bool skip = false; //Was already found to be in a surface and was counted.
                for(const auto &asphere : spheres){
                    if(ray_pos.sq_dist(asphere) < sq_radius){
                        accumulated_length += RaydL;

                        //Find the dose at the half-way point.
                        auto encompass_imgs = img_arr_ptr->imagecoll.get_images_which_encompass_point( midpoint );
                        for(const auto &enc_img : encompass_imgs){
                            const auto pix_val = enc_img->value(midpoint, 0);
                            accumulated_doselength += RaydL * pix_val;
                        }
                        skip = true;
                        break;
                    }
                }

                if(!skip){
                    for(const auto &acylinder : cylinders){
                        if(acylinder.Within_Cylindrical_Volume(ray_pos, CylinderRadius)){
                            accumulated_length += RaydL;

                            //Find the dose at the half-way point.
                            auto encompass_imgs = img_arr_ptr->imagecoll.get_images_which_encompass_point( midpoint );
                            for(const auto &enc_img : encompass_imgs){
                                const auto pix_val = enc_img->value(midpoint, 0);
                                accumulated_doselength += RaydL * pix_val;
                            }
                            break;
                        }
                    }
                }
            }

            //Deposit the dose in the images.
            SourceImg.reference(row, col, 0) = static_cast<float>(accumulated_length);
            DetectImg.reference(row, col, 0) = static_cast<float>(accumulated_doselength);
        }
    }

    // Save image maps to file.
    if(!WriteToFITS(SourceImg, LengthMapFileName)){
        throw std::runtime_error("Unable to write FITS file for length map.");
    }
    if(!WriteToFITS(DetectImg, DoseLengthMapFileName)){
        throw std::runtime_error("Unable to write FITS file for dose-length map.");
    }

    return DICOM_data;
}
