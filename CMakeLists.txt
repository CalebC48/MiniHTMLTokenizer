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