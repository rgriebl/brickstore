#!/bin/sh

set -e

stdbuf -oL -eL /usr/bin/brickstore "$@"

dbpath=/data/brickstore

echo COMPRESSING

for i in 1 2 3 4 5 6 7 8 9; do
  dbname=database-v$i

  [ -e $dbpath/$dbname ] || continue

  echo -n "  > $dbname... "
  lzma_alone e $dbpath/$dbname $dbpath/$dbname.lzma 2>/dev/null
  echo "done"
done
