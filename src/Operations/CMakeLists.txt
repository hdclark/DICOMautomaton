

add_library(Operations_objs OBJECT 
    BoostSerializeDrover.cc
    BuildLexiconInteractively.cc
    ConvertDoseToImage.cc
    CopyLastImage.cc
    CropImageDoseToROIs.cc
    CT_Liver_Perfusion.cc
    CT_Liver_Perfusion_First_Run.cc
    CT_Liver_Perfusion_Ortho_Views.cc
    CT_Liver_Perfusion_Pharmaco_1Compartment2Input_5Param.cc
    CT_Liver_Perfusion_Pharmaco_1Compartment2Input_Reduced3Param.cc
    ContourSimilarity.cc
    ContouringAides.cc
    ConvertNaNsToAir.cc
    DecimatePixels.cc
    DumpAllOrderedImageMetadataToFile.cc
    DumpAnEncompassedPoint.cc
    DumpFilesPartitionedByTime.cc
    DumpImageMetadataOccurrencesToFile.cc
    DumpPerROIParams_KineticModel_1Compartment2Input_5Param.cc
    DumpPixelValuesOverTimeForAnEncompassedPoint.cc
    DumpROIContours.cc
    DumpROIData.cc
    DumpROIDoseInfo.cc
    DumpROISurfaceMeshes.cc
    GenerateSurfaceMask.cc
    GenerateVirtualDataPerfusionV1.cc
    GiveWholeImageArrayABoneWindowLevel.cc
    GiveWholeImageArrayAHeadAndNeckWindowLevel.cc
    GiveWholeImageArrayAThoraxWindowLevel.cc
    GiveWholeImageArrayAnAbdominalWindowLevel.cc
    GridBasedRayCastDoseAccumulate.cc
    ImageRoutineTests.cc
    MaxMinPixels.cc
    PlotPerROITimeCourses.cc
    PreFilterEnormousCTValues.cc
    ContourBasedRayCastDoseAccumulate.cc
    SelectSlicesIntersectingROI.cc
    SFML_Viewer.cc
    Subsegment_ComputeDose_VanLuijk.cc
    SupersampleImageGrid.cc
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


