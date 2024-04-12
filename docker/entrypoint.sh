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

exec &> >(tee >(gzip -c > $LOG_PATH/log-`date -Iseconds`.log.gz) )

stdbuf -oL -eL /usr/bin/brickstore "$@"

echo
echo " Compressing database "
echo "======================"
echo

cd "$DB_PATH"
parallel -i sh -c 'sha512sum < "{}" | xxd -r -p > "$BRICKSTORE_CACHE_PATH/{}.lzma" ; \
                   xz -F alone -T 1 -c -v "{}" >> "$BRICKSTORE_CACHE_PATH/{}.lzma"' \
  -- database-v* |& sort -V

echo
echo " FINISHED."
echo
