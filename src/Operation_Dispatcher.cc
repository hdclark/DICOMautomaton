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

#include <boost/algorithm/string.hpp> //For boost:iequals().

#include "Structs.h"

#include "Operations/BoostSerializeDrover.h"
#include "Operations/CT_Liver_Perfusion.h"
#include "Operations/CT_Liver_Perfusion_First_Run.h"
#include "Operations/CT_Liver_Perfusion_Ortho_Views.h"
#include "Operations/CT_Liver_Perfusion_Pharmaco.h"
#include "Operations/ContourSimilarity.h"
#include "Operations/ContouringAides.h"
#include "Operations/ConvertNaNsToAir.h"
#include "Operations/DecimatePixels.h"
#include "Operations/DumpAllOrderedImageMetadataToFile.h"
#include "Operations/DumpAnEncompassedPoint.h"
#include "Operations/DumpFilesPartitionedByTime.h"
#include "Operations/DumpImageMetadataOccurrencesToFile.h"
#include "Operations/DumpPixelValuesOverTimeForAnEncompassedPoint.h"
#include "Operations/DumpROIData.h"
#include "Operations/GiveWholeImageArrayABoneWindowLevel.h"
#include "Operations/GiveWholeImageArrayAHeadAndNeckWindowLevel.h"
#include "Operations/GiveWholeImageArrayAThoraxWindowLevel.h"
#include "Operations/GiveWholeImageArrayAnAbdominalWindowLevel.h"
#include "Operations/ImageRoutineTests.h"
#include "Operations/PreFilterEnormousCTValues.h"
#include "Operations/SFML_Viewer.h"
#include "Operations/UBC3TMRI_DCE.h"
#include "Operations/UBC3TMRI_DCE_Differences.h"
#include "Operations/UBC3TMRI_DCE_Experimental.h"
#include "Operations/UBC3TMRI_IVIM_ADC.h"

#include "Operation_Dispatcher.h"

bool Operation_Dispatcher( Drover &DICOM_data,
                           std::map<std::string,std::string> &InvocationMetadata,
                           std::string &FilenameLex,
                           std::list<std::string> &Operations ){

    //Generate a listing containing the operation names and the corresponding function.
    typedef std::function<Drover(Drover, std::map<std::string,std::string>, std::string)> op_func_t;
    std::map<std::string, op_func_t> op_name_mapping;

    op_name_mapping["BoostSerializeDrover"] = Boost_Serialize_Drover;
    op_name_mapping["CT_Liver_Perfusion"] = CT_Liver_Perfusion;
    op_name_mapping["CT_Liver_Perfusion_First_Run"] = CT_Liver_Perfusion_First_Run;
    op_name_mapping["CT_Liver_Perfusion_Ortho_Views"] = CT_Liver_Perfusion_Ortho_Views ;
    op_name_mapping["CT_Liver_Perfusion_Pharmaco"] = CT_Liver_Perfusion_Pharmaco;
    op_name_mapping["ContourSimilarity"] = ContourSimilarity;
    op_name_mapping["ContouringAides"] = ContouringAides;
    op_name_mapping["ConvertNaNsToAir"] = ConvertNaNsToAir;
    op_name_mapping["DecimatePixels"] = DecimatePixels;
    op_name_mapping["DumpAllOrderedImageMetadataToFile"] = DumpAllOrderedImageMetadataToFile;
    op_name_mapping["DumpAnEncompassedPoint"] = DumpAnEncompassedPoint;
    op_name_mapping["DumpFilesPartitionedByTime"] = DumpFilesPartitionedByTime;
    op_name_mapping["DumpImageMetadataOccurrencesToFile"] = DumpImageMetadataOccurrencesToFile;
    op_name_mapping["DumpPixelValuesOverTimeForAnEncompassedPoint"] = DumpPixelValuesOverTimeForAnEncompassedPoint;
    op_name_mapping["DumpROIData"] = DumpROIData;
    op_name_mapping["GiveWholeImageArrayABoneWindowLevel"] = GiveWholeImageArrayABoneWindowLevel;
    op_name_mapping["GiveWholeImageArrayAHeadAndNeckWindowLevel"] = GiveWholeImageArrayAHeadAndNeckWindowLevel;
    op_name_mapping["GiveWholeImageArrayAThoraxWindowLevel"] = GiveWholeImageArrayAThoraxWindowLevel;
    op_name_mapping["GiveWholeImageArrayAnAbdominalWindowLevel"] = GiveWholeImageArrayAnAbdominalWindowLevel;
    op_name_mapping["ImageRoutineTests"] = ImageRoutineTests;
    op_name_mapping["PreFilterEnormousCTValues"] = PreFilterEnormousCTValues;
    op_name_mapping["SFML_Viewer"] = SFML_Viewer;
    op_name_mapping["UBC3TMRI_DCE"] = UBC3TMRI_DCE;
    op_name_mapping["UBC3TMRI_DCE_Differences"] = UBC3TMRI_DCE_Differences;
    op_name_mapping["UBC3TMRI_DCE_Experimental"] = UBC3TMRI_DCE_Experimental;
    op_name_mapping["UBC3TMRI_IVIM_ADC"] = UBC3TMRI_IVIM_ADC;


    try{
        for(const auto &op_req : Operations){
            bool WasFound = false;
            for(const auto &op_func : op_name_mapping){
                if(boost::iequals(op_func.first,op_req)){
                    WasFound = true;
                    FUNCINFO("Performing operation '" << op_func.first << "' now..");
                    DICOM_data = op_func.second(DICOM_data, InvocationMetadata, FilenameLex);
                }
            }
            if(!WasFound) throw std::invalid_argument("No operation matched '" + op_req + "'");
        }
    }catch(const std::exception &e){
        FUNCWARN("Analysis failed: '" << e.what() << "'. Aborting remaining analyses");
        return false;
    }

    return true;
}
