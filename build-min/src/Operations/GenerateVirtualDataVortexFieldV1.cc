// GenerateVirtualDataVortexFieldV1.cc - A part of DICOMautomaton 2026. Written by hal clark.

#include <cmath>
#include <list>
#include <map>
#include <memory>
#include <string>    
#include <cstdint>

#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorImages.h"
#include "YgorString.h"       //Needed for GetFirstRegex(...)

#include "../Structs.h"
#include "../Alignment_Field.h"

#include "GenerateVirtualDataVortexFieldV1.h"


OperationDoc OpArgDocGenerateVirtualDataVortexFieldV1(){
    OperationDoc out;
    out.name = "GenerateVirtualDataVortexFieldV1";

    out.tags.emplace_back("category: deformation");
    out.tags.emplace_back("category: generator");
    out.tags.emplace_back("category: virtual phantom");

    out.desc = 
        "This operation generates a deterministic synthetic cylindrical vortex deformation field"
        " centered at (256.0, 256.0, 50.0) that can be used to warp images with bounds"
        " x in [0, 512.0], y in [0, 512.0], and z in [0, 100.0]."
        " The vortex has displacement vectors rotating around the center axis (z-axis)."
        " The magnitude of all displacements is normalized everywhere to a maximum of 1.0"
        " and a minimum of 0.0 (at the precise center of the vortex)."
        " The deformation field is saved as a Transform3 object and can be used for testing"
        " and benchmarking deformable image registration algorithms.";

    return out;
}

bool GenerateVirtualDataVortexFieldV1(Drover &DICOM_data,
                                      const OperationArgPkg&,
                                      std::map<std::string, std::string>& /*InvocationMetadata*/,
                                      const std::string& /*FilenameLex*/){

    // Parameters fixed for V1 virtual phantom.
    const vec3<double> vortex_center(256.0, 256.0, 50.0);
    const double x_min = 0.0;
    const double x_max = 512.0;
    const double y_min = 0.0;
    const double y_max = 512.0;
    const double z_min = 0.0;
    const double z_max = 100.0;
    
    // Grid resolution - controls the sampling density of the deformation field
    const int64_t N_rows = 512;      // Along y-axis
    const int64_t N_cols = 512;      // Along x-axis
    const int64_t N_imgs = 100;       // Along z-axis
    const int64_t N_channels = 3;    // dx, dy, dz
    
    // Pixel spacing
    const double pxl_dx = (x_max - x_min) / static_cast<double>(N_cols);
    const double pxl_dy = (y_max - y_min) / static_cast<double>(N_rows);
    const double pxl_dz = (z_max - z_min) / static_cast<double>(N_imgs);
    
    // Image orientation (standard axial orientation)
    const vec3<double> row_unit(1.0, 0.0, 0.0);    // x-axis
    const vec3<double> col_unit(0.0, 1.0, 0.0);    // y-axis
    const vec3<double> img_unit(0.0, 0.0, 1.0);    // z-axis (orthogonal to slices)
    
    const vec3<double> anchor(0.0, 0.0, 0.0);
    
    // Create the deformation field as a planar_image_collection
    planar_image_collection<double, double> field_coll;
    
    // Track the maximum displacement magnitude for normalization
    double max_displacement = 0.0;
    
    // First pass: create images and compute displacements, tracking the maximum
    for(int64_t img_idx = 0; img_idx < N_imgs; ++img_idx){
        planar_image<double, double> img;
        
        // Position of this slice
        const double z = z_min + pxl_dz * (static_cast<double>(img_idx) + 0.5);
        const vec3<double> offset = anchor + img_unit * z;
        
        // Initialize the image
        img.init_orientation(row_unit, col_unit);
        img.init_buffer(N_rows, N_cols, N_channels);
        img.init_spatial(pxl_dx, pxl_dy, pxl_dz, anchor, offset);
        
        // Set metadata
        img.metadata["Description"] = "Cylindrical vortex deformation field";
        img.metadata["VortexCenterX"] = std::to_string(vortex_center.x);
        img.metadata["VortexCenterY"] = std::to_string(vortex_center.y);
        img.metadata["VortexCenterZ"] = std::to_string(vortex_center.z);
        img.metadata["SliceNumber"] = std::to_string(img_idx + 1);
        
        // Fill in the displacement vectors
        for(int64_t row = 0; row < N_rows; ++row){
            for(int64_t col = 0; col < N_cols; ++col){
                const auto pos = img.position(row, col);
                
                // Compute displacement from center (in the xy-plane)
                const double dx_from_center = pos.x - vortex_center.x;
                const double dy_from_center = pos.y - vortex_center.y;
                const double dz_from_center = pos.z - vortex_center.z;
                
                // Radial distance in the xy-plane (cylindrical coordinates)
                const double r = std::sqrt(dx_from_center * dx_from_center + 
                                          dy_from_center * dy_from_center);
                
                // Create a cylindrical vortex: displacement vectors rotate around the z-axis
                // The displacement is tangent to circles centered on the vortex center
                // Using right-hand rule: displacement is perpendicular to the radial direction
                double disp_x = 0.0;
                double disp_y = 0.0;
                double disp_z = 0.0;
                
                if(r > 1e-6){  // Avoid division by zero at the center
                    // Tangential direction (perpendicular to radial, in xy-plane)
                    // For a vortex rotating counter-clockwise when viewed from above (+z):
                    // tangent = (-dy, dx) / r
                    const double tangent_x = -dy_from_center / r;
                    const double tangent_y = dx_from_center / r;
                    
                    // Displacement magnitude as a function of radius
                    // Use a profile that increases with radius up to a maximum
                    // and then decreases to create a well-defined vortex
                    const double r_max = std::min(vortex_center.x, vortex_center.y);
                    const double r_normalized = r / r_max;
                    
                    // Smooth displacement profile: starts at 0, peaks in middle, decreases
                    // Using a Gaussian-like profile: exp(-k*(r/r_max - 1)^2)
                    const double displacement_profile = r_normalized * std::exp(-2.0 * (r_normalized - 0.5) * (r_normalized - 0.5));
                    
                    // Also add a slight z-component that varies with position
                    // to make it more interesting and test 3D deformation
                    const double z_normalized = std::abs(dz_from_center) / ((z_max - z_min) / 2.0);
                    const double z_component_scale = 0.3 * std::exp(-z_normalized * z_normalized);
                    
                    disp_x = tangent_x * displacement_profile;
                    disp_y = tangent_y * displacement_profile;
                    // Add small z-displacement that depends on radial position
                    disp_z = z_component_scale * displacement_profile * 
                             (dz_from_center > 0 ? 1.0 : -1.0);
                }
                
                // Track maximum displacement magnitude
                const double magnitude = std::sqrt(disp_x * disp_x + 
                                                  disp_y * disp_y + 
                                                  disp_z * disp_z);
                if(magnitude > max_displacement){
                    max_displacement = magnitude;
                }
                
                // Store the displacement components
                img.reference(row, col, 0) = disp_x;  // dx
                img.reference(row, col, 1) = disp_y;  // dy
                img.reference(row, col, 2) = disp_z;  // dz
            }
        }
        
        field_coll.images.push_back(img);
    }
    
    // Second pass: normalize all displacements to [0, 1] range
    if(max_displacement > 1e-10){  // Avoid division by zero
        for(auto &img : field_coll.images){
            for(int64_t row = 0; row < N_rows; ++row){
                for(int64_t col = 0; col < N_cols; ++col){
                    img.reference(row, col, 0) /= max_displacement;
                    img.reference(row, col, 1) /= max_displacement;
                    img.reference(row, col, 2) /= max_displacement;
                }
            }
        }
    }
    
    // Create the deformation_field object
    deformation_field def_field(std::move(field_coll));
    
    // Create a Transform3 object and store the deformation field
    auto trans_ptr = std::make_shared<Transform3>();
    trans_ptr->transform = std::move(def_field);
    trans_ptr->metadata["Description"] = "Cylindrical vortex deformation field";
    trans_ptr->metadata["TransformType"] = "DeformationField";
    trans_ptr->metadata["VortexCenterX"] = std::to_string(vortex_center.x);
    trans_ptr->metadata["VortexCenterY"] = std::to_string(vortex_center.y);
    trans_ptr->metadata["VortexCenterZ"] = std::to_string(vortex_center.z);
    trans_ptr->metadata["BoundsXMin"] = std::to_string(x_min);
    trans_ptr->metadata["BoundsXMax"] = std::to_string(x_max);
    trans_ptr->metadata["BoundsYMin"] = std::to_string(y_min);
    trans_ptr->metadata["BoundsYMax"] = std::to_string(y_max);
    trans_ptr->metadata["BoundsZMin"] = std::to_string(z_min);
    trans_ptr->metadata["BoundsZMax"] = std::to_string(z_max);
    trans_ptr->metadata["MaxDisplacementBeforeNormalization"] = std::to_string(max_displacement);
    
    // Add to Drover's transform data
    DICOM_data.trans_data.emplace_back(trans_ptr);
    
    return true;
}
