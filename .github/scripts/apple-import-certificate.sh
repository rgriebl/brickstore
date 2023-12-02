#!/bin/bash

set -euo pipefail

security create-keychain -p "" build.keychain
security list-keychains -s build.keychain
security default-keychain -s build.keychain
security unlock-keychain -p "" build.keychain
security set-keychain-settings
TMP_SIGCERT=/tmp/sigcert.p12
echo $SIGNING_CERTIFICATE_P12_DATA | base64 --decode >${TMP_SIGCERT}
security import ${TMP_SIGCERT} \
                -f pkcs12 \
                -k build.keychain \
                -P $SIGNING_CERTIFICATE_PASSWORD \
                -T /usr/bin/codesign
rm -f ${TMP_SIGCERT}
security set-key-partition-list -S apple-tool:,apple: -s -k "" build.keychain
