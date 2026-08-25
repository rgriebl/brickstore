# Copyright (C) 2004-2026 Robert Griebl
# SPDX-License-Identifier: GPL-3.0-only

# Stages the Android package source directory in the build directory and generates
# the launcher icons into it.
#
# androiddeployqt takes exactly one package source directory and copies it over its
# Gradle template, with no way to add generated files on the side. Staging the whole
# thing is what keeps the icons out of the source tree.
#
# This sits in the directory it stages, next to the Gradle files it belongs with, so
# it excludes itself from the copy below. Nothing breaks if that exclude is ever
# lost: androiddeployqt hands the staged tree to Gradle, and AGP packages only res/,
# assets/, src/ and the manifest, so a stray .cmake could never reach the APK - it
# would only be clutter in the Gradle project. The exclude takes the name from
# CMAKE_CURRENT_LIST_FILE instead of spelling it out, so renaming this file cannot
# quietly break it.
#
# Note that the staging is additive - a file removed from android/ stays behind in
# an existing OUT_DIR, so that one has to be deleted by hand.
#
#   cmake -DIMAGESCALE=<path> -DASSET_DIR=<srcdir>/assets -DSOURCE_DIR=<srcdir>/android
#         -DOUT_DIR=<build>/android-package -P android/generate-package.cmake

cmake_minimum_required(VERSION 3.19.0)

include("${CMAKE_CURRENT_LIST_DIR}/../cmake/RunImageScale.cmake")

if (NOT IMAGESCALE OR NOT ASSET_DIR OR NOT SOURCE_DIR OR NOT OUT_DIR)
    message(FATAL_ERROR "IMAGESCALE, ASSET_DIR, SOURCE_DIR and OUT_DIR are required")
endif()

# file(COPY) preserves timestamps and skips anything already up to date, so this
# does not churn the inputs of the Gradle build on every run.
# .gradle is a local Gradle cache, complete with lock files, and editor backups are
# no more welcome in the Gradle project - both used to be copied in when the package
# source directory still pointed straight at android/.
# The launcher icons are skipped as well: they are generated below, and a stray
# copy in the source tree must never shadow that.
# And this script, which lives in the very tree it is copying.
get_filename_component(THIS_SCRIPT "${CMAKE_CURRENT_LIST_FILE}" NAME)

file(COPY "${SOURCE_DIR}/" DESTINATION "${OUT_DIR}"
    PATTERN ".gradle" EXCLUDE
    PATTERN "*~" EXCLUDE
    PATTERN "${THIS_SCRIPT}" EXCLUDE
    REGEX "/res/drawable-[a-z]+/icon\\.png$" EXCLUDE
)

set(MASTER "${ASSET_DIR}/brickstore.png")

# The density buckets: 0.75x, 1x, 1.5x, 2x, 3x and 4x of the 48px mdpi baseline
foreach (bucket IN ITEMS "ldpi:36" "mdpi:48" "hdpi:72" "xhdpi:96" "xxhdpi:144" "xxxhdpi:192")
    string(REPLACE ":" ";" parts "${bucket}")
    list(GET parts 0 density)
    list(GET parts 1 size)

    set(dir "${OUT_DIR}/res/drawable-${density}")
    file(MAKE_DIRECTORY "${dir}")

    # same reason as the file(COPY) above: only touch it when an input moved on.
    # This script is an input too - it is where the sizes below are decided.
    if (EXISTS "${dir}/icon.png"
            AND NOT "${MASTER}" IS_NEWER_THAN "${dir}/icon.png"
            AND NOT "${CMAKE_CURRENT_LIST_FILE}" IS_NEWER_THAN "${dir}/icon.png")
        continue()
    endif()

    message(STATUS "Generating the ${density} launcher icon (${size}x${size})")

    imagescale_run("the ${density} launcher icon" "${MASTER}" "${dir}" "${size}:icon")
endforeach()
