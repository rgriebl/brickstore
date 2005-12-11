#!/bin/bash

ldview="/cygdrive/c/Programme/LDView/LDView.exe"
src=logo.ldr


for i in `seq 1 36`; do

  num=`printf %02d $i`
  angle=$(( 40 + $(( $i * 10 )) ))

  $ldview "$src" -SaveSnapshot=logo$num.png -SaveZoomToFit=0 -SaveHeight=200 -SaveWidth=200 -SaveAlpha=1 -DefaultLatLong=$angle,45
done
