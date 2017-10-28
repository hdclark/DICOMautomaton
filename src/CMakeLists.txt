

# ----------------------------------------------------------------------------------
# Find Threading library.
# ----------------------------------------------------------------------------------
# via CMake find script:
find_package(Threads REQUIRED)

# ----------------------------------------------------------------------------------
# Find Eigen.
# ----------------------------------------------------------------------------------
# via CMake find script:
#find_package(Eigen3 3.1.0)
#include(${EIGEN3_USE_FILE})
#
# via pkg-config method:
find_package(PkgConfig REQUIRED)
pkg_check_modules(EIGEN3 REQUIRED eigen3)
include_directories( ${EIGEN3_INCLUDE_DIRS} )
#
# Fallback if the above methods fail:
#include_directories("/usr/include/eigen3/")

# ----------------------------------------------------------------------------------
# Find CGAL.
# ----------------------------------------------------------------------------------
find_package(CGAL REQUIRED COMPONENTS Core)
include(${CGAL_USE_FILE})
add_definitions(-DCGAL_EIGEN3_ENABLED=1) # Explicitly instruct CGAL to use Eigen.
# ----------------------------------------------------------------------------------

add_subdirectory(Operations)


add_library(Structs_obj OBJECT
    Structs.cc     
)
set_target_properties(Structs_obj PROPERTIES
    POSITION_INDEPENDENT_CODE TRUE
)

add_library(BED_Conversion_obj OBJECT
    BED_Conversion.cc
)
set_target_properties(BED_Conversion_obj PROPERTIES
    POSITION_INDEPENDENT_CODE TRUE
)

add_library(Colour_Maps_obj OBJECT
    Colour_Maps.cc     
)
set_target_properties(Colour_Maps_obj PROPERTIES
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

add_library(Contour_Boolean_Operations_obj OBJECT
    Contour_Boolean_Operations.cc     
)
set_target_properties(Contour_Boolean_Operations_obj PROPERTIES
    POSITION_INDEPENDENT_CODE TRUE
)

add_library(Dose_Meld_obj OBJECT 
    Dose_Meld.cc    
)
set_target_properties(Dose_Meld_obj PROPERTIES
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

add_library(FITS_File_Loader_obj OBJECT
    FITS_File_Loader.cc  
)
set_target_properties(FITS_File_Loader_obj PROPERTIES
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

add_library (kineticmodel_1c2i_5param_linearinterp_levenbergmarquardt
    KineticModel_1Compartment2Input_5Param_LinearInterp_Common.cc
    KineticModel_1Compartment2Input_5Param_LinearInterp_LevenbergMarquardt.cc
)
target_include_directories(kineticmodel_1c2i_5param_linearinterp_levenbergmarquardt SYSTEM PUBLIC 
    ./
)

add_library (kineticmodel_1c2i_5param_chebyshev_freeformoptimization
    KineticModel_1Compartment2Input_5Param_Chebyshev_Common.cc
    KineticModel_1Compartment2Input_5Param_Chebyshev_FreeformOptimization.cc
)
target_include_directories(kineticmodel_1c2i_5param_chebyshev_freeformoptimization SYSTEM PUBLIC 
    ./
)

add_library (kineticmodel_1c2i_5param_chebyshev_levenbergmarquardt
    KineticModel_1Compartment2Input_5Param_Chebyshev_Common.cc
    KineticModel_1Compartment2Input_5Param_Chebyshev_LevenbergMarquardt.cc
)
target_include_directories(kineticmodel_1c2i_5param_chebyshev_levenbergmarquardt SYSTEM PUBLIC 
    ./
)

add_library (kineticmodel_1c2i_reduced3param_chebyshev_freeformoptimization
    KineticModel_1Compartment2Input_Reduced3Param_Chebyshev_Common.cc
    KineticModel_1Compartment2Input_Reduced3Param_Chebyshev_FreeformOptimization.cc
)
target_include_directories(kineticmodel_1c2i_reduced3param_chebyshev_freeformoptimization SYSTEM PUBLIC 
    ./
)


# Executable.
add_executable (dicomautomaton_dispatcher
    DICOMautomaton_Dispatcher.cc

    $<TARGET_OBJECTS:Structs_obj>
    $<TARGET_OBJECTS:Dose_Meld_obj>
    $<TARGET_OBJECTS:BED_Conversion_obj>
    $<TARGET_OBJECTS:Colour_Maps_obj>
    $<TARGET_OBJECTS:Common_Boost_Serialization_obj>
    $<TARGET_OBJECTS:Common_Plotting_obj>
    $<TARGET_OBJECTS:Contour_Boolean_Operations_obj>

    $<TARGET_OBJECTS:PACS_Loader_obj>
    $<TARGET_OBJECTS:Boost_Serialization_File_Loader_obj>
    $<TARGET_OBJECTS:DICOM_File_Loader_obj>
    $<TARGET_OBJECTS:FITS_File_Loader_obj>
    $<TARGET_OBJECTS:Operation_Dispatcher_obj>

    $<TARGET_OBJECTS:YgorImaging_Functor_objs>
    $<TARGET_OBJECTS:YgorImaging_Helper_objs>

    $<TARGET_OBJECTS:Operations_objs>
)
target_link_libraries (dicomautomaton_dispatcher
    imebrashim
    kineticmodel_1c2i_5param_linearinterp_levenbergmarquardt
    kineticmodel_1c2i_5param_chebyshev_freeformoptimization
    kineticmodel_1c2i_5param_chebyshev_levenbergmarquardt
    kineticmodel_1c2i_reduced3param_chebyshev_freeformoptimization
    explicator 
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
    boost_thread
    boost_system 
    z
    mpfr
    gmp
    m
    Threads::Threads
)

# Executable.
add_executable(dicomautomaton_webserver
    DICOMautomaton_WebServer.cc

    $<TARGET_OBJECTS:Structs_obj>
    $<TARGET_OBJECTS:Dose_Meld_obj>
    $<TARGET_OBJECTS:BED_Conversion_obj>
    $<TARGET_OBJECTS:Colour_Maps_obj>
    $<TARGET_OBJECTS:Common_Boost_Serialization_obj>
    $<TARGET_OBJECTS:Common_Plotting_obj>
    $<TARGET_OBJECTS:Contour_Boolean_Operations_obj>

    $<TARGET_OBJECTS:PACS_Loader_obj>
    $<TARGET_OBJECTS:Boost_Serialization_File_Loader_obj>
    $<TARGET_OBJECTS:DICOM_File_Loader_obj>
    $<TARGET_OBJECTS:FITS_File_Loader_obj>
    $<TARGET_OBJECTS:Operation_Dispatcher_obj>

    $<TARGET_OBJECTS:YgorImaging_Functor_objs>
    $<TARGET_OBJECTS:YgorImaging_Helper_objs>

    $<TARGET_OBJECTS:Operations_objs>
)
target_link_libraries(dicomautomaton_webserver
    imebrashim
    kineticmodel_1c2i_5param_linearinterp_levenbergmarquardt
    kineticmodel_1c2i_5param_chebyshev_freeformoptimization
    kineticmodel_1c2i_5param_chebyshev_levenbergmarquardt
    kineticmodel_1c2i_reduced3param_chebyshev_freeformoptimization
    explicator 
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
    wt
    wthttp
    boost_filesystem 
    boost_serialization
    boost_iostreams
    boost_thread
    boost_system 
    z
    mpfr
    gmp
    m
    Threads::Threads
)

# Executable.
add_executable(dicomautomaton_bsarchive_convert
    Boost_Serialization_Archive_Converter.cc

    $<TARGET_OBJECTS:Structs_obj>
    $<TARGET_OBJECTS:Dose_Meld_obj>
    $<TARGET_OBJECTS:Common_Boost_Serialization_obj>

    $<TARGET_OBJECTS:Boost_Serialization_File_Loader_obj>
)
target_link_libraries(dicomautomaton_bsarchive_convert
    explicator 
    ygor 
    boost_filesystem 
    boost_serialization
    boost_iostreams
    boost_system 
    z
    m
    Threads::Threads
)

# Executable.
add_executable(pacs_ingress
    PACS_Ingress.cc
    $<TARGET_OBJECTS:Structs_obj>
    $<TARGET_OBJECTS:Dose_Meld_obj>
)
target_link_libraries(pacs_ingress
    imebrashim
    explicator 
    ygor 
    pqxx 
    pq
    m
    Threads::Threads
)

# Executable.
add_executable(pacs_duplicate_cleaner
    PACS_Duplicate_Cleaner.cc
    $<TARGET_OBJECTS:Structs_obj>
    $<TARGET_OBJECTS:Dose_Meld_obj>
)
target_link_libraries(pacs_duplicate_cleaner
    imebrashim
    explicator 
    ygor 
    pqxx 
    pq
    m
    Threads::Threads
)

# Executable.
add_executable(pacs_refresh
    PACS_Refresh.cc
    $<TARGET_OBJECTS:Structs_obj>
    $<TARGET_OBJECTS:Dose_Meld_obj>
)
target_link_libraries(pacs_refresh
    imebrashim
    explicator 
    ygor 
    pqxx 
    pq
    m
    Threads::Threads
)

# Executable.    
add_executable(overlaydosedata
    Overlay_Dose_Data.cc
    $<TARGET_OBJECTS:Structs_obj>
    $<TARGET_OBJECTS:Dose_Meld_obj>
)
target_link_libraries(overlaydosedata
    imebrashim
    explicator 
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
    Threads::Threads
)
target_include_directories (overlaydosedata SYSTEM PUBLIC /usr/X11R6/lib/ )


# Executable.
add_executable(automaton
    Automaton.cc
    $<TARGET_OBJECTS:Structs_obj>
    $<TARGET_OBJECTS:Dose_Meld_obj>
)
target_link_libraries(automaton
    imebrashim
    explicator 
    ygor 
    pqxx 
    pq
    m
    Threads::Threads
)

# Executable.
add_executable(dicomautomaton_dump
    DICOMautomaton_Dump.cc
    $<TARGET_OBJECTS:Structs_obj>
    $<TARGET_OBJECTS:Dose_Meld_obj>
)
target_link_libraries(dicomautomaton_dump
    imebrashim
    ygor 
    m
)

# Installation info.
install(TARGETS 
    dicomautomaton_dispatcher
    dicomautomaton_webserver
    dicomautomaton_bsarchive_convert
    pacs_ingress
    pacs_duplicate_cleaner
    pacs_refresh
    overlaydosedata
    automaton
    dicomautomaton_dump
    imebrashim
    kineticmodel_1c2i_5param_linearinterp_levenbergmarquardt
    kineticmodel_1c2i_5param_chebyshev_freeformoptimization
    kineticmodel_1c2i_5param_chebyshev_levenbergmarquardt
    kineticmodel_1c2i_reduced3param_chebyshev_freeformoptimization
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)

