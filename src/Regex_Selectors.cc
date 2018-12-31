//Regex_Selectors.cc - A part of DICOMautomaton 2018. Written by hal clark.

#include "Regex_Selectors.h"

#include <string>
#include <list>
#include <initializer_list>
#include <functional>
#include <regex>
#include <experimental/optional>

#include "YgorString.h"
#include "YgorMath.h"

#include "Structs.h"


// --------------------------------------- Misc. ---------------------------------------

// Compile and return a regex using the application-wide default settings.
std::regex
Compile_Regex(std::string input){
    return std::regex(input, std::regex::icase | 
                             std::regex::nosubs |
                             std::regex::optimize |
                             std::regex::extended);
}


// ---------------------------------- Contours / ROIs ----------------------------------

// Stuff references to all contour collections into a list.
//
// Note: the output is meant to be filtered out using the selectors below.
std::list<std::reference_wrapper<contour_collection<double>>>
All_CCs( Drover DICOM_data ){
    std::list<std::reference_wrapper<contour_collection<double>>> cc_all;
    for(auto & cc : DICOM_data.contour_data->ccs){
        if(cc.contours.empty()) continue;
        auto base_ptr = reinterpret_cast<contour_collection<double> *>(&cc);
        if(base_ptr == nullptr) continue;
        cc_all.push_back( std::ref(*base_ptr) );
    }
    return cc_all;
}


// Whitelist contour collections using the provided regex.
std::list<std::reference_wrapper<contour_collection<double>>>
Whitelist( std::list<std::reference_wrapper<contour_collection<double>>> ccs,
           std::string MetadataKey,
           std::string MetadataValueRegex,
           Regex_Selector_Opts Opts ){

    auto theregex = Compile_Regex(MetadataValueRegex);

    ccs.remove_if([&](std::reference_wrapper<contour_collection<double>> cc) -> bool {
        if(cc.get().contours.empty()) return true; // Remove collections containing no contours.

        if(false){
        }else if(Opts.validation == Regex_Selector_Opts::Validation::Representative){
            auto ValueOpt = cc.get().contours.front().GetMetadataValueAs<std::string>(MetadataKey);
            if(ValueOpt){
                return !(std::regex_match(ValueOpt.value(),theregex));
            }else if(Opts.nas == Regex_Selector_Opts::NAs::Include){
                return false;
            }else if(Opts.nas == Regex_Selector_Opts::NAs::Exclude){
                return true;
            }else if(Opts.nas == Regex_Selector_Opts::NAs::TreatAsEmpty){
                return !(std::regex_match("",theregex));
            }
            throw std::logic_error("Regex selector representative->NAs option not understood. Cannot continue.");

        }else if(Opts.validation == Regex_Selector_Opts::Validation::Pedantic){
            const auto Values = cc.get().get_unique_values_for_key(MetadataKey);

            if(Values.empty()){
                if(false){
                }else if(Opts.nas == Regex_Selector_Opts::NAs::Include){
                    return false;
                }else if(Opts.nas == Regex_Selector_Opts::NAs::Exclude){
                    return true;
                }
                throw std::logic_error("Regex selector pedantic->NAs option not understood. Cannot continue.");

            }else{
                for(const auto & Value : Values){
                    if( !std::regex_match(Value,theregex) ) return true;
                }
                return false;
            }
            throw std::logic_error("Regex selector pedantic option not understood. Cannot continue.");

        }
        throw std::logic_error("Regex selector option not understood. Cannot continue.");
        return true; // Should never get here.
    });
    return ccs;
}

// This is a convenience routine to combine multiple filtering passes into a single logical statement.
std::list<std::reference_wrapper<contour_collection<double>>>
Whitelist( std::list<std::reference_wrapper<contour_collection<double>>> ccs,
           std::initializer_list< std::pair<std::string, 
                                            std::string> > MetadataKeyValueRegex,
           Regex_Selector_Opts Opts ){

    for(auto kv_pair : MetadataKeyValueRegex){
        ccs = Whitelist(ccs, kv_pair.first, kv_pair.second, Opts);
    }
    return ccs;
}


// ----------------------------------- Image Arrays ------------------------------------

// Provide iterators for all image arrays into a list.
//
// Note: The output is meant to be filtered out using the selectors below.
//
// Note: List iterators are provided because it is common to need to shuffle image ordering around.
//       The need appears to be less common for contours, so the interface is slightly different
//       compared to the contour whitelist interface.
std::list<std::list<std::shared_ptr<Image_Array>>::iterator>
All_IAs( Drover &DICOM_data ){
    std::list<std::list<std::shared_ptr<Image_Array>>::iterator> ia_all;

    for(auto iap_it = DICOM_data.image_data.begin(); iap_it != DICOM_data.image_data.end(); ++iap_it){
        if((*iap_it) == nullptr) continue;
        ia_all.push_back(iap_it);
    }
    return ia_all;
}

// Whitelist image arrays using the provided regex.
std::list<std::list<std::shared_ptr<Image_Array>>::iterator>
Whitelist( std::list<std::list<std::shared_ptr<Image_Array>>::iterator> ias,
           std::string MetadataKey,
           std::string MetadataValueRegex,
           Regex_Selector_Opts Opts ){

    auto theregex = Compile_Regex(MetadataValueRegex);

    ias.remove_if([&](std::list<std::shared_ptr<Image_Array>>::iterator iap_it) -> bool {
        if((*iap_it) == nullptr) return true;
        if((*iap_it)->imagecoll.images.empty()) return true; // Remove arrays containing no images.

        if(false){
        }else if(Opts.validation == Regex_Selector_Opts::Validation::Representative){
            auto ValueOpt = (*iap_it)->imagecoll.images.front().GetMetadataValueAs<std::string>(MetadataKey);
            if(ValueOpt){
                return !(std::regex_match(ValueOpt.value(),theregex));
            }else if(Opts.nas == Regex_Selector_Opts::NAs::Include){
                return false;
            }else if(Opts.nas == Regex_Selector_Opts::NAs::Exclude){
                return true;
            }else if(Opts.nas == Regex_Selector_Opts::NAs::TreatAsEmpty){
                return !(std::regex_match("",theregex));
            }
            throw std::logic_error("Regex selector representative->NAs option not understood. Cannot continue.");

        }else if(Opts.validation == Regex_Selector_Opts::Validation::Pedantic){
            const auto Values = (*iap_it)->imagecoll.get_unique_values_for_key(MetadataKey);

            if(Values.empty()){
                if(false){
                }else if(Opts.nas == Regex_Selector_Opts::NAs::Include){
                    return false;
                }else if(Opts.nas == Regex_Selector_Opts::NAs::Exclude){
                    return true;
                }
                throw std::logic_error("Regex selector pedantic->NAs option not understood. Cannot continue.");

            }else{
                for(const auto & Value : Values){
                    if( !std::regex_match(Value,theregex) ) return true;
                }
                return false;
            }
            throw std::logic_error("Regex selector pedantic option not understood. Cannot continue.");

        }
        throw std::logic_error("Regex selector option not understood. Cannot continue.");
        return true; // Should never get here.
    });
    return ias;
}

// Whitelist image arrays using a limited vocabulary of specifiers.
// 
// Note: Positional specifiers (e.g., "first") act on the current whitelist. 
//       Beware when chaining filters!
std::list<std::list<std::shared_ptr<Image_Array>>::iterator>
Whitelist( std::list<std::list<std::shared_ptr<Image_Array>>::iterator> ias,
           std::string Specifier,
           Regex_Selector_Opts Opts ){

    // Multiple key-value specifications stringified together.
    // For example, "key1@value1;key2@value2".
    do{
        const auto regex_split = Compile_Regex("^.*;.*$");
        if(!std::regex_match(Specifier, regex_split)) break; // Not a multi-key@value statement.

        auto v_kvs = SplitStringToVector(Specifier, ';', 'd');
        if(v_kvs.size() <= 1) throw std::logic_error("Unable to separate multiple key@value specifiers");

        for(auto & keyvalue : v_kvs){
            ias = Whitelist(ias, keyvalue, Opts);
        }
        return ias;
    }while(false);

    // A single key-value specifications stringified together.
    // For example, "key@value".
    do{
        const auto regex_split = Compile_Regex("^.*@.*$");
        if(!std::regex_match(Specifier, regex_split)) break; // Not a key@value statement.
        
        auto v_k_v = SplitStringToVector(Specifier, '@', 'd');
        if(v_k_v.size() <= 1) throw std::logic_error("Unable to separate key@value specifier");
        if(v_k_v.size() != 2) break; // Not a key@value statement (hint: maybe multiple @'s present?).

        ias = Whitelist(ias, v_k_v.front(), v_k_v.back(), Opts);
        return ias;
    }while(false);

    // Single-word positional specifiers, i.e. "all", "none", "first", "last", or zero-based 
    // numerical specifiers, e.g., "#0" (front), "#1" (second), "#-0" (last), and "#-1" (second-from-last).
    do{
        const auto regex_none  = Compile_Regex("^no?n?e?$");
        const auto regex_first = Compile_Regex("^fi?r?s?t?$");
        const auto regex_last  = Compile_Regex("^la?s?t?$");
        const auto regex_all   = Compile_Regex("^al?l?$");
        const auto regex_pnum  = Compile_Regex("^#[0-9].*$");
        const auto regex_nnum  = Compile_Regex("^#-[0-9].*$");
        
        if(std::regex_match(Specifier, regex_none)){
            ias.clear();
            return ias;
        }
        if(std::regex_match(Specifier, regex_all)){
            return ias;
        }
        if(std::regex_match(Specifier, regex_first)){
            decltype(ias) out;
            if(!ias.empty()) out.emplace_back(ias.front());
            return out;
        }
        if(std::regex_match(Specifier, regex_last)){
            decltype(ias) out;
            if(!ias.empty()) out.emplace_back(ias.back());
            return out;
        }
        if(std::regex_match(Specifier, regex_pnum)){
            auto pnum_extractor = Compile_Regex("^#([0-9]*).*$");
            auto N = std::stoul(GetFirstRegex(Specifier, pnum_extractor));

            decltype(ias) out;
            if(N < ias.size()){
                auto l_it = std::next( ias.begin(), N );
                out.emplace_back(*l_it);
            }
            return out;
        }
        if(std::regex_match(Specifier, regex_nnum)){
            auto nnum_extractor = Compile_Regex("^#-([0-9]*).*$");
            auto N = std::stoul(GetFirstRegex(Specifier, nnum_extractor));

            decltype(ias) out;
            if(N < ias.size()){
                auto l_it = std::next( ias.rbegin(), N );
                out.emplace_back(*l_it);
            }
            return out;
        }
    }while(false);

    throw std::invalid_argument("Image selection is not valid. Cannot continue.");
    decltype(ias) out;
    return out;
}


// This is a convenience routine to combine multiple filtering passes into a single logical statement.
std::list<std::list<std::shared_ptr<Image_Array>>::iterator>
Whitelist( std::list<std::list<std::shared_ptr<Image_Array>>::iterator> ias,
           std::initializer_list< std::pair<std::string, 
                                            std::string> > MetadataKeyValueRegex,
           Regex_Selector_Opts Opts ){

    for(auto kv_pair : MetadataKeyValueRegex){
        ias = Whitelist(ias, kv_pair.first, kv_pair.second, Opts);
    }
    return ias;
}

// Utility function documenting the image array whitelist routines for operations.
OperationArgDoc IAWhitelistOpArgDoc(void){
    OperationArgDoc out;

    out.name = "ImageSelection";
    out.desc = "Select image arrays to operate on."
               " Specifiers can be of two types: positional or metadata-based key@value regex."
               " Positional specifiers can be 'first', 'last', 'none', or 'all' literals."
               " Additionally '#N' for some positive integer N selects the Nth image array (with zero-based indexing)."
               " Likewise, '#-N' selects the Nth-from-last image array."
               " Metadata-based key@value expressions are applied by matching the keys verbatim and the values with regex."
               " Multiple criteria can be specified by separating them with a ';' and are applied in the order specified."
               " Both positional and metadata-based criteria can be mixed together."
               " Note that image arrays can hold anything, but will typically represent a single contiguous"
               " 3D volume (i.e., a volumetric CT scan) or '4D' time-series."
               " Note regexes are case insensitive and should use extended POSIX syntax.";
    out.default_val = "all";
    out.expected = true;
    out.examples = { "last", "first", "all", "none", "#0", "#-0",
                     "key@.*value.*", "key1@.*value1.*;key2@^value2$;first" };

    return out;
}

