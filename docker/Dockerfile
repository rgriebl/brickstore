# Copyright (C) 2004-2026 Robert Griebl
# SPDX-License-Identifier: GPL-3.0-only

FROM debian:trixie-slim
MAINTAINER Robert Griebl <robert@griebl.org>

ARG BRICKSTORE_BACKEND_DEB=brickstore-backend.deb

ENV LC_ALL="C.UTF-8" LANG="en_US.UTF-8" LANGUAGE="en_US.UTF-8"

RUN apt-get update && apt-get install -y --no-install-recommends \
  wget \
  ca-certificates \
  xxd \
  xz-utils \
  moreutils

ADD $BRICKSTORE_BACKEND_DEB brickstore-backend.deb

RUN /bin/bash -c 'apt-get install -y --no-install-recommends ./brickstore-backend.deb \
  && rm ./brickstore-backend.deb \
  && rm -rf /var/lib/apt/lists/* \
  '
  
RUN mkdir /data
VOLUME /data

ENV DB_PATH=/data/db
ENV CACHE_PATH=/data/cache

## ENV BRICKLINK_USERNAME
## ENV BRICKLINK_PASSWORD
## ENV BRICKLINK_AFFILIATE_APIKEY
## ENV REBRICKABLE_APIKEY

COPY entrypoint.sh /entrypoint.sh
RUN chmod 755 /entrypoint.sh

ENTRYPOINT [ "/entrypoint.sh" ]
CMD [ "--rebuild-database" ]
