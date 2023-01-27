//ValidateRTPlan.cc - A part of DICOMautomaton 2022. Written by hal clark.

#include <asio.hpp>
#include <algorithm>
#include <optional>
#include <fstream>
#include <iterator>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <regex>
#include <set> 
#include <stdexcept>
#include <string>    
#include <utility>            //Needed for std::pair.
#include <vector>

#include "Explicator.h"       //Needed for Explicator class.

#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorLog.h"
#include "YgorStats.h"        //Needed for Stats:: namespace.
#include "YgorString.h"       //Needed for GetFirstRegex(...)

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../Thread_Pool.h"
#include "../String_Parsing.h"
#include "../Metadata.h"
#include "../Tables.h"

#include "ValidateRTPlan.h"




table_placement_t get_table_placement(common_context_t& c){
    table_placement_t out;
    out.empty_row     = c.table.get().next_empty_row();
    out.pass_fail_col = c.depth + 0;
    out.title_col     = c.depth + 1;
    out.expl_col      = c.depth + 2;
    return out;
}

static
std::optional<double> get_min(const std::vector<double> &numbers){
    std::optional<double> out;
    const auto beg = std::begin(numbers);
    const auto end = std::end(numbers);
    const auto mp = std::min_element(beg, end);
    if(mp != end) out = *mp;
    return out;
}


// Ensure beam is a VMAT arc.
static bool beam_is_vmat_arc(const Dynamic_Machine_State &beam){
    const auto beg = std::begin(beam.static_states);
    const auto end = std::end(beam.static_states);
    const auto mmp = std::minmax_element( beg, end,
                                          []( const Static_Machine_State &l,
                                              const Static_Machine_State &r ){
                                              return (l.GantryAngle < r.GantryAngle);
                                          } );
    return ( 1u < beam.static_states.size() )
        && ( mmp.first != mmp.second )
        && ( 1.0 < std::abs(mmp.first->GantryAngle - mmp.second->GantryAngle) );
}

// Get the collimator angle (should usually be unique).
static std::optional<double> get_collimator_angle(const Dynamic_Machine_State &beam){
    std::optional<double> out;
    const double equivalent_angle_tol = 1.0; // in degrees.
    if(!beam.static_states.empty()){
        const auto beg = std::begin(beam.static_states);
        const auto end = std::end(beam.static_states);
        const auto mmp = std::minmax_element( beg, end,
                                              []( const Static_Machine_State &l,
                                                  const Static_Machine_State &r ){
                                                  return (l.BeamLimitingDeviceAngle < r.BeamLimitingDeviceAngle);
                                              } );
        const auto disc = std::abs(mmp.first->BeamLimitingDeviceAngle - mmp.second->BeamLimitingDeviceAngle);
        if(disc < equivalent_angle_tol){
            out = mmp.first->BeamLimitingDeviceAngle;
        }
    }
    return out;
}

// Extract jaw openings.
static
std::vector<std::pair<double,double>>
get_jaw_openings( const Dynamic_Machine_State &beam ){
    std::vector<std::pair<double,double>> out;
    const auto nan = std::numeric_limits<double>::quiet_NaN();
    for(const auto& s : beam.static_states){
        const auto x = (s.JawPositionsX.size() == 2) ? std::abs(s.JawPositionsX[1] - s.JawPositionsX[0]) : nan;
        const auto y = (s.JawPositionsY.size() == 2) ? std::abs(s.JawPositionsY[1] - s.JawPositionsY[0]) : nan;
        out.push_back( {x, y} );
    }
    return out;
}

static
std::optional<double>
get_smallest_jaw_perimeter( const std::vector<std::pair<double,double>>& jaw_openings ){
    std::vector<double> p;
    for(const auto& o : jaw_openings) p.emplace_back(2.0 * (o.first + o.second));
    return get_min(p);
}

static
std::optional<double>
get_smallest_X_jaw_opening( const std::vector<std::pair<double,double>>& jaw_openings ){
    std::vector<double> p;
    for(const auto& o : jaw_openings) p.emplace_back(o.first);
    return get_min(p);
}

static
std::optional<double>
get_smallest_Y_jaw_opening( const std::vector<std::pair<double,double>>& jaw_openings ){
    std::vector<double> p;
    for(const auto& o : jaw_openings) p.emplace_back(o.second);
    return get_min(p);
}

// Look for angles closer than the user-provided tolerance.
static bool minimal_separation_is_larger_than(std::vector<double> numbers,
                                              double tolerance ){
    const auto beg = std::begin(numbers);
    const auto end = std::end(numbers);
    std::sort( beg, end );
    const auto dup = std::adjacent_find( beg, end,
                                         [tolerance](const double &l, const double &r){
                                             return std::abs(l - r) < tolerance;
                                         } );
    return (dup == end);
}

static
std::optional<double> min_number(const std::vector<double> &numbers){
    std::optional<double> out;
    const auto beg = std::begin(numbers);
    const auto end = std::end(numbers);
    const auto mp = std::min_element(beg, end);
    if(mp != end) out = *mp;
    return out;
}

std::vector<check_t> get_checks(){
    std::vector<check_t> out;

    // Logical statements.
    out.emplace_back();
    out.back().name = "pass";
    out.back().desc = "This check always passes.";
    out.back().name_regex = "^pass$|^true$";
    out.back().check_impl = [](common_context_t&) -> bool {
        return true;
    };

    out.emplace_back();
    out.back().name = "fail";
    out.back().desc = "This check never passes.";
    out.back().name_regex = "^fail$|^false$";
    out.back().check_impl = [](common_context_t&) -> bool {
        return false;
    };

    // Logical checks.
    out.emplace_back();
    out.back().name = "all of";
    out.back().desc = "All children checks must pass for this check to pass.";
    out.back().name_regex = "^requ?i?r?e?s?$|^all[-_ ]?of$";
    out.back().check_impl = [](common_context_t& c) -> bool {
        const auto check_child = [&c](const parsed_function& pf) -> bool {
            common_context_t l_c(c);
            l_c.pf = std::cref(pf);
            l_c.depth++;
            return dispatch_checks(l_c);
        };
        const bool passed = std::all_of( std::begin(c.pf.get().children), std::end(c.pf.get().children), check_child );
        return passed;
    };

    out.emplace_back();
    out.back().name = "one or more of";
    out.back().desc = "At least one of the children checks must pass for this check to pass.";
    out.back().name_regex = "^any[-_ ]?of$";
    out.back().check_impl = [](common_context_t& c) -> bool {
        const auto check_child = [&c](const parsed_function& pf) -> bool {
            common_context_t l_c(c);
            l_c.pf = std::cref(pf);
            l_c.depth++;
            return dispatch_checks(l_c);
        };
        const bool passed = std::any_of( std::begin(c.pf.get().children), std::end(c.pf.get().children), check_child );
        return passed;
    };

    out.emplace_back();
    out.back().name = "none of";
    out.back().desc = "All children checks must fail for this check to pass.";
    out.back().name_regex = "^none[-_ ]?of$";
    out.back().check_impl = [](common_context_t& c) -> bool {
        const auto check_child = [&c](const parsed_function& pf) -> bool {
            common_context_t l_c(c);
            l_c.pf = std::cref(pf);
            l_c.depth++;
            return dispatch_checks(l_c);
        };
        const bool passed = std::none_of( std::begin(c.pf.get().children), std::end(c.pf.get().children), check_child );
        return passed;
    };

    // Specific checks.
    out.emplace_back();
    out.back().name = "plan name has no spaces";
    out.back().desc = "Ensure the plan name does not contain any spaces.";
    out.back().name_regex = "^plan[-_ ]?name[-_ ]has[-_ ]no[-_ ]spaces$";
    out.back().check_impl = [](common_context_t& c) -> bool {
        const auto tp = get_table_placement(c);

        const auto RTPlanLabelOpt = c.plan.get().GetMetadataValueAs<std::string>("RTPlanLabel"); 
        const auto RTPlanLabel = RTPlanLabelOpt.value_or("unknown"); 
        c.table.get().inject(c.report_row, tp.expl_col, RTPlanLabel);
        const bool has_space = (RTPlanLabel.find(" ") != std::string::npos);
        const bool passed = !has_space;
        return passed;
    };

    out.emplace_back();
    out.back().name = "has VMAT arc";
    out.back().desc = "Ensure the plan name does not contain any spaces.";
    out.back().name_regex = "^has[-_ ]?VMAT[-_ ]arc$";
    out.back().check_impl = [](common_context_t& c) -> bool {
        const auto tp = get_table_placement(c);

        bool has_VMAT_arc = false;
        for(const auto& beam : c.plan.get().dynamic_states){
            has_VMAT_arc = beam_is_vmat_arc(beam);
            if(has_VMAT_arc) break;
        }
        return has_VMAT_arc;
    };

    out.emplace_back();
    out.back().name = "VMAT arc collimator angles not degenerate";
    out.back().desc = "All VMAT arc collimator angles should be distinct to minimize optimization cost-function degeneracy.";
    out.back().name_regex = "^VMAT[-_ ]?arc[-_ ]?collimator[-_ ]?angles[-_ ]?not[-_ ]?degenerate$";
    out.back().check_impl = [](common_context_t& c) -> bool {
        const auto tp = get_table_placement(c);

        std::vector<double> coll_angles;
        for(const auto& beam : c.plan.get().dynamic_states){
            if(!beam_is_vmat_arc(beam)) continue;

            const auto coll_angle = get_collimator_angle(beam);
            if(coll_angle) coll_angles.push_back( coll_angle.value() );
        }

        bool passed = true;
        std::stringstream ss;
        if(coll_angles.empty()){
            ss << "no VMAT arcs detected";

        }else{
            for(const auto& ca : coll_angles) ss << std::to_string(ca) << " ";

            // Wrap angles around 360 and look for angle pairs nearer than tolerance.
            auto wrapped = coll_angles;
            for(const auto& a : coll_angles) wrapped.emplace_back(a + 360.0);
            const double tol = 10.0;
            passed = minimal_separation_is_larger_than(wrapped, tol);
        }

        c.table.get().inject(c.report_row, tp.expl_col, ss.str());
        return passed;
    };

    out.emplace_back();
    out.back().name = "jaw openings larger than";
    out.back().desc = "The X and Y jaws should be opened sufficiently to facilitate accurate dosimetric modeling."
                      " Minimum X and Y jaw openings (in mm) are required";
    out.back().name_regex = "^jaw[-_ ]?openings[-_ ]?larger[-_ ]?than$";
    out.back().check_impl = [](common_context_t& c) -> bool {
        const auto tp = get_table_placement(c);
        std::stringstream ss;

        // Get user-provided parameters.
        double X_min = 20.0;
        double Y_min = 20.0;
        if( (c.pf.get().parameters.size() == 2)
        &&  (c.pf.get().parameters[0].number)
        &&  (c.pf.get().parameters[1].number) ){
            X_min = c.pf.get().parameters[0].number.value();
            Y_min = c.pf.get().parameters[1].number.value();
        }else{
            throw std::invalid_argument("This function requires X and Y jaw arguments");
        }
        ss << X_min << "/" << Y_min << ": ";

        const auto nan = std::numeric_limits<double>::quiet_NaN();
        bool passed = true;
        for(const auto& beam : c.plan.get().dynamic_states){
            const auto openings = get_jaw_openings(beam);
            const auto actual_X_min = get_smallest_X_jaw_opening(openings);
            const auto actual_Y_min = get_smallest_Y_jaw_opening(openings);
            passed =  actual_X_min
                   && actual_Y_min
                   && std::isfinite(actual_X_min.value())
                   && std::isfinite(actual_Y_min.value())
                   && (X_min <= actual_X_min.value())
                   && (Y_min <= actual_Y_min.value());

            ss << actual_X_min.value_or(nan) << "/"
               << actual_Y_min.value_or(nan) << " ";
            if(!passed) break;
        }

        c.table.get().inject(c.report_row, tp.expl_col, ss.str());
        return passed;
    };

    // ...

    return out;
}

bool dispatch_checks( common_context_t& c ){
    const auto tp = get_table_placement(c);
    c.report_row = tp.empty_row;
    bool check_found = false;
    bool passed = false;

    for(const auto& check : get_checks()){
        if(std::regex_match(c.pf.get().name, Compile_Regex(check.name_regex))){
            check_found = true;
            c.table.get().inject(c.report_row, tp.title_col, check.name);
            passed = check.check_impl(c);
            break;
        }
    }
    if(!check_found){
        throw std::invalid_argument("Unable to find check matching '"_s + c.pf.get().name + "'");
    }

    c.table.get().inject(c.report_row, tp.pass_fail_col, (passed ? "pass" : "fail"));
    return passed;
}


OperationDoc OpArgDocValidateRTPlan(){
    OperationDoc out;
    out.name = "ValidateRTPlan";

    out.desc = 
        "This operation evaluates a radiotherapy treatment plan against user-specified criteria.";

    out.args.emplace_back();
    out.args.back() = TPWhitelistOpArgDoc();
    out.args.back().name = "RTPlanSelection";
    out.args.back().default_val = "last";


    std::string checks_list;
    for(const auto& c : get_checks()) checks_list += "\n'" + c.name + "' -- " + c.desc + "\n";
    out.args.emplace_back();
    out.args.back().name = "Checks";
    out.args.back().desc = "The specific checks to perform when evaluating the plan."
                           " This parameter will often contain a script with multiple checks."
                           " List of supported checks:"
                           "\n" + checks_list + "\n";
    out.args.back().default_val = "";
    out.args.back().expected = true;
    out.args.back().examples = { "require(){ check_A(); check_B(); ... }",
                                 "all_of(){ check_A(); check_B(); ... }",
                                 "any_of(){ check_A(); check_B(); ... }" };

    out.args.emplace_back();
    out.args.back() = STWhitelistOpArgDoc();
    out.args.back().name = "TableSelection";
    out.args.back().default_val = "last";

    return out;
}

bool ValidateRTPlan(Drover &DICOM_data,
                    const OperationArgPkg& OptArgs,
                    std::map<std::string, std::string>& InvocationMetadata,
                    const std::string& FilenameLex){

    Explicator X(FilenameLex);

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto RTPlanSelectionStr = OptArgs.getValueStr("RTPlanSelection").value();

    const auto TableSelectionStr = OptArgs.getValueStr("TableSelection").value();
    const auto ChecksStr = OptArgs.getValueStr("Checks").value();

    //-----------------------------------------------------------------------------------------------------------------

    const auto pfs = parse_functions(ChecksStr);
    if(pfs.empty()){
        throw std::invalid_argument("No checks specified, nothing to check");
    }

    auto TPs_all = All_TPs( DICOM_data );
    auto TPs = Whitelist( TPs_all, RTPlanSelectionStr );
    if(TPs.empty()){
        throw std::invalid_argument("No plans specified, nothing to check");
    }

    // Locate or create a table for the results.
    auto STs_all = All_STs( DICOM_data );
    auto STs = Whitelist( STs_all, TableSelectionStr );
    if(STs.empty()){
        DICOM_data.table_data.emplace_back( std::make_shared<Sparse_Table>() );
        auto* tab_ptr = &( DICOM_data.table_data.back()->table );
        auto l_meta = coalesce_metadata_for_basic_table(tab_ptr->metadata);
        l_meta.merge(tab_ptr->metadata);
        tab_ptr->metadata = l_meta;

        STs_all = All_STs( DICOM_data );
        STs = Whitelist( STs_all, TableSelectionStr );
    }
    if(STs.size() != 1u){
        throw std::invalid_argument("Multiple tables selected");
    }

    for(auto & tp_it : TPs){
        // Process each treatment plan separately.
        for(const auto& pf : pfs){
            common_context_t c {
                std::cref(pf),
                std::ref(*(*tp_it)),
                std::ref((*(STs.front()))->table),
                std::ref(DICOM_data),
                std::cref(OptArgs),
                std::ref(InvocationMetadata),
                std::cref(FilenameLex),
                0,
                0 };

            dispatch_checks(c);
        }
    }

    return true;
}

