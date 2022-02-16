//Metadata.h.

#pragma once

#include <string>
#include <map>
#include <set>
#include <optional>
#include <list>
#include <initializer_list>
#include <functional>
#include <regex>

#include "YgorString.h"
#include "YgorMath.h"
#include "YgorTime.h"

#include "Structs.h"


using metadata_map_t = std::map<std::string,std::string>; // key, value.
using metadata_multimap_t = std::map<std::string,std::set<std::string>>; // key, value.

// ---------------------------------- Extractors ------------------------------------------
// A routine that extracts metadata from each of the Drover members.
template <class ptr>
std::set<std::string> Extract_Distinct_Values(ptr p, const std::string &key);

// -------------------------------- Generic helpers ---------------------------------------
//Generic helper functions.
std::string Generate_Random_UID(long int len);

std::string Generate_Random_Int_Str(long int low, long int high);


// Retrieve the metadata value corresponding to a given key, but only if present and it can be converted to type T.
template <class T>
std::optional<T>
get_as(const metadata_map_t &map,
       const std::string &key);


// Interpret the metadata value corresponding to a given key as a numeric type T, if present and convertible apply the
// given function, and then replace the existing value. The updated value (if a replacement is performed) is
// returned.
//
// This function will not add a new metadata key. It will only update an existing key when it can be convertible to T.
template <class T>
std::optional<T>
apply_as(metadata_map_t &map,
         const std::string &key,
         const std::function<T(T)> &f);


// Combine metadata maps together. Only distinct values are retained.
void
combine_distinct(metadata_multimap_t &combined,
                 const metadata_map_t &input);


// Extract the subset of keys that have a single distinct value.
metadata_map_t
singular_keys(const metadata_multimap_t &multi);

// --------------------------------- Operation helpers --------------------------------------
// Hash both keys and values of a metadata map.
size_t hash_std_map(const metadata_map_t &m);

// Recursively expand the values of the working metadata set. Macros can refer to either working or reference maps.
void recursively_expand_macros(metadata_map_t &working,
                               const metadata_map_t &ref );

// Expand time helper functions in metadata values.
void evaluate_time_functions(metadata_map_t &working,
                             std::optional<time_mark> t_ref);

// Utility function to help parse key-value strings to a metadata list.
metadata_map_t parse_key_values(const std::string &s);

// Insert a copy of the user-provided key-values, but pre-process to replace macros and evaluate known functions.
void inject_metadata( metadata_map_t &target,
                      metadata_map_t &&to_inject );

// Utility function documenting the metadata mutation operation.
OperationArgDoc MetadataInjectionOpArgDoc();

// ----------------------------- Object creation helpers ------------------------------------
// Sets of DICOM metadata key-value elements that provide reasonable defaults or which draw from the provided reference
// map, if available.
//
// Note that these roughly correspond to DICOM modules/macros.
metadata_map_t coalesce_metadata_sop_common(const metadata_map_t &ref);
metadata_map_t coalesce_metadata_patient(const metadata_map_t &ref);
metadata_map_t coalesce_metadata_general_study(const metadata_map_t &ref);
metadata_map_t coalesce_metadata_general_series(const metadata_map_t &ref);
metadata_map_t coalesce_metadata_rt_series(const metadata_map_t &ref);
metadata_map_t coalesce_metadata_patient_study(const metadata_map_t &ref);
metadata_map_t coalesce_metadata_frame_of_reference(const metadata_map_t &ref);
metadata_map_t coalesce_metadata_general_equipment(const metadata_map_t &ref);
metadata_map_t coalesce_metadata_general_image(const metadata_map_t &ref);
metadata_map_t coalesce_metadata_image_plane(const metadata_map_t &ref);
metadata_map_t coalesce_metadata_image_pixel(const metadata_map_t &ref);
metadata_map_t coalesce_metadata_multi_frame(const metadata_map_t &ref);
metadata_map_t coalesce_metadata_voi_lut(const metadata_map_t &ref);
metadata_map_t coalesce_metadata_modality_lut(const metadata_map_t &ref);
metadata_map_t coalesce_metadata_rt_dose(const metadata_map_t &ref);
metadata_map_t coalesce_metadata_ct_image(const metadata_map_t &ref);
metadata_map_t coalesce_metadata_rt_image(const metadata_map_t &ref);
metadata_map_t coalesce_metadata_rt_plan(const metadata_map_t &ref);
metadata_map_t coalesce_metadata_mr_image(const metadata_map_t &ref);
metadata_map_t coalesce_metadata_mr_diffusion(const metadata_map_t &ref);
metadata_map_t coalesce_metadata_mr_spectroscopy(const metadata_map_t &ref);
metadata_map_t coalesce_metadata_mr_private_siemens_diffusion(const metadata_map_t &ref);
metadata_map_t coalesce_metadata_structure_set(const metadata_map_t &ref);
metadata_map_t coalesce_metadata_roi_contour(const metadata_map_t &ref);
metadata_map_t coalesce_metadata_rt_roi_observations(const metadata_map_t &ref);
metadata_map_t coalesce_metadata_misc(const metadata_map_t &ref);


// Modality-specific wrappers.
//
// "iterate" will re-assign per-item UIDs, and may iterate metadata numbers that should be changed for each item, but
// will leave all other information as-is. Iteration is useful when building synthetic image arrays, for example.
enum class meta_evolve {
   none,
   iterate,
};
metadata_map_t coalesce_metadata_for_lsamp(const metadata_map_t &ref, meta_evolve e = meta_evolve::none);
metadata_map_t coalesce_metadata_for_rtdose(const metadata_map_t &ref, meta_evolve e = meta_evolve::none);
metadata_map_t coalesce_metadata_for_rtstruct(const metadata_map_t &ref, meta_evolve e = meta_evolve::none);
metadata_map_t coalesce_metadata_for_basic_image(const metadata_map_t &ref, meta_evolve e = meta_evolve::none);
metadata_map_t coalesce_metadata_for_basic_mr_image(const metadata_map_t &ref, meta_evolve e = meta_evolve::none);
metadata_map_t coalesce_metadata_for_basic_ct_image(const metadata_map_t &ref, meta_evolve e = meta_evolve::none);
metadata_map_t coalesce_metadata_for_basic_mesh(const metadata_map_t &ref, meta_evolve e = meta_evolve::none);
metadata_map_t coalesce_metadata_for_basic_def_reg(const metadata_map_t &ref, meta_evolve e = meta_evolve::none);
metadata_map_t coalesce_metadata_for_basic_table(const metadata_map_t &ref, meta_evolve e = meta_evolve::none);

