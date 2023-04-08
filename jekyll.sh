#!/bin/sh

export JEKYLL_VERSION=latest
  
docker run --rm \
  --env JEKYLL_UID=$(id -u) \
  --env JEKYLL_GID=$(id -g) \
  -p 4000:4000 \
  --volume="$PWD:/srv/jekyll" \
  --volume="$PWD/.vendor/bundle:/usr/local/bundle" \
  --volume="$PWD/.vendor/gem:/usr/gem" \
  -it jekyll/jekyll:$JEKYLL_VERSION \
  "$@"
