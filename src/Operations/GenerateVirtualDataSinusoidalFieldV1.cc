// GenerateVirtualDataSinusoidalFieldV1.cc - A part of DICOMautomaton 2026. Written by hal clark.

#include <cmath>
#include <limits>
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

#include "GenerateVirtualDataSinusoidalFieldV1.h"


OperationDoc OpArgDocGenerateVirtualDataSinusoidalFieldV1(){
    OperationDoc out;
    out.name = "GenerateVirtualDataSinusoidalFieldV1";

    out.tags.emplace_back("category: deformation");
    out.tags.emplace_back("category: generator");
    out.tags.emplace_back("category: virtual phantom");

    out.desc = 
        "This operation generates a deterministic synthetic 3D sinusoidal deformation field"
        " that can be used to warp images with bounds"
        " x in [0, 512.0], y in [0, 512.0], and z in [0, 100.0]."
        " The field has sinusoidally varying displacement vectors in all three dimensions."
        " The magnitude of all displacements is normalized everywhere to a maximum of 1.0"
        " and a minimum of 0.0."
        " The deformation field is saved as a Transform3 object and can be used for testing"
        " and benchmarking deformable image registration algorithms.";

    return out;
}

bool GenerateVirtualDataSinusoidalFieldV1(Drover &DICOM_data,
                                          const OperationArgPkg&,
                                          std::map<std::string, std::string>& /*InvocationMetadata*/,
                                          const std::string& /*FilenameLex*/){

    constexpr auto pi = std::acos(-1.0);

    // Parameters fixed for V1 virtual phantom.
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
    
    // Sinusoidal wave parameters
    // Use different wavelengths in each direction to create an interesting 3D pattern
    const double wavelength_x = (x_max - x_min) / 2.0;  // 2 complete waves in x
    const double wavelength_y = (y_max - y_min) / 3.0;  // 3 complete waves in y
    const double wavelength_z = (z_max - z_min) / 1.5;  // 1.5 complete waves in z
    
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
        img.metadata["Description"] = "Sinusoidal deformation field";
        img.metadata["WavelengthX"] = std::to_string(wavelength_x);
        img.metadata["WavelengthY"] = std::to_string(wavelength_y);
        img.metadata["WavelengthZ"] = std::to_string(wavelength_z);
        img.metadata["SliceNumber"] = std::to_string(img_idx + 1);
        
        // Fill in the displacement vectors
        for(int64_t row = 0; row < N_rows; ++row){
            for(int64_t col = 0; col < N_cols; ++col){
                const auto pos = img.position(row, col);
                
                // Get position relative to origin for sinusoidal calculations
                const double x_rel = pos.x - x_min;
                const double y_rel = pos.y - y_min;
                const double z_rel = pos.z - z_min;
                
                // Create sinusoidal displacements
                // The displacement in each direction is a function of position in other directions
                // This creates a 3D wave pattern
                
                // dx varies sinusoidally with y and z
                const double disp_x = std::sin(2.0 * pi * y_rel / wavelength_y) *
                                     std::cos(2.0 * pi * z_rel / wavelength_z);
                
                // dy varies sinusoidally with x and z
                const double disp_y = std::sin(2.0 * pi * x_rel / wavelength_x) *
                                     std::cos(2.0 * pi * z_rel / wavelength_z);
                
                // dz varies sinusoidally with x and y
                const double disp_z = std::sin(2.0 * pi * x_rel / wavelength_x) *
                                     std::sin(2.0 * pi * y_rel / wavelength_y);
                
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
    // First, we need to shift to make the minimum 0, then scale to make the maximum 1
    // For a sinusoidal field, the minimum will be negative, so we need to handle that
    
    // Find min and max for proper normalization
    double min_val = std::numeric_limits<double>::max();
    double max_val = std::numeric_limits<double>::lowest();
    
    for(auto &img : field_coll.images){
        for(int64_t row = 0; row < N_rows; ++row){
            for(int64_t col = 0; col < N_cols; ++col){
                const double dx = img.reference(row, col, 0);
                const double dy = img.reference(row, col, 1);
                const double dz = img.reference(row, col, 2);
                const double magnitude = std::sqrt(dx * dx + dy * dy + dz * dz);
                
                if(magnitude < min_val) min_val = magnitude;
                if(magnitude > max_val) max_val = magnitude;
            }
        }
    }
    
    // Normalize to [0, 1] range based on magnitude
    if(max_val > min_val && max_val > 1e-10){
        const double range = max_val - min_val;
        for(auto &img : field_coll.images){
            for(int64_t row = 0; row < N_rows; ++row){
                for(int64_t col = 0; col < N_cols; ++col){
                    const double dx = img.reference(row, col, 0);
                    const double dy = img.reference(row, col, 1);
                    const double dz = img.reference(row, col, 2);
                    const double magnitude = std::sqrt(dx * dx + dy * dy + dz * dz);
                    
                    // Scale to [0, 1] while preserving direction
                    const double scale_factor = (magnitude > 1e-10) ? ((magnitude - min_val) / range) / magnitude : 0.0;
                    
                    img.reference(row, col, 0) = dx * scale_factor;
                    img.reference(row, col, 1) = dy * scale_factor;
                    img.reference(row, col, 2) = dz * scale_factor;
                }
            }
        }
    }
    
    // Create the deformation_field object
    deformation_field def_field(std::move(field_coll));
    
    // Create a Transform3 object and store the deformation field
    auto trans_ptr = std::make_shared<Transform3>();
    trans_ptr->transform = std::move(def_field);
    trans_ptr->metadata["Description"] = "Sinusoidal deformation field";
    trans_ptr->metadata["TransformType"] = "DeformationField";
    trans_ptr->metadata["WavelengthX"] = std::to_string(wavelength_x);
    trans_ptr->metadata["WavelengthY"] = std::to_string(wavelength_y);
    trans_ptr->metadata["WavelengthZ"] = std::to_string(wavelength_z);
    trans_ptr->metadata["BoundsXMin"] = std::to_string(x_min);
    trans_ptr->metadata["BoundsXMax"] = std::to_string(x_max);
    trans_ptr->metadata["BoundsYMin"] = std::to_string(y_min);
    trans_ptr->metadata["BoundsYMax"] = std::to_string(y_max);
    trans_ptr->metadata["BoundsZMin"] = std::to_string(z_min);
    trans_ptr->metadata["BoundsZMax"] = std::to_string(z_max);
    trans_ptr->metadata["MaxDisplacementBeforeNormalization"] = std::to_string(max_displacement);
    trans_ptr->metadata["MinDisplacementMagnitude"] = std::to_string(min_val);
    trans_ptr->metadata["MaxDisplacementMagnitude"] = std::to_string(max_val);
    
    // Add to Drover's transform data
    DICOM_data.trans_data.emplace_back(trans_ptr);
    
    return true;
}
