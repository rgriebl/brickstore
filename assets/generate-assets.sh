#!/bin/bash

## Copyright (C) 2004-2022 Robert Griebl. All rights reserved.
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

b="$(dirname $0)"
which realpath >/dev/null && b="$(realpath --relative-to=. $b)"

cus="$b/custom"

gai="$b/generated-app-icons"
gin="$b/generated-installers"

theme="brickstore-breeze"
s=64

mkdir -p "$gai"
mkdir -p "$gin"

######################################
# app and doc icons

echo -n "Generating app and doc icons... "

# Unix icons
convert $b/brickstore.png -resize 256 $gai/brickstore.png
convert -size 256x256 canvas:transparent \
        $cus/oxygen-x-office-spreadsheet.png -composite \
        $b/brickstore.png -geometry 172x172+40+8 -composite \
        $gai/brickstore_doc.png

# Windows icons
convert $gai/brickstore.png -define icon:auto-resize=256,96,48,32,16 $gai/brickstore.ico
convert $gai/brickstore_doc.png -define icon:auto-resize=256,96,48,32,16 $gai/brickstore_doc.ico

# macOS icons
## png2icns is broken for icons >= 256x256
#png2icns $gai/brickstore.icns $gai/brickstore.png >/dev/null
#png2icns $gai/brickstore_doc.icns $gai/brickstore_doc.png >/dev/null

## and makeicns is only available on macOS via brew
if which makeicns 2>/dev/null >/dev/null; then
  makeicns -256 $gai/brickstore.png -32 $gai/brickstore.png -out $gai/brickstore.icns
  makeicns -256 $gai/brickstore_doc.png -32 $gai/brickstore_doc.png -out $gai/brickstore_doc.icns
fi

# iOS icons
gai_ios=$gai/AppIcon.xcassets/AppIcon.appiconset
mkdir -p $gai_ios

function ios_icon() {
  local s=$1
  local id=$2

  convert -size ${s}x${s} canvas:white \
        \( $b/brickstore.png -scale $((s*9/10)) \) -geometry +$((s*1/20))+$((s*1/20)) -composite \
        -alpha remove -alpha off $gai_ios/icon${id}.png
}

ios_icon 167 "-83.5@2x~ipad"
ios_icon 152 "@2x~ipad"
ios_icon 120 "@2x"
ios_icon 180 "@3x"
ios_icon 1024 "-1024"

echo "done"

#######################################
# edit icons with overlays

echo -n "Generating action icons... "

tmp="$b/tmp"
mkdir -p "$tmp"

for color in "" "-dark"; do

  rsvg-convert $b/custom/brick-1x1$color.svg -w $s -h $s -f png -o $tmp/brick-1x1$color.png
  rsvg-convert $b/icons/$theme$color/svg/taxes-finances.svg -w $s -h $s -f png -o $tmp/dollar$color.png
  rsvg-convert $b/icons/$theme$color/svg/help-about.svg -w $s -h $s -f png -o $tmp/info$color.png
  rsvg-convert $b/icons/$theme$color/svg/format-number-percent.svg -w $s -h $s -f png -o $tmp/percent$color.png

  out="$b/icons/$theme$color/generated"
  mkdir -p "$out"

  cp $tmp/brick-1x1$color.png $out/brick-1x1.png
  convert $cus/bricklink.png -colorspace sRGB -scale $s $out/bricklink.png
  convert $cus/bricklink-studio.png -colorspace sRGB -scale $s $out/bricklink-studio.png
  convert $cus/bricklink-cart$color.png -colorspace sRGB -scale $s $out/bricklink-cart.png
  convert $cus/bricklink-store$color.png -colorspace sRGB -scale $s $out/bricklink-store.png

  convert $tmp/brick-1x1$color.png -colorspace sRGB -scale $s $cus/overlay_plus.png -scale $s -composite $out/edit-additems.png
  convert $tmp/brick-1x1$color.png -colorspace sRGB -scale $s $cus/overlay_divide.png -scale $s -composite $out/edit-qty-divide.png
  convert $tmp/brick-1x1$color.png -colorspace sRGB -scale $s $cus/overlay_multiply.png -scale $s -composite $out/edit-qty-multiply.png
  convert $tmp/brick-1x1$color.png -colorspace sRGB -scale $s $cus/overlay_minus.png -scale $s -composite $out/edit-subtractitems.png
  convert $tmp/brick-1x1$color.png -colorspace sRGB -scale $s $cus/overlay_merge.png -scale $s -composite $out/edit-mergeitems.png
  convert $tmp/brick-1x1$color.png -colorspace sRGB -scale $s $cus/overlay_split.png -scale $s -composite $out/edit-partoutitems.png

  convert $tmp/dollar$color.png -colorspace sRGB -scale $s $cus/overlay_plusminus.png -scale $s -composite $out/edit-price-inc-dec.png
  convert $tmp/dollar$color.png -colorspace sRGB -scale $s $cus/overlay_equals.png -scale $s -composite $out/edit-price-set.png
  convert $tmp/dollar$color.png -colorspace sRGB -scale $s $cus/overlay_percent.png -scale $s -composite $out/edit-sale.png

  convert -size ${s}x${s} canvas:transparent \
        \( $tmp/dollar$color.png -scale $s \) -geometry +0+0 -composite \
        \( $cus/bricklink.png -scale $((s*5/8)) \) -geometry +$((s*3/8))+$((s*3/8)) -composite \
        $out/edit-price-to-priceguide.png

  convert -size ${s}x${s} canvas:transparent \
        \( $cus/bricklink.png -scale $((s*5/8)) \) -geometry +0+0 -composite \
        \( $tmp/dollar$color.png -scale $((s*5/8)) \) -geometry +$((s*3/8))+$((s*3/8)) -composite \
        $out/bricklink-priceguide.png

  convert -size ${s}x${s} canvas:transparent \
        \( $cus/bricklink.png -scale $((s*5/8)) \) -geometry +0+0 -composite \
        \( $tmp/info$color.png -scale $((s*5/8)) \) -geometry +$((s*3/8))+$((s*3/8)) -composite \
        $out/bricklink-catalog.png

  convert -size ${s}x${s} canvas:transparent \
        \( $cus/bricklink.png -scale $((s*5/8)) \) -geometry +0+0 -composite \
        \( $tmp/brick-1x1$color.png -scale $((s*5/8)) \) -geometry +$((s*3/8))+$((s*3/8)) -composite \
        $out/bricklink-lotsforsale.png

  convert -size ${s}x${s} canvas:transparent \
        \( $tmp/percent$color.png -scale $((s*5/8)) \) -geometry +0+0 -composite \
        \( $b/icons/$theme$color/svg/package-remove.svg -scale $((s*5/8)) \) -geometry +$((s*3/8))+$((s*3/8)) -composite \
        $out/vat-excluded.png
  convert -size ${s}x${s} canvas:transparent \
        \( $tmp/percent$color.png -scale $((s*5/8)) \) -geometry +0+0 -composite \
        \( $b/icons/$theme$color/svg/emblem-checked.svg -scale $((s*5/8)) \) -geometry +$((s*3/8))+$((s*3/8)) -composite \
        $out/vat-included.png
  convert -size ${s}x${s} canvas:transparent \
        \( $tmp/percent$color.png -scale $((s*5/8)) \) -geometry +0+0 -composite \
        \( $b/flags/EU.png -scale $((s * 9 / 10)) -gravity southeast \) -composite \
        $out/vat-included-eu.png
  convert -size ${s}x${s} canvas:transparent \
        \( $tmp/percent$color.png -scale $((s*5/8)) \) -geometry +0+0 -composite \
        \( $b/flags/UK.png -scale $((s * 10 / 10)) -gravity southeast \) -composite \
        $out/vat-included-uk.png
  convert -size ${s}x${s} canvas:transparent \
        \( $tmp/percent$color.png -scale $((s*5/8)) \) -geometry +0+0 -composite \
        \( $b/flags/US.png -scale $((s * 10 / 10)) -gravity southeast \) -composite \
        $out/vat-included-us.png
  convert -size ${s}x${s} canvas:transparent \
        \( $tmp/percent$color.png -scale $((s*5/8)) \) -geometry +0+0 -composite \
        \( $b/flags/NO.png -scale $((s * 8 / 10)) -gravity southeast \) -composite \
        $out/vat-included-no.png

done

rm -rf "$tmp"

echo "done"

#######################################
# creating installer images

echo -n "Generating images for installers... "

convert $gai/brickstore.png -resize 96x96 -define bmp3:alpha=true bmp3:$gin/windows-installer.bmp

echo "done"


#######################################
# optimize sizes

if which zopflipng 2>/dev/null >/dev/null; then
  echo "Optimizing..."

  for png in $(ls -1 $cus/*.png $gai/*.png $b/icons/$theme/generated $b/icons/${theme}-dark/generated); do
    echo -n " > ${png}... "
    zopflipng -my "$png" "$png" >/dev/null
    #optipng -o7 "$1"
    echo "done"
  done
else
  echo "Not optimizing: zopflipng is not available"
fi
