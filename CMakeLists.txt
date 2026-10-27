cmake_minimum_required(VERSION 3.24)
project(TitanXFS VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler definitions and flags
if (WIN32)
    add_compile_definitions(NOMINMAX _CRT_SECURE_NO_WARNINGS)
endif()

if (MSVC)
    add_compile_options(/W4 /permissive- /Zc:preprocessor)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-variable -Wno-unused-but-set-variable)
    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        add_compile_options(-Wno-packed-not-aligned)
    endif()
    if (WIN32)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")
    endif()
endif()

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Standalone TitanXFS Core Library
add_library(titanxfs_core INTERFACE)
target_include_directories(titanxfs_core INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Test Suite Executable
add_executable(titanxfs_tests tests/test_xfs.cpp)
target_link_libraries(titanxfs_tests PRIVATE titanxfs_core)

# CLI Formatting Tool
add_executable(titanxfs_format src/tools/titanxfs_format.cpp)
target_link_libraries(titanxfs_format PRIVATE titanxfs_core)

# CLI Inspection & Scrubber Tool
add_executable(titanxfs_check src/tools/titanxfs_check.cpp)
target_link_libraries(titanxfs_check PRIVATE titanxfs_core)

# Background Service Daemon
add_executable(titanxfs_svc src/service/titanxfs_svc.cpp)
target_link_libraries(titanxfs_svc PRIVATE titanxfs_core)
if (WIN32)
    target_link_libraries(titanxfs_svc PRIVATE advapi32)
endif()

if (WIN32)
    # WinFsp Mount Driver Bridge
    add_executable(titanxfs_mount src/mount/winfsp_mount.cpp)
    target_link_libraries(titanxfs_mount PRIVATE titanxfs_core advapi32)

    # Standalone Installer Packager
    add_executable(titanxfs_packager src/tools/titanxfs_packager.cpp)
    target_link_libraries(titanxfs_packager PRIVATE titanxfs_core)

    # Standalone 1-Click GUI Installer
    add_executable(titanxfs_setup WIN32 src/installer/titanxfs_setup.cpp)
    target_link_libraries(titanxfs_setup PRIVATE titanxfs_core user32 gdi32 gdiplus comctl32 dwmapi uxtheme ole32 oleaut32 shell32 advapi32)
    add_dependencies(titanxfs_setup titanxfs_format titanxfs_check titanxfs_mount titanxfs_svc titanxfs_packager)

    set(WINFSP_MSI "${CMAKE_CURRENT_SOURCE_DIR}/redist/winfsp-2.1.25156.msi")
    set(PACKAGER_EXTRA_ARGS "")
    if (EXISTS "${WINFSP_MSI}")
        set(PACKAGER_EXTRA_ARGS "${WINFSP_MSI}")
    endif()

    add_custom_command(
        TARGET titanxfs_setup POST_BUILD
        COMMAND titanxfs_packager $<TARGET_FILE:titanxfs_setup>
                $<TARGET_FILE:titanxfs_format>
                $<TARGET_FILE:titanxfs_check>
                $<TARGET_FILE:titanxfs_mount>
                $<TARGET_FILE:titanxfs_svc>
                ${CMAKE_CURRENT_SOURCE_DIR}/assets/titanxfs_hero.jpg
                ${PACKAGER_EXTRA_ARGS}
        COMMENT "Embedding all binaries, WinFsp MSI, and assets into standalone titanxfs_setup.exe..."
    )
endif()

# ============================================================================
# CPack Packaging Configuration
# ============================================================================
set(CPACK_PACKAGE_NAME "TitanXFS")
set(CPACK_PACKAGE_VENDOR "MicaNT-Kernel")
set(CPACK_PACKAGE_VERSION "1.0.0")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Silicon Graphics XFS File System Storage Engine and WinFsp Mount Bridge for Windows NT")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")

install(TARGETS titanxfs_tests titanxfs_format titanxfs_check titanxfs_svc RUNTIME DESTINATION bin)
if (WIN32)
    install(TARGETS titanxfs_mount titanxfs_setup RUNTIME DESTINATION bin)
    install(DIRECTORY assets/ DESTINATION assets)
    install(DIRECTORY redist/ DESTINATION redist OPTIONAL)
endif()

set(CPACK_GENERATOR "ZIP")
include(CPack)

# CTest registration
enable_testing()
add_test(NAME TitanXfsCoreTests COMMAND titanxfs_tests)
