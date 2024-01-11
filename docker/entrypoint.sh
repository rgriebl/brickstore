#!/bin/bash

# Copyright (C) 2004-2024 Robert Griebl
# SPDX-License-Identifier: GPL-3.0-only

set -e
set -o pipefail

export XDG_CACHE_HOME="$CACHE_PATH"
BRICKSTORE_CACHE_PATH="$CACHE_PATH/BrickStore"

mkdir -p "$DB_PATH"
mkdir -p "$LOG_PATH"
mkdir -p "$CACHE_PATH"

stdbuf -oL -eL /usr/bin/brickstore "$@" 2>&1 \
  | tee >(gzip -c > $LOG_PATH/log-`date -Iseconds`.log.gz)

echo
echo "Compressing databases..."

for i in $(seq 4 20); do
  dbname=database-v$i

  rm -f "$DB_PATH/$dbname"

  [ -e "$BRICKSTORE_CACHE_PATH/$dbname" ] || continue

  echo -n "  > $dbname... "

  sha512sum < "$BRICKSTORE_CACHE_PATH/$dbname" | xxd -r -p > "$DB_PATH/$dbname.lzma"
  lzma_alone e "$BRICKSTORE_CACHE_PATH/$dbname" -so >>"$DB_PATH/$dbname.lzma" 2>/dev/null

  echo "done"
done
