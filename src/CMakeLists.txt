

add_custom_command(
    OUTPUT  "${CMAKE_CURRENT_BINARY_DIR}/Standard_Scripts.cc" # Treated as relative to CMAKE_CURRENT_BINARY_DIR
    COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/../scripts/generate_standard_scripts.sh"
                "${CMAKE_CURRENT_SOURCE_DIR}/Standard_Scripts.cc.in"
                "${CMAKE_CURRENT_BINARY_DIR}/Standard_Scripts.cc"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Standard_Scripts.cc.in"   # Treated as relative to CMAKE_CURRENT_SOURCE_DIR
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../"
)

add_subdirectory(Operations)
if(WITH_THRIFT)
    add_subdirectory(rpc)
endif()

add_library(            DCMA_Version_obj OBJECT DCMA_Version.cc)
set_target_properties(  DCMA_Version_obj PROPERTIES POSITION_INDEPENDENT_CODE TRUE )
target_compile_definitions( DCMA_Version_obj PUBLIC -DDCMA_VERSION=${DCMA_VERSION} )

add_library(            Structs_obj OBJECT Structs.cc)
set_target_properties(  Structs_obj PROPERTIES POSITION_INDEPENDENT_CODE TRUE )

add_library(            Tables_obj OBJECT Tables.cc)
set_target_properties(  Tables_obj PROPERTIES POSITION_INDEPENDENT_CODE TRUE )

add_library(            Partition_Drover_obj OBJECT Partition_Drover.cc)
set_target_properties(  Partition_Drover_obj PROPERTIES POSITION_INDEPENDENT_CODE TRUE )

add_library(            DCMA_DICOM_obj OBJECT DCMA_DICOM.cc)
set_target_properties(  DCMA_DICOM_obj PROPERTIES POSITION_INDEPENDENT_CODE TRUE )

add_library(            BED_Conversion_obj OBJECT BED_Conversion.cc )
set_target_properties(  BED_Conversion_obj PROPERTIES POSITION_INDEPENDENT_CODE TRUE )

add_library(            Alignment_Rigid_obj OBJECT Alignment_Rigid.cc )
set_target_properties(  Alignment_Rigid_obj PROPERTIES POSITION_INDEPENDENT_CODE TRUE )

add_library(            Alignment_TPSRPM_obj OBJECT Alignment_TPSRPM.cc )
set_target_properties(  Alignment_TPSRPM_obj PROPERTIES POSITION_INDEPENDENT_CODE TRUE )

add_library(            Alignment_Field_obj OBJECT Alignment_Field.cc )
set_target_properties(  Alignment_Field_obj PROPERTIES POSITION_INDEPENDENT_CODE TRUE )

add_library(            Colour_Maps_obj OBJECT Colour_Maps.cc )
set_target_properties(  Colour_Maps_obj PROPERTIES POSITION_INDEPENDENT_CODE TRUE )

add_library(            Common_Plotting_obj OBJECT Common_Plotting.cc )
set_target_properties(  Common_Plotting_obj PROPERTIES POSITION_INDEPENDENT_CODE TRUE )

add_library(            Insert_Contours_obj OBJECT Insert_Contours.cc )
set_target_properties(  Insert_Contours_obj PROPERTIES POSITION_INDEPENDENT_CODE TRUE )

add_library(            Regex_Selectors_obj OBJECT Regex_Selectors.cc )
set_target_properties(  Regex_Selectors_obj PROPERTIES POSITION_INDEPENDENT_CODE TRUE )

add_library(            String_Parsing_obj OBJECT String_Parsing.cc )
set_target_properties(  String_Parsing_obj PROPERTIES POSITION_INDEPENDENT_CODE TRUE )

add_library(            Metadata_obj OBJECT Metadata.cc )
set_target_properties(  Metadata_obj PROPERTIES POSITION_INDEPENDENT_CODE TRUE )

add_library(            CSG_SDF_obj OBJECT CSG_SDF.cc )
set_target_properties(  CSG_SDF_obj PROPERTIES POSITION_INDEPENDENT_CODE TRUE )

add_library(            Common_Boost_Serialization_obj OBJECT Common_Boost_Serialization.cc )
set_target_properties(  Common_Boost_Serialization_obj PROPERTIES POSITION_INDEPENDENT_CODE TRUE )

if(WITH_CGAL)
    add_library(            Contour_Boolean_Operations_obj OBJECT Contour_Boolean_Operations.cc )
    set_target_properties(  Contour_Boolean_Operations_obj PROPERTIES POSITION_INDEPENDENT_CODE TRUE )
endif()

add_library(            Dose_Meld_obj OBJECT Dose_Meld.cc )
set_target_properties(  Dose_Meld_obj PROPERTIES POSITION_INDEPENDENT_CODE TRUE )

if(WITH_POSTGRES)
    add_library(            PACS_Loader_obj OBJECT PACS_Loader.cc )
    set_target_properties(  PACS_Loader_obj PROPERTIES POSITION_INDEPENDENT_CODE TRUE )
endif()

add_library(            DICOM_File_Loader_obj OBJECT DICOM_File_Loader.cc )
set_target_properties(  DICOM_File_Loader_obj PROPERTIES POSITION_INDEPENDENT_CODE TRUE )

add_library(            Lexicon_Loader_obj OBJECT Lexicon_Loader.cc )
set_target_properties(  Lexicon_Loader_obj PROPERTIES POSITION_INDEPENDENT_CODE TRUE )

add_library(            Contour_Collection_Estimates_obj OBJECT Contour_Collection_Estimates.cc )
set_target_properties(  Contour_Collection_Estimates_obj PROPERTIES POSITION_INDEPENDENT_CODE TRUE )

add_library(            File_Loader_obj OBJECT File_Loader.cc )
set_target_properties(  File_Loader_obj PROPERTIES POSITION_INDEPENDENT_CODE TRUE )

add_library(            FITS_File_Loader_obj OBJECT FITS_File_Loader.cc )
set_target_properties(  FITS_File_Loader_obj PROPERTIES POSITION_INDEPENDENT_CODE TRUE )

add_library(            XYZ_File_Loader_obj OBJECT XYZ_File_Loader.cc )
set_target_properties(  XYZ_File_Loader_obj PROPERTIES POSITION_INDEPENDENT_CODE TRUE )

add_library(            CSV_File_Loader_obj OBJECT CSV_File_Loader.cc )
set_target_properties(  CSV_File_Loader_obj PROPERTIES POSITION_INDEPENDENT_CODE TRUE )

add_library(            XIM_File_Loader_obj OBJECT XIM_File_Loader.cc )
set_target_properties(  XIM_File_Loader_obj PROPERTIES POSITION_INDEPENDENT_CODE TRUE )

add_library(            SNC_File_Loader_obj OBJECT SNC_File_Loader.cc )
set_target_properties(  SNC_File_Loader_obj PROPERTIES POSITION_INDEPENDENT_CODE TRUE )

add_library(            DVH_File_Loader_obj OBJECT DVH_File_Loader.cc )
set_target_properties(  DVH_File_Loader_obj PROPERTIES POSITION_INDEPENDENT_CODE TRUE )

add_library(            TAR_File_Loader_obj OBJECT TAR_File_Loader.cc )
set_target_properties(  TAR_File_Loader_obj PROPERTIES POSITION_INDEPENDENT_CODE TRUE )

add_library(            3ddose_File_Loader_obj OBJECT 3ddose_File_Loader.cc )
set_target_properties(  3ddose_File_Loader_obj PROPERTIES POSITION_INDEPENDENT_CODE TRUE )

add_library(            OFF_File_Loader_obj OBJECT OFF_File_Loader.cc )
set_target_properties(  OFF_File_Loader_obj PROPERTIES POSITION_INDEPENDENT_CODE TRUE )

add_library(            STL_File_Loader_obj OBJECT STL_File_Loader.cc )
set_target_properties(  STL_File_Loader_obj PROPERTIES POSITION_INDEPENDENT_CODE TRUE )

add_library(            OBJ_File_Loader_obj OBJECT OBJ_File_Loader.cc )
set_target_properties(  OBJ_File_Loader_obj PROPERTIES POSITION_INDEPENDENT_CODE TRUE )

add_library(            PLY_File_Loader_obj OBJECT PLY_File_Loader.cc )
set_target_properties(  PLY_File_Loader_obj PROPERTIES POSITION_INDEPENDENT_CODE TRUE )

add_library(            Line_Sample_File_Loader_obj OBJECT Line_Sample_File_Loader.cc )
set_target_properties(  Line_Sample_File_Loader_obj PROPERTIES POSITION_INDEPENDENT_CODE TRUE )

add_library(            Transformation_File_Loader_obj OBJECT Transformation_File_Loader.cc )
set_target_properties(  Transformation_File_Loader_obj PROPERTIES POSITION_INDEPENDENT_CODE TRUE )

add_library(            Boost_Serialization_File_Loader_obj OBJECT Boost_Serialization_File_Loader.cc 
                                                                   StructsIOBoostSerialization.h )
set_target_properties(  Boost_Serialization_File_Loader_obj PROPERTIES POSITION_INDEPENDENT_CODE TRUE )

add_library(            Script_Loader_obj OBJECT Script_Loader.cc )
set_target_properties(  Script_Loader_obj PROPERTIES POSITION_INDEPENDENT_CODE TRUE )

add_library(            CC_File_Loader_obj OBJECT Contour_Collection_File_Loader.cc )
set_target_properties(  CC_File_Loader_obj PROPERTIES POSITION_INDEPENDENT_CODE TRUE )

add_library(            Standard_Scripts_obj OBJECT Standard_Scripts.cc )
set_target_properties(  Standard_Scripts_obj PROPERTIES POSITION_INDEPENDENT_CODE TRUE )

add_library(            Surface_Meshes_obj OBJECT Surface_Meshes.cc )
set_target_properties(  Surface_Meshes_obj PROPERTIES POSITION_INDEPENDENT_CODE TRUE )

add_library(            Complex_Branching_Meshing_obj OBJECT Complex_Branching_Meshing.cc )
set_target_properties(  Complex_Branching_Meshing_obj PROPERTIES POSITION_INDEPENDENT_CODE TRUE )

add_library(            Simple_Meshing_obj OBJECT Simple_Meshing.cc )
set_target_properties(  Simple_Meshing_obj PROPERTIES POSITION_INDEPENDENT_CODE TRUE )

add_library(            Triple_Three_obj OBJECT Triple_Three.cc)
set_target_properties(  Triple_Three_obj PROPERTIES POSITION_INDEPENDENT_CODE TRUE )

add_library(            Write_File_obj OBJECT Write_File.cc)
set_target_properties(  Write_File_obj PROPERTIES POSITION_INDEPENDENT_CODE TRUE )

add_library(            Operation_Dispatcher_obj OBJECT Operation_Dispatcher.cc )
set_target_properties(  Operation_Dispatcher_obj PROPERTIES POSITION_INDEPENDENT_CODE TRUE )

add_library(            Documentation_obj OBJECT Documentation.cc )
set_target_properties(  Documentation_obj PROPERTIES POSITION_INDEPENDENT_CODE TRUE )

add_library(            Font_DCMA_Minimal_obj OBJECT Font_DCMA_Minimal.cc )
set_target_properties(  Font_DCMA_Minimal_obj PROPERTIES POSITION_INDEPENDENT_CODE TRUE )


FILE(GLOB ygorimaging_functors "./YgorImages_Functors/*/*cc")
add_library( YgorImaging_Functor_objs OBJECT ${ygorimaging_functors} )
set_target_properties( YgorImaging_Functor_objs PROPERTIES POSITION_INDEPENDENT_CODE TRUE )

FILE(GLOB ygorimaging_helpers  "./YgorImages_Functors/*cc")
add_library( YgorImaging_Helper_objs OBJECT ${ygorimaging_helpers} )
set_target_properties( YgorImaging_Helper_objs PROPERTIES POSITION_INDEPENDENT_CODE TRUE )

if(WITH_SDL)
    add_library( IMGui_objs OBJECT imgui20210904/imgui_impl_opengl3.cpp
                                   imgui20210904/imgui_impl_sdl.cpp
                                   imgui20210904/imgui.cpp
                                   imgui20210904/imgui_draw.cpp
                                   imgui20210904/imgui_widgets.cpp 
                                   imgui20210904/imgui_tables.cpp
                                   imgui20210904/imgui_demo.cpp
                                   implot20210904/implot.cpp 
                                   implot20210904/implot_items.cpp
                                   implot20210904/implot_demo.cpp )
    set_target_properties( IMGui_objs PROPERTIES POSITION_INDEPENDENT_CODE TRUE )
endif()


# Imebra library (built separately to maximally compartmentalize the long build process).
add_library (imebrashim 
    Imebra_Shim.cc 
    $<TARGET_OBJECTS:Structs_obj>
    $<TARGET_OBJECTS:Tables_obj>
    $<TARGET_OBJECTS:Alignment_Rigid_obj>
    $<TARGET_OBJECTS:Alignment_Field_obj>
    $<TARGET_OBJECTS:DCMA_DICOM_obj>
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
target_compile_options(imebrashim PUBLIC -w) # Inhibit imebra-related warnings.
target_link_libraries(imebrashim
    explicator 
    ygor 
    Boost::serialization
    Boost::iostreams
    Boost::thread
    Boost::system
    "$<$<BOOL:${MINGW}>:ws2_32>"
    "$<$<BOOL:${MINGW}>:wsock32>"
    z
    "$<$<BOOL:${BUILD_SHARED_LIBS}>:${CMAKE_DL_LIBS}>"
    mpfr
    gmp
    m
    Threads::Threads
)

# Dialog library (built separately to minimize header/macro clashes).
add_library(dialogshim
    Dialogs.cc 
    Dialogs/Text_Query.cc
    Dialogs/Tray_Notification.cc
)
set_target_properties(dialogshim PROPERTIES POSITION_INDEPENDENT_CODE TRUE )
target_link_libraries(dialogshim
    "$<$<BOOL:${MINGW}>:ws2_32>"
    "$<$<BOOL:${MINGW}>:wsock32>"
    "$<$<BOOL:${MINGW}>:comctl32>"
    "$<$<BOOL:${MINGW}>:uuid>"
    "$<$<BOOL:${BUILD_SHARED_LIBS}>:${CMAKE_DL_LIBS}>"
    mpfr
    gmp
    m
    Threads::Threads
)


# Pharmacokinetic modeling libraries (built separately for easier reuse).

if(WITH_GNU_GSL)
    add_library (kineticmodel_1c2i_5param_linearinterp_levenbergmarquardt
        KineticModel_1Compartment2Input_5Param_LinearInterp_Common.cc
        KineticModel_1Compartment2Input_5Param_LinearInterp_LevenbergMarquardt.cc
    )
    target_include_directories(kineticmodel_1c2i_5param_linearinterp_levenbergmarquardt SYSTEM PUBLIC ./ )

    add_library (kineticmodel_1c2i_5param_chebyshev_levenbergmarquardt
        KineticModel_1Compartment2Input_5Param_Chebyshev_Common.cc
        KineticModel_1Compartment2Input_5Param_Chebyshev_LevenbergMarquardt.cc
    )
    target_include_directories(kineticmodel_1c2i_5param_chebyshev_levenbergmarquardt SYSTEM PUBLIC ./ )

    add_library (kineticmodel_1c2i_reduced3param_chebyshev_freeformoptimization
        KineticModel_1Compartment2Input_Reduced3Param_Chebyshev_Common.cc
        KineticModel_1Compartment2Input_Reduced3Param_Chebyshev_FreeformOptimization.cc
    )
    target_include_directories(kineticmodel_1c2i_reduced3param_chebyshev_freeformoptimization SYSTEM PUBLIC ./ )

    # Note: GSL is not actually needed for this routine, but it doesn't make much sense to try separate the
    # functionality.
    add_library (kineticmodel_1c2i_5param_chebyshev_freeformoptimization
        KineticModel_1Compartment2Input_5Param_Chebyshev_Common.cc
        KineticModel_1Compartment2Input_5Param_Chebyshev_FreeformOptimization.cc
    )
    target_include_directories(kineticmodel_1c2i_5param_chebyshev_freeformoptimization SYSTEM PUBLIC ./ )

endif()

# Windows icon bundling.
#    "${TOOLCHAIN}-windres" win_res.rc -O coff -o win_res.o
#    "${TOOLCHAIN}-g++" -o dicomautomaton_dispatcher.exe ... win_res.o ...
if(MINGW)
    add_library(               WindowsIcon_obj OBJECT WindowsIcon.rc )
    #set_target_properties(     WindowsIcon_obj PROPERTIES POSITION_INDEPENDENT_CODE TRUE )
    target_include_directories(WindowsIcon_obj PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../artifacts/logos/")
endif()

# Executable.
add_executable (dicomautomaton_dispatcher
    DICOMautomaton_Dispatcher.cc

    $<TARGET_OBJECTS:Structs_obj>
    $<TARGET_OBJECTS:Tables_obj>
    $<TARGET_OBJECTS:Partition_Drover_obj>
    $<TARGET_OBJECTS:Dose_Meld_obj>
    $<TARGET_OBJECTS:BED_Conversion_obj>
    $<TARGET_OBJECTS:Alignment_Rigid_obj>
    $<TARGET_OBJECTS:Alignment_TPSRPM_obj>
    $<TARGET_OBJECTS:Alignment_Field_obj>
    $<TARGET_OBJECTS:Colour_Maps_obj>
    $<TARGET_OBJECTS:Common_Boost_Serialization_obj>
    $<TARGET_OBJECTS:Common_Plotting_obj>
    $<$<BOOL:${WITH_CGAL}>:$<TARGET_OBJECTS:Contour_Boolean_Operations_obj>>
    $<TARGET_OBJECTS:Contour_Collection_Estimates_obj>
    $<TARGET_OBJECTS:Insert_Contours_obj>
    $<TARGET_OBJECTS:Surface_Meshes_obj>
    $<TARGET_OBJECTS:Simple_Meshing_obj>
    $<TARGET_OBJECTS:Complex_Branching_Meshing_obj>
    $<TARGET_OBJECTS:Regex_Selectors_obj>
    $<TARGET_OBJECTS:String_Parsing_obj>
    $<TARGET_OBJECTS:Metadata_obj>
    $<TARGET_OBJECTS:CSG_SDF_obj>
    $<$<BOOL:${WITH_SDL}>:$<TARGET_OBJECTS:IMGui_objs>>
    $<$<BOOL:${WITH_POSTGRES}>:$<TARGET_OBJECTS:PACS_Loader_obj>>
    $<TARGET_OBJECTS:File_Loader_obj>
    $<TARGET_OBJECTS:Boost_Serialization_File_Loader_obj>
    $<TARGET_OBJECTS:DICOM_File_Loader_obj>
    $<TARGET_OBJECTS:Lexicon_Loader_obj>
    $<TARGET_OBJECTS:FITS_File_Loader_obj>
    $<TARGET_OBJECTS:XYZ_File_Loader_obj>
    $<TARGET_OBJECTS:XIM_File_Loader_obj>
    $<TARGET_OBJECTS:SNC_File_Loader_obj>
    $<TARGET_OBJECTS:CSV_File_Loader_obj>
    $<TARGET_OBJECTS:DVH_File_Loader_obj>
    $<TARGET_OBJECTS:TAR_File_Loader_obj>
    $<TARGET_OBJECTS:3ddose_File_Loader_obj>
    $<TARGET_OBJECTS:OFF_File_Loader_obj>
    $<TARGET_OBJECTS:STL_File_Loader_obj>
    $<TARGET_OBJECTS:OBJ_File_Loader_obj>
    $<TARGET_OBJECTS:PLY_File_Loader_obj>
    $<TARGET_OBJECTS:Line_Sample_File_Loader_obj>
    $<TARGET_OBJECTS:Transformation_File_Loader_obj>
    $<TARGET_OBJECTS:Script_Loader_obj>
    $<TARGET_OBJECTS:CC_File_Loader_obj>
    $<TARGET_OBJECTS:Standard_Scripts_obj>
    $<TARGET_OBJECTS:Triple_Three_obj>
    $<TARGET_OBJECTS:Write_File_obj>
    $<TARGET_OBJECTS:Operation_Dispatcher_obj>
    $<TARGET_OBJECTS:Documentation_obj>
    $<TARGET_OBJECTS:Font_DCMA_Minimal_obj>

    $<TARGET_OBJECTS:YgorImaging_Functor_objs>
    $<TARGET_OBJECTS:YgorImaging_Helper_objs>

    $<TARGET_OBJECTS:Operations_objs>
    $<$<BOOL:${WITH_THRIFT}>:$<TARGET_OBJECTS:Thrift_objs>>
    $<TARGET_OBJECTS:DCMA_Version_obj>
    $<$<BOOL:${MINGW}>:$<TARGET_OBJECTS:WindowsIcon_obj>>
)
target_link_libraries (dicomautomaton_dispatcher
    imebrashim
    dialogshim
    $<$<BOOL:${WITH_GNU_GSL}>:kineticmodel_1c2i_5param_linearinterp_levenbergmarquardt>
    $<$<BOOL:${WITH_GNU_GSL}>:kineticmodel_1c2i_5param_chebyshev_levenbergmarquardt>
    $<$<BOOL:${WITH_GNU_GSL}>:kineticmodel_1c2i_reduced3param_chebyshev_freeformoptimization>
    $<$<BOOL:${WITH_GNU_GSL}>:kineticmodel_1c2i_5param_chebyshev_freeformoptimization>
    explicator 
    ygor 
    $<$<BOOL:${WITH_CGAL}>:CGAL>
    "$<$<BOOL:${WITH_GNU_GSL}>:${GNU_GSL_LIBRARIES}>"
    $<$<BOOL:${WITH_JANSSON}>:jansson>
    "$<$<BOOL:${WITH_NLOPT}>:${NLOPT_LIBRARIES}>"
    "$<$<BOOL:${WITH_SFML}>:${SFML_LIBRARIES}>"
    "$<$<BOOL:${WITH_SDL}>:$<IF:$<AND:$<BOOL:${MINGW}>,$<NOT:$<BOOL:${BUILD_SHARED_LIBS}>>>,mingw32;SDL2::SDL2main;SDL2::SDL2-static,SDL2::SDL2>>"
    "$<$<BOOL:${WITH_SDL}>:${GLEW_LIBRARIES}>"
    "$<$<BOOL:${WITH_SDL}>:${OPENGL_LIBRARIES}>"
    "$<$<BOOL:${WITH_POSTGRES}>:${POSTGRES_LIBRARIES}>"
    "$<$<BOOL:${WITH_THRIFT}>:${THRIFT_LIBRARIES}>"
    Boost::serialization
    Boost::iostreams
    Boost::thread
    Boost::system
    "$<$<BOOL:${MINGW}>:ws2_32>"
    "$<$<BOOL:${MINGW}>:wsock32>"
    z
    "$<$<BOOL:${BUILD_SHARED_LIBS}>:${CMAKE_DL_LIBS}>"
    mpfr
    gmp
    m
    Threads::Threads
)

if(WITH_WT)
    # Executable.
    add_executable(dicomautomaton_webserver
        DICOMautomaton_WebServer.cc

        $<TARGET_OBJECTS:Structs_obj>
        $<TARGET_OBJECTS:Tables_obj>
        $<TARGET_OBJECTS:Partition_Drover_obj>
        $<TARGET_OBJECTS:Dose_Meld_obj>
        $<TARGET_OBJECTS:BED_Conversion_obj>
        $<TARGET_OBJECTS:Alignment_Rigid_obj>
        $<TARGET_OBJECTS:Alignment_TPSRPM_obj>
        $<TARGET_OBJECTS:Alignment_Field_obj>
        $<TARGET_OBJECTS:Colour_Maps_obj>
        $<TARGET_OBJECTS:Common_Boost_Serialization_obj>
        $<TARGET_OBJECTS:Common_Plotting_obj>
        $<$<BOOL:${WITH_CGAL}>:$<TARGET_OBJECTS:Contour_Boolean_Operations_obj>>
        $<TARGET_OBJECTS:Contour_Collection_Estimates_obj>
        $<TARGET_OBJECTS:Insert_Contours_obj>
        $<TARGET_OBJECTS:Surface_Meshes_obj>
        $<TARGET_OBJECTS:Simple_Meshing_obj>
        $<TARGET_OBJECTS:Complex_Branching_Meshing_obj>
        $<TARGET_OBJECTS:Regex_Selectors_obj>
        $<TARGET_OBJECTS:String_Parsing_obj>
        $<TARGET_OBJECTS:Metadata_obj>
        $<TARGET_OBJECTS:CSG_SDF_obj>
        $<$<BOOL:${WITH_SDL}>:$<TARGET_OBJECTS:IMGui_objs>>
        $<$<BOOL:${WITH_POSTGRES}>:$<TARGET_OBJECTS:PACS_Loader_obj>>
        $<TARGET_OBJECTS:File_Loader_obj>
        $<TARGET_OBJECTS:Boost_Serialization_File_Loader_obj>
        $<TARGET_OBJECTS:DICOM_File_Loader_obj>
        $<TARGET_OBJECTS:Lexicon_Loader_obj>
        $<TARGET_OBJECTS:FITS_File_Loader_obj>
        $<TARGET_OBJECTS:XYZ_File_Loader_obj>
        $<TARGET_OBJECTS:XIM_File_Loader_obj>
        $<TARGET_OBJECTS:SNC_File_Loader_obj>
        $<TARGET_OBJECTS:CSV_File_Loader_obj>
        $<TARGET_OBJECTS:DVH_File_Loader_obj>
        $<TARGET_OBJECTS:TAR_File_Loader_obj>
        $<TARGET_OBJECTS:3ddose_File_Loader_obj>
        $<TARGET_OBJECTS:OFF_File_Loader_obj>
        $<TARGET_OBJECTS:STL_File_Loader_obj>
        $<TARGET_OBJECTS:OBJ_File_Loader_obj>
        $<TARGET_OBJECTS:PLY_File_Loader_obj>
        $<TARGET_OBJECTS:Line_Sample_File_Loader_obj>
        $<TARGET_OBJECTS:Transformation_File_Loader_obj>
        $<TARGET_OBJECTS:Script_Loader_obj>
        $<TARGET_OBJECTS:CC_File_Loader_obj>
        $<TARGET_OBJECTS:Standard_Scripts_obj>
        $<TARGET_OBJECTS:Triple_Three_obj>
        $<TARGET_OBJECTS:Write_File_obj>
        $<TARGET_OBJECTS:Operation_Dispatcher_obj>
        $<TARGET_OBJECTS:Documentation_obj>
        $<TARGET_OBJECTS:Font_DCMA_Minimal_obj>

        $<TARGET_OBJECTS:YgorImaging_Functor_objs>
        $<TARGET_OBJECTS:YgorImaging_Helper_objs>

        $<TARGET_OBJECTS:Operations_objs>
        $<$<BOOL:${WITH_THRIFT}>:$<TARGET_OBJECTS:Thrift_objs>>
        $<TARGET_OBJECTS:DCMA_Version_obj>
    )
    target_link_libraries(dicomautomaton_webserver
        imebrashim
        dialogshim
        $<$<BOOL:${WITH_GNU_GSL}>:kineticmodel_1c2i_5param_linearinterp_levenbergmarquardt>
        $<$<BOOL:${WITH_GNU_GSL}>:kineticmodel_1c2i_5param_chebyshev_levenbergmarquardt>
        $<$<BOOL:${WITH_GNU_GSL}>:kineticmodel_1c2i_reduced3param_chebyshev_freeformoptimization>
        $<$<BOOL:${WITH_GNU_GSL}>:kineticmodel_1c2i_5param_chebyshev_freeformoptimization>
        explicator 
        ygor 
        $<$<BOOL:${WITH_CGAL}>:CGAL>
        "$<$<BOOL:${WITH_GNU_GSL}>:${GNU_GSL_LIBRARIES}>"
        $<$<BOOL:${WITH_JANSSON}>:jansson>
        "$<$<BOOL:${WITH_NLOPT}>:${NLOPT_LIBRARIES}>"
        "$<$<BOOL:${WITH_SFML}>:${SFML_LIBRARIES}>"
        "$<$<BOOL:${WITH_SDL}>:$<IF:$<AND:$<BOOL:${MINGW}>,$<NOT:$<BOOL:${BUILD_SHARED_LIBS}>>>,mingw32;SDL2::SDL2main;SDL2::SDL2-static,SDL2::SDL2>>"
        "$<$<BOOL:${WITH_SDL}>:${GLEW_LIBRARIES}>"
        "$<$<BOOL:${WITH_SDL}>:${OPENGL_LIBRARIES}>"
        "$<$<BOOL:${WITH_POSTGRES}>:${POSTGRES_LIBRARIES}>"
        "$<$<BOOL:${WITH_THRIFT}>:${THRIFT_LIBRARIES}>"
        wt
        wthttp
        Boost::serialization
        Boost::iostreams
        Boost::thread
        Boost::system
        "$<$<BOOL:${MINGW}>:ws2_32>"
        "$<$<BOOL:${MINGW}>:wsock32>"
        z
        "$<$<BOOL:${BUILD_SHARED_LIBS}>:${CMAKE_DL_LIBS}>"
        mpfr
        gmp
        m
        Threads::Threads
    )
endif()

# Executable.
add_executable(dicomautomaton_bsarchive_convert
    Boost_Serialization_Archive_Converter.cc
    $<TARGET_OBJECTS:Structs_obj>
    $<TARGET_OBJECTS:Tables_obj>
    $<TARGET_OBJECTS:Dose_Meld_obj>
    $<TARGET_OBJECTS:Common_Boost_Serialization_obj>
    $<TARGET_OBJECTS:Regex_Selectors_obj>
    $<TARGET_OBJECTS:String_Parsing_obj>
    $<TARGET_OBJECTS:Metadata_obj>
    $<TARGET_OBJECTS:Boost_Serialization_File_Loader_obj>
)
target_link_libraries(dicomautomaton_bsarchive_convert
    explicator 
    ygor 
    "$<$<BOOL:${WITH_GNU_GSL}>:${GNU_GSL_LIBRARIES}>"
    "$<$<BOOL:${WITH_NLOPT}>:${NLOPT_LIBRARIES}>"
    Boost::serialization
    Boost::iostreams
    Boost::thread
    Boost::system
    "$<$<BOOL:${MINGW}>:ws2_32>"
    "$<$<BOOL:${MINGW}>:wsock32>"
    z
    m
    Threads::Threads
)

if(WITH_POSTGRES)
    # Executable.
    add_executable(pacs_ingress
        PACS_Ingress.cc
        $<TARGET_OBJECTS:Structs_obj>
        $<TARGET_OBJECTS:Tables_obj>
        $<TARGET_OBJECTS:Dose_Meld_obj>
        $<TARGET_OBJECTS:Regex_Selectors_obj>
        $<TARGET_OBJECTS:String_Parsing_obj>
        $<TARGET_OBJECTS:Metadata_obj>
    )
    target_link_libraries(pacs_ingress
        imebrashim
        explicator 
        ygor 
        "$<$<BOOL:${WITH_GNU_GSL}>:${GNU_GSL_LIBRARIES}>"
        "${POSTGRES_LIBRARIES}"
        m
        Threads::Threads
    )

    # Executable.
    add_executable(pacs_duplicate_cleaner
        PACS_Duplicate_Cleaner.cc
        $<TARGET_OBJECTS:Structs_obj>
        $<TARGET_OBJECTS:Tables_obj>
        $<TARGET_OBJECTS:Dose_Meld_obj>
        $<TARGET_OBJECTS:Regex_Selectors_obj>
        $<TARGET_OBJECTS:String_Parsing_obj>
        $<TARGET_OBJECTS:Metadata_obj>
    )
    target_link_libraries(pacs_duplicate_cleaner
        imebrashim
        explicator 
        ygor 
        "$<$<BOOL:${WITH_GNU_GSL}>:${GNU_GSL_LIBRARIES}>"
        "${POSTGRES_LIBRARIES}"
        m
        Threads::Threads
    )

    # Executable.
    add_executable(pacs_refresh
        PACS_Refresh.cc
        $<TARGET_OBJECTS:Structs_obj>
        $<TARGET_OBJECTS:Tables_obj>
        $<TARGET_OBJECTS:Dose_Meld_obj>
        $<TARGET_OBJECTS:Regex_Selectors_obj>
        $<TARGET_OBJECTS:String_Parsing_obj>
        $<TARGET_OBJECTS:Metadata_obj>
    )
    target_link_libraries(pacs_refresh
        imebrashim
        explicator 
        ygor 
        "$<$<BOOL:${WITH_GNU_GSL}>:${GNU_GSL_LIBRARIES}>"
        "${POSTGRES_LIBRARIES}"
        m
        Threads::Threads
    )
endif()


# Executable.
add_executable(dicomautomaton_dump
    DICOMautomaton_Dump.cc
    $<TARGET_OBJECTS:Structs_obj>
    $<TARGET_OBJECTS:Tables_obj>
    $<TARGET_OBJECTS:Dose_Meld_obj>
    $<TARGET_OBJECTS:Regex_Selectors_obj>
    $<TARGET_OBJECTS:String_Parsing_obj>
    $<TARGET_OBJECTS:Metadata_obj>
)
target_link_libraries(dicomautomaton_dump
    imebrashim
    ygor 
    "$<$<BOOL:${WITH_GNU_GSL}>:${GNU_GSL_LIBRARIES}>"
    m
)

# Installation info.
install(TARGETS dicomautomaton_dispatcher
                dicomautomaton_bsarchive_convert
                dicomautomaton_dump
                imebrashim
                dialogshim
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
if(WITH_WT)
    install(TARGETS dicomautomaton_webserver
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()
if(WITH_POSTGRES)
    install(TARGETS pacs_ingress
                    pacs_duplicate_cleaner
                    pacs_refresh
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()
if(WITH_GNU_GSL)
    install(TARGETS kineticmodel_1c2i_5param_linearinterp_levenbergmarquardt
                    kineticmodel_1c2i_5param_chebyshev_levenbergmarquardt
                    kineticmodel_1c2i_reduced3param_chebyshev_freeformoptimization
                    kineticmodel_1c2i_5param_chebyshev_freeformoptimization
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()

