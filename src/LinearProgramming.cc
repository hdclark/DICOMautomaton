//LinearProgramming.cc - A part of DICOMautomaton 2026. Written by hal clark.
//
// This file contains a linear programming solver using the simplex algorithm.

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>
#include <string>
#include <cstdint>

#include "doctest20251212/doctest.h"

#include "LinearProgramming.h"

namespace linear_programming {

std::string lp_status_to_string(lp_status s) {
    switch(s) {
        case lp_status::optimal:        return "optimal";
        case lp_status::unbounded:      return "unbounded";
        case lp_status::infeasible:     return "infeasible";
        case lp_status::max_iterations: return "max_iterations";
        case lp_status::numerical_error: return "numerical_error";
    }
    return "unknown";
}

lp_problem create_lp_problem(
    const std::vector<double> &objective,
    const std::vector<std::vector<double>> &constraints,
    const std::vector<double> &bounds) {
    
    lp_problem prob;
    prob.objective = objective;
    prob.constraints = constraints;
    prob.bounds = bounds;
    return prob;
}

// Simplex tableau-based solver.
//
// The tableau is structured as follows for a problem with n variables and m constraints:
//
// After adding slack variables, we have n + m variables total.
// The tableau has (m + 1) rows and (n + m + 1) columns:
//   - Rows 0 to m-1: constraint rows
//   - Row m: objective row (negated for maximization)
//   - Columns 0 to n+m-1: variable coefficients (original + slack)
//   - Column n+m: RHS values (b for constraints, objective value for last row)
//
lp_result solve_lp(const lp_problem &prob) {
    lp_result result;
    result.status = lp_status::numerical_error;
    result.iterations = 0;

    const int64_t n = static_cast<int64_t>(prob.objective.size());
    const int64_t m = static_cast<int64_t>(prob.constraints.size());

    if(n == 0) {
        result.status = lp_status::optimal;
        result.objective_value = 0.0;
        return result;
    }

    if(m == 0) {
        // No constraints - check if objective coefficients are all non-positive.
        bool all_non_positive = true;
        for(const auto &c : prob.objective) {
            if(c > prob.tolerance) {
                all_non_positive = false;
                break;
            }
        }
        if(all_non_positive) {
            result.status = lp_status::optimal;
            result.solution.resize(n, 0.0);
            result.objective_value = 0.0;
            return result;
        } else {
            result.status = lp_status::unbounded;
            return result;
        }
    }

    // Validate constraint dimensions.
    for(int64_t i = 0; i < m; ++i) {
        if(static_cast<int64_t>(prob.constraints[i].size()) != n) {
            return result; // numerical_error - dimension mismatch.
        }
    }
    if(static_cast<int64_t>(prob.bounds.size()) != m) {
        return result; // numerical_error - dimension mismatch.
    }

    // Check for negative RHS values (infeasibility without Big-M or two-phase).
    // For simplicity, we assume all b_i >= 0. If not, a more sophisticated
    // two-phase method would be needed.
    for(int64_t i = 0; i < m; ++i) {
        if(prob.bounds[i] < -prob.tolerance) {
            result.status = lp_status::infeasible;
            return result;
        }
    }

    // Build the simplex tableau.
    // Tableau size: (m + 1) rows x (n + m + 1) columns.
    const int64_t num_cols = n + m + 1;
    const int64_t num_rows = m + 1;
    std::vector<std::vector<double>> tableau(num_rows, std::vector<double>(num_cols, 0.0));

    // Fill constraint rows.
    for(int64_t i = 0; i < m; ++i) {
        for(int64_t j = 0; j < n; ++j) {
            tableau[i][j] = prob.constraints[i][j];
        }
        // Slack variable.
        tableau[i][n + i] = 1.0;
        // RHS.
        tableau[i][num_cols - 1] = prob.bounds[i];
    }

    // Fill objective row (negated for maximization in standard tableau form).
    for(int64_t j = 0; j < n; ++j) {
        tableau[m][j] = -prob.objective[j];
    }
    // Slack variables have zero coefficient in objective.
    // RHS of objective row starts at 0.

    // Track basic variables (initially the slack variables).
    std::vector<int64_t> basic_vars(m);
    for(int64_t i = 0; i < m; ++i) {
        basic_vars[i] = n + i;
    }

    // Main simplex loop.
    for(int64_t iter = 0; iter < prob.max_iterations; ++iter) {
        result.iterations = iter + 1;

        // Find entering variable (most negative coefficient in objective row).
        int64_t pivot_col = -1;
        double min_val = -prob.tolerance;
        for(int64_t j = 0; j < n + m; ++j) {
            if(tableau[m][j] < min_val) {
                min_val = tableau[m][j];
                pivot_col = j;
            }
        }

        // If no negative coefficient, we've reached optimality.
        if(pivot_col < 0) {
            result.status = lp_status::optimal;
            break;
        }

        // Find leaving variable (minimum ratio test).
        int64_t pivot_row = -1;
        double min_ratio = std::numeric_limits<double>::infinity();
        for(int64_t i = 0; i < m; ++i) {
            if(tableau[i][pivot_col] > prob.tolerance) {
                double ratio = tableau[i][num_cols - 1] / tableau[i][pivot_col];
                if(ratio < min_ratio) {
                    min_ratio = ratio;
                    pivot_row = i;
                }
            }
        }

        // If no valid pivot row, problem is unbounded.
        if(pivot_row < 0) {
            result.status = lp_status::unbounded;
            return result;
        }

        // Perform pivot operation.
        double pivot_element = tableau[pivot_row][pivot_col];
        
        // Normalize pivot row.
        for(int64_t j = 0; j < num_cols; ++j) {
            tableau[pivot_row][j] /= pivot_element;
        }

        // Eliminate pivot column in all other rows.
        for(int64_t i = 0; i < num_rows; ++i) {
            if(i != pivot_row) {
                double factor = tableau[i][pivot_col];
                for(int64_t j = 0; j < num_cols; ++j) {
                    tableau[i][j] -= factor * tableau[pivot_row][j];
                }
            }
        }

        // Update basic variable tracking.
        basic_vars[pivot_row] = pivot_col;
    }

    if(result.status != lp_status::optimal) {
        if(result.iterations >= prob.max_iterations) {
            result.status = lp_status::max_iterations;
        }
        return result;
    }

    // Extract solution.
    result.solution.resize(n, 0.0);
    for(int64_t i = 0; i < m; ++i) {
        int64_t var = basic_vars[i];
        if(var < n) {
            result.solution[var] = tableau[i][num_cols - 1];
        }
    }

    // Objective value is the RHS of the objective row.
    result.objective_value = tableau[m][num_cols - 1];

    return result;
}

// ============================================================================
// Unit tests
// ============================================================================

TEST_CASE("LinearProgramming_basic_maximization") {
    // Simple problem:
    // Maximize: 3*x1 + 2*x2
    // Subject to:
    //   x1 + x2 <= 4
    //   x1 <= 2
    //   x2 <= 3
    //   x1, x2 >= 0
    //
    // Optimal solution: x1 = 2, x2 = 2, objective = 10

    lp_problem prob;
    prob.objective = {3.0, 2.0};
    prob.constraints = {
        {1.0, 1.0},
        {1.0, 0.0},
        {0.0, 1.0}
    };
    prob.bounds = {4.0, 2.0, 3.0};

    lp_result res = solve_lp(prob);

    REQUIRE(res.status == lp_status::optimal);
    REQUIRE(res.solution.size() == 2);
    CHECK(std::abs(res.solution[0] - 2.0) < 1e-6);
    CHECK(std::abs(res.solution[1] - 2.0) < 1e-6);
    CHECK(std::abs(res.objective_value - 10.0) < 1e-6);
}

TEST_CASE("LinearProgramming_single_variable") {
    // Maximize: 5*x
    // Subject to:
    //   x <= 10
    //   x >= 0
    //
    // Optimal: x = 10, objective = 50

    lp_problem prob;
    prob.objective = {5.0};
    prob.constraints = {{1.0}};
    prob.bounds = {10.0};

    lp_result res = solve_lp(prob);

    REQUIRE(res.status == lp_status::optimal);
    REQUIRE(res.solution.size() == 1);
    CHECK(std::abs(res.solution[0] - 10.0) < 1e-6);
    CHECK(std::abs(res.objective_value - 50.0) < 1e-6);
}

TEST_CASE("LinearProgramming_unbounded") {
    // Maximize: x
    // Subject to: (no constraints)
    // This should be unbounded.

    lp_problem prob;
    prob.objective = {1.0};
    prob.constraints = {};
    prob.bounds = {};

    lp_result res = solve_lp(prob);

    CHECK(res.status == lp_status::unbounded);
}

TEST_CASE("LinearProgramming_no_variables") {
    // Edge case: empty problem.
    lp_problem prob;
    prob.objective = {};
    prob.constraints = {};
    prob.bounds = {};

    lp_result res = solve_lp(prob);

    CHECK(res.status == lp_status::optimal);
    CHECK(res.objective_value == 0.0);
}

TEST_CASE("LinearProgramming_diet_problem") {
    // Classic diet problem variant:
    // Maximize: x1 + x2 (total food units)
    // Subject to:
    //   2*x1 + x2 <= 20 (nutrient A)
    //   x1 + 3*x2 <= 30 (nutrient B)
    //   x1, x2 >= 0
    //
    // Optimal: x1 = 6, x2 = 8, objective = 14

    lp_problem prob;
    prob.objective = {1.0, 1.0};
    prob.constraints = {
        {2.0, 1.0},
        {1.0, 3.0}
    };
    prob.bounds = {20.0, 30.0};

    lp_result res = solve_lp(prob);

    REQUIRE(res.status == lp_status::optimal);
    REQUIRE(res.solution.size() == 2);
    CHECK(std::abs(res.solution[0] - 6.0) < 1e-6);
    CHECK(std::abs(res.solution[1] - 8.0) < 1e-6);
    CHECK(std::abs(res.objective_value - 14.0) < 1e-6);
}

TEST_CASE("LinearProgramming_zero_objective") {
    // All zero objective coefficients.
    // Maximize: 0*x1 + 0*x2
    // Subject to: x1 + x2 <= 5
    //
    // Any feasible solution is optimal with value 0.

    lp_problem prob;
    prob.objective = {0.0, 0.0};
    prob.constraints = {{1.0, 1.0}};
    prob.bounds = {5.0};

    lp_result res = solve_lp(prob);

    REQUIRE(res.status == lp_status::optimal);
    CHECK(std::abs(res.objective_value) < 1e-6);
}

TEST_CASE("LinearProgramming_three_variables") {
    // Maximize: 2*x1 + 3*x2 + 4*x3
    // Subject to:
    //   x1 + x2 + x3 <= 100
    //   2*x1 + x2 <= 80
    //   x2 + 2*x3 <= 60
    //   x1, x2, x3 >= 0

    lp_problem prob;
    prob.objective = {2.0, 3.0, 4.0};
    prob.constraints = {
        {1.0, 1.0, 1.0},
        {2.0, 1.0, 0.0},
        {0.0, 1.0, 2.0}
    };
    prob.bounds = {100.0, 80.0, 60.0};

    lp_result res = solve_lp(prob);

    REQUIRE(res.status == lp_status::optimal);
    REQUIRE(res.solution.size() == 3);
    // Verify solution is feasible.
    double c1 = res.solution[0] + res.solution[1] + res.solution[2];
    double c2 = 2.0 * res.solution[0] + res.solution[1];
    double c3 = res.solution[1] + 2.0 * res.solution[2];
    CHECK(c1 <= 100.0 + 1e-6);
    CHECK(c2 <= 80.0 + 1e-6);
    CHECK(c3 <= 60.0 + 1e-6);
    // Verify objective is computed correctly.
    double obj = 2.0 * res.solution[0] + 3.0 * res.solution[1] + 4.0 * res.solution[2];
    CHECK(std::abs(res.objective_value - obj) < 1e-6);
}

TEST_CASE("LinearProgramming_status_to_string") {
    CHECK(lp_status_to_string(lp_status::optimal) == "optimal");
    CHECK(lp_status_to_string(lp_status::unbounded) == "unbounded");
    CHECK(lp_status_to_string(lp_status::infeasible) == "infeasible");
    CHECK(lp_status_to_string(lp_status::max_iterations) == "max_iterations");
    CHECK(lp_status_to_string(lp_status::numerical_error) == "numerical_error");
}

} // namespace linear_programming

