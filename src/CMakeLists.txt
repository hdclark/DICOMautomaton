cmake_minimum_required (VERSION 3.1.0 FATAL_ERROR)
project (DICOMautomaton)

set (DICOMautomaton_VERSION_MAJOR 2)
set (DICOMautomaton_VERSION_MINOR 0)
set (DICOMautomaton_VERSION_PATCH 0)

# Default build is 'debug'.
if(NOT CMAKE_BUILD_TYPE)
    SET(CMAKE_BUILD_TYPE Debug CACHE STRING "default to debug" FORCE)
endif()

#set(BINDIR "${CMAKE_INSTALL_PREFIX}/bin")


set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fdiagnostics-color=always -fno-var-tracking-assignments")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated -Wall -Wextra")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUSTREAM_H") # -DUSE_ICU_STRINGS


SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
SET(BUILD_SHARED_LIBS TRUE)
SET(POSITION_INDEPENDENT_CODE TRUE)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # For clang-tidy et al.


# Use the directory where CMakeLists.txt is for inclusions.
INCLUDE_DIRECTORIES(/usr/local/lib/)
INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR})

# Generate object files for sources shared by many targets.
add_library ( Structs_obj      OBJECT Structs.cc      )
set_target_properties(Structs_obj PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

add_library ( Dose_Meld_obj    OBJECT Dose_Meld.cc    )
set_target_properties(Dose_Meld_obj PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

add_library ( Whole_Match_obj  OBJECT Whole_Match.cc  )
set_target_properties(Whole_Match_obj PROPERTIES POSITION_INDEPENDENT_CODE TRUE)


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
target_include_directories (imebrashim SYSTEM PUBLIC ./imebra20121219/library/imebra/include/ )


# Executable.
FILE(GLOB ygorimaging_functors "./YgorImages_Functors/*/*cc")
add_executable (petct_perfusion_analysis 
    PETCT_Perfusion_Analysis.cc
    $<TARGET_OBJECTS:Structs_obj>
    $<TARGET_OBJECTS:Dose_Meld_obj>
    $<TARGET_OBJECTS:Whole_Match_obj>
    ${ygorimaging_functors}
)
target_link_libraries (petct_perfusion_analysis 
    imebrashim
    ygor 
    explicator 
    demarcator 
    nlopt 
    sfml-graphics 
    sfml-audio 
    sfml-window 
    sfml-system
    jansson 
    pqxx 
    pq
    m
)

# Executable.
add_executable (pacs_ingress
    PACS_Ingress.cc
    $<TARGET_OBJECTS:Structs_obj>
    $<TARGET_OBJECTS:Dose_Meld_obj>
#    $<TARGET_OBJECTS:Whole_Match_obj>
)
target_link_libraries (pacs_ingress
    imebrashim
    ygor 
    explicator 
    demarcator 
    pqxx 
    pq
    m
)

# Executable.
add_executable (pacs_refresh
    PACS_Refresh.cc
    $<TARGET_OBJECTS:Structs_obj>
    $<TARGET_OBJECTS:Dose_Meld_obj>
#    $<TARGET_OBJECTS:Whole_Match_obj>
)
target_link_libraries (pacs_refresh
    imebrashim
    ygor 
    explicator 
    demarcator 
    pqxx 
    pq
    m
)

# Executable.
add_executable (pacs_verify_filesystem_store
    PACS_Verify_Filesystem_Store.cc
    $<TARGET_OBJECTS:Structs_obj>
    $<TARGET_OBJECTS:Dose_Meld_obj>
#    $<TARGET_OBJECTS:Whole_Match_obj>
)
target_link_libraries (pacs_verify_filesystem_store
    imebrashim
    ygor 
    explicator 
    demarcator 
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
    ygor 
    explicator 
    demarcator 
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
#    $<TARGET_OBJECTS:Whole_Match_obj>
)
target_link_libraries (automaton
    imebrashim
    ygor 
    explicator 
    demarcator 
    csvtools
    pqxx 
    pq
    m
)

# Installation info.
INSTALL(TARGETS 
    petct_perfusion_analysis
    pacs_ingress
    pacs_refresh
    pacs_verify_filesystem_store
    overlaydosedata
    automaton
    imebrashim
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)

INCLUDE(CPack)

