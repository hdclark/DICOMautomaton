//Metadata.cc - A part of DICOMautomaton 2021. Written by hal clark.

#include <string>
#include <list>
#include <numeric>
#include <initializer_list>
#include <functional>
#include <regex>
#include <optional>
#include <utility>
#include <random>
#include <chrono>

#include "YgorString.h"
#include "YgorMath.h"
#include "YgorTime.h"

#include "Metadata.h"

static
void insert(metadata_map_t &out, const std::string &key, const std::string &val){
    out[key] = val;
    return;
}

static
void insert_or_default(metadata_map_t &out, const metadata_map_t &ref, const std::string &key, const std::string &default_val){
    out[key] = get_as<std::string>(ref, key).value_or(default_val);
    return;
}

static
void insert_if_nonempty(metadata_map_t &out, const metadata_map_t &ref, const std::string &key){
    if(auto val = get_as<std::string>(ref, key)){
        out[key] = val.value();
    }
    return;
}

static
std::pair<std::string,std::string> get_date_time(){
    const auto datetime_now = time_mark().Dump_as_postgres_string(); // "2013-11-30 13:05:35"
    const auto date_now = datetime_now.substr(0,10);
    const auto time_now = datetime_now.substr(12);
    return {date_now, time_now};
}

std::string Generate_Random_UID(long int len){
    std::string out;
    const std::string alphanum(R"***(.0123456789)***");
    std::default_random_engine gen;

    try{
        std::random_device rd;  //Constructor can fail if many threads create instances (maybe limited fd's?).
        gen.seed(rd()); //Seed with a true random number.
    }catch(const std::exception &){
        const auto timeseed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        gen.seed(timeseed); //Seed with time. 
    }

    std::uniform_int_distribution<int> dist(0,alphanum.length()-1);
    out = "1.2.840.66.1.";
    char last = '.';
    while(static_cast<long int>(out.size()) != len){
        const auto achar = alphanum[dist(gen)];
        if((achar == '0') && (last == '.')) continue; // Zeros are not significant.
        if((achar == '.') && (achar == last)) continue; // Do not double separators.
        if((achar == '.') && (static_cast<long int>(out.size()+1) == len)) continue; // Do not stop on a separator.
        out += achar;
        last = achar;
    }
    return out;
}

std::string Generate_Random_Int_Str(long int L, long int H){
    std::string out;
    std::default_random_engine gen;

    try{
        std::random_device rd;  //Constructor can fail if many threads create instances (maybe limited fd's?).
        gen.seed(rd()); //Seed with a true random number.
    }catch(const std::exception &){
        const auto timeseed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        gen.seed(timeseed); //Seed with time. 
    }

    std::uniform_int_distribution<long int> dist(L,H);
    return std::to_string(dist(gen));
}


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


// Combine metadata maps together. Only distinct values are retained.
void
combine_distinct(metadata_multimap_t &combined,
                 const metadata_map_t &input){
    for(const auto& [key, val] : input) combined[key].insert(val);
    return;
}

// Extract the subset of keys that have a single distinct value.
metadata_map_t
singular_keys(const metadata_multimap_t &multi){
    metadata_map_t out;
    for(const auto& [key, vals] : multi){
        if(vals.size() == 1){
            out[key] = *std::begin(vals);
        }
    }
    return out;
}

// This is not provided in the std library. Not sure why?
size_t hash_std_map(const metadata_map_t &m){
    size_t h = 0;
    for(const auto &p : m){
        h ^= std::hash<std::string>{}(p.first);
        h ^= std::hash<std::string>{}(p.second);
    }
    return h;
}


void recursively_expand_macros(metadata_map_t &working,
                               const metadata_map_t &ref ){
    // Continually attempt replacements until no changes occur. This will cover recursive changes (up to a
    // point) which adds some extra capabilities.
    auto prev_hash = hash_std_map(working);
    long int i = 0;
    while(true){
        // Expand macros against the reference metadata, if any are present.
        for(auto &kv : working){
            kv.second = ExpandMacros(kv.second, ref);
        }

        // Expand macros against the working metadata, if any are present.
        for(auto &kv : working){
            kv.second = ExpandMacros(kv.second, working);
        }

        const auto new_hash = hash_std_map(working);
        if(prev_hash == new_hash) break;
        prev_hash = new_hash;
        if(500 < ++i){
            throw std::invalid_argument("Excessive number of recursive macro replacements detected.");
        }
    }
    return;
}

void evaluate_time_functions(metadata_map_t &working,
                             std::optional<time_mark> t_ref){
    if(!t_ref){
        t_ref = time_mark();
        t_ref.value().Set_unix_epoch();
    }

    for(auto &kv : working){
        // See if the 'to_seconds()' function is present.
        {
            //const auto to_seconds_regex = std::regex(R"***((.*)to_seconds[(]([^)]*)[)](.*))***", std::regex::icase | std::regex::optimize );
            //if(const auto tokens = GetAllRegex2(kv.second, to_seconds_regex); (tokens.size() == 3) ){
            const auto p1 = kv.second.find("to_seconds(");
            const auto p2 = kv.second.find(")", p1);
            if( (p1 != std::string::npos)
            &&  (p2 != std::string::npos)
            &&  (p1 < p2) ){
                const auto token_0 = kv.second.substr(0,p1+11);
                const auto token_1 = kv.second.substr(p1+11,(p2-p1-11));
                const auto token_2 = kv.second.substr(p2);

                double fractional_seconds = 0.0;
                if(time_mark t; t.Read_from_string(token_1, &fractional_seconds) ){
                    const auto seconds = std::to_string(static_cast<double>(t_ref.value().Diff_in_Seconds(t)) + fractional_seconds);
                    kv.second = kv.second.substr(0,p1) + seconds + kv.second.substr(p2+1);
                }
            }
        }
    }
    return;
}

metadata_map_t parse_key_values(const std::string &s){
    // Parse user-provided metadata, if any has been provided.
    std::map<std::string,std::string> key_values;
    if(s.empty()) return key_values;

    for(const auto& a : SplitStringToVector(s, ';', 'd')){
        auto b = SplitStringToVector(a, '@', 'd');
        if(b.size() != 2) throw std::runtime_error("Cannot parse subexpression: "_s + a);

        key_values[b.front()] = b.back();
    }
    return key_values;
}

// Insert a copy of the user-provided key-values, but pre-process to replace macros and evaluate known functions.
void inject_metadata( metadata_map_t &target,
                      metadata_map_t &&to_inject ){

    recursively_expand_macros(to_inject, target);
    evaluate_time_functions(to_inject, std::nullopt);

    // Update or insert all metadata.
    to_inject.merge(target);
    target = to_inject;
    //for(const auto &kv_pair : to_inject){
    //    target[ kv_pair.first ] = kv_pair.second;
    //}
    return;
}

OperationArgDoc MetadataInjectionOpArgDoc(){
    OperationArgDoc out;

    out.name = "KeyValues";
    out.desc = "Key-value pairs in the form of 'key1@value1;key2@value2' that will be injected into the"
               " selected objects."
               " Values can use macros that refer to other metadata keys using the '$' character."
               " If macros refer to non-existent metadata elements, then the replacement is literal."
               " Dates, times, and datetimes can be converted to seconds (since the Unix epoch) using the"
               " 'to_seconds()' function."
               "\n\n"
               "Existing conflicting metadata will be overwritten."
               " Both keys and values are case-sensitive."
               " Note that a semi-colon separates key-value pairs, not a colon."
               " Note that quotation marks are not stripped internally, but may have to be"
               " provided on the command line for shells to properly interpret the argument."
               " Also note that updating spatial metadata will not result in the object characteristics"
               " being altered -- use the specific parameters provided to update spatial characteristics.";
    out.default_val = "all";
    out.expected = false;
    out.examples = { "Description@'some description'",
                     "'Description@some description'", 
                     "'Description@Research scan performed on $ContentDate'", 
                     "'ContentTimeInSeconds@to_seconds($ContentDate-$ContentDate)'", 
                     "MinimumSeparation@1.23", 
                     "'Description@some description;MinimumSeparation@1.23'" };
    return out;
}

metadata_map_t coalesce_metadata_sop_common(const metadata_map_t &ref){
    metadata_map_t out;
    const auto SOPInstanceUID = Generate_Random_UID(60);
    const auto [date_now, time_now] = get_date_time();

    //Common base elements which are convenient to put here...
    out["ImplementationVersionName"] = "DICOMautomaton";
    out["ImplementationClassUID"] = "1.2.513.264.765.1.1.578";
    insert_or_default(out, ref, "MediaStorageSOPInstanceUID", SOPInstanceUID);
    insert_if_nonempty(out, ref, "Filename");

    //SOP Common Module.
    insert_or_default(out, ref, "SOPInstanceUID", SOPInstanceUID);
    insert_or_default(out, ref, "InstanceCreationDate", date_now);
    insert_or_default(out, ref, "InstanceCreationTime", time_now);
    insert_or_default(out, ref, "InstanceCreatorUID", out["ImplementationClassUID"]);
    insert_or_default(out, ref, "InstanceNumber", "");
    return out;
}

metadata_map_t coalesce_metadata_patient(const metadata_map_t &ref){
    metadata_map_t out;
    const auto [date_now, time_now] = get_date_time();

    //Patient Module.
    insert_or_default(out, ref, "PatientsName", "DICOMautomaton^DICOMautomaton");
    insert_or_default(out, ref, "PatientID", "DCMA_"_s + Generate_Random_String_of_Length(10));
    insert_or_default(out, ref, "PatientsGender", "O");
    insert_or_default(out, ref, "PatientsBirthDate", date_now);
    insert_or_default(out, ref, "PatientsBirthTime", time_now);
    return out;
}

metadata_map_t coalesce_metadata_general_study(const metadata_map_t &ref){
    metadata_map_t out;
    const auto [date_now, time_now] = get_date_time();

    //General Study Module.
    insert_or_default(out, ref, "StudyInstanceUID", Generate_Random_UID(31) );
    insert_or_default(out, ref, "StudyDate", date_now );
    insert_or_default(out, ref, "StudyTime", time_now );
    insert_or_default(out, ref, "ReferringPhysiciansName", "UNSPECIFIED^UNSPECIFIED" );
    insert_or_default(out, ref, "StudyID", "DCMA_"_s + Generate_Random_String_of_Length(10) ); // i.e., "Course"
    insert_or_default(out, ref, "AccessionNumber", Generate_Random_String_of_Length(14) );
    insert_or_default(out, ref, "StudyDescription", "UNSPECIFIED" );
    return out;
}

metadata_map_t coalesce_metadata_general_series(const metadata_map_t &ref){
    metadata_map_t out;
    const auto [date_now, time_now] = get_date_time();

    //General Series Module.
    insert_or_default(out, ref, "Modality", "UNSPECIFIED");
    insert_or_default(out, ref, "SeriesInstanceUID", Generate_Random_UID(31));
    insert_or_default(out, ref, "SeriesNumber", Generate_Random_Int_Str(5000, 32767)); // Upper: 2^15 - 1.
    insert_or_default(out, ref, "SeriesDate", date_now);
    insert_or_default(out, ref, "SeriesTime", time_now);
    insert_or_default(out, ref, "SeriesDescription", "UNSPECIFIED");
    insert_or_default(out, ref, "BodyPartExamined", "");
    insert_or_default(out, ref, "PatientPosition", "");
    insert_or_default(out, ref, "RequestedProcedureID", "UNSPECIFIED");
    insert_or_default(out, ref, "ScheduledProcedureStepID", "UNSPECIFIED");
    insert_or_default(out, ref, "OperatorsName", "UNSPECIFIED");
    return out;
}

metadata_map_t coalesce_metadata_patient_study(const metadata_map_t &ref){
    metadata_map_t out;

    //Patient Study Module.
    insert_or_default(out, ref, "PatientsWeight", "");
    return out;
}

metadata_map_t coalesce_metadata_frame_of_reference(const metadata_map_t &ref){
    metadata_map_t out;

    //Frame of Reference Module.
    insert_or_default(out, ref, "FrameOfReferenceUID", Generate_Random_UID(32));
    insert_or_default(out, ref, "PositionReferenceIndicator", "BB");
    if(const auto o = get_as<std::string>(ref, "ReferencedFrameOfReferenceSequence/FrameOfReferenceUID")){
        // Allow a newer-style FrameOfReferenceUID tag to supercede an earlier-style tag, if present.
        //
        // Note that each contour can have a separate FrameOfReferenceUID. This simple mapping won't work in those cases.
        out["FrameOfReferenceUID"] = o.value();
    }
    return out;
}

metadata_map_t coalesce_metadata_general_equipment(const metadata_map_t &ref){
    metadata_map_t out;

    //General Equipment Module.
    insert_or_default(out, ref, "Manufacturer", "UNSPECIFIED");
    insert_or_default(out, ref, "InstitutionName", "UNSPECIFIED");
    insert_or_default(out, ref, "StationName", "UNSPECIFIED");
    insert_or_default(out, ref, "InstitutionalDepartmentName", "UNSPECIFIED");
    insert_or_default(out, ref, "ManufacturersModelName", "UNSPECIFIED");
    insert_or_default(out, ref, "SoftwareVersions", "UNSPECIFIED");
    insert_or_default(out, ref, "DeviceSerialNumber", "");
    return out;
}

metadata_map_t coalesce_metadata_general_image(const metadata_map_t &ref){
    metadata_map_t out;
    const auto [date_now, time_now] = get_date_time();

    //General Image Module.
    insert_or_default(out, ref, "InstanceNumber", "");
    insert_or_default(out, ref, "PatientOrientation", "UNSPECIFIED");
    insert_or_default(out, ref, "ContentDate", date_now);
    insert_or_default(out, ref, "ContentTime", time_now);
    insert_if_nonempty(out, ref, "ImageType");
    insert_if_nonempty(out, ref, "AcquisitionNumber");
    insert_if_nonempty(out, ref, "AcquisitionDate");
    insert_if_nonempty(out, ref, "AcquisitionTime");
    insert_if_nonempty(out, ref, "DerivationDescription");
    insert_if_nonempty(out, ref, "DerivationCodeSequence");
    insert_if_nonempty(out, ref, "ImagesInAcquisition");
    insert_if_nonempty(out, ref, "ImageComments");
    insert_if_nonempty(out, ref, "QualityControlImage");
    out["ImageComments"] = "Research image generated by DICOMautomaton. Not for clinical use!";
    return out;
}

metadata_map_t coalesce_metadata_image_plane(const metadata_map_t &ref){
    metadata_map_t out;

    //Image Plane Module.
    insert_if_nonempty(out, ref, "PixelSpacing");
    insert_if_nonempty(out, ref, "ImageOrientationPatient");
    insert_if_nonempty(out, ref, "ImagePositionPatient");
    insert_if_nonempty(out, ref, "SliceThickness");
    insert_if_nonempty(out, ref, "SliceLocation");
    return out;
}

metadata_map_t coalesce_metadata_image_pixel(const metadata_map_t &ref){
    metadata_map_t out;

    //Image Pixel Module.
    insert_or_default(out, ref, "SamplesPerPixel", "1");
    insert_or_default(out, ref, "PhotometricInterpretation", "MONOCHROME2");
    insert_if_nonempty(out, ref, "Rows"); // == Ygor row_count.
    insert_if_nonempty(out, ref, "Columns"); // == Ygor col_count.

    insert_or_default(out, ref, "BitsAllocated", "32");
    insert_or_default(out, ref, "BitsStored", "32");
    insert_or_default(out, ref, "HighBit", "31");
    insert_or_default(out, ref, "PixelRepresentation", "0"); // 0 == unsigned.

    insert_if_nonempty(out, ref, "PlanarConfiguration");
    insert_if_nonempty(out, ref, "PixelAspectRatio");
    return out;
}

metadata_map_t coalesce_metadata_multi_frame(const metadata_map_t &ref){
    metadata_map_t out;

    //Multi-Frame Module.
    insert_if_nonempty(out, ref, "NumberOfFrames"); // == number of images.
    insert_if_nonempty(out, ref, "FrameIncrementPointer"); // Default to (3004,000c).
    return out;
}

metadata_map_t coalesce_metadata_voi_lut(const metadata_map_t &ref){
    metadata_map_t out;

    //VOI LUT Module.
    insert_if_nonempty(out, ref, "WindowCenter");
    insert_if_nonempty(out, ref, "WindowWidth");
    return out;
}

metadata_map_t coalesce_metadata_modality_lut(const metadata_map_t &ref){
    metadata_map_t out;

    //Modality LUT Module.
    //insert_if_nonempty(out, ref, "ModalityLUTSequence");
    insert_if_nonempty(out, ref, "LUTDescriptor");
    insert_if_nonempty(out, ref, "ModalityLUTType");
    insert_if_nonempty(out, ref, "LUTData");
    insert_if_nonempty(out, ref, "RescaleIntercept");
    insert_if_nonempty(out, ref, "RescaleSlope");
    insert_if_nonempty(out, ref, "RescaleType");
    return out;
}

metadata_map_t coalesce_metadata_rt_dose(const metadata_map_t &ref){
    metadata_map_t out;

    //RT Dose Module.
    insert_if_nonempty(out, ref, "SamplesPerPixel");
    insert_if_nonempty(out, ref, "PhotometricInterpretation");
    insert_if_nonempty(out, ref, "BitsAllocated");
    insert_if_nonempty(out, ref, "BitsStored");
    insert_if_nonempty(out, ref, "HighBit");
    insert_if_nonempty(out, ref, "PixelRepresentation");
    insert_if_nonempty(out, ref, "DoseUnits");
    insert_if_nonempty(out, ref, "DoseType");
    insert_if_nonempty(out, ref, "DoseSummationType");
    insert_if_nonempty(out, ref, "DoseGridScaling");

    insert_if_nonempty(out, ref, "ReferencedRTPlanSequence/ReferencedSOPClassUID");
    insert_if_nonempty(out, ref, "ReferencedRTPlanSequence/ReferencedSOPInstanceUID");
    insert_if_nonempty(out, ref, "ReferencedFractionGroupSequence/ReferencedFractionGroupNumber");
    insert_if_nonempty(out, ref, "ReferencedBeamSequence/ReferencedBeamNumber");
    insert_if_nonempty(out, ref, "ReferencedRTPlanSequence/ReferencedFractionGroupSequence/ReferencedBeamSequence/ReferencedBeamNumber");
    return out;
}

metadata_map_t coalesce_metadata_ct_image(const metadata_map_t &ref){
    metadata_map_t out;

    //CT Image Module.
    insert_if_nonempty(out, ref, "KVP");
    return out;
}

metadata_map_t coalesce_metadata_rt_image(const metadata_map_t &ref){
    metadata_map_t out;

    //RT Image Module.
    insert_if_nonempty(out, ref, "RTImageLabel");
    insert_if_nonempty(out, ref, "RTImageDescription");
    insert_if_nonempty(out, ref, "ReportedValuesOrigin");
    insert_if_nonempty(out, ref, "RTImagePlane");
    insert_if_nonempty(out, ref, "XRayImageReceptorTranslation");
    insert_if_nonempty(out, ref, "XRayImageReceptorAngle");
    insert_if_nonempty(out, ref, "RTImageOrientation");
    insert_if_nonempty(out, ref, "ImagePlanePixelSpacing");
    insert_if_nonempty(out, ref, "RTImagePosition");
    insert_if_nonempty(out, ref, "RadiationMachineName");
    insert_if_nonempty(out, ref, "RadiationMachineSAD");
    insert_if_nonempty(out, ref, "RTImageSID");
    insert_if_nonempty(out, ref, "FractionNumber");

    insert_if_nonempty(out, ref, "PrimaryDosimeterUnit");
    insert_if_nonempty(out, ref, "GantryAngle");
    insert_if_nonempty(out, ref, "BeamLimitingDeviceAngle");
    insert_if_nonempty(out, ref, "PatientSupportAngle");
    insert_if_nonempty(out, ref, "TableTopVerticalPosition");
    insert_if_nonempty(out, ref, "TableTopLongitudinalPosition");
    insert_if_nonempty(out, ref, "TableTopLateralPosition");
    insert_if_nonempty(out, ref, "IsocenterPosition");

    insert_if_nonempty(out, ref, "ReferencedBeamNumber");
    insert_if_nonempty(out, ref, "StartCumulativeMetersetWeight");
    insert_if_nonempty(out, ref, "EndCumulativeMetersetWeight");
    insert_if_nonempty(out, ref, "ReferencedFractionGroupNumber");

    insert_if_nonempty(out, ref, "ExposureSequence/KVP");
    insert_if_nonempty(out, ref, "ExposureSequence/ExposureTime");
    insert_if_nonempty(out, ref, "ExposureSequence/MetersetExposure");
    insert_if_nonempty(out, ref, "ExposureSequence/BeamLimitingDeviceSequence/RTBeamLimitingDeviceType");
    insert_if_nonempty(out, ref, "ExposureSequence/BeamLimitingDeviceSequence/NumberOfLeafJawPairs");
    insert_if_nonempty(out, ref, "ExposureSequence/BeamLimitingDeviceSequence/LeafJawPositions");
    return out;
}

metadata_map_t coalesce_metadata_rt_plan(const metadata_map_t &ref){
    metadata_map_t out;

    //RT Plan Module.
    insert_if_nonempty(out, ref, "RTPlanLabel");
    insert_if_nonempty(out, ref, "RTPlanName");
    insert_if_nonempty(out, ref, "RTPlanDescription");
    insert_if_nonempty(out, ref, "RTPlanDate");
    insert_if_nonempty(out, ref, "RTPlanTime");
    insert_if_nonempty(out, ref, "RTPlanGeometry");
    return out;
}

metadata_map_t coalesce_metadata_mr_image(const metadata_map_t &ref){
    metadata_map_t out;

    // MR Image Module
    insert_if_nonempty(out, ref, "ScanningSequence");
    insert_if_nonempty(out, ref, "SequenceVariant");
    insert_if_nonempty(out, ref, "SequenceName");
    insert_if_nonempty(out, ref, "ScanOptions");
    insert_if_nonempty(out, ref, "MRAcquisitionType");
    insert_if_nonempty(out, ref, "RepetitionTime");
    insert_if_nonempty(out, ref, "EchoTime");
    insert_if_nonempty(out, ref, "EchoTrainLength");
    insert_if_nonempty(out, ref, "InversionTime");
    insert_if_nonempty(out, ref, "TriggerTime");

    insert_if_nonempty(out, ref, "AngioFlag");
    insert_if_nonempty(out, ref, "NominalInterval");
    insert_if_nonempty(out, ref, "CardiacNumberofImages");

    insert_if_nonempty(out, ref, "NumberofAverages");
    insert_if_nonempty(out, ref, "ImagingFrequency");
    insert_if_nonempty(out, ref, "ImagedNucleus");
    insert_if_nonempty(out, ref, "EchoNumbers");
    insert_if_nonempty(out, ref, "MagneticFieldStrength");

    insert_if_nonempty(out, ref, "SpacingBetweenSlices");
    insert_if_nonempty(out, ref, "NumberofPhaseEncodingSteps");
    insert_if_nonempty(out, ref, "PercentSampling");
    insert_if_nonempty(out, ref, "PercentPhaseFieldofView");
    insert_if_nonempty(out, ref, "PixelBandwidth");

    insert_if_nonempty(out, ref, "ReceiveCoilName");
    insert_if_nonempty(out, ref, "TransmitCoilName");
    insert_if_nonempty(out, ref, "AcquisitionMatrix");
    insert_if_nonempty(out, ref, "InplanePhaseEncodingDirection");
    insert_if_nonempty(out, ref, "FlipAngle");
    insert_if_nonempty(out, ref, "VariableFlipAngleFlag");
    insert_if_nonempty(out, ref, "SAR");
    insert_if_nonempty(out, ref, "dB_dt");
    return out;
}

metadata_map_t coalesce_metadata_mr_diffusion(const metadata_map_t &ref){
    metadata_map_t out;

    //MR Diffusion Macro Attributes.
    insert_if_nonempty(out, ref, "MRDiffusionSequence/DiffusionBValue");
    insert_if_nonempty(out, ref, "MRDiffusionSequence/DiffusionDirection");
    insert_if_nonempty(out, ref, "MRDiffusionSequence/DiffusionGradientDirectionSequence/DiffusionGradientOrientation");
    insert_if_nonempty(out, ref, "MRDiffusionSequence/DiffusionAnisotropyType");
    return out;
}

metadata_map_t coalesce_metadata_mr_spectroscopy(const metadata_map_t &ref){
    metadata_map_t out;

    //MR Image and Spectroscopy Instance Macro.
    insert_if_nonempty(out, ref, "AcquisitionDuration");
    return out;
}

metadata_map_t coalesce_metadata_mr_private_siemens_diffusion(const metadata_map_t &ref){
    metadata_map_t out;

    //Siemens MR Private Diffusion Module, as detailed in syngo(R) MR E11 conformance statement.
    insert_if_nonempty(out, ref, "SiemensMRHeader");
    insert_if_nonempty(out, ref, "DiffusionBValue");
    insert_if_nonempty(out, ref, "DiffusionDirection");
    insert_if_nonempty(out, ref, "DiffusionGradientVector");
    insert_if_nonempty(out, ref, "DiffusionBMatrix");  // multiplicity = 3.
    insert_if_nonempty(out, ref, "PixelRepresentation"); // multiplicity = 6.
    return out;
}

metadata_map_t coalesce_metadata_misc(const metadata_map_t &ref){
    metadata_map_t out;

    insert_if_nonempty(out, ref, "SliceNumber");
    insert_if_nonempty(out, ref, "ImageIndex");

    insert_if_nonempty(out, ref, "GridFrameOffsetVector");

    insert_if_nonempty(out, ref, "TemporalPositionIdentifier");
    insert_if_nonempty(out, ref, "TemporalPositionIndex");
    insert_if_nonempty(out, ref, "NumberofTemporalPositions");

    insert_if_nonempty(out, ref, "TemporalResolution");
    insert_if_nonempty(out, ref, "FrameReferenceTime");
    insert_if_nonempty(out, ref, "FrameTime");
    insert_if_nonempty(out, ref, "TriggerTime");
    insert_if_nonempty(out, ref, "TriggerTimeOffset");

    insert_if_nonempty(out, ref, "PerformedProcedureStepStartDate");
    insert_if_nonempty(out, ref, "PerformedProcedureStepStartTime");
    insert_if_nonempty(out, ref, "PerformedProcedureStepEndDate");
    insert_if_nonempty(out, ref, "PerformedProcedureStepEndTime");

    insert_if_nonempty(out, ref, "Exposure");
    insert_if_nonempty(out, ref, "ExposureTime");
    insert_if_nonempty(out, ref, "ExposureInMicroAmpereSeconds");
    insert_if_nonempty(out, ref, "XRayTubeCurrent");

    insert_if_nonempty(out, ref, "ProtocolName");

    insert_if_nonempty(out, ref, "ReferringPhysicianName");
    return out;
}


metadata_map_t coalesce_metadata_for_lsamp(const metadata_map_t &ref, meta_evolve e){
    metadata_map_t out;
    out["Modality"] = "LINESAMPLE";
    out["MediaStorageSOPClassUID"] = "";
    out["SOPClassUID"] = "";

    insert_or_default(out, ref, "LineName", "unspecified");
    insert_or_default(out, ref, "Abscissa", "unspecified");
    insert_or_default(out, ref, "Ordinate", "unspecified");

    out.merge( coalesce_metadata_sop_common(ref) );
    out.merge( coalesce_metadata_patient(ref) );
    out.merge( coalesce_metadata_general_study(ref) );
    out.merge( coalesce_metadata_general_series(ref) );
    out.merge( coalesce_metadata_patient_study(ref) );
    out.merge( coalesce_metadata_frame_of_reference(ref) );
    out.merge( coalesce_metadata_general_equipment(ref) );
    out.merge( coalesce_metadata_misc(ref) );

    if(e == meta_evolve::iterate){
        // Assign a new SOP Instance UID.
        auto new_sop = coalesce_metadata_sop_common({});
        insert(out, "SOPInstanceUID", new_sop["SOPInstanceUID"]);
        insert(out, "MediaStorageSOPInstanceUID", new_sop["MediaStorageSOPInstanceUID"]);
    }
    return out;
}

metadata_map_t coalesce_metadata_for_rtdose(const metadata_map_t &ref, meta_evolve e){
    metadata_map_t out;
    out["Modality"] = "RTDOSE";
    out["MediaStorageSOPClassUID"] = "1.2.840.10008.5.1.4.1.1.481.2"; //Radiation Therapy Dose Storage
    out["SOPClassUID"] = "1.2.840.10008.5.1.4.1.1.481.2";

    out.merge( coalesce_metadata_sop_common(ref) );
    out.merge( coalesce_metadata_patient(ref) );
    out.merge( coalesce_metadata_general_study(ref) );
    out.merge( coalesce_metadata_general_series(ref) );
    out.merge( coalesce_metadata_patient_study(ref) );
    out.merge( coalesce_metadata_frame_of_reference(ref) );
    out.merge( coalesce_metadata_general_equipment(ref) );
    out.merge( coalesce_metadata_general_image(ref) );
    out.merge( coalesce_metadata_image_plane(ref) );
    out.merge( coalesce_metadata_image_pixel(ref) );
    out.merge( coalesce_metadata_multi_frame(ref) );
    out.merge( coalesce_metadata_voi_lut(ref) );
    out.merge( coalesce_metadata_rt_dose(ref) );
    out.merge( coalesce_metadata_misc(ref) );

    if(e == meta_evolve::iterate){
        // Assign a new SOP Instance UID.
        auto new_sop = coalesce_metadata_sop_common({});
        insert(out, "SOPInstanceUID", new_sop["SOPInstanceUID"]);
        insert(out, "MediaStorageSOPInstanceUID", new_sop["MediaStorageSOPInstanceUID"]);
    }
    return out;
}

metadata_map_t coalesce_metadata_for_basic_image(const metadata_map_t &ref, meta_evolve e){
    metadata_map_t out;

    out.merge( coalesce_metadata_sop_common(ref) );
    out.merge( coalesce_metadata_patient(ref) );
    out.merge( coalesce_metadata_general_study(ref) );
    out.merge( coalesce_metadata_general_series(ref) );
    out.merge( coalesce_metadata_patient_study(ref) );
    out.merge( coalesce_metadata_frame_of_reference(ref) );
    out.merge( coalesce_metadata_general_equipment(ref) );
    out.merge( coalesce_metadata_general_image(ref) );
    out.merge( coalesce_metadata_image_plane(ref) );
    out.merge( coalesce_metadata_image_pixel(ref) );
    out.merge( coalesce_metadata_misc(ref) );

    if(e == meta_evolve::iterate){
        // Assign a new SOP Instance UID.
        auto new_sop = coalesce_metadata_sop_common({});
        insert(out, "SOPInstanceUID", new_sop["SOPInstanceUID"]);
        insert(out, "MediaStorageSOPInstanceUID", new_sop["MediaStorageSOPInstanceUID"]);
    }
    return out;
}

metadata_map_t coalesce_metadata_for_basic_mr_image(const metadata_map_t &ref, meta_evolve e){
    metadata_map_t out;
    out["Modality"] = "MR";
    out["MediaStorageSOPClassUID"] = "1.2.840.10008.5.1.4.1.1.4"; // (non-enhanced) MR
    out["SOPClassUID"] = "1.2.840.10008.5.1.4.1.1.4";

    out.merge( coalesce_metadata_sop_common(ref) );
    out.merge( coalesce_metadata_patient(ref) );
    out.merge( coalesce_metadata_general_study(ref) );
    out.merge( coalesce_metadata_general_series(ref) );
    out.merge( coalesce_metadata_patient_study(ref) );
    out.merge( coalesce_metadata_frame_of_reference(ref) );
    out.merge( coalesce_metadata_general_equipment(ref) );
    out.merge( coalesce_metadata_general_image(ref) );
    out.merge( coalesce_metadata_image_plane(ref) );
    out.merge( coalesce_metadata_image_pixel(ref) );
    out.merge( coalesce_metadata_voi_lut(ref) );
    out.merge( coalesce_metadata_mr_image(ref) );
    out.merge( coalesce_metadata_misc(ref) );

    if(e == meta_evolve::iterate){
        // Assign a new SOP Instance UID.
        auto new_sop = coalesce_metadata_sop_common({});
        insert(out, "SOPInstanceUID", new_sop["SOPInstanceUID"]);
        insert(out, "MediaStorageSOPInstanceUID", new_sop["MediaStorageSOPInstanceUID"]);
    }
    return out;
}

metadata_map_t coalesce_metadata_for_basic_ct_image(const metadata_map_t &ref, meta_evolve e){
    metadata_map_t out;
    out["Modality"] = "CT";
    out["MediaStorageSOPClassUID"] = "1.2.840.10008.5.1.4.1.1.2"; // (non-enhanced) CT
    out["SOPClassUID"] = "1.2.840.10008.5.1.4.1.1.2";

    out.merge( coalesce_metadata_sop_common(ref) );
    out.merge( coalesce_metadata_patient(ref) );
    out.merge( coalesce_metadata_general_study(ref) );
    out.merge( coalesce_metadata_general_series(ref) );
    out.merge( coalesce_metadata_patient_study(ref) );
    out.merge( coalesce_metadata_frame_of_reference(ref) );
    out.merge( coalesce_metadata_general_equipment(ref) );
    out.merge( coalesce_metadata_general_image(ref) );
    out.merge( coalesce_metadata_image_plane(ref) );
    out.merge( coalesce_metadata_image_pixel(ref) );
    out.merge( coalesce_metadata_voi_lut(ref) );
    out.merge( coalesce_metadata_ct_image(ref) );
    out.merge( coalesce_metadata_misc(ref) );

    if(e == meta_evolve::iterate){
        // Assign a new SOP Instance UID.
        auto new_sop = coalesce_metadata_sop_common({});
        insert(out, "SOPInstanceUID", new_sop["SOPInstanceUID"]);
        insert(out, "MediaStorageSOPInstanceUID", new_sop["MediaStorageSOPInstanceUID"]);
    }
    return out;
}

metadata_map_t coalesce_metadata_for_basic_mesh(const metadata_map_t &ref, meta_evolve e){
    metadata_map_t out;
    out["Modality"] = "SEG";
    out["MediaStorageSOPClassUID"] = "1.2.840.10008.5.1.4.1.1.66.5"; // Surface Segmentation Storage
    out["SOPClassUID"] = "1.2.840.10008.5.1.4.1.1.66.5";

    out.merge( coalesce_metadata_sop_common(ref) );
    out.merge( coalesce_metadata_patient(ref) );
    out.merge( coalesce_metadata_general_study(ref) );
    out.merge( coalesce_metadata_general_series(ref) );
    out.merge( coalesce_metadata_patient_study(ref) );
    out.merge( coalesce_metadata_frame_of_reference(ref) );
    out.merge( coalesce_metadata_general_equipment(ref) );
    //out.merge( coalesce_metadata_enhanced_general_equipment(ref) );
    //out.merge( coalesce_metadata_surface_segmentation(ref) );
    //out.merge( coalesce_metadata_surface_mesh(ref) );
    out.merge( coalesce_metadata_misc(ref) );

    if(e == meta_evolve::iterate){
        // Assign a new SOP Instance UID.
        auto new_sop = coalesce_metadata_sop_common({});
        insert(out, "SOPInstanceUID", new_sop["SOPInstanceUID"]);
        insert(out, "MediaStorageSOPInstanceUID", new_sop["MediaStorageSOPInstanceUID"]);
    }
    return out;
}

metadata_map_t coalesce_metadata_for_basic_def_reg(const metadata_map_t &ref, meta_evolve e){
    metadata_map_t out;
    out["Modality"] = "REG";
    out["MediaStorageSOPClassUID"] = "1.2.840.10008.5.1.4.1.1.66.3"; // Deformable Spatial Registration Storage
    out["SOPClassUID"] = "1.2.840.10008.5.1.4.1.1.66.3";

    out.merge( coalesce_metadata_sop_common(ref) );
    out.merge( coalesce_metadata_patient(ref) );
    out.merge( coalesce_metadata_general_study(ref) );
    out.merge( coalesce_metadata_general_series(ref) );
    out.merge( coalesce_metadata_patient_study(ref) );
    out.merge( coalesce_metadata_frame_of_reference(ref) );
    out.merge( coalesce_metadata_general_equipment(ref) );
    //out.merge( coalesce_metadata_enhanced_general_equipment(ref) );
    //out.merge( coalesce_metadata_spatial_registration(ref) );
    //out.merge( coalesce_metadata_deformable_spatial_registration(ref) );
    out.merge( coalesce_metadata_misc(ref) );

    if(e == meta_evolve::iterate){
        // Assign a new SOP Instance UID.
        auto new_sop = coalesce_metadata_sop_common({});
        insert(out, "SOPInstanceUID", new_sop["SOPInstanceUID"]);
        insert(out, "MediaStorageSOPInstanceUID", new_sop["MediaStorageSOPInstanceUID"]);
    }
    return out;
}

metadata_map_t coalesce_metadata_for_basic_table(const metadata_map_t &ref, meta_evolve e){
    metadata_map_t out;
    out["Modality"] = "TAB";

    out.merge( coalesce_metadata_sop_common(ref) );
    //out.merge( coalesce_metadata_patient(ref) );
    out.merge( coalesce_metadata_general_study(ref) );
    out.merge( coalesce_metadata_general_series(ref) );
    //out.merge( coalesce_metadata_patient_study(ref) );
    //out.merge( coalesce_metadata_frame_of_reference(ref) );
    //out.merge( coalesce_metadata_general_equipment(ref) );
    out.merge( coalesce_metadata_misc(ref) );

    if(e == meta_evolve::iterate){
        // Assign a new SOP Instance UID.
        auto new_sop = coalesce_metadata_sop_common({});
        insert(out, "SOPInstanceUID", new_sop["SOPInstanceUID"]);
        insert(out, "MediaStorageSOPInstanceUID", new_sop["MediaStorageSOPInstanceUID"]);
    }
    return out;
}

