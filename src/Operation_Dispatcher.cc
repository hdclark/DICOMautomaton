//Operation_Dispatcher.cc - A part of DICOMautomaton 2015, 2016. Written by hal clark.
//
// This routine routes loaded data to/through specified operations.
// Operations can be anything, e.g., analyses, serialization, and visualization.
//

#include <string>    
#include <set> 
#include <map>
#include <list>
#include <functional>
#include <utility>

#include <boost/algorithm/string.hpp> //For boost:iequals().

#include "Structs.h"

#include "Operations/BoostSerializeDrover.h"
#include "Operations/BuildLexiconInteractively.h"
#include "Operations/CopyLastImage.h"
#include "Operations/CT_Liver_Perfusion.h"
#include "Operations/CT_Liver_Perfusion_First_Run.h"
#include "Operations/CT_Liver_Perfusion_Ortho_Views.h"
#include "Operations/CT_Liver_Perfusion_Pharmaco_1Compartment2Input_5Param.h"
#include "Operations/CT_Liver_Perfusion_Pharmaco_1Compartment2Input_Reduced3Param.h"
#include "Operations/ContourSimilarity.h"
#include "Operations/ContouringAides.h"
#include "Operations/ConvertDoseToImage.h"
#include "Operations/ConvertNaNsToAir.h"
#include "Operations/DecimatePixels.h"
#include "Operations/DumpAllOrderedImageMetadataToFile.h"
#include "Operations/DumpAnEncompassedPoint.h"
#include "Operations/DumpFilesPartitionedByTime.h"
#include "Operations/DumpImageMetadataOccurrencesToFile.h"
#include "Operations/DumpPerROIParams_KineticModel_1Compartment2Input_5Param.h"
#include "Operations/DumpPixelValuesOverTimeForAnEncompassedPoint.h"
#include "Operations/DumpROIData.h"
#include "Operations/DumpROIDoseInfo.h"
#include "Operations/DumpROISurfaceMeshes.h"
#include "Operations/GenerateSurfaceMask.h"
#include "Operations/GenerateVirtualDataPerfusionV1.h"
#include "Operations/GiveWholeImageArrayABoneWindowLevel.h"
#include "Operations/GiveWholeImageArrayAHeadAndNeckWindowLevel.h"
#include "Operations/GiveWholeImageArrayAThoraxWindowLevel.h"
#include "Operations/GiveWholeImageArrayAnAbdominalWindowLevel.h"
#include "Operations/ImageRoutineTests.h"
#include "Operations/MaxMinPixels.h"
#include "Operations/PlotPerROITimeCourses.h"
#include "Operations/PreFilterEnormousCTValues.h"
#include "Operations/ContourBasedRayCastDoseAccumulate.h"
#include "Operations/SelectSlicesIntersectingROI.h"
#include "Operations/SFML_Viewer.h"
#include "Operations/Subsegment_ComputeDose_VanLuijk.h"
#include "Operations/UBC3TMRI_DCE.h"
#include "Operations/UBC3TMRI_DCE_Differences.h"
#include "Operations/UBC3TMRI_DCE_Experimental.h"
#include "Operations/UBC3TMRI_IVIM_ADC.h"

#include "Operation_Dispatcher.h"



std::map<std::string, op_packet_t> Known_Operations(void){
    std::map<std::string, op_packet_t> out;

    out["BoostSerializeDrover"] = std::make_pair(OpArgDocBoost_Serialize_Drover, Boost_Serialize_Drover);
    out["BuildLexiconInteractively"] = std::make_pair(OpArgDocBuildLexiconInteractively, BuildLexiconInteractively);
    out["CopyLastImage"] = std::make_pair(OpArgDocCopyLastImage, CopyLastImage);
    out["CT_Liver_Perfusion"] = std::make_pair(OpArgDocCT_Liver_Perfusion, CT_Liver_Perfusion);
    out["CT_Liver_Perfusion_First_Run"] = std::make_pair(OpArgDocCT_Liver_Perfusion_First_Run, CT_Liver_Perfusion_First_Run);
    out["CT_Liver_Perfusion_Ortho_Views"] = std::make_pair(OpArgDocCT_Liver_Perfusion_Ortho_Views , CT_Liver_Perfusion_Ortho_Views );
    out["CT_Liver_Perfusion_Pharmaco_1C2I_5Param"] = std::make_pair(OpArgDocCT_Liver_Perfusion_Pharmaco_1C2I_5Param, 
                                                                    CT_Liver_Perfusion_Pharmaco_1C2I_5Param);
    out["CT_Liver_Perfusion_Pharmaco_1C2I_Reduced3Param"] = std::make_pair(OpArgDocCT_Liver_Perfusion_Pharmaco_1C2I_Reduced3Param, 
                                                                           CT_Liver_Perfusion_Pharmaco_1C2I_Reduced3Param);
    out["ContourSimilarity"] = std::make_pair(OpArgDocContourSimilarity, ContourSimilarity);
    out["ContouringAides"] = std::make_pair(OpArgDocContouringAides, ContouringAides);
    out["ConvertDoseToImage"] = std::make_pair(OpArgDocConvertDoseToImage, ConvertDoseToImage);
    out["ConvertNaNsToAir"] = std::make_pair(OpArgDocConvertNaNsToAir, ConvertNaNsToAir);
    out["DecimatePixels"] = std::make_pair(OpArgDocDecimatePixels, DecimatePixels);
    out["DumpAllOrderedImageMetadataToFile"] = std::make_pair(OpArgDocDumpAllOrderedImageMetadataToFile, DumpAllOrderedImageMetadataToFile);
    out["DumpAnEncompassedPoint"] = std::make_pair(OpArgDocDumpAnEncompassedPoint, DumpAnEncompassedPoint);
    out["DumpFilesPartitionedByTime"] = std::make_pair(OpArgDocDumpFilesPartitionedByTime, DumpFilesPartitionedByTime);
    out["DumpImageMetadataOccurrencesToFile"] = std::make_pair(OpArgDocDumpImageMetadataOccurrencesToFile, DumpImageMetadataOccurrencesToFile);
      
    out["DumpPerROIParams_KineticModel_1C2I_5P"] = std::make_pair(OpArgDocDumpPerROIParams_KineticModel_1Compartment2Input_5Param, 
                                                                  DumpPerROIParams_KineticModel_1Compartment2Input_5Param);
    out["DumpPixelValuesOverTimeForAnEncompassedPoint"] = std::make_pair(OpArgDocDumpPixelValuesOverTimeForAnEncompassedPoint,
                                                                         DumpPixelValuesOverTimeForAnEncompassedPoint);
    out["DumpROIData"] = std::make_pair(OpArgDocDumpROIData, DumpROIData);
    out["DumpROIDoseInfo"] = std::make_pair(OpArgDocDumpROIDoseInfo, DumpROIDoseInfo);
    out["DumpROISurfaceMeshes"] = std::make_pair(OpArgDocDumpROISurfaceMeshes, DumpROISurfaceMeshes);
    out["GenerateSurfaceMask"] = std::make_pair(OpArgDocGenerateSurfaceMask, GenerateSurfaceMask);
    out["GenerateVirtualDataPerfusionV1"] = std::make_pair(OpArgDocGenerateVirtualDataPerfusionV1, GenerateVirtualDataPerfusionV1);
    out["GiveWholeImageArrayABoneWindowLevel"] = std::make_pair(OpArgDocGiveWholeImageArrayABoneWindowLevel, GiveWholeImageArrayABoneWindowLevel);
    out["GiveWholeImageArrayAHeadAndNeckWindowLevel"] = std::make_pair(OpArgDocGiveWholeImageArrayAHeadAndNeckWindowLevel, GiveWholeImageArrayAHeadAndNeckWindowLevel);
    out["GiveWholeImageArrayAThoraxWindowLevel"] = std::make_pair(OpArgDocGiveWholeImageArrayAThoraxWindowLevel, GiveWholeImageArrayAThoraxWindowLevel);
    out["GiveWholeImageArrayAnAbdominalWindowLevel"] = std::make_pair(OpArgDocGiveWholeImageArrayAnAbdominalWindowLevel, GiveWholeImageArrayAnAbdominalWindowLevel);
    out["ImageRoutineTests"] = std::make_pair(OpArgDocImageRoutineTests, ImageRoutineTests);
    out["MaxMinPixels"] = std::make_pair(OpArgDocMaxMinPixels, MaxMinPixels);
    out["ContourBasedRayCastDoseAccumulate"] = std::make_pair(OpArgDocContourBasedRayCastDoseAccumulate, ContourBasedRayCastDoseAccumulate);
    out["PlotPerROITimeCourses"] = std::make_pair(OpArgDocPlotPerROITimeCourses, PlotPerROITimeCourses);
    out["PreFilterEnormousCTValues"] = std::make_pair(OpArgDocPreFilterEnormousCTValues, PreFilterEnormousCTValues);
    out["SelectSlicesIntersectingROI"] = std::make_pair(OpArgDocSelectSlicesIntersectingROI, SelectSlicesIntersectingROI);
    out["SFML_Viewer"] = std::make_pair(OpArgDocSFML_Viewer, SFML_Viewer);
    out["Subsegment_ComputeDose_VanLuijk"] = std::make_pair(OpArgDocSubsegment_ComputeDose_VanLuijk, Subsegment_ComputeDose_VanLuijk);
    out["UBC3TMRI_DCE"] = std::make_pair(OpArgDocUBC3TMRI_DCE, UBC3TMRI_DCE);
    out["UBC3TMRI_DCE_Differences"] = std::make_pair(OpArgDocUBC3TMRI_DCE_Differences, UBC3TMRI_DCE_Differences);
    out["UBC3TMRI_DCE_Experimental"] = std::make_pair(OpArgDocUBC3TMRI_DCE_Experimental, UBC3TMRI_DCE_Experimental);
    out["UBC3TMRI_IVIM_ADC"] = std::make_pair(OpArgDocUBC3TMRI_IVIM_ADC, UBC3TMRI_IVIM_ADC);

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
                    auto OpArgDocs = op_func.second.first();
                    for(const auto &r : OpArgDocs){
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
