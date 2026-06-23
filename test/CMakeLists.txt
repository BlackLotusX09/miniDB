add_executables(run_tests test_main.cpp ../src/)

target_include_directories(run_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include)

add_test(NAME CoreLibraryTests COMMAND run_tests)
