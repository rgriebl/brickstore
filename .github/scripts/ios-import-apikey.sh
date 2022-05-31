#!/bin/bash

set -euo pipefail

mkdir -p ~/.appstoreconnect/private_keys
echo "$API_KEY_DATA" | base64 --decode > ~/.appstoreconnect/private_keys/AuthKey_${API_KEY}.p8
