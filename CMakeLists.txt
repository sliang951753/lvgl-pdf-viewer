cmake_minimum_required(VERSION 3.20)
project(lvgl_pdf_viewer
    VERSION 0.1.0
    DESCRIPTION "Cross-platform PDF viewer built on LVGL 9.5 + MuPDF"
    LANGUAGES C CXX
)

# ----------------------------------------------------------------------
# Standards
# ----------------------------------------------------------------------
set(CMAKE_C_STANDARD   11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ----------------------------------------------------------------------
# Platform detection
# ----------------------------------------------------------------------
if(WIN32)
    set(PLATFORM_WIN32 TRUE)
    set(PLATFORM_NAME  "windows")
    message(STATUS "Platform: Windows (Win32 API driver + MSVC)")
else()
    set(PLATFORM_LINUX TRUE)
    set(PLATFORM_NAME  "linux")
    message(STATUS "Platform: Linux (SDL2 driver)")
endif()

# ----------------------------------------------------------------------
# LVGL  (git submodule at third_party/lvgl)
# ----------------------------------------------------------------------
set(LVGL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/lvgl")

if(NOT EXISTS "${LVGL_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "LVGL submodule not initialised.\n"
        "Run:  git submodule update --init --recursive")
endif()

# Point LVGL at our lv_conf.h  (LVGL 9.x uses LV_BUILD_CONF_PATH)
set(LV_BUILD_CONF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/src/lv_conf.h"
    CACHE PATH "Path to lv_conf.h" FORCE)

add_subdirectory("${LVGL_DIR}" lvgl EXCLUDE_FROM_ALL)

# ----------------------------------------------------------------------
# MuPDF  (built via external cmake helper)
# ----------------------------------------------------------------------
include(cmake/BuildMuPDF.cmake)

# ----------------------------------------------------------------------
# Source files
# ----------------------------------------------------------------------
set(SOURCES
    src/main.c
    src/pdf/pdf_view.c
    src/ui/ui_main.c
)

if(PLATFORM_WIN32)
    list(APPEND SOURCES src/platform/platform_win32.c)
else()
    list(APPEND SOURCES src/platform/platform_sdl2.c)
endif()

# ----------------------------------------------------------------------
# Executable
# ----------------------------------------------------------------------
if(WIN32)
    # WIN32 entry point = WinMain (no console window in release)
    add_executable(${PROJECT_NAME} WIN32 ${SOURCES})
else()
    add_executable(${PROJECT_NAME} ${SOURCES})
endif()

# ----------------------------------------------------------------------
# Include dirs
# ----------------------------------------------------------------------
target_include_directories(${PROJECT_NAME} PRIVATE
    src
    src/platform
    src/pdf
    src/ui
    "${MUPDF_INCLUDE_DIR}"
)

# ----------------------------------------------------------------------
# Platform-specific setup
# ----------------------------------------------------------------------
if(PLATFORM_WIN32)
    # MSVC: warning level + UTF-8 source encoding
    target_compile_options(${PROJECT_NAME} PRIVATE
        /W3
        /utf-8
        /wd4996   # suppress 'deprecated' warnings from C runtime
    )

    # Link LVGL Win32 driver (included in LVGL 9 source tree)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        _WIN32_WINNT=0x0601  # Windows 7+
        UNICODE
        _UNICODE
    )

    target_link_libraries(${PROJECT_NAME} PRIVATE
        lvgl
        ${MUPDF_LIBRARIES}
        user32 gdi32 shell32 imm32 ole32 oleaut32
    )

else()
    # Linux / T507
    find_package(SDL2 REQUIRED)

    target_compile_options(${PROJECT_NAME} PRIVATE
        -Wall -Wextra
        -Wno-unused-parameter
    )

    target_compile_definitions(${PROJECT_NAME} PRIVATE
        LV_USE_SDL=1
    )

    target_include_directories(${PROJECT_NAME} PRIVATE
        ${SDL2_INCLUDE_DIRS}
    )

    target_link_libraries(${PROJECT_NAME} PRIVATE
        lvgl
        ${MUPDF_LIBRARIES}
        ${SDL2_LIBRARIES}
        m pthread log
    )
endif()

# ----------------------------------------------------------------------
# Install rules (for T507 deployment)
# ----------------------------------------------------------------------
if(PLATFORM_LINUX)
    include(GNUInstallDirs)
    install(TARGETS ${PROJECT_NAME}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
    install(DIRECTORY assets/
        DESTINATION ${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}
    )
endif()

# ----------------------------------------------------------------------
# Test executables (Linux host only)
# ----------------------------------------------------------------------
if(PLATFORM_LINUX)
    add_executable(test_search_cli
        tests/test_search_cli.c
        src/pdf/pdf_view.c
    )
    target_include_directories(test_search_cli PRIVATE
        src
        src/pdf
        "${MUPDF_INCLUDE_DIR}"
    )
    target_link_libraries(test_search_cli PRIVATE
        lvgl
        ${MUPDF_LIBRARIES}
        m pthread log
    )

    add_executable(test_pageapi_cli
        tests/test_pageapi_cli.c
        src/pdf/pdf_view.c
    )
    target_include_directories(test_pageapi_cli PRIVATE
        src
        src/pdf
        "${MUPDF_INCLUDE_DIR}"
    )
    target_link_libraries(test_pageapi_cli PRIVATE
        lvgl
        ${MUPDF_LIBRARIES}
        m pthread log
    )
endif()

# ----------------------------------------------------------------------
# Summary
# ----------------------------------------------------------------------
message(STATUS "")
message(STATUS "=== ${PROJECT_NAME} v${PROJECT_VERSION} ===")
message(STATUS "  Platform   : ${PLATFORM_NAME}")
message(STATUS "  Build type : ${CMAKE_BUILD_TYPE}")
message(STATUS "  MuPDF inc  : ${MUPDF_INCLUDE_DIR}")
message(STATUS "")
