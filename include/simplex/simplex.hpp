#pragma once

#include <cstddef>
#include <string_view>
#include <vector>

namespace simplex {

enum class OptimizationDirection {
    maximize,
    minimize,
};

enum class SolveStatus {
    optimal,
    infeasible,
    unbounded,
    iteration_limit,
    invalid_input,
};

struct LinearProgram {
    std::vector<double> objective;
    std::vector<std::vector<double>> constraints;
    std::vector<double> bounds;
    OptimizationDirection direction{OptimizationDirection::maximize};
};

struct SolveOptions {
    double tolerance{1e-9};
    std::size_t max_iterations{10'000};
};

struct SolveResult {
    SolveStatus status{SolveStatus::invalid_input};
    std::vector<double> variables;
    double objective_value{0.0};
    std::size_t iterations{0};
};

[[nodiscard]] SolveResult solve(
    const LinearProgram& program,
    const SolveOptions& options = {});

[[nodiscard]] constexpr std::string_view to_string(SolveStatus status) noexcept {
    switch (status) {
    case SolveStatus::optimal:
        return "optimal";
    case SolveStatus::infeasible:
        return "infeasible";
    case SolveStatus::unbounded:
        return "unbounded";
    case SolveStatus::iteration_limit:
        return "iteration_limit";
    case SolveStatus::invalid_input:
        return "invalid_input";
    }
    return "invalid_input";
}

} // namespace simplex
