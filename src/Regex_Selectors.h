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


// ----------------------------------- Image Arrays ------------------------------------

// Provide iterators for all image arrays into a list.
//
// Note: The output is meant to be filtered using the selectors below.
//
// Note: Iterators returned by this routine will be invalidated when the Drover object is copied!
std::list<std::list<std::shared_ptr<Image_Array>>::iterator>
All_IAs( Drover &DICOM_data );

// Whitelist image arrays using the provided regex.
std::list<std::list<std::shared_ptr<Image_Array>>::iterator>
Whitelist( std::list<std::list<std::shared_ptr<Image_Array>>::iterator> ias,
           std::string MetadataKey,
           std::string MetadataValueRegex,
           Regex_Selector_Opts Opts = Regex_Selector_Opts() );

// Whitelist image arrays using a limited vocabulary of specifiers.
std::list<std::list<std::shared_ptr<Image_Array>>::iterator>
Whitelist( std::list<std::list<std::shared_ptr<Image_Array>>::iterator> ias,
           std::string Specifier,
           Regex_Selector_Opts Opts = Regex_Selector_Opts() );

// This is a convenience routine to combine multiple filtering passes into a single logical statement.
std::list<std::list<std::shared_ptr<Image_Array>>::iterator>
Whitelist( std::list<std::list<std::shared_ptr<Image_Array>>::iterator> ias,
           std::initializer_list< std::pair<std::string,        // MetadataKey
                                            std::string> > MetadataKeyValueRegex, // MetadataValueRegex
           Regex_Selector_Opts Opts = Regex_Selector_Opts() );

// Utility function documenting the image array whitelist routines for operations.
OperationArgDoc IAWhitelistOpArgDoc(void);


// ----------------------------------- Point Clouds ------------------------------------

// Provide pointers for all point clouds into a list.
//
// Note: The output is meant to be filtered using the selectors below.
std::list<std::list<std::shared_ptr<Point_Cloud>>::iterator>
All_PCs( Drover &DICOM_data );


// Whitelist point clouds using the provided regex.
std::list<std::list<std::shared_ptr<Point_Cloud>>::iterator>
Whitelist( std::list<std::list<std::shared_ptr<Point_Cloud>>::iterator> pcs,
           std::string MetadataKey,
           std::string MetadataValueRegex,
           Regex_Selector_Opts Opts = Regex_Selector_Opts() );

// Whitelist point clouds using a limited vocabulary of specifiers.
std::list<std::list<std::shared_ptr<Point_Cloud>>::iterator>
Whitelist( std::list<std::list<std::shared_ptr<Point_Cloud>>::iterator> pcs,
           std::string Specifier,
           Regex_Selector_Opts Opts = Regex_Selector_Opts() );

// This is a convenience routine to combine multiple filtering passes into a single logical statement.
std::list<std::list<std::shared_ptr<Point_Cloud>>::iterator>
Whitelist( std::list<std::list<std::shared_ptr<Point_Cloud>>::iterator> pcs,
           std::initializer_list< std::pair<std::string,        // MetadataKey
                                            std::string> > MetadataKeyValueRegex, // MetadataValueRegex
           Regex_Selector_Opts Opts = Regex_Selector_Opts() );

// Utility function documenting the point cloud whitelist routines for operations.
OperationArgDoc PCWhitelistOpArgDoc(void);


// ----------------------------------- Surface Meshes ------------------------------------

// Provide pointers for all surface meshes into a list.
//
// Note: The output is meant to be filtered using the selectors below.
std::list<std::list<std::shared_ptr<Surface_Mesh>>::iterator>
All_SMs( Drover &DICOM_data );


// Whitelist surface meshes using the provided regex.
std::list<std::list<std::shared_ptr<Surface_Mesh>>::iterator>
Whitelist( std::list<std::list<std::shared_ptr<Surface_Mesh>>::iterator> sms,
           std::string MetadataKey,
           std::string MetadataValueRegex,
           Regex_Selector_Opts Opts = Regex_Selector_Opts() );

// Whitelist surface meshes using a limited vocabulary of specifiers.
std::list<std::list<std::shared_ptr<Surface_Mesh>>::iterator>
Whitelist( std::list<std::list<std::shared_ptr<Surface_Mesh>>::iterator> sms,
           std::string Specifier,
           Regex_Selector_Opts Opts = Regex_Selector_Opts() );

// This is a convenience routine to combine multiple filtering passes into a single logical statement.
std::list<std::list<std::shared_ptr<Surface_Mesh>>::iterator>
Whitelist( std::list<std::list<std::shared_ptr<Surface_Mesh>>::iterator> sms,
           std::initializer_list< std::pair<std::string,        // MetadataKey
                                            std::string> > MetadataKeyValueRegex, // MetadataValueRegex
           Regex_Selector_Opts Opts = Regex_Selector_Opts() );

// Utility function documenting the surface mesh whitelist routines for operations.
OperationArgDoc SMWhitelistOpArgDoc(void);


// ------------------------------------ TPlan_Config -------------------------------------

// Provide pointers for all surface meshes into a list.
//
// Note: The output is meant to be filtered using the selectors below.
std::list<std::list<std::shared_ptr<TPlan_Config>>::iterator>
All_TPs( Drover &DICOM_data );


// Whitelist surface meshes using the provided regex.
std::list<std::list<std::shared_ptr<TPlan_Config>>::iterator>
Whitelist( std::list<std::list<std::shared_ptr<TPlan_Config>>::iterator> tps,
           std::string MetadataKey,
           std::string MetadataValueRegex,
           Regex_Selector_Opts Opts = Regex_Selector_Opts() );

// Whitelist surface meshes using a limited vocabulary of specifiers.
std::list<std::list<std::shared_ptr<TPlan_Config>>::iterator>
Whitelist( std::list<std::list<std::shared_ptr<TPlan_Config>>::iterator> tps,
           std::string Specifier,
           Regex_Selector_Opts Opts = Regex_Selector_Opts() );

// This is a convenience routine to combine multiple filtering passes into a single logical statement.
std::list<std::list<std::shared_ptr<TPlan_Config>>::iterator>
Whitelist( std::list<std::list<std::shared_ptr<TPlan_Config>>::iterator> tps,
           std::initializer_list< std::pair<std::string,        // MetadataKey
                                            std::string> > MetadataKeyValueRegex, // MetadataValueRegex
           Regex_Selector_Opts Opts = Regex_Selector_Opts() );

// Utility function documenting the surface mesh whitelist routines for operations.
OperationArgDoc TPWhitelistOpArgDoc(void);

// ----------------------------------- Line Samples ------------------------------------

// Provide pointers for all line samples into a list.
//
// Note: The output is meant to be filtered using the selectors below.
std::list<std::list<std::shared_ptr<Line_Sample>>::iterator>
All_LSs( Drover &DICOM_data );


// Whitelist line samples using the provided regex.
std::list<std::list<std::shared_ptr<Line_Sample>>::iterator>
Whitelist( std::list<std::list<std::shared_ptr<Line_Sample>>::iterator> lss,
           std::string MetadataKey,
           std::string MetadataValueRegex,
           Regex_Selector_Opts Opts = Regex_Selector_Opts() );

// Whitelist line samples using a limited vocabulary of specifiers.
std::list<std::list<std::shared_ptr<Line_Sample>>::iterator>
Whitelist( std::list<std::list<std::shared_ptr<Line_Sample>>::iterator> lss,
           std::string Specifier,
           Regex_Selector_Opts Opts = Regex_Selector_Opts() );

// This is a convenience routine to combine multiple filtering passes into a single logical statement.
std::list<std::list<std::shared_ptr<Line_Sample>>::iterator>
Whitelist( std::list<std::list<std::shared_ptr<Line_Sample>>::iterator> lss,
           std::initializer_list< std::pair<std::string,        // MetadataKey
                                            std::string> > MetadataKeyValueRegex, // MetadataValueRegex
           Regex_Selector_Opts Opts = Regex_Selector_Opts() );

// Utility function documenting the line sample whitelist routines for operations.
OperationArgDoc LSWhitelistOpArgDoc(void);

// ------------------------------------ Transform3 -------------------------------------

// Provide pointers for all transformations into a list.
//
// Note: The output is meant to be filtered using the selectors below.
std::list<std::list<std::shared_ptr<Transform3>>::iterator>
All_T3s( Drover &DICOM_data );


// Whitelist transforms using the provided regex.
std::list<std::list<std::shared_ptr<Transform3>>::iterator>
Whitelist( std::list<std::list<std::shared_ptr<Transform3>>::iterator> lss,
           std::string MetadataKey,
           std::string MetadataValueRegex,
           Regex_Selector_Opts Opts = Regex_Selector_Opts() );

// Whitelist transforms using a limited vocabulary of specifiers.
std::list<std::list<std::shared_ptr<Transform3>>::iterator>
Whitelist( std::list<std::list<std::shared_ptr<Transform3>>::iterator> lss,
           std::string Specifier,
           Regex_Selector_Opts Opts = Regex_Selector_Opts() );

// This is a convenience routine to combine multiple filtering passes into a single logical statement.
std::list<std::list<std::shared_ptr<Transform3>>::iterator>
Whitelist( std::list<std::list<std::shared_ptr<Transform3>>::iterator> lss,
           std::initializer_list< std::pair<std::string,        // MetadataKey
                                            std::string> > MetadataKeyValueRegex, // MetadataValueRegex
           Regex_Selector_Opts Opts = Regex_Selector_Opts() );

// Utility function documenting the transform whitelist routines for operations.
OperationArgDoc T3WhitelistOpArgDoc(void);


