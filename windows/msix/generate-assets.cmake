# Copyright (C) 2004-2026 Robert Griebl
# SPDX-License-Identifier: GPL-3.0-only

# Generates the tile and file-association images for the MSIX package.
#
# Unlike everything in assets/generated-app-icons, these are not committed: they
# only ever exist inside the package, no qrc or resource script references them,
# and the scale and target size matrix means ~70 files for two source images.
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

# The sizes the shell asks the app list icon for, per "Construct your Windows
# App's Icon". All three theme forms are wanted even when the image is identical:
# without the unplated ones the icon is drawn smaller, on a system backplate.
set(TARGET_SIZES 16 20 24 30 32 36 40 48 60 64 72 80 96 256)

add_targetsizes(app_assets Square44x44Logo ""                       ${TARGET_SIZES})
add_targetsizes(app_assets Square44x44Logo "_altform-unplated"      ${TARGET_SIZES})
add_targetsizes(app_assets Square44x44Logo "_altform-lightunplated" ${TARGET_SIZES})

# Windows 10 also picks the app list icon by scale factor. The tile and the store
# logo only ever go by scale - and the store logo has to cover all five to be
# publishable.
add_scales(app_assets 44 Square44x44Logo 100 125 150 200 400)
add_scales(app_assets 150 Square150x150Logo 100 125 150 200 400)
add_scales(app_assets 50 StoreLogo 100 125 150 200 400)

# The .bsx association icon, referenced by the manifest's file type extension.
# Microsoft documents no size list for those, so it mirrors the app list one.
add_targetsizes(doc_assets BsxDocumentLogo "" ${TARGET_SIZES})

# The manifest names the unqualified files - everything above is an alternative
# that the resource system picks by DPI once makepri has indexed it. Emitting the
# base files too keeps a manifest reference resolvable on its own.
list(APPEND app_assets "44:Square44x44Logo" "150:Square150x150Logo" "50:StoreLogo")
list(APPEND doc_assets "48:BsxDocumentLogo")

message(STATUS "Generating MSIX assets in ${OUT_DIR}")

# Square150x150Logo.scale-400 is 600px, above the 512 master - it is the only
# entry that needs the upscale, the same way the iOS store icon does.
imagescale_run("the MSIX app assets"
    --allow-upscale "${ASSET_DIR}/brickstore.png" "${OUT_DIR}" ${app_assets})
imagescale_run("the MSIX document assets"
    "${ASSET_DIR}/generated-app-icons/brickstore_doc.png" "${OUT_DIR}" ${doc_assets})

list(LENGTH app_assets app_count)
list(LENGTH doc_assets doc_count)
math(EXPR count "${app_count} + ${doc_count}")
message(STATUS "Generated ${count} MSIX assets")
