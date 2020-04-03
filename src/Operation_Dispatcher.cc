//Operation_Dispatcher.cc - A part of DICOMautomaton 2015, 2016, 2017, 2018, 2019. Written by hal clark.
//
// This routine routes loaded data to/through specified operations.
// Operations can be anything, e.g., analyses, serialization, and visualization.
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

#include <YgorMisc.h>

#include "Structs.h"

#include "Operations/AccumulateRowsColumns.h"
#include "Operations/AlignPoints.h"
#include "Operations/AnalyzeDoseVolumeHistograms.h"
#include "Operations/AnalyzeLightRadFieldCoincidence.h"
#include "Operations/AnalyzePicketFence.h"
#include "Operations/AnalyzeTPlan.h"
#include "Operations/ApplyCalibrationCurve.h"
#include "Operations/AutoCropImages.h"
#include "Operations/Average.h"
#include "Operations/BoostSerializeDrover.h"
#include "Operations/BuildLexiconInteractively.h"
#include "Operations/ClusterDBSCAN.h"
#include "Operations/ComparePixels.h"
#include "Operations/ContourBasedRayCastDoseAccumulate.h"
#include "Operations/ContourSimilarity.h"
#include "Operations/ContourViaGeometry.h"
#include "Operations/ContourVote.h"
#include "Operations/ContourWholeImages.h"
#include "Operations/ContouringAides.h"
#include "Operations/ConvertContoursToPoints.h"
#include "Operations/ConvertDoseToImage.h"
#include "Operations/ConvertImageToDose.h"
#include "Operations/ConvertNaNsToAir.h"
#include "Operations/ConvertNaNsToZeros.h"
#include "Operations/ConvertPixelsToPoints.h"
#include "Operations/ConvolveImages.h"
#include "Operations/CopyImages.h"
#include "Operations/CopyMeshes.h"
#include "Operations/CopyPoints.h"
#include "Operations/CountVoxels.h"
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
#include "Operations/DeleteImages.h"
#include "Operations/DeleteMeshes.h"
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
#include "Operations/DumpROIDoseInfo.h"
#include "Operations/DumpROISNR.h"
#include "Operations/DumpTPlanMetadataOccurrencesToFile.h"
#include "Operations/DumpVoxelDoseInfo.h"
#include "Operations/EQDConvert.h"
#include "Operations/EvaluateDoseVolumeStats.h"
#include "Operations/EvaluateNTCPModels.h"
#include "Operations/EvaluateTCPModels.h"
#include "Operations/ExportFITSImages.h"
#include "Operations/ExportLineSamples.h"
#include "Operations/ExportSurfaceMeshes.h"
#include "Operations/ExportPointClouds.h"
#include "Operations/ExtractDoseVolumeHistograms.h"
#include "Operations/ExtractAlphaBeta.h"
#include "Operations/FVPicketFence.h"
#include "Operations/GenerateCalibrationCurve.h"
#include "Operations/GenerateSurfaceMask.h"
#include "Operations/GenerateSyntheticImages.h"
#include "Operations/GenerateVirtualDataContourViaThresholdTestV1.h"
#include "Operations/GenerateVirtualDataDoseStairsV1.h"
#include "Operations/GenerateVirtualDataImageSphereV1.h"
#include "Operations/GenerateVirtualDataPerfusionV1.h"
#include "Operations/GiveWholeImageArrayABoneWindowLevel.h"
#include "Operations/GiveWholeImageArrayAHeadAndNeckWindowLevel.h"
#include "Operations/GiveWholeImageArrayAThoraxWindowLevel.h"
#include "Operations/GiveWholeImageArrayAnAbdominalWindowLevel.h"
#include "Operations/GridBasedRayCastDoseAccumulate.h"
#include "Operations/GroupImages.h"
#include "Operations/GrowContours.h"
#include "Operations/HighlightROIs.h"
#include "Operations/ImageRoutineTests.h"
#include "Operations/ImprintImages.h"
#include "Operations/InterpolateSlices.h"
#include "Operations/IsolatedVoxelFilter.h"
#include "Operations/LoadFiles.h"
#include "Operations/LogScale.h"
#include "Operations/MaxMinPixels.h"
#include "Operations/MeldDose.h"
#include "Operations/ModifyContourMetadata.h"
#include "Operations/ModifyImageMetadata.h"
#include "Operations/NegatePixels.h"
#include "Operations/NormalizePixels.h"
#include "Operations/OptimizeStaticBeams.h"
#include "Operations/OrderImages.h"
#include "Operations/PartitionContours.h"
#include "Operations/PlotPerROITimeCourses.h"
#include "Operations/PlotLineSamples.h"
#include "Operations/PointSeparation.h"
#include "Operations/PreFilterEnormousCTValues.h"
#include "Operations/PruneEmptyImageDoseArrays.h"
#include "Operations/PurgeContours.h"
#include "Operations/RankPixels.h"
#include "Operations/ReduceNeighbourhood.h"
#include "Operations/ScalePixels.h"
#include "Operations/SelectSlicesIntersectingROI.h"
#include "Operations/SimplifyContours.h"
#include "Operations/SimulateRadiograph.h"
#include "Operations/SpatialBlur.h"
#include "Operations/SpatialDerivative.h"
#include "Operations/SpatialSharpen.h"
#include "Operations/Subsegment_ComputeDose_VanLuijk.h"
#include "Operations/SubsegmentContours.h"
#include "Operations/SubtractImages.h"
#include "Operations/SupersampleImageGrid.h"
#include "Operations/ThresholdImages.h"
#include "Operations/ThresholdOtsu.h"
#include "Operations/TransformContours.h"
#include "Operations/TransformMeshes.h"
#include "Operations/TrimROIDose.h"
#include "Operations/UBC3TMRI_DCE.h"
#include "Operations/UBC3TMRI_DCE_Differences.h"
#include "Operations/UBC3TMRI_DCE_Experimental.h"
#include "Operations/UBC3TMRI_IVIM_ADC.h"
#include "Operations/VolumetricCorrelationDetector.h"
#include "Operations/VolumetricSpatialBlur.h"
#include "Operations/VolumetricSpatialDerivative.h"

#ifdef DCMA_USE_SFML
    #include "Operations/PresentationImage.h"
    #include "Operations/SFML_Viewer.h"
#endif // DCMA_USE_SFML

#ifdef DCMA_USE_EIGEN
    #include "Operations/DetectGrid3D.h"
    #include "Operations/VoxelRANSAC.h"
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
    #include "Operations/ContourViaThreshold.h"
    #include "Operations/ConvertImageToMeshes.h"
    #include "Operations/ConvertMeshesToContours.h"
    #include "Operations/DumpROISurfaceMeshes.h"
    #include "Operations/ExtractRadiomicFeatures.h"
    #include "Operations/MakeMeshesManifold.h"
    #include "Operations/MinkowskiSum3D.h"
    #include "Operations/RemeshSurfaceMeshes.h"
    #include "Operations/SeamContours.h"
    #include "Operations/SimplifySurfaceMeshes.h"
    #include "Operations/SubdivideSurfaceMeshes.h"
    #include "Operations/SurfaceBasedRayCastDoseAccumulate.h"
#endif // DCMA_USE_CGAL

#include "Operation_Dispatcher.h"


std::map<std::string, op_packet_t> Known_Operations(void){
    std::map<std::string, op_packet_t> out;

    out["AccumulateRowsColumns"] = std::make_pair(OpArgDocAccumulateRowsColumns, AccumulateRowsColumns);
    out["AlignPoints"] = std::make_pair(OpArgDocAlignPoints, AlignPoints);
    out["AnalyzeDoseVolumeHistograms"] = std::make_pair(OpArgDocAnalyzeDoseVolumeHistograms, AnalyzeDoseVolumeHistograms);
    out["AnalyzeLightRadFieldCoincidence"] = std::make_pair(OpArgDocAnalyzeLightRadFieldCoincidence, AnalyzeLightRadFieldCoincidence);
    out["AnalyzePicketFence"] = std::make_pair(OpArgDocAnalyzePicketFence, AnalyzePicketFence);
    out["AnalyzeTPlan"] = std::make_pair(OpArgDocAnalyzeTPlan, AnalyzeTPlan);
    out["ApplyCalibrationCurve"] = std::make_pair(OpArgDocApplyCalibrationCurve, ApplyCalibrationCurve);
    out["AutoCropImages"] = std::make_pair(OpArgDocAutoCropImages, AutoCropImages);
    out["Average"] = std::make_pair(OpArgDocAverage, Average);
    out["BoostSerializeDrover"] = std::make_pair(OpArgDocBoost_Serialize_Drover, Boost_Serialize_Drover);
    out["BuildLexiconInteractively"] = std::make_pair(OpArgDocBuildLexiconInteractively, BuildLexiconInteractively);
    out["ClusterDBSCAN"] = std::make_pair(OpArgDocClusterDBSCAN, ClusterDBSCAN);
    out["ComparePixels"] = std::make_pair(OpArgDocComparePixels, ComparePixels);
    out["ContourBasedRayCastDoseAccumulate"] = std::make_pair(OpArgDocContourBasedRayCastDoseAccumulate, ContourBasedRayCastDoseAccumulate);
    out["ContourSimilarity"] = std::make_pair(OpArgDocContourSimilarity, ContourSimilarity);
    out["ContourViaGeometry"] = std::make_pair(OpArgDocContourViaGeometry, ContourViaGeometry);
    out["ContourVote"] = std::make_pair(OpArgDocContourVote, ContourVote);
    out["ContourWholeImages"] = std::make_pair(OpArgDocContourWholeImages, ContourWholeImages);
    out["ContouringAides"] = std::make_pair(OpArgDocContouringAides, ContouringAides);
    out["ConvertContoursToPoints"] = std::make_pair(OpArgDocConvertContoursToPoints, ConvertContoursToPoints);
    out["ConvertDoseToImage"] = std::make_pair(OpArgDocConvertDoseToImage, ConvertDoseToImage);
    out["ConvertImageToDose"] = std::make_pair(OpArgDocConvertImageToDose, ConvertImageToDose);
    out["ConvertNaNsToAir"] = std::make_pair(OpArgDocConvertNaNsToAir, ConvertNaNsToAir);
    out["ConvertNaNsToZeros"] = std::make_pair(OpArgDocConvertNaNsToZeros, ConvertNaNsToZeros);
    out["ConvertPixelsToPoints"] = std::make_pair(OpArgDocConvertPixelsToPoints, ConvertPixelsToPoints);
    out["ConvolveImages"] = std::make_pair(OpArgDocConvolveImages, ConvolveImages);
    out["CopyImages"] = std::make_pair(OpArgDocCopyImages, CopyImages);
    out["CopyMeshes"] = std::make_pair(OpArgDocCopyMeshes, CopyMeshes);
    out["CopyPoints"] = std::make_pair(OpArgDocCopyPoints, CopyPoints);
    out["CountVoxels"] = std::make_pair(OpArgDocCountVoxels, CountVoxels);
    out["CropImageDoseToROIs"] = std::make_pair(OpArgDocCropImageDoseToROIs, CropImageDoseToROIs);
    out["CropImages"] = std::make_pair(OpArgDocCropImages, CropImages);
    out["CropROIDose"] = std::make_pair(OpArgDocCropROIDose, CropROIDose);
    out["DCEMRI_IAUC"] = std::make_pair(OpArgDocDCEMRI_IAUC, DCEMRI_IAUC);
    out["DCEMRI_Nonparametric_CE"] = std::make_pair(OpArgDocDCEMRI_Nonparametric_CE, DCEMRI_Nonparametric_CE);
    out["DICOMExportImagesAsCT"] = std::make_pair(OpArgDocDICOMExportImagesAsCT, DICOMExportImagesAsCT);
    out["DICOMExportImagesAsDose"] = std::make_pair(OpArgDocDICOMExportImagesAsDose, DICOMExportImagesAsDose);
    out["DICOMExportContours"] = std::make_pair(OpArgDocDICOMExportContours, DICOMExportContours);
    out["DecayDoseOverTimeHalve"] = std::make_pair(OpArgDocDecayDoseOverTimeHalve, DecayDoseOverTimeHalve);
    out["DecayDoseOverTimeJones2014"] = std::make_pair(OpArgDocDecayDoseOverTimeJones2014, DecayDoseOverTimeJones2014);
    out["DecimatePixels"] = std::make_pair(OpArgDocDecimatePixels, DecimatePixels);
    out["DeDuplicateImages"] = std::make_pair(OpArgDocDeDuplicateImages, DeDuplicateImages);
    out["DeleteImages"] = std::make_pair(OpArgDocDeleteImages, DeleteImages);
    out["DeleteMeshes"] = std::make_pair(OpArgDocDeleteMeshes, DeleteMeshes);
    out["DeletePoints"] = std::make_pair(OpArgDocDeletePoints, DeletePoints);
    out["DetectShapes3D"] = std::make_pair(OpArgDocDetectShapes3D, DetectShapes3D);
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
    out["DumpROIDoseInfo"] = std::make_pair(OpArgDocDumpROIDoseInfo, DumpROIDoseInfo);
    out["DumpROISNR"] = std::make_pair(OpArgDocDumpROISNR, DumpROISNR);
    out["DumpTPlanMetadataOccurrencesToFile"] = std::make_pair(OpArgDocDumpTPlanMetadataOccurrencesToFile, DumpTPlanMetadataOccurrencesToFile);
    out["DumpVoxelDoseInfo"] = std::make_pair(OpArgDocDumpVoxelDoseInfo, DumpVoxelDoseInfo);
    out["EQDConvert"] = std::make_pair(OpArgDocEQDConvert, EQDConvert);
    out["EvaluateDoseVolumeStats"] = std::make_pair(OpArgDocEvaluateDoseVolumeStats, EvaluateDoseVolumeStats);
    out["EvaluateNTCPModels"] = std::make_pair(OpArgDocEvaluateNTCPModels, EvaluateNTCPModels);
    out["EvaluateTCPModels"] = std::make_pair(OpArgDocEvaluateTCPModels, EvaluateTCPModels);
    out["ExportFITSImages"] = std::make_pair(OpArgDocExportFITSImages, ExportFITSImages);
    out["ExportLineSamples"] = std::make_pair(OpArgDocExportLineSamples, ExportLineSamples);
    out["ExportPointClouds"] = std::make_pair(OpArgDocExportPointClouds, ExportPointClouds);
    out["ExportSurfaceMeshes"] = std::make_pair(OpArgDocExportSurfaceMeshes, ExportSurfaceMeshes);
    out["ExtractDoseVolumeHistograms"] = std::make_pair(OpArgDocExtractDoseVolumeHistograms, ExtractDoseVolumeHistograms);
    out["ExtractAlphaBeta"] = std::make_pair(OpArgDocExtractAlphaBeta, ExtractAlphaBeta);
    out["FVPicketFence"] = std::make_pair(OpArgDocFVPicketFence, FVPicketFence);
    out["GenerateCalibrationCurve"] = std::make_pair(OpArgDocGenerateCalibrationCurve, GenerateCalibrationCurve);
    out["GenerateSurfaceMask"] = std::make_pair(OpArgDocGenerateSurfaceMask, GenerateSurfaceMask);
    out["GenerateSyntheticImages"] = std::make_pair(OpArgDocGenerateSyntheticImages, GenerateSyntheticImages);
    out["GenerateVirtualDataContourViaThresholdTestV1"] = std::make_pair(OpArgDocGenerateVirtualDataContourViaThresholdTestV1, GenerateVirtualDataContourViaThresholdTestV1);
    out["GenerateVirtualDataDoseStairsV1"] = std::make_pair(OpArgDocGenerateVirtualDataDoseStairsV1, GenerateVirtualDataDoseStairsV1);
    out["GenerateVirtualDataImageSphereV1"] = std::make_pair(OpArgDocGenerateVirtualDataImageSphereV1, GenerateVirtualDataImageSphereV1);
    out["GenerateVirtualDataPerfusionV1"] = std::make_pair(OpArgDocGenerateVirtualDataPerfusionV1, GenerateVirtualDataPerfusionV1);
    out["GiveWholeImageArrayABoneWindowLevel"] = std::make_pair(OpArgDocGiveWholeImageArrayABoneWindowLevel, GiveWholeImageArrayABoneWindowLevel);
    out["GiveWholeImageArrayAHeadAndNeckWindowLevel"] = std::make_pair(OpArgDocGiveWholeImageArrayAHeadAndNeckWindowLevel, GiveWholeImageArrayAHeadAndNeckWindowLevel);
    out["GiveWholeImageArrayAThoraxWindowLevel"] = std::make_pair(OpArgDocGiveWholeImageArrayAThoraxWindowLevel, GiveWholeImageArrayAThoraxWindowLevel);
    out["GiveWholeImageArrayAnAbdominalWindowLevel"] = std::make_pair(OpArgDocGiveWholeImageArrayAnAbdominalWindowLevel, GiveWholeImageArrayAnAbdominalWindowLevel);
    out["GridBasedRayCastDoseAccumulate"] = std::make_pair(OpArgDocGridBasedRayCastDoseAccumulate, GridBasedRayCastDoseAccumulate);
    out["GroupImages"] = std::make_pair(OpArgDocGroupImages, GroupImages);
    out["GrowContours"] = std::make_pair(OpArgDocGrowContours, GrowContours);
    out["HighlightROIs"] = std::make_pair(OpArgDocHighlightROIs, HighlightROIs);
    out["ImageRoutineTests"] = std::make_pair(OpArgDocImageRoutineTests, ImageRoutineTests);
    out["ImprintImages"] = std::make_pair(OpArgDocImprintImages, ImprintImages);
    out["InterpolateSlices"] = std::make_pair(OpArgDocInterpolateSlices, InterpolateSlices);
    out["IsolatedVoxelFilter"] = std::make_pair(OpArgDocIsolatedVoxelFilter, IsolatedVoxelFilter);
    out["LoadFiles"] = std::make_pair(OpArgDocLoadFiles, LoadFiles);
    out["LogScale"] = std::make_pair(OpArgDocLogScale, LogScale);
    out["MaxMinPixels"] = std::make_pair(OpArgDocMaxMinPixels, MaxMinPixels);
    out["MeldDose"] = std::make_pair(OpArgDocMeldDose, MeldDose);
    out["ModifyContourMetadata"] = std::make_pair(OpArgDocModifyContourMetadata, ModifyContourMetadata);
    out["ModifyImageMetadata"] = std::make_pair(OpArgDocModifyImageMetadata, ModifyImageMetadata);
    out["NegatePixels"] = std::make_pair(OpArgDocNegatePixels, NegatePixels);
    out["NormalizePixels"] = std::make_pair(OpArgDocNormalizePixels, NormalizePixels);
    out["OptimizeStaticBeams"] = std::make_pair(OpArgDocOptimizeStaticBeams, OptimizeStaticBeams);
    out["OrderImages"] = std::make_pair(OpArgDocOrderImages, OrderImages);
    out["PlotLineSamples"] = std::make_pair(OpArgDocPlotLineSamples, PlotLineSamples);
    out["PartitionContours"] = std::make_pair(OpArgDocPartitionContours, PartitionContours);
    out["PlotPerROITimeCourses"] = std::make_pair(OpArgDocPlotPerROITimeCourses, PlotPerROITimeCourses);
    out["PointSeparation"] = std::make_pair(OpArgDocPointSeparation, PointSeparation);
    out["PreFilterEnormousCTValues"] = std::make_pair(OpArgDocPreFilterEnormousCTValues, PreFilterEnormousCTValues);
    out["PruneEmptyImageDoseArrays"] = std::make_pair(OpArgDocPruneEmptyImageDoseArrays, PruneEmptyImageDoseArrays);
    out["PurgeContours"] = std::make_pair(OpArgDocPurgeContours, PurgeContours);
    out["RankPixels"] = std::make_pair(OpArgDocRankPixels, RankPixels);
    out["ReduceNeighbourhood"] = std::make_pair(OpArgDocReduceNeighbourhood, ReduceNeighbourhood);
    out["ScalePixels"] = std::make_pair(OpArgDocScalePixels, ScalePixels);
    out["SelectSlicesIntersectingROI"] = std::make_pair(OpArgDocSelectSlicesIntersectingROI, SelectSlicesIntersectingROI);
    out["SimplifyContours"] = std::make_pair(OpArgDocSimplifyContours, SimplifyContours);
    out["SimulateRadiograph"] = std::make_pair(OpArgDocSimulateRadiograph, SimulateRadiograph);
    out["SpatialBlur"] = std::make_pair(OpArgDocSpatialBlur, SpatialBlur);
    out["SpatialDerivative"] = std::make_pair(OpArgDocSpatialDerivative, SpatialDerivative);
    out["SpatialSharpen"] = std::make_pair(OpArgDocSpatialSharpen, SpatialSharpen);
    out["SubsegmentContours"] = std::make_pair(OpArgDocSubsegmentContours, SubsegmentContours);
    out["Subsegment_ComputeDose_VanLuijk"] = std::make_pair(OpArgDocSubsegment_ComputeDose_VanLuijk, Subsegment_ComputeDose_VanLuijk);
    out["SubtractImages"] = std::make_pair(OpArgDocSubtractImages, SubtractImages);
    out["SupersampleImageGrid"] = std::make_pair(OpArgDocSupersampleImageGrid, SupersampleImageGrid);
    out["ThresholdImages"] = std::make_pair(OpArgDocThresholdImages, ThresholdImages);
    out["ThresholdOtsu"] = std::make_pair(OpArgDocThresholdOtsu, ThresholdOtsu);
    out["TransformContours"] = std::make_pair(OpArgDocTransformContours, TransformContours);
    out["TransformMeshes"] = std::make_pair(OpArgDocTransformMeshes, TransformMeshes);
    out["TrimROIDose"] = std::make_pair(OpArgDocTrimROIDose, TrimROIDose);
    out["UBC3TMRI_DCE"] = std::make_pair(OpArgDocUBC3TMRI_DCE, UBC3TMRI_DCE);
    out["UBC3TMRI_DCE_Differences"] = std::make_pair(OpArgDocUBC3TMRI_DCE_Differences, UBC3TMRI_DCE_Differences);
    out["UBC3TMRI_DCE_Experimental"] = std::make_pair(OpArgDocUBC3TMRI_DCE_Experimental, UBC3TMRI_DCE_Experimental);
    out["UBC3TMRI_IVIM_ADC"] = std::make_pair(OpArgDocUBC3TMRI_IVIM_ADC, UBC3TMRI_IVIM_ADC);
    out["VolumetricCorrelationDetector"] = std::make_pair(OpArgDocVolumetricCorrelationDetector, VolumetricCorrelationDetector);
    out["VolumetricSpatialBlur"] = std::make_pair(OpArgDocVolumetricSpatialBlur, VolumetricSpatialBlur);
    out["VolumetricSpatialDerivative"] = std::make_pair(OpArgDocVolumetricSpatialDerivative, VolumetricSpatialDerivative);

#ifdef DCMA_USE_SFML
    out["PresentationImage"] = std::make_pair(OpArgDocPresentationImage, PresentationImage);
    out["SFML_Viewer"] = std::make_pair(OpArgDocSFML_Viewer, SFML_Viewer);
#endif // DCMA_USE_SFML

#ifdef DCMA_USE_EIGEN
    out["DetectGrid3D"] = std::make_pair(OpArgDocDetectGrid3D, DetectGrid3D);
    out["VoxelRANSAC"] = std::make_pair(OpArgDocVoxelRANSAC, VoxelRANSAC);
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
    out["ContourViaThreshold"] = std::make_pair(OpArgDocContourViaThreshold, ContourViaThreshold);
    out["ConvertImageToMeshes"] = std::make_pair(OpArgDocConvertImageToMeshes, ConvertImageToMeshes);
    out["ConvertMeshesToContours"] = std::make_pair(OpArgDocConvertMeshesToContours, ConvertMeshesToContours);
    out["DumpROISurfaceMeshes"] = std::make_pair(OpArgDocDumpROISurfaceMeshes, DumpROISurfaceMeshes);
    out["ExtractRadiomicFeatures"] = std::make_pair(OpArgDocExtractRadiomicFeatures, ExtractRadiomicFeatures);
    out["MakeMeshesManifold"] = std::make_pair(OpArgDocMakeMeshesManifold, MakeMeshesManifold);
    out["MinkowskiSum3D"] = std::make_pair(OpArgDocMinkowskiSum3D, MinkowskiSum3D);
    out["RemeshSurfaceMeshes"] = std::make_pair(OpArgDocRemeshSurfaceMeshes, RemeshSurfaceMeshes);
    out["SeamContours"] = std::make_pair(OpArgDocSeamContours, SeamContours);
    out["SimplifySurfaceMeshes"] = std::make_pair(OpArgDocSimplifySurfaceMeshes, SimplifySurfaceMeshes);
    out["SubdivideSurfaceMeshes"] = std::make_pair(OpArgDocSubdivideSurfaceMeshes, SubdivideSurfaceMeshes);
    out["SurfaceBasedRayCastDoseAccumulate"] = std::make_pair(OpArgDocSurfaceBasedRayCastDoseAccumulate, SurfaceBasedRayCastDoseAccumulate);
#endif // DCMA_USE_CGAL

    return out;
}


bool Operation_Dispatcher( Drover &DICOM_data,
                           std::map<std::string,std::string> &InvocationMetadata,
                           std::string &FilenameLex,
                           std::list<OperationArgPkg> &Operations ){

    auto op_name_mapping = Known_Operations();

    try{
        for(const auto &OptArgs : Operations){
            auto optargs = OptArgs;
            bool WasFound = false;
            for(const auto &op_func : op_name_mapping){
                if(boost::iequals(op_func.first,optargs.getName())){
                    WasFound = true;

                    //Attempt to insert all expected, documented parameters with the default value.
                    auto OpDocs = op_func.second.first();
                    for(const auto &r : OpDocs.args){
                        if(r.expected) optargs.insert( r.name, r.default_val );
                    }

                    FUNCINFO("Performing operation '" << op_func.first << "' now..");
                    DICOM_data = op_func.second.second(DICOM_data, optargs, InvocationMetadata, FilenameLex);
                }
            }
            if(!WasFound) throw std::invalid_argument("No operation matched '" + optargs.getName() + "'");
        }
    }catch(const std::exception &e){
        FUNCWARN("Analysis failed: '" << e.what() << "'. Aborting remaining analyses");
        return false;
    }

    return true;
}
