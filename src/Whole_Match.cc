//Whole_Match.cc - A prototype for identifying all organs in a given collection in one go,
//                 using exclusion and positional information to increase match rate.
//
//                 This routine uses contextual information about the problem domain to vastly
//                 improve automated organ contour recognition. This is at the expense of 
//                 flexibility. But if the domain is well-specified (e.g., you're interested
//                 in a small subset of organs - such as parotid and submandibular) then this
//                 routine should reliably perform well.

#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <vector>
#include <set>
#include <map>
#include <list>
#include <functional>
#include <algorithm>
#include <memory>

#include <cstdlib>         //Needed for exit() calls.
#include <utility>         //Needed for std::pair.

#include "YgorMisc.h"        //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorMath.h"        //Needed for vec3 class.
#include "YgorFilesDirs.h"   //Needed for Does_File_Exist_And_Can_Be_Read(...), etc..
#include "YgorContainers.h"  //Needed for bimap class.
#include "YgorPerformance.h" //Needed for YgorPerformance_dt_from_last().
#include "YgorAlgorithms.h"  //Needed for For_Each_In_Parallel<..>(...)
#include "YgorString.h"      //Needed for GetFirstRegex(...)
#include "Explicator.h"    //Needed for Explicator class.
#include "Demarcator.h"    //Needed for Demarcator class.

#include "Structs.h"

//   map<    dirty,      clean   >
std::map<std::string, std::string> Whole_Match(Explicator &X, Demarcator &Y, const Contour_Data &incomingcd){
    std::map<std::string,std::string> output;
    Contour_Data cd(incomingcd); //We will gradually erase elements if we are certain about them.

    //First look for the Body contour. After it is found, eliminate it from the input.
    bool found_Body = false;
    for(auto cc_it = cd.ccs.begin(); cc_it != cd.ccs.end(); ++cc_it){
        const std::string dirty(cc_it->Raw_ROI_name);
        std::string suspected = X(dirty);

        if((suspected == "Body") || (dirty == "Body") || (dirty == "BODY")){
            output[dirty] = "Body";
            cd.ccs.erase(cc_it);
            found_Body = true;
            break;
        }
    }
    if(!found_Body) FUNCWARN("Unable to find the Body contour");
   
    //Look for the Cord contour. Eliminate it from the input.
    bool found_Cord = false;
    for(auto cc_it = cd.ccs.begin(); cc_it != cd.ccs.end(); ++cc_it){
        const std::string dirty(cc_it->Raw_ROI_name);
        std::string suspected = X(dirty);

        if((suspected == "Cord") || (dirty == "Cord") || (dirty == "CORD") || (dirty == "Spinal Cord" || (dirty == "SPINAL CORD"))){
            output[dirty] = "Cord";
            cd.ccs.erase(cc_it);
            found_Cord = true;
            break;
        }
    }
    if(!found_Cord) FUNCWARN("Unable to find the Cord contour");

    //Search for cases where the text and geometry matching agree. Assume that they are valid if they agree.
    for(auto cc_it = cd.ccs.begin(); cc_it != cd.ccs.end(); ){
        const std::string dirty(cc_it->Raw_ROI_name);
        const contour_collection<double> *thecc = &(*cc_it);

        const auto x = X(dirty);
        const auto y = Y(*thecc);

        if((x == y) && (x != X.suspected_mistranslation) && (y != Y.suspected_mistranslation)){
            output[dirty] = x;
            cc_it = cd.ccs.erase(cc_it);
        }else{
            ++cc_it;
        }
    }

    //---------------------------------------------------------------------------------------------------------------------------
    //Now sort the structures into those on the LHS, those on the RHS, and those which cross the LHS/RHS boundary.
    std::map<std::string,contour_collection<double>> LHS, RHS, NHS;  //'no-hand-side'

    const plane<double> P(vec3<double>(-1.0, 0.0, 0.0), vec3<double>(0.0, 0.0, 0.0));
    for(auto cc_it = cd.ccs.begin(); cc_it != cd.ccs.end(); ++cc_it){
        const std::string dirty(cc_it->Raw_ROI_name);
        const contour_collection<double> *thecc = &(*cc_it);

        const auto crossing = thecc->Avoids_Plane(P); //-1 for below (no crossing), 0 for lying across, 1 for above (no crossing.)
        if(crossing == -1){
            //Add to LHS. 
            //FUNCINFO("On the LHS: " << dirty);
            LHS[dirty] = *thecc;
        }else if(crossing == 0){
            //Add to NHS.
            //FUNCINFO("Crosses the center: " << dirty);
            NHS[dirty] = *thecc;
        }else if(crossing == 1){
            //Add to RHS.
            //FUNCINFO("On the RHS: " << dirty);
            RHS[dirty] = *thecc;
        }
    }

/*
--(I) In function: Whole_Match: On the RHS: Artifact.
--(I) In function: Whole_Match: Crosses the center: Brainstem.
--(I) In function: Whole_Match: On the RHS: GTV.
--(I) In function: Whole_Match: On the LHS: L orbit.
--(I) In function: Whole_Match: Crosses the center: LD2.
--(I) In function: Whole_Match: On the LHS: LDneck.
--(I) In function: Whole_Match: Crosses the center: LDptv.
--(I) In function: Whole_Match: On the LHS: Left Submand.
--(I) In function: Whole_Match: On the LHS: Lt Optic Nerve.
--(I) In function: Whole_Match: On the LHS: Lt Parotid.
--(I) In function: Whole_Match: On the LHS: Lt Temp Lobe.
--(I) In function: Whole_Match: Crosses the center: Nodes.
--(I) In function: Whole_Match: Crosses the center: Optic Chiasm.
--(I) In function: Whole_Match: Crosses the center: PTV56.
--(I) In function: Whole_Match: On the RHS: PTV70.
--(I) In function: Whole_Match: On the RHS: R Submandibular.
--(I) In function: Whole_Match: On the RHS: R Temp Lobe.
--(I) In function: Whole_Match: On the RHS: R orbit.
--(I) In function: Whole_Match: On the RHS: Rt Optic Nerve.
--(I) In function: Whole_Match: On the RHS: Rt Parotid.
--(I) In function: Whole_Match: On the RHS: Rt Submand.
--(I) In function: Whole_Match: On the LHS: l Submandibular.
*/

    //Go through the separated data now. Look for items which are unlikely to appear on each side and remove them.
    for(auto m_it = LHS.begin(); m_it != LHS.end(); ++m_it){
        const std::string dirty(m_it->first);
        const auto cc(m_it->second);

        const auto x = X(dirty);
        const auto y = Y(cc);
       
        auto xres = X.Get_Last_Results(); 
        auto yres = Y.Get_Last_Results();

        for(auto it = xres->begin(); it != xres->end(); ){
            if((it->first.size() >= 5) && (it->first.substr(0,5) == "Right")){
                it = xres->erase(it);
            }else{ 
                ++it;
            }
        }
        for(auto it = yres->begin(); it != yres->end(); ){
            if((it->first.size() >= 5) && (it->first.substr(0,5) == "Right")){
                it = yres->erase(it);
            }else{ 
                ++it;
            }
        }

        //Now combine results from both. 
        std::map<std::string,float> combined;
        for(auto it = xres->begin(); it != xres->end(); ++it)  combined[it->first] += it->second;  //Sum results. Better technique?
        for(auto it = yres->begin(); it != yres->end(); ++it)  combined[it->first] += it->second;

        auto ltcomp = [](const std::pair<std::string,float> &A, const std::pair<std::string,float> &B) -> bool { return A.second < B.second; };
        const auto bestguessit = std::max_element(combined.begin(), combined.end(), ltcomp);

        if(bestguessit != combined.end()){ 
            output[dirty] = bestguessit->first;
        }
    }


    for(auto m_it = RHS.begin(); m_it != RHS.end(); ++m_it){
        const std::string dirty(m_it->first);
        const auto cc(m_it->second);

        const auto x = X(dirty);
        const auto y = Y(cc);

        auto xres = X.Get_Last_Results();
        auto yres = Y.Get_Last_Results();

        for(auto it = xres->begin(); it != xres->end(); ){
            if((it->first.size() >= 4) && (it->first.substr(0,4) == "Left")){
                it = xres->erase(it);
            }else{
                ++it;
            }
        }
        for(auto it = yres->begin(); it != yres->end(); ){
            if((it->first.size() >= 4) && (it->first.substr(0,4) == "Left")){
                it = yres->erase(it);
            }else{
                ++it;
            }
        }

        //Now combine results from both. 
        std::map<std::string,float> combined;
        for(auto it = xres->begin(); it != xres->end(); ++it)  combined[it->first] += it->second;  //Sum results. Better technique?
        for(auto it = yres->begin(); it != yres->end(); ++it)  combined[it->first] += it->second;

        auto ltcomp = [](const std::pair<std::string,float> &A, const std::pair<std::string,float> &B) -> bool { return A.second < B.second; };
        const auto bestguessit = std::max_element(combined.begin(), combined.end(), ltcomp);

        if(bestguessit != combined.end()){ 
            output[dirty] = bestguessit->first;
        }
    }


    for(auto m_it = NHS.begin(); m_it != NHS.end(); ++m_it){
        const std::string dirty(m_it->first);
        const auto cc(m_it->second);

        const auto x = X(dirty);
        const auto y = Y(cc);

        auto xres = X.Get_Last_Results();
        auto yres = Y.Get_Last_Results();

        for(auto it = xres->begin(); it != xres->end(); ){
            if((it->first.size() >= 4) && (it->first.substr(0,4) == "Left")){
                it = xres->erase(it);
            }else{
                ++it;
            }
        }
        for(auto it = yres->begin(); it != yres->end(); ){
            if((it->first.size() >= 4) && (it->first.substr(0,4) == "Left")){
                it = yres->erase(it);
            }else{
                ++it;
            }
        }
        for(auto it = xres->begin(); it != xres->end(); ){
            if((it->first.size() >= 5) && (it->first.substr(0,5) == "Right")){
                it = xres->erase(it);
            }else{
                ++it;
            }
        }
        for(auto it = yres->begin(); it != yres->end(); ){
            if((it->first.size() >= 5) && (it->first.substr(0,5) == "Right")){
                it = yres->erase(it);
            }else{
                ++it;
            }
        }

        //Now combine results from both. 
        std::map<std::string,float> combined;
        for(auto it = xres->begin(); it != xres->end(); ++it)  combined[it->first] += it->second;  //Sum results. Better technique?
        for(auto it = yres->begin(); it != yres->end(); ++it)  combined[it->first] += it->second;

        auto ltcomp = [](const std::pair<std::string,float> &A, const std::pair<std::string,float> &B) -> bool { return A.second < B.second; };
        const auto bestguessit = std::max_element(combined.begin(), combined.end(), ltcomp);

        if(bestguessit != combined.end()){ 
            output[dirty] = bestguessit->first;
        }
    }

/*
    //Text-matching only.
    for(auto m_it = ccs.begin(); m_it != ccs.end(); ++m_it){
        const std::string dirty(m_it->first);
        const auto cc(m_it->second);
        std::string bestchoice;

        //Get the list of results from matching.
        X(dirty);
        std::unique_ptr<std::map<std::string,float>> text_matches = X.Get_Last_Results();
        Y(cc);
        std::unique_ptr<std::map<std::string,float>> geom_matches = Y.Get_Last_Results();
                
        //For now, assume the text is perfect and the geometry is irrelevant.
        float bestscore = 0.0;
        for(auto tX_it = text_matches->begin(); tX_it != text_matches->end(); ++tX_it){
            const float score = tX_it->second;
            if(bestscore < score){
                bestscore  = score;
                bestchoice = tX_it->first;
            }
        }
        output[dirty] = bestchoice;
    }
*/    
    return output;
}
