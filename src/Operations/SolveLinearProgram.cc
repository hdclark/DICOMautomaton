//SolveLinearProgram.cc - A part of DICOMautomaton 2026. Written by hal clark.

#include <algorithm>
#include <optional>
#include <fstream>
#include <iterator>
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
#include "../Tables.h"
#include "../Regex_Selectors.h"
#include "../Thread_Pool.h"
#include "../String_Parsing.h"
#include "../Metadata.h"
#include "../LinearProgramming.h"

#include "SolveLinearProgram.h"


OperationDoc OpArgDocSolveLinearProgram(){
    OperationDoc out;
    out.name = "SolveLinearProgram";

    out.tags.emplace_back("category: table processing");
    out.tags.emplace_back("category: optimization");
    out.tags.emplace_back("category: mathematical");

    out.desc = 
        "This operation solves a linear programming problem using the simplex algorithm."
        " The problem is formulated as a maximization problem:"
        " Maximize c^T * x, subject to A * x <= b and x >= 0."
        "\n\n"
        "The objective function coefficients (c), constraint matrix (A), and constraint"
        " bounds (b) are provided as semicolon-separated lists."
        " Results are written to a new table that is imbued into the Drover object.";

    out.notes.emplace_back(
        "The linear programming solver uses the simplex algorithm, which may not be"
        " suitable for very large problems or problems with numerical instability."
    );

    out.notes.emplace_back(
        "All decision variables are assumed to be non-negative (x >= 0)."
    );

    out.notes.emplace_back(
        "Constraints are in the form A*x <= b (less-than-or-equal-to)."
    );

    out.args.emplace_back();
    out.args.back().name = "Objective";
    out.args.back().desc = "The objective function coefficients as a semicolon-separated list of values."
                           " These define the cost function to be maximized: c^T * x."
                           " The number of values determines the number of decision variables.";
    out.args.back().default_val = "1.0;1.0";
    out.args.back().expected = true;
    out.args.back().examples = { "3.0;2.0", "1.0;1.0;1.0", "5.0" };

    out.args.emplace_back();
    out.args.back().name = "Constraints";
    out.args.back().desc = "The constraint matrix coefficients."
                           " Each row is separated by a pipe '|', and values within a row"
                           " are separated by semicolons."
                           " Each row must have the same number of values as Objective."
                           " These define the left-hand side of constraints: A * x.";
    out.args.back().default_val = "1.0;1.0";
    out.args.back().expected = true;
    out.args.back().examples = { "1.0;1.0|1.0;0.0|0.0;1.0", "2.0;1.0|1.0;3.0", "1.0" };

    out.args.emplace_back();
    out.args.back().name = "Bounds";
    out.args.back().desc = "The constraint bounds (right-hand side) as a semicolon-separated list."
                           " The number of values must match the number of constraint rows."
                           " These define b in: A * x <= b.";
    out.args.back().default_val = "4.0";
    out.args.back().expected = true;
    out.args.back().examples = { "4.0;2.0;3.0", "20.0;30.0", "100.0" };

    out.args.emplace_back();
    out.args.back().name = "TableLabel";
    out.args.back().desc = "A label to attach to the results table.";
    out.args.back().default_val = "linear_program_result";
    out.args.back().expected = true;
    out.args.back().examples = { "lp_result", "optimization_output", "solution" };

    out.args.emplace_back();
    out.args.back().name = "MaxIterations";
    out.args.back().desc = "Maximum number of simplex iterations before terminating.";
    out.args.back().default_val = "1000";
    out.args.back().expected = true;
    out.args.back().examples = { "100", "1000", "10000" };

    return out;
}

bool SolveLinearProgram(Drover &DICOM_data,
                        const OperationArgPkg& OptArgs,
                        std::map<std::string, std::string>& /*InvocationMetadata*/,
                        const std::string& FilenameLex){

    Explicator X(FilenameLex);

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ObjectiveStr = OptArgs.getValueStr("Objective").value();
    const auto ConstraintsStr = OptArgs.getValueStr("Constraints").value();
    const auto BoundsStr = OptArgs.getValueStr("Bounds").value();
    const auto TableLabel = OptArgs.getValueStr("TableLabel").value();
    const auto MaxIterationsStr = OptArgs.getValueStr("MaxIterations").value();

    //-----------------------------------------------------------------------------------------------------------------

    // Parse objective coefficients.
    std::vector<double> objective;
    {
        auto tokens = SplitStringToVector(ObjectiveStr, ';', 'd');
        for(const auto &t : tokens){
            try {
                objective.push_back(std::stod(t));
            } catch(...) {
                throw std::invalid_argument("Unable to parse objective coefficient: '" + t + "'");
            }
        }
    }

    const int64_t num_vars = static_cast<int64_t>(objective.size());
    if(num_vars == 0){
        throw std::invalid_argument("At least one objective coefficient is required.");
    }

    // Parse constraint matrix.
    std::vector<std::vector<double>> constraints;
    {
        auto rows = SplitStringToVector(ConstraintsStr, '|', 'd');
        for(const auto &row : rows){
            std::vector<double> row_vals;
            auto tokens = SplitStringToVector(row, ';', 'd');
            for(const auto &t : tokens){
                try {
                    row_vals.push_back(std::stod(t));
                } catch(...) {
                    throw std::invalid_argument("Unable to parse constraint coefficient: '" + t + "'");
                }
            }
            if(static_cast<int64_t>(row_vals.size()) != num_vars){
                throw std::invalid_argument("Constraint row has " + std::to_string(row_vals.size()) +
                    " values, but objective has " + std::to_string(num_vars) + " variables.");
            }
            constraints.push_back(row_vals);
        }
    }

    const int64_t num_constraints = static_cast<int64_t>(constraints.size());

    // Parse bounds.
    std::vector<double> bounds;
    {
        auto tokens = SplitStringToVector(BoundsStr, ';', 'd');
        for(const auto &t : tokens){
            try {
                bounds.push_back(std::stod(t));
            } catch(...) {
                throw std::invalid_argument("Unable to parse bound value: '" + t + "'");
            }
        }
    }

    if(static_cast<int64_t>(bounds.size()) != num_constraints){
        throw std::invalid_argument("Number of bounds (" + std::to_string(bounds.size()) +
            ") does not match number of constraint rows (" + std::to_string(num_constraints) + ").");
    }

    // Parse max iterations.
    int64_t max_iterations = 1000;
    try {
        max_iterations = std::stoll(MaxIterationsStr);
    } catch(...) {
        throw std::invalid_argument("Unable to parse MaxIterations: '" + MaxIterationsStr + "'");
    }
    if(max_iterations <= 0){
        throw std::invalid_argument("MaxIterations must be positive.");
    }

    // Set up the LP problem.
    linear_programming::lp_problem prob;
    prob.objective = objective;
    prob.constraints = constraints;
    prob.bounds = bounds;
    prob.max_iterations = max_iterations;

    // Solve the problem.
    YLOGINFO("Solving linear program with " << num_vars << " variables and " << num_constraints << " constraints");
    auto result = linear_programming::solve_lp(prob);

    YLOGINFO("LP result: status = " << linear_programming::lp_status_to_string(result.status)
             << ", iterations = " << result.iterations
             << ", objective value = " << result.objective_value);

    // Create the result table.
    const auto NormalizedTableLabel = X(TableLabel);

    auto meta = coalesce_metadata_for_basic_table({}, meta_evolve::iterate);
    DICOM_data.table_data.emplace_back(std::make_unique<Sparse_Table>());
    auto &table = DICOM_data.table_data.back()->table;
    table.metadata = meta;
    table.metadata["TableLabel"] = TableLabel;
    table.metadata["NormalizedTableLabel"] = NormalizedTableLabel;
    table.metadata["Description"] = "Linear programming solution";

    // Write headers.
    table.inject(0, 0, "Key");
    table.inject(0, 1, "Value");

    int64_t row = 1;

    // Write status.
    table.inject(row, 0, "Status");
    table.inject(row, 1, linear_programming::lp_status_to_string(result.status));
    ++row;

    // Write iterations.
    table.inject(row, 0, "Iterations");
    table.inject(row, 1, std::to_string(result.iterations));
    ++row;

    // Write objective value.
    table.inject(row, 0, "ObjectiveValue");
    table.inject(row, 1, std::to_string(result.objective_value));
    ++row;

    // Write solution values.
    for(size_t i = 0; i < result.solution.size(); ++i){
        table.inject(row, 0, "x" + std::to_string(i + 1));
        table.inject(row, 1, std::to_string(result.solution[i]));
        ++row;
    }

    // Return success if optimal solution found.
    return (result.status == linear_programming::lp_status::optimal);
}
