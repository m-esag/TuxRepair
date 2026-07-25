# TuxRepair test suite.
#
# Each test file is its own executable: isolates failures, runs under its own
# ASan/UBSan instance, and keeps the per-test binary small. All link against
# tuxrepair-core (never the UI) so tests stay headless and fast.

set(TUXREPAIR_TESTS
    test_db_basic
    test_accounting
)

foreach(test_name IN LISTS TUXREPAIR_TESTS)
    add_executable(${test_name} ${test_name}.cpp)
    target_link_libraries(${test_name}
        PRIVATE
            tuxrepair-core
            TuxRepair::Warnings
    )
    target_compile_features(${test_name} PRIVATE cxx_std_23)

    # Run the test from the build dir so any DB-using test that defaults to a
    # file path stays inside the build tree. Our tests use :memory: exclusively,
    # but the CWD pin is cheap insurance.
    add_test(NAME ${test_name} COMMAND ${test_name})
    set_tests_properties(${test_name} PROPERTIES
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
endforeach()
