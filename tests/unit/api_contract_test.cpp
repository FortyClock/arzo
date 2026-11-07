#include "simplex/simplex.hpp"

#include <cassert>

int main() {
    using simplex::SolveStatus;
    assert(simplex::to_string(SolveStatus::optimal) == "optimal");

    const simplex::LinearProgram malformed{};
    const auto result = simplex::solve(malformed);
    assert(result.status == SolveStatus::invalid_input);
    return 0;
}
