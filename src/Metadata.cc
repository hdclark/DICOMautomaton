//Metadata.cc - A part of DICOMautomaton 2021. Written by hal clark.

#include "Metadata.h"

#include <string>
#include <list>
#include <numeric>
#include <initializer_list>
#include <functional>
#include <regex>
#include <optional>
#include <utility>

#include "YgorString.h"
#include "YgorMath.h"
#include "YgorTime.h"

#include "Metadata.h"

template <class T>
std::optional<T>
get_as(const metadata_map_t &map,
       const std::string &key){

    const auto metadata_cit = map.find(key);
    if( (metadata_cit == std::end(map))
    ||  !Is_String_An_X<T>(metadata_cit->second) ){
        return std::optional<T>();
    }else{
        return std::make_optional(stringtoX<T>(metadata_cit->second));
    }
}
//template std::optional<uint8_t    > get_as(const metadata_map_t &, const std::string &);
//template std::optional<uint16_t   > get_as(const metadata_map_t &, const std::string &);
template std::optional<uint32_t   > get_as(const metadata_map_t &, const std::string &);
template std::optional<uint64_t   > get_as(const metadata_map_t &, const std::string &);
//template std::optional<int8_t     > get_as(const metadata_map_t &, const std::string &);
//template std::optional<int16_t    > get_as(const metadata_map_t &, const std::string &);
template std::optional<int32_t    > get_as(const metadata_map_t &, const std::string &);
template std::optional<int64_t    > get_as(const metadata_map_t &, const std::string &);
template std::optional<float      > get_as(const metadata_map_t &, const std::string &);
template std::optional<double     > get_as(const metadata_map_t &, const std::string &);
template std::optional<std::string> get_as(const metadata_map_t &, const std::string &);


template <class T>
std::optional<T>
apply_as(metadata_map_t &map,
         const std::string &key,
         const std::function<T(T)> &f){

    auto val = get_as<T>(map, key);
    if(val){
        val = f(val.value());
        map[key] = Xtostring<T>(val.value());
    }
    return val;
}
//template std::optional<uint8_t    > apply_as(metadata_map_t &, const std::string &, const std::function<uint8_t    (uint8_t    )> &);
//template std::optional<uint16_t   > apply_as(metadata_map_t &, const std::string &, const std::function<uint16_t   (uint16_t   )> &);
template std::optional<uint32_t   > apply_as(metadata_map_t &, const std::string &, const std::function<uint32_t   (uint32_t   )> &);
template std::optional<uint64_t   > apply_as(metadata_map_t &, const std::string &, const std::function<uint64_t   (uint64_t   )> &);
//template std::optional<int8_t     > apply_as(metadata_map_t &, const std::string &, const std::function<int8_t    (int8_t      )> &);
//template std::optional<int16_t    > apply_as(metadata_map_t &, const std::string &, const std::function<int16_t    (int16_t    )> &);
template std::optional<int32_t    > apply_as(metadata_map_t &, const std::string &, const std::function<int32_t    (int32_t    )> &);
template std::optional<int64_t    > apply_as(metadata_map_t &, const std::string &, const std::function<int64_t    (int64_t    )> &);
template std::optional<float      > apply_as(metadata_map_t &, const std::string &, const std::function<float      (float      )> &);
template std::optional<double     > apply_as(metadata_map_t &, const std::string &, const std::function<double     (double     )> &);
template std::optional<std::string> apply_as(metadata_map_t &, const std::string &, const std::function<std::string(std::string)> &);


metadata_map_t
default_metadata_common(const metadata_map_t &map){
    metadata_map_t out;
    const auto insert = [&](const std::string &key, const std::string &default_val){
        out[key] = get_as<std::string>(map, key).value_or(default_val);
        return;
    };
    insert("FrameOfReferenceUID", "unspecified");
    insert("SeriesDescription", "unspecified");

    return out;
}


metadata_map_t
default_metadata_lsamp(const metadata_map_t &map){
    metadata_map_t out = default_metadata_common(map);
    const auto insert = [&](const std::string &key, const std::string &default_val){
        out[key] = get_as<std::string>(map, key).value_or(default_val);
        return;
    };
    insert("LineName", "unspecified");
    insert("Modality", "LS");

    return out;
}

