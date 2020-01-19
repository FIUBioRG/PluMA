# Copyright 2019 Florida International University
# SPDX-License-Identifier: MIT

FROM alpine:latest AS builder

ARG CFLAGS="-O2 -mtune=generic -march=native -pipe -fPIC $CFLAGS"
ARG CXXFLAGS="-O2 -mtune=generic -march=native -pipe -fPIC $CFLAGS"
ARG LDFLAGS="-L/usr/include -lm"
ARG MAKEFLAGS="-j$(nproc) $MAKEFLAGS"

WORKDIR /tmp/build

RUN apk update && \
  apk add --no-cache --upgrade \
    binutils \
    python3 \
    musl-utils \
    musl-dev \
    git \
    gcc \
    libstdc++ \
    libc6-compat \
    build-base \
    R \
    R-dev \
    python3 \
    python3-dev
    pkgconf \
    perl \
    perl-dev \
    perl-utils

# RUN scons

RUN install -Dm644 LICENSE $WORKDIR/usr/share/licenses/PluMA/LICENSE

FROM scratch

WORKDIR /

COPY --from=builder /tmp/build

CMD ["/usr/bin/pluma"]
