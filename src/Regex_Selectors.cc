//Regex_Selectors.cc - A part of DICOMautomaton 2018. Written by hal clark.

#include "Regex_Selectors.h"

#include <string>
#include <list>
#include <initializer_list>
#include <functional>
#include <regex>
#include <optional>
#include <utility>

#include "YgorString.h"
#include "YgorMath.h"

#include "Structs.h"

// ------------------------------------- Templates -------------------------------------

// Whitelist image arrays or point clouds using a limited vocabulary of specifiers.
// 
// Note: Positional specifiers (e.g., "first") act on the current whitelist. 
//       Beware when chaining filters!
template <class L> // L is a list of list::iterators of shared_ptr<Image_Array or Point_Cloud>.
L
Whitelist_Core( L lops,
           const std::string& Specifier,
           Regex_Selector_Opts Opts ){

    // Multiple key-value specifications stringified together.
    // For example, "key1@value1;key2@value2".
    do{
        const auto regex_split = Compile_Regex("^.*;.*$");
        if(!std::regex_match(Specifier, regex_split)) break; // Not a multi-key@value statement.

        auto v_kvs = SplitStringToVector(Specifier, ';', 'd');
        if(v_kvs.size() <= 1) throw std::logic_error("Unable to separate multiple key@value specifiers");

        for(auto & keyvalue : v_kvs){
            lops = Whitelist(lops, keyvalue, Opts);
        }
        return lops;
    }while(false);

    // A single key-value specifications stringified together.
    // For example, "key@value".
    do{
        const auto regex_split = Compile_Regex("^.*@.*$");
        if(!std::regex_match(Specifier, regex_split)) break; // Not a key@value statement.
        
        auto v_k_v = SplitStringToVector(Specifier, '@', 'd');
        if(v_k_v.size() <= 1) throw std::logic_error("Unable to separate key@value specifier");
        if(v_k_v.size() != 2) break; // Not a key@value statement (hint: maybe multiple @'s present?).

        lops = Whitelist(lops, v_k_v.front(), v_k_v.back(), Opts);
        return lops;
    }while(false);

    // Single-word positional specifiers, i.e. "all", "none", "first", "last", or zero-based 
    // numerical specifiers, e.g., "#0" (front), "#1" (second), "#-0" (last), and "#-1" (second-from-last).
    do{
        const auto regex_none  = Compile_Regex("^no?n?e?$");
        const auto regex_all   = Compile_Regex("^al?l?$");
        const auto regex_1st   = Compile_Regex("^fi?r?s?t?$");
        const auto regex_2nd   = Compile_Regex("^se?c?o?n?d?$");
        const auto regex_3rd   = Compile_Regex("^th?i?r?d?$");
        const auto regex_last  = Compile_Regex("^la?s?t?$");
        const auto regex_pnum  = Compile_Regex("^[#][0-9].*$");
        const auto regex_nnum  = Compile_Regex("^[#]-[0-9].*$");

        const auto regex_i_none  = Compile_Regex("^[!]no?n?e?$"); // Inverted variants of the above.
        const auto regex_i_all   = Compile_Regex("^[!]al?l?$");
        const auto regex_i_1st   = Compile_Regex("^[!]fi?r?s?t?$");
        const auto regex_i_2nd   = Compile_Regex("^[!]se?c?o?n?d?$");
        const auto regex_i_3rd   = Compile_Regex("^[!]th?i?r?d?$");
        const auto regex_i_last  = Compile_Regex("^[!]la?s?t?$");
        const auto regex_i_pnum  = Compile_Regex("^[!][#][0-9].*$");
        const auto regex_i_nnum  = Compile_Regex("^[!][#]-[0-9].*$");
        
        if(std::regex_match(Specifier, regex_i_none)){
            return lops;
        }
        if(std::regex_match(Specifier, regex_none)){
            lops.clear();
            return lops;
        }

        if(std::regex_match(Specifier, regex_i_all)){
            lops.clear();
            return lops;
        }
        if(std::regex_match(Specifier, regex_all)){
            return lops;
        }

        if( std::regex_match(Specifier, regex_i_1st)
        ||  std::regex_match(Specifier, regex_i_2nd) 
        ||  std::regex_match(Specifier, regex_i_3rd)  ){
            size_t N = 0; // Target.
            if( std::regex_match(Specifier, regex_i_1st) ){ N = 1;
            }else if( std::regex_match(Specifier, regex_i_2nd) ){ N = 2;
            }else if( std::regex_match(Specifier, regex_i_3rd) ){ N = 3;
            }else{
                throw std::logic_error("Regex positional specifier not understood. Cannot continue.");
            }

            decltype(lops) out;
            size_t i = 1;
            for(const auto &l : lops) if(N != i++) out.emplace_back(l);
            return out;
        }

        if( std::regex_match(Specifier, regex_1st)
        ||  std::regex_match(Specifier, regex_2nd) 
        ||  std::regex_match(Specifier, regex_3rd)  ){
            size_t N = 0; // Target.
            if( std::regex_match(Specifier, regex_1st) ){ N = 1;
            }else if( std::regex_match(Specifier, regex_2nd) ){ N = 2;
            }else if( std::regex_match(Specifier, regex_3rd) ){ N = 3;
            }else{
                throw std::logic_error("Regex positional specifier not understood. Cannot continue.");
            }

            decltype(lops) out;
            size_t i = 1;
            for(const auto &l : lops) if(N == i++) out.emplace_back(l);
            return out;
        }

        if(std::regex_match(Specifier, regex_i_last)){
            if(!lops.empty()) lops.pop_back();
            return lops;
        }
        if(std::regex_match(Specifier, regex_last)){
            decltype(lops) out;
            if(!lops.empty()) out.emplace_back(lops.back());
            return out;
        }

        if(std::regex_match(Specifier, regex_i_pnum)){
            auto pnum_extractor = std::regex("^[!][#]([0-9]*).*$", std::regex::icase |
                                                                   std::regex::optimize |
                                                                   std::regex::extended);
            auto N = std::stoul(GetFirstRegex(Specifier, pnum_extractor));

            if(N < lops.size()){
                auto l_it = std::next( lops.begin(), N );
                lops.erase( l_it );
            }
            return lops;
        }
        if(std::regex_match(Specifier, regex_pnum)){
            auto pnum_extractor = std::regex("^[#]([0-9]*).*$", std::regex::icase |
                                                                std::regex::optimize |
                                                                std::regex::extended);
            auto N = std::stoul(GetFirstRegex(Specifier, pnum_extractor));

            decltype(lops) out;
            if(N < lops.size()){
                auto l_it = std::next( lops.begin(), N );
                out.emplace_back(*l_it);
            }
            return out;
        }

        if(std::regex_match(Specifier, regex_i_nnum)){
            auto nnum_extractor = std::regex("^[!][#]-([0-9]*).*$", std::regex::icase |
                                                                    std::regex::optimize |
                                                                    std::regex::extended);
            auto N = std::stoul(GetFirstRegex(Specifier, nnum_extractor));

            if(N < lops.size()) return lops;

            // Note: this one is slightly harder than the rest because you cannot directly erase() a reverse iterator.
            decltype(lops) out;
            size_t i = lops.size();
            for(auto l_it = lops.begin(); l_it != lops.end(); ++l_it, --i){
                if(i == N) continue;
                out.emplace_back(*l_it);
            }
            return out;
        }
        if(std::regex_match(Specifier, regex_nnum)){
            auto nnum_extractor = std::regex("^[#]-([0-9]*).*$", std::regex::icase |
                                                                 std::regex::optimize |
                                                                 std::regex::extended);
            auto N = std::stoul(GetFirstRegex(Specifier, nnum_extractor));

            decltype(lops) out;
            if(N < lops.size()){
                auto l_it = std::next( lops.rbegin(), N );
                out.emplace_back(*l_it);
            }
            return out;
        }

    }while(false);

    throw std::invalid_argument("Selection is not valid. Cannot continue.");
    decltype(lops) out;
    return out;
}

// This is a convenience routine to combine multiple filtering passes into a single logical statement.
template <class L> // L is a list of list::iterators of shared_ptr<Image_Array or Point_Cloud>.
L
Whitelist_Core( L lops,
           std::initializer_list< std::pair<std::string, 
                                            std::string> > MetadataKeyValueRegex,
           Regex_Selector_Opts Opts ){

    for(const auto& kv_pair : MetadataKeyValueRegex){
        lops = Whitelist(lops, kv_pair.first, kv_pair.second, Opts);
    }
    return lops;
}


// --------------------------------------- Misc. ---------------------------------------

// Compile and return a regex using the application-wide default settings.
std::regex
Compile_Regex(const std::string& input){
    return std::regex(input, std::regex::icase | 
                             std::regex::nosubs |
                             std::regex::optimize |
                             std::regex::extended);
}

// Human-readable information about how selectors can be specified.
static
std::string
GenericSelectionInfo(const std::string &name_of_unit){
    return  " Selection specifiers can be of two types: positional or metadata-based key@value regex."_s
         +  " Positional specifiers can be 'first', 'last', 'none', or 'all' literals."_s
         +  " Additionally '#N' for some positive integer N selects the Nth "_s + name_of_unit 
         +  " (with zero-based indexing)."_s
         +  " Likewise, '#-N' selects the Nth-from-last "_s + name_of_unit + "."_s
         +  " Positional specifiers can be inverted by prefixing with a '!'."_s
         +  " Metadata-based key@value expressions are applied by matching the keys verbatim and the values with regex."_s
         +  " In order to invert metadata-based selectors, the regex logic must be inverted"_s
         +  " (i.e., you can *not* prefix metadata-based selectors with a '!')."_s
         +  " Multiple criteria can be specified by separating them with a ';' and are applied in the order specified."_s
         +  " Both positional and metadata-based criteria can be mixed together."_s
         +  " Note regexes are case insensitive and should use extended POSIX syntax."_s;
}

// ---------------------------------- Contours / ROIs ----------------------------------

// Stuff references to all contour collections into a list.
//
// Note: the output is meant to be filtered out using the selectors below.
std::list<std::reference_wrapper<contour_collection<double>>>
All_CCs( Drover DICOM_data ){
    std::list<std::reference_wrapper<contour_collection<double>>> cc_all;
    if(DICOM_data.contour_data != nullptr){
        for(auto & cc : DICOM_data.contour_data->ccs){
            if(cc.contours.empty()) continue;
            auto base_ptr = reinterpret_cast<contour_collection<double> *>(&cc);
            if(base_ptr == nullptr) continue;
            cc_all.push_back( std::ref(*base_ptr) );
        }
    }
    return cc_all;
}


// Whitelist contour collections using the provided regex.
std::list<std::reference_wrapper<contour_collection<double>>>
Whitelist( std::list<std::reference_wrapper<contour_collection<double>>> ccs,
           std::string MetadataKey,
           std::string MetadataValueRegex,
           Regex_Selector_Opts Opts ){

    auto theregex = Compile_Regex(std::move(MetadataValueRegex));

    ccs.remove_if([&](std::reference_wrapper<contour_collection<double>> cc) -> bool {
        if(cc.get().contours.empty()) return true; // Remove collections containing no contours.

        if(Opts.validation == Regex_Selector_Opts::Validation::Representative){
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
                if(Opts.nas == Regex_Selector_Opts::NAs::Include){
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

    for(const auto& kv_pair : MetadataKeyValueRegex){
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

    auto theregex = Compile_Regex(std::move(MetadataValueRegex));

    ias.remove_if([&](std::list<std::shared_ptr<Image_Array>>::iterator iap_it) -> bool {
        if((*iap_it) == nullptr) return true;
        if((*iap_it)->imagecoll.images.empty()) return true; // Remove arrays containing no images.

        if(Opts.validation == Regex_Selector_Opts::Validation::Representative){
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
                if(Opts.nas == Regex_Selector_Opts::NAs::Include){
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
std::list<std::list<std::shared_ptr<Image_Array>>::iterator>
Whitelist( std::list<std::list<std::shared_ptr<Image_Array>>::iterator> ias,
           std::string Specifier,
           Regex_Selector_Opts Opts ){

    return Whitelist_Core( std::move(ias), std::move(Specifier), Opts );
}


// This is a convenience routine to combine multiple filtering passes into a single logical statement.
std::list<std::list<std::shared_ptr<Image_Array>>::iterator>
Whitelist( std::list<std::list<std::shared_ptr<Image_Array>>::iterator> ias,
           std::initializer_list< std::pair<std::string, 
                                            std::string> > MetadataKeyValueRegex,
           Regex_Selector_Opts Opts ){

    return Whitelist_Core( std::move(ias), MetadataKeyValueRegex, Opts );
}


// Utility function documenting the image array whitelist routines for operations.
OperationArgDoc IAWhitelistOpArgDoc(){
    OperationArgDoc out;

    out.name = "ImageSelection";
    out.desc = "Select one or more image arrays."_s
               + " Note that image arrays can hold anything, but will typically represent a single contiguous"_s
               + " 3D volume (i.e., a volumetric CT scan) or '4D' time-series."_s
               + " Be aware that it is possible to mix logically unrelated images together."_s
               + GenericSelectionInfo("image array");
    out.default_val = "all";
    out.expected = true;
    out.examples = { "last", "first", "all", "none", 
                     "#0", "#-0",
                     "!last", "!#-3",
                     "key@.*value.*", "key1@.*value1.*;key2@^value2$;first" };

    return out;
}

// ----------------------------------- Point Clouds ------------------------------------

// Provide pointers for all point clouds into a list.
//
// Note: The output is meant to be filtered using the selectors below.
std::list<std::list<std::shared_ptr<Point_Cloud>>::iterator>
All_PCs( Drover &DICOM_data ){
    std::list<std::list<std::shared_ptr<Point_Cloud>>::iterator> pc_all;

    for(auto pcp_it = DICOM_data.point_data.begin(); pcp_it != DICOM_data.point_data.end(); ++pcp_it){
        if((*pcp_it) == nullptr) continue;
        pc_all.push_back(pcp_it);
    }
    return pc_all;
}


// Whitelist point clouds using the provided regex.
std::list<std::list<std::shared_ptr<Point_Cloud>>::iterator>
Whitelist( std::list<std::list<std::shared_ptr<Point_Cloud>>::iterator> pcs,
           std::string MetadataKey,
           std::string MetadataValueRegex,
           Regex_Selector_Opts Opts ){

    auto theregex = Compile_Regex(std::move(MetadataValueRegex));

    pcs.remove_if([&](std::list<std::shared_ptr<Point_Cloud>>::iterator pcp_it) -> bool {
        if((*pcp_it) == nullptr) return true;
        if((*pcp_it)->pset.points.empty()) return true; // Remove arrays containing no images.

        if( // Note: Point_Clouds are dissimilar to Image_Arrays in that individual images can have different
                  //       metadata, but point clouds cannot. We keep these options for consistency.
                  (Opts.validation == Regex_Selector_Opts::Validation::Representative)
              ||  (Opts.validation == Regex_Selector_Opts::Validation::Pedantic)        ){

            auto ValueOpt = (*pcp_it)->pset.GetMetadataValueAs<std::string>(MetadataKey);
            if(ValueOpt){
                return !(std::regex_match(ValueOpt.value(),theregex));
            }else if(Opts.nas == Regex_Selector_Opts::NAs::Include){
                return false;
            }else if(Opts.nas == Regex_Selector_Opts::NAs::Exclude){
                return true;
            }else if(Opts.nas == Regex_Selector_Opts::NAs::TreatAsEmpty){
                return !(std::regex_match("",theregex));
            }
            throw std::logic_error("NAs option not understood. Cannot continue.");
        }
        throw std::logic_error("Regex selector option not understood. Cannot continue.");
        return true; // Should never get here.
    });
    return pcs;
}


// Whitelist point clouds using a limited vocabulary of specifiers.
//
// Note: this routine shares the generic Image_Arrays implementation above.
std::list<std::list<std::shared_ptr<Point_Cloud>>::iterator>
Whitelist( std::list<std::list<std::shared_ptr<Point_Cloud>>::iterator> pcs,
           std::string Specifier,
           Regex_Selector_Opts Opts ){

    return Whitelist_Core( std::move(pcs), std::move(Specifier), Opts );
}    


// This is a convenience routine to combine multiple filtering passes into a single logical statement.
//
// Note: this routine shares the generic Image_Arrays implementation above.
std::list<std::list<std::shared_ptr<Point_Cloud>>::iterator>
Whitelist( std::list<std::list<std::shared_ptr<Point_Cloud>>::iterator> pcs,
           std::initializer_list< std::pair<std::string, 
                                            std::string> > MetadataKeyValueRegex,
           Regex_Selector_Opts Opts ){

    return Whitelist_Core( std::move(pcs), MetadataKeyValueRegex, Opts );
}


// Utility function documenting the point cloud whitelist routines for operations.
OperationArgDoc PCWhitelistOpArgDoc(){
    OperationArgDoc out;

    out.name = "PointSelection";
    out.desc = "Select one or more point clouds."_s
               + " Note that point clouds can hold a variety of data with varying attributes,"_s
               + " but each point cloud is meant to represent a single logically cohesive collection of points."_s
               + " Be aware that it is possible to mix logically unrelated points together."_s
               + GenericSelectionInfo("point cloud");
    out.default_val = "all";
    out.expected = true;
    out.examples = { "last", "first", "all", "none", 
                     "#0", "#-0",
                     "!last", "!#-3",
                     "key@.*value.*", "key1@.*value1.*;key2@^value2$;first" };

    return out;
}

// ----------------------------------- Surface Meshes ------------------------------------

// Provide pointers for all surface meshes into a list.
//
// Note: The output is meant to be filtered using the selectors below.
std::list<std::list<std::shared_ptr<Surface_Mesh>>::iterator>
All_SMs( Drover &DICOM_data ){
    std::list<std::list<std::shared_ptr<Surface_Mesh>>::iterator> sm_all;

    for(auto smp_it = DICOM_data.smesh_data.begin(); smp_it != DICOM_data.smesh_data.end(); ++smp_it){
        if((*smp_it) == nullptr) continue;
        sm_all.push_back(smp_it);
    }
    return sm_all;
}


// Whitelist surface meshes using the provided regex.
std::list<std::list<std::shared_ptr<Surface_Mesh>>::iterator>
Whitelist( std::list<std::list<std::shared_ptr<Surface_Mesh>>::iterator> sms,
           std::string MetadataKey,
           std::string MetadataValueRegex,
           Regex_Selector_Opts Opts ){

    auto theregex = Compile_Regex(std::move(MetadataValueRegex));

    sms.remove_if([&](std::list<std::shared_ptr<Surface_Mesh>>::iterator smp_it) -> bool {
        if((*smp_it) == nullptr) return true;
        if((*smp_it)->meshes.vertices.empty()) return true; // Remove meshes containing no vertices.
        if((*smp_it)->meshes.faces.empty()) return true; // Remove meshes containing no faces.

        if( // Note: A single Surface_Mesh corresponds to one individual metadata store. While a single
                  //       Surface_Mesh can be comprised of multiple disconnected meshes, they are herein considered
                  //       to be part of the same logical group. As for Point_Clouds, we keep the following options for
                  //       consistency.
                  (Opts.validation == Regex_Selector_Opts::Validation::Representative)
              ||  (Opts.validation == Regex_Selector_Opts::Validation::Pedantic)        ){

            std::optional<std::string> ValueOpt 
                    = ( (*smp_it)->meshes.metadata.count(MetadataKey) != 0 ) ?
                      (*smp_it)->meshes.metadata[MetadataKey] :
                      std::optional<std::string>();
            if(ValueOpt){
                return !(std::regex_match(ValueOpt.value(),theregex));
            }else if(Opts.nas == Regex_Selector_Opts::NAs::Include){
                return false;
            }else if(Opts.nas == Regex_Selector_Opts::NAs::Exclude){
                return true;
            }else if(Opts.nas == Regex_Selector_Opts::NAs::TreatAsEmpty){
                return !(std::regex_match("",theregex));
            }
            throw std::logic_error("NAs option not understood. Cannot continue.");
        }
        throw std::logic_error("Regex selector option not understood. Cannot continue.");
        return true; // Should never get here.
    });
    return sms;
}


// Whitelist surface meshes using a limited vocabulary of specifiers.
//
// Note: this routine shares the generic Image_Arrays and Point_Clouds implementation above.
std::list<std::list<std::shared_ptr<Surface_Mesh>>::iterator>
Whitelist( std::list<std::list<std::shared_ptr<Surface_Mesh>>::iterator> sms,
           std::string Specifier,
           Regex_Selector_Opts Opts ){

    return Whitelist_Core( std::move(sms), std::move(Specifier), Opts );
}    


// This is a convenience routine to combine multiple filtering passes into a single logical statement.
//
// Note: this routine shares the generic Image_Arrays and Point_Clouds implementation above.
std::list<std::list<std::shared_ptr<Surface_Mesh>>::iterator>
Whitelist( std::list<std::list<std::shared_ptr<Surface_Mesh>>::iterator> sms,
           std::initializer_list< std::pair<std::string, 
                                            std::string> > MetadataKeyValueRegex,
           Regex_Selector_Opts Opts ){

    return Whitelist_Core( std::move(sms), MetadataKeyValueRegex, Opts );
}


// Utility function documenting the surface mesh whitelist routines for operations.
OperationArgDoc SMWhitelistOpArgDoc(){
    OperationArgDoc out;

    out.name = "MeshSelection";
    out.desc = "Select one or more surface meshes."_s
               + " Note that a single surface mesh may hold many disconnected mesh components;"_s
               + " they should collectively represent a single logically cohesive object."_s
               + " Be aware that it is possible to mix logically unrelated sub-meshes together in a single mesh."_s
               + GenericSelectionInfo("surface mesh");
    out.default_val = "all";
    out.expected = true;
    out.examples = { "last", "first", "all", "none", 
                     "#0", "#-0",
                     "!last", "!#-3",
                     "key@.*value.*", "key1@.*value1.*;key2@^value2$;first" };

    return out;
}

// ------------------------------------ TPlan_Config -------------------------------------

// Provide pointers for all treatment plans into a list.
//
// Note: The output is meant to be filtered using the selectors below.
std::list<std::list<std::shared_ptr<TPlan_Config>>::iterator>
All_TPs( Drover &DICOM_data ){
    std::list<std::list<std::shared_ptr<TPlan_Config>>::iterator> tp_all;

    for(auto tpp_it = DICOM_data.tplan_data.begin(); tpp_it != DICOM_data.tplan_data.end(); ++tpp_it){
        if((*tpp_it) == nullptr) continue;
        tp_all.push_back(tpp_it);
    }
    return tp_all;
}


// Whitelist treatment plans using the provided regex.
std::list<std::list<std::shared_ptr<TPlan_Config>>::iterator>
Whitelist( std::list<std::list<std::shared_ptr<TPlan_Config>>::iterator> tps,
           std::string MetadataKey,
           std::string MetadataValueRegex,
           Regex_Selector_Opts Opts ){

    auto theregex = Compile_Regex(std::move(MetadataValueRegex));

    tps.remove_if([&](std::list<std::shared_ptr<TPlan_Config>>::iterator tpp_it) -> bool {
        if((*tpp_it) == nullptr) return true;
        if((*tpp_it)->dynamic_states.empty()) return true; // Remove plans containing no beams.

        if( // Note: A TPlan_Config corresponds to one individual metadata store. While a single
                  //       TPlan_Config can be comprised of multiple disconnected beams, they are 
                  //       herein considered to be part of the same logical group.
                  (Opts.validation == Regex_Selector_Opts::Validation::Representative)
              ||  (Opts.validation == Regex_Selector_Opts::Validation::Pedantic)        ){

            std::optional<std::string> ValueOpt 
                    = ( (*tpp_it)->metadata.count(MetadataKey) != 0 ) ?
                      (*tpp_it)->metadata[MetadataKey] :
                      std::optional<std::string>();

            // TODO: support selection of Dynamic_Machine_State and Static_Machine_State metadata too.

            if(ValueOpt){
                return !(std::regex_match(ValueOpt.value(),theregex));
            }else if(Opts.nas == Regex_Selector_Opts::NAs::Include){
                return false;
            }else if(Opts.nas == Regex_Selector_Opts::NAs::Exclude){
                return true;
            }else if(Opts.nas == Regex_Selector_Opts::NAs::TreatAsEmpty){
                return !(std::regex_match("",theregex));
            }
            throw std::logic_error("NAs option not understood. Cannot continue.");
        }
        throw std::logic_error("Regex selector option not understood. Cannot continue.");
        return true; // Should never get here.
    });
    return tps;
}


// Whitelist treatment plans using a limited vocabulary of specifiers.
//
// Note: this routine shares the generic Image_Arrays, Point_Clouds, and Surface_Mesh implementation above.
std::list<std::list<std::shared_ptr<TPlan_Config>>::iterator>
Whitelist( std::list<std::list<std::shared_ptr<TPlan_Config>>::iterator> tps,
           std::string Specifier,
           Regex_Selector_Opts Opts ){

    return Whitelist_Core( std::move(tps), std::move(Specifier), Opts );
}    


// This is a convenience routine to combine multiple filtering passes into a single logical statement.
//
// Note: this routine shares the generic Image_Arrays, Point_Clouds, and Surface_Mesh implementation above.
std::list<std::list<std::shared_ptr<TPlan_Config>>::iterator>
Whitelist( std::list<std::list<std::shared_ptr<TPlan_Config>>::iterator> tps,
           std::initializer_list< std::pair<std::string, 
                                            std::string> > MetadataKeyValueRegex,
           Regex_Selector_Opts Opts ){

    return Whitelist_Core( std::move(tps), MetadataKeyValueRegex, Opts );
}


// Utility function documenting the treatment plan whitelist routines for operations.
OperationArgDoc TPWhitelistOpArgDoc(){
    OperationArgDoc out;

    out.name = "TPlanSelection";
    out.desc = "Select one or more treatment plans."_s
               + " Note that a single treatment plan may be composed of multiple beams;"_s
               + " if delivered sequentially, they should collectively represent a single logically cohesive plan."_s
               + GenericSelectionInfo("treatment plan");
    out.default_val = "all";
    out.expected = true;
    out.examples = { "last", "first", "all", "none", 
                     "#0", "#-0",
                     "!last", "!#-3",
                     "key@.*value.*", "key1@.*value1.*;key2@^value2$;first" };

    return out;
}

// ----------------------------------- Line Samples ------------------------------------

// Provide pointers for all line samples into a list.
//
// Note: The output is meant to be filtered using the selectors below.
std::list<std::list<std::shared_ptr<Line_Sample>>::iterator>
All_LSs( Drover &DICOM_data ){
    std::list<std::list<std::shared_ptr<Line_Sample>>::iterator> ls_all;

    for(auto lsp_it = DICOM_data.lsamp_data.begin(); lsp_it != DICOM_data.lsamp_data.end(); ++lsp_it){
        if((*lsp_it) == nullptr) continue;
        ls_all.push_back(lsp_it);
    }
    return ls_all;
}


// Whitelist line samples using the provided regex.
std::list<std::list<std::shared_ptr<Line_Sample>>::iterator>
Whitelist( std::list<std::list<std::shared_ptr<Line_Sample>>::iterator> lss,
           std::string MetadataKey,
           std::string MetadataValueRegex,
           Regex_Selector_Opts Opts ){

    auto theregex = Compile_Regex(std::move(MetadataValueRegex));

    lss.remove_if([&](std::list<std::shared_ptr<Line_Sample>>::iterator lsp_it) -> bool {
        if((*lsp_it) == nullptr) return true;
        if((*lsp_it)->line.samples.empty()) return true; // Remove arrays containing no samples.

        if( // Note: Line_Samples are dissimilar to Image_Arrays in that individual images can have different
                  //       metadata, but point clouds cannot. We keep these options for consistency.
                  (Opts.validation == Regex_Selector_Opts::Validation::Representative)
              ||  (Opts.validation == Regex_Selector_Opts::Validation::Pedantic)        ){

            std::optional<std::string> ValueOpt 
                    = ( (*lsp_it)->line.metadata.count(MetadataKey) != 0 ) ?
                      (*lsp_it)->line.metadata[MetadataKey] :
                      std::optional<std::string>();
            if(ValueOpt){
                return !(std::regex_match(ValueOpt.value(),theregex));
            }else if(Opts.nas == Regex_Selector_Opts::NAs::Include){
                return false;
            }else if(Opts.nas == Regex_Selector_Opts::NAs::Exclude){
                return true;
            }else if(Opts.nas == Regex_Selector_Opts::NAs::TreatAsEmpty){
                return !(std::regex_match("",theregex));
            }
            throw std::logic_error("NAs option not understood. Cannot continue.");
        }
        throw std::logic_error("Regex selector option not understood. Cannot continue.");
        return true; // Should never get here.
    });
    return lss;
}


// Whitelist line samples using a limited vocabulary of specifiers.
//
// Note: this routine shares the generic Image_Arrays implementation above.
std::list<std::list<std::shared_ptr<Line_Sample>>::iterator>
Whitelist( std::list<std::list<std::shared_ptr<Line_Sample>>::iterator> lss,
           std::string Specifier,
           Regex_Selector_Opts Opts ){

    return Whitelist_Core( std::move(lss), std::move(Specifier), Opts );
}    


// This is a convenience routine to combine multiple filtering passes into a single logical statement.
//
// Note: this routine shares the generic Image_Arrays implementation above.
std::list<std::list<std::shared_ptr<Line_Sample>>::iterator>
Whitelist( std::list<std::list<std::shared_ptr<Line_Sample>>::iterator> lss,
           std::initializer_list< std::pair<std::string, 
                                            std::string> > MetadataKeyValueRegex,
           Regex_Selector_Opts Opts ){

    return Whitelist_Core( std::move(lss), MetadataKeyValueRegex, Opts );
}


// Utility function documenting the point cloud whitelist routines for operations.
OperationArgDoc LSWhitelistOpArgDoc(){
    OperationArgDoc out;

    out.name = "LSampSelection";
    out.desc = "Select one or more line samples."_s
               + GenericSelectionInfo("line sample");
    out.default_val = "all";
    out.expected = true;
    out.examples = { "last", "first", "all", "none", 
                     "#0", "#-0",
                     "!last", "!#-3",
                     "key@.*value.*", "key1@.*value1.*;key2@^value2$;first" };

    return out;
}

// ------------------------------------ Transform3 -------------------------------------

// Provide pointers for all transforms into a list.
//
// Note: The output is meant to be filtered using the selectors below.
std::list<std::list<std::shared_ptr<Transform3>>::iterator>
All_T3s( Drover &DICOM_data ){
    std::list<std::list<std::shared_ptr<Transform3>>::iterator> t3_all;

    for(auto t3p_it = DICOM_data.trans_data.begin(); t3p_it != DICOM_data.trans_data.end(); ++t3p_it){
        if((*t3p_it) == nullptr) continue;
        t3_all.push_back(t3p_it);
    }
    return t3_all;
}


// Whitelist transforms using the provided regex.
std::list<std::list<std::shared_ptr<Transform3>>::iterator>
Whitelist( std::list<std::list<std::shared_ptr<Transform3>>::iterator> t3s,
           std::string MetadataKey,
           std::string MetadataValueRegex,
           Regex_Selector_Opts Opts ){

    auto theregex = Compile_Regex(std::move(MetadataValueRegex));

    t3s.remove_if([&](std::list<std::shared_ptr<Transform3>>::iterator t3p_it) -> bool {
        if((*t3p_it) == nullptr) return true;
        if(std::holds_alternative<std::monostate>( (*t3p_it)->transform )) return true; // Remove empty transforms.

        if( (Opts.validation == Regex_Selector_Opts::Validation::Representative)
              ||  (Opts.validation == Regex_Selector_Opts::Validation::Pedantic)        ){

            std::optional<std::string> ValueOpt 
                    = ( (*t3p_it)->metadata.count(MetadataKey) != 0 ) ?
                      (*t3p_it)->metadata[MetadataKey] :
                      std::optional<std::string>();
            if(ValueOpt){
                return !(std::regex_match(ValueOpt.value(),theregex));
            }else if(Opts.nas == Regex_Selector_Opts::NAs::Include){
                return false;
            }else if(Opts.nas == Regex_Selector_Opts::NAs::Exclude){
                return true;
            }else if(Opts.nas == Regex_Selector_Opts::NAs::TreatAsEmpty){
                return !(std::regex_match("",theregex));
            }
            throw std::logic_error("NAs option not understood. Cannot continue.");
        }
        throw std::logic_error("Regex selector option not understood. Cannot continue.");
        return true; // Should never get here.
    });
    return t3s;
}


// Whitelist transforms using a limited vocabulary of specifiers.
//
// Note: this routine shares the generic Image_Arrays implementation above.
std::list<std::list<std::shared_ptr<Transform3>>::iterator>
Whitelist( std::list<std::list<std::shared_ptr<Transform3>>::iterator> t3s,
           std::string Specifier,
           Regex_Selector_Opts Opts ){

    return Whitelist_Core( std::move(t3s), std::move(Specifier), Opts );
}    


// This is a convenience routine to combine multiple filtering passes into a single logical statement.
//
// Note: this routine shares the generic Image_Arrays implementation above.
std::list<std::list<std::shared_ptr<Transform3>>::iterator>
Whitelist( std::list<std::list<std::shared_ptr<Transform3>>::iterator> t3s,
           std::initializer_list< std::pair<std::string, 
                                            std::string> > MetadataKeyValueRegex,
           Regex_Selector_Opts Opts ){

    return Whitelist_Core( std::move(t3s), MetadataKeyValueRegex, Opts );
}


// Utility function documenting the transform whitelist routines for operations.
OperationArgDoc T3WhitelistOpArgDoc(){
    OperationArgDoc out;

    out.name = "TransformSelection";
    out.desc = "Select one or more transforms."_s
               + GenericSelectionInfo("transformation");
    out.default_val = "all";
    out.expected = true;
    out.examples = { "last", "first", "all", "none", 
                     "#0", "#-0",
                     "!last", "!#-3",
                     "key@.*value.*", "key1@.*value1.*;key2@^value2$;first" };

    return out;
}


