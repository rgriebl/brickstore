FROM debian:bullseye-slim
MAINTAINER Robert Griebl <robert@griebl.org>

ENV LC_ALL="C.UTF-8" LANG="en_US.UTF-8" LANGUAGE="en_US.UTF-8"

RUN apt-get update && apt-get install -y --no-install-recommends \
  wget \
  ca-certificates \
  jq \
  lzma-alone

RUN /bin/bash -c 'URL=$(wget -q -nv -O- https://api.github.com/repos/rgriebl/brickstore/releases/latest 2>/dev/null | jq -r ".assets[] | select(.browser_download_url | contains(\"Debian\")) | .browser_download_url") \
  && wget -q -nv -O brickstore.deb "$URL" \
  && apt-get install -y --no-install-recommends ./brickstore.deb \
  && rm ./brickstore.deb \
  && rm -rf /var/lib/apt/lists/* \
  '
  
RUN mkdir /data
ENV XDG_CACHE_HOME=/data 
VOLUME /data

## ENV BRICKSTORE_USERNAME
## ENV BRICKSTORE_PASSWORD

COPY entrypoint.sh /entrypoint.sh
RUN chmod 755 /entrypoint.sh

ENTRYPOINT [ "/entrypoint.sh" ]
CMD [ "--rebuild-database" ]
