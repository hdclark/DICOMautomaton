//Metadata.h.

#pragma once

#include <string>
#include <map>
#include <optional>
#include <list>
#include <initializer_list>
#include <functional>
#include <regex>

#include "YgorString.h"
#include "YgorMath.h"

using metadata_map_t = std::map<std::string,std::string>;

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


// Specify minimum set of metadata elements for all objects, drawing from provided map if available.
metadata_map_t
default_metadata_common(const metadata_map_t &map);

// Specify minimum set of metadata elements for line samples, drawing from provided map if available.
metadata_map_t
default_metadata_lsamp(const metadata_map_t &map);

