//Regex_Selectors.h.

#pragma once

#include <string>
#include <list>
#include <initializer_list>
#include <functional>
#include <regex>

#include "YgorString.h"
#include "YgorMath.h"

#include "Structs.h"


// A parameter struct for regex selector functions.
struct Regex_Selector_Opts {

    enum class 
    NAs {             // Items missing the specified metadata.
        TreatAsEmpty, // Treat as if they were the empty string.
                      //   This is useful because often the regex will match everything and matching N/As is
                      //   desired, but if something specific is needed the empty string often won't match.
        Include,      // Unilaterally include them in the output.
        Exclude,      // Unilaterally filter them out.
    } nas = NAs::TreatAsEmpty;

    enum class
    Validation {        // How items are treated.
        Representative, // Use items that should normally be representative of a partition or group.
        Pedantic,       // Explicitly verify each individual item.
                        //   Each item in the partition or group must satisfy the criteria.
    } validation = Validation::Representative;

};


// --------------------------------------- Misc. ---------------------------------------

// Compile and return a regex using the application-wide default settings.
std::regex
Compile_Regex(std::string input);


// ---------------------------------- Contours / ROIs ----------------------------------

// Stuff references to all contour collections into a list.
//
// Note: the output is meant to be filtered out using the selectors below.
std::list<std::reference_wrapper<contour_collection<double>>>
All_CCs( Drover DICOM_data );


// Whitelist contour collections using the provided regex.
std::list<std::reference_wrapper<contour_collection<double>>>
Whitelist( std::list<std::reference_wrapper<contour_collection<double>>> ccs,
           std::string MetadataKey,
           std::string MetadataValueRegex,
           Regex_Selector_Opts Opts = Regex_Selector_Opts() );

// This is a convenience routine to combine multiple filtering passes into a single logical statement.
std::list<std::reference_wrapper<contour_collection<double>>>
Whitelist( std::list<std::reference_wrapper<contour_collection<double>>> ccs,
           std::initializer_list< std::pair<std::string,        // MetadataKey
                                            std::string> > MetadataKeyValueRegex, // MetadataValueRegex
           Regex_Selector_Opts Opts = Regex_Selector_Opts() );


