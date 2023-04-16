//BuildLexiconInteractively.cc - A part of DICOMautomaton 2016. Written by hal clark.

#include <optional>
#include <fstream>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <regex>
#include <set> 
#include <stdexcept>
#include <string>    
#include <utility>            //Needed for std::pair.
#include <vector>
#include <cstdint>

#include "Explicator.h"       //Needed for Explicator class.

#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorLog.h"
#include "YgorString.h"       //Needed for GetFirstRegex(...)

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "BuildLexiconInteractively.h"


OperationDoc OpArgDocBuildLexiconInteractively(){
    OperationDoc out;
    out.name = "BuildLexiconInteractively";
    out.desc = 
        "This operation interactively builds a lexicon using the currently loaded contour labels."
        " It is useful for constructing a domain-specific lexicon from a set of representative data.";


    out.notes.emplace_back(
        "An exclusive approach is taken for ROI selection rather than an inclusive approach because "
        " regex negations are not easily supported in the POSIX syntax."
    );


    out.args.emplace_back();
    out.args.back().name = "CleanLabels";
    out.args.back().desc = "A listing of the labels of interest. These will be (some of) the 'clean' entries in the"
                           " finished lexicon. You should only name ROIs you specifically care about and which have"
                           " a single, unambiguous occurence in the data set (e.g., 'Left_Parotid' is good, but"
                           " 'JUNK' and 'Parotids' are bad -- you won't be able to select the single 'JUNK' label"
                           " if all you care about are parotids.";
    out.args.back().default_val = "Body,Brainstem,Chiasm,Cord,Larynx Pharynx,Left Eye,Left Optic Nerve"
                                  ",Left Parotid,Left Submand,Left Temp Lobe,Oral Cavity,Right Eye,Right Optic Nerve"
                                  ",Right Parotid,Right Submand,Right Temp Lobe";
    out.args.back().expected = true;
    out.args.back().examples = { "Left Parotid,Right Parotid,Left Submand,Right Submand", 
                            "Left Submand,Right Submand" };

    out.args.emplace_back();
    out.args.back().name = "JunkLabel";
    out.args.back().desc = "A label to apply to the un-matched labels. This helps prevent false positives by"
                           " excluding names which are close to a desired clean label. For example, if you"
                           " are looking for 'Left_Parotid' you will want to mark 'left-parotid_opti' and"
                           " 'OLDLeftParotid' as junk. Passing an empty string disables junk labeling.";
    out.args.back().default_val = "JUNK";
    out.args.back().expected = true;
    out.args.back().examples = { "",
                            "Junk",
                            "Irrelevant",
                            "NA_Organ" };

    out.args.emplace_back();
    out.args.back() = RCWhitelistOpArgDoc();
    out.args.back().name = "OmitROILabelRegex";
    out.args.back().desc = "This parameter selects ROI labels/names to prune. Only matching ROIs will be pruned."_s
                         + " The default will match no ROIs. " + out.args.back().desc;
    out.args.back().default_val = "";
    out.args.back().expected = false;
    out.args.back().examples = { R"***(.*left.*|.*right.*|.*eyes.*)***",
                                 R"***(.*PTV.*|.*CTV.*|.*GTV.*)***" };


    out.args.emplace_back();
    out.args.back().name = "LexiconSeedFile";
    out.args.back().desc = "A file containing a 'seed' lexicon to use and add to. This is the lexicon that"
                           " is being built. It will be modified.";
    out.args.back().default_val = "";
    out.args.back().expected = true;
    out.args.back().examples = { "./some_lexicon", "/tmp/temp_lexicon" };

    return out;
}

bool BuildLexiconInteractively(Drover &DICOM_data,
                                 const OperationArgPkg& OptArgs,
                                 std::map<std::string, std::string>& /*InvocationMetadata*/,
                                 const std::string&){

        

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto CleanLabelsStr       = OptArgs.getValueStr("CleanLabels").value();
    const auto JunkLabel            = OptArgs.getValueStr("JunkLabel").value();
    const auto LexiconSeedFileName  = OptArgs.getValueStr("LexiconSeedFile").value();
    const auto OmitROILabelRegexOpt = OptArgs.getValueStr("OmitROILabelRegex");

    //-----------------------------------------------------------------------------------------------------------------

    std::list<std::string> CleanLabels;
    for(auto a : SplitStringToVector(CleanLabelsStr, ',', 'd')){
        CleanLabels.emplace_back(a);
    }
    if(CleanLabels.empty()){
        throw std::invalid_argument("No lexicon 'clean' labels provided.");
    }


    // Generate exclusion regex. If no exclusions provided, generate an invalid placeholder to fail ASAP.
    const std::string OmitROILabelRegexStr = ( OmitROILabelRegexOpt ? OmitROILabelRegexOpt.value() : "" );
    const auto excluderegex = std::regex(OmitROILabelRegexStr, std::regex::icase | std::regex::nosubs 
                                                                                 | std::regex::optimize 
                                                                                 | std::regex::extended);

    Explicator X(LexiconSeedFileName);

    //Gather the data set's contour labels.
    //
    // NOTE: Items will be purged if they can be matched exactly by the lexicon.
    std::set<std::string> cc_labels;
    std::string PatientID;
    DICOM_data.Ensure_Contour_Data_Allocated();
    for(const auto &cc : DICOM_data.contour_data->ccs){
        for(const auto &c : cc.contours){
            auto label = c.GetMetadataValueAs<std::string>("ROIName");
            if(label){
                if(!OmitROILabelRegexOpt //No exclusion criteria supplied.
                || !(std::regex_match(label.value(),excluderegex)) ){ 
                    cc_labels.insert(label.value());
                }
            }

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
                YLOGWARN("Dropping '" << *cc_label_it << "' because it maps to '" << CleanLabel << "'");
                cc_label_it = cc_labels.erase(cc_label_it);
                PerfectMatchFound[CleanLabel] = true;
            }else if( (best_score >= 1.0) && !JunkLabel.empty() && (best_mapping == JunkLabel) ){
                //Special case: purge this entry because it is already junk (and would most likely be labeled as junk if
                // it does not get claimed ... and it probably won't because it is already mapped to the junk label).
                //
                // NOTE: This branch can cause unexpected behaviour if you are using the junk label for something other
                //       than the intended use. This branch also reduces a LOT of clutter if the lexicon is mostly
                //       complete. Be careful! 
                YLOGWARN("Dropping '" << *cc_label_it << "' because it maps to '" << JunkLabel << "'");
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
        const int64_t Response = std::stol(ResponseStr);

        if((Response <= 0) || (Response > static_cast<int64_t>(candidates.size()))) continue;

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

    return true;
}
