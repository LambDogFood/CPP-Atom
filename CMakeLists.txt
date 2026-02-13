cmake_minimum_required(VERSION 3.20)
project(atom)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(main main.cpp)

add_executable(tests tests.cpp)
target_link_libraries(tests PRIVATE pthread)

add_executable(tests_tsan tests.cpp)
target_link_libraries(tests_tsan PRIVATE pthread)
target_compile_options(tests_tsan PRIVATE -fsanitize=thread -g)
target_link_options(tests_tsan PRIVATE -fsanitize=thread)