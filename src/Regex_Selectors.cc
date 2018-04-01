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
            }
            throw std::logic_error("Regex selector option not understood. Cannot continue.");

        }else if(Opts.validation == Regex_Selector_Opts::Validation::Pedantic){
            const auto Values = cc.get().get_unique_values_for_key(MetadataKey);

            if(Values.empty()){
                if(false){
                }else if(Opts.nas == Regex_Selector_Opts::NAs::Include){
                    return false;
                }else if(Opts.nas == Regex_Selector_Opts::NAs::Exclude){
                    return true;
                }
                throw std::logic_error("Regex selector option not understood. Cannot continue.");

            }else{
                for(const auto & Value : Values){
                    if( !std::regex_match(Value,theregex) ) return true;
                }
                return false;
            }
            throw std::logic_error("Regex selector option not understood. Cannot continue.");

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
