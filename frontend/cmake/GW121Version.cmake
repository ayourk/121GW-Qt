# SPDX-License-Identifier: MIT
# cmake/GW121Version.cmake: every spelling of 121gw-qt's version.
#
# The version is the VERSION of project() in CMakeLists.txt. A nightly build adds a stamp, the UTC
# date it was built:
#
#     GW121_NIGHTLY = YYYY.MM.DD, or YYYY.MM.DD.N for build N of that date (N from 1)
#
# given as -DGW121_NIGHTLY=... or in the environment, the way CI passes it so that every packaging
# tool's own CMake run sees it. Empty or unset means a release build.
#
#   key      release   nightly 2026.11.03.1         used by
#   base     0.1.0     0.1.0                        macOS short version
#   nightly  (empty)   2026.11.03.1                 Nightly or not
#   full     0.1.0     0.1.0.2026.11.03.1           --version, deb, rpm, Arch
#   display  0.1.0     Nightly 2026.11.03.1 (0.1.0) window title, release titles
#   file     0.1.0     nightly-2026.11.03.1         file names: 121gw-qt-<file>-<arch>...
#   bundle   0.1.0     2026.11.03                   macOS CFBundleVersion
#   tag      v0.1.0    znightly-2026.11.03.1        git tag
#
# Nightly versions sort above their base in dpkg, rpm and pacman, so a base that has had nightlies
# is never released as is: the release after nightlies of 0.1.0 is the next version.
#
# Two ways in:
#   include(cmake/GW121Version.cmake) after project() sets GW121_VERSION_<KEY> (upper case).
#   cmake [-DFIELD=<key>] [-DOUT=<file>] -P cmake/GW121Version.cmake reads CMakeLists.txt beside
#   this directory: FIELD prints that one value; otherwise every key=value line is printed, or
#   appended to OUT ($GITHUB_OUTPUT in a workflow).

set(_GW121_VERSION_KEYS base nightly full display file bundle tag)

function(gw121_version_compute base stamp)
    if(NOT base MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
        message(FATAL_ERROR "121gw-qt version '${base}' is not MAJOR.MINOR.PATCH")
    endif()
    if(stamp STREQUAL "")
        set(nightly "")
        set(full    "${base}")
        set(display "${base}")
        set(file    "${base}")
        set(bundle  "${base}")
        set(tag     "v${base}")
    else()
        if(NOT stamp MATCHES "^([0-9][0-9][0-9][0-9])\\.([0-9][0-9])\\.([0-9][0-9])(\\.([1-9][0-9]*))?$")
            message(FATAL_ERROR "GW121_NIGHTLY '${stamp}' is not YYYY.MM.DD or YYYY.MM.DD.N "
                                "(two-digit month and day, N from 1)")
        endif()
        set(year "${CMAKE_MATCH_1}")
        set(month "${CMAKE_MATCH_2}")
        set(day "${CMAKE_MATCH_3}")
        math(EXPR m "${month} + 0")
        math(EXPR d "${day} + 0")
        if(m LESS 1 OR m GREATER 12 OR d LESS 1 OR d GREATER 31)
            message(FATAL_ERROR "GW121_NIGHTLY '${stamp}' is not a date")
        endif()
        set(nightly "${stamp}")
        set(full    "${base}.${stamp}")
        set(display "Nightly ${stamp} (${base})")
        set(file    "nightly-${stamp}")
        set(bundle  "${year}.${month}.${day}")
        set(tag     "znightly-${stamp}")
    endif()
    foreach(key IN LISTS _GW121_VERSION_KEYS)
        string(TOUPPER "${key}" upper)
        set(GW121_VERSION_${upper} "${${key}}" PARENT_SCOPE)
    endforeach()
endfunction()

if(CMAKE_SCRIPT_MODE_FILE STREQUAL CMAKE_CURRENT_LIST_FILE)
    # A -P run starts with no policies set, so IN_LIST would not parse without this.
    cmake_minimum_required(VERSION 3.22)
    get_filename_component(_root "${CMAKE_CURRENT_LIST_DIR}" DIRECTORY)
    file(STRINGS "${_root}/CMakeLists.txt" _line REGEX "^project\\(121gw-qt VERSION [0-9]+\\.[0-9]+\\.[0-9]+" LIMIT_COUNT 1)
    if(NOT _line MATCHES "VERSION ([0-9]+\\.[0-9]+\\.[0-9]+)")
        message(FATAL_ERROR "no 'project(121gw-qt VERSION x.y.z' line in ${_root}/CMakeLists.txt")
    endif()
    set(_base "${CMAKE_MATCH_1}")
    if(NOT DEFINED GW121_NIGHTLY)
        set(GW121_NIGHTLY "$ENV{GW121_NIGHTLY}")
    endif()
    gw121_version_compute("${_base}" "${GW121_NIGHTLY}")
    if(DEFINED FIELD)
        if(NOT FIELD IN_LIST _GW121_VERSION_KEYS)
            message(FATAL_ERROR "FIELD '${FIELD}' is not one of: ${_GW121_VERSION_KEYS}")
        endif()
        string(TOUPPER "${FIELD}" _upper)
        execute_process(COMMAND "${CMAKE_COMMAND}" -E echo "${GW121_VERSION_${_upper}}")
    else()
        foreach(_key IN LISTS _GW121_VERSION_KEYS)
            string(TOUPPER "${_key}" _upper)
            if(DEFINED OUT)
                file(APPEND "${OUT}" "${_key}=${GW121_VERSION_${_upper}}\n")
            else()
                execute_process(COMMAND "${CMAKE_COMMAND}" -E echo "${_key}=${GW121_VERSION_${_upper}}")
            endif()
        endforeach()
    endif()
else()
    # -DGW121_NIGHTLY wins, then the environment. Not cached, so a later configure in the same build
    # tree follows the environment it is given rather than a remembered stamp.
    if(NOT DEFINED GW121_NIGHTLY)
        set(GW121_NIGHTLY "$ENV{GW121_NIGHTLY}")
    endif()
    gw121_version_compute("${PROJECT_VERSION}" "${GW121_NIGHTLY}")
endif()
