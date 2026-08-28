//LinearProgramming.h - A part of DICOMautomaton 2026. Written by hal clark.
//
// This file contains a linear programming solver using the simplex algorithm.

#pragma once

#include <vector>
#include <string>
#include <optional>

namespace linear_programming {

// Result status of the linear program optimization.
enum class lp_status {
    optimal,           // Optimal solution found.
    unbounded,         // Problem is unbounded.
    infeasible,        // Problem is infeasible.
    max_iterations,    // Maximum iterations reached.
    numerical_error,   // Numerical issues encountered.
};

std::string lp_status_to_string(lp_status s);

// Result structure containing the solution.
struct lp_result {
    lp_status status = lp_status::numerical_error;
    std::vector<double> solution;  // Optimal values for decision variables.
    double objective_value = 0.0;  // Optimal objective function value.
    int64_t iterations = 0;        // Number of iterations performed.
};

// Linear programming problem definition for maximization.
//
// Maximizes: c^T * x
// Subject to: A * x <= b
//             x >= 0 (non-negativity constraints)
//
// Where:
//   c = objective coefficients (size n)
//   A = constraint matrix (size m x n)
//   b = constraint bounds (size m)
//   x = decision variables (size n)
//
struct lp_problem {
    std::vector<double> objective;              // Objective coefficients (c).
    std::vector<std::vector<double>> constraints; // Constraint matrix (A).
    std::vector<double> bounds;                 // Constraint bounds (b).
    int64_t max_iterations = 1000;              // Maximum simplex iterations.
    double tolerance = 1.0e-10;                 // Numerical tolerance.
};

// Solve a linear programming problem using the simplex method.
//
// The problem is formulated as:
//   Maximize: c^T * x
//   Subject to: A * x <= b
//               x >= 0
//
lp_result solve_lp(const lp_problem &prob);

// Convenience function to create a simple LP problem.
lp_problem create_lp_problem(
    const std::vector<double> &objective,
    const std::vector<std::vector<double>> &constraints,
    const std::vector<double> &bounds);

} // namespace linear_programming

