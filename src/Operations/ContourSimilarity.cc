//ContourSimilarity.cc - A part of DICOMautomaton 2015, 2016. Written by hal clark.

#include <any>
#include <fstream>
#include <functional>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>    
#include <vector>

#include <boost/interprocess/creation_tags.hpp>
#include <boost/interprocess/sync/named_mutex.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>

#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorString.h"       //Needed for GetFirstRegex(...)
#include "YgorFilesDirs.h"

#include "Explicator.h"

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../YgorImages_Functors/Compute/Contour_Similarity.h"

#include "ContourSimilarity.h"

OperationDoc OpArgDocContourSimilarity(){
    OperationDoc out;
    out.name = "ContourSimilarity";
    out.desc = 
        "This operation estimates the similarity or overlap between two sets of contours."
        " The comparison is based on point samples. It is useful for comparing contouring styles."
        " This operation currently reports Dice and Jaccard similarity metrics.";

    out.notes.emplace_back(
        "This routine requires an image grid, which is used to control where the contours are sampled."
        " Images are not modified."
    );

    out.args.emplace_back();
    out.args.back() = IAWhitelistOpArgDoc();
    out.args.back().name = "ImageSelection";
    out.args.back().default_val = "last";

    out.args.emplace_back();
    out.args.back().name = "NormalizedROILabelRegexA";
    out.args.back().desc = "A regex matching ROI labels/names to consider. The default will match"
                           " all available ROIs. Be aware that input spaces are trimmed to a single space."
                           " If your ROI name has more than two sequential spaces, use regex to avoid them."
                           " All ROIs have to match the single regex, so use the 'or' token if needed."
                           " Regex is case insensitive and uses extended POSIX syntax.";
    out.args.back().default_val = ".*";
    out.args.back().expected = true;
    out.args.back().examples = { ".*", ".*Body.*", "Body", "Gross_Liver",
                            R"***(.*Left.*Parotid.*|.*Right.*Parotid.*|.*Eye.*)***",
                            R"***(Left Parotid|Right Parotid)***" };

    out.args.emplace_back();
    out.args.back().name = "ROILabelRegexA";
    out.args.back().desc = "A regex matching ROI labels/names to consider. The default will match"
                           " all available ROIs. Be aware that input spaces are trimmed to a single space."
                           " If your ROI name has more than two sequential spaces, use regex to avoid them."
                           " All ROIs have to match the single regex, so use the 'or' token if needed."
                           " Regex is case insensitive and uses extended POSIX syntax.";
    out.args.back().default_val = ".*";
    out.args.back().expected = true;
    out.args.back().examples = { ".*", ".*body.*", "body", "Gross_Liver",
                                 R"***(.*left.*parotid.*|.*right.*parotid.*|.*eyes.*)***",
                                 R"***(left_parotid|right_parotid)***" };

    out.args.emplace_back();
    out.args.back().name = "NormalizedROILabelRegexB";
    out.args.back().desc = "A regex matching ROI labels/names to consider. The default will match"
                           " all available ROIs. Be aware that input spaces are trimmed to a single space."
                           " If your ROI name has more than two sequential spaces, use regex to avoid them."
                           " All ROIs have to match the single regex, so use the 'or' token if needed."
                           " Regex is case insensitive and uses extended POSIX syntax.";
    out.args.back().default_val = ".*";
    out.args.back().expected = true;
    out.args.back().examples = { ".*", ".*Body.*", "Body", "Gross_Liver",
                            R"***(.*Left.*Parotid.*|.*Right.*Parotid.*|.*Eye.*)***",
                            R"***(Left Parotid|Right Parotid)***" };

    out.args.emplace_back();
    out.args.back().name = "ROILabelRegexB";
    out.args.back().desc = "A regex matching ROI labels/names to consider. The default will match"
                           " all available ROIs. Be aware that input spaces are trimmed to a single space."
                           " If your ROI name has more than two sequential spaces, use regex to avoid them."
                           " All ROIs have to match the single regex, so use the 'or' token if needed."
                           " Regex is case insensitive and uses extended POSIX syntax.";
    out.args.back().default_val = ".*";
    out.args.back().expected = true;
    out.args.back().examples = { ".*", ".*body.*", "body", "Gross_Liver",
                                 R"***(.*left.*parotid.*|.*right.*parotid.*|.*eyes.*)***",
                                 R"***(left_parotid|right_parotid)***" };

    out.args.emplace_back();
    out.args.back().name = "FileName";
    out.args.back().desc = "A filename (or full path) in which to append similarity data generated by this routine."
                           " The format is CSV. Leave empty to dump to generate a unique temporary file.";
    out.args.back().default_val = "";
    out.args.back().expected = true;
    out.args.back().examples = { "", "/tmp/somefile", "localfile.csv", "derivative_data.csv" };
    out.args.back().mimetype = "text/csv";

    out.args.emplace_back();
    out.args.back().name = "UserComment";
    out.args.back().desc = "A string that will be inserted into the output file which will simplify merging output"
                           " with differing parameters, from different sources, or using sub-selections of the data.";
    out.args.back().default_val = "";
    out.args.back().expected = true;
    out.args.back().examples = { "", "Using XYZ", "Patient treatment plan C" };

    return out;
}

Drover ContourSimilarity(Drover DICOM_data,
                         const OperationArgPkg& OptArgs,
                         const std::map<std::string, std::string>&
                         /*InvocationMetadata*/,
                         const std::string& FilenameLex){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();

    const auto NormalizedROILabelRegexA = OptArgs.getValueStr("NormalizedROILabelRegexA").value();
    const auto ROILabelRegexA = OptArgs.getValueStr("ROILabelRegexA").value();
    const auto NormalizedROILabelRegexB = OptArgs.getValueStr("NormalizedROILabelRegexB").value();
    const auto ROILabelRegexB = OptArgs.getValueStr("ROILabelRegexB").value();

    auto FileName = OptArgs.getValueStr("FileName").value();
    const auto UserComment = OptArgs.getValueStr("UserComment");
    //-----------------------------------------------------------------------------------------------------------------
    Explicator X(FilenameLex);

    auto cc_all = All_CCs( DICOM_data );

    auto cc_A = Whitelist( cc_all, { { "ROIName", ROILabelRegexA },
                                     { "NormalizedROIName", NormalizedROILabelRegexA } } );
    if(cc_A.empty()){
        throw std::invalid_argument("No contours selected (A). Cannot continue.");
    }
    if(cc_A.size() != 1){
        throw std::invalid_argument("Multiple contour collections selected (A). Refusing to continue.");
    }

    auto cc_B = Whitelist( cc_all, { { "ROIName", ROILabelRegexB },
                                     { "NormalizedROIName", NormalizedROILabelRegexB } } );
    if(cc_B.empty()){
        throw std::invalid_argument("No contours selected (B). Cannot continue.");
    }
    if(cc_B.size() != 1){
        throw std::invalid_argument("Multiple contour collections selected (B). Refusing to continue.");
    }

    auto IAs_all = All_IAs( DICOM_data );
    auto IAs = Whitelist( IAs_all, ImageSelectionStr );
    if(IAs.empty()){
        throw std::invalid_argument("No image arrays selected. Cannot continue.");
    }
    if(IAs.size() != 1){
        throw std::invalid_argument("Multiple image arrays selected. Cannot continue.");
    }
    auto iap_it = IAs.front();
    ComputeContourSimilarityUserData ud;
    ud.Clear();
    if(!(*iap_it)->imagecoll.Compute_Images( ComputeContourSimilarity, { },
                                           { cc_A.front(), cc_B.front() }, &ud )){
        throw std::runtime_error("Unable to compute contour similarity metrics. Cannot continue.");
    }
    FUNCINFO("Dice coefficient(A,B) = " << ud.Dice_Coefficient());
    FUNCINFO("Jaccard coefficient(A,B) = " << ud.Jaccard_Coefficient());

    // Attempt to identify the patient for reporting purposes.
    std::string patient_ID;
    if( auto o = cc_A.front().get().contours.front().GetMetadataValueAs<std::string>("PatientID") ){
        patient_ID = o.value();
    }else if( auto o = cc_B.front().get().contours.front().GetMetadataValueAs<std::string>("PatientID") ){
        patient_ID = o.value();
    }else if( auto o = cc_A.front().get().contours.front().GetMetadataValueAs<std::string>("StudyInstanceUID") ){
        patient_ID = o.value();
    }else if( auto o = cc_B.front().get().contours.front().GetMetadataValueAs<std::string>("StudyInstanceUID") ){
        patient_ID = o.value();
    }else{
        patient_ID = "unknown_patient";
    }

    std::string ROINameA;
    if( auto o = cc_A.front().get().contours.front().GetMetadataValueAs<std::string>("ROIName") ){
        ROINameA = o.value();
    }else{
        ROINameA = "unknown_roi";
    }

    std::string ROINameB;
    if( auto o = cc_B.front().get().contours.front().GetMetadataValueAs<std::string>("ROIName") ){
        ROINameB = o.value();
    }else{
        ROINameB = "unknown_roi";
    }

    //Report the findings. 
    FUNCINFO("Attempting to claim a mutex");

    {
        //File-based locking is used so this program can be run over many patients concurrently.
        //
        //Try open a named mutex. Probably created in /dev/shm/ if you need to clear it manually...
        boost::interprocess::named_mutex mutex(boost::interprocess::open_or_create,
                                               "dicomautomaton_operation_contoursimilarity_mutex");
        boost::interprocess::scoped_lock<boost::interprocess::named_mutex> lock(mutex);

        if(FileName.empty()){
            FileName = Get_Unique_Sequential_Filename("/tmp/dicomautomaton_contoursimilarity_", 6, ".csv");
        }
        const auto FirstWrite = !Does_File_Exist_And_Can_Be_Read(FileName);
        std::fstream FO(FileName, std::fstream::out | std::fstream::app);
        if(!FO){
            throw std::runtime_error("Unable to open file for reporting similarity. Cannot continue.");
        }
        if(FirstWrite){ // Write a CSV header.
            FO << "UserComment,"
               << "PatientID,"
               << "ROInameA,"
               << "NormalizedROInameA,"
               << "ROInameB,"
               << "NormalizedROInameB,"
               << "DiceSimilarity,"
               << "JaccardSimilarity"
               << std::endl;
        }
        FO << UserComment.value_or("") << ","
           << patient_ID        << ","
           << ROINameA          << ","
           << X(ROINameA)       << ","
           << ROINameB          << ","
           << X(ROINameB)       << ","
           << ud.Dice_Coefficient() << ","
           << ud.Jaccard_Coefficient()
           << std::endl;
        FO.flush();
        FO.close();

    }

    return DICOM_data;
}
