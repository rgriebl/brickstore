#!/bin/bash

rm -f small-logo??.png

for i in logo??.png; do convert $i -resize 22x22 small-$i; done

tiles=`ls small-logo??.png | wc -l`

montage -background transparent -geometry 22x22 -tile 1x$tiles small-logo??.png spinner.png

rm -f small-logo??.png
rm -f logo??.png
