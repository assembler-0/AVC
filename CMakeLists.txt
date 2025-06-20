cmake_minimum_required(VERSION 3.30)
project(AVC)

set(CMAKE_C_STANDARD 99)

# Include directories
include_directories(include)

# Source files
file(GLOB_RECURSE SOURCES "src/*.c")

# Create executable
add_executable(avc ${SOURCES})

# Link libraries (you'll need OpenSSL for SHA-1)
find_package(OpenSSL REQUIRED)
target_link_libraries(avc OpenSSL::SSL OpenSSL::Crypto)

# Tests (optional)
enable_testing()
file(GLOB TEST_SOURCES "tests/*.c")
foreach(test_file ${TEST_SOURCES})
    get_filename_component(test_name ${test_file} NAME_WE)
    add_executable(${test_name} ${test_file} ${SOURCES}
            src/commands/shared.c)
    target_link_libraries(${test_name} OpenSSL::SSL OpenSSL::Crypto)
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()