# syntax=docker/dockerfile:1.3-labs
# Copyright 2019-2021 Florida International University
# SPDX-License-Identifier: MIT

FROM alpine:latest

WORKDIR /app

COPY ./ ./

RUN echo http://dl-cdn.alpinelinux.org/alpine/v3.16/main >> /etc/apk/repositories

RUN apk update && apk add -t .runtime-deps \
  build-base scons bash blas blas-dev musl-dev musl libstdc++ \
  perl perl-dev git R R-dev R-mathlib python3-dev libffi \
  libffi-dev elfutils-dev libc-dev libexecinfo-dev \
  py3-pip py3-pandas py3-numpy swig pcre pcre-dev

RUN pip install pythonds

RUN mkdir -p ~/.R \
  && printf 'PKG_CXXFLAGS += -D__MUSL__' > ~/.R/Makevars \
  && Rscript -e "install.packages('RInside', repos = 'https://cloud.r-project.org')"

RUN python3 Plugins.py download \
  && scons

VOLUME ["/app/plugins"]
VOLUME ["/app/pipelines"]

ENTRYPOINT ["/app/pluma"]
