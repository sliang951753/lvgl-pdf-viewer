# ---------------------------------------------------------------------------
# BuildMuPDF.cmake
#
# Builds MuPDF from third_party/mupdf and exposes the imported target
# MuPDF::mupdf for the main project to link against.
#
# Strategy:
#   * Windows (MSVC):  use MuPDF's own platform/win32/mupdf.sln via
#                      ExternalProject_Add + MSBuild.
#   * Linux / T507:    invoke MuPDF's GNU Makefile with the right CC/AR
#                      and XCFLAGS for static linking.
#
# After this file is included, the following variables are set:
#   MUPDF_INCLUDE_DIR   — path to mupdf/fitz.h  etc.
#   MUPDF_LIBRARIES     — list of .lib/.a files to link
# ---------------------------------------------------------------------------

set(MUPDF_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/mupdf")

if(NOT EXISTS "${MUPDF_SOURCE_DIR}/include/mupdf/fitz.h")
    message(FATAL_ERROR
        "MuPDF submodule not initialised.\n"
        "Run:  git submodule update --init --recursive")
endif()

set(MUPDF_INCLUDE_DIR "${MUPDF_SOURCE_DIR}/include" CACHE PATH "MuPDF include dir")

# ---------------------------------------------------------------------------
# Windows / MSVC
# ---------------------------------------------------------------------------
if(WIN32)
    # MuPDF ships a ready-made VS solution under platform/win32/
    set(MUPDF_VS_SLN "${MUPDF_SOURCE_DIR}/platform/win32/mupdf.sln")

    include(ExternalProject)
    ExternalProject_Add(mupdf_build
        SOURCE_DIR   "${MUPDF_SOURCE_DIR}"
        CONFIGURE_COMMAND ""   # no cmake configure step
        BUILD_COMMAND
            msbuild "${MUPDF_VS_SLN}"
                /t:libmupdf
                /p:Configuration=Release
                /p:Platform=x64
                /m   # parallel build
        INSTALL_COMMAND ""
        BUILD_IN_SOURCE 1
        STAMP_DIR "${CMAKE_BINARY_DIR}/mupdf_stamp"
    )

    # Output library path produced by MuPDF's VS solution
    set(MUPDF_LIB_DIR "${MUPDF_SOURCE_DIR}/platform/win32/Release/x64")

    set(MUPDF_LIBRARIES
        "${MUPDF_LIB_DIR}/libmupdf.lib"
        "${MUPDF_LIB_DIR}/libmupdf-third.lib"
        CACHE STRING "MuPDF libraries" FORCE
    )

# ---------------------------------------------------------------------------
# Linux / T507 (and any non-Windows platform)
# ---------------------------------------------------------------------------
else()
    # Determine make parallelism
    include(ProcessorCount)
    ProcessorCount(NPROC)
    if(NPROC EQUAL 0)
        set(NPROC 4)
    endif()

    # Cross-compiler selection: honour CMake toolchain variables
    if(CMAKE_C_COMPILER)
        set(MUPDF_CC  "CC=${CMAKE_C_COMPILER}")
        set(MUPDF_AR  "AR=${CMAKE_AR}")
    else()
        set(MUPDF_CC  "")
        set(MUPDF_AR  "")
    endif()

    # Build directory (out-of-source not natively supported by MuPDF GNU make,
    # so we build in-source but use a build tag to keep flavours separate)
    set(MUPDF_BUILD_TAG "release-${CMAKE_SYSTEM_PROCESSOR}")

    include(ExternalProject)
    ExternalProject_Add(mupdf_build
        SOURCE_DIR   "${MUPDF_SOURCE_DIR}"
        CONFIGURE_COMMAND ""
        BUILD_COMMAND
            make -C "${MUPDF_SOURCE_DIR}"
                -j${NPROC}
                ${MUPDF_CC}
                ${MUPDF_AR}
                "build=${MUPDF_BUILD_TAG}"
                HAVE_X11=no
                HAVE_GLUT=no
                HAVE_CURL=no
                HAVE_GPERF=no
                libs
        INSTALL_COMMAND ""
        BUILD_IN_SOURCE 1
        STAMP_DIR "${CMAKE_BINARY_DIR}/mupdf_stamp"
    )

    set(MUPDF_LIB_DIR "${MUPDF_SOURCE_DIR}/build/${MUPDF_BUILD_TAG}")

    set(MUPDF_LIBRARIES
        "${MUPDF_LIB_DIR}/libmupdf.a"
        "${MUPDF_LIB_DIR}/libmupdf-third.a"
        CACHE STRING "MuPDF libraries" FORCE
    )
endif()

# ---------------------------------------------------------------------------
# Make main target depend on mupdf_build
# ---------------------------------------------------------------------------
# Caller adds:  add_dependencies(${PROJECT_NAME} mupdf_build)
# We do it via a function so it can be called after add_executable.
function(target_link_mupdf target)
    add_dependencies(${target} mupdf_build)
    target_include_directories(${target} PRIVATE "${MUPDF_INCLUDE_DIR}")
    target_link_libraries(${target} PRIVATE ${MUPDF_LIBRARIES})
endfunction()

message(STATUS "MuPDF source : ${MUPDF_SOURCE_DIR}")
message(STATUS "MuPDF include: ${MUPDF_INCLUDE_DIR}")
message(STATUS "MuPDF libs   : ${MUPDF_LIBRARIES}")
