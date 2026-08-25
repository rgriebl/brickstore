# Copyright (C) 2004-2026 Robert Griebl
# SPDX-License-Identifier: GPL-3.0-only

# Generates the iOS AppIcon set.
#
# These are not plain downscales: the App Store rejects an icon with an alpha
# channel, and the artwork sits on an opaque plate with a margin - which is what
# --background and --inset are for.
#
#   cmake -DIMAGESCALE=<path> -DASSET_DIR=<srcdir>/assets -DCONTENTS_JSON=<file>
#         -DOUT_DIR=<staging>/AppIcon.xcassets/AppIcon.appiconset
#         -P macos/generate-ios-icons.cmake

cmake_minimum_required(VERSION 3.19.0)

include("${CMAKE_CURRENT_LIST_DIR}/../cmake/RunImageScale.cmake")

if (NOT IMAGESCALE OR NOT ASSET_DIR OR NOT CONTENTS_JSON OR NOT OUT_DIR)
    message(FATAL_ERROR "IMAGESCALE, ASSET_DIR, CONTENTS_JSON and OUT_DIR are required")
endif()

file(MAKE_DIRECTORY "${OUT_DIR}")
configure_file("${CONTENTS_JSON}" "${OUT_DIR}/Contents.json" COPYONLY)

# The names are the file names Contents.json refers to, the sizes are the points
# times the scale factor: 60@2x, 60@3x, 76@2x, 83.5@2x, plus the store icon.
set(icons
    "120:icon@2x"
    "180:icon@3x"
    "152:icon@2x~ipad"
    "167:icon-83.5@2x~ipad"
    "1024:icon-1024"
)

message(STATUS "Generating iOS app icons in ${OUT_DIR}")

# --allow-upscale is only needed for the 1024 store icon: its artwork is 922px
# against a 512px master. A bigger master would be the real fix.
imagescale_run("the iOS app icons"
    --background white --inset 5 --allow-upscale
    "${ASSET_DIR}/brickstore.png" "${OUT_DIR}" ${icons})

list(LENGTH icons count)
message(STATUS "Generated ${count} iOS app icons")
