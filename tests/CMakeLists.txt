add_executable(simplex_unit_tests unit/api_contract_test.cpp)
target_link_libraries(simplex_unit_tests PRIVATE simplex::core)
add_test(NAME unit.api_contract COMMAND simplex_unit_tests)

foreach(scenario IN ITEMS file stdin invalid bad_option limit)
    add_test(NAME integration.cli_${scenario}
        COMMAND ${CMAKE_COMMAND}
            "-DCLI=$<TARGET_FILE:simplex_cli>"
            "-DCASES=${PROJECT_SOURCE_DIR}/experiments/data/input"
            "-DSCENARIO=${scenario}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/integration/cli_test.cmake")
endforeach()
