cmake_minimum_required (VERSION 3.1.0 FATAL_ERROR)
project (DICOMautomaton)

set (DICOMautomaton_VERSION_MAJOR 3)
set (DICOMautomaton_VERSION_MINOR 0)
set (DICOMautomaton_VERSION_PATCH 0)

# Set the release type. 
set(CMAKE_BUILD_TYPE Release) # Comment this line to use fallback default.
if(NOT CMAKE_BUILD_TYPE)
    # Default to debug builds.
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "default to debug" FORCE)
endif()

SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
SET(BUILD_SHARED_LIBS TRUE)
SET(POSITION_INDEPENDENT_CODE TRUE)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # For clang-tidy et al.

# Use the directory where CMakeLists.txt is for inclusions.
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_INCLUDE_CURRENT_DIR_IN_INTERFACE ON)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

#############################################################

# Methods for finding Eigen.
# 
# CMake find script:
#find_package(Eigen3 3.1.0)
#include(${EIGEN3_USE_FILE})
#
# pkg-config method:
find_package(PkgConfig REQUIRED)
pkg_check_modules(EIGEN3 REQUIRED eigen3)
include_directories( ${EIGEN3_INCLUDE_DIRS} )
#
# Fallback if the above methods fail:
#include_directories("/usr/include/eigen3/")

# Find CGAL.
find_package(CGAL REQUIRED COMPONENTS Core)
include(${CGAL_USE_FILE})
add_definitions(-DCGAL_EIGEN3_ENABLED=1) # Explicitly instruct CGAL to use Eigen.


set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fdiagnostics-color=always -fno-var-tracking-assignments")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated -Wall -Wextra")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUSTREAM_H") # -DUSE_ICU_STRINGS


add_subdirectory(Operations)

# Generate object files for sources shared by many targets.
add_library(Structs_obj OBJECT
    Structs.cc     
)
set_target_properties(Structs_obj PROPERTIES
    POSITION_INDEPENDENT_CODE TRUE
)

add_library(Common_Plotting_obj OBJECT
    Common_Plotting.cc     
)
set_target_properties(Common_Plotting_obj PROPERTIES
    POSITION_INDEPENDENT_CODE TRUE
)

add_library(Common_Boost_Serialization_obj OBJECT
    Common_Boost_Serialization.cc     
)
set_target_properties(Common_Boost_Serialization_obj PROPERTIES
    POSITION_INDEPENDENT_CODE TRUE
)

add_library(Dose_Meld_obj OBJECT 
    Dose_Meld.cc    
)
set_target_properties(Dose_Meld_obj PROPERTIES
    POSITION_INDEPENDENT_CODE TRUE
)

add_library(Whole_Match_obj OBJECT
    Whole_Match.cc  
)
set_target_properties(Whole_Match_obj PROPERTIES
    POSITION_INDEPENDENT_CODE TRUE
)

add_library(PACS_Loader_obj OBJECT
    PACS_Loader.cc  
)
set_target_properties(PACS_Loader_obj PROPERTIES
    POSITION_INDEPENDENT_CODE TRUE
)

add_library(DICOM_File_Loader_obj OBJECT
    DICOM_File_Loader.cc  
)
set_target_properties(DICOM_File_Loader_obj PROPERTIES
    POSITION_INDEPENDENT_CODE TRUE
)

add_library(Boost_Serialization_File_Loader_obj OBJECT
    Boost_Serialization_File_Loader.cc  
    StructsIOBoostSerialization.h
)
set_target_properties(Boost_Serialization_File_Loader_obj PROPERTIES
    POSITION_INDEPENDENT_CODE TRUE
)

add_library(Operation_Dispatcher_obj OBJECT
    Operation_Dispatcher.cc
)
set_target_properties(Operation_Dispatcher_obj PROPERTIES
    POSITION_INDEPENDENT_CODE TRUE
)


FILE(GLOB ygorimaging_functors "./YgorImages_Functors/*/*cc")
add_library(YgorImaging_Functor_objs OBJECT
    ${ygorimaging_functors}
)
set_target_properties(YgorImaging_Functor_objs PROPERTIES
    POSITION_INDEPENDENT_CODE TRUE
)

FILE(GLOB ygorimaging_helpers  "./YgorImages_Functors/*cc")
add_library(YgorImaging_Helper_objs OBJECT
    ${ygorimaging_helpers}
)
set_target_properties(YgorImaging_Helper_objs PROPERTIES
    POSITION_INDEPENDENT_CODE TRUE
)






# Imebra library (built separately to maximally compartmentalize the long build process).
add_library (imebrashim 
    Imebra_Shim.cc 
    $<TARGET_OBJECTS:Structs_obj>
    imebra20121219/library/imebra/src/dataHandlerStringUT.cpp
    imebra20121219/library/imebra/src/data.cpp
    imebra20121219/library/imebra/src/colorTransformsFactory.cpp
    imebra20121219/library/imebra/src/dataHandlerTime.cpp
    imebra20121219/library/imebra/src/transaction.cpp
    imebra20121219/library/imebra/src/colorTransform.cpp
    imebra20121219/library/imebra/src/dataHandlerStringDS.cpp
    imebra20121219/library/imebra/src/dataHandlerString.cpp
    imebra20121219/library/imebra/src/MONOCHROME2ToYBRFULL.cpp
    imebra20121219/library/imebra/src/dataHandlerStringPN.cpp
    imebra20121219/library/imebra/src/image.cpp
    imebra20121219/library/imebra/src/drawBitmap.cpp
    imebra20121219/library/imebra/src/dataHandlerDateTimeBase.cpp
    imebra20121219/library/imebra/src/dataHandlerStringLT.cpp
    imebra20121219/library/imebra/src/dataHandlerDateTime.cpp
    imebra20121219/library/imebra/src/dataHandlerStringAE.cpp
    imebra20121219/library/imebra/src/YBRFULLToRGB.cpp
    imebra20121219/library/imebra/src/dicomCodec.cpp
    imebra20121219/library/imebra/src/transform.cpp
    imebra20121219/library/imebra/src/charsetsList.cpp
    imebra20121219/library/imebra/src/dataHandlerStringST.cpp
    imebra20121219/library/imebra/src/viewHelper.cpp
    imebra20121219/library/imebra/src/transformsChain.cpp
    imebra20121219/library/imebra/src/YBRPARTIALToRGB.cpp
    imebra20121219/library/imebra/src/dataGroup.cpp
    imebra20121219/library/imebra/src/dataHandlerStringCS.cpp
    imebra20121219/library/imebra/src/dataHandlerDate.cpp
    imebra20121219/library/imebra/src/transformHighBit.cpp
    imebra20121219/library/imebra/src/dataHandlerStringSH.cpp
    imebra20121219/library/imebra/src/buffer.cpp
    imebra20121219/library/imebra/src/codec.cpp
    imebra20121219/library/imebra/src/dataHandlerStringAS.cpp
    imebra20121219/library/imebra/src/dataHandlerStringUnicode.cpp
    imebra20121219/library/imebra/src/dataHandlerStringLO.cpp
    imebra20121219/library/imebra/src/VOILUT.cpp
    imebra20121219/library/imebra/src/MONOCHROME1ToMONOCHROME2.cpp
    imebra20121219/library/imebra/src/RGBToYBRFULL.cpp
    imebra20121219/library/imebra/src/dataHandlerStringIS.cpp
    imebra20121219/library/imebra/src/MONOCHROME2ToRGB.cpp
    imebra20121219/library/imebra/src/RGBToYBRPARTIAL.cpp
    imebra20121219/library/imebra/src/YBRFULLToMONOCHROME2.cpp
    imebra20121219/library/imebra/src/dataSet.cpp
    imebra20121219/library/imebra/src/dataHandlerStringUI.cpp
    imebra20121219/library/imebra/src/jpegCodec.cpp
    imebra20121219/library/imebra/src/waveform.cpp
    imebra20121219/library/imebra/src/MONOCHROME1ToRGB.cpp
    imebra20121219/library/imebra/src/dicomDir.cpp
    imebra20121219/library/imebra/src/modalityVOILUT.cpp
    imebra20121219/library/imebra/src/PALETTECOLORToRGB.cpp
    imebra20121219/library/imebra/src/RGBToMONOCHROME2.cpp
    imebra20121219/library/imebra/src/codecFactory.cpp
    imebra20121219/library/imebra/src/LUT.cpp
    imebra20121219/library/imebra/src/dicomDict.cpp
    imebra20121219/library/imebra/src/dataHandler.cpp
    imebra20121219/library/base/src/criticalSection.cpp
    imebra20121219/library/base/src/baseObject.cpp
    imebra20121219/library/base/src/charsetConversion.cpp
    imebra20121219/library/base/src/stream.cpp
    imebra20121219/library/base/src/streamController.cpp
    imebra20121219/library/base/src/thread.cpp
    imebra20121219/library/base/src/memory.cpp
    imebra20121219/library/base/src/exception.cpp
    imebra20121219/library/base/src/streamReader.cpp
    imebra20121219/library/base/src/memoryStream.cpp
    imebra20121219/library/base/src/baseStream.cpp
    imebra20121219/library/base/src/streamWriter.cpp
    imebra20121219/library/base/src/huffmanTable.cpp 
)
target_include_directories(imebrashim SYSTEM PUBLIC 
    ./imebra20121219/library/imebra/include/ 
)


# Pharmacokinetic modeling libraries (built separately for easier reuse).
add_library (pharmacokinetic_modeling_via_optimization
    Pharmacokinetic_Modeling_via_Optimization.cc
)
target_include_directories(pharmacokinetic_modeling_via_optimization SYSTEM PUBLIC 
    ./
)

add_library (pharmacokinetic_modeling_via_least_squares
    Pharmacokinetic_Modeling_via_Least_Squares.cc
)
target_include_directories(pharmacokinetic_modeling_via_least_squares SYSTEM PUBLIC 
    ./
)


# Executable.
add_executable (dicomautomaton_dispatcher
    DICOMautomaton_Dispatcher.cc

    $<TARGET_OBJECTS:Structs_obj>
    $<TARGET_OBJECTS:Dose_Meld_obj>
    $<TARGET_OBJECTS:Common_Boost_Serialization_obj>
    $<TARGET_OBJECTS:Common_Plotting_obj>
    $<TARGET_OBJECTS:Whole_Match_obj>

    $<TARGET_OBJECTS:PACS_Loader_obj>
    $<TARGET_OBJECTS:Boost_Serialization_File_Loader_obj>
    $<TARGET_OBJECTS:DICOM_File_Loader_obj>
    $<TARGET_OBJECTS:Operation_Dispatcher_obj>

    $<TARGET_OBJECTS:YgorImaging_Functor_objs>
    $<TARGET_OBJECTS:YgorImaging_Helper_objs>

    $<TARGET_OBJECTS:Operations_objs>
)
target_link_libraries (dicomautomaton_dispatcher
    imebrashim
    pharmacokinetic_modeling_via_optimization
    pharmacokinetic_modeling_via_least_squares
    explicator 
    demarcator 
    ygor 
    CGAL
    nlopt 
    gsl
    gslcblas
    sfml-graphics 
    sfml-audio 
    sfml-window 
    sfml-system
    jansson 
    pqxx 
    pq
    boost_filesystem 
    boost_serialization
    boost_iostreams
    boost_system 
    z
    mpfr
    gmp
    m
)

# Executable.
add_executable(dicomautomaton_bsarchive_convert
    Boost_Serialization_Archive_Converter.cc

    $<TARGET_OBJECTS:Structs_obj>
    $<TARGET_OBJECTS:Dose_Meld_obj>
    $<TARGET_OBJECTS:Common_Boost_Serialization_obj>
    $<TARGET_OBJECTS:Whole_Match_obj>

    $<TARGET_OBJECTS:Boost_Serialization_File_Loader_obj>
)
target_link_libraries (dicomautomaton_bsarchive_convert
    explicator 
    demarcator 
    ygor 
    boost_filesystem 
    boost_serialization
    boost_iostreams
    boost_system 
    z
    m
)

# Executable.
add_executable (pacs_ingress
    PACS_Ingress.cc
    $<TARGET_OBJECTS:Structs_obj>
    $<TARGET_OBJECTS:Dose_Meld_obj>
)
target_link_libraries (pacs_ingress
    imebrashim
    explicator 
    demarcator 
    ygor 
    pqxx 
    pq
    m
)

# Executable.
add_executable (pacs_refresh
    PACS_Refresh.cc
    $<TARGET_OBJECTS:Structs_obj>
    $<TARGET_OBJECTS:Dose_Meld_obj>
)
target_link_libraries (pacs_refresh
    imebrashim
    explicator 
    demarcator 
    ygor 
    pqxx 
    pq
    m
)

# Executable.    
add_executable (overlaydosedata
    Overlay_Dose_Data.cc
    $<TARGET_OBJECTS:Structs_obj>
    $<TARGET_OBJECTS:Dose_Meld_obj>
    $<TARGET_OBJECTS:Whole_Match_obj>
)
target_link_libraries (overlaydosedata
    imebrashim
    explicator 
    demarcator 
    ygor 
    pqxx 
    pq
    X11
    Xi
    Xmu
    glut
    GL
    GLU
    m
)
target_include_directories (overlaydosedata SYSTEM PUBLIC /usr/X11R6/lib/ )


# Executable.
add_executable (automaton
    Automaton.cc
    $<TARGET_OBJECTS:Structs_obj>
    $<TARGET_OBJECTS:Dose_Meld_obj>
)
target_link_libraries (automaton
    imebrashim
    explicator 
    demarcator 
    csvtools
    ygor 
    pqxx 
    pq
    m
)

# Executable.
add_executable (dicomautomaton_dump
    DICOMautomaton_Dump.cc
    $<TARGET_OBJECTS:Structs_obj>
    $<TARGET_OBJECTS:Dose_Meld_obj>
)
target_link_libraries (dicomautomaton_dump
    imebrashim
    ygor 
    m
)

# Installation info.
install(TARGETS 
    dicomautomaton_dispatcher
    dicomautomaton_bsarchive_convert
    pacs_ingress
    pacs_refresh
    overlaydosedata
    automaton
    dicomautomaton_dump
    imebrashim
    pharmacokinetic_modeling_via_optimization
    pharmacokinetic_modeling_via_least_squares
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)

include(CPack)

