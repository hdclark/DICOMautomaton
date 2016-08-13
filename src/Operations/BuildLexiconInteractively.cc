//BuildLexiconInteractively.cc - A part of DICOMautomaton 2016. Written by hal clark.

#include <iostream>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <string>    
#include <vector>
#include <set> 
#include <map>
#include <unordered_map>
#include <list>
#include <functional>
#include <thread>
#include <array>
#include <mutex>
#include <limits>
#include <cmath>

#include <getopt.h>           //Needed for 'getopts' argument parsing.
#include <cstdlib>            //Needed for exit() calls.
#include <utility>            //Needed for std::pair.
#include <algorithm>
#include <experimental/optional>

#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMathPlottingGnuplot.h" //Needed for YgorMathPlottingGnuplot::*.
#include "YgorMathChebyshev.h" //Needed for cheby_approx class.
#include "YgorStats.h"        //Needed for Stats:: namespace.
#include "YgorFilesDirs.h"    //Needed for Does_File_Exist_And_Can_Be_Read(...), etc..
#include "YgorContainers.h"   //Needed for bimap class.
#include "YgorPerformance.h"  //Needed for YgorPerformance_dt_from_last().
#include "YgorAlgorithms.h"   //Needed for For_Each_In_Parallel<..>(...)
#include "YgorArguments.h"    //Needed for ArgumentHandler class.
#include "YgorString.h"       //Needed for GetFirstRegex(...)
#include "YgorImages.h"
#include "YgorImagesIO.h"
#include "YgorImagesPlotting.h"

#include "Explicator.h"       //Needed for Explicator class.

#include "../Structs.h"

#include "../YgorImages_Functors/Grouping/Misc_Functors.h"

#include "../YgorImages_Functors/Processing/DCEMRI_AUC_Map.h"
#include "../YgorImages_Functors/Processing/DCEMRI_S0_Map.h"
#include "../YgorImages_Functors/Processing/DCEMRI_T1_Map.h"
#include "../YgorImages_Functors/Processing/Highlight_ROI_Voxels.h"
#include "../YgorImages_Functors/Processing/Kitchen_Sink_Analysis.h"
#include "../YgorImages_Functors/Processing/IVIMMRI_ADC_Map.h"
#include "../YgorImages_Functors/Processing/Time_Course_Slope_Map.h"
#include "../YgorImages_Functors/Processing/CT_Perfusion_Clip_Search.h"
#include "../YgorImages_Functors/Processing/CT_Perf_Pixel_Filter.h"
#include "../YgorImages_Functors/Processing/CT_Convert_NaNs_to_Air.h"
#include "../YgorImages_Functors/Processing/Min_Pixel_Value.h"
#include "../YgorImages_Functors/Processing/Max_Pixel_Value.h"
#include "../YgorImages_Functors/Processing/CT_Reasonable_HU_Window.h"
#include "../YgorImages_Functors/Processing/Slope_Difference.h"
#include "../YgorImages_Functors/Processing/Centralized_Moments.h"
#include "../YgorImages_Functors/Processing/Logarithmic_Pixel_Scale.h"
#include "../YgorImages_Functors/Processing/Per_ROI_Time_Courses.h"
#include "../YgorImages_Functors/Processing/DBSCAN_Time_Courses.h"
#include "../YgorImages_Functors/Processing/In_Image_Plane_Bilinear_Supersample.h"
#include "../YgorImages_Functors/Processing/In_Image_Plane_Bicubic_Supersample.h"
#include "../YgorImages_Functors/Processing/In_Image_Plane_Pixel_Decimate.h"
#include "../YgorImages_Functors/Processing/Cross_Second_Derivative.h"
#include "../YgorImages_Functors/Processing/Orthogonal_Slices.h"

#include "../YgorImages_Functors/Transform/DCEMRI_C_Map.h"
#include "../YgorImages_Functors/Transform/DCEMRI_Signal_Difference_C.h"
#include "../YgorImages_Functors/Transform/CT_Perfusion_Signal_Diff.h"
#include "../YgorImages_Functors/Transform/DCEMRI_S0_Map_v2.h"
#include "../YgorImages_Functors/Transform/DCEMRI_T1_Map_v2.h"
#include "../YgorImages_Functors/Transform/Pixel_Value_Histogram.h"
#include "../YgorImages_Functors/Transform/Subtract_Spatially_Overlapping_Images.h"

#include "../YgorImages_Functors/Compute/Per_ROI_Time_Courses.h"
#include "../YgorImages_Functors/Compute/Contour_Similarity.h"

#include "BuildLexiconInteractively.h"


std::list<OperationArgDoc> OpArgDocBuildLexiconInteractively(void){
    std::list<OperationArgDoc> out;

    out.emplace_back();
    out.back().name = "CleanLabels";
    out.back().desc = "A listing of the labels of interest. These will be (some of) the 'clean' entries in the"
                      " finished lexicon. You should only name ROIs you specifically care about and which have"
                      " a single, unambiguous occurence in the data set (e.g., 'Left_Parotid' is good, but"
                      " 'JUNK' and 'Parotids' are bad -- you won't be able to select the single 'JUNK' label"
                      " if all you care about are parotids.";
    out.back().default_val = "Body,Brainstem,Chiasm,Cord,Larynx Pharynx,Left Eye,Left Optic Nerve"
                             ",Left Parotid,Left Submand,Left Temp Lobe,Oral Cavity,Right Eye,Right Optic Nerve"
                             ",Right Parotid,Right Submand,Right Temp Lobe";
    out.back().expected = true;
    out.back().examples = { "Left Parotid,Right Parotid,Left Submand,Right Submand", 
                            "Left Submand,Right Submand" };

    out.emplace_back();
    out.back().name = "JunkLabel";
    out.back().desc = "A label to apply to the un-matched labels. This helps prevent false positives by"
                      " excluding names which are close to a desired clean label. For example, if you"
                      " are looking for 'Left_Parotid' you will want to mark 'left-parotid_opti' and"
                      " 'OLDLeftParotid' as junk. Passing an empty string disables junk labeling.";
    out.back().default_val = "JUNK";
    out.back().expected = true;
    out.back().examples = { "",
                            "Junk",
                            "Irrelevant",
                            "NA_Organ" };

    out.emplace_back();
    out.back().name = "LexiconSeedFile";
    out.back().desc = "A file containing a 'seed' lexicon to use and add to. This is the lexicon that"
                      " is being built. It will be modified.";
    out.back().default_val = "";
    out.back().expected = true;
    out.back().examples = { "./some_lexicon", "/tmp/temp_lexicon" };

    return out;
}

Drover BuildLexiconInteractively(Drover DICOM_data, OperationArgPkg OptArgs, std::map<std::string,std::string> /*InvocationMetadata*/, std::string FilenameLex){

    //This operation interactively builds a lexicon using the currently loaded contour labels.
    // 

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto CleanLabelsStr      = OptArgs.getValueStr("CleanLabels").value();
    const auto JunkLabel           = OptArgs.getValueStr("JunkLabel").value();
    const auto LexiconSeedFileName = OptArgs.getValueStr("LexiconSeedFile").value();

    //-----------------------------------------------------------------------------------------------------------------

    std::list<std::string> CleanLabels;
    for(auto a : SplitStringToVector(CleanLabelsStr, ',', 'd')){
        CleanLabels.emplace_back(a);
    }
    if(CleanLabels.empty()){
        throw std::invalid_argument("No lexicon 'clean' labels provided.");
    }

    Explicator X(LexiconSeedFileName);

    //Gather the data set's contour labels.
    //
    // NOTE: Items will be purged if they can be matched exactly by the lexicon.
    std::set<std::string> cc_labels;
    std::string PatientID;
    for(const auto &cc : DICOM_data.contour_data->ccs){
        for(const auto &c : cc.contours){
            auto label = c.GetMetadataValueAs<std::string>("ROIName");
            if(label) cc_labels.insert(label.value());

            auto patientID = c.GetMetadataValueAs<std::string>("PatientID");
            if(patientID) PatientID = patientID.value(); 
        }
    }


    //Firsts-pass: remove the exact matches. They will play no part in the building of the lexicon.
    std::map<std::string,bool> PerfectMatchFound;
    for(const auto &CleanLabel : CleanLabels){
        PerfectMatchFound[CleanLabel] = false;

        //Look for exact matches to this clean label. There might be many, so find them all.
        auto cc_label_it = cc_labels.begin();
        while( cc_label_it != cc_labels.end() ){
            const auto best_mapping = X(*cc_label_it);
            const auto best_score = X.Get_Last_Best_Score();
            if( (best_score >= 1.0) && (best_mapping == CleanLabel) ){
                FUNCWARN("Dropping '" << *cc_label_it << "' because it maps to '" << CleanLabel << "'");
                cc_label_it = cc_labels.erase(cc_label_it);
                PerfectMatchFound[CleanLabel] = true;
            }else if( (best_score >= 1.0) && !JunkLabel.empty() && (best_mapping == JunkLabel) ){
                //Special case: purge this entry because it is already junk (and would most likely be labeled as junk if
                // it does not get claimed ... and it probably won't because it is already mapped to the junk label).
                //
                // NOTE: This branch can cause unexpected behaviour if you are using the junk label for something other
                //       than the intended use. This branch also reduces a LOT of clutter if the lexicon is mostly
                //       complete. Be careful! 
                FUNCWARN("Dropping '" << *cc_label_it << "' because it maps to '" << JunkLabel << "'");
                cc_label_it = cc_labels.erase(cc_label_it);
            }else{
                ++cc_label_it;
            }
        }
    }


    //Cycle through the user-specified clean labels, attempting to translate with the seed lexicon.
    // If the mapping is exact then do not bother asking the user -- the lexicon already contains 
    // such information.
    std::list<std::pair<std::string, std::string>> dLexicon; //(clean, dirty).
    for(const auto &CleanLabel : CleanLabels){
        //If a perfect match was found, there is nothing to do.
        if(PerfectMatchFound[CleanLabel] == true) continue;

        //If there are no remaining data set labels, forgo asking the user anything.
        if(cc_labels.empty()) continue;

        //If there was no perfect match, ask the user what the answer is.
        std::vector<std::string> candidates;
        for(const auto &cc_label : cc_labels){
            //If the data set label exactly matches some other clean label, then it is not a candidate.
            //
            // Note: Even if it is a perfect match for another clean, it might still end up being marked by the junk
            //       label if the match is not claimed. In other words, if the lexicon contains clean strings that we do
            //       not know about here, then the matches will not be honoured. This can cause issues if you build a
            //       lexicon with changing clean labels. Try avoid doing so unless you know for certain that you no
            //       longer care about specific organs.
            X(cc_label);
            const auto best_score = X.Get_Last_Best_Score();
            if(best_score < 1.0) candidates.emplace_back(cc_label);
        }

        std::stringstream ss;
        ss << "dialog --clear --menu \"Which data set label corresponds to lexicon item '"
           << CleanLabel << "' ?"
           << " \\n (Note: PatientID = '" << PatientID << "')\""
           << " 50 100 40"; // dialog options: (height, width, inner height).
        ss << " 0 'None of the following'";
        {
            int i = 1;
            for(const auto &candidate : candidates){
                ss << " " << i++ << " '" << candidate << "'";
            }
        }
        ss << " 3>&1 1>&2 2>&3"; //Swap stdout and stderr for easier access to 

        const auto ResponseStr = Execute_Command_In_Pipe(ss.str());
        const long int Response = std::stol(ResponseStr);

        if((Response <= 0) || (Response > static_cast<long int>(candidates.size()))) continue;

        dLexicon.emplace_back( std::make_pair( CleanLabel, candidates[Response-1] ) );
        cc_labels.erase( candidates[Response-1] );
    }


    //If there is a junk label, apply it to the remaining data set labels.
    if(!JunkLabel.empty()){
        for(const auto &cc_label : cc_labels){
            dLexicon.emplace_back( std::make_pair( JunkLabel, cc_label ) );
        }
    }


    //Append the new lexicon entries to the seed lexicon. (Duplicates not handled!)
    if(!dLexicon.empty()){
        std::fstream FO(LexiconSeedFileName, std::fstream::out | std::fstream::app);
        if(!FO) throw std::invalid_argument("Cannot append to seed lexicon.");
        FO << "###" << std::endl;
        FO << "### Built interactively for patient '" << PatientID << "'" << std::endl;
        FO << "###" << std::endl;
        for(const auto &apair : dLexicon){
            FO << apair.first << " : " << apair.second;
            FO << std::endl;
        }
    }

    return std::move(DICOM_data);
}
