cmake_minimum_required(VERSION 3.20)
project(AVC VERSION 0.1.1 LANGUAGES C)

# Set C standard
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_EXTENSIONS OFF)

# Cross-platform build option
option(AVC_PORTABLE_BUILD "Build without native CPU optimizations for better portability" OFF)

# Compiler-specific optimizations
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    if(AVC_PORTABLE_BUILD)
        # Portable release build
        set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG")
    else()
        # Native optimized release build
        set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG -march=native -flto")
        if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
            set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -fomit-frame-pointer -funroll-loops")
        endif()
    endif()
elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
    # Debug flags
    set(CMAKE_C_FLAGS_DEBUG "-O0 -g3 -Wall -Wextra -Wpedantic -Wformat=2 -Wno-unused-parameter")
    if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
        set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -fsanitize=address -fsanitize=undefined")
        set(CMAKE_EXE_LINKER_FLAGS_DEBUG "-fsanitize=address -fsanitize=undefined")
    endif()
endif()

# Set default build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Find required packages
find_package(OpenSSL REQUIRED)
find_package(OpenMP)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBDEFLATE REQUIRED libdeflate)

# BLAKE3 source files
set(BLAKE3_SOURCES
    src/blake3/blake3.c
    src/blake3/blake3_dispatch.c
    src/blake3/blake3_portable.c
)

# Add SIMD sources only for non-portable builds
if(NOT AVC_PORTABLE_BUILD)
    list(APPEND BLAKE3_SOURCES
        src/blake3/blake3_sse2.c
        src/blake3/blake3_sse41.c
        src/blake3/blake3_avx2.c
    )
endif()

add_library(blake3 STATIC ${BLAKE3_SOURCES})
target_include_directories(blake3 PUBLIC src/blake3)
# Create a library for core functionality
add_library(avc_core STATIC
        # Core modules
        src/core/hash.c
        src/core/index.c
        src/core/fast_index.c
        src/core/memory_pool.c
        src/core/objects.c
        src/core/repository.c

        # Utilities
        src/utils/file_utils.c
        src/utils/arg_parser.c
        src/utils/tui.c

        # Commands
        src/commands/add.c
        src/commands/commit.c
        src/commands/init.c
        src/commands/log.c
        src/commands/reset.c
        src/commands/rm.c
        src/commands/status.c
        src/commands/version.c
        src/commands/clean.c
        agcl/agcl.c
        agcl/fast_agcl.c
)

# Set library properties
target_include_directories(avc_core PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/api
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core
        ${CMAKE_CURRENT_SOURCE_DIR}/src/commands
        ${CMAKE_CURRENT_SOURCE_DIR}/src/utils
        ${CMAKE_CURRENT_SOURCE_DIR}/agcl
        ${OPENSSL_INCLUDE_DIR}
        ${LIBDEFLATE_INCLUDE_DIRS}
)

target_link_libraries(avc_core PUBLIC
        ${LIBDEFLATE_LIBRARIES}
        OpenMP::OpenMP_C
        blake3
        OpenSSL::SSL
        OpenSSL::Crypto
        z
        m
)
# BLAKE3 SIMD configuration
if(AVC_PORTABLE_BUILD)
    target_compile_definitions(blake3 PUBLIC 
        BLAKE3_NO_SSE2=1
        BLAKE3_NO_SSE41=1 
        BLAKE3_NO_AVX2=1
        BLAKE3_NO_AVX512=1
        BLAKE3_USE_PORTABLE_IMPL=1
    )
else()
    target_compile_definitions(blake3 PUBLIC BLAKE3_NO_AVX512=1)
endif()
target_compile_definitions(avc_core PRIVATE
        $<$<CONFIG:Debug>:AVC_DEBUG>
        $<$<CONFIG:Release>:AVC_RELEASE>
)

# Create main executable
add_executable(avc src/main.c)
target_link_libraries(avc PRIVATE avc_core)

# Compiler warnings for the executable
target_compile_options(avc PRIVATE
        $<$<C_COMPILER_ID:GNU>:-Wall -Wextra -Wpedantic>
        $<$<C_COMPILER_ID:Clang>:-Wall -Wextra -Wpedantic>
)

# Link Time Optimization for Release builds
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set_target_properties(avc avc_core PROPERTIES
            INTERPROCEDURAL_OPTIMIZATION TRUE
    )
endif()

# Strip symbols in Release builds
if(CMAKE_BUILD_TYPE STREQUAL "Release" AND NOT WIN32)
    add_custom_command(TARGET avc POST_BUILD
            COMMAND ${CMAKE_STRIP} $<TARGET_FILE:avc>
            COMMENT "Stripping symbols from release binary"
    )
endif()

# Installation
include(GNUInstallDirs)
install(TARGETS avc
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        COMPONENT Runtime
)

# Install man page if it exists
if(EXISTS "${CMAKE_SOURCE_DIR}/docs/avc.1")
    install(FILES docs/avc.1
            DESTINATION ${CMAKE_INSTALL_MANDIR}/man1
            COMPONENT Documentation
    )
endif()

# CPack configuration for packaging
set(CPACK_PACKAGE_NAME "avc")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Archive Version Control - A Git-like VCS")
set(CPACK_PACKAGE_CONTACT "your.email@example.com")
set(CPACK_GENERATOR "TGZ;DEB")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libssl3, libdeflate0")
include(CPack)

# Testing configuration
if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR BUILD_TESTING)
    enable_testing()

    # Find test files
    file(GLOB TEST_SOURCES "test/*.c")

    if(TEST_SOURCES)
        foreach(test_file ${TEST_SOURCES})
            get_filename_component(test_name ${test_file} NAME_WE)
            add_executable(${test_name} ${test_file})
            target_link_libraries(${test_name} PRIVATE avc_core)
            add_test(NAME ${test_name} COMMAND ${test_name})

            # Set test properties
            set_tests_properties(${test_name} PROPERTIES
                    TIMEOUT 30
                    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            )
        endforeach()
    endif()
endif()

# Print build configuration
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C Compiler: ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}")
message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "Portable build: ${AVC_PORTABLE_BUILD}")
if(CMAKE_BUILD_TYPE STREQUAL "Release" AND NOT AVC_PORTABLE_BUILD)
    message(STATUS "LTO enabled: ${CMAKE_INTERPROCEDURAL_OPTIMIZATION}")
endif()
