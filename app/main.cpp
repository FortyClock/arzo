#include "simplex/simplex.hpp"

#include <charconv>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

class InputError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

void print_usage(std::ostream& out) {
    out << "Usage: simplex_cli [--tolerance VALUE] [--max-iterations N] [FILE|-]\n"
           "Read an LP1 problem from FILE or standard input.\n";
}

std::string read_token(std::istream& input, std::string_view field) {
    std::string token;
    if (!(input >> token)) {
        throw InputError("missing or unreadable " + std::string(field));
    }
    return token;
}

template <class Number>
Number parse_number(std::string_view token, std::string_view field) {
    if (token.starts_with('+')) {
        token.remove_prefix(1);
        if (token.starts_with('-')) {
            throw InputError("invalid " + std::string(field));
        }
    }
    Number value{};
    if (token.empty()) {
        throw InputError("invalid " + std::string(field));
    }
    const auto end = token.data() + token.size();
    const auto result = std::from_chars(token.data(), end, value);
    if (result.ec != std::errc{} || result.ptr != end || !std::isfinite(value)) {
        throw InputError("invalid " + std::string(field));
    }
    return value;
}

simplex::LinearProgram read_program(std::istream& input) {
    std::string magic = read_token(input, "format marker");
    if (magic.starts_with("\xef\xbb\xbf")) {
        magic.erase(0, 3);
    }
    if (magic != "LP1") {
        throw InputError("expected LP1 format marker");
    }
    simplex::LinearProgram program;
    const auto direction = read_token(input, "optimization direction");
    if (direction == "min") {
        program.direction = simplex::OptimizationDirection::minimize;
    } else if (direction != "max") {
        throw InputError("optimization direction must be max or min");
    }
    const auto rows = parse_number<std::size_t>(read_token(input, "row count"), "row count");
    const auto columns = parse_number<std::size_t>(read_token(input, "variable count"),
                                                   "variable count");
    if (columns == 0) {
        throw InputError("at least one variable is required");
    }
    for (std::size_t column = 0; column < columns; ++column) {
        program.objective.push_back(
            parse_number<double>(read_token(input, "objective coefficient"),
                                 "objective coefficient"));
    }
    for (std::size_t row = 0; row < rows; ++row) {
        auto& coefficients = program.constraints.emplace_back();
        for (std::size_t column = 0; column < columns; ++column) {
            coefficients.push_back(
                parse_number<double>(read_token(input, "constraint coefficient"),
                                     "constraint coefficient"));
        }
        program.bounds.push_back(
            parse_number<double>(read_token(input, "right-hand side"), "right-hand side"));
    }
    if (std::string extra; input >> extra) {
        throw InputError("unexpected data after the last constraint");
    }
    if (input.bad() || !input.eof()) {
        throw InputError("unable to read the complete problem");
    }
    return program;
}

void print_result(const simplex::SolveResult& result) {
    std::cout << std::setprecision(std::numeric_limits<double>::max_digits10)
              << "{\"status\":\"" << simplex::to_string(result.status)
              << "\",\"iterations\":" << result.iterations << ",\"objective\":";
    if (result.status == simplex::SolveStatus::optimal) {
        std::cout << result.objective_value << ",\"variables\":[";
        for (std::size_t index = 0; index < result.variables.size(); ++index) {
            if (index != 0) {
                std::cout << ',';
            }
            std::cout << result.variables[index];
        }
    } else {
        std::cout << "null,\"variables\":[";
    }
    std::cout << "]}\n";
}

int run(int argc, char* argv[]) {
    if (argc == 2 && std::string_view(argv[1]) == "--help") {
        print_usage(std::cout);
        return 0;
    }
    simplex::SolveOptions options;
    std::string_view filename;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument = argv[index];
        if (argument == "--tolerance" || argument == "--max-iterations") {
            if (++index == argc) {
                throw InputError("missing value for " + std::string(argument));
            }
            if (argument == "--tolerance") {
                options.tolerance = parse_number<double>(argv[index], "tolerance");
                if (options.tolerance <= 0.0) {
                    throw InputError("tolerance must be positive");
                }
            } else {
                options.max_iterations = parse_number<std::size_t>(argv[index], "pivot limit");
            }
        } else if ((argument == "-" || !argument.starts_with('-')) && filename.empty()) {
            filename = argument;
        } else {
            throw InputError("unexpected argument: " + std::string(argument));
        }
    }

    std::ifstream file;
    if (!filename.empty() && filename != "-") {
        file.open(std::string(filename));
        if (!file) {
            throw InputError("unable to open " + std::string(filename));
        }
    }
    auto& input = file.is_open() ? static_cast<std::istream&>(file) : std::cin;
    const auto result = simplex::solve(read_program(input), options);
    print_result(result);
    return result.status == simplex::SolveStatus::invalid_input ? 2 : 0;
}

} // namespace

int main(int argc, char* argv[]) {
    try {
        return run(argc, argv);
    } catch (const InputError& error) {
        std::cerr << "simplex_cli: " << error.what() << '\n';
        print_result({});
        return 2;
    } catch (const std::exception& error) {
        std::cerr << "simplex_cli: " << error.what() << '\n';
        return 1;
    }
}
