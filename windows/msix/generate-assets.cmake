# Copyright (C) 2004-2026 Robert Griebl
# SPDX-License-Identifier: GPL-3.0-only

# Generates the tile and file-association images for the MSIX package.
#
# Unlike everything in assets/generated-app-icons, these are not committed: they
# only ever exist inside the package, no qrc or resource script references them,
# and the scale matrix means ~20 files for two source images.
#
# The scaling itself is done by the imagescale tool, so that no image toolchain
# has to be installed on a Windows machine:
#   cmake -DIMAGESCALE=<path> -DASSET_DIR=<srcdir>/assets -DOUT_DIR=<staging>/Assets
#         -P windows/msix/generate-assets.cmake

cmake_minimum_required(VERSION 3.19.0)

include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/RunImageScale.cmake")

if (NOT IMAGESCALE OR NOT ASSET_DIR OR NOT OUT_DIR)
    message(FATAL_ERROR "IMAGESCALE, ASSET_DIR and OUT_DIR are required")
endif()

file(MAKE_DIRECTORY "${OUT_DIR}")

# <name>.scale-<percent>.png: the plated tile images, picked by the shell's DPI
function(add_scales out_var base_size name)
    set(assets ${${out_var}})
    foreach (scale IN ITEMS ${ARGN})
        math(EXPR size "(${base_size} * ${scale} + 50) / 100")
        list(APPEND assets "${size}:${name}.scale-${scale}")
    endforeach()
    set(${out_var} ${assets} PARENT_SCOPE)
endfunction()

# <name>.targetsize-<px>[_altform-unplated].png: the sizes the shell asks for
# directly - taskbar, alt-tab, explorer. Unplated means no tile plate behind it.
function(add_targetsizes out_var name suffix)
    set(assets ${${out_var}})
    foreach (size IN ITEMS ${ARGN})
        list(APPEND assets "${size}:${name}.targetsize-${size}${suffix}")
    endforeach()
    set(${out_var} ${assets} PARENT_SCOPE)
endfunction()

# The masters are 512x512 and 256x256, and imagescale refuses to upscale: adding
# Square150x150Logo.scale-400 (600px) or a 310x310 tile needs a bigger master first.
add_scales(app_assets 44 Square44x44Logo 100 125 150 200 400)
add_targetsizes(app_assets Square44x44Logo "_altform-unplated" 16 24 32 48 256)
add_scales(app_assets 150 Square150x150Logo 100 125 150 200)
add_scales(app_assets 50 StoreLogo 100 200)

# the .bsx association icon, referenced by the manifest's file type extension
add_targetsizes(doc_assets BsxDocumentLogo "" 16 32 48 256)

message(STATUS "Generating MSIX assets in ${OUT_DIR}")

imagescale_run("the MSIX app assets"
    "${ASSET_DIR}/brickstore.png" "${OUT_DIR}" ${app_assets})
imagescale_run("the MSIX document assets"
    "${ASSET_DIR}/generated-app-icons/brickstore_doc.png" "${OUT_DIR}" ${doc_assets})

list(LENGTH app_assets app_count)
list(LENGTH doc_assets doc_count)
math(EXPR count "${app_count} + ${doc_count}")
message(STATUS "Generated ${count} MSIX assets")
