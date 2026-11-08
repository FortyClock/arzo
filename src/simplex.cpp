#include "simplex/simplex.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace simplex {
namespace {

using Number = long double;
using Coefficients = std::vector<Number>;

struct Row {
    explicit Row(std::size_t size) : values(size) {}

    Number& operator[](std::size_t column) { return values[column]; }
    Number operator[](std::size_t column) const { return values[column]; }
    Number& rhs() { return values.back(); }
    Number rhs() const { return values.back(); }

    Coefficients values;
    bool rounded{false};
};

bool has_valid_input(const LinearProgram& program, const SolveOptions& options) {
    if (program.objective.empty() ||
        program.constraints.size() != program.bounds.size() ||
        !std::isfinite(options.tolerance) || options.tolerance <= 0.0 ||
        (program.direction != OptimizationDirection::maximize &&
         program.direction != OptimizationDirection::minimize)) {
        return false;
    }

    for (const double coefficient : program.objective) {
        if (!std::isfinite(coefficient)) {
            return false;
        }
    }
    for (const auto& row : program.constraints) {
        if (row.size() != program.objective.size()) {
            return false;
        }
        for (const double coefficient : row) {
            if (!std::isfinite(coefficient)) {
                return false;
            }
        }
    }
    for (const double bound : program.bounds) {
        if (!std::isfinite(bound)) {
            return false;
        }
    }
    return true;
}

Number row_scale(const std::vector<double>& coefficients, double bound) {
    Number scale = std::abs(static_cast<Number>(bound));
    for (const double coefficient : coefficients) {
        scale = std::max(scale, std::abs(static_cast<Number>(coefficient)));
    }
    if (scale == 0.0L) {
        return 1.0L;
    }
    // A binary scale keeps the original significands during normalization.
    int exponent = 0;
    std::frexp(scale, &exponent);
    return std::ldexp(1.0L, exponent - 1);
}

bool eliminate_column(Row& target, const Row& pivot_row, std::size_t column) {
    const Number factor = target[column];
    if (factor == 0.0L) {
        return true;
    }

    constexpr Number roundoff = 8.0L * std::numeric_limits<Number>::epsilon();
    const bool approximate = target.rounded || pivot_row.rounded;
    bool rounded = approximate;
    for (std::size_t j = 0; j < target.values.size(); ++j) {
        if (j == column || pivot_row[j] == 0.0L) {
            continue;
        }
        const Number previous = target[j];
        const Number updated = std::fma(-factor, pivot_row[j], previous);
        if (!std::isfinite(updated) || (previous == 0.0L && updated == 0.0L)) {
            return false;
        }
        // Preserve an accurately computed small difference of exact operands.
        target[j] = approximate && std::abs(updated) <= roundoff * std::abs(previous)
            ? 0.0L : updated;
        if (!rounded) {
            const Number product = factor * pivot_row[j];
            // With an exact product, the larger addend recovers the smaller one
            // exactly if the subtraction did not lose low-order bits.
            const bool exact_sum = std::abs(previous) >= std::abs(product)
                ? previous - updated == product
                : updated + product == previous;
            rounded = product == 0.0L || !std::isfinite(product) ||
                std::fma(factor, pivot_row[j], -product) != 0.0L || !exact_sum;
        }
    }
    target[column] = 0.0L;
    target.rounded = rounded;
    return true;
}

bool satisfies_constraints(const LinearProgram& program, const std::vector<double>& variables,
                           Number tolerance);

class Tableau {
public:
    Tableau(std::size_t row_count, std::size_t column_count, std::size_t limit)
        : rows_(row_count, Row(column_count + 1)),
          objective_(column_count + 1),
          basis_(row_count),
          limit_(limit) {}

    bool initialize(const LinearProgram& program, std::size_t artificial_start) {
        const std::size_t variables = program.objective.size();
        std::size_t artificial_column = artificial_start;
        for (std::size_t i = 0; i < rows_.size(); ++i) {
            const auto& coefficients = program.constraints[i];
            const double bound = program.bounds[i];
            const Number scale = row_scale(coefficients, bound);
            const Number sign = bound < 0.0 ? -1.0L : 1.0L;
            Row& row = rows_[i];
            for (std::size_t j = 0; j < variables; ++j) {
                row[j] = sign * static_cast<Number>(coefficients[j]) / scale;
                if (coefficients[j] != 0.0 && row[j] == 0.0L) {
                    return false;
                }
                row.rounded = row.rounded || std::fpclassify(row[j]) == FP_SUBNORMAL;
            }
            row.rhs() = std::abs(static_cast<Number>(bound)) / scale;
            if (bound != 0.0 && row.rhs() == 0.0L) {
                return false;
            }
            row.rounded = row.rounded || std::fpclassify(row.rhs()) == FP_SUBNORMAL;
            row[variables + i] = sign;
            basis_[i] = variables + i;
            if (bound < 0.0) {
                row[artificial_column] = 1.0L;
                basis_[i] = artificial_column++;
            }
        }
        return true;
    }

    bool set_objective(const Coefficients& costs) {
        objective_.rhs() = 0.0L;
        objective_.rounded = false;
        for (std::size_t j = 0; j < costs.size(); ++j) {
            objective_[j] = -costs[j];
        }
        for (std::size_t i = 0; i < rows_.size(); ++i) {
            if (!eliminate_column(objective_, rows_[i], basis_[i])) {
                return false;
            }
        }
        return true;
    }

    SolveStatus optimize() {
        for (;;) {
            const std::size_t entering = entering_column();
            if (entering == objective_.values.size() - 1) {
                return SolveStatus::optimal;
            }
            std::size_t leaving = rows_.size();
            Number min_ratio = 0.0L;
            for (std::size_t i = 0; i < rows_.size(); ++i) {
                const Number coefficient = rows_[i][entering];
                if (coefficient <= 0.0L) {
                    continue;
                }
                const Number bound = rows_[i].rhs();
                const Number ratio = bound / coefficient;
                if (!std::isfinite(ratio) || (bound != 0.0L && ratio == 0.0L)) {
                    return SolveStatus::numerical_failure;
                }
                if (leaving == rows_.size() || ratio < min_ratio ||
                    (ratio == min_ratio && basis_[i] < basis_[leaving])) {
                    leaving = i;
                    min_ratio = ratio;
                }
            }
            if (leaving == rows_.size()) {
                return SolveStatus::unbounded;
            }
            const SolveStatus status = pivot(leaving, entering);
            if (status != SolveStatus::optimal) {
                return status;
            }
        }
    }

    SolveStatus remove_artificial_variables(std::size_t artificial_start,
                                            const LinearProgram& program) {
        for (std::size_t i = 0; i < rows_.size(); ++i) {
            if (basis_[i] < artificial_start || rows_[i].rhs() == 0.0L) {
                continue;
            }
            if (objective_.rhs() < 0.0L) {
                return SolveStatus::infeasible;
            }
            // Cancellation can make the auxiliary objective and artificial
            // basic values disagree. Check the original inequalities first.
            std::vector<double> candidate(program.objective.size());
            if (!read_variables(candidate) || !satisfies_constraints(program, candidate, 0.0L)) {
                return SolveStatus::numerical_failure;
            }
            for (std::size_t j = i; j < rows_.size(); ++j) {
                if (basis_[j] >= artificial_start) {
                    rows_[j].rhs() = 0.0L;
                    rows_[j].rounded = true;
                }
            }
            break;
        }

        std::size_t i = 0;
        while (i < rows_.size()) {
            if (basis_[i] < artificial_start) {
                ++i;
                continue;
            }
            const Row& row = rows_[i];
            std::size_t entering = 0;
            while (entering < artificial_start && row[entering] == 0.0L) {
                ++entering;
            }
            if (entering == artificial_start) {
                // With artificial variables set to zero this row is 0 = 0.
                rows_.erase(rows_.begin() + static_cast<std::ptrdiff_t>(i));
                basis_.erase(basis_.begin() + static_cast<std::ptrdiff_t>(i));
                continue;
            }
            const SolveStatus status = pivot(i, entering);
            if (status != SolveStatus::optimal) {
                return status;
            }
            ++i;
        }
        for (Row& row : rows_) {
            row[artificial_start] = row.rhs();
            row.values.resize(artificial_start + 1);
        }
        objective_.values.resize(artificial_start + 1);
        return SolveStatus::optimal;
    }

    bool read_variables(std::vector<double>& variables) const {
        for (std::size_t i = 0; i < rows_.size(); ++i) {
            if (basis_[i] < variables.size() &&
                !convert_result(rows_[i].rhs(), variables[basis_[i]])) {
                return false;
            }
        }
        return true;
    }

    std::size_t iterations() const {
        return iterations_;
    }

    static bool convert_result(Number value, double& output) {
        if (!std::isfinite(value) || std::abs(value) > std::numeric_limits<double>::max()) {
            return false;
        }
        output = static_cast<double>(value);
        return value == 0.0L || output != 0.0;
    }

private:
    std::size_t entering_column() const {
        const std::size_t end = objective_.values.size() - 1;
        for (std::size_t j = 0; j < end; ++j) {
            if (objective_[j] < 0.0L) {
                return j;
            }
        }
        return end;
    }

    SolveStatus pivot(std::size_t leaving, std::size_t entering) {
        if (iterations_ == limit_) {
            return SolveStatus::iteration_limit;
        }
        Row& pivot_row = rows_[leaving];
        const Number pivot_element = pivot_row[entering];
        for (Number& value : pivot_row.values) {
            const Number normalized = value / pivot_element;
            if (!std::isfinite(normalized) || (value != 0.0L && normalized == 0.0L)) {
                return SolveStatus::numerical_failure;
            }
            pivot_row.rounded = pivot_row.rounded ||
                std::fma(normalized, pivot_element, -value) != 0.0L;
            value = normalized;
        }
        for (std::size_t i = 0; i < rows_.size(); ++i) {
            if (i != leaving &&
                (!eliminate_column(rows_[i], pivot_row, entering) || rows_[i].rhs() < 0.0L)) {
                return SolveStatus::numerical_failure;
            }
        }
        if (!eliminate_column(objective_, pivot_row, entering)) {
            return SolveStatus::numerical_failure;
        }
        basis_[leaving] = entering;
        ++iterations_;
        return SolveStatus::optimal;
    }

    std::vector<Row> rows_;
    Row objective_;
    std::vector<std::size_t> basis_;
    std::size_t limit_;
    std::size_t iterations_{0};
};

bool satisfies_constraints(const LinearProgram& program, const std::vector<double>& variables,
                           Number tolerance) {
    for (std::size_t i = 0; i < program.constraints.size(); ++i) {
        const auto& coefficients = program.constraints[i];
        const Number scale = row_scale(coefficients, program.bounds[i]);
        const Number bound = static_cast<Number>(program.bounds[i]) / scale;
        Number activity = 0.0L;
        Number magnitude = std::abs(bound);
        for (std::size_t j = 0; j < variables.size(); ++j) {
            const Number coefficient = static_cast<Number>(coefficients[j]) / scale;
            const Number term = coefficient * variables[j];
            if (coefficients[j] != 0.0 && variables[j] != 0.0 && term == 0.0L) {
                return false;
            }
            activity += term;
            magnitude += std::abs(term);
        }
        if (!std::isfinite(activity) || !std::isfinite(magnitude) ||
            (activity > bound &&
             (tolerance == 0.0L || (activity - bound) / magnitude > tolerance))) {
            return false;
        }
    }
    return true;
}

} // namespace

SolveResult solve(const LinearProgram& program, const SolveOptions& options) {
    if (!has_valid_input(program, options)) {
        return {};
    }

    const std::size_t variables = program.objective.size();
    const std::size_t rows = program.bounds.size();
    const std::size_t nonartificial_columns = variables + rows;
    const std::size_t artificial_columns = static_cast<std::size_t>(
        std::count_if(program.bounds.begin(), program.bounds.end(),
                      [](double bound) { return bound < 0.0; }));
    Tableau tableau(rows, nonartificial_columns + artificial_columns, options.max_iterations);
    if (!tableau.initialize(program, nonartificial_columns)) {
        return {SolveStatus::numerical_failure, {}, 0.0, tableau.iterations()};
    }

    if (artificial_columns != 0) {
        Coefficients phase_one_costs(nonartificial_columns + artificial_columns);
        std::fill(phase_one_costs.end() - static_cast<std::ptrdiff_t>(artificial_columns),
                  phase_one_costs.end(), -1.0L);
        if (!tableau.set_objective(phase_one_costs)) {
            return {SolveStatus::numerical_failure, {}, 0.0, tableau.iterations()};
        }
        const SolveStatus status = tableau.optimize();
        if (status != SolveStatus::optimal) {
            // The auxiliary objective is bounded above by zero.
            return {status == SolveStatus::unbounded ? SolveStatus::numerical_failure : status,
                    {}, 0.0, tableau.iterations()};
        }
        const SolveStatus cleanup = tableau.remove_artificial_variables(nonartificial_columns, program);
        if (cleanup != SolveStatus::optimal) {
            return {cleanup, {}, 0.0, tableau.iterations()};
        }
    }

    Coefficients costs(nonartificial_columns);
    const Number sign = program.direction == OptimizationDirection::maximize ? 1.0L : -1.0L;
    for (std::size_t j = 0; j < variables; ++j) {
        costs[j] = sign * program.objective[j];
    }
    if (!tableau.set_objective(costs)) {
        return {SolveStatus::numerical_failure, {}, 0.0, tableau.iterations()};
    }
    const SolveStatus status = tableau.optimize();
    if (status != SolveStatus::optimal) {
        return {status, {}, 0.0, tableau.iterations()};
    }

    std::vector<double> result(variables);
    if (!tableau.read_variables(result) || !satisfies_constraints(program, result, options.tolerance)) {
        return {SolveStatus::numerical_failure, {}, 0.0, tableau.iterations()};
    }
    Number objective = 0.0L;
    for (std::size_t j = 0; j < variables; ++j) {
        objective = std::fma(static_cast<Number>(program.objective[j]), result[j], objective);
    }
    double value = 0.0;
    if (!Tableau::convert_result(objective, value)) {
        return {SolveStatus::numerical_failure, {}, 0.0, tableau.iterations()};
    }
    return {SolveStatus::optimal, std::move(result), value, tableau.iterations()};
}

} // namespace simplex
