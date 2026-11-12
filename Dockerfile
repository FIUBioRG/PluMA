# syntax=docker/dockerfile:1.3-labs
# Copyright 2019-2021 Florida International University
# SPDX-License-Identifier: MIT

# The 3.23 series, not one patch release: rebuilds pick up its security fixes.
FROM alpine:3.23

ENV CC="clang"
ENV CXX="clang++"

WORKDIR /app

COPY ./ ./

RUN apk add -t .runtime-deps \
  build-base scons blas blas-dev clang musl-dev musl \
  perl perl-dev git R R-dev R-mathlib python3-dev libffi \
  libffi-dev libc-dev \
  py3-pip py3-pandas py3-numpy swig pcre pcre-dev \
&& ln -sf /usr/bin/python3 /usr/bin/python \
&& pip install --break-system-packages pythonds \
&& mkdir -p ~/.R \
&& printf 'CXX=clang++\nPKG_CXXFLAGS += -D__MUSL__' > ~/.R/Makevars \
&& Rscript -e "install.packages('RInside', repos = 'https://cloud.r-project.org')" \
&& scons

VOLUME ["/app/plugins"]
VOLUME ["/app/pipelines"]

ENTRYPOINT ["/app/pluma"]
