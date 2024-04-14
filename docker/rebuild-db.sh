#!/bin/bash

# Copyright (C) 2004-2024 Robert Griebl
# SPDX-License-Identifier: GPL-3.0-only

set -e

# This script can be run from a cronjob to trigger a db rebuild
# using brickstore-backend docker image.
# Make sure to adjust to your environment!

BRICKSTORE_BUILD_NUMBER=966

LOCAL_USER="brickstore"
LOCAL_DIR="/srv/brickstore"
UPLOAD_SSH_PORT="22"
UPLOAD_DESTINATION="user@host:/dir"

BRICKLINK_USERNAME='xxx'
BRICKLINK_PASSWORD='yyy'
BRICKLINK_AFFILIATE_APIKEY='aaa'
REBRICKABLE_APIKEY='zzz'


######################################################################

interactive=""
if [ -t 0 ]; then
  interactive="-it"
else
  mkdir -p "$LOCAL_DIR/logs"
  exec &> >(tee $LOCAL_DIR/logs/log-`date -Iseconds`.log)
fi

if [ "$(id -un)" != "$LOCAL_USER" ]; then
  echo "Please run this script as user '$LOCAL_USER'."
  exit 2
fi

docker run $interactive --rm \
    -e BRICKLINK_USERNAME="$BRICKLINK_USERNAME" \
    -e BRICKLINK_PASSWORD="$BRICKLINK_PASSWORD" \
    -e BRICKLINK_AFFILIATE_APIKEY="$BRICKLINK_AFFILIATE_APIKEY" \
    -e REBRICKABLE_APIKEY="$REBRICKABLE_APIKEY" \
    -v "$LOCAL_DIR:/data" \
    --user $(id -u $LOCAL_USER) \
    --name "brickstore-backend" \
    rgriebl/brickstore-backend:${BRICKSTORE_BUILD_NUMBER}

cd "$LOCAL_DIR"

echo
echo " Uploading to server "
echo "====================="
echo

scp -q -P ${UPLOAD_SSH_PORT} db/*.lzma "${UPLOAD_SSH_DESTINATION}"

echo
echo " FINISHED."
echo
