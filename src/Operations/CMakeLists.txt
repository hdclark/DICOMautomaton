

add_library(Operations_objs OBJECT 
    BoostSerializeDrover.cc
    CopyLastImage.cc
    CT_Liver_Perfusion.cc
    CT_Liver_Perfusion_First_Run.cc
    CT_Liver_Perfusion_Ortho_Views.cc
    CT_Liver_Perfusion_Pharmaco.cc
    ContourSimilarity.cc
    ContouringAides.cc
    ConvertNaNsToAir.cc
    DecimatePixels.cc
    DumpAllOrderedImageMetadataToFile.cc
    DumpAnEncompassedPoint.cc
    DumpFilesPartitionedByTime.cc
    DumpImageMetadataOccurrencesToFile.cc
    DumpPixelValuesOverTimeForAnEncompassedPoint.cc
    DumpROIData.cc
    DumpROIDoseInfo.cc
    DumpROISurfaceMeshes.cc
    GenerateVirtualDataPerfusionV1.cc
    GiveWholeImageArrayABoneWindowLevel.cc
    GiveWholeImageArrayAHeadAndNeckWindowLevel.cc
    GiveWholeImageArrayAThoraxWindowLevel.cc
    GiveWholeImageArrayAnAbdominalWindowLevel.cc
    ImageRoutineTests.cc
    MaxMinPixels.cc
    PlotPerROITimeCourses.cc
    PreFilterEnormousCTValues.cc
    SelectSlicesIntersectingROI.cc
    SFML_Viewer.cc
    UBC3TMRI_DCE.cc
    UBC3TMRI_DCE_Differences.cc
    UBC3TMRI_DCE_Experimental.cc
    UBC3TMRI_IVIM_ADC.cc
)
set_target_properties(Operations_objs
    PROPERTIES POSITION_INDEPENDENT_CODE TRUE
)
#target_include_directories(Operations_obj
#    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
#)


