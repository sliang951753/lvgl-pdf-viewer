# ============================================================================
# FindMuPDF.cmake
# Locates MuPDF headers and static libraries built from third_party/mupdf.
#
# Variables set:
#   MUPDF_INCLUDE_DIRS
#   MUPDF_LIBRARIES
# ============================================================================
set(MUPDF_ROOT "${CMAKE_SOURCE_DIR}/third_party/mupdf" CACHE PATH "MuPDF source root")

find_path(MUPDF_INCLUDE_DIR
    NAMES mupdf/fitz.h
    PATHS "${MUPDF_ROOT}/include"
    NO_DEFAULT_PATH
)

if(WIN32)
    # MSVC build artifacts (built via platform/mupdf/win32/build_mupdf.bat)
    set(_mupdf_lib_dirs
        "${MUPDF_ROOT}/platform/win32/x64/Release"
        "${MUPDF_ROOT}/platform/win32/x64/Debug"
        "${MUPDF_ROOT}/build/release"
    )
    find_library(MUPDF_LIB        NAMES libmupdf  libthirdparty PATHS ${_mupdf_lib_dirs} NO_DEFAULT_PATH)
    find_library(MUPDF_THIRDPARTY NAMES libthirdparty           PATHS ${_mupdf_lib_dirs} NO_DEFAULT_PATH)
    set(MUPDF_LIBRARIES ${MUPDF_LIB} ${MUPDF_THIRDPARTY})
else()
    set(_mupdf_lib_dirs
        "${MUPDF_ROOT}/build/release"
        "${MUPDF_ROOT}/build/debug"
    )
    find_library(MUPDF_LIB        NAMES mupdf        PATHS ${_mupdf_lib_dirs} NO_DEFAULT_PATH)
    find_library(MUPDF_THIRDPARTY NAMES mupdf-third  PATHS ${_mupdf_lib_dirs} NO_DEFAULT_PATH)
    set(MUPDF_LIBRARIES ${MUPDF_LIB} ${MUPDF_THIRDPARTY} m pthread)
endif()

set(MUPDF_INCLUDE_DIRS ${MUPDF_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(MuPDF
    REQUIRED_VARS MUPDF_INCLUDE_DIR MUPDF_LIB
)

if(NOT MUPDF_FOUND)
    message(WARNING
        "\n[FindMuPDF] MuPDF library not found.\n"
        "  Build MuPDF first:\n"
        "    Windows: open third_party/mupdf/platform/win32/mupdf.sln in VS2022, build Release|x64\n"
        "    Linux:   cd third_party/mupdf && make HAVE_X11=no HAVE_GLUT=no -j$(nproc)\n"
        "  Or run: scripts/build_mupdf_linux.sh / scripts/build_mupdf_win32.bat\n"
    )
endif()

mark_as_advanced(MUPDF_INCLUDE_DIR MUPDF_LIB MUPDF_THIRDPARTY)
