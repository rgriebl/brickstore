#!/bin/bash

set -euo pipefail

# iOS Distribution
mkdir -p ~/Library/MobileDevice/Provisioning\ Profiles
echo "$PROVISIONING_PROFILE_DATA" | base64 --decode > ~/Library/MobileDevice/Provisioning\ Profiles/${PROVISIONING_PROFILE_SPECIFIER}.mobileprovision

# Apple Distribution
mkdir -p ~/Library/Developer/Xcode/UserData/Provisioning\ Profiles
echo "$PROVISIONING_PROFILE_DATA" | base64 --decode > ~/Library/Developer/Xcode/UserData/Provisioning\ Profiles/${PROVISIONING_PROFILE_SPECIFIER}.mobileprovision
