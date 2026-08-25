# Copyright (C) 2004-2026 Robert Griebl
# SPDX-License-Identifier: GPL-3.0-only

# Generates the Windows images that are not committed: the .ico files that
# brickstore.rc compiles into the executable, and the Inno Setup wizard image.
#
#   cmake -DIMAGESCALE=<path> -DASSET_DIR=<srcdir>/assets -DOUT_DIR=<build>/generated/icons
#         -P windows/generate-icons.cmake

cmake_minimum_required(VERSION 3.19.0)

include("${CMAKE_CURRENT_LIST_DIR}/../cmake/RunImageScale.cmake")

if (NOT IMAGESCALE OR NOT ASSET_DIR OR NOT OUT_DIR)
    message(FATAL_ERROR "IMAGESCALE, ASSET_DIR and OUT_DIR are required")
endif()

file(MAKE_DIRECTORY "${OUT_DIR}")

# 256 is what an icon entry can hold at most, and what Explorer uses for the
# large views; the rest are the sizes the shell asks for below that.
set(SIZES 256 96 48 32 16)

function(generate_ico source name)
    message(STATUS "Generating ${name}.ico")

    imagescale_run("${name}.ico" --ico "${OUT_DIR}/${name}.ico" "${source}" ${SIZES})
endfunction()

generate_ico("${ASSET_DIR}/brickstore.png" brickstore)

# the document icon is a composite that only generate-assets.sh can build, so the
# committed 256px result is the master here
generate_ico("${ASSET_DIR}/generated-app-icons/brickstore_doc.png" brickstore_doc)

# The Inno Setup wizard image. WizardSmallImageFile takes a .png since Inno 6.3,
# so one file covers every scaling mode: the image area runs from 58x58 at 100%
# DPI to 159x159 at 250%, and Inno shrinks a single larger image to fit.
message(STATUS "Generating windows-installer.png")

imagescale_run("windows-installer.png"
    "${ASSET_DIR}/brickstore.png" "${OUT_DIR}" 256:windows-installer)
