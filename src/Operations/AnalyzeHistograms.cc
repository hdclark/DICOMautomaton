//AnalyzeHistograms.cc - A part of DICOMautomaton 2018, 2020. Written by hal clark.

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
#include <filesystem>

#include "../Insert_Contours.h"
#include "../Structs.h"
#include "../Write_File.h"
#include "../Regex_Selectors.h"
#include "../String_Parsing.h"

#include "AnalyzeHistograms.h"

#include "YgorFilesDirs.h"    //Needed for Does_File_Exist_And_Can_Be_Read(...), etc..
#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorLog.h"
#include "YgorString.h"       //Needed for GetFirstRegex(...)


OperationDoc OpArgDocAnalyzeHistograms(){
    OperationDoc out;
    out.name = "AnalyzeHistograms";

    out.desc = 
        "This operation analyzes the selected line samples as if they were cumulative dose-volume histograms (DVHs)."
        " Multiple criteria can be specified. Results can be written to a CSV file that can be concatenated or"
        " appended to other output files to provide a summary of multiple criteria, or stored as key-values in the"
        " global parameter table.";

    out.notes.emplace_back(
        "This routine will filter out non-matching line samples. Currently required: Modality=Histogram; each must be"
        " explicitly marked as a cumulative, unscaled abscissa + unscaled ordinate histogram; and differential"
        " distribution statistics must be available (e.g., min, mean, and max voxel doses)."
    );
    out.notes.emplace_back(
        "No output files will be written when (1) there are no constraints to evaluate, and/or (2) results are written"
        " in the global parameter table."
    );

    out.args.emplace_back();
    out.args.back() = LSWhitelistOpArgDoc();
    out.args.back().name = "LineSelection";
    out.args.back().default_val = "last";


    out.args.emplace_back();
    out.args.back().name = "SummaryFilename";
    out.args.back().desc = "A summary of the criteria and results will be appended to this file."
                           " The format is CSV. Leave empty to dump to generate a unique temporary file."
                           " If an existing file is present, rows will be appended without writing a header."
                           " If there is nothing to write, a no file will be created.";
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
                           " 'The modality is Histogram'. If the metadata key is not present, the expression will remain"
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
                           " 'The modality is Histogram'. If the metadata key is not present, the expression will remain"
                           " unexpanded (i.e., with a preceeding '$').";
    out.args.back().default_val = "";
    out.args.back().expected = false;
    out.args.back().examples = { "Liver", "Lung", "Liver - GTV", "$LineName" };


    out.args.emplace_back();
    out.args.back().name = "Constraints";
    out.args.back().desc = "Constraint criteria that will be evaluated against the selected line samples."
                           " Three general types of constraints are recognized."
                           "\n\n"
                           " First, constraints in the style of 'Dmax < 50.0 Gy'."
                           " The left-hand-size (LHS) can be any of {Dmin, Dmean, Dmax}."
                           " The inequality can be any of {<, lt, <=, lte, >, gt, >=, gte}."
                           " The right-hand-side (RHS) units can be any of {Gy, %, none} where"
                           " the default units are assumed to be 'Gy',"
                           " '%' means the RHS number is a percentage of the ReferenceDose,"
                           " and 'none' uses the existing values without printing or conversion."
                           "\n\n"
                           "Second, constraints in the style of 'D(coldest 500.0 cc) < 50.4 Gy'."
                           " The inner LHS can be any of {coldest, hottest}."
                           " The inner LHS units can be any of {cc, cm3, cm^3, %} where"
                           " '%' means the inner LHS number is a percentage of the total volume."
                           " The inequality can be any of {<, lt, <=, lte, >, gt, >=, gte}."
                           " The RHS units can be any of {Gy, %, none} where"
                           " the default units are assumed to be 'Gy',"
                           " '%' means the RHS number is a percentage of the ReferenceDose"
                           " and 'none' uses the existing values without printing or conversion."
                           "\n\n"
                           "Third, constraints in the style of 'V(24.5 Gy) < 500.0 cc'."
                           " The inner LHS units can be any of {Gy, %, none} where"
                           " the default units are assumed to be 'Gy',"
                           " '%' means the inner LHS number is a percentage of the ReferenceDose"
                           " and 'none' uses the existing values without printing or conversion."
                           " The inequality can be any of {<, lt, <=, lte, >, gt, >=, gte}."
                           " The RHS units can be any of {cc, cm3, cm^3, %} where"
                           " '%' means the inner LHS number is a percentage of the total volume."
                           "\n\n"
                           "All of the preceding constraints can be prefixed with '(x,y):' to assign"
                           " the results to variables in the global parameter table. For example,"
                           " '(var_x, var_y) : Dmax < 50.0 Gy' will save the key-value 'var_x' as"
                           " the extracted value of Dmax, and 'var_y' as the boolean result of the"
                           " inequality (i.e., either 'true' or 'false'). Dose and volume units are"
                           " consistent with the expression's RHS units. An underscore can be used"
                           " to ignore one of the results. Note that saving results in the parameter"
                           " table precludes them from being written to file."
                           "\n\n"
                           "Additionally, dosimetric values can be directly assigned to a variable name"
                           " and inserted into the global parameter store, e.g., 'x : Dmin'."
                           " Dose is reported in the default units, as-is, without printing or conversion"
                           " (e.g., DICOM units; Gy)."
                           " Note that variables assigned this way are not written to file."
                           "\n\n"
                           "Multiple constraints can be supplied by separating them with ';' delimiters."
                           " Each will be evaluated separately."
                           " Newlines can also be used, though constraints should all end with a ';'."
                           " Comments can be included by preceeding with a '#', which facilitate supplying"
                           " lists of constraints piped in (e.g., from a file via Bash process substitution)."
                           "";
    out.args.back().default_val = "";
    out.args.back().expected = true;
    out.args.back().examples = { "Dmax < 50.0 Gy", 
                                 "Dmean lte 80 %",
                                 "Dmin >= 80 %",
                                 "Dmin >= 65 Gy",
                                 "Dmin >= 65",
                                 "D(coldest 500.0 cc) <= 25.0 Gy",
                                 "D(coldest 500.0 cc) <= 15.0 %",
                                 "D(coldest 50%) <= 15.0 %",
                                 "D(hottest 10%) gte 95.0 %",
                                 "V(24.5 Gy) < 500.0 cc",
                                 "V(10%) < 50.0 cc",
                                 "V(24.5 Gy) < 500.0 cc",
                                 "V(24.5) < 500.0 cc",
                                 "(x,y) : V(24.5 Gy) < 500.0 cc",
                                 "(x,_) : Dmean < 5 %",
                                 "(_,x) : V(24.5 Gy) < 500.0 cc",
                                 "max_dose = Dmax",
                                 "x = D(coldest 10%)",
                                 "(Dmax,_): Dmax < 50.0 Gy ; x: Dmin; (dose_median,passed): D(hottest 50%) <= 5 Gy; Dmean lte 80 % ; D(hottest 10%) gte 95.0 %" };


    out.args.emplace_back();
    out.args.back().name = "ReferenceDose";
    out.args.back().desc = "The absolute dose that relative (i.e., percentage) constraint doses will be considered against."
                           " Generally this will be the prescription dose (in DICOM units; Gy)."
                           " If there are multiple prescriptions, either the prescription appropriate for the constraint"
                           " should be supplied, or relative dose constraints should not be used.";
    out.args.back().default_val = "nan";
    out.args.back().expected = true;
    out.args.back().examples = { "70.0", "42.5" };


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
lCompile_Regex(const std::string& input){
    return std::regex(input, std::regex::icase | 
                             //std::regex::nosubs |
                             std::regex::optimize |
                             std::regex::extended);
}

bool AnalyzeHistograms(Drover &DICOM_data,
                       const OperationArgPkg& OptArgs,
                       std::map<std::string, std::string>& InvocationMetadata,
                       const std::string& /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto LineSelectionStr = OptArgs.getValueStr("LineSelection").value();

    const auto SummaryFilename = OptArgs.getValueStr("SummaryFilename").value();
    const auto ConstraintsStr = OptArgs.getValueStr("Constraints").value();
    const auto ReferenceDose = std::stod( OptArgs.getValueStr("ReferenceDose").value() );

    const auto DescriptionOpt = OptArgs.getValueStr("Description");
    const auto UserCommentOpt = OptArgs.getValueStr("UserComment");
    //-----------------------------------------------------------------------------------------------------------------
    const auto nan = std::numeric_limits<double>::quiet_NaN();

    std::stringstream header;
    std::stringstream report;

    // Cycle over the Line_Segments, processing each one-at-a-time.
    auto LSs_all = All_LSs( DICOM_data );
    auto LSs_f = Whitelist( LSs_all, LineSelectionStr );
    auto LSs = Whitelist( LSs_f, { { "Modality", ".*Histogram.*" },
                                   { "HistogramType", ".*Cumulative.*" },
                                   { "AbscissaScaling", ".*None.*" },
                                   { "OrdinateScaling", ".*None.*" } } );
    
    YLOGINFO("Selected " << LSs.size() << " line samples for analysis");

    for(auto & lsp_it : LSs){
        //const auto Modality = (*lsp_it)->line.metadata["Modality"];
        if((*lsp_it)->line.samples.empty()) throw std::invalid_argument("Unable to find histogram data to analyze.");

        //Determine which PatientID(s) to report.
        const auto PatientIDOpt = (*lsp_it)->line.GetMetadataValueAs<std::string>("PatientID");
        const auto PatientID = PatientIDOpt.value_or("unknown");

        const auto LineName = (*lsp_it)->line.GetMetadataValueAs<std::string>("LineName").value_or("unspecified");
        const auto MinDose  = (*lsp_it)->line.GetMetadataValueAs<double>("DistributionMin").value_or(nan);
        const auto MeanDose = (*lsp_it)->line.GetMetadataValueAs<double>("DistributionMean").value_or(nan);
        const auto MaxDose  = (*lsp_it)->line.GetMetadataValueAs<double>("DistributionMax").value_or(nan);

        // Expand $-variables in the UserComment and Description with metadata.
        auto ExpandedUserCommentOpt = UserCommentOpt;
        if(UserCommentOpt){
            ExpandedUserCommentOpt = ExpandMacros(ExpandedUserCommentOpt.value(),
                                                  (*lsp_it)->line.metadata, "$");
        }
        auto ExpandedDescriptionOpt = DescriptionOpt;
        if(DescriptionOpt){
            ExpandedDescriptionOpt = ExpandMacros(ExpandedDescriptionOpt.value(),
                                                  (*lsp_it)->line.metadata, "$");
        }

        // Grab the line sample more specifically.
        const auto DVH_abs_D_abs_V = (*lsp_it)->line;

        // Evaluate the criteria.
        auto split_constraints = SplitStringToVector(ConstraintsStr, ';', 'd');
        split_constraints = SplitVector(split_constraints, '\n', 'd');
        split_constraints = SplitVector(split_constraints, '\r', 'd');
        split_constraints = SplitVector(split_constraints, '\t', 'd');
        for(auto ac : split_constraints){

            // Check if this line is a comment, empty, or is likely to contain a constraint.
            const auto ctrim = CANONICALIZE::TRIM_ENDS;
            ac = Canonicalize_String2(ac, ctrim);
            if(ac.empty()) continue;

            const auto r_comment = lCompile_Regex(R"***(^[ ]*[#].*$)***");
            if( std::regex_match(ac, r_comment ) ) continue;

            // Peel off any assignment statements, like:
            //    (x,y) : Dmin < 10 Gy
            // where 'x' should hold the value of Dmin and 'y' should hold the result of the inequality.
            std::string key_LHS;
            std::string key_RHS;
            if(auto q = lCompile_Regex(R"***(^[ ]*[(]([ ]*[^ ,]*)[ ]*[,][ ]*([^ )]*)[ ]*[)][ ]*[:](.*))***");
                     std::regex_match(ac, q) ){

                const auto p = Get_All_Regex(ac, q);
                //for(const auto &x : p) YLOGINFO("    Parsed parameter: '" << x << "'");                
                if(p.size() == 3UL){
                    key_LHS = Canonicalize_String2(p.at(0UL), ctrim);
                    key_RHS = Canonicalize_String2(p.at(1UL), ctrim);
                    ac = Canonicalize_String2(p.at(2UL), ctrim);

                    if(key_LHS == "_") key_LHS.clear();
                    if(key_RHS == "_") key_RHS.clear();

                    if(!key_LHS.empty()) InvocationMetadata.erase(key_LHS);
                    if(!key_RHS.empty()) InvocationMetadata.erase(key_RHS);
                }
            }

            const auto emit_boilerplate = [&]() -> void {
                //Reset the header so only one is ever emitted.
                //
                // Note: this requires a fixed, consistent report structure!
                {
                    std::stringstream placeholder;
                    header = std::move(placeholder);
                }

                // Maximize numerical precision to support (near-)lossless interchange.
                const auto print_digits = std::numeric_limits<double>::max_digits10;
                header.precision(print_digits);
                report.precision(print_digits);

                // Patient metadata.
                {
                    header << "PatientID";
                    report << PatientID;
                    
                    header << ",Name";
                    report << "," << LineName;
                    
                    header << ",UserComment";
                    report << "," << ExpandedUserCommentOpt.value_or("");

                    header << ",Constraint";
                    report << ",'" << ac << "'";

                    header << ",Description";
                    report << "," << ExpandedDescriptionOpt.value_or("");
                }

                return;
            };

            // Classify the statement.
            const auto r_lt     = lCompile_Regex(R"***(^.*[<][^=].*$|^.*lt[^e].*$)***");
            const auto r_gt     = lCompile_Regex(R"***(^.*[>][^=].*$|^.*gt[^e].*$)***");
            const auto r_lte    = lCompile_Regex(R"***(^.*[<][=].*$|^.*lte.*$)***");
            const auto r_gte    = lCompile_Regex(R"***(^.*[>][=].*$|^.*gte.*$)***");
            const auto r_assign = lCompile_Regex(R"***(^.*[^><][:].*$|^.*as.*$)***");

            enum class equality_t { na, assign, lt, lte, gt, gte };   // inequality type.
            equality_t eq = equality_t::na;

            if( false ){
            }else if( std::regex_match(ac, r_lt     ) ){    eq = equality_t::lt;
            }else if( std::regex_match(ac, r_gt     ) ){    eq = equality_t::gt;
            }else if( std::regex_match(ac, r_lte    ) ){    eq = equality_t::lte;
            }else if( std::regex_match(ac, r_gte    ) ){    eq = equality_t::gte;
            }else if( std::regex_match(ac, r_assign ) ){    eq = equality_t::assign;
            }else{ throw std::invalid_argument("No inequality type recognized. Cannot continue.");
            }

            const auto compare_inequality = [&](double L, double R) -> bool {
                if( false ){
                }else if(eq == equality_t::lt ){   return (L < R);
                }else if(eq == equality_t::gt ){   return (L > R);
                }else if(eq == equality_t::lte){   return (L <= R);
                }else if(eq == equality_t::gte){   return (L >= R);
                }
                throw std::logic_error("Unrecognized inequality type. Cannot continue.");
                return false; // Should never get here.
            };

            const auto r_min  = lCompile_Regex(R"***(.*min.*)***");
            const auto r_mean = lCompile_Regex(R"***(.*mean.*)***");
            const auto r_max  = lCompile_Regex(R"***(.*max.*)***");

            const auto r_gy   = lCompile_Regex(R"***(.*Gy.*)***");
            const auto r_pcnt = lCompile_Regex(R"***(.*[%].*)***");
            const auto r_none = lCompile_Regex(R"***(^[ ]*$|^$)***");

            const auto r_cc   = lCompile_Regex(R"***(.*cc.*|.*cm3.*|.*cm\^3.*)***");

            const auto r_hot  = lCompile_Regex(R"***(.*hot.*)***");
            const auto r_cold = lCompile_Regex(R"***(.*cold.*)***");

            /////////////////////////////////////////////////////////////////////////////////
            // D{min,mean,max} {<,<=,>=,>,lt,lte,gt,gte} 123.123 {%,Gy,none}.
            // For example, 'Dmin < 70 Gy' or 'Dmean <= 105%' or 'Dmax lte 23.2Gy'.
            // Note that a '%' on the RHS is relative to the ReferenceDose.
            if(auto q = lCompile_Regex(R"***([ ]*D(min|max|mean).*(<|<=|>=|>|lte|lt|gte|gt)[^0-9.]*([0-9.]+)[^0-9.]*(Gy|%|[ ]*|[]*).*)***");
                     std::regex_match(ac, q) ){

                const auto p = Get_All_Regex(ac, q);
                //for(const auto &x : p) YLOGINFO("    Parsed parameter: '" << x << "'");                
                if(p.size() != 4) throw std::runtime_error("Unable to parse constraint (C).");
                const auto mmm = p.at(0);
                //const auto ineq = p.at(1);
                const auto D_rhs = std::stod(p.at(2));
                const auto unit = p.at(3);

                // Determine which statistic is needed.
                double D_mmm = nan;
                if( false ){
                }else if( std::regex_match(mmm, r_min ) ){    D_mmm = MinDose;
                }else if( std::regex_match(mmm, r_mean) ){    D_mmm = MeanDose;
                }else if( std::regex_match(mmm, r_max ) ){    D_mmm = MaxDose;
                }else{  throw std::runtime_error("Unable to parse constraint (D).");
                }

                // Scale the LHS statistic to match the units of the RHS so we can compare them and report the actual
                // LHS value (which makes it easier for the user to compare).
                std::string out_unit;
                if( false ){
                }else if( std::regex_match(unit, r_gy ) ){
                    D_mmm *= 1.0; // a no-op.
                    out_unit = "Gy";
                }else if( std::regex_match(unit, r_pcnt ) ){
                    D_mmm *= 100.0 / ReferenceDose; // Express as a percentage of ReferenceDose.
                    out_unit = "%";
                }else if( std::regex_match(unit, r_none ) ){
                    D_mmm *= 1.0; // a no-op.
                    out_unit = "";
                }else{
                    throw std::runtime_error("Unable to parse RHS unit.");
                }

                const bool passed = compare_inequality(D_mmm, D_rhs);

                // Store the result.
                if( !key_LHS.empty() || !key_RHS.empty() ){
                    if(!key_LHS.empty()) InvocationMetadata[key_LHS] = to_string_max_precision(D_mmm);
                    if(!key_RHS.empty()) InvocationMetadata[key_RHS] = (passed ? "true" : "false");

                // Add to the report.
                }else{
                    emit_boilerplate();

                    header << ",Actual";
                    report << "," << D_mmm;
                    if(!out_unit.empty()) report << " " << out_unit;

                    header << ",Passed";
                    report << "," << std::boolalpha << passed;

                    header << std::endl;
                    report << std::endl;
                }

            /////////////////////////////////////////////////////////////////////////////////
            // D( hottest 500 cc) <=> 70 Gy 
            // D( hottest 500 cc) <= 70 Gy 
            // D( coldest 25% ) lte 25 %
            }else if(auto q = lCompile_Regex(R"***([ ]*D[(][ ]*(hott?e?s?t?|cold?e?s?t?)[ ]*([0-9.]+)[ ]*(cc|cm3|cm\^3|%)[ ]*[)][ ]*(<|<=|>=|>|lte|lt|gte|gt)[^0-9.]*([0-9.]+)[^0-9.]*(Gy|%|[ ]*|[]*).*)***");
                     std::regex_match(ac, q) ){

                const auto p = Get_All_Regex(ac, q);
                if(p.size() != 6) throw std::runtime_error("Unable to parse constraint (E).");
                //for(const auto &x : p) YLOGINFO("    Parsed parameter: '" << x << "'");                

                const auto HC = p.at(0); // hot or cold.
                const auto V_lhs = std::stod(p.at(1)); // inner volume number.
                const auto LHS_unit = p.at(2); // cc or cm3 or cm^3 or %.
                const auto ineq = p.at(3); // < or <= or ... .
                const auto D_rhs = std::stod(p.at(4)); // dose number.
                const auto RHS_unit = p.at(5); // Gy or %.
                
                // Determine the equivalent absolute volume from the inner LHS.
                double V_abs = nan; // in mm^3.
                if( std::regex_match(LHS_unit, r_cc ) ){
                    // Convert cm^3 to mm^3.
                    V_abs = V_lhs * 1000.0;
                }else if( std::regex_match(LHS_unit, r_pcnt) ){
                    // Convert from % to mm^3 by taking the highest bin (which should contain all voxels) scaling the entire volume.
                    const auto V_total = DVH_abs_D_abs_V.Get_Extreme_Datum_y().second[2];
                    V_abs = V_total * V_lhs / 100.0;
                }else{ 
                    throw std::runtime_error("Unable to convert inner LHS V units.");
                }

                // Find the corresponding D from the LHS.
                double D_eval = nan;
                if( std::regex_match(HC, r_hot ) ){
                    // For a cumulative DVH, we merely have to find the right-most crossing point of the volume
                    // threshold and the DVH curve.
                    const auto crossings = DVH_abs_D_abs_V.Crossings(V_abs);
                    if(crossings.empty()){
                        throw std::invalid_argument("LHS inner constraint numeric volumetric value cannot be evaluated. Cannot continue.");
                    }
                    D_eval = crossings.Get_Extreme_Datum_x().second[0];

                }else if( std::regex_match(HC, r_cold ) ){
                    // For a cumulative DVH, we merely have to find the left-most crossing point of the volume
                    // threshold and the DVH curve. The volume threshold is modified by subtracting from the maximum
                    // since the left-most accumulates all voxels, regardless of dose (so the left-most bin should
                    // contain all voxels). 
                    const auto V_total = DVH_abs_D_abs_V.Get_Extreme_Datum_y().second[2];
                    const auto V_intersect = V_total - V_abs;
                    if(!isininc(0.0, V_intersect, V_total)){
                        throw std::invalid_argument("Volumetric constraint cannot be evaluated; insufficient target volume.");
                    }

                    const auto crossings = DVH_abs_D_abs_V.Crossings(V_intersect);
                    if(crossings.empty()){
                        throw std::invalid_argument("LHS inner constraint numeric volumetric value cannot be evaluated. Cannot continue.");
                    }
                    D_eval = crossings.Get_Extreme_Datum_x().first[0];

                }else{
                    throw std::runtime_error("Unable to parse directionality (i.e., hot or cold).");
                }

                // Express the LHS evaluated dose in the same units that the RHS has been stated.
                std::string out_unit;
                if( false ){
                }else if( std::regex_match(RHS_unit, r_gy ) ){
                    D_eval *= 1.0; // A no-op.
                    out_unit = "Gy";
                }else if( std::regex_match(RHS_unit, r_pcnt ) ){
                    D_eval *= 100.0 / ReferenceDose; // Express as a percentage of ReferenceDose.
                    out_unit = "%";
                }else if( std::regex_match(RHS_unit, r_none ) ){
                    D_eval *= 1.0; // a no-op.
                    out_unit = "";
                }else{
                    throw std::runtime_error("Unable to parse RHS unit.");
                }

                const bool passed = compare_inequality(D_eval, D_rhs);

                // Store the result.
                if( !key_LHS.empty() || !key_RHS.empty() ){
                    if(!key_LHS.empty()) InvocationMetadata[key_LHS] = to_string_max_precision(D_eval);
                    if(!key_RHS.empty()) InvocationMetadata[key_RHS] = (passed ? "true" : "false");

                // Add to the report.
                }else{
                    emit_boilerplate();

                    header << ",Actual";
                    report << "," << D_eval;
                    if(!out_unit.empty()) report << " " << out_unit;

                    header << ",Passed";
                    report << "," << std::boolalpha << passed;

                    header << std::endl;
                    report << std::endl;
                }

            /////////////////////////////////////////////////////////////////////////////////
            // V(24 Gy) < 500 cc
            // V(20%) < 500 cc
            // V(25 Gy) < 25%
            // V(20%) < 25%
            }else if(auto q = lCompile_Regex(R"***([ ]*V[(][ ]*([0-9.]+)[ ]*(Gy|%|[ ]*|[]*)[ ]*[)][ ]*(<|<=|>=|>|lte|lt|gte|gt)[^0-9.]*([0-9.]+)[^0-9.]*(cc|cm3|cm\^3|%).*)***");
                     std::regex_match(ac, q) ){

                const auto p = Get_All_Regex(ac, q);
                if(p.size() != 5) throw std::runtime_error("Unable to parse constraint (E).");
                //for(const auto &x : p) YLOGINFO("    Parsed parameter: '" << x << "'");                

                const auto D_lhs = std::stod(p.at(0)); // inner dose number.
                const auto LHS_unit = p.at(1); // Gy or %.
                const auto ineq = p.at(2); // < or <= or ... .
                const auto V_rhs = std::stod(p.at(3)); // volume number.
                const auto RHS_unit = p.at(4); // cc or cm3 or cm^3 or %.

                const auto V_total = DVH_abs_D_abs_V.Get_Extreme_Datum_y().second[2];

                // Determine the equivalent absolute dose from the inner LHS.
                double D_abs = nan; // in Gy.
                if( false ){
                }else if( std::regex_match(LHS_unit, r_gy ) ){
                    D_abs = D_lhs; // A no-op.
                }else if( std::regex_match(LHS_unit, r_pcnt) ){
                    D_abs = D_lhs * ReferenceDose / 100.0; // Convert from % to abs.
                }else if( std::regex_match(LHS_unit, r_none ) ){
                    D_abs = D_lhs; // A no-op.
                }else{ 
                    throw std::runtime_error("Unable to convert inner LHS D units.");
                }

                // Evaluate the dose in the DVH.
                double V_eval = nan;
                if(D_abs <= 0.0){
                    // Special case where interpolation won't work well (would have to extrapolate).
                    V_eval = V_total;
                }else{
                    V_eval = DVH_abs_D_abs_V.Interpolate_Linearly( D_abs )[2];
                }

                // Express the LHS evaluated volume in the same units that the RHS has been stated.
                std::string out_unit;
                if( false ){
                }else if( std::regex_match(RHS_unit, r_cc ) ){
                    // Convert mm^3 to cm^3.
                    V_eval /= 1000.0;
                    out_unit = "cm^3";
                }else if( std::regex_match(RHS_unit, r_pcnt) ){
                    // Convert from % to mm^3 by taking the highest bin (which should contain all voxels) scaling the entire volume.
                    V_eval = 100.0 * V_eval / V_total;
                    out_unit = "%";
                }else{ 
                    throw std::runtime_error("Unable to express the LHS volume in RHS units.");
                }

                const bool passed = compare_inequality(V_eval, V_rhs);

                // Store the result.
                if( !key_LHS.empty() || !key_RHS.empty() ){
                    if(!key_LHS.empty()) InvocationMetadata[key_LHS] = to_string_max_precision(V_eval);
                    if(!key_RHS.empty()) InvocationMetadata[key_RHS] = (passed ? "true" : "false");

                // Add to the report.
                }else{
                    emit_boilerplate();

                    header << ",Actual";
                    report << "," << V_eval << " " << out_unit;

                    header << ",Passed";
                    report << "," << std::boolalpha << passed;

                    header << std::endl;
                    report << std::endl;
                }

            /////////////////////////////////////////////////////////////////////////////////
            // Assignment:
            //   var_name : D{min,mean,max}
            }else if(auto q = lCompile_Regex(R"***([ ]*([^ :]*)[ ]*[:][ ]*D(min|max|mean).*)***");
                     std::regex_match(ac, q) ){

                const auto p = Get_All_Regex(ac, q);
                if(p.size() != 2) throw std::runtime_error("Unable to parse constraint (F).");
                const auto var_name = p.at(0);
                const auto mmm = p.at(1);

                InvocationMetadata.erase(var_name);

                double D_mmm = nan;
                if( false ){
                }else if( std::regex_match(mmm, r_min ) ){    D_mmm = MinDose;
                }else if( std::regex_match(mmm, r_mean) ){    D_mmm = MeanDose;
                }else if( std::regex_match(mmm, r_max ) ){    D_mmm = MaxDose;
                }else{  throw std::runtime_error("Unable to parse constraint (F).");
                }

                InvocationMetadata[var_name] = to_string_max_precision(D_mmm);

            /////////////////////////////////////////////////////////////////////////////////
            // Assignment:
            //   var_name : D( hottest 500 cc)
            //   var_name : D( hottest 500 cc) 
            //   var_name : D( coldest 25% )
            }else if(auto q = lCompile_Regex(R"***([ ]*([^ :]*)[ ]*[:][ ]*D[(][ ]*(hott?e?s?t?|cold?e?s?t?)[ ]*([0-9.]+)[ ]*(cc|cm3|cm\^3|%)[ ]*[)][ ]*)***");
                     std::regex_match(ac, q) ){

                const auto p = Get_All_Regex(ac, q);
                if(p.size() != 4) throw std::runtime_error("Unable to parse constraint (G).");
                //for(const auto &x : p) YLOGINFO("    Parsed parameter: '" << x << "'");                

                const auto var_name = p.at(0);
                const auto HC = p.at(1); // hot or cold.
                const auto V_rhs = std::stod(p.at(2)); // inner volume number.
                const auto V_unit = p.at(3); // cc or cm3 or cm^3 or %.

                InvocationMetadata.erase(var_name);
                
                // Determine the equivalent absolute volume from the inner RHS.
                double V_abs = nan; // in mm^3.
                if( std::regex_match(V_unit, r_cc ) ){
                    // Convert cm^3 to mm^3.
                    V_abs = V_rhs * 1000.0;
                }else if( std::regex_match(V_unit, r_pcnt) ){
                    // Convert from % to mm^3 by taking the highest bin (which should contain all voxels) scaling the entire volume.
                    const auto V_total = DVH_abs_D_abs_V.Get_Extreme_Datum_y().second[2];
                    V_abs = V_total * V_rhs / 100.0;
                }else{ 
                    throw std::runtime_error("Unable to convert inner RHS V units.");
                }

                // Find the corresponding D from the RHS.
                double D_eval = nan;
                if( std::regex_match(HC, r_hot ) ){
                    // For a cumulative DVH, we merely have to find the right-most crossing point of the volume
                    // threshold and the DVH curve.
                    const auto crossings = DVH_abs_D_abs_V.Crossings(V_abs);
                    if(crossings.empty()){
                        throw std::invalid_argument("RHS inner constraint numeric volumetric value cannot be evaluated. Cannot continue.");
                    }
                    D_eval = crossings.Get_Extreme_Datum_x().second[0];

                }else if( std::regex_match(HC, r_cold ) ){
                    // For a cumulative DVH, we merely have to find the left-most crossing point of the volume
                    // threshold and the DVH curve. The volume threshold is modified by subtracting from the maximum
                    // since the left-most accumulates all voxels, regardless of dose (so the left-most bin should
                    // contain all voxels). 
                    const auto V_total = DVH_abs_D_abs_V.Get_Extreme_Datum_y().second[2];
                    const auto V_intersect = V_total - V_abs;
                    if(!isininc(0.0, V_intersect, V_total)){
                        throw std::invalid_argument("Volumetric constraint cannot be evaluated; insufficient target volume.");
                    }

                    const auto crossings = DVH_abs_D_abs_V.Crossings(V_intersect);
                    if(crossings.empty()){
                        throw std::invalid_argument("RHS inner constraint numeric volumetric value cannot be evaluated. Cannot continue.");
                    }
                    D_eval = crossings.Get_Extreme_Datum_x().first[0];

                }else{
                    throw std::runtime_error("Unable to parse directionality (i.e., hot or cold).");
                }

                InvocationMetadata[var_name] = to_string_max_precision(D_eval);

            // If the constraint did not match any known format.
            }else{
                YLOGWARN("Constraint '" << ac << "' did not match any known format");
             
                // Also acknowledge in-band that the constraint did not match any implemented form.
                emit_boilerplate();

                header << ",Actual";
                report << "," << "no match";

                header << ",Passed";
                report << "," << "no match";

                header << std::endl;
                report << std::endl;
            }

        } // Loop over criteria.

    } // Loop over line samples.

    //Print the report.
    //YLOGINFO("Report will contain: " << report.str());

    //Write the report to file.
    if( !LSs.empty()
    &&  !header.str().empty()
    &&  !report.str().empty() ){
        auto gen_filename = [&]() -> std::string {
            if(!SummaryFilename.empty()){
                return SummaryFilename;
            }
            const auto base = std::filesystem::temp_directory_path() / "dcma_analyzehistograms_";
            return Get_Unique_Sequential_Filename(base.string(), 6, ".csv");
        };

        Append_File( gen_filename,
                     "dcma_op_analyzehistograms_mutex",
                     header.str(),
                     report.str() );
    }

    return true;
}
