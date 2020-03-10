//AnalyzeTPlan.cc - A part of DICOMautomaton 2018. Written by hal clark.

#include <boost/interprocess/creation_tags.hpp>
#include <boost/interprocess/sync/named_mutex.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>            //Needed for exit() calls.
#include <exception>
#include <optional>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>    
#include <utility>            //Needed for std::pair.
#include <vector>

#include "../Insert_Contours.h"
#include "../Structs.h"
#include "../Write_File.h"
#include "../Regex_Selectors.h"

#include "AnalyzeTPlan.h"

#include "YgorFilesDirs.h"    //Needed for Does_File_Exist_And_Can_Be_Read(...), etc..
#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMathBSpline.h" //Needed for basis_spline class.
#include "YgorMathPlottingGnuplot.h" //Needed for YgorMathPlottingGnuplot::*.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorString.h"       //Needed for GetFirstRegex(...)


OperationDoc OpArgDocAnalyzeTPlan(void){
    OperationDoc out;
    out.name = "AnalyzeTPlan";

    out.desc = 
        "This operation analyzes the selected RT plans, performing a general analysis"
        " suitable for exploring or comparing plans at a high-level."
        " Currently, only the total leaf opening (i.e., the sum of all leaf openings --"
        " the distance between a leaf in bank A to the opposing leaf in bank B) is"
        " reported for each plan, beam, and control point."
        " The output is a CSV file that can be concatenated or appended to other"
        " output files to provide a summary of multiple criteria.";

    out.args.emplace_back();
    out.args.back() = TPWhitelistOpArgDoc();
    out.args.back().name = "TPlanSelection";
    out.args.back().default_val = "last";


    out.args.emplace_back();
    out.args.back().name = "SummaryFilename";
    out.args.back().desc = "Analysis results will be appended to this file."
                           " The format is CSV. Leave empty to dump to generate a unique temporary file."
                           " If an existing file is present, rows will be appended without writing a header.";
    out.args.back().default_val = "";
    out.args.back().expected = true;
    out.args.back().examples = { "", "/tmp/somefile", "localfile.csv", "derivative_data.csv" };
    out.args.back().mimetype = "text/csv";


    out.args.emplace_back();
    out.args.back().name = "UserComment";
    out.args.back().desc = "A string that will be inserted into the output file which will simplify merging output"
                           " with differing parameters, from different sources, or using sub-selections of the data."
                           " Even if left empty, the column will remain in the output to ensure the outputs from"
                           " multiple runs can be safely concatenated."
                           " Preceding alphanumeric variables with a '$' will cause them to be treated as metadata"
                           " keys and replaced with the corresponding key's value, if present. For example,"
                           " 'The modality is $Modality' might be (depending on the metadata) expanded to"
                           " 'The modality is RTPLAN'. If the metadata key is not present, the expression will remain"
                           " unexpanded (i.e., with a preceeding '$').";
    out.args.back().default_val = "";
    out.args.back().expected = false;
    out.args.back().examples = { "Using XYZ", "Patient treatment plan C", "$PatientID" };


    out.args.emplace_back();
    out.args.back().name = "Description";
    out.args.back().desc = "A string that will be inserted into the output file which should be used to describe the"
                           " constraint and any caveats that the viewer should be aware of. Generally, the UserComment"
                           " is best for broadly-defined notes whereas the Description is tailored for each constraint."
                           " Preceding alphanumeric variables with a '$' will cause them to be treated as metadata"
                           " keys and replaced with the corresponding key's value, if present. For example,"
                           " 'The modality is $Modality' might be (depending on the metadata) expanded to"
                           " 'The modality is RTPLAN'. If the metadata key is not present, the expression will remain"
                           " unexpanded (i.e., with a preceeding '$').";
    out.args.back().default_val = "";
    out.args.back().expected = false;
    out.args.back().examples = { "2 Arcs", "1 Arc", "IMRT" };

    return out;
}

static
std::vector<std::string>
Get_All_Regex(const std::string &source, std::regex &regex_the_query){
    std::vector<std::string> out;
    std::sregex_iterator it(source.begin(), source.end(), regex_the_query);
    std::sregex_iterator end;
    for( ; it != end; ++it){
        for(decltype((*it).size()) i = 1; i < (*it).size(); ++i){ //The Zeroth contains the entire match (not the matches enclosed in parenthesis!)
            if((*it)[i].matched) out.push_back( (*it)[i].str());
        }
    }
    return out;
}

static
std::regex
lCompile_Regex(std::string input){
    return std::regex(input, std::regex::icase | 
                             //std::regex::nosubs |
                             std::regex::optimize |
                             std::regex::extended);
}

Drover AnalyzeTPlan(Drover DICOM_data, OperationArgPkg OptArgs, std::map<std::string,std::string> /*InvocationMetadata*/, std::string /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto TPlanSelectionStr = OptArgs.getValueStr("TPlanSelection").value();

    const auto SummaryFilename = OptArgs.getValueStr("SummaryFilename").value();

    const auto DescriptionOpt = OptArgs.getValueStr("Description");
    const auto UserCommentOpt = OptArgs.getValueStr("UserComment");
    //-----------------------------------------------------------------------------------------------------------------
    const auto nan = std::numeric_limits<double>::quiet_NaN();

    std::stringstream header;
    std::stringstream report;

    // Cycle over the Line_Segments, processing each one-at-a-time.
    auto TPs_all = All_TPs( DICOM_data );
    auto TPs = Whitelist( TPs_all, TPlanSelectionStr );

    for(auto & tp_it : TPs){

        //Determine which PatientID(s) to report.
        const auto PatientIDOpt = (*tp_it)->GetMetadataValueAs<std::string>("PatientID");
        const auto PatientID = PatientIDOpt.value_or("unknown");

        const auto RTPlanLabelOpt = (*tp_it)->GetMetadataValueAs<std::string>("RTPlanLabel");
        const auto RTPlanNameOpt = (*tp_it)->GetMetadataValueAs<std::string>("RTPlanName");
        const auto TPlanLabel = RTPlanLabelOpt.value_or( RTPlanNameOpt.value_or("unknown") );

        // Expand $-variables in the UserComment and Description with metadata.
        auto ExpandedUserCommentOpt = UserCommentOpt;
        if(UserCommentOpt){
            ExpandedUserCommentOpt = ExpandMacros(ExpandedUserCommentOpt.value(),
                                                  (*tp_it)->metadata, "$");
        }
        auto ExpandedDescriptionOpt = DescriptionOpt;
        if(DescriptionOpt){
            ExpandedDescriptionOpt = ExpandMacros(ExpandedDescriptionOpt.value(),
                                                  (*tp_it)->metadata, "$");
        }

        const auto emit_boilerplate = [&](void) -> void {
            //Reset the header so only one is ever emitted.
            //
            // Note: this requires a fixed, consistent report structure!
            {
                std::stringstream dummy;
                header = std::move(dummy);
            }

            // Patient metadata.
            {
                header << "PatientID";
                report << PatientID;
                
                header << ",TPlanLabel";
                report << "," << TPlanLabel;

                header << ",UserComment";
                report << "," << ExpandedUserCommentOpt.value_or("");

                header << ",Description";
                report << "," << ExpandedDescriptionOpt.value_or("");
            }

            report.precision(std::numeric_limits<double>::digits10 + 1);

            return;
        };

        for(const auto &ds : (*tp_it)->dynamic_states){
            long int control_point_num = 0;
            for(const auto &ss : ds.static_states){
                double total_leaf_opening = 0.0;

                const auto N_leaves = ss.MLCPositionsX.size();
                if( (N_leaves == 0) || ((N_leaves % 2) != 0)){
                    FUNCWARN("Invalid leaf count. Skipping beam");
                    continue;
                }

                for(size_t l_A = 0; l_A < (N_leaves / 2); ++l_A){
                    const auto l_B = l_A + (N_leaves / 2);

                    const auto p_l_A = ss.MLCPositionsX.at(l_A);
                    const auto p_l_B = ss.MLCPositionsX.at(l_B);

                    const auto leaf_opening = (p_l_B - p_l_A);
                    //std::cout << leaf_opening << " ";

                    if(leaf_opening < 0.0){
                        throw std::logic_error("Found negative leaf opening. Model is invalid!");
                    }
                    total_leaf_opening += leaf_opening;
                }

                const auto BeamNumberOpt = ds.GetMetadataValueAs<std::string>("BeamNumber");
                const auto BeamNumber = BeamNumberOpt.value_or("unknown");

                const auto BeamNameOpt = ds.GetMetadataValueAs<std::string>("BeamName");
                const auto BeamName = BeamNameOpt.value_or("unknown");

                // Emit the results as a new row.
                emit_boilerplate();

                header << ",BeamNumber";
                report << "," << BeamNumber;

                header << ",BeamName";
                report << "," << BeamName;

                header << ",ControlPoint";
                report << "," << control_point_num;

                header << ",CumulativeMetersetWeight";
                report << "," << ss.CumulativeMetersetWeight;

                header << ",GantryAngle";
                report << "," << ss.GantryAngle;

                header << ",LeafOpening";
                report << "," << total_leaf_opening;

                header << std::endl;
                report << std::endl;

                ++control_point_num;
            } // Loop over static states.
        } // Loop over dynamic states.
    } // Loop over treatment plans.

    //Print the report.
    //FUNCINFO("Report will contain: " << report.str());

    //Write the report to file.
    if(!TPs.empty()){
        try{
            auto gen_filename = [&](void) -> std::string {
                if(!SummaryFilename.empty()){
                    return SummaryFilename;
                }
                return Get_Unique_Sequential_Filename("/tmp/dcma_analyzetreatmentplans_", 6, ".csv");
            };

            FUNCINFO("About to claim a mutex");
            Append_File( gen_filename,
                         "dicomautomaton_operation_analyzetreatmentplans_mutex",
                         header.str(),
                         report.str() );

        }catch(const std::exception &e){
            FUNCERR("Unable to write to output file: '" << e.what() << "'");
        }
    }

    return DICOM_data;
}
