add_executable(simplex_unit_tests unit/api_contract_test.cpp)
target_link_libraries(simplex_unit_tests PRIVATE simplex::core)
add_test(NAME unit.api_contract COMMAND simplex_unit_tests)

add_test(NAME integration.cli_starts COMMAND simplex_cli)
set_tests_properties(integration.cli_starts PROPERTIES
    PASS_REGULAR_EXPRESSION "infrastructure is ready"
)
