#!/bin/bash

set -e
set -o pipefail

export XDG_CACHE_HOME="$CACHE_PATH"
BRICKSTORE_CACHE_PATH="$CACHE_PATH/BrickStore"

mkdir -p "$DB_PATH"
mkdir -p "$LOG_PATH"
mkdir -p "$CACHE_PATH"

stdbuf -oL -eL /usr/bin/brickstore "$@" 2>&1 \
  | tee >(gzip -c > $LOG_PATH/brickstore-rebuild-database-`date -Iseconds`.log.gz)

echo
echo " Compressing database"
echo "======================"
echo

for i in $(seq 3 20); do
  dbname=database-v$i

  rm -f "$DB_PATH/$db_name"

  [ -e "$BRICKSTORE_CACHE_PATH/$dbname" ] || continue

  echo -n "  > $dbname... "

  if [ "$i" -ge "4" ]; then
    sha512sum < "$BRICKSTORE_CACHE_PATH/$dbname" | xxd -r -p > "$DB_PATH/$dbname.lzma"
  else
    rm -f "$DB_PATH/$dbname.lzma"
  fi

  lzma_alone e "$BRICKSTORE_CACHE_PATH/$dbname" -so >>"$DB_PATH/$dbname.lzma" 2>/dev/null
  echo "done"
done
