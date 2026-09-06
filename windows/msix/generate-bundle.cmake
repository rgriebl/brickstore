# Copyright (C) 2004-2026 Robert Griebl
# SPDX-License-Identifier: GPL-3.0-only

# Bundles the per-architecture packages into one .msixbundle. That is what the
# Store submission takes; it hands every machine the matching architecture.
#
#   cmake "-DPACKAGES=<x64.msix>;<arm64.msix>" -DOUT_FILE=<BrickStore.msixbundle>
#         [-DBUNDLE_VERSION=2026.8.1.0]
#         -P windows/msix/generate-bundle.cmake
#
# makeappx can also bundle a whole directory, but then everything in it has to be
# a package. A mapping file names the inputs instead, so that the staging
# directories next to them do not get in the way.

cmake_minimum_required(VERSION 3.19.0)

foreach (var IN ITEMS PACKAGES OUT_FILE)
    if (NOT ${var})
        message(FATAL_ERROR "${var} is required")
    endif()
endforeach()

find_program(MAKEAPPX_EXE makeappx REQUIRED)

set(mapping "[Files]\n")
foreach (package IN LISTS PACKAGES)
    if (NOT EXISTS "${package}")
        message(FATAL_ERROR "${package} does not exist")
    endif()
    get_filename_component(name "${package}" NAME)
    file(TO_NATIVE_PATH "${package}" native)
    string(APPEND mapping "\"${native}\" \"${name}\"\n")
endforeach()

set(MAPPING_FILE "${OUT_FILE}.map")
file(WRITE "${MAPPING_FILE}" "${mapping}")

# without /bv makeappx derives the bundle version from the current time, which
# works but is not the version anything else knows the release by
set(version_args "")
if (BUNDLE_VERSION)
    set(version_args /bv "${BUNDLE_VERSION}")
endif()

message(STATUS "Bundling ${OUT_FILE}")

execute_process(
    COMMAND "${MAKEAPPX_EXE}" bundle /f "${MAPPING_FILE}" /p "${OUT_FILE}" ${version_args} /o
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE output
)
file(REMOVE "${MAPPING_FILE}")
if (NOT result EQUAL 0)
    message(FATAL_ERROR "makeappx bundle failed:\n${output}")
endif()
