#!/bin/bash

## Copyright (C) 2004-2021 Robert Griebl. All rights reserved.
##
## This file is part of BrickStore.
##
## This file may be distributed and/or modified under the terms of the GNU 
## General Public License version 2 as published by the Free Software Foundation 
## and appearing in the file LICENSE.GPL included in the packaging of this file.
##
## This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
## WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
##
## See http://fsf.org/licensing/licenses/gpl.html for GPL licensing information.

#set +x

b="$(dirname $0)/../assets"
which realpath >/dev/null && b="$(realpath --relative-to=. $b)"

i="$b/generated-icons"
g="$b/generated-32x32"
x="$b/generated-misc"
a="$b/crystal-clear-32x32/actions"
f="$b/crystal-clear-32x32/filesystems"
m="$b/crystal-clear-128x128/mimetypes"
c="$b/custom-64x64"
s=32

mkdir -p "$g"
mkdir -p "$i"
mkdir -p "$x"

######################################
# app and doc icons

echo -n "Generating app and doc icons..."

# Unix icons
convert $b/brickstore.png -resize 256 $i/brickstore.png
convert -size 128x128 canvas:transparent \
        $m/spreadsheet.png -composite \
        $b/brickstore.png -geometry 88x88+20+4 -composite \
        $i/brickstore_doc.png

# Windows icons
convert $i/brickstore.png -define icon:auto-resize=256,48,32,16 $i/brickstore.ico
convert $i/brickstore_doc.png -define icon:auto-resize=128,48,32,16 $i/brickstore_doc.ico

# macOS icons
## png2icns is broken for icons >= 256x256
#png2icns $i/brickstore.icns $i/brickstore.png >/dev/null
#png2icns $i/brickstore_doc.icns $i/brickstore_doc.png >/dev/null

## and makeicns is only available on macOS via brew
if which makeicns >/dev/null; then
  makeicns -256 $i/brickstore.png -32 $i/brickstore.png -out $i/brickstore.icns
  makeicns -128 $i/brickstore_doc.png -32 $i/brickstore_doc.png -out $i/brickstore_doc.icns
fi

echo "done"

#######################################
# edit icons with overlays

echo -n "Generating action icons..."

convert $b/brickstore.png -resize $((s)) $g/brickstore.png

convert $c/tab.png -resize $((s)) $g/tab.png

convert $c/brick_1x1.png -colorspace sRGB -scale $s $c/overlay_plus.png -scale $s -composite $g/items_add.png
convert $c/brick_1x1.png -colorspace sRGB -scale $s $c/overlay_divide.png -scale $s -composite $g/items_divide.png
convert $c/brick_1x1.png -colorspace sRGB -scale $s $c/overlay_multiply.png -scale $s -composite $g/items_multiply.png
convert $c/brick_1x1.png -colorspace sRGB -scale $s $c/overlay_minus.png -scale $s -composite $g/items_subtract.png
convert $c/brick_1x1.png -colorspace sRGB -scale $s $c/overlay_merge.png -scale $s -composite $g/items_merge.png
convert $c/brick_1x1.png -colorspace sRGB -scale $s $c/overlay_split.png -scale $s -composite $g/items_part_out.png

convert $c/dollar.png -colorspace sRGB -scale $s $c/overlay_plusminus.png -scale $s -composite $g/price_inc_dec.png
convert $c/dollar.png -colorspace sRGB -scale $s $c/overlay_equals.png -scale $s -composite $g/price_set.png
convert $c/dollar.png -colorspace sRGB -scale $s $c/overlay_percent.png -scale $s -composite $g/price_sale.png

convert -resize $s $c/ldraw.png $g/ldraw.png
convert -resize $s $c/bricklink.png $g/bricklink.png
convert -resize $s $c/status_plus.png $g/status_extra.png
convert -resize $s $c/status_unknown.png $g/status_unknown.png

convert -size ${s}x${s} canvas:transparent \
        \( $a/search.png -scale $((s*3/4)) \) -geometry +$((s/8))+$((s/8)) -composite \
        $g/filter.png

convert -size ${s}x${s} canvas:transparent \
        \( $m/spreadsheet.png -scale $((s*3/4)) \) -geometry +$((s/4))+$((s/4)) -composite \
        \( $c/overlay_import_export.png -scale $((s*3/4)) \) -geometry +0+0 -composite \
        $g/file_import.png
        
convert -size ${s}x${s} canvas:transparent \
        \( $m/spreadsheet.png -scale $((s*3/4)) \) -geometry +0+0 -composite \
        \( $c/overlay_import_export.png -scale $((s*3/4)) \) -geometry +$((s/4))+$((s/4)) -composite \
        $g/file_export.png

convert -size ${s}x${s} canvas:transparent \
        \( $c/dollar.png -scale $s \) -geometry +0+0 -composite \
        \( $c/bricklink.png -scale $((s*5/8)) \) -geometry +$((s*3/8))+$((s*3/8)) -composite \
        $g/bricklink_priceguide.png

convert -size ${s}x${s} canvas:transparent \
        \( $a/info.png -scale $s \) -geometry +0+0 -composite \
        \( $c/bricklink.png -scale $((s*5/8)) \) -geometry +$((s*3/8))+$((s*3/8)) -composite \
        $g/bricklink_info.png

echo "done"

#######################################
# creating installer images

echo "Generating images for installers..."

convert $i/brickstore.png -resize 96x96 -define bmp3:alpha=true bmp3:$x/windows-installer.bmp

echo "done"

#######################################
# optimize sizes

if which zopflipng >/dev/null; then
  echo "Optimizing..."

  for png in $(ls -1 $g/*.png $i/*.png $m/*.png); do
    echo -n " > ${png}... "
    zopflipng -my "$png" "$png" >/dev/null
    #optipng -o7 "$1"
    echo "done"
  done
else
  echo "Not optimizing: zopflipng is not available"
fi
