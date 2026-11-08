#include "simplex/simplex.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string_view>
#include <vector>

namespace {

void expect(bool condition, std::string_view description) {
    if (!condition) {
        std::cerr << "Failed: " << description << '\n';
        std::exit(EXIT_FAILURE);
    }
}

bool near(double actual, double expected) {
    return std::abs(actual - expected) <= 1e-9 * std::max(1.0, std::abs(expected));
}

} // namespace

int main() {
    using simplex::LinearProgram;
    using simplex::OptimizationDirection;
    using simplex::SolveStatus;

    const LinearProgram example{{3, 2}, {{1, 1}, {1, 0}, {0, 1}}, {4, 2, 3}};
    const auto optimum = simplex::solve(example);
    expect(optimum.status == SolveStatus::optimal && optimum.variables.size() == 2,
           "worked example has two optimal coordinates");
    expect(near(optimum.variables[0], 2) && near(optimum.variables[1], 2) &&
               near(optimum.objective_value, 10),
           "worked example optimum");
    const auto limited = simplex::solve(example, {.max_iterations = 1});
    expect(limited.status == SolveStatus::iteration_limit && limited.iterations == 1 &&
               limited.variables.empty(),
           "iteration limit counts completed pivots");

    const LinearProgram minimum{{2, 1}, {{-1, -1}, {-1, 0}, {1, 0}, {0, 1}},
                                {-3, -1, 3, 3}, OptimizationDirection::minimize};
    const auto phase_one = simplex::solve(minimum);
    expect(phase_one.status == SolveStatus::optimal && phase_one.variables.size() == 2,
           "phase one finds a feasible basis");
    expect(near(phase_one.variables[0], 1) && near(phase_one.variables[1], 2) &&
               near(phase_one.objective_value, 4),
           "phase two recovers the original minimization objective");

    const LinearProgram phase_one_roundoff{{7, -4},
                                           {{1, 0}, {0, 1}, {-3, 3}, {3, -2},
                                            {1, -1}, {1, -3}, {-4, -3}},
                                           {1, 3, 9, -4, 0, -9, -5}};
    const auto rounded_basis = simplex::solve(phase_one_roundoff);
    expect(rounded_basis.status == SolveStatus::optimal && rounded_basis.variables.size() == 2 &&
               near(rounded_basis.variables[0], 0) && near(rounded_basis.variables[1], 3) &&
               near(rounded_basis.objective_value, -12),
           "degenerate phase one tolerates arithmetic roundoff at a feasible point");

    expect(simplex::solve({{1}, {{1}, {-1}}, {1, -2}}).status == SolveStatus::infeasible,
           "contradictory bounds");
    expect(simplex::solve({{1}, {{-1}}, {-1}}).status == SolveStatus::unbounded,
           "unbounded objective after phase one");
    expect(simplex::solve({{1}, {}, {}}).status == SolveStatus::unbounded,
           "unbounded without explicit constraints");
    const auto zero_pivots = simplex::solve({{-1}, {}, {}}, {.max_iterations = 0});
    expect(zero_pivots.status == SolveStatus::optimal && zero_pivots.iterations == 0 &&
               zero_pivots.variables == std::vector<double>{0},
           "zero pivot limit permits an immediate optimum");

    const LinearProgram degenerate{{10, -57, -9, -24},
                                   {{0.5, -5.5, -2.5, 9},
                                    {0.5, -1.5, -0.5, 1},
                                    {1, 0, 0, 0}},
                                   {0, 0, 1}};
    const auto blands_rule = simplex::solve(degenerate, {.max_iterations = 20});
    expect(blands_rule.status == SolveStatus::optimal && near(blands_rule.objective_value, 1),
           "degenerate cycling example terminates");
    const auto scaled = simplex::solve({{1}, {{1e-12}}, {1}});
    expect(scaled.status == SolveStatus::optimal && near(scaled.objective_value, 1e12),
           "small coefficient still bounds the objective");
    expect(simplex::solve({{1}, {{0}}, {-1e-12}}).status == SolveStatus::infeasible,
           "tiny negative bound on a zero row is infeasible");

    const double infinity = std::numeric_limits<double>::infinity();
    expect(simplex::solve({}).status == SolveStatus::invalid_input,
           "empty objective");
    expect(simplex::solve({{1, 2}, {{1}}, {1}}).status == SolveStatus::invalid_input,
           "inconsistent matrix shape");
    expect(simplex::solve({{1}, {{1}}, {infinity}}).status == SolveStatus::invalid_input,
           "nonfinite coefficient");
    expect(simplex::solve(example, {.tolerance = 0.0}).status == SolveStatus::invalid_input,
           "nonpositive tolerance");
    const double largest = std::numeric_limits<double>::max();
    expect(simplex::solve({{largest}, {{1}}, {2}}).status == SolveStatus::numerical_failure,
           "overflowing result receives a numerical diagnostic");
    expect(simplex::to_string(SolveStatus::numerical_failure) == "numerical_failure",
           "numerical diagnostic has a stable name");
}
