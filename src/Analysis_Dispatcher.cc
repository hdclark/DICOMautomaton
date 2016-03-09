//Analysis_Dispatcher.cc - A part of DICOMautomaton 2015, 2016. Written by hal clark.
//
// This routine routes loaded data to/through specified analysis routine(s).
//

#include <string>    
#include <set> 
#include <map>
#include <list>
#include <functional>

#include <boost/algorithm/string.hpp> //For boost:iequals().

#include "Structs.h"

#include "Analyses/CT_Liver_Perfusion.h"
#include "Analyses/CT_Liver_Perfusion_First_Run.h"
#include "Analyses/CT_Liver_Perfusion_Ortho_Views.h"
#include "Analyses/CT_Liver_Perfusion_Pharmaco.h"
#include "Analyses/ContourSimilarity.h"
#include "Analyses/ContouringAides.h"
#include "Analyses/ConvertNaNsToAir.h"
#include "Analyses/DecimatePixels.h"
#include "Analyses/DumpAllOrderedImageMetadataToFile.h"
#include "Analyses/DumpAnEncompassedPoint.h"
#include "Analyses/DumpFilesPartitionedByTime.h"
#include "Analyses/DumpImageMetadataOccurrencesToFile.h"
#include "Analyses/DumpPixelValuesOverTimeForAnEncompassedPoint.h"
#include "Analyses/DumpROIData.h"
#include "Analyses/GiveWholeImageArrayABoneWindowLevel.h"
#include "Analyses/GiveWholeImageArrayAHeadAndNeckWindowLevel.h"
#include "Analyses/GiveWholeImageArrayAThoraxWindowLevel.h"
#include "Analyses/GiveWholeImageArrayAnAbdominalWindowLevel.h"
#include "Analyses/ImageRoutineTests.h"
#include "Analyses/PreFilterEnormousCTValues.h"
#include "Analyses/SFML_Viewer.h"
#include "Analyses/UBC3TMRI_DCE.h"
#include "Analyses/UBC3TMRI_DCE_Differences.h"
#include "Analyses/UBC3TMRI_DCE_Experimental.h"
#include "Analyses/UBC3TMRI_IVIM_ADC.h"

#include "Analysis_Dispatcher.h"

bool Analysis_Dispatcher( Drover &DICOM_data,
                          std::map<std::string,std::string> &InvocationMetadata,
                          std::string &FilenameLex,
                          std::list<std::string> &Operations ){

    //Generate a listing containing the operation names and the corresponding function.
    typedef std::function<Drover(Drover, std::map<std::string,std::string>, std::string)> op_func_t;
    std::map<std::string, op_func_t> op_name_mapping;

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
