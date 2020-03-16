 # Copyright 2019 Florida International University
# SPDX-License-Identifier: MIT

FROM alpine:latest AS builder

ENV CXX="clang++"
ARG PYPY_VERSION
ENV PYPY_VERSION ${PYPY_VERSION:-3.6-v7.3.0}

WORKDIR /tmp/build

ADD ./ ./

RUN apk update \
  && apk add --no-cache -t .runtime-deps \
    perl \
    R \
    git \
    musl \
    clang \
    make \
  && apk add --no-cache -t .build-deps \
    binutils \
    blas \
    blas-dev \
    build-base \
    busybox \
    clang-dev \
    clang-libs \
    gcc \
    git \
    libstdc++ \
    openrc \
    pcre-dev \
    perl \
    perl-dev \
    pkgconf \
    py3-pip \
    python3 \
    python3-dev \
    R-dev \
  && /usr/bin/pip3 install --upgrade pip \
  && /usr/bin/pip3 install --no-cache-dir -r requirements.txt \
  && ln -sf /usr/bin/python3 /usr/bin/python \
  && ln -sf /usr/bin/python3-config /usr/bin/python-config \
  && mkdir ~/.R && echo 'CXX=clang++' > ~/.R/Makevars \
  && Rscript -e "install.packages('RInside', repos = 'https://cloud.r-project.org')"

RUN scons

ADD https://github.com/just-containers/s6-overlay/releases/latest/download/s6-overlay-amd64.tar.gz /tmp/build/s6overlay.tar.gz

ADD https://bitbucket.org/pypy/pypy/downloads/pypy${PYPY_VERSION}.tar.bz2 /tmp/build/pypy${PYPY_VERSION}-linux64.tar.bz2

WORKDIR /tmp/build-out

RUN tar xzf /tmp/build/s6overlay.tar.gc -C ./ \
  && mkdir -p ./usr/share/pypy3 \
  && tar xf /tmp/build/pypy${PYPY_VERSION}-linux64.tar.bz2 -C ./usr/share/pypy3 \
  && printf '#!/bin/sh\n\nexec /usr/share/pyp3/bin/pypy3 "$@"\n' \ > ./usr/bin/python \
  && ln -sf ./usr/bin/python ./usr/bin/python3 \
  && tar xzf /tmp/build/s6overlay.tar.gz -C / \
  && rm s6overlay.tar.gz \

  && apk del .build-deps \
  && mkdir -p /usr/lib/pluma \
  && mv /tmp/build/pluma /tmp/build/PluGen /tmp/build/plugins \
    /tmp/build/pipelines /tmp/build/LICENSE ./usr/lib/pluma/ \
  && rm -rf /tmp/build \
  && rm -rf /root/.cache/pip \
  && apk fetch perl

FROM scratch

COPY from=builder /tmp/build-out/* /*
COPY from=builder /etc/R ./etc/R
COPY from=builder /usr/bin/R ./usr/bin/R
COPY from=builder /usr/bin/Rscript ./usr/bin/Rscript
COPY from=builder /usr/lib/R ./usr/lib/R
COPY from=builder /usr/share/R/ ./usr/share/R
COPY from=builder /usr/bin ./usr/bin
COPY from=builder /usr/share ./usr/share

RUN  printf '#!/bin/sh\n\nexec /usr/share/pluma/pluma "$@"\n' \
  > ./usr/bin/pluma \
  && printf '#!/bin/sh\n\nexec /usr/share/pluma/PluGen/plugen "$@"\n' \
     > ./usr/bin/plugen \

VOLUME ["/usr/share/pluma/plugins"]
VOLUME ["/usr/share/pluma/pipelines"]

ENTRYPOINT ["/init"]
CMD ["/pluma"]
