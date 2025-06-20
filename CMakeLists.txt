
cmake_minimum_required(VERSION 3.20)
project(AVC VERSION 1.0.0 LANGUAGES C)

# Set C standard
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_EXTENSIONS OFF)

# Compiler-specific optimizations
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    # Release optimizations
    set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG -march=native -flto")
    if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
        set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -fomit-frame-pointer -funroll-loops")
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
find_package(ZLIB REQUIRED)

# Create a library for core functionality
add_library(avc_core STATIC
        # Core modules
        src/core/hash.c
        src/core/index.c
        src/core/objects.c
        src/core/repository.c

        # Utilities
        src/utils/file_utils.c

        # Commands
        src/commands/add.c
        src/commands/commit.c
        src/commands/init.c
        src/commands/log.c
        src/commands/reset.c
        src/commands/rm.c
        src/commands/status.c
)

# Set library properties
target_include_directories(avc_core PUBLIC
        include
        src
        src/core
        src/commands
        src/utils
)

target_link_libraries(avc_core PUBLIC
        OpenSSL::SSL
        OpenSSL::Crypto
        ${ZLIB_LIBRARIES}
)

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
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libssl3, zlib1g")
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
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    message(STATUS "LTO enabled: ${CMAKE_INTERPROCEDURAL_OPTIMIZATION}")
endif()
