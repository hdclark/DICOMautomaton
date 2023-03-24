//Operation_Dispatcher.cc - A part of DICOMautomaton 2015-2021. Written by hal clark.
//
// This routine routes loaded data to/through specified operations.
//

#include <boost/algorithm/string/predicate.hpp>
#include <exception>
#include <functional>
#include <list>
#include <map>
#include <ostream>
#include <stdexcept>
#include <string>    
#include <type_traits>
#include <utility>

#include <Explicator.h>

#include <YgorMisc.h>
#include "YgorLog.h"
#include <YgorString.h>

#include "Structs.h"

#include "Operations/AccumulateRowsColumns.h"
#include "Operations/AnalyzeHistograms.h"
#include "Operations/AnalyzeLightRadFieldCoincidence.h"
#include "Operations/AnalyzePicketFence.h"
#include "Operations/AnalyzeRTPlan.h"
#include "Operations/And.h"
#include "Operations/AnyOf.h"
#include "Operations/ApplyCalibrationCurve.h"
#include "Operations/AutoCropImages.h"
#include "Operations/Average.h"
#include "Operations/BEDConvert.h"
#include "Operations/BoostSerializeDrover.h"
#include "Operations/BuildLexiconInteractively.h"
#include "Operations/ClusterDBSCAN.h"
#include "Operations/CombineMeshes.h"
#include "Operations/CompareMeshes.h"
#include "Operations/ComparePixels.h"
#include "Operations/ContourBasedRayCastDoseAccumulate.h"
#include "Operations/ContourSimilarity.h"
#include "Operations/ContourViaGeometry.h"
#include "Operations/ContourViaThreshold.h"
#include "Operations/ContourVote.h"
#include "Operations/ContourWholeImages.h"
#include "Operations/ContouringAides.h"
#include "Operations/ConvertContoursToMeshes.h"
#include "Operations/ConvertContoursToPoints.h"
#include "Operations/ConvertDoseToImage.h"
#include "Operations/ConvertImageToDose.h"
#include "Operations/ConvertImageToMeshes.h"
#include "Operations/ConvertImageToWarp.h"
#include "Operations/ConvertMeshesToPoints.h"
#include "Operations/ConvertNaNsToAir.h"
#include "Operations/ConvertNaNsToZeros.h"
#include "Operations/ConvertPixelsToPoints.h"
#include "Operations/ConvertWarpToImage.h"
#include "Operations/ConvertWarpToMeshes.h"
#include "Operations/ConvolveImages.h"
#include "Operations/CellularAutomata.h"
#include "Operations/CopyContours.h"
#include "Operations/CopyImages.h"
#include "Operations/CopyLineSamples.h"
#include "Operations/CopyMeshes.h"
#include "Operations/CopyTables.h"
#include "Operations/CopyPoints.h"
#include "Operations/CountVoxels.h"
#include "Operations/CreateCustomContour.h"
#include "Operations/CropImageDoseToROIs.h"
#include "Operations/CropImages.h"
#include "Operations/CropROIDose.h"
#include "Operations/DCEMRI_IAUC.h"
#include "Operations/DCEMRI_Nonparametric_CE.h"
#include "Operations/DICOMExportImagesAsCT.h"
#include "Operations/DICOMExportImagesAsDose.h"
#include "Operations/DICOMExportContours.h"
#include "Operations/DecayDoseOverTimeHalve.h"
#include "Operations/DecayDoseOverTimeJones2014.h"
#include "Operations/DecimatePixels.h"
#include "Operations/DeDuplicateImages.h"
#include "Operations/DeleteContours.h"
#include "Operations/DeleteImages.h"
#include "Operations/DeleteLineSamples.h"
#include "Operations/DeleteMeshes.h"
#include "Operations/DeleteTables.h"
#include "Operations/DeletePoints.h"
#include "Operations/DetectShapes3D.h"
#include "Operations/DrawGeometry.h"
#include "Operations/DroverDebug.h"
#include "Operations/DumpAllOrderedImageMetadataToFile.h"
#include "Operations/DumpAnEncompassedPoint.h"
#include "Operations/DumpFilesPartitionedByTime.h"
#include "Operations/DumpImageMeshes.h"
#include "Operations/DumpImageMetadataOccurrencesToFile.h"
#include "Operations/DumpPixelValuesOverTimeForAnEncompassedPoint.h"
#include "Operations/DumpPlanSummary.h"
#include "Operations/DumpROIContours.h"
#include "Operations/DumpROIData.h"
#include "Operations/DumpROISNR.h"
#include "Operations/DumpRTPlanMetadataOccurrencesToFile.h"
#include "Operations/DumpVoxelDoseInfo.h"
#include "Operations/EvaluateDoseVolumeStats.h"
#include "Operations/EvaluateNTCPModels.h"
#include "Operations/EvaluateTCPModels.h"
#include "Operations/ValidateRTPlan.h"
#include "Operations/ExportFITSImages.h"
#include "Operations/ExportContours.h"
#include "Operations/ExportLineSamples.h"
#include "Operations/ExportSNCImages.h"
#include "Operations/ExportSurfaceMeshes.h"
#include "Operations/ExportSurfaceMeshesOBJ.h"
#include "Operations/ExportSurfaceMeshesPLY.h"
#include "Operations/ExportSurfaceMeshesSTL.h"
#include "Operations/ExportSurfaceMeshesOFF.h"
#include "Operations/ExportTables.h"
#include "Operations/ExportWarps.h"
#include "Operations/ExportPointClouds.h"
#include "Operations/ExtractAlphaBeta.h"
#include "Operations/ExtractImageHistograms.h"
#include "Operations/ExtractPointsWarp.h"
#include "Operations/False.h"
#include "Operations/ForEachDistinct.h"
#include "Operations/ForEachRTPlan.h"
#include "Operations/FVPicketFence.h"
#include "Operations/GenerateCalibrationCurve.h"
#include "Operations/GenerateMeshes.h"
#include "Operations/GenerateSurfaceMask.h"
#include "Operations/GenerateSyntheticImages.h"
#include "Operations/GenerateTable.h"
#include "Operations/GenerateWarp.h"
#include "Operations/GenerateVirtualDataContourViaThresholdTestV1.h"
#include "Operations/GenerateVirtualDataDoseStairsV1.h"
#include "Operations/GenerateVirtualDataImageSphereV1.h"
#include "Operations/GenerateVirtualDataPerfusionV1.h"
#include "Operations/GiveWholeImageArrayABoneWindowLevel.h"
#include "Operations/GiveWholeImageArrayAHeadAndNeckWindowLevel.h"
#include "Operations/GiveWholeImageArrayAThoraxWindowLevel.h"
#include "Operations/GiveWholeImageArrayAnAbdominalWindowLevel.h"
#include "Operations/GiveWholeImageArrayAnAlphaBetaWindowLevel.h"
#include "Operations/GridBasedRayCastDoseAccumulate.h"
#include "Operations/GroupImages.h"
#include "Operations/GrowContours.h"
#include "Operations/HighlightROIs.h"
#include "Operations/IfElse.h"
#include "Operations/Ignore.h"
#include "Operations/ImageRoutineTests.h"
#include "Operations/ImprintImages.h"
#include "Operations/InterpolateSlices.h"
#include "Operations/IsolatedVoxelFilter.h"
#include "Operations/LoadFiles.h"
#include "Operations/LoadFilesInteractively.h"
#include "Operations/LogScale.h"
#include "Operations/MaxMinPixels.h"
#include "Operations/MeldDose.h"
#include "Operations/ModifyContourMetadata.h"
#include "Operations/ModifyImageMetadata.h"
#include "Operations/ModifyParameters.h"
#include "Operations/NegatePixels.h"
#include "Operations/NoOp.h"
#include "Operations/NoneOf.h"
#include "Operations/NormalizeLineSamples.h"
#include "Operations/NormalizePixels.h"
#include "Operations/NotifyUser.h"
#include "Operations/OptimizeStaticBeams.h"
#include "Operations/OrderImages.h"
#include "Operations/PartitionContours.h"
#include "Operations/PerturbPixels.h"
#include "Operations/PlotPerROITimeCourses.h"
#include "Operations/PlotLineSamples.h"
#include "Operations/PointSeparation.h"
#include "Operations/PollDirectories.h"
#include "Operations/Polyominoes.h"
#include "Operations/PreFilterEnormousCTValues.h"
#include "Operations/PruneEmptyImageDoseArrays.h"
#include "Operations/PurgeContours.h"
#include "Operations/QuantizePixels.h"
#include "Operations/QueryUserInteractively.h"
#include "Operations/RankPixels.h"
#include "Operations/ReduceNeighbourhood.h"
#include "Operations/Repeat.h"
#include "Operations/RigidWarpImages.h"
#include "Operations/ScalePixels.h"
#include "Operations/SelectionIsPresent.h"
#include "Operations/SelectSlicesIntersectingROI.h"
#include "Operations/SimplifyContours.h"
#include "Operations/SimplifySurfaceMeshes.h"
#include "Operations/SimulateRadiograph.h"
#include "Operations/Sleep.h"
#include "Operations/SpatialBlur.h"
#include "Operations/SpatialDerivative.h"
#include "Operations/SpatialSharpen.h"
#include "Operations/Subsegment_ComputeDose_VanLuijk.h"
#include "Operations/SubsegmentContours.h"
#include "Operations/SubtractImages.h"
#include "Operations/SupersampleImageGrid.h"
#include "Operations/TabulateImageMetadata.h"
#include "Operations/Terminal_Viewer.h"
#include "Operations/ThresholdImages.h"
#include "Operations/ThresholdOtsu.h"
#include "Operations/Time.h"
#include "Operations/Transaction.h"
#include "Operations/TrimROIDose.h"
#include "Operations/True.h"
#include "Operations/UBC3TMRI_DCE.h"
#include "Operations/UBC3TMRI_DCE_Differences.h"
#include "Operations/UBC3TMRI_DCE_Experimental.h"
#include "Operations/UBC3TMRI_IVIM_ADC.h"
#include "Operations/VolumetricCorrelationDetector.h"
#include "Operations/VolumetricSpatialBlur.h"
#include "Operations/VolumetricSpatialDerivative.h"
#include "Operations/WarpContours.h"
#include "Operations/WarpMeshes.h"
#include "Operations/WarpImages.h"
#include "Operations/WarpPoints.h"
#include "Operations/While.h"

#ifdef DCMA_USE_SDL
    #include "Operations/SDL_Viewer.h"
#endif // DCMA_USE_SDL

#ifdef DCMA_USE_SFML
    #include "Operations/PresentationImage.h"
    #include "Operations/SFML_Viewer.h"
#endif // DCMA_USE_SFML

#ifdef DCMA_USE_EIGEN
    #include "Operations/DetectGrid3D.h"
    #include "Operations/ModelIVIM.h"
    #include "Operations/VoxelRANSAC.h"
    #include "Operations/DecomposeImagesSVD.h"
#endif // DCMA_USE_EIGEN

#ifdef DCMA_USE_GNU_GSL
    #include "Operations/CT_Liver_Perfusion.h"
    #include "Operations/CT_Liver_Perfusion_First_Run.h"
    #include "Operations/CT_Liver_Perfusion_Ortho_Views.h"
    #include "Operations/CT_Liver_Perfusion_Pharmaco_1Compartment2Input_5Param.h"
    #include "Operations/CT_Liver_Perfusion_Pharmaco_1Compartment2Input_Reduced3Param.h"
    #include "Operations/DumpPerROIParams_KineticModel_1Compartment2Input_5Param.h"
#endif // DCMA_USE_GNU_GSL

#ifdef DCMA_USE_CGAL
    #include "Operations/BCCAExtractRadiomicFeatures.h"
    #include "Operations/ContourBooleanOperations.h"
    #include "Operations/ConvertMeshesToContours.h"
    #include "Operations/DumpROISurfaceMeshes.h"
    #include "Operations/ExtractRadiomicFeatures.h"
    #include "Operations/MakeMeshesManifold.h"
    #include "Operations/MinkowskiSum3D.h"
    #include "Operations/RemeshSurfaceMeshes.h"
    #include "Operations/SeamContours.h"
    #include "Operations/SubdivideSurfaceMeshes.h"
    #include "Operations/SurfaceBasedRayCastDoseAccumulate.h"
#endif // DCMA_USE_CGAL

#ifdef DCMA_USE_THRIFT
    #include "Operations/RPCReceive.h"
    #include "Operations/RPCSend.h"
#endif // DCMA_USE_THRIFT

#include "Operation_Dispatcher.h"


std::map<std::string, op_packet_t> Known_Operations(){
    std::map<std::string, op_packet_t> out;

    out["AccumulateRowsColumns"] = std::make_pair(OpArgDocAccumulateRowsColumns, AccumulateRowsColumns);
    out["AnalyzeHistograms"] = std::make_pair(OpArgDocAnalyzeHistograms, AnalyzeHistograms);
    out["AnalyzeLightRadFieldCoincidence"] = std::make_pair(OpArgDocAnalyzeLightRadFieldCoincidence, AnalyzeLightRadFieldCoincidence);
    out["AnalyzePicketFence"] = std::make_pair(OpArgDocAnalyzePicketFence, AnalyzePicketFence);
    out["AnalyzeRTPlan"] = std::make_pair(OpArgDocAnalyzeRTPlan, AnalyzeRTPlan);
    out["And"] = std::make_pair(OpArgDocAnd, And);
    out["AnyOf"] = std::make_pair(OpArgDocAnyOf, AnyOf);
    out["ApplyCalibrationCurve"] = std::make_pair(OpArgDocApplyCalibrationCurve, ApplyCalibrationCurve);
    out["AutoCropImages"] = std::make_pair(OpArgDocAutoCropImages, AutoCropImages);
    out["Average"] = std::make_pair(OpArgDocAverage, Average);
    out["BEDConvert"] = std::make_pair(OpArgDocBEDConvert, BEDConvert);
    out["BoostSerializeDrover"] = std::make_pair(OpArgDocBoost_Serialize_Drover, Boost_Serialize_Drover);
    out["BuildLexiconInteractively"] = std::make_pair(OpArgDocBuildLexiconInteractively, BuildLexiconInteractively);
    out["CellularAutomata"] = std::make_pair(OpArgDocCellularAutomata, CellularAutomata);
    out["ClusterDBSCAN"] = std::make_pair(OpArgDocClusterDBSCAN, ClusterDBSCAN);
    out["CombineMeshes"] = std::make_pair(OpArgDocCombineMeshes, CombineMeshes);
    out["CompareMeshes"] = std::make_pair(OpArgDocCompareMeshes, CompareMeshes);
    out["ComparePixels"] = std::make_pair(OpArgDocComparePixels, ComparePixels);
    out["ContourBasedRayCastDoseAccumulate"] = std::make_pair(OpArgDocContourBasedRayCastDoseAccumulate, ContourBasedRayCastDoseAccumulate);
    out["ContouringAides"] = std::make_pair(OpArgDocContouringAides, ContouringAides);
    out["ContourSimilarity"] = std::make_pair(OpArgDocContourSimilarity, ContourSimilarity);
    out["ContourViaGeometry"] = std::make_pair(OpArgDocContourViaGeometry, ContourViaGeometry);
    out["ContourViaThreshold"] = std::make_pair(OpArgDocContourViaThreshold, ContourViaThreshold);
    out["ContourVote"] = std::make_pair(OpArgDocContourVote, ContourVote);
    out["ContourWholeImages"] = std::make_pair(OpArgDocContourWholeImages, ContourWholeImages);
    out["ConvertContoursToMeshes"] = std::make_pair(OpArgDocConvertContoursToMeshes, ConvertContoursToMeshes);
    out["ConvertContoursToPoints"] = std::make_pair(OpArgDocConvertContoursToPoints, ConvertContoursToPoints);
    out["ConvertDoseToImage"] = std::make_pair(OpArgDocConvertDoseToImage, ConvertDoseToImage);
    out["ConvertImageToDose"] = std::make_pair(OpArgDocConvertImageToDose, ConvertImageToDose);
    out["ConvertImageToMeshes"] = std::make_pair(OpArgDocConvertImageToMeshes, ConvertImageToMeshes);
    out["ConvertImageToWarp"] = std::make_pair(OpArgDocConvertImageToWarp, ConvertImageToWarp);
    out["ConvertMeshesToPoints"] = std::make_pair(OpArgDocConvertMeshesToPoints, ConvertMeshesToPoints);
    out["ConvertNaNsToAir"] = std::make_pair(OpArgDocConvertNaNsToAir, ConvertNaNsToAir);
    out["ConvertNaNsToZeros"] = std::make_pair(OpArgDocConvertNaNsToZeros, ConvertNaNsToZeros);
    out["ConvertPixelsToPoints"] = std::make_pair(OpArgDocConvertPixelsToPoints, ConvertPixelsToPoints);
    out["ConvertWarpToImage"] = std::make_pair(OpArgDocConvertWarpToImage, ConvertWarpToImage);
    out["ConvertWarpToMeshes"] = std::make_pair(OpArgDocConvertWarpToMeshes, ConvertWarpToMeshes);
    out["ConvolveImages"] = std::make_pair(OpArgDocConvolveImages, ConvolveImages);
    out["CopyContours"] = std::make_pair(OpArgDocCopyContours, CopyContours);
    out["CopyImages"] = std::make_pair(OpArgDocCopyImages, CopyImages);
    out["CopyLineSamples"] = std::make_pair(OpArgDocCopyLineSamples, CopyLineSamples);
    out["CopyMeshes"] = std::make_pair(OpArgDocCopyMeshes, CopyMeshes);
    out["CopyTables"] = std::make_pair(OpArgDocCopyTables, CopyTables);
    out["CopyPoints"] = std::make_pair(OpArgDocCopyPoints, CopyPoints);
    out["CountVoxels"] = std::make_pair(OpArgDocCountVoxels, CountVoxels);
    out["CreateCustomContour"] = std::make_pair(OpArgDocCreateCustomContour, CreateCustomContour);
    out["CropImageDoseToROIs"] = std::make_pair(OpArgDocCropImageDoseToROIs, CropImageDoseToROIs);
    out["CropImages"] = std::make_pair(OpArgDocCropImages, CropImages);
    out["CropROIDose"] = std::make_pair(OpArgDocCropROIDose, CropROIDose);
    out["DCEMRI_IAUC"] = std::make_pair(OpArgDocDCEMRI_IAUC, DCEMRI_IAUC);
    out["DCEMRI_Nonparametric_CE"] = std::make_pair(OpArgDocDCEMRI_Nonparametric_CE, DCEMRI_Nonparametric_CE);
    out["DecayDoseOverTimeHalve"] = std::make_pair(OpArgDocDecayDoseOverTimeHalve, DecayDoseOverTimeHalve);
    out["DecayDoseOverTimeJones2014"] = std::make_pair(OpArgDocDecayDoseOverTimeJones2014, DecayDoseOverTimeJones2014);
    out["DecimatePixels"] = std::make_pair(OpArgDocDecimatePixels, DecimatePixels);
    out["DeDuplicateImages"] = std::make_pair(OpArgDocDeDuplicateImages, DeDuplicateImages);
    out["DeleteContours"] = std::make_pair(OpArgDocDeleteContours, DeleteContours);
    out["DeleteImages"] = std::make_pair(OpArgDocDeleteImages, DeleteImages);
    out["DeleteLineSamples"] = std::make_pair(OpArgDocDeleteLineSamples, DeleteLineSamples);
    out["DeleteMeshes"] = std::make_pair(OpArgDocDeleteMeshes, DeleteMeshes);
    out["DeleteTables"] = std::make_pair(OpArgDocDeleteTables, DeleteTables);
    out["DeletePoints"] = std::make_pair(OpArgDocDeletePoints, DeletePoints);
    out["DetectShapes3D"] = std::make_pair(OpArgDocDetectShapes3D, DetectShapes3D);
    out["DICOMExportContours"] = std::make_pair(OpArgDocDICOMExportContours, DICOMExportContours);
    out["DICOMExportImagesAsCT"] = std::make_pair(OpArgDocDICOMExportImagesAsCT, DICOMExportImagesAsCT);
    out["DICOMExportImagesAsDose"] = std::make_pair(OpArgDocDICOMExportImagesAsDose, DICOMExportImagesAsDose);
    out["DrawGeometry"] = std::make_pair(OpArgDocDrawGeometry, DrawGeometry);
    out["DroverDebug"] = std::make_pair(OpArgDocDroverDebug, DroverDebug);
    out["DumpAllOrderedImageMetadataToFile"] = std::make_pair(OpArgDocDumpAllOrderedImageMetadataToFile, DumpAllOrderedImageMetadataToFile);
    out["DumpAnEncompassedPoint"] = std::make_pair(OpArgDocDumpAnEncompassedPoint, DumpAnEncompassedPoint);
    out["DumpFilesPartitionedByTime"] = std::make_pair(OpArgDocDumpFilesPartitionedByTime, DumpFilesPartitionedByTime);
    out["DumpImageMeshes"] = std::make_pair(OpArgDocDumpImageMeshes, DumpImageMeshes);
    out["DumpImageMetadataOccurrencesToFile"] = std::make_pair(OpArgDocDumpImageMetadataOccurrencesToFile, DumpImageMetadataOccurrencesToFile);
    out["DumpPixelValuesOverTimeForAnEncompassedPoint"] = std::make_pair(OpArgDocDumpPixelValuesOverTimeForAnEncompassedPoint, DumpPixelValuesOverTimeForAnEncompassedPoint);
    out["DumpPlanSummary"] = std::make_pair(OpArgDocDumpPlanSummary, DumpPlanSummary);
    out["DumpROIContours"] = std::make_pair(OpArgDocDumpROIContours, DumpROIContours);
    out["DumpROIData"] = std::make_pair(OpArgDocDumpROIData, DumpROIData);
    out["DumpROISNR"] = std::make_pair(OpArgDocDumpROISNR, DumpROISNR);
    out["DumpRTPlanMetadataOccurrencesToFile"] = std::make_pair(OpArgDocDumpRTPlanMetadataOccurrencesToFile, DumpRTPlanMetadataOccurrencesToFile);
    out["DumpVoxelDoseInfo"] = std::make_pair(OpArgDocDumpVoxelDoseInfo, DumpVoxelDoseInfo);
    out["EvaluateDoseVolumeStats"] = std::make_pair(OpArgDocEvaluateDoseVolumeStats, EvaluateDoseVolumeStats);
    out["EvaluateNTCPModels"] = std::make_pair(OpArgDocEvaluateNTCPModels, EvaluateNTCPModels);
    out["EvaluateTCPModels"] = std::make_pair(OpArgDocEvaluateTCPModels, EvaluateTCPModels);
    out["ValidateRTPlan"] = std::make_pair(OpArgDocValidateRTPlan, ValidateRTPlan);
    out["ExportFITSImages"] = std::make_pair(OpArgDocExportFITSImages, ExportFITSImages);
    out["ExportContours"] = std::make_pair(OpArgDocExportContours, ExportContours);
    out["ExportLineSamples"] = std::make_pair(OpArgDocExportLineSamples, ExportLineSamples);
    out["ExportPointClouds"] = std::make_pair(OpArgDocExportPointClouds, ExportPointClouds);
    out["ExportSNCImages"] = std::make_pair(OpArgDocExportSNCImages, ExportSNCImages);
    out["ExportSurfaceMeshesOBJ"] = std::make_pair(OpArgDocExportSurfaceMeshesOBJ, ExportSurfaceMeshesOBJ);
    out["ExportSurfaceMeshesOFF"] = std::make_pair(OpArgDocExportSurfaceMeshesOFF, ExportSurfaceMeshesOFF);
    out["ExportSurfaceMeshesPLY"] = std::make_pair(OpArgDocExportSurfaceMeshesPLY, ExportSurfaceMeshesPLY);
    out["ExportSurfaceMeshes"] = std::make_pair(OpArgDocExportSurfaceMeshes, ExportSurfaceMeshes);
    out["ExportSurfaceMeshesSTL"] = std::make_pair(OpArgDocExportSurfaceMeshesSTL, ExportSurfaceMeshesSTL);
    out["ExportTables"] = std::make_pair(OpArgDocExportTables, ExportTables);
    out["ExportWarps"] = std::make_pair(OpArgDocExportWarps, ExportWarps);
    out["ExtractAlphaBeta"] = std::make_pair(OpArgDocExtractAlphaBeta, ExtractAlphaBeta);
    out["ExtractImageHistograms"] = std::make_pair(OpArgDocExtractImageHistograms, ExtractImageHistograms);
    out["ExtractPointsWarp"] = std::make_pair(OpArgDocExtractPointsWarp, ExtractPointsWarp);
    out["False"] = std::make_pair(OpArgDocFalse, False);
    out["ForEachDistinct"] = std::make_pair(OpArgDocForEachDistinct, ForEachDistinct);
    out["ForEachRTPlan"] = std::make_pair(OpArgDocForEachRTPlan, ForEachRTPlan);
    out["FVPicketFence"] = std::make_pair(OpArgDocFVPicketFence, FVPicketFence);
    out["GenerateCalibrationCurve"] = std::make_pair(OpArgDocGenerateCalibrationCurve, GenerateCalibrationCurve);
    out["GenerateMeshes"] = std::make_pair(OpArgDocGenerateMeshes, GenerateMeshes);
    out["GenerateSurfaceMask"] = std::make_pair(OpArgDocGenerateSurfaceMask, GenerateSurfaceMask);
    out["GenerateSyntheticImages"] = std::make_pair(OpArgDocGenerateSyntheticImages, GenerateSyntheticImages);
    out["GenerateTable"] = std::make_pair(OpArgDocGenerateTable, GenerateTable);
    out["GenerateVirtualDataContourViaThresholdTestV1"] = std::make_pair(OpArgDocGenerateVirtualDataContourViaThresholdTestV1, GenerateVirtualDataContourViaThresholdTestV1);
    out["GenerateVirtualDataDoseStairsV1"] = std::make_pair(OpArgDocGenerateVirtualDataDoseStairsV1, GenerateVirtualDataDoseStairsV1);
    out["GenerateVirtualDataImageSphereV1"] = std::make_pair(OpArgDocGenerateVirtualDataImageSphereV1, GenerateVirtualDataImageSphereV1);
    out["GenerateVirtualDataPerfusionV1"] = std::make_pair(OpArgDocGenerateVirtualDataPerfusionV1, GenerateVirtualDataPerfusionV1);
    out["GenerateWarp"] = std::make_pair(OpArgDocGenerateWarp, GenerateWarp);
    out["GiveWholeImageArrayABoneWindowLevel"] = std::make_pair(OpArgDocGiveWholeImageArrayABoneWindowLevel, GiveWholeImageArrayABoneWindowLevel);
    out["GiveWholeImageArrayAHeadAndNeckWindowLevel"] = std::make_pair(OpArgDocGiveWholeImageArrayAHeadAndNeckWindowLevel, GiveWholeImageArrayAHeadAndNeckWindowLevel);
    out["GiveWholeImageArrayAnAbdominalWindowLevel"] = std::make_pair(OpArgDocGiveWholeImageArrayAnAbdominalWindowLevel, GiveWholeImageArrayAnAbdominalWindowLevel);
    out["GiveWholeImageArrayAnAlphaBetaWindowLevel"] = std::make_pair(OpArgDocGiveWholeImageArrayAnAlphaBetaWindowLevel, GiveWholeImageArrayAnAlphaBetaWindowLevel);
    out["GiveWholeImageArrayAThoraxWindowLevel"] = std::make_pair(OpArgDocGiveWholeImageArrayAThoraxWindowLevel, GiveWholeImageArrayAThoraxWindowLevel);
    out["GridBasedRayCastDoseAccumulate"] = std::make_pair(OpArgDocGridBasedRayCastDoseAccumulate, GridBasedRayCastDoseAccumulate);
    out["GroupImages"] = std::make_pair(OpArgDocGroupImages, GroupImages);
    out["GrowContours"] = std::make_pair(OpArgDocGrowContours, GrowContours);
    out["HighlightROIs"] = std::make_pair(OpArgDocHighlightROIs, HighlightROIs);
    out["IfElse"] = std::make_pair(OpArgDocIfElse, IfElse);
    out["Ignore"] = std::make_pair(OpArgDocIgnore, Ignore);
    out["ImageRoutineTests"] = std::make_pair(OpArgDocImageRoutineTests, ImageRoutineTests);
    out["ImprintImages"] = std::make_pair(OpArgDocImprintImages, ImprintImages);
    out["InterpolateSlices"] = std::make_pair(OpArgDocInterpolateSlices, InterpolateSlices);
    out["IsolatedVoxelFilter"] = std::make_pair(OpArgDocIsolatedVoxelFilter, IsolatedVoxelFilter);
    out["LoadFiles"] = std::make_pair(OpArgDocLoadFiles, LoadFiles);
    out["LoadFilesInteractively"] = std::make_pair(OpArgDocLoadFilesInteractively, LoadFilesInteractively);
    out["LogScale"] = std::make_pair(OpArgDocLogScale, LogScale);
    out["MaxMinPixels"] = std::make_pair(OpArgDocMaxMinPixels, MaxMinPixels);
    out["MeldDose"] = std::make_pair(OpArgDocMeldDose, MeldDose);
    out["ModifyContourMetadata"] = std::make_pair(OpArgDocModifyContourMetadata, ModifyContourMetadata);
    out["ModifyImageMetadata"] = std::make_pair(OpArgDocModifyImageMetadata, ModifyImageMetadata);
    out["ModifyParameters"] = std::make_pair(OpArgDocModifyParameters, ModifyParameters);
    out["NegatePixels"] = std::make_pair(OpArgDocNegatePixels, NegatePixels);
    out["NoneOf"] = std::make_pair(OpArgDocNoneOf, NoneOf);
    out["NoOp"] = std::make_pair(OpArgDocNoOp, NoOp);
    out["NormalizeLineSamples"] = std::make_pair(OpArgDocNormalizeLineSamples, NormalizeLineSamples);
    out["NormalizePixels"] = std::make_pair(OpArgDocNormalizePixels, NormalizePixels);
    out["NotifyUser"] = std::make_pair(OpArgDocNotifyUser, NotifyUser);
    out["OptimizeStaticBeams"] = std::make_pair(OpArgDocOptimizeStaticBeams, OptimizeStaticBeams);
    out["OrderImages"] = std::make_pair(OpArgDocOrderImages, OrderImages);
    out["PartitionContours"] = std::make_pair(OpArgDocPartitionContours, PartitionContours);
    out["PerturbPixels"] = std::make_pair(OpArgDocPerturbPixels, PerturbPixels);
    out["PlotLineSamples"] = std::make_pair(OpArgDocPlotLineSamples, PlotLineSamples);
    out["PlotPerROITimeCourses"] = std::make_pair(OpArgDocPlotPerROITimeCourses, PlotPerROITimeCourses);
    out["PointSeparation"] = std::make_pair(OpArgDocPointSeparation, PointSeparation);
    out["PollDirectories"] = std::make_pair(OpArgDocPollDirectories, PollDirectories);
    out["Polyominoes"] = std::make_pair(OpArgDocPolyominoes, Polyominoes);
    out["PreFilterEnormousCTValues"] = std::make_pair(OpArgDocPreFilterEnormousCTValues, PreFilterEnormousCTValues);
    out["PruneEmptyImageDoseArrays"] = std::make_pair(OpArgDocPruneEmptyImageDoseArrays, PruneEmptyImageDoseArrays);
    out["PurgeContours"] = std::make_pair(OpArgDocPurgeContours, PurgeContours);
    out["QuantizePixels"] = std::make_pair(OpArgDocQuantizePixels, QuantizePixels);
    out["QueryUserInteractively"] = std::make_pair(OpArgDocQueryUserInteractively, QueryUserInteractively);
    out["RankPixels"] = std::make_pair(OpArgDocRankPixels, RankPixels);
    out["ReduceNeighbourhood"] = std::make_pair(OpArgDocReduceNeighbourhood, ReduceNeighbourhood);
    out["Repeat"] = std::make_pair(OpArgDocRepeat, Repeat);
    out["RigidWarpImages"] = std::make_pair(OpArgDocRigidWarpImages, RigidWarpImages);
    out["ScalePixels"] = std::make_pair(OpArgDocScalePixels, ScalePixels);
    out["SelectionIsPresent"] = std::make_pair(OpArgDocSelectionIsPresent, SelectionIsPresent);
    out["SelectSlicesIntersectingROI"] = std::make_pair(OpArgDocSelectSlicesIntersectingROI, SelectSlicesIntersectingROI);
    out["SimplifyContours"] = std::make_pair(OpArgDocSimplifyContours, SimplifyContours);
    out["SimplifySurfaceMeshes"] = std::make_pair(OpArgDocSimplifySurfaceMeshes, SimplifySurfaceMeshes);
    out["SimulateRadiograph"] = std::make_pair(OpArgDocSimulateRadiograph, SimulateRadiograph);
    out["Sleep"] = std::make_pair(OpArgDocSleep, Sleep);
    out["SpatialBlur"] = std::make_pair(OpArgDocSpatialBlur, SpatialBlur);
    out["SpatialDerivative"] = std::make_pair(OpArgDocSpatialDerivative, SpatialDerivative);
    out["SpatialSharpen"] = std::make_pair(OpArgDocSpatialSharpen, SpatialSharpen);
    out["Subsegment_ComputeDose_VanLuijk"] = std::make_pair(OpArgDocSubsegment_ComputeDose_VanLuijk, Subsegment_ComputeDose_VanLuijk);
    out["SubsegmentContours"] = std::make_pair(OpArgDocSubsegmentContours, SubsegmentContours);
    out["SubtractImages"] = std::make_pair(OpArgDocSubtractImages, SubtractImages);
    out["SupersampleImageGrid"] = std::make_pair(OpArgDocSupersampleImageGrid, SupersampleImageGrid);
    out["TabulateImageMetadata"] = std::make_pair(OpArgDocTabulateImageMetadata, TabulateImageMetadata);
    out["Terminal_Viewer"] = std::make_pair(OpArgDocTerminal_Viewer, Terminal_Viewer);
    out["ThresholdImages"] = std::make_pair(OpArgDocThresholdImages, ThresholdImages);
    out["ThresholdOtsu"] = std::make_pair(OpArgDocThresholdOtsu, ThresholdOtsu);
    out["Time"] = std::make_pair(OpArgDocTime, Time);
    out["Transaction"] = std::make_pair(OpArgDocTransaction, Transaction);
    out["TrimROIDose"] = std::make_pair(OpArgDocTrimROIDose, TrimROIDose);
    out["True"] = std::make_pair(OpArgDocTrue, True);
    out["UBC3TMRI_DCE_Differences"] = std::make_pair(OpArgDocUBC3TMRI_DCE_Differences, UBC3TMRI_DCE_Differences);
    out["UBC3TMRI_DCE_Experimental"] = std::make_pair(OpArgDocUBC3TMRI_DCE_Experimental, UBC3TMRI_DCE_Experimental);
    out["UBC3TMRI_DCE"] = std::make_pair(OpArgDocUBC3TMRI_DCE, UBC3TMRI_DCE);
    out["UBC3TMRI_IVIM_ADC"] = std::make_pair(OpArgDocUBC3TMRI_IVIM_ADC, UBC3TMRI_IVIM_ADC);
    out["VolumetricCorrelationDetector"] = std::make_pair(OpArgDocVolumetricCorrelationDetector, VolumetricCorrelationDetector);
    out["VolumetricSpatialBlur"] = std::make_pair(OpArgDocVolumetricSpatialBlur, VolumetricSpatialBlur);
    out["VolumetricSpatialDerivative"] = std::make_pair(OpArgDocVolumetricSpatialDerivative, VolumetricSpatialDerivative);
    out["WarpContours"] = std::make_pair(OpArgDocWarpContours, WarpContours);
    out["WarpImages"] = std::make_pair(OpArgDocWarpImages, WarpImages);
    out["WarpMeshes"] = std::make_pair(OpArgDocWarpMeshes, WarpMeshes);
    out["WarpPoints"] = std::make_pair(OpArgDocWarpPoints, WarpPoints);
    out["While"] = std::make_pair(OpArgDocWhile, While);

#ifdef DCMA_USE_SDL
    out["SDL_Viewer"] = std::make_pair(OpArgDocSDL_Viewer, SDL_Viewer);
#endif // DCMA_USE_SDL

#ifdef DCMA_USE_SFML
    out["PresentationImage"] = std::make_pair(OpArgDocPresentationImage, PresentationImage);
    out["SFML_Viewer"] = std::make_pair(OpArgDocSFML_Viewer, SFML_Viewer);
#endif // DCMA_USE_SFML

#ifdef DCMA_USE_EIGEN
    out["DetectGrid3D"] = std::make_pair(OpArgDocDetectGrid3D, DetectGrid3D);
    out["ModelIVIM"] = std::make_pair(OpArgDocModelIVIM, ModelIVIM);
    out["VoxelRANSAC"] = std::make_pair(OpArgDocVoxelRANSAC, VoxelRANSAC);
    out["DecomposeImagesSVD"] = std::make_pair(OpArgDocDecomposeImagesSVD, DecomposeImagesSVD);
#endif // DCMA_USE_EIGEN

#ifdef DCMA_USE_GNU_GSL
    out["CT_Liver_Perfusion"] = std::make_pair(OpArgDocCT_Liver_Perfusion, CT_Liver_Perfusion);
    out["CT_Liver_Perfusion_First_Run"] = std::make_pair(OpArgDocCT_Liver_Perfusion_First_Run, CT_Liver_Perfusion_First_Run);
    out["CT_Liver_Perfusion_Ortho_Views"] = std::make_pair(OpArgDocCT_Liver_Perfusion_Ortho_Views , CT_Liver_Perfusion_Ortho_Views );
    out["CT_Liver_Perfusion_Pharmaco_1C2I_5Param"] = std::make_pair(OpArgDocCT_Liver_Perfusion_Pharmaco_1C2I_5Param, CT_Liver_Perfusion_Pharmaco_1C2I_5Param);
    out["CT_Liver_Perfusion_Pharmaco_1C2I_Reduced3Param"] = std::make_pair(OpArgDocCT_Liver_Perfusion_Pharmaco_1C2I_Reduced3Param, CT_Liver_Perfusion_Pharmaco_1C2I_Reduced3Param);
    out["DumpPerROIParams_KineticModel_1C2I_5P"] = std::make_pair(OpArgDocDumpPerROIParams_KineticModel_1Compartment2Input_5Param, DumpPerROIParams_KineticModel_1Compartment2Input_5Param);
#endif // DCMA_USE_GNU_GSL

#ifdef DCMA_USE_CGAL
    out["BCCAExtractRadiomicFeatures"] = std::make_pair(OpArgDocBCCAExtractRadiomicFeatures, BCCAExtractRadiomicFeatures);
    out["ContourBooleanOperations"] = std::make_pair(OpArgDocContourBooleanOperations, ContourBooleanOperations);
    out["ConvertMeshesToContours"] = std::make_pair(OpArgDocConvertMeshesToContours, ConvertMeshesToContours);
    out["DumpROISurfaceMeshes"] = std::make_pair(OpArgDocDumpROISurfaceMeshes, DumpROISurfaceMeshes);
    out["ExtractRadiomicFeatures"] = std::make_pair(OpArgDocExtractRadiomicFeatures, ExtractRadiomicFeatures);
    out["MakeMeshesManifold"] = std::make_pair(OpArgDocMakeMeshesManifold, MakeMeshesManifold);
    out["MinkowskiSum3D"] = std::make_pair(OpArgDocMinkowskiSum3D, MinkowskiSum3D);
    out["RemeshSurfaceMeshes"] = std::make_pair(OpArgDocRemeshSurfaceMeshes, RemeshSurfaceMeshes);
    out["SeamContours"] = std::make_pair(OpArgDocSeamContours, SeamContours);
    out["SubdivideSurfaceMeshes"] = std::make_pair(OpArgDocSubdivideSurfaceMeshes, SubdivideSurfaceMeshes);
    out["SurfaceBasedRayCastDoseAccumulate"] = std::make_pair(OpArgDocSurfaceBasedRayCastDoseAccumulate, SurfaceBasedRayCastDoseAccumulate);
#endif // DCMA_USE_CGAL

#ifdef DCMA_USE_THRIFT
    out["RPCReceive"] = std::make_pair(OpArgDocRPCReceive, RPCReceive);
    out["RPCSend"] = std::make_pair(OpArgDocRPCSend, RPCSend);
#endif // DCMA_USE_THRIFT

    return out;
}

std::map<std::string, op_packet_t> Known_Operations_and_Aliases(){
    auto out = Known_Operations();

    // Create a separate map for all listed aliases.
    std::map<std::string, op_packet_t> aliases;
    for(const auto &p : out){
        const auto op_name = p.first;
        auto OpDocs = p.second.first();
        for(const auto &alias : OpDocs.aliases){
            // Wrap the canonical functor to rewrite aliases to both include the canonical name and exclude the alias.
            op_doc_func_t l_op_doc = [op_name,alias,OpDocs](){
                auto l_docs = OpDocs;
                l_docs.aliases.push_back(op_name);
                l_docs.aliases.remove(alias);
                return l_docs;
            };

            aliases[alias] = std::make_pair(l_op_doc, p.second.second);
        }
    }

    out.merge(aliases);
    return out;
}

std::map<std::string, std::string> Operation_Lexicon(){
    // Prepare a lexicon (suitable for an Explicator instance) for performing fuzzy operation name matching.
    auto op_name_mapping = Known_Operations();

    std::map<std::string, std::string> op_name_lex;
    for(const auto &op_func : op_name_mapping){
        const auto op_name = op_func.first;
        op_name_lex[op_name] = op_name;

        auto OpDocs = op_func.second.first();
        for(const auto &alias : OpDocs.aliases){
            op_name_lex[alias] = op_name;
        }
    }

    // Explicit mappings go here.

    // ... TODO ...

    return op_name_lex;
}

bool Operation_Dispatcher( Drover &DICOM_data,
                           std::map<std::string,std::string> &InvocationMetadata,
                           const std::string &FilenameLex,
                           const std::list<OperationArgPkg> &Operations ){

    auto op_name_mapping = Known_Operations();
    Explicator op_name_X( Operation_Lexicon() );

    try{
        for(const auto &OptArgs : Operations){
            auto optargs = OptArgs;

            // Find or estimate the canonical name. If not an exact match, issue a warning.
            const auto user_op_name = optargs.getName();
            const auto canonical_op_name = op_name_X(user_op_name);
            if( op_name_X.last_best_score < 1.0 ){
                YLOGWARN("Selecting operation '" << canonical_op_name << "' because '" << user_op_name << "' not understood");
            }

            bool WasFound = false;
            for(const auto &op_func : op_name_mapping){
                if(boost::iequals(op_func.first, canonical_op_name)){
                    WasFound = true;

                    // Attempt to insert all expected, documented parameters with the default value.
                    //
                    // Note that existing keys will not be replaced.
                    auto OpDocs = op_func.second.first();
                    for(const auto &r : OpDocs.args){
                        if(r.expected) optargs.insert( r.name, r.default_val );
                    }

                    // Perform macro replacement using the parameter table.
                    optargs.visit_opts([&InvocationMetadata](const std::string &key, std::string &val){
                        val = ExpandMacros(val, InvocationMetadata, "$");
                        return;
                    });

                    YLOGINFO("Performing operation '" << op_func.first << "' now..");
                    const bool res = op_func.second.second(DICOM_data,
                                                           optargs,
                                                           InvocationMetadata,
                                                           FilenameLex);
                    if(!res) throw std::runtime_error("Truthiness is false");

                    break;
                }
            }
            if(!WasFound) throw std::invalid_argument("No operation matched '" + optargs.getName() + "'");
        }
    }catch(const std::exception &e){
        YLOGWARN("Analysis failed: '" << e.what() << "'. Aborting remaining analyses");
        return false;
    }

    return true;
}
