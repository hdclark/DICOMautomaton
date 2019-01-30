#!/bin/bash

# This script is used to inject tab completion into bash for DICOMautomaton.
#
# Source this file to enable tab completion:
#  $> source $0
# or
#  $> . $0
# 
# See 'https://debian-administration.org/article/317/An_introduction_to_bash_completion_part_2' (accessed 20190130) for
# a guide.
#
# Written by Hal Clark, 2019.
#

_dicomautomaton_dispatcher () {
  local cur # The current (incomplete) completion 'word.'
  local prev # The previous 'word.'
  cur="${COMP_WORDS[COMP_CWORD]}"
  prev="${COMP_WORDS[COMP_CWORD-1]}"

  COMPREPLY=() # Possible completions passed back.


  # List all options.
  case "${cur}" in
    -*)
        #~/portable_dcma/portable_dcma -u | grep '^Op' | sed -e "s/[^']*[']\([^']*\)['].*/\1/g"
        COMPREPLY=( $( compgen -W '-h -u -l -d -f -n -s -v -m -o -x \
                                   --help --detailed-usage --lexicon --database-parameters \
                                   --filter-query-file --next-group --standalone \
                                   --virtual-data --metadata --operation --disregard' -- "${cur}" ) )
        return 0
    ;;
  esac


  # List possibilities for specific options.
  case "${prev}" in

    # SQL filter files.
    -f | --filter-query-file)
        COMPREPLY=( $( compgen -f -X '!*sql' -- "${cur}" ) )
        return 0
    ;;

    # Stand-alone files (e.g., DICOM files).
    -s | --standalone)
        COMPREPLY=( $( compgen -f -- "${cur}" ) )
        return 0
    ;;

    # Operation names.
    -o | --operation | -x | --disregard)
        #~/portable_dcma/portable_dcma -u | grep '^Op' | sed -e "s/[^']*[']\([^']*\)['].*/\1/g"
        local operations=$( printf '
                        AccumulateRowsColumns \
                        AnalyzeLightRadFieldCoincidence \
                        AnalyzePicketFence \
                        ApplyCalibrationCurve \
                        AutoCropImages \
                        Average \
                        BCCAExtractRadiomicFeatures \
                        BoostSerializeDrover \
                        BuildLexiconInteractively \
                        CT_Liver_Perfusion \
                        CT_Liver_Perfusion_First_Run \
                        CT_Liver_Perfusion_Ortho_Views \
                        CT_Liver_Perfusion_Pharmaco_1C2I_5Param \
                        CT_Liver_Perfusion_Pharmaco_1C2I_Reduced3Param \
                        ComparePixels \
                        ContourBasedRayCastDoseAccumulate \
                        ContourBooleanOperations \
                        ContourSimilarity \
                        ContourViaThreshold \
                        ContourVote \
                        ContourWholeImages \
                        ContouringAides \
                        ConvertDoseToImage \
                        ConvertImageToDose \
                        ConvertNaNsToAir \
                        ConvertNaNsToZeros \
                        CopyImages \
                        CropImageDoseToROIs \
                        CropImages \
                        CropROIDose \
                        DCEMRI_IAUC \
                        DCEMRI_Nonparametric_CE \
                        DICOMExportImagesAsDose \
                        DecayDoseOverTimeHalve \
                        DecayDoseOverTimeJones2014 \
                        DecimatePixels \
                        DeleteImages \
                        DetectShapes3D \
                        DroverDebug \
                        DumpAllOrderedImageMetadataToFile \
                        DumpAnEncompassedPoint \
                        DumpFilesPartitionedByTime \
                        DumpImageMetadataOccurrencesToFile \
                        DumpPerROIParams_KineticModel_1C2I_5P \
                        DumpPixelValuesOverTimeForAnEncompassedPoint \
                        DumpROIContours \
                        DumpROIData \
                        DumpROIDoseInfo \
                        DumpROISNR \
                        DumpROISurfaceMeshes \
                        DumpVoxelDoseInfo \
                        EQD2Convert \
                        EvaluateDoseVolumeHistograms \
                        EvaluateDoseVolumeStats \
                        EvaluateNTCPModels \
                        EvaluateTCPModels \
                        ExtractRadiomicFeatures \
                        FVPicketFence \
                        GenerateSurfaceMask \
                        GenerateVirtualDataContourViaThresholdTestV1 \
                        GenerateVirtualDataDoseStairsV1 \
                        GenerateVirtualDataPerfusionV1 \
                        GiveWholeImageArrayABoneWindowLevel \
                        GiveWholeImageArrayAHeadAndNeckWindowLevel \
                        GiveWholeImageArrayAThoraxWindowLevel \
                        GiveWholeImageArrayAnAbdominalWindowLevel \
                        GridBasedRayCastDoseAccumulate \
                        GroupImages \
                        GrowContours \
                        HighlightROIs \
                        ImageRoutineTests \
                        LogScale \
                        MaxMinPixels \
                        MeldDose \
                        MinkowskiSum3D \
                        ModifyContourMetadata \
                        ModifyImageMetadata \
                        NegatePixels \
                        OptimizeStaticBeams \
                        OrderImages \
                        PlotPerROITimeCourses \
                        PreFilterEnormousCTValues \
                        PresentationImage \
                        PruneEmptyImageDoseArrays \
                        PurgeContours \
                        RankPixels \
                        SFML_Viewer \
                        SeamContours \
                        SelectSlicesIntersectingROI \
                        SimplifyContours \
                        SpatialBlur \
                        SpatialDerivative \
                        SpatialSharpen \
                        Subsegment_ComputeDose_VanLuijk \
                        SubtractImages \
                        SupersampleImageGrid \
                        SurfaceBasedRayCastDoseAccumulate \
                        ThresholdImages \
                        TrimROIDose \
                        UBC3TMRI_DCE \
                        UBC3TMRI_DCE_Differences \
                        UBC3TMRI_DCE_Experimental \
                        UBC3TMRI_IVIM_ADC \
                        ')
        COMPREPLY=( $( compgen -W "${operations}" -- "${cur}" ) )
        return 0
    ;;

    # If nothing else applies, the user can usually specify a filename...
    *)
        COMPREPLY=( $( compgen -f -- "${cur}" ) )
        return 0
    ;;

  esac

  #printf -- "${COMPREPLY[@]}\n"

  return 0
}

#complete -F _dicomautomaton_dispatcher -o filenames dicomautomaton_dispatcher
complete -F _dicomautomaton_dispatcher dicomautomaton_dispatcher
complete -F _dicomautomaton_dispatcher portable_dcma

