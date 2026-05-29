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
#include <cstdint>
#include <ostream>

#include "YgorString.h"
#include "YgorMath.h"
#include "YgorTime.h"

#include "Structs.h"


using metadata_map_t = std::map<std::string,std::string>; // key, value.
using metadata_multimap_t = std::map<std::string,std::set<std::string>>; // key, values.
using metadata_stow_t = std::map<std::string,std::optional<std::string>>; // key, value.

// ---------------------------------- Extractors ------------------------------------------
// A routine that extracts metadata from each of the Drover members.
template <class ptr>
std::set<std::string> Extract_Distinct_Values(ptr p, const std::string &key);

// -------------------------------- Generic helpers ---------------------------------------
//Generic helper functions.
std::string Generate_Random_UID(int64_t len);

std::string Generate_Random_Int_Str(int64_t low, int64_t high);

void print( std::ostream &os,
            const metadata_map_t &m );

// Insert a *new* key-value pair. Will not overwrite an existing key-value pair if the key is already present.
// The value is not considered.
//
// Returns true only when insertion successful.
bool insert_if_new( metadata_map_t &map,
                    const std::string &key,
                    const std::string &val );


// Attempts to insert all entries from map_b into map_a, but will not overwrite any existing entried in map_a.
void
coalesce(metadata_map_t &map_a,
         const metadata_map_t &map_b);

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


// Copies the given key and value from one map to another, but only if (1) the key-value is present in the source, or
// (2) a valid optional fallback is provided.
//
// If a new key name is provided, it will be used (only) when inserting into the destination.
bool
copy_overwrite(const metadata_map_t &source,
               metadata_map_t &destination,
               const std::string &key,
               std::optional<std::string> new_key = {},
               std::optional<std::string> fallback = {});

// Filter the key-values in a map by removing all entries that do *not* match the given regex.
metadata_map_t
filter_keys_retain_only( const metadata_map_t &m,
                         const std::regex &f );

// Combine metadata maps together. Only distinct values are retained.
void
combine_distinct(metadata_multimap_t &combined,
                 const metadata_map_t &input);


// Extract the subset of keys that have a single distinct value.
metadata_map_t
singular_keys(const metadata_multimap_t &multi);


// --------------------------------------- Stowing ------------------------------------------
// Temporarily stow metadata key-values and then later replace them.

// Stow using a functor. Note this function cannot encode 'removals'.
//
// Note that stowed key-values are removed when stowed.
metadata_stow_t
stow_metadata( metadata_map_t &m,
               std::optional<metadata_stow_t> stow,
               std::function<bool( const metadata_map_t::iterator &)> f_should_stow );

// Stow a specific key-value.
// If present, the value will be restored later.
// If not present, it will be encoded as a removal when restored.
//
// Note that stowed key-values are removed when stowed.
metadata_stow_t
stow_metadata( metadata_map_t &m,
               std::optional<metadata_stow_t> stow,
               const std::string &key );

void
restore_stowed( metadata_map_t &m,
                metadata_stow_t &stow );

class metadata_stow_guard {
    private:
        std::reference_wrapper<metadata_map_t> l_m;
        std::reference_wrapper<metadata_stow_t> l_m_stow;
    public:
        metadata_stow_guard(metadata_map_t &m, metadata_stow_t &m_stow);
        ~metadata_stow_guard();
};

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
//
// Note that any duplicate keys already present in target will be overwritten (i.e., to_inject takes priority).
enum class metadata_preprocessing {
    none,
    replace_macros,
    evaluate_functions,
    all
};
void inject_metadata( metadata_map_t &target,
                      metadata_map_t &&to_inject,
                      metadata_preprocessing processing = metadata_preprocessing::all );

// Utility function documenting the metadata mutation operation.
OperationArgDoc MetadataInjectionOpArgDoc();

// 'Natural' sort based on metadata values (i.e., optional-wrapped strings).
// This sort separately considers text fragments (lexicographically) and numbers
// (numerically) by splitting non-numbers and numbers into token strings.
// Disengaged optionals are treated as nulls.
bool natural_lt( const std::optional<std::string>& A_opt,
                 const std::optional<std::string>& B_opt );

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
metadata_map_t coalesce_metadata_for_basic_pset(const metadata_map_t &ref, meta_evolve e = meta_evolve::none);
metadata_map_t coalesce_metadata_for_basic_def_reg(const metadata_map_t &ref, meta_evolve e = meta_evolve::none);
metadata_map_t coalesce_metadata_for_basic_table(const metadata_map_t &ref, meta_evolve e = meta_evolve::none);

