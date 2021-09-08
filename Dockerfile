# syntax=docker/dockerfile:1.3-labs
# Copyright 2019-2021 Florida International University
# SPDX-License-Identifier: MIT

FROM alpine:latest

ENV CC="clang"
ENV CXX="clang++"
ENV PLUMA_DIRECTORY="/app"

WORKDIR /app

ADD ./ ./

RUN apk add --no-cache build-base scons blas clang clang-dev musl musl-dev blas-dev perl-dev perl git R R-mathlib R-dev python3-dev libffi-dev libexecinfo-dev python3 libffi libc-dev musl-dev py3-pip swig pcre-dev \
  && ln -sf /usr/bin/python3 /usr/bin/python \
  && ln -sf /usr/bin/pip3 /usr/bin/pip \
  && mkdir -p /root/.R \
  && printf 'CXX=clang++\nPKG_CXXFLAGS += -D__MUSL__' > ~/.R/Makevars \
  && Rscript -e "install.packages('RInside', repos = 'https://cloud.r-project.org')"

RUN scons \
  && rm -rf /var/cache/apk/*

VOLUME ["/app/plugins"]
VOLUME ["/app/pipelines"]

CMD ["/app/pluma"]
