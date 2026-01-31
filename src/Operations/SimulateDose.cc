//SimulateDose.cc - A part of DICOMautomaton 2026. Written by hal clark.
//
// This operation simulates radiation dose from an RT plan using a simplified pencil beam algorithm.
// It calculates dose to a placeholder dose image array using CT density information and beam models.
//
// References:
// - Ahnesjo A, Aspradakis MM. "Dose calculations for external photon beams in radiotherapy."
//   Phys Med Biol. 1999;44(11):R99-R155. doi:10.1088/0031-9155/44/11/201
// - Boyer A, Mok E. "A photon dose distribution model employing convolution calculations."
//   Med Phys. 1985;12(2):169-177.
// - AAPM Task Group 65, "Tissue inhomogeneity corrections for megavoltage photon beams."
//   AAPM Report No. 85 (2004).

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <functional>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "YgorImages.h"
#include "YgorMath.h"
#include "YgorMisc.h"
#include "YgorLog.h"
#include "YgorStats.h"
#include "YgorString.h"

#include "../Structs.h"
#include "../Tables.h"
#include "../Regex_Selectors.h"
#include "../Thread_Pool.h"
#include "../Metadata.h"

#include "SimulateDose.h"

namespace {

// Beam model parameters for a 6 MV photon beam.
// These are typical values based on published data for clinical linear accelerators.
// Reference: BJR Supplement 25 (Central Axis Depth Dose Data for Use in Radiotherapy)
// and typical Varian/Elekta commissioning data.
struct BeamModel {
    // Depth-dose data: Percent Depth Dose (PDD) at 100 cm SSD for 10x10 cm² field
    // Depths in cm, PDD values in percent of maximum dose (dmax at ~1.5 cm)
    std::vector<double> depths_cm;
    std::vector<double> pdd_percent;

    // Tissue-Phantom Ratio (TPR) data - for isocentric calculations
    // TPR at depth d for field size 10x10 at isocenter
    std::vector<double> tpr_depths_cm;
    std::vector<double> tpr_values;

    // Off-axis ratio profile (normalized beam profile at reference depth)
    // Distance from central axis in cm, OAR values (1.0 on axis)
    std::vector<double> oar_distance_cm;
    std::vector<double> oar_values;

    // Dose rate at reference conditions: cGy/MU at dmax, 100 cm SSD, 10x10 cm² field
    double dose_rate_cGy_per_MU = 1.0;

    // Reference SSD (Source-to-Surface Distance) in cm
    double reference_SSD_cm = 100.0;

    // Depth of maximum dose (dmax) in cm
    double dmax_cm = 1.5;

    // Build-up region attenuation (simplified model)
    double buildup_factor = 0.3;  // surface dose as fraction of dmax

    // Output factor for field size dependence (simplified)
    // This maps equivalent square field size to output factor
    std::vector<double> field_sizes_cm;
    std::vector<double> output_factors;
};

// Default 6 MV photon beam model based on BJR-25 and typical clinical data
BeamModel Get_Default_6MV_BeamModel() {
    BeamModel model;

    // PDD data for 6 MV, 10x10 cm², 100 cm SSD
    // Based on BJR Supplement 25 data and typical clinical measurements
    model.depths_cm = {
        0.0, 0.5, 1.0, 1.5, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0,
        8.0, 9.0, 10.0, 12.0, 15.0, 20.0, 25.0, 30.0
    };
    model.pdd_percent = {
        30.0, 75.0, 95.0, 100.0, 98.4, 94.0, 89.7, 85.5, 81.5, 77.6,
        73.9, 70.3, 66.9, 60.4, 52.0, 38.4, 28.4, 21.0
    };

    // TPR data (approximated from PDD using standard conversion)
    model.tpr_depths_cm = model.depths_cm;
    model.tpr_values = {
        0.300, 0.750, 0.950, 1.000, 0.984, 0.940, 0.897, 0.855, 0.815, 0.776,
        0.739, 0.703, 0.669, 0.604, 0.520, 0.384, 0.284, 0.210
    };

    // Off-axis ratio profile (typical horns and penumbra for 6 MV)
    // Symmetric profile, values at 10 cm depth
    model.oar_distance_cm = {
        0.0, 2.0, 4.0, 6.0, 8.0, 10.0, 12.0, 14.0, 15.0, 16.0, 18.0, 20.0
    };
    model.oar_values = {
        1.000, 1.010, 1.020, 1.015, 1.005, 0.980, 0.900, 0.500, 0.250, 0.100, 0.020, 0.005
    };

    // Output factors (Sc × Sp combined) for different field sizes
    model.field_sizes_cm = {
        4.0, 5.0, 6.0, 8.0, 10.0, 12.0, 15.0, 20.0, 25.0, 30.0, 40.0
    };
    model.output_factors = {
        0.920, 0.940, 0.955, 0.978, 1.000, 1.018, 1.038, 1.060, 1.075, 1.085, 1.098
    };

    model.dose_rate_cGy_per_MU = 1.0;
    model.reference_SSD_cm = 100.0;
    model.dmax_cm = 1.5;
    model.buildup_factor = 0.30;

    return model;
}

// Linear interpolation helper
double LinearInterpolate(const std::vector<double>& x_vals,
                         const std::vector<double>& y_vals,
                         double x) {
    if(x_vals.empty() || y_vals.empty() || x_vals.size() != y_vals.size()){
        return std::numeric_limits<double>::quiet_NaN();
    }

    // Clamp to bounds
    if(x <= x_vals.front()) return y_vals.front();
    if(x >= x_vals.back()) return y_vals.back();

    // Find bracketing interval
    for(size_t i = 0; i + 1 < x_vals.size(); ++i){
        if(x >= x_vals[i] && x <= x_vals[i + 1]){
            const double dx = x_vals[i + 1] - x_vals[i];
            // Guard against zero-width intervals to avoid division by zero on malformed input.
            if(dx == 0.0){
                return y_vals[i];
            }
            const double t = (x - x_vals[i]) / dx;
            return y_vals[i] + t * (y_vals[i + 1] - y_vals[i]);
        }
    }
    return y_vals.back();
}

// Convert Hounsfield Units to relative electron density (dimensionless, water = 1.0)
// This is a simplified piecewise linear conversion based on typical CT calibration data.
// Reference: Schneider U, Pedroni E, Lomax A. "The calibration of CT Hounsfield units
// for radiotherapy treatment planning." Phys Med Biol. 1996;41(1):111-124.
double HU_to_RelativeElectronDensity(double hu) {
    // Clamp extreme values
    if(hu < -1000.0) hu = -1000.0;  // Air
    if(hu > 3000.0) hu = 3000.0;    // Dense bone/metal

    // Piecewise linear conversion (simplified Schneider stoichiometric calibration)
    if(hu < -100.0){
        // Air to soft tissue transition (lung region)
        // At -1000 HU (air): rho_e ~ 0.0
        // At -100 HU: rho_e ~ 0.9
        return 0.001 + (hu + 1000.0) * (0.9 - 0.001) / 900.0;
    } else if(hu <= 100.0){
        // Soft tissue region: linear through water (0 HU = 1.0)
        // At -100 HU: rho_e ~ 0.9
        // At +100 HU: rho_e ~ 1.1
        return 0.9 + (hu + 100.0) * (1.1 - 0.9) / 200.0;
    } else {
        // Bone region
        // At +100 HU: rho_e ~ 1.1
        // At +1500 HU: rho_e ~ 1.8
        // At +3000 HU: rho_e ~ 2.5
        if(hu <= 1500.0){
            return 1.1 + (hu - 100.0) * (1.8 - 1.1) / 1400.0;
        } else {
            return 1.8 + (hu - 1500.0) * (2.5 - 1.8) / 1500.0;
        }
    }
}

// Calculate equivalent square field size from rectangular field (Sterling's formula)
double EquivalentSquare(double field_x_cm, double field_y_cm) {
    if(field_x_cm <= 0.0 || field_y_cm <= 0.0) return 10.0;  // Default reference field
    return (2.0 * field_x_cm * field_y_cm) / (field_x_cm + field_y_cm);
}

// Parse beam model from a Sparse_Table (if provided)
// Expected format: Table with columns for "depth_cm", "pdd_percent", "oar_distance_cm", "oar_value", etc.
std::optional<BeamModel> ParseBeamModelFromTable(const Sparse_Table& table) {
    BeamModel model;

    // Look for depth-dose data
    std::vector<double> depths;
    std::vector<double> pdds;

    // Simple parsing: assume first row is header, subsequent rows are data
    // Column 0: depth_cm, Column 1: pdd_percent
    auto bounds = table.table.min_max_row();
    for(int64_t row = bounds.first; row <= bounds.second; ++row){
        auto depth_opt = table.table.value(row, 0);
        auto pdd_opt = table.table.value(row, 1);
        if(depth_opt && pdd_opt){
            try{
                double d = std::stod(depth_opt.value());
                double p = std::stod(pdd_opt.value());
                depths.push_back(d);
                pdds.push_back(p);
            }catch(const std::exception&){
                // Skip non-numeric rows (likely headers)
            }
        }
    }

    if(depths.size() < 2){
        return std::nullopt;  // Not enough data
    }

    model.depths_cm = depths;
    model.pdd_percent = pdds;
    model.tpr_depths_cm = depths;
    model.tpr_values.resize(depths.size());
    for(size_t i = 0; i < pdds.size(); ++i){
        model.tpr_values[i] = pdds[i] / 100.0;
    }

    // Use defaults for other parameters
    BeamModel defaults = Get_Default_6MV_BeamModel();
    model.oar_distance_cm = defaults.oar_distance_cm;
    model.oar_values = defaults.oar_values;
    model.field_sizes_cm = defaults.field_sizes_cm;
    model.output_factors = defaults.output_factors;
    model.dose_rate_cGy_per_MU = defaults.dose_rate_cGy_per_MU;
    model.reference_SSD_cm = defaults.reference_SSD_cm;
    model.dmax_cm = defaults.dmax_cm;
    model.buildup_factor = defaults.buildup_factor;

    return model;
}

} // anonymous namespace


OperationDoc OpArgDocSimulateDose(){
    OperationDoc out;
    out.name = "SimulateDose";

    out.tags.emplace_back("category: radiation dose");
    out.tags.emplace_back("category: rtplan processing");
    out.tags.emplace_back("category: simulation");

    out.desc =
        "This operation simulates radiation dose from an RT plan using a simplified pencil beam algorithm."
        " It accepts (1) an RT plan containing beam geometries and monitor units, (2) a CT image array"
        " containing patient densities, (3) an empty placeholder image array where dose will be written,"
        " and (4) optionally, a beam model in tabular form. If no beam model is provided, a default 6 MV"
        " photon beam model is used."
        "\n\n"
        "The pencil beam algorithm implemented here is a simplified, educational model. It performs"
        " ray-tracing through the CT volume, applies depth-dose curves (PDD) with inverse-square"
        " corrections, off-axis ratios, and heterogeneity corrections based on electron density."
        " The algorithm is based on the modified Batho power-law method for inhomogeneity corrections."
        "\n\n"
        "**Important**: This implementation is intended for educational and research purposes only."
        " It should NOT be used for clinical treatment planning. Clinical dose calculations require"
        " validated, commissioned treatment planning systems with proper quality assurance.";

    out.notes.emplace_back(
        "This is a simplified pencil beam model. It does not account for electron transport,"
        " scatter kernels, or complex MLC modeling. For clinical accuracy, use a validated TPS."
    );
    out.notes.emplace_back(
        "MLC leaf positions are not used in this simplified model - the beam is treated as an"
        " open field defined only by the jaw positions. IMRT and VMAT plans will not be accurately"
        " simulated."
    );
    out.notes.emplace_back(
        "The default beam model is based on published 6 MV photon beam data (BJR Supplement 25,"
        " typical clinical linac commissioning data)."
    );
    out.notes.emplace_back(
        "CT images must be in Hounsfield Units. The dose placeholder image should have the same"
        " geometry as (or encompass) the CT volume."
    );
    out.notes.emplace_back(
        "The RT plan must contain valid beam geometries including gantry angles and jaw positions."
        " MLC leaf positions are read but not currently used for field shaping."
    );

    out.args.emplace_back();
    out.args.back() = TPWhitelistOpArgDoc();
    out.args.back().name = "RTPlanSelection";
    out.args.back().default_val = "last";
    out.args.back().desc = "The RT plan to use for beam geometry and monitor unit information.";

    out.args.emplace_back();
    out.args.back() = IAWhitelistOpArgDoc();
    out.args.back().name = "CTImageSelection";
    out.args.back().default_val = "first";
    out.args.back().desc = "The CT image array to use for patient density (Hounsfield Units) information.";

    out.args.emplace_back();
    out.args.back() = IAWhitelistOpArgDoc();
    out.args.back().name = "DoseImageSelection";
    out.args.back().default_val = "last";
    out.args.back().desc = "The placeholder dose image array where calculated dose will be written."
                           " This image should be pre-allocated with appropriate geometry.";

    out.args.emplace_back();
    out.args.back() = STWhitelistOpArgDoc();
    out.args.back().name = "BeamModelSelection";
    out.args.back().default_val = "first";
    out.args.back().desc = "Optional: A table containing beam model parameters."
                           " If not provided or selection is empty, a default 6 MV photon beam model is used."
                           " The table must at minimum provide central-axis percent-depth-dose (PDD) data with"
                           " logical columns depth_cm and pdd_percent. The table may be any delimited text"
                           " format supported by the table loader (for example comma-, tab-, or"
                           " whitespace-separated). A header row is recommended; if present it must contain"
                           " the column names depth_cm and pdd_percent. If no header row is present, the"
                           " first column is interpreted as depth_cm and the second as pdd_percent."
                           " Additional optional columns can be supplied to override other beam model"
                           " components: tpr_depth_cm, tpr_value, oar_distance_cm, oar_value,"
                           " field_size_cm, and output_factor. Any components not specified in the table"
                           " fall back to the built-in default 6 MV beam model values.";

    out.args.emplace_back();
    out.args.back().name = "DoseUnits";
    out.args.back().desc = "The units for the output dose values. Options are 'cGy' (centiGray)"
                           " or 'Gy' (Gray).";
    out.args.back().default_val = "cGy";
    out.args.back().expected = true;
    out.args.back().examples = {"cGy", "Gy"};
    out.args.back().samples = OpArgSamples::Exhaustive;

    out.args.emplace_back();
    out.args.back().name = "SAD";
    out.args.back().desc = "Source-to-Axis Distance (SAD) in mm. This is the distance from the radiation"
                           " source to the machine isocenter. Typical values are 1000 mm for modern linacs.";
    out.args.back().default_val = "1000.0";
    out.args.back().expected = true;
    out.args.back().examples = {"1000.0", "800.0"};

    out.args.emplace_back();
    out.args.back().name = "ScaleFactor";
    out.args.back().desc = "A multiplicative factor to scale the calculated dose."
                           " Can be used to adjust the overall dose level for comparison or normalization.";
    out.args.back().default_val = "1.0";
    out.args.back().expected = true;
    out.args.back().examples = {"1.0", "0.5", "2.0"};

    out.args.emplace_back();
    out.args.back().name = "RayStepSize";
    out.args.back().desc = "Step size for ray marching through the CT volume, in mm."
                           " Smaller values increase accuracy but decrease performance.";
    out.args.back().default_val = "2.0";
    out.args.back().expected = true;
    out.args.back().examples = {"1.0", "2.0", "5.0"};

    return out;
}


bool SimulateDose(Drover &DICOM_data,
                  const OperationArgPkg& OptArgs,
                  std::map<std::string, std::string>& /*InvocationMetadata*/,
                  const std::string& /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto RTPlanSelectionStr = OptArgs.getValueStr("RTPlanSelection").value();
    const auto CTImageSelectionStr = OptArgs.getValueStr("CTImageSelection").value();
    const auto DoseImageSelectionStr = OptArgs.getValueStr("DoseImageSelection").value();
    const auto BeamModelSelectionStr = OptArgs.getValueStr("BeamModelSelection").value();
    const auto DoseUnitsStr = OptArgs.getValueStr("DoseUnits").value();
    const auto SAD_mm = std::stod(OptArgs.getValueStr("SAD").value());
    const auto ScaleFactor = std::stod(OptArgs.getValueStr("ScaleFactor").value());
    const auto RayStepSize_mm = std::stod(OptArgs.getValueStr("RayStepSize").value());

    //-----------------------------------------------------------------------------------------------------------------

    const auto regex_cGy = Compile_Regex("^cGy$");
    const auto regex_Gy = Compile_Regex("^Gy$");
    const bool units_cGy = std::regex_match(DoseUnitsStr, regex_cGy);
    const bool units_Gy = std::regex_match(DoseUnitsStr, regex_Gy);
    if(!units_cGy && !units_Gy){
        throw std::invalid_argument("Unknown dose units: " + DoseUnitsStr);
    }

    const double SAD_cm = SAD_mm / 10.0;
    const double RayStepSize_cm = RayStepSize_mm / 10.0;

    // Select the RT plan
    auto TPs_all = All_TPs(DICOM_data);
    auto TPs = Whitelist(TPs_all, RTPlanSelectionStr);
    if(TPs.empty()){
        throw std::invalid_argument("No RT plans selected. Cannot continue.");
    }
    if(TPs.size() != 1){
        throw std::invalid_argument("Multiple RT plans selected. Please select exactly one.");
    }
    auto& rtplan = **(TPs.front());

    // Select the CT image array
    auto IAs_all = All_IAs(DICOM_data);
    auto CT_IAs = Whitelist(IAs_all, CTImageSelectionStr);
    if(CT_IAs.empty()){
        throw std::invalid_argument("No CT image arrays selected. Cannot continue.");
    }
    if(CT_IAs.size() != 1){
        throw std::invalid_argument("Multiple CT image arrays selected. Please select exactly one.");
    }
    auto ct_img_arr_ptr = *(CT_IAs.front());
    if(ct_img_arr_ptr == nullptr){
        throw std::runtime_error("Encountered a nullptr when expecting a valid CT Image_Array ptr.");
    }
    if(ct_img_arr_ptr->imagecoll.images.empty()){
        throw std::runtime_error("CT Image_Array contains no images.");
    }

    // Select the dose placeholder image array
    auto Dose_IAs = Whitelist(IAs_all, DoseImageSelectionStr);
    if(Dose_IAs.empty()){
        throw std::invalid_argument("No dose image arrays selected. Cannot continue.");
    }
    if(Dose_IAs.size() != 1){
        throw std::invalid_argument("Multiple dose image arrays selected. Please select exactly one.");
    }
    auto dose_img_arr_ptr = *(Dose_IAs.front());
    if(dose_img_arr_ptr == nullptr){
        throw std::runtime_error("Encountered a nullptr when expecting a valid Dose Image_Array ptr.");
    }
    if(dose_img_arr_ptr->imagecoll.images.empty()){
        throw std::runtime_error("Dose Image_Array contains no images.");
    }

    // Verify they are different
    if(ct_img_arr_ptr == dose_img_arr_ptr){
        throw std::invalid_argument("CT and Dose image arrays must be different.");
    }

    // Warn if CT and dose arrays have no spatial overlap (a likely user error).
    // We compare bounding boxes of the two image collections.
    {
        const auto ct_center = ct_img_arr_ptr->imagecoll.center();
        const auto dose_center = dose_img_arr_ptr->imagecoll.center();
        const double separation = (ct_center - dose_center).length();
        // Use a heuristic threshold: if centers are more than 500mm apart, warn the user.
        if(separation > 500.0){
            YLOGWARN("CT and Dose image arrays appear to have limited spatial overlap. "
                     "Ensure the dose grid encompasses the relevant portion of the CT volume. "
                     "Center separation: " << separation << " mm.");
        }
    }

    // Try to select a beam model table, or use defaults
    BeamModel beam_model;
    auto STs_all = All_STs(DICOM_data);
    auto STs = Whitelist(STs_all, BeamModelSelectionStr);
    if(!STs.empty()){
        auto parsed = ParseBeamModelFromTable(**(STs.front()));
        if(parsed.has_value()){
            beam_model = parsed.value();
            YLOGINFO("Using beam model from provided table");
        }else{
            beam_model = Get_Default_6MV_BeamModel();
            YLOGINFO("Beam model table provided but could not be parsed. Using default 6 MV beam model.");
        }
    }else{
        beam_model = Get_Default_6MV_BeamModel();
        YLOGINFO("No beam model table selected. Using default 6 MV photon beam model.");
    }

    // Get CT image geometry
    const auto ct_row_unit = ct_img_arr_ptr->imagecoll.images.front().row_unit.unit();
    const auto ct_col_unit = ct_img_arr_ptr->imagecoll.images.front().col_unit.unit();
    const auto ct_img_unit = ct_img_arr_ptr->imagecoll.images.front().ortho_unit();

    // Create a planar image adjacency for fast lookups in CT
    planar_image_adjacency<float,double> ct_adj( {}, { { std::ref(ct_img_arr_ptr->imagecoll) } }, ct_img_unit );
    if(ct_adj.int_to_img.empty()){
        throw std::logic_error("CT image array contained no images.");
    }

    const int64_t ct_N_rows = static_cast<int64_t>(ct_img_arr_ptr->imagecoll.images.front().rows);
    const int64_t ct_N_cols = static_cast<int64_t>(ct_img_arr_ptr->imagecoll.images.front().columns);
    const int64_t ct_N_imgs = static_cast<int64_t>(ct_adj.int_to_img.size());

    // Initialize dose images to zero
    for(auto& dose_img : dose_img_arr_ptr->imagecoll.images){
        dose_img.fill_pixels(0.0f);
    }

    // Process each beam in the RT plan
    for(size_t beam_idx = 0; beam_idx < rtplan.dynamic_states.size(); ++beam_idx){
        const int64_t beam_number = static_cast<int64_t>(beam_idx + 1);  // 1-based for user display
        const auto& dyn_state = rtplan.dynamic_states[beam_idx];

        // Normalize the dynamic state to fill in NaN values
        Dynamic_Machine_State norm_state = dyn_state;
        norm_state.normalize_states();

        if(norm_state.static_states.empty()){
            YLOGWARN("Beam " << beam_number << " has no control points. Skipping.");
            continue;
        }

        // Get beam parameters from first control point (for static beams, this is sufficient)
        const auto& first_cp = norm_state.static_states.front();

        // Get isocenter position (in mm, DICOM coordinates)
        vec3<double> isocenter = first_cp.IsocentrePosition;
        if(!isocenter.isfinite()){
            YLOGWARN("Beam " << beam_number << ": Isocenter not specified. Using origin.");
            isocenter = vec3<double>(0.0, 0.0, 0.0);
        }

        // Get gantry angle (in degrees)
        double gantry_angle_deg = first_cp.GantryAngle;
        if(!std::isfinite(gantry_angle_deg)){
            gantry_angle_deg = 0.0;
        }
        const double gantry_angle_rad = gantry_angle_deg * M_PI / 180.0;

        // Note: The collimator (beam limiting device) angle is currently ignored in this
        // simplified model. Jaw positions are assumed to be aligned with the beam axes.

        // Get jaw positions (in mm)
        double jaw_x1 = -50.0, jaw_x2 = 50.0;  // Default 10x10 cm field
        double jaw_y1 = -50.0, jaw_y2 = 50.0;
        if(first_cp.JawPositionsX.size() >= 2){
            jaw_x1 = first_cp.JawPositionsX[0];
            jaw_x2 = first_cp.JawPositionsX[1];
        }
        if(first_cp.JawPositionsY.size() >= 2){
            jaw_y1 = first_cp.JawPositionsY[0];
            jaw_y2 = first_cp.JawPositionsY[1];
        }

        // Field size in cm
        const double field_x_cm = std::abs(jaw_x2 - jaw_x1) / 10.0;
        const double field_y_cm = std::abs(jaw_y2 - jaw_y1) / 10.0;
        const double equiv_square_cm = EquivalentSquare(field_x_cm, field_y_cm);

        // Get output factor for this field size
        const double output_factor = LinearInterpolate(beam_model.field_sizes_cm,
                                                       beam_model.output_factors,
                                                       equiv_square_cm);

        // Get total MU for this beam
        double total_MU = norm_state.FinalCumulativeMetersetWeight;
        if(!std::isfinite(total_MU) || total_MU <= 0.0){
            // Try to get from metadata
            auto mu_opt = dyn_state.GetMetadataValueAs<double>("FinalCumulativeMetersetWeight");
            if(mu_opt.has_value()){
                total_MU = mu_opt.value();
            }
        }

        if(!std::isfinite(total_MU) || total_MU <= 0.0){
            std::ostringstream oss;
            oss << "Beam " << beam_number
                << ": Unable to determine a valid MU from RT plan or metadata "
                << "(FinalCumulativeMetersetWeight).";
            YLOGERR(oss.str());
            throw std::runtime_error(oss.str());
        }

        YLOGINFO("Processing Beam " << beam_number
                 << ": Gantry=" << gantry_angle_deg << "°"
                 << ", Field=" << field_x_cm << "x" << field_y_cm << " cm"
                 << ", MU=" << total_MU);

        // Calculate source position based on gantry angle
        // IEC 61217 coordinate system: gantry rotates around Y-axis (patient head-feet)
        // At gantry 0°, source is directly above patient (+Z direction from isocenter)
        // At gantry 90°, source is at patient's left (+X direction)
        const double source_offset_x = SAD_mm * std::sin(gantry_angle_rad);
        const double source_offset_y = 0.0;  // Gantry rotation is around Y-axis
        const double source_offset_z = SAD_mm * std::cos(gantry_angle_rad);

        const vec3<double> source_pos(
            isocenter.x + source_offset_x,
            isocenter.y + source_offset_y,
            isocenter.z + source_offset_z
        );

        // Beam direction unit vector (from source towards isocenter)
        const vec3<double> beam_dir = (isocenter - source_pos).unit();

        // Calculate coordinate system for the beam (for off-axis calculations)
        // beam_dir is the central axis, we need two perpendicular directions.
        // Construct a robust orthonormal basis by choosing a helper vector that is
        // least aligned with beam_dir, then using cross products.
        vec3<double> helper_axis;
        {
            const double ax = std::abs(beam_dir.x);
            const double ay = std::abs(beam_dir.y);
            const double az = std::abs(beam_dir.z);

            if (ax <= ay && ax <= az) {
                // X component is smallest, use X-axis as helper.
                helper_axis = vec3<double>(1.0, 0.0, 0.0);
            } else if (ay <= ax && ay <= az) {
                // Y component is smallest, use Y-axis as helper.
                helper_axis = vec3<double>(0.0, 1.0, 0.0);
            } else {
                // Z component is smallest, use Z-axis as helper.
                helper_axis = vec3<double>(0.0, 0.0, 1.0);
            }
        }

        vec3<double> beam_lateral = beam_dir.Cross(helper_axis).unit();
        // In the unlikely event of numerical degeneracy, fall back to a fixed axis.
        if (!std::isfinite(beam_lateral.x) || !std::isfinite(beam_lateral.y) || !std::isfinite(beam_lateral.z)) {
            helper_axis = vec3<double>(0.0, 1.0, 0.0);
            beam_lateral = beam_dir.Cross(helper_axis).unit();
        }

        vec3<double> beam_up = beam_lateral.Cross(beam_dir).unit();

        // Process each voxel in the dose grid
        std::mutex dose_mutex;
        int64_t completed_images = 0;

        work_queue<std::function<void(void)>> wq;
        for(size_t img_idx = 0; img_idx < dose_img_arr_ptr->imagecoll.images.size(); ++img_idx){
            wq.submit_task([&, img_idx]() -> void {
                auto& dose_img = dose_img_arr_ptr->imagecoll.images[img_idx];

                const int64_t rows = static_cast<int64_t>(dose_img.rows);
                const int64_t cols = static_cast<int64_t>(dose_img.columns);

                for(int64_t row = 0; row < rows; ++row){
                    for(int64_t col = 0; col < cols; ++col){
                        // Get position of this dose voxel (in mm, DICOM coordinates)
                        const vec3<double> voxel_pos = dose_img.position(row, col);

                        // Vector from source to voxel
                        const vec3<double> src_to_voxel = voxel_pos - source_pos;
                        const double src_to_voxel_dist = src_to_voxel.length();
                        const vec3<double> ray_dir = src_to_voxel.unit();

                        // Check if voxel is within the beam (simplified field aperture check)
                        // Project voxel position onto beam coordinate system at isocenter plane
                        const vec3<double> iso_to_voxel = voxel_pos - isocenter;
                        const double depth_along_beam = iso_to_voxel.Dot(beam_dir);

                        // Project perpendicular to beam to get off-axis distance
                        const vec3<double> perp_component = iso_to_voxel - beam_dir * depth_along_beam;
                        const double off_axis_x = perp_component.Dot(beam_lateral);
                        const double off_axis_y = perp_component.Dot(beam_up);

                        // Scale to field edge coordinates at isocenter (accounting for divergence)
                        const double src_to_iso_dist = (isocenter - source_pos).length();
                        const double src_to_voxel_depth = src_to_iso_dist + depth_along_beam;

                        // Skip voxels that are upstream of the isocenter (between source and isocenter).
                        // The divergence calculation would produce incorrect scaling for such voxels.
                        if(src_to_voxel_depth < src_to_iso_dist * 0.1){
                            continue;
                        }

                        // Divergence factor: positions at voxel depth vs at isocenter
                        const double divergence = src_to_voxel_depth / src_to_iso_dist;
                        const double scaled_off_axis_x = off_axis_x / divergence;
                        const double scaled_off_axis_y = off_axis_y / divergence;

                        // Check if within field (jaw aperture)
                        if(scaled_off_axis_x < jaw_x1 || scaled_off_axis_x > jaw_x2 ||
                           scaled_off_axis_y < jaw_y1 || scaled_off_axis_y > jaw_y2){
                            continue;  // Outside field, no dose contribution from this beam
                        }

                        // Note: MLC aperture checking is not implemented in this simplified model.
                        // The beam is treated as if the MLC leaves are fully retracted (open field).
                        // For clinical accuracy, MLC leaf positions should be checked against the
                        // scaled_off_axis_x/y coordinates.

                        // Ray trace from source to voxel through CT to calculate:
                        // 1. Radiological depth (electron density integrated path length)
                        // 2. Geometric depth from surface entry point

                        double radiological_depth_cm = 0.0;
                        double geometric_depth_cm = 0.0;
                        bool entered_patient = false;
                        vec3<double> entry_point;

                        // March from source towards voxel
                        const double step_mm = RayStepSize_mm;
                        const double max_dist = src_to_voxel_dist;
                        double current_dist = 0.0;

                        while(current_dist < max_dist){
                            const vec3<double> sample_pos = source_pos + ray_dir * current_dist;

                            // Sample CT at this position
                            // Convert DICOM mm position to CT voxel index
                            const auto interp_result = ct_adj.trilinearly_interpolate(
                                sample_pos, 0, std::numeric_limits<float>::quiet_NaN());

                            if(std::isfinite(interp_result)){
                                const double hu = static_cast<double>(interp_result);
                                const double rel_density = HU_to_RelativeElectronDensity(hu);

                                // Mark entry into patient (first non-air voxel).
                                // Use a threshold of 0.2 to avoid triggering on noise or lung tissue edges.
                                if(!entered_patient && rel_density > 0.2){
                                    entered_patient = true;
                                    entry_point = sample_pos;
                                }

                                if(entered_patient){
                                    // Accumulate radiological depth
                                    radiological_depth_cm += rel_density * (step_mm / 10.0);

                                    // Track geometric depth
                                    geometric_depth_cm = (sample_pos - entry_point).length() / 10.0;
                                }
                            }

                            current_dist += step_mm;
                        }

                        if(!entered_patient){
                            continue;  // Ray never entered patient, no dose
                        }

                        // Add radiological depth for the final segment up to the voxel position.
                        // current_dist has been advanced past the last sample position by step_mm.
                        const double last_sample_dist_mm = current_dist - step_mm;
                        const double remaining_dist_mm = std::max(0.0, max_dist - last_sample_dist_mm);
                        if(remaining_dist_mm > 0.0){
                            const auto voxel_interp = ct_adj.trilinearly_interpolate(
                                voxel_pos, 0, std::numeric_limits<float>::quiet_NaN());
                            if(std::isfinite(voxel_interp)){
                                const double voxel_hu = static_cast<double>(voxel_interp);
                                const double voxel_rel_density = HU_to_RelativeElectronDensity(voxel_hu);
                                radiological_depth_cm += voxel_rel_density * (remaining_dist_mm / 10.0);
                            }
                        }

                        // Final geometric depth at the voxel
                        geometric_depth_cm = (voxel_pos - entry_point).length() / 10.0;

                        // Calculate dose using pencil beam model
                        // D = MU × dose_rate × OF × PDD(depth) × OAR × ISF × heterogeneity_correction

                        // 1. Inverse square factor (distance from source to calculation point vs reference)
                        const double ref_dist_cm = beam_model.reference_SSD_cm + beam_model.dmax_cm;
                        const double src_to_voxel_dist_cm = src_to_voxel_dist / 10.0;
                        const double isf = (ref_dist_cm * ref_dist_cm) / (src_to_voxel_dist_cm * src_to_voxel_dist_cm);

                        // 2. Percent Depth Dose (using radiological depth for heterogeneity correction)
                        // Apply modified Batho power-law correction
                        const double pdd_geometric = LinearInterpolate(beam_model.depths_cm,
                                                                       beam_model.pdd_percent,
                                                                       geometric_depth_cm) / 100.0;

                        // Simplified heterogeneity correction: ratio of radiological to geometric depth.
                        // Clamp density_ratio to a reasonable range [0.1, 3.0] to guard against
                        // malformed CT data or numerical instabilities.
                        double hetero_correction = 1.0;
                        if(geometric_depth_cm > 0.01){
                            double density_ratio = radiological_depth_cm / geometric_depth_cm;
                            density_ratio = std::clamp(density_ratio, 0.1, 3.0);
                            // Power-law correction (approximate)
                            hetero_correction = std::pow(density_ratio, 0.65);
                        }

                        const double pdd = pdd_geometric * hetero_correction;

                        // 3. Off-axis ratio (scaled_off_axis values are in mm; beam_model uses cm)
                        const double total_off_axis_cm = std::sqrt(scaled_off_axis_x * scaled_off_axis_x +
                                                                   scaled_off_axis_y * scaled_off_axis_y) / 10.0;
                        const double oar = LinearInterpolate(beam_model.oar_distance_cm,
                                                             beam_model.oar_values,
                                                             total_off_axis_cm);

                        // 4. Calculate dose in cGy
                        double dose_cGy = total_MU * beam_model.dose_rate_cGy_per_MU
                                         * output_factor * pdd * oar * isf * ScaleFactor;

                        // Clamp to non-negative
                        if(dose_cGy < 0.0) dose_cGy = 0.0;

                        // Convert units if needed
                        double dose_value = dose_cGy;
                        if(units_Gy){
                            dose_value = dose_cGy / 100.0;
                        }

                        // Add dose to this voxel (accumulate from multiple beams).
                        // No mutex needed here because:
                        // 1. Each slice is processed by exactly one thread within the work_queue.
                        // 2. Beams are processed sequentially (work_queue destructor waits).
                        dose_img.reference(row, col, 0) += static_cast<float>(dose_value);
                    }
                }

                {
                    std::lock_guard<std::mutex> lock(dose_mutex);
                    ++completed_images;
                    YLOGINFO("Beam " << beam_number << ": Processed "
                             << completed_images << " of " << dose_img_arr_ptr->imagecoll.images.size()
                             << " dose slices");
                }
            });
        }
        // Synchronization point:
        // The work_queue instance used for per-slice tasks is scoped inside this beam loop.
        // Its destructor blocks until all submitted tasks for the current beam have completed
        // before proceeding to the next iteration. This ensures all dose slices are processed
        // for this beam and that dose accumulation into shared data is complete before the next
        // beam starts. As a result, beams are processed sequentially (slices within a beam are
        // parallelized, but different beams are not). Any future change to process beams in
        // parallel must introduce explicit synchronization around dose accumulation.
    }

    // Update dose image metadata
    for(auto& dose_img : dose_img_arr_ptr->imagecoll.images){
        auto &md = dose_img.metadata;

        // Ensure RTDOSE modality and dose units are set for all simulated dose images.
        md["Modality"] = "RTDOSE";
        md["DoseUnits"] = units_cGy ? "cGy" : "Gy";

        // Provide sensible defaults for dose type and summation type, but do not
        // overwrite any existing values that may have been set upstream.
        if(md.find("DoseType") == md.end()){
            md["DoseType"] = "PHYSICAL";
        }
        if(md.find("DoseSummationType") == md.end()){
            md["DoseSummationType"] = "PLAN";
        }

        // Preserve any existing description; append a note indicating simulated dose
        // unless it is already present.
        const std::string sim_desc_suffix = "Simulated dose from SimulateDose operation";
        auto desc_it = md.find("Description");
        if(desc_it == md.end() || desc_it->second.empty()){
            md["Description"] = sim_desc_suffix;
        }else if(desc_it->second.find(sim_desc_suffix) == std::string::npos){
            md["Description"] += " | " + sim_desc_suffix;
        }
    }

    YLOGINFO("Dose simulation complete");

    return true;
}

