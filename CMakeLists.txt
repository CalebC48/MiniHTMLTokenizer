cmake_minimum_required(VERSION 3.16)

project(mini_html_tokenizer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(mini_html_tokenizer
    src/main.cpp
    src/output.cpp
    src/tokenizer.cpp
)

target_include_directories(mini_html_tokenizer PRIVATE include)

enable_testing()

add_executable(tokenizer_tests
    tests/tokenizer_tests.cpp
    src/tokenizer.cpp
)
target_include_directories(tokenizer_tests PRIVATE include src)

add_test(NAME tokenizer_unit_tests COMMAND tokenizer_tests)

set(FIXTURE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")

add_test(
    NAME cli_valid_human
    COMMAND ${CMAKE_COMMAND}
        -DTEST_EXE=$<TARGET_FILE:mini_html_tokenizer>
        -DEXPECT_EXIT=0
        -DARG1=${FIXTURE_DIR}/valid_basic.html
        -DEXPECT_STDOUT_SUBSTR=StartTag
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_check.cmake
)

add_test(
    NAME cli_valid_json
    COMMAND ${CMAKE_COMMAND}
        -DTEST_EXE=$<TARGET_FILE:mini_html_tokenizer>
        -DEXPECT_EXIT=0
        -DARG1=${FIXTURE_DIR}/valid_basic.html
        -DARG2=--json
        -DEXPECT_STDOUT_SUBSTR=\"type\":\"Doctype\"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_check.cmake
)

add_test(
    NAME cli_errors_flag_outputs_stderr
    COMMAND ${CMAKE_COMMAND}
        -DTEST_EXE=$<TARGET_FILE:mini_html_tokenizer>
        -DEXPECT_EXIT=3
        -DARG1=${FIXTURE_DIR}/fatal_unterminated_tag.html
        -DARG2=--errors
        -DEXPECT_STDERR_SUBSTR=Unterminated tag
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_check.cmake
)

add_test(
    NAME cli_empty_file_success
    COMMAND ${CMAKE_COMMAND}
        -DTEST_EXE=$<TARGET_FILE:mini_html_tokenizer>
        -DEXPECT_EXIT=0
        -DARG1=${FIXTURE_DIR}/empty.html
        -DEXPECT_STDOUT_SUBSTR=EOFToken
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_check.cmake
)

add_test(
    NAME cli_non_html_rejected
    COMMAND ${CMAKE_COMMAND}
        -DTEST_EXE=$<TARGET_FILE:mini_html_tokenizer>
        -DEXPECT_EXIT=2
        -DARG1=${FIXTURE_DIR}/not_html.txt
        -DEXPECT_STDERR_SUBSTR=non-HTML
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_check.cmake
)

add_test(
    NAME cli_missing_file
    COMMAND ${CMAKE_COMMAND}
        -DTEST_EXE=$<TARGET_FILE:mini_html_tokenizer>
        -DEXPECT_EXIT=2
        -DARG1=${FIXTURE_DIR}/missing.html
        -DEXPECT_STDERR_SUBSTR=cannot
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_check.cmake
)

add_test(
    NAME cli_cannot_read_case
    COMMAND ${CMAKE_COMMAND}
        -DTEST_EXE=$<TARGET_FILE:mini_html_tokenizer>
        -DCASE_NAME=cannot_read
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_generated_cli_case.cmake
)

add_test(
    NAME cli_size_limit_case
    COMMAND ${CMAKE_COMMAND}
        -DTEST_EXE=$<TARGET_FILE:mini_html_tokenizer>
        -DCASE_NAME=size_limit
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_generated_cli_case.cmake
)