#!/bin/bash

# Copyright (C) 2004-2023 Robert Griebl
# SPDX-License-Identifier: GPL-3.0-only

# This script can be run from a cronjob to trigger a db rebuild
# using brickstore-backend docker image.
# Make sure to adjust to your environment!

BRICKSTORE_BUILD_NUMBER=59

BS_LOCAL_USER=brickstore
BS_LOCAL_DIR="/srv/brickstore"
BS_UPLOAD_PORT="22"
BS_UPLOAD_DIR="user@host:/dir"

if [ "$(id -un)" != "$BS_LOCAL_USER" ]; then
  echo "Please run this script as user '$BS_LOCAL_USER'."
  exit 2
fi

interactive=""
[ -t 0 ] && interactive="-it"

docker run $interactive --rm \
    -e 'BRICKLINK_USERNAME=xxx' \
    -e 'BRICKLINK_PASSWORD=yyy' \
    -e 'REBRICKABLE_APIKEY=zzz' \
    -v "$BS_LOCAL_DIR:/data" \
    --user $(id -u $BS_LOCAL_USER) \
    --name "brickstore-backend" \
    rgriebl/brickstore-backend:${BRICKSTORE_BUILD_NUMBER}

cd "$BS_DIR"

echo
echo -n "Uploading to server... "

scp -q -P ${BS_UPLOAD_PORT} db/*.lzma "${BS_UPLOAD_DIR}"

echo "done"
echo
