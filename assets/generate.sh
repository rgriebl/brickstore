#!/bin/sh

set +x

i="icons"
g="generated-32x32"
a="crystal-clear-32x32/actions"
f="crystal-clear-32x32/filesystems"
m="crystal-clear-128x128/mimetypes"
c="custom-64x64"
s=32

mkdir -p "$g"
mkdir -p "$i"

######################################
# app and doc icons

echo -n "Generating app and doc icons..."

# Unix icons
convert brickstore.png -resize 256 $i/brickstore.png
composite -geometry 88x88+20+4 brickstore.png $m/spreadsheet.png $i/brickstore_doc.png

# Windows icons
convert $i/brickstore.png -define icon:auto-resize=256,48,32,16 $i/brickstore.ico
convert $i/brickstore_doc.png -define icon:auto-resize=128,48,32,16 $i/brickstore_doc.ico

# macOS icons
png2icns $i/brickstore.icns $i/brickstore.png >/dev/null
png2icns $i/brickstore_doc.icns $i/brickstore_doc.png >/dev/null

echo "done"

#######################################
# edit icons with overlays

echo -n "Generating action icons..."

convert brickstore.png -resize $((s*2)) $g/brickstore.png  # double size

convert $c/brick_1x1.png -scale $s $c/overlay_plus.png -scale $s -composite $g/items_add.png
convert $c/brick_1x1.png -scale $s $c/overlay_divide.png -scale $s -composite $g/items_divide.png
convert $c/brick_1x1.png -scale $s $c/overlay_multiply.png -scale $s -composite $g/items_multiply.png
convert $c/brick_1x1.png -scale $s $c/overlay_minus.png -scale $s -composite $g/items_subtract.png
convert $c/brick_1x1.png -scale $s $c/overlay_merge.png -scale $s -composite $g/items_merge.png
convert $c/brick_1x1.png -scale $s $c/overlay_split.png -scale $s -composite $g/items_part_out.png

convert $c/dollar.png -scale $s $c/overlay_plusminus.png -scale $s -composite $g/price_inc_dec.png
convert $c/dollar.png -scale $s $c/overlay_equals.png -scale $s -composite $g/price_set.png
convert $c/dollar.png -scale $s $c/overlay_percent.png -scale $s -composite $g/price_sale.png

convert -resize $s $c/ldraw.png $g/ldraw.png
convert -resize $s $c/bricklink.png $g/bricklink.png
convert -resize $s $c/status_plus.png $g/status_extra.png
convert -resize $s $c/status_unknown.png $g/status_unknown.png

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
# optimize sizes

echo "Optimizing..."

for png in $(ls -1 $g/*.png $i/*.png); do
  echo -n " > ${png}... "
  zopflipng -my "$png" "$png" >/dev/null
  #optipng -o7 "$1"
  echo "done"
done
