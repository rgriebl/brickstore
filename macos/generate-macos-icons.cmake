# Copyright (C) 2004-2026 Robert Griebl
# SPDX-License-Identifier: GPL-3.0-only

# Generates the macOS asset catalog: an AppIcon set, compiled into the Assets.car
# that CFBundleIconName refers to (macOS 10.13 and later).
#
# The macOS build uses the Ninja generator, so there is no asset catalog step to
# hook into the way the Xcode generator provides one for iOS - actool has to be
# called directly. It ships with Xcode, not with the command line tools.
#
#   cmake -DIMAGESCALE=<path> -DASSET_DIR=<srcdir>/assets -DOUT_DIR=<build>/macos
#         [-DDEPLOYMENT_TARGET=13.0] -P macos/generate-macos-icons.cmake

cmake_minimum_required(VERSION 3.19.0)

include("${CMAKE_CURRENT_LIST_DIR}/../cmake/RunImageScale.cmake")

if (NOT IMAGESCALE OR NOT ASSET_DIR OR NOT OUT_DIR)
    message(FATAL_ERROR "IMAGESCALE, ASSET_DIR and OUT_DIR are required")
endif()

find_program(XCRUN xcrun REQUIRED)

set(ICONSET "${OUT_DIR}/Assets.xcassets/AppIcon.appiconset")
file(MAKE_DIRECTORY "${ICONSET}")

# macOS wants every size at 1x and 2x. The file names are arbitrary - Contents.json
# is what ties a file to a size and a scale - but these are what Xcode would use.
set(icons "")
set(entries "")
foreach (points IN ITEMS 16 32 128 256 512)
    foreach (scale IN ITEMS 1 2)
        math(EXPR pixels "${points} * ${scale}")
        set(name "icon_${points}x${points}")
        if (scale EQUAL 2)
            string(APPEND name "@2x")
        endif()
        list(APPEND icons "${pixels}:${name}")
        string(APPEND entries
            "    {\n"
            "      \"idiom\" : \"mac\",\n"
            "      \"size\" : \"${points}x${points}\",\n"
            "      \"scale\" : \"${scale}x\",\n"
            "      \"filename\" : \"${name}.png\"\n"
            "    },\n")
    endforeach()
endforeach()

string(REGEX REPLACE ",\n$" "\n" entries "${entries}")
file(WRITE "${ICONSET}/Contents.json"
    "{\n"
    "  \"images\" : [\n${entries}  ],\n"
    "  \"info\" : { \"author\" : \"xcode\", \"version\" : 1 }\n"
    "}\n")

message(STATUS "Generating the macOS app icons in ${ICONSET}")

# 512@2x is 1024, above the 512 master - the iOS store icon has the same problem.
# --allow-upscale keeps the guard on for every other size.
imagescale_run("the macOS app icons"
    --allow-upscale "${ASSET_DIR}/brickstore.png" "${ICONSET}" ${icons})

set(deployment_target_args "")
if (DEPLOYMENT_TARGET)
    set(deployment_target_args --minimum-deployment-target "${DEPLOYMENT_TARGET}")
endif()

message(STATUS "Compiling the asset catalog into ${OUT_DIR}/Assets.car")

execute_process(
    COMMAND "${XCRUN}" actool
            --output-format human-readable-text --notices --warnings --errors
            --platform macosx --target-device mac
            --enable-on-demand-resources NO
            ${deployment_target_args}
            --app-icon AppIcon
            --output-partial-info-plist "${OUT_DIR}/AppIcon-partial.plist"
            --compile "${OUT_DIR}"
            "${OUT_DIR}/Assets.xcassets"
    RESULT_VARIABLE result
)
if (NOT result EQUAL 0)
    message(FATAL_ERROR "actool failed")
endif()
if (NOT EXISTS "${OUT_DIR}/Assets.car")
    message(FATAL_ERROR "actool did not produce an Assets.car")
endif()

# actool is supposed to emit an .icns next to the catalog for macOS targets. If it
# does, CFBundleIconFile can point at that one and assets/generated-app-icons/
# brickstore.icns stops being needed - report it either way, so that the first run
# on a machine with Xcode answers the question.
if (EXISTS "${OUT_DIR}/AppIcon.icns")
    message(STATUS "actool also produced AppIcon.icns")
else()
    message(STATUS "actool did not produce an AppIcon.icns")
endif()
