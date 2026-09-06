# Copyright (C) 2004-2026 Robert Griebl
# SPDX-License-Identifier: GPL-3.0-only

# Stages one architecture's MSIX package layout: a deployed tree, the generated
# assets, the manifest, and the resource index that ties the qualified asset
# names to the unqualified ones the manifest refers to.
#
#   cmake -DPAYLOAD_DIR=<deployed tree> -DASSETS_DIR=<generated assets>
#         -DSOURCE_DIR=<srcdir> -DOUT_DIR=<staging> -DMSIX_ARCH=x64
#         -DMSIX_VERSION=2026.8.1.0 [-DMAKEAPPX=ON]
#         -P windows/msix/generate-package.cmake
#
# Without MAKEAPPX it stops after the layout, which is what
#     Add-AppxPackage -Register <staging>\AppxManifest.xml
# wants: that deploys the staged directory as-is, no signing and no Store
# involved, and is the fastest way to see the icons and the .bsx association.
# With it, the package lands next to the staging directory as
# BrickStore-<version>-<arch>.msix, ready for generate-bundle.cmake.

cmake_minimum_required(VERSION 3.19.0)

foreach (var IN ITEMS PAYLOAD_DIR ASSETS_DIR SOURCE_DIR OUT_DIR MSIX_ARCH MSIX_VERSION)
    if (NOT ${var})
        message(FATAL_ERROR "${var} is required")
    endif()
endforeach()

# makepri and makeappx live in the Windows SDK and are not on PATH unless this
# runs from a Developer Command Prompt
find_program(MAKEPRI makepri REQUIRED)
find_program(MAKEAPPX_EXE makeappx REQUIRED)

file(MAKE_DIRECTORY "${OUT_DIR}")

message(STATUS "Staging the ${MSIX_ARCH} package in ${OUT_DIR}")

# vc_redist.*.exe: a packaged app cannot run an installer; the manifest declares
# the MSVC runtime as a framework package dependency instead. *.pdb: debug info,
# which the installers do not ship either.
file(COPY "${PAYLOAD_DIR}/" DESTINATION "${OUT_DIR}"
    PATTERN "vc_redist.*.exe" EXCLUDE
    PATTERN "*.pdb" EXCLUDE
)
file(COPY "${ASSETS_DIR}/" DESTINATION "${OUT_DIR}/Assets")

set(DESCRIPTION "BrickStore - an offline BrickLink inventory management tool")

# every language the app ships translations for
set(MSIX_RESOURCES "")
foreach (lang IN ITEMS en de fr es sv)
    string(APPEND MSIX_RESOURCES "    <Resource Language=\"${lang}\" />\n")
endforeach()
string(REGEX REPLACE "\n$" "" MSIX_RESOURCES "${MSIX_RESOURCES}")

configure_file("${SOURCE_DIR}/windows/msix/AppxManifest.xml.in"
               "${OUT_DIR}/AppxManifest.xml" @ONLY)

# The assets are named Square44x44Logo.scale-200.png and the manifest asks for
# Square44x44Logo.png - the resource index is what maps one to the other. Without
# it the shell falls back to the unqualified file at every size.
#
# makepri indexes everything below its project root and names the resources
# relative to it. Indexing the layout itself pulls in every Qt DLL and QML file
# (1500+ resources), and narrowing the index to Assets\ via startIndexAt drops
# the Assets\ prefix from the names the manifest refers to. So it gets a project
# directory of its own that holds nothing but Assets\, at the same relative
# position it has in the package.
set(PRI_PROJECT "${OUT_DIR}-pri")
file(REMOVE_RECURSE "${PRI_PROJECT}")
file(COPY "${ASSETS_DIR}/" DESTINATION "${PRI_PROJECT}/Assets")

message(STATUS "Indexing the resources")

# /mn: the resource map has to carry the package's identity name, or the shell
# will not find it. makepri takes the name from a manifest at the project root,
# and there is none there - without /mn it would silently call the map
# "Application".
execute_process(
    COMMAND "${MAKEPRI}" new /pr "${PRI_PROJECT}"
                             /cf "${SOURCE_DIR}/windows/msix/priconfig.xml"
                             /mn "${OUT_DIR}/AppxManifest.xml"
                             /of "${OUT_DIR}/resources.pri" /o
    RESULT_VARIABLE result
)
if (NOT result EQUAL 0)
    message(FATAL_ERROR "makepri new failed")
endif()

# A broken index does not fail: the shell quietly uses the unqualified file at
# every size. So check that the four names the manifest refers to are in there,
# under the identity name the shell looks them up by.
file(READ "${OUT_DIR}/AppxManifest.xml" manifest)
if (NOT manifest MATCHES "<Identity[ \t\r\n]+Name=\"([^\"]+)\"")
    message(FATAL_ERROR "Cannot find the Identity Name in the manifest")
endif()
set(IDENTITY_NAME "${CMAKE_MATCH_1}")

set(PRI_DUMP "${PRI_PROJECT}/dump.xml")
execute_process(
    COMMAND "${MAKEPRI}" dump /if "${OUT_DIR}/resources.pri" /of "${PRI_DUMP}" /o
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE output
)
if (NOT result EQUAL 0)
    message(FATAL_ERROR "makepri dump failed:\n${output}")
endif()
file(READ "${PRI_DUMP}" pri_dump)
foreach (name IN ITEMS Square44x44Logo Square150x150Logo StoreLogo BsxDocumentLogo)
    if (NOT pri_dump MATCHES "ms-resource://${IDENTITY_NAME}/Files/Assets/${name}\\.png")
        message(FATAL_ERROR "resources.pri does not index Assets\\${name}.png under "
                            "${IDENTITY_NAME} - see ${PRI_DUMP}")
    endif()
endforeach()

file(REMOVE_RECURSE "${PRI_PROJECT}")

if (NOT MAKEAPPX)
    message(STATUS "Layout ready - Add-AppxPackage -Register ${OUT_DIR}\\AppxManifest.xml")
    return()
endif()

set(PACKAGE "${OUT_DIR}/../BrickStore-${MSIX_VERSION}-${MSIX_ARCH}.msix")

message(STATUS "Packing ${PACKAGE}")

# makeappx narrates every file it adds, so its output is only shown on failure
execute_process(
    COMMAND "${MAKEAPPX_EXE}" pack /d "${OUT_DIR}" /p "${PACKAGE}" /o
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE output
)
if (NOT result EQUAL 0)
    string(REGEX REPLACE "Processing \"[^\n]*\n" "" output "${output}")
    message(FATAL_ERROR "makeappx pack failed:\n${output}")
endif()
